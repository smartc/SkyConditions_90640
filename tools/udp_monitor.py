#!/usr/bin/env python3
"""
UDP Safety / Park Sensor Monitor
Listens on the DDA park-sensor broadcast port and prints incoming packets.

Packets with a deviceType not in LISTEN_FOR are logged as ignored so you
can discover what other devices are broadcasting on the same port.

Usage:
    python udp_monitor.py
"""

import socket
import json

PORT = 23435
LISTEN_FOR = {"SafetySensor"}   # add e.g. "ScopeParkSensor" to also show park sensors

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
sock.bind(("", PORT))

print(f"Listening for UDP broadcasts on port {PORT}...")
print(f"Accepting deviceType: {LISTEN_FOR}")
print()

while True:
    data, addr = sock.recvfrom(512)
    try:
        msg = json.loads(data.decode())
        dev_type = msg.get("deviceType", "")

        if dev_type not in LISTEN_FOR:
            print(f"[{addr[0]}] Ignored: deviceType={dev_type!r}")
            continue

        safe     = msg.get("isSafe")
        rain     = msg.get("rainState", "?")
        temp     = msg.get("ambTemp")
        name     = msg.get("name", "?")
        serial   = msg.get("serialNumber", "?")

        temp_str = f"{temp:.1f}°C" if temp is not None else "null"
        safe_str = "SAFE ✓" if safe else "UNSAFE ✗"

        print(f"[{addr[0]}] {name} ({dev_type} / {serial})")
        print(f"  → {safe_str}  rain={rain}  ambTemp={temp_str}")
        print()

    except json.JSONDecodeError:
        print(f"[{addr[0]}] Non-JSON packet: {data!r}")
    except Exception as e:
        print(f"[{addr[0]}] Error: {e}  raw={data!r}")
