"""Offline sources, so the companion can be built and tested with no board attached."""

from __future__ import annotations

import json
import os
import queue
import shutil
import subprocess
import threading
import time
from pathlib import Path
from typing import Iterator

from . import protocol, reader

ORACLE_ENV_VAR = "PI_COMP_ORACLE"
STATUS_PERIOD_S = 0.1
ANNOUNCE_PERIOD_S = 5.0


class OracleNotFound(RuntimeError):
    """The host oracle binary has not been built yet."""


def find_oracle() -> Path:
    override = os.environ.get(ORACLE_ENV_VAR)
    if override and Path(override).is_file():
        return Path(override)

    repo_tools = Path(__file__).resolve().parents[2] / "tools"
    for name in ("host_json_dump", "host_json_dump.exe"):
        candidate = repo_tools / name
        if candidate.is_file():
            return candidate

    found = shutil.which("host_json_dump")
    if found:
        return Path(found)

    raise OracleNotFound(
        "host_json_dump not found - build it first:\n"
        f"    cd {repo_tools} && make\n"
        f"or point {ORACLE_ENV_VAR} at an existing binary."
    )


def maze_line(seed: int) -> str:
    oracle = find_oracle()
    result = subprocess.run(
        [str(oracle), str(seed)],
        capture_output=True,
        text=True,
        check=True,
    )
    return result.stdout.strip()


def stream_lines(
    seed: int = 42,
    *,
    realtime: bool = True,
    source: SimulatedSource | None = None,
) -> Iterator[str]:
    hello = f'{{"type":"hello","proto":{protocol.PROTO_VERSION},"board":"stm32f103c8-sim"}}'
    maze = maze_line(seed)
    maze_msg = json.loads(maze)
    cells = protocol.walk_path(maze_msg)

    yield hello
    yield maze

    started = time.monotonic()
    last_announce = started
    step_idx = 0
    hold_ticks = 0
    total_steps = len(cells)

    while True:
        # Apply pending frequency change ONLY when starting a new pass (step_idx == 0)
        if source is not None:
            if step_idx == 0 and source.pending_step_delay is not None:
                source.step_delay = source.pending_step_delay
                source.pending_step_delay = None
            delay = source.step_delay
        else:
            delay = 0.2

        if realtime:
            time.sleep(delay)

        now = time.monotonic()
        tick = int((now - started) * 1000)

        if step_idx < total_steps and cells:
            ball = list(cells[step_idx])
            if step_idx < total_steps - 1:
                dr = cells[step_idx + 1][0] - cells[step_idx][0]
                dc = cells[step_idx + 1][1] - cells[step_idx][1]
                if dr > 0:
                    tilt = "down"
                elif dr < 0:
                    tilt = "up"
                elif dc > 0:
                    tilt = "right"
                elif dc < 0:
                    tilt = "left"
                else:
                    tilt = "none"
                state = "solving"
            else:
                tilt = "none"
                state = "solved"
        else:
            ball = [0, 0]
            tilt = "none"
            state = "waiting"

        status_msg = {
            "type": "status",
            "tick": tick,
            "diagnostics": 0,
            "button": False,
            "ball": ball,
            "tilt": tilt,
            "state": state,
        }

        yield json.dumps(status_msg, separators=(",", ":"))

        if step_idx < total_steps - 1:
            step_idx += 1
        else:
            hold_ticks += 1
            hold_target = max(1, int(1.5 / max(0.05, delay)))
            if hold_ticks >= hold_target:
                step_idx = 0
                hold_ticks = 0

        if now - last_announce >= ANNOUNCE_PERIOD_S:
            last_announce = now
            yield hello
            yield maze


class SimulatedSource:
    def __init__(self, seed: int = 42, events: queue.Queue | None = None) -> None:
        self.seed = seed
        self.events: queue.Queue = events if events is not None else queue.Queue()
        self.step_delay: float = 0.2  # default 5 Hz
        self.pending_step_delay: float | None = None

    def set_step_delay(self, delay: float) -> None:
        self.pending_step_delay = max(0.02, delay)

    def start(self) -> None:
        if self._thread is not None:
            raise RuntimeError("source already started")

        self._stop.clear()
        self._thread = threading.Thread(target=self._run, name="simulator", daemon=True)
        self._thread.start()

    def stop(self, timeout: float = 2.0) -> None:
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=timeout)
            self._thread = None

    @property
    def running(self) -> bool:
        return self._thread is not None and self._thread.is_alive()

    def _run(self) -> None:
        self.events.put(
            {"kind": "state", "state": reader.STATE_CONNECTING, "detail": "simulator"}
        )

        try:
            lines = stream_lines(self.seed, source=self)
        except OracleNotFound as exc:
            self.events.put(
                {"kind": "state", "state": reader.STATE_DISCONNECTED, "detail": str(exc)}
            )
            return

        self.events.put(
            {"kind": "state", "state": reader.STATE_CONNECTED, "detail": "simulated board"}
        )

        for line in lines:
            if self._stop.is_set():
                break
            try:
                msg = protocol.parse_line(line)
            except protocol.ProtocolError as exc:
                self.events.put({"kind": "bad_line", "raw": line, "error": str(exc)})
                continue
            if msg is not None:
                self.events.put({"kind": "message", "msg": msg, "raw": line})

        self.events.put(
            {"kind": "state", "state": reader.STATE_DISCONNECTED, "detail": "simulator stopped"}
        )


def serve_pty(seed: int = 42) -> None:
    if not hasattr(os, "openpty"):
        raise RuntimeError("--serve-pty needs a Unix pty; run it on Linux or the Pi")

    master, slave = os.openpty()
    device = os.ttyname(slave)

    print(f"serving the simulated board on {device}")
    print(f"connect with:  pi-comp --port {device}")
    print("ctrl-c to stop")

    try:
        for line in stream_lines(seed):
            os.write(master, (line + "\n").encode("utf-8"))
    except KeyboardInterrupt:
        print("\nstopped")
    except OSError as exc:
        print(f"pty closed: {exc}")
    finally:
        os.close(master)
        os.close(slave)
