#!/usr/bin/env python3
"""Tiny v0.5 closed-loop pilot for the Endless Sky AI Lab.

This intentionally stays boring: it reads AI telemetry JSONL, waits until the
game reports the AI movement gate is in flight, then writes short movement
commands to the command file. It is a harness for proving observation -> action,
not a gameplay bot.
"""

from __future__ import annotations

import argparse
import json
import os
import time
from pathlib import Path
from typing import Any


def latest_record(path: Path, record_type: str, max_bytes: int = 1_048_576) -> dict[str, Any] | None:
    if not path.exists():
        return None

    with path.open("rb") as handle:
        handle.seek(0, os.SEEK_END)
        size = handle.tell()
        handle.seek(max(0, size - max_bytes), os.SEEK_SET)
        data = handle.read().decode("utf-8", errors="replace")

    if size > max_bytes and "\n" in data:
        data = data.split("\n", 1)[1]

    for line in reversed(data.splitlines()):
        line = line.strip()
        if not line:
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        if record.get("type") == record_type:
            return record
    return None


def next_sequence(command_path: Path) -> int:
    if not command_path.exists():
        return 1
    try:
        with command_path.open("r", encoding="utf-8") as handle:
            record = json.load(handle)
    except (OSError, json.JSONDecodeError):
        return 1
    seq = record.get("seq")
    return seq + 1 if isinstance(seq, int) and seq >= 0 else 1


def atomic_write_json(path: Path, record: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp_path = path.with_suffix(path.suffix + ".tmp")
    with tmp_path.open("w", encoding="utf-8") as handle:
        json.dump(record, handle, separators=(",", ":"))
        handle.write("\n")
    tmp_path.replace(path)


def angle_delta(target: float, current: float) -> float:
    return (target - current + 180.0) % 360.0 - 180.0


def choose_action(record: dict[str, Any], args: argparse.Namespace) -> str | None:
    if not record.get("in_flight"):
        return None
    if not record.get("has_flagship"):
        return None

    active = record.get("ai_control")
    if isinstance(active, dict) and active.get("remaining", 0) > 0:
        return None

    flagship = record.get("flagship")
    if not isinstance(flagship, dict):
        return None
    if flagship.get("disabled") or flagship.get("destroyed"):
        return None

    angle = float(flagship.get("angle", 0.0))
    speed = float(flagship.get("speed", 0.0))
    delta = angle_delta(args.heading, angle)

    if abs(delta) > args.turn_tolerance:
        turn_right = delta > 0.0
        if args.invert_turn:
            turn_right = not turn_right
        return "turn_right" if turn_right else "turn_left"

    if speed < args.target_speed:
        return "thrust"

    return None


def write_command(command_path: Path, seq: int, action: str, duration: int) -> None:
    atomic_write_json(command_path, {
        "type": "ai_command",
        "seq": seq,
        "action": action,
        "duration": duration,
    })


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Run a tiny AI Lab heading/speed pilot.")
    parser.add_argument("--telemetry", default="ai-telemetry.jsonl", type=Path,
        help="Telemetry JSONL file written by Endless Sky.")
    parser.add_argument("--command", default="ai-command.json", type=Path,
        help="Command JSON file read by Endless Sky.")
    parser.add_argument("--heading", default=0.0, type=float,
        help="Target heading in degrees.")
    parser.add_argument("--target-speed", default=3.0, type=float,
        help="Thrust while below this speed after heading is aligned.")
    parser.add_argument("--turn-tolerance", default=5.0, type=float,
        help="Allowed heading error in degrees before turning.")
    parser.add_argument("--duration", default=6, type=int,
        help="Movement command duration in frames, clamped to the game limit.")
    parser.add_argument("--interval", default=0.10, type=float,
        help="Seconds to wait between telemetry polls.")
    parser.add_argument("--invert-turn", action="store_true",
        help="Swap left/right if your heading test moves the wrong direction.")
    parser.add_argument("--once", action="store_true",
        help="Process one telemetry record and exit.")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    duration = max(1, min(300, args.duration))
    seq = next_sequence(args.command)
    last_tick: int | None = None

    while True:
        record = latest_record(args.telemetry, "ai_telemetry")
        if record is None:
            if args.once:
                return 1
            time.sleep(args.interval)
            continue

        tick = record.get("tick")
        if tick == last_tick:
            if args.once:
                return 0
            time.sleep(args.interval)
            continue
        last_tick = tick if isinstance(tick, int) else None

        action = choose_action(record, args)
        if action:
            write_command(args.command, seq, action, duration)
            print(f"tick={tick} seq={seq} action={action} duration={duration}", flush=True)
            seq += 1
        elif args.once:
            return 0

        time.sleep(args.interval)


if __name__ == "__main__":
    raise SystemExit(main())
