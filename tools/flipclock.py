#!/usr/bin/env python3
"""
flipclock — drive the LilyGO T4-S3 flip clock's emotion API.

Two transports, same JSON schema:

  HTTP  POST http://flipclock.local/emotion    (needs same Wi-Fi)
  BLE   write to Nordic UART RX characteristic (needs BLE range)

Usage
-----
    flipclock.py thinking
    flipclock.py working --duration 30 --message "building"
    flipclock.py celebrate -d 5
    flipclock.py health
    flipclock.py --scan

    flipclock.py success --transport ble        # force BLE
    flipclock.py success --transport http       # force HTTP

Default transport is "auto": try HTTP first (faster, no pairing), fall back
to BLE. Set FLIPCLOCK_HOST / FLIPCLOCK_BLE_NAME to override discovery.

BLE requires `pip install bleak`. HTTP needs nothing beyond the stdlib.

macOS note: BLE does not expose MAC addresses; devices are matched by
advertised name. The terminal app running this needs Bluetooth permission
(System Settings > Privacy & Security > Bluetooth) the first time.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import urllib.error
import urllib.request

STATES = [
    # working states — what the device shows most of the time
    "thinking", "working", "searching", "focused", "waiting",
    "reading", "editing", "building", "testing", "flashing", "debugging",
    # positive
    "success", "celebrate", "joy", "excited", "proud", "trust",
    "content", "calm", "relief",
    # low arousal
    "sleepy", "bored",
    # negative
    "sad", "disappointed", "confused", "surprise", "fear",
    "frustrated", "anger", "error", "disgust", "anticipation",
    # stop and revert to the clock
    "clear",
]

HOST     = os.environ.get("FLIPCLOCK_HOST", "flipclock.local")
BLE_NAME = os.environ.get("FLIPCLOCK_BLE_NAME", "FlipClock")

NUS_RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
NUS_TX = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

MSG_MAX = 48


# ----------------------------------------------------------------- HTTP --

def http_post(payload: dict, host: str = HOST, timeout: float = 4.0) -> dict:
    req = urllib.request.Request(
        f"http://{host}/emotion",
        data=json.dumps(payload).encode(),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read().decode())


def http_health(host: str = HOST, timeout: float = 4.0) -> dict:
    with urllib.request.urlopen(f"http://{host}/health", timeout=timeout) as r:
        return json.loads(r.read().decode())


# ------------------------------------------------------------------ BLE --

def _need_bleak():
    try:
        import bleak  # noqa: F401
    except ImportError:
        sys.exit("BLE transport needs bleak:  pip install bleak")


async def _ble_send(payload: dict, name: str, timeout: float) -> str:
    from bleak import BleakClient, BleakScanner

    device = await BleakScanner.find_device_by_name(name, timeout=timeout)
    if device is None:
        raise RuntimeError(
            f'no BLE device named "{name}" in range '
            "(check Settings > BLE on the clock)"
        )

    reply: list[str] = []

    def on_notify(_, data: bytearray):
        reply.append(data.decode(errors="replace"))

    async with BleakClient(device) as client:
        try:
            await client.start_notify(NUS_TX, on_notify)
        except Exception:
            pass  # notifications are a nicety, not a requirement
        await client.write_gatt_char(
            NUS_RX, json.dumps(payload).encode(), response=True
        )
        # Give the device a moment to answer before tearing the link down.
        import asyncio
        await asyncio.sleep(0.4)

    return reply[0] if reply else "(no reply)"


def ble_send(payload: dict, name: str = BLE_NAME, timeout: float = 10.0) -> str:
    _need_bleak()
    import asyncio
    return asyncio.run(_ble_send(payload, name, timeout))


def ble_scan(timeout: float = 6.0):
    _need_bleak()
    import asyncio
    from bleak import BleakScanner

    async def run():
        return await BleakScanner.discover(timeout=timeout)

    return asyncio.run(run())


# ------------------------------------------------------------------ API --

def emote(state: str, duration_s: int = 5, message: str = "",
          transport: str = "auto", host: str = HOST,
          ble_name: str = BLE_NAME) -> str:
    """Show an emotion. Returns a short human-readable result string."""
    if state not in STATES:
        raise ValueError(f"state must be one of {', '.join(STATES)}")
    if state == "clear":
        duration_s = 0
    if len(message) > MSG_MAX:
        raise ValueError(f"message must be <= {MSG_MAX} characters")

    payload = {"state": state, "duration_s": int(duration_s)}
    if message:
        payload["message"] = message

    if transport in ("auto", "http"):
        try:
            return f"http: {http_post(payload, host)}"
        except (urllib.error.URLError, OSError, TimeoutError) as e:
            if transport == "http":
                raise
            print(f"[flipclock] HTTP failed ({e}); falling back to BLE",
                  file=sys.stderr)

    return f"ble: {ble_send(payload, ble_name)}"


# ------------------------------------------------------------------ CLI --

def main() -> int:
    p = argparse.ArgumentParser(
        description="Drive the flip clock's emotion API.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument("state", nargs="?", choices=STATES + ["health"],
                   help="emotion to show, or 'health'")
    p.add_argument("-d", "--duration", type=int, default=5,
                   help="seconds before it reverts to the clock (default 5)")
    p.add_argument("-m", "--message", default="",
                   help=f"optional caption, <= {MSG_MAX} chars")
    p.add_argument("-t", "--transport", choices=["auto", "http", "ble"],
                   default="auto")
    p.add_argument("--host", default=HOST)
    p.add_argument("--ble-name", default=BLE_NAME)
    p.add_argument("--scan", action="store_true",
                   help="list nearby BLE devices and exit")
    args = p.parse_args()

    if args.scan:
        for d in ble_scan():
            print(f"{d.name or '(unnamed)':<28} {d.address}")
        return 0

    if not args.state:
        p.print_help()
        return 2

    if args.state == "health":
        print(json.dumps(http_health(args.host), indent=2))
        return 0

    try:
        print(emote(args.state, args.duration, args.message,
                    args.transport, args.host, args.ble_name))
    except Exception as e:
        print(f"error: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
