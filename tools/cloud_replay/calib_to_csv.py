#!/usr/bin/env python3
"""Convert CloudRoll nightly calibration files to the CSV layout cloud_replay reads.

  https://cloudroll.iot.thesmarts.ca/media/calibration/nights/<YYYY-MM-DD>.jsonl.gz

Each dark minute becomes one row: time, cloud_cover (as reported then),
frame_ambient, humidity, allsky_stars, sun_alt and the 32x24 grid as
r00c00..r23c31, so cloud_replay can score it against the allsky star count.

  python3 calib_to_csv.py 2026-10-15.jsonl.gz > night-2026-10-15.csv
"""
import csv
import gzip
import json
import sys

ROWS, COLS = 24, 32


def main(paths):
    out = csv.writer(sys.stdout, lineterminator="\n")
    out.writerow(["time", "cloud_cover", "frame_ambient", "humidity", "allsky_stars", "sun_alt"]
                 + [f"r{r:02d}c{c:02d}" for r in range(ROWS) for c in range(COLS)])
    for path in paths:
        opener = gzip.open if path.endswith(".gz") else open
        with opener(path, "rt", encoding="utf-8") as f:
            for line in f:
                rec = json.loads(line)
                if rec.get("type") != "minute":
                    continue
                pixels = rec.get("pixels")
                if not pixels or len(pixels) != ROWS:
                    continue
                readings = rec.get("readings") or {}
                out.writerow([
                    rec["time"],
                    readings.get("cloud_cover", ""),
                    rec.get("ambient", readings.get("ambient_temp", "")),
                    readings.get("humidity", ""),
                    (rec.get("allsky") or {}).get("stars", ""),
                    (rec.get("astro") or {}).get("sun_alt", ""),
                ] + [v for row in pixels for v in row])


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    main(sys.argv[1:])
