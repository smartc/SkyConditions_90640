/*
 * cloud_replay – run the firmware's cloud-cover maths (src/sensors/cloud_model.cpp)
 * on the desktop, against unit checks and CloudRoll recordings.
 *
 * Build (any C++11 compiler; from the repo root):
 *   g++ -O2 -std=c++17 -I. -Isrc/sensors -o cloud_replay \
 *       tools/cloud_replay/cloud_replay.cpp src/sensors/cloud_model.cpp
 *
 * Unit checks (exit status 1 on failure):
 *   ./cloud_replay --selftest
 *
 * Replay CloudRoll minute CSVs (https://cloudroll.iot.thesmarts.ca/api/readings/<date>.csv?grid=1):
 *   ./cloud_replay --clear 26 --overcast 16 --amb-slope 0.58 --amb-ref 0 \
 *       --method pixel --region full --edge 3 2026-10-14.csv 2026-10-15.csv
 *
 * Options (defaults = firmware defaults):
 *   --clear D --overcast D        Clear / Overcast Δ (°C)            [20 / 5]
 *   --amb-slope S --amb-ref T     ambient compensation               [0 / 0]
 *   --dew-slope S --dew-ref T     dew-point compensation (humidity)  [0 / 0]
 *   --tau SEC                     ambient smoothing time constant    [0]
 *   --method mean|pixel           reported method                    [mean]
 *   --region center|full --edge N per-pixel region                   [center / 2]
 *   --avg-hours H                 AveragePeriod median window        [0]
 *   --rows                        print every row, not just the summary
 *
 * Rows are one per minute, so smoothing and AveragePeriod run at 1/min rather
 * than the device's 5 s frame interval.  Night rows with an allsky star count
 * (sun_alt ≤ −18°) are scored: ≥ 800 stars = clear, < 50 = overcast.
 */

#include "cloud_model.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Self-test
// ---------------------------------------------------------------------------

static int failures = 0;

static void check(bool ok, const char *what, double got)
{
  printf("  %-4s %-58s (got %.2f)\n", ok ? "ok" : "FAIL", what, got);
  if (!ok) failures++;
}

static void fillFrame(float *frame, float t)
{
  for (int i = 0; i < SENSOR_PIXELS; i++) frame[i] = t;
}

