"""Command line entry point for the tilting-maze companion.

    pi-comp                          open the GUI and pick a port in it
    pi-comp --port /dev/ttyUSB0      open the GUI and connect straight away
    pi-comp --simulate               run against a simulated board, no hardware
    pi-comp --serve-pty              publish a simulated board on a pty (Linux)
    pi-comp --replay stream.ndjson   check a captured stream, no GUI
"""

from __future__ import annotations

import argparse
import sys
from typing import TextIO

from . import protocol, reader


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="pi-comp",
        description="Desktop companion for the tilting-maze board.",
    )

    parser.add_argument(
        "--port",
        help="serial device, e.g. /dev/ttyUSB0 (USB adapter) or /dev/ttyACM0 (CDC bridge)",
    )
    parser.add_argument(
        "--baud",
        type=int,
        default=reader.DEFAULT_BAUD,
        help=f"line rate, default {reader.DEFAULT_BAUD} (the firmware uses 8N1)",
    )
    parser.add_argument(
        "--simulate",
        action="store_true",
        help="drive the GUI from a simulated board instead of a serial port",
    )
    parser.add_argument(
        "--serve-pty",
        action="store_true",
        help="publish a simulated board on a pseudo-terminal and print its device path",
    )
    parser.add_argument(
        "--replay",
        metavar="FILE",
        help="parse a captured NDJSON stream and report on it; - reads stdin",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=42,
        help="maze seed for the simulated board, default 42",
    )

    return parser


def run(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)

    if args.serve_pty:
        from . import simulator

        try:
            simulator.serve_pty(args.seed)
        except (RuntimeError, simulator.OracleNotFound) as exc:
            print(f"error: {exc}", file=sys.stderr)
            return 1
        return 0

    if args.replay:
        if args.replay == "-":
            return replay(sys.stdin)
        try:
            with open(args.replay, encoding="utf-8", errors="replace") as handle:
                return replay(handle)
        except OSError as exc:
            print(f"error: {exc}", file=sys.stderr)
            return 1

    # Imported here so the modes above work on a machine with no display and no Tk.
    from .gui.app import CompanionApp

    app = CompanionApp(
        port=args.port,
        baud=args.baud,
        simulate=args.simulate,
        seed=args.seed,
    )
    app.mainloop()
    return 0


def replay(stream: TextIO) -> int:
    """Parse a captured stream and summarise it, without a GUI.

    This is the verification path: pipe the host oracle straight into it and the exit
    status says whether the firmware's output and this parser agree.

    Returns:
        0 if every non-blank line parsed, 1 otherwise.
    """
    counts: dict[str, int] = {}
    bad = 0
    total = 0

    for number, line in enumerate(stream, start=1):
        if not line.strip():
            continue

        total += 1
        try:
            msg = protocol.parse_line(line)
        except protocol.ProtocolError as exc:
            bad += 1
            print(f"line {number}: {exc}", file=sys.stderr)
            continue

        if msg is None:
            continue

        kind = msg["type"]
        counts[kind] = counts.get(kind, 0) + 1

        if kind == protocol.MSG_MAZE:
            raised, free = protocol.wall_counts(msg["walls"])
            cells = protocol.walk_path(msg)
            print(
                f"maze  seed={msg.get('seed')} "
                f"solvable={msg.get('solvable')} "
                f"walls={raised} free={free} "
                f"path={len(msg.get('path', []))} steps "
                f"ends at {cells[-1]}"
            )

    print(f"\n{total} lines, {total - bad} parsed, {bad} rejected")
    for kind in sorted(counts):
        print(f"  {kind}: {counts[kind]}")

    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(run())
