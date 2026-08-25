"""The top-level GUI window."""

from __future__ import annotations

import json
import queue
import time
import tkinter as tk
from collections import deque
from tkinter import filedialog, messagebox
from typing import Any

from .. import protocol, reader, simulator
from .maze_view import MazeView
from .panels import ConnectionBar, LogPane, StatusPanel
from .theme import COLORS, FONT_UI_BOLD

DRAIN_INTERVAL_MS = 50
MAX_EVENTS_PER_DRAIN = 200
RATE_WINDOW_S = 3.0


class CompanionApp(tk.Tk):
    """The companion window."""

    def __init__(
        self,
        port: str | None = None,
        baud: int = reader.DEFAULT_BAUD,
        simulate: bool = False,
        seed: int = 42,
    ) -> None:
        super().__init__()

        self.title("Tilting Maze — Companion")
        self.geometry("1080x820")
        self.minsize(900, 700)
        self.configure(bg=COLORS["bg"])

        self._events: queue.Queue = queue.Queue()
        self._source: Any | None = None
        self._seed = seed

        self._last_maze: dict[str, Any] | None = None
        self._last_message_at: float | None = None
        self._recent: deque[float] = deque()
        self._dropped = 0

        self._build_ui()

        if simulate:
            self._start_source(simulator.SimulatedSource(seed=seed, events=self._events))
        elif port:
            self._on_connect(port, baud)

        self.protocol("WM_DELETE_WINDOW", self._on_close)
        self.after(DRAIN_INTERVAL_MS, self._drain)

    def _build_ui(self) -> None:
        self.connection = ConnectionBar(
            self,
            self._on_connect,
            self._on_disconnect,
            on_freq=self._on_set_freq,
        )
        self.connection.pack(fill=tk.X, padx=12, pady=(12, 8))

        middle = tk.Frame(self, bg=COLORS["bg"])
        middle.pack(fill=tk.BOTH, expand=True, padx=12)

        left = tk.Frame(middle, bg=COLORS["bg"])
        left.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        self.maze_view = MazeView(left, size=520)
        self.maze_view.pack(fill=tk.BOTH, expand=True)

        side = tk.Frame(middle, bg=COLORS["bg"], width=300)
        side.pack(side=tk.LEFT, fill=tk.Y, padx=(12, 0))
        side.pack_propagate(False)

        self.status = StatusPanel(side)
        self.status.pack(fill=tk.X)

        self.save_btn = tk.Button(
            side,
            text="SAVE MAZE AS JSON",
            command=self._save_json,
            bg=COLORS["panel"],
            fg=COLORS["muted"],
            activebackground=COLORS["border"],
            activeforeground=COLORS["text"],
            relief=tk.FLAT,
            font=FONT_UI_BOLD,
            cursor="hand2",
            state=tk.DISABLED,
        )
        self.save_btn.pack(fill=tk.X, pady=(10, 0), ipady=8)

        self.log = LogPane(self)
        self.log.pack(fill=tk.BOTH, expand=True, padx=12, pady=12)

    def _on_set_freq(self, hz: float) -> None:
        delay = 1.0 / max(0.5, hz)
        if self._source is not None and hasattr(self._source, "set_step_delay"):
            self._source.set_step_delay(delay)

    def _start_source(self, source: Any) -> None:
        self._stop_source()
        self._source = source
        source.start()

    def _stop_source(self) -> None:
        if self._source is not None:
            self._source.stop()
            self._source = None

    def _on_connect(self, port: str, baud: int) -> None:
        self.log.append(f"# connecting to {port} at {baud} 8N1", "event")
        self._start_source(reader.SerialReader(port, baud, events=self._events))

    def _on_disconnect(self) -> None:
        self._stop_source()
        self.connection.set_state(reader.STATE_DISCONNECTED, "stopped by user")
        self.log.append("# disconnected", "event")

    def _drain(self) -> None:
        handled = 0

        while handled < MAX_EVENTS_PER_DRAIN:
            try:
                event = self._events.get_nowait()
            except queue.Empty:
                break

            self._handle(event)
            handled += 1

        self._refresh_derived()
        self.after(DRAIN_INTERVAL_MS, self._drain)

    def _handle(self, event: dict[str, Any]) -> None:
        kind = event.get("kind")

        if kind == "state":
            state = event["state"]
            self.connection.set_state(state, event.get("detail", ""))
            self.log.append(f"# {state}: {event.get('detail', '')}", "event")

        elif kind == "message":
            self._note_message()
            self.log.append(event["raw"])
            self._apply(event["msg"])

        elif kind == "bad_line":
            self._dropped += 1
            self.log.append(f"! {event['error']}: {event['raw'][:200]}", "bad")

    def _note_message(self) -> None:
        now = time.monotonic()
        self._last_message_at = now
        self._recent.append(now)

        cutoff = now - RATE_WINDOW_S
        while self._recent and self._recent[0] < cutoff:
            self._recent.popleft()

    def _apply(self, msg: dict[str, Any]) -> None:
        kind = msg.get("type")

        if kind == protocol.MSG_MAZE:
            self._last_maze = msg
            self.maze_view.set_maze(msg)
            self.save_btn.config(state=tk.NORMAL, fg=COLORS["text"])

            raised, free = protocol.wall_counts(msg["walls"])
            self.status.set("path_len", str(len(msg.get("path", []))))
            self.status.set("walls", f"{raised} / {free}")

        elif kind == protocol.MSG_STATUS:
            tick = msg.get("tick")
            if isinstance(tick, int):
                self.status.set("uptime", _format_uptime(tick))

            diag = msg.get("diagnostics") if "diagnostics" in msg else msg.get("diag")
            if diag is not None:
                self._show_diag(diag)

            if "button" in msg and msg["button"] is not None:
                self.status.set("button", "pressed" if msg["button"] else "released")

            if "state" in msg and msg["state"] is not None:
                self.status.set("state", str(msg["state"]))

            if "tilt" in msg and msg["tilt"] is not None:
                t = msg["tilt"]
                self.status.set("tilt", str(t).upper())

            if "ball" in msg and msg["ball"] is not None:
                b = msg["ball"]
                self.status.set("ball", f"({b[0]}, {b[1]})")

            self.maze_view.set_live_state(
                ball=msg.get("ball"),
                tilt=msg.get("tilt"),
                state=msg.get("state"),
            )

        elif kind == protocol.MSG_FAULT:
            diag = msg.get("diagnostics", msg.get("diag", 0))
            self._show_diag(diag)

        elif kind == protocol.MSG_LOG:
            level = msg.get("level", "info")
            self.log.append(f"# board [{level}] {msg.get('msg', '')}", "event")

    def _show_diag(self, diag: Any) -> None:
        if not isinstance(diag, int):
            return

        flags = protocol.decode_diag(diag)
        if flags:
            self.status.set("diagnostics", ", ".join(flags), COLORS["error"])
        else:
            self.status.set("diagnostics", "OK", COLORS["ok"])

    def _refresh_derived(self) -> None:
        if self._last_message_at is None:
            self.status.set("age", "—", COLORS["muted"])
        else:
            age = time.monotonic() - self._last_message_at
            color = COLORS["text"] if age < 1.0 else COLORS["warn"]
            self.status.set("age", f"{age:.1f}s ago", color)

        self.status.set(
            "dropped",
            str(self._dropped),
            COLORS["warn"] if self._dropped else COLORS["muted"],
        )

    def _save_json(self) -> None:
        if self._last_maze is None:
            return

        seed = self._last_maze.get("seed", "unknown")
        path = filedialog.asksaveasfilename(
            title="Save maze",
            defaultextension=".json",
            initialfile=f"maze-{seed}.json",
            filetypes=[("JSON", "*.json"), ("All files", "*.*")],
        )
        if not path:
            return

        try:
            with open(path, "w", encoding="utf-8") as handle:
                json.dump(self._last_maze, handle, indent=2)
                handle.write("\n")
        except OSError as exc:
            messagebox.showerror("Could not save", str(exc))
            return

        self.log.append(f"# saved {path}", "event")

    def _on_close(self) -> None:
        self._stop_source()
        self.destroy()


def _format_uptime(tick_ms: int) -> str:
    seconds, ms = divmod(tick_ms, 1000)
    minutes, seconds = divmod(seconds, 60)
    hours, minutes = divmod(minutes, 60)

    if hours:
        return f"{hours}h {minutes:02d}m {seconds:02d}s"
    if minutes:
        return f"{minutes}m {seconds:02d}s"
    return f"{seconds}.{ms // 100}s"