static int selftest()
{
  printf("Thresholds\n");
  CloudParams p = { 26.0f, 16.0f, 0.0f, 0.0f, 0.0f, 0.0f };
  CloudThresholds t = cloudEffectiveThresholds(p, 21.0f, NAN);
  check(t.clear == 26.0f && t.overcast == 16.0f, "slope 0 leaves thresholds unchanged", t.clear);

  // Issue #9 spot checks: Clear 26, Overcast 16, slope 0.58, ref 0.
  p.ambSlope = 0.58f;
  float c;
  t = cloudEffectiveThresholds(p, 21.0f, NAN);
  c = cloudCoverFromDelta(27.5f, t);
  check(c >= 99.9f, "overcast at 21 C (delta 27.5) -> 100 %", c);
  t = cloudEffectiveThresholds(p, 20.0f, NAN);
  c = cloudCoverFromDelta(38.0f, t);
  check(c <= 0.1f, "clear afternoon at 20 C (delta 38) -> 0 %", c);
  t = cloudEffectiveThresholds(p, 0.0f, NAN);
  c = cloudCoverFromDelta(25.0f, t);
  check(c >= 9.9f && c <= 10.1f, "clear night at 0 C (delta 25) -> 10 %", c);
  c = cloudCoverFromDelta(28.0f, t);
  check(c <= 0.1f, "clear night at 0 C (delta 28) -> 0 %", c);
  t = cloudEffectiveThresholds(p, 2.0f, NAN);
  c = cloudCoverFromDelta(9.0f, t);
  check(c >= 99.9f, "overcast night at 2 C (delta 9) -> 100 %", c);

  // Dew-point term: skipped when NAN, applied otherwise.
  p.ambSlope = 0.0f; p.dewSlope = -0.2f; p.dewRef = 0.0f;
  t = cloudEffectiveThresholds(p, 10.0f, NAN);
  check(t.clear == 26.0f, "dew term skipped without humidity", t.clear);
  t = cloudEffectiveThresholds(p, 10.0f, -10.0f);
  check(fabsf(t.clear - 28.0f) < 1e-4f && fabsf(t.overcast - 18.0f) < 1e-4f,
        "dew slope -0.2 at dew point -10 C shifts +2", t.clear);
  c = cloudDewPointC(20.0f, 50.0f);
  check(fabsf(c - 9.26f) < 0.05f, "dew point 20 C / 50 % ~ 9.26 C", c);

  // Degenerate span.
  CloudThresholds bad = { 10.0f, 10.0f };
  c = cloudCoverFromDelta(5.0f, bad);
  check(c == 50.0f, "zero span -> 50 %", c);

  printf("Per-pixel\n");
  static float frame[SENSOR_PIXELS];
  CloudThresholds t2 = { 20.0f, 10.0f };
  fillFrame(frame, -15.0f);     // ambient 0 → Δ 15 → 50 %
  CloudRegion r = cloudPixelRegion(1, 3);
  check(r.rowStart == 3 && r.rowEnd == 20 && r.colStart == 3 && r.colEnd == 28,
        "full frame, edge 3 -> rows 3..20, cols 3..28", r.rowEnd);
  c = cloudCoverPixel(frame, 0.0f, t2, r);
  check(fabsf(c - 50.0f) < 1e-3f, "uniform frame at delta 15 -> 50 %", c);
  // Edge pixels are excluded: a warm border must not change the result.
  for (int i = 0; i < SENSOR_COLS; i++) frame[i] = 30.0f;
  c = cloudCoverPixel(frame, 0.0f, t2, r);
  check(fabsf(c - 50.0f) < 1e-3f, "warm row 0 ignored with edge 3", c);
  r = cloudPixelRegion(1, 20);
  check(r.rowStart == CENTER_ROW_START, "oversized edge falls back to centre FOV", r.rowStart);

  printf("Ambient smoothing\n");
  AmbientSmoother s;
  float v = s.update(5.0f, 0.0f, 120.0f, CLOUD_AMB_RESET_C, true);
  check(v == 5.0f, "first reading passes through", v);
  v = s.update(3.8f, 5.0f, 120.0f, CLOUD_AMB_RESET_C, false);
  check(fabsf(v - (5.0f - 1.2f * 5.0f / 125.0f)) < 1e-4f, "1.2 C dip damped (alpha = dt/(tau+dt))", v);
  for (int i = 0; i < 120; i++) v = s.update(3.8f, 5.0f, 120.0f, CLOUD_AMB_RESET_C, false);
  check(fabsf(v - 3.8f) < 0.01f, "converges after 10 min at tau 120 s", v);
  v = s.update(8.0f, 5.0f, 120.0f, CLOUD_AMB_RESET_C, false);
  check(v == 8.0f, "jump > reset threshold snaps to raw", v);
  v = s.update(7.0f, 5.0f, 120.0f, CLOUD_AMB_RESET_C, true);
  check(v == 7.0f, "source change resets", v);
  v = s.update(6.0f, 5.0f, 0.0f, CLOUD_AMB_RESET_C, false);
  check(v == 6.0f, "tau 0 = off", v);

  printf("AveragePeriod window\n");
  CloudCoverWindow w;
  float m, px; int n;
  check(!w.median(0, 0, m, px, n), "empty window reports nothing", 0);
  const float covers[] = { 0, 0, 100, 0, 0, 0, 10 };   // one spike
  uint32_t ms = 1000;
  for (float cv : covers) { w.push(ms, cv, cv + 1); ms += 5000; }
  uint32_t now = ms - 5000;
  w.median(now, 0, m, px, n);
  check(n == 1 && m == 10.0f, "period 0 -> latest frame", m);
  w.median(now, 30000, m, px, n);
  check(n == 7 && m == 0.0f && px == 1.0f, "30 s median ignores the single spike", m);
  w.median(now, 10000, m, px, n);
  check(n == 3, "10 s window takes 3 frames", n);
  w.median(now, 0xFFFFFFFFu, m, px, n);
  check(n == 7, "oversized window uses the whole buffer", n);
  CloudCoverWindow big;
  for (int i = 0; i < CLOUD_AVG_SAMPLES + 50; i++) big.push(i * 5000u, (float)(i % 2), 0);
  big.median((CLOUD_AVG_SAMPLES + 49) * 5000u, 0xFFFFFFFFu, m, px, n);
  check(n == CLOUD_AVG_SAMPLES, "ring keeps CLOUD_AVG_SAMPLES", n);

  printf(failures ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", failures);
  return failures ? 1 : 0;
}

// ---------------------------------------------------------------------------
// CloudRoll CSV replay
// ---------------------------------------------------------------------------

static std::vector<std::string> splitCsv(const std::string &line)
{
  std::vector<std::string> out;
  std::string cur;
  bool q = false;
  for (size_t i = 0; i < line.size(); i++) {
    char ch = line[i];
    if (q) {
      if (ch == '"' && i + 1 < line.size() && line[i + 1] == '"') { cur += '"'; i++; }
      else if (ch == '"') q = false;
      else cur += ch;
    } else if (ch == '"') q = true;
    else if (ch == ',') { out.push_back(cur); cur.clear(); }
    else if (ch != '\r') cur += ch;
  }
  out.push_back(cur);
  return out;
}

static bool num(const std::string &s, float &v)
{
  if (s.empty()) return false;
  char *end;
  v = strtof(s.c_str(), &end);
  return end != s.c_str();
}

// Seconds since 1970 for an ISO-8601 local timestamp (offset ignored – only
// differences are used).
static double isoSeconds(const std::string &s)
{
  int y, mo, d, h, mi, se;
  if (sscanf(s.c_str(), "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &se) != 6) return NAN;
  y -= mo <= 2;
  int era = (y >= 0 ? y : y - 399) / 400;
  int yoe = y - era * 400;
  int doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  double days = era * 146097.0 + doe - 719468.0;
  return days * 86400.0 + h * 3600.0 + mi * 60.0 + se;
}

struct Score {
  std::vector<float> v;
  void add(float x) { v.push_back(x); }
  void print(const char *name, const char *badLabel, bool overcast) {
    if (v.empty()) { printf("  %-9s no rows\n", name); return; }
    std::sort(v.begin(), v.end());
    int bad = 0;
    for (float x : v) if (overcast ? x < 80.0f : x >= 20.0f) bad++;
    printf("  %-9s n=%-5zu median %5.1f %%   %s %5.1f %%\n", name, v.size(),
           v[v.size() / 2], badLabel, 100.0 * bad / v.size());
  }
};

int main(int argc, char **argv)
{
  CloudParams p = { 20.0f, 5.0f, 0.0f, 0.0f, 0.0f, 0.0f };
  float tau = 0.0f, avgHours = 0.0f;
  bool pixel = false, rows = false;
  uint8_t region = 0, edge = 2;
  std::vector<std::string> files;

  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    auto next = [&]() -> const char * {
      if (i + 1 >= argc) { fprintf(stderr, "%s needs a value\n", a.c_str()); exit(2); }
      return argv[++i];
    };
    if      (a == "--selftest")  return selftest();
    else if (a == "--clear")     p.clearDelta    = atof(next());
    else if (a == "--overcast")  p.overcastDelta = atof(next());
    else if (a == "--amb-slope") p.ambSlope      = atof(next());
    else if (a == "--amb-ref")   p.ambRef        = atof(next());
    else if (a == "--dew-slope") p.dewSlope      = atof(next());
    else if (a == "--dew-ref")   p.dewRef        = atof(next());
    else if (a == "--tau")       tau             = atof(next());
    else if (a == "--avg-hours") avgHours        = atof(next());
    else if (a == "--method")    pixel           = std::string(next()) == "pixel";
    else if (a == "--region")    region          = std::string(next()) == "full" ? 1 : 0;
    else if (a == "--edge")      edge            = (uint8_t)atoi(next());
    else if (a == "--rows")      rows            = true;
    else if (a.rfind("--", 0) == 0) { fprintf(stderr, "unknown option %s\n", a.c_str()); return 2; }
    else files.push_back(a);
  }
  if (files.empty()) { fprintf(stderr, "usage: see the comment at the top of cloud_replay.cpp\n"); return 2; }

  printf("clear %.2f  overcast %.2f  amb slope %.3f ref %.1f  dew slope %.3f ref %.1f  "
         "tau %.0f s  method %s  region %s edge %u  avg %.4f h\n",
         p.clearDelta, p.overcastDelta, p.ambSlope, p.ambRef, p.dewSlope, p.dewRef,
         tau, pixel ? "pixel" : "mean", region ? "full" : "center", edge, avgHours);

  CloudRegion rgn = cloudPixelRegion(region, edge);
  AmbientSmoother smoother;
  static CloudCoverWindow window;
  static float frame[SENSOR_PIXELS];
  double lastT = NAN, t0 = NAN;
  Score clear, overcast;
  std::map<std::string, std::vector<float>> hourly;   // "YYYY-MM-DDTHH" → reported cover

  if (rows) printf("%-25s %7s %7s %6s %6s %6s %6s %6s\n",
                   "time", "amb", "ambCld", "dew", "clrEff", "ovcEff", "old%", "new%");

  for (const std::string &path : files) {
    std::ifstream in(path);
    if (!in) { fprintf(stderr, "cannot open %s\n", path.c_str()); return 2; }
    std::string line;
    std::getline(in, line);
    std::vector<std::string> head = splitCsv(line);
    auto col = [&](const char *name) {
      for (size_t i = 0; i < head.size(); i++) if (head[i] == name) return (int)i;
      return -1;
    };
    int cTime = col("time"), cOld = col("cloud_cover"), cAmb = col("frame_ambient"),
        cAmb2 = col("ambient_temp"), cHum = col("humidity"), cStars = col("allsky_stars"),
        cSun = col("sun_alt"), cGrid = col("r00c00");
    if (cTime < 0 || cGrid < 0) { fprintf(stderr, "%s: no time / grid columns (use ?grid=1)\n", path.c_str()); return 2; }

    while (std::getline(in, line)) {
      std::vector<std::string> f = splitCsv(line);
      if ((int)f.size() < cGrid + SENSOR_PIXELS) continue;
      bool ok = true;
      for (int i = 0; i < SENSOR_PIXELS && ok; i++) ok = num(f[cGrid + i], frame[i]);
      float amb;
      if (!ok || !(num(f[cAmb], amb) || (cAmb2 >= 0 && num(f[cAmb2], amb)))) continue;

      double t = isoSeconds(f[cTime]);
      float dt = std::isnan(lastT) ? 0.0f : (float)(t - lastT);
      bool gap = std::isnan(lastT) || dt > 600.0f;
      lastT = t;

      float hum, dew = NAN;
      if (cHum >= 0 && num(f[cHum], hum) && hum > 0) dew = cloudDewPointC(amb, hum);

      float ambCloud = smoother.update(amb, dt, tau, CLOUD_AMB_RESET_C, gap);
      CloudThresholds th = cloudEffectiveThresholds(p, ambCloud, dew);

      float sky = 0;
      for (int r = CENTER_ROW_START; r <= CENTER_ROW_END; r++)
        for (int c = CENTER_COL_START; c <= CENTER_COL_END; c++) sky += frame[r * SENSOR_COLS + c];
      sky /= CENTER_PIXEL_COUNT;
      float meanCov = cloudCoverFromDelta(ambCloud - sky, th);
      float pxCov   = cloudCoverPixel(frame, ambCloud, th, rgn);

      if (std::isnan(t0)) t0 = t;
      uint32_t ms = (uint32_t)((t - t0) * 1000.0);
      window.push(ms, meanCov, pxCov);
      float mAvg, pAvg; int n;
      window.median(ms, (uint32_t)(avgHours * 3600000.0), mAvg, pAvg, n);
      float cov = pixel ? pAvg : mAvg;

      float old = NAN;
      if (cOld >= 0) num(f[cOld], old);
      hourly[f[cTime].substr(0, 13)].push_back(cov);

      float stars, sun;
      if (cStars >= 0 && cSun >= 0 && num(f[cStars], stars) && num(f[cSun], sun) && sun <= -18.0f) {
        if (stars >= 800) clear.add(cov);
        else if (stars < 50) overcast.add(cov);
      }

      if (rows) printf("%-25s %7.2f %7.2f %6.1f %6.1f %6.1f %6.1f %6.1f\n", f[cTime].c_str(), amb,
                       ambCloud, dew, th.clear, th.overcast, old, cov);
    }
  }

  printf("\nHourly median cloud cover (local time):\n");
  std::string day;
  for (auto &kv : hourly) {
    std::string d = kv.first.substr(0, 10);
    if (d != day) { printf("%s%s ", day.empty() ? "" : "\n", d.c_str()); day = d; }
    std::vector<float> &v = kv.second;
    std::sort(v.begin(), v.end());
    printf(" %s:%3.0f", kv.first.substr(11, 2).c_str(), v[v.size() / 2]);
  }
  printf("\n\nScored night minutes (allsky stars, sun <= -18):\n");
  clear.print("clear", ">=20 %:", false);
  overcast.print("overcast", "<80 %: ", true);
  return 0;
}
