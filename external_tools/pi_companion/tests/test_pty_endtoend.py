"""End-to-end test over a real serial device, without a board.

``--serve-pty`` publishes the simulated board on a pseudo-terminal. The reader then opens
it with pyserial exactly as it would open ``/dev/ttyUSB0``, so this exercises the parts
the other tests stub out: opening a device, blocking reads, chunk boundaries decided by
the kernel rather than by a fixture, and the threading around all of it.

Unix only, which covers the Pi and WSL. Skipped elsewhere.
"""

import os
import queue
import subprocess
import sys
import time
from pathlib import Path

import pytest

from pi_comp import protocol, reader, simulator

pytestmark = pytest.mark.skipif(
    not hasattr(os, "openpty"), reason="needs a Unix pty"
)

SRC = str(Path(__file__).resolve().parents[1] / "src")

#: Long enough for a maze plus a few status lines at 10Hz, short enough to stay snappy.
COLLECT_TIMEOUT_S = 12.0


@pytest.fixture
def board():
    """Start a simulated board on a pty and yield its device path."""
    try:
        simulator.find_oracle()
    except simulator.OracleNotFound as exc:
        pytest.skip(str(exc))

    env = {**os.environ, "PYTHONPATH": SRC, "PYTHONUNBUFFERED": "1"}
    proc = subprocess.Popen(
        [sys.executable, "-m", "pi_comp.main", "--serve-pty"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        env=env,
    )

    device = None
    deadline = time.monotonic() + 15.0

    try:
        while time.monotonic() < deadline:
            line = proc.stdout.readline()
            if not line:
                break
            if "serving the simulated board on " in line:
                device = line.strip().rsplit(" ", 1)[-1]
                break

        if device is None:
            proc.kill()
            pytest.fail("simulated board never reported a device path")

        yield device

    finally:
        proc.kill()
        proc.wait(timeout=5)


def collect(device, want_types):
    """Read from the device until every wanted message type has been seen."""
    events: queue.Queue = queue.Queue()
    r = reader.SerialReader(device, events=events, reconnect=False)
    r.start()

    seen = {}
    bad = []
    deadline = time.monotonic() + COLLECT_TIMEOUT_S

    try:
        while time.monotonic() < deadline and not want_types <= set(seen):
            try:
                event = events.get(timeout=0.5)
            except queue.Empty:
                continue

            if event["kind"] == "message":
                seen.setdefault(event["msg"]["type"], event["msg"])
            elif event["kind"] == "bad_line":
                bad.append(event)
    finally:
        r.stop()

    return seen, bad


def test_reader_receives_the_stream_over_a_real_device(board):
    seen, bad = collect(board, {protocol.MSG_HELLO, protocol.MSG_MAZE, protocol.MSG_STATUS})

    assert protocol.MSG_HELLO in seen, "no hello message"
    assert protocol.MSG_MAZE in seen, "no maze message"
    assert protocol.MSG_STATUS in seen, "no status message"

    # At most the one partial line the reader may land on when it opens the device.
    assert len(bad) <= 1, [e["error"] for e in bad]


def test_maze_received_over_the_wire_is_intact(board):
    seen, _ = collect(board, {protocol.MSG_MAZE})
    maze = seen[protocol.MSG_MAZE]

    assert len(maze["walls"]) == protocol.SIDE
    assert all(len(row) == protocol.SIDE for row in maze["walls"])

    cells = protocol.walk_path(maze)
    assert cells[-1] == tuple(maze["goal"])
    for r, c in cells:
        assert maze["walls"][r][c] == 0


def test_hello_announces_the_protocol_version(board):
    seen, _ = collect(board, {protocol.MSG_HELLO})
    assert seen[protocol.MSG_HELLO]["proto"] == protocol.PROTO_VERSION
