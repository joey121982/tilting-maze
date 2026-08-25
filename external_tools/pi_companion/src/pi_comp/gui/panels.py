"""The non-canvas widgets: connection bar, status, and raw log."""

from __future__ import annotations

import tkinter as tk
from typing import Callable

from .. import protocol, reader
from .theme import COLORS, FONT_LABEL, FONT_MONO, FONT_TITLE, FONT_UI, FONT_UI_BOLD


def _button(master: tk.Misc, text: str, command: Callable[[], None], **kwargs) -> tk.Button:
    options = dict(
        bg=COLORS["panel"],
        fg=COLORS["text"],
        activebackground=COLORS["border"],
        activeforeground=COLORS["text"],
        disabledforeground=COLORS["border"],
        relief=tk.FLAT,
        font=FONT_UI_BOLD,
        cursor="hand2",
    )
    options.update(kwargs)
    return tk.Button(master, text=text, command=command, **options)


LOG_MAX_LINES = 2000


class ConnectionBar(tk.Frame):
    """Connection status and simulation step frequency control."""

    def __init__(
        self,
        master: tk.Misc,
        on_connect: Callable[[str, int], None],
        on_disconnect: Callable[[], None],
        on_freq: Callable[[float], None] | None = None,
    ) -> None:
        super().__init__(master, bg=COLORS["panel"])

        self._on_connect = on_connect
        self._on_disconnect = on_disconnect
        self._on_freq = on_freq
        self._connected = False

        self.connect_btn = tk.Button(
            self,
            text="CONNECT",
            command=self._toggle,
            bg=COLORS["accent"],
            fg=COLORS["on_accent"],
            activebackground=COLORS["accent_active"],
            activeforeground=COLORS["on_accent"],
            relief=tk.FLAT,
            font=FONT_UI_BOLD,
            cursor="hand2",
            padx=16,
        )
        self.connect_btn.pack(side=tk.LEFT, padx=14, pady=10)

        self.state_label = tk.Label(
            self, text="disconnected", bg=COLORS["panel"], fg=COLORS["muted"], font=FONT_UI
        )
        self.state_label.pack(side=tk.LEFT, padx=6, pady=10)

        # Simulation Frequency Selector
        freq_frame = tk.Frame(self, bg=COLORS["panel"])
        freq_frame.pack(side=tk.RIGHT, padx=14, pady=10)

        tk.Label(
            freq_frame, text="SIM FREQ:", bg=COLORS["panel"], fg=COLORS["muted"], font=FONT_LABEL
        ).pack(side=tk.LEFT, padx=(0, 6))

        for hz in (1, 2, 5, 10, 20):
            btn = tk.Button(
                freq_frame,
                text=f"{hz}Hz",
                command=lambda val=hz: self._set_freq(val),
                bg=COLORS["bg"],
                fg=COLORS["text"],
                activebackground=COLORS["border"],
                relief=tk.FLAT,
                font=FONT_LABEL,
                cursor="hand2",
                padx=5,
                pady=2,
            )
            btn.pack(side=tk.LEFT, padx=2)

    def _set_freq(self, hz: float) -> None:
        if self._on_freq is not None:
            self._on_freq(hz)

    def refresh_ports(self) -> None:
        pass

    def _toggle(self) -> None:
        if self._connected:
            self._on_disconnect()
        else:
            ports = reader.available_ports()
            port = ports[0][0] if ports else "COM1"
            self._on_connect(port, reader.DEFAULT_BAUD)

    def set_state(self, state: str, detail: str = "") -> None:
        self._connected = state in (reader.STATE_CONNECTING, reader.STATE_CONNECTED)
        self.connect_btn.config(
            text="DISCONNECT" if self._connected else "CONNECT",
            bg=COLORS["error"] if self._connected else COLORS["accent"],
        )

        text = state
        if detail:
            text += f" — {detail}"
        color = COLORS["ok"] if state == reader.STATE_CONNECTED else COLORS["muted"]
        self.state_label.config(text=text, fg=color)


class StatusPanel(tk.Frame):
    """Key/value readout of real-time maze state and telemetry."""

    FIELDS = (
        ("state", "State"),
        ("tilt", "Tilt"),
        ("ball", "Ball"),
        ("button", "Button"),
        ("path_len", "Path length"),
        ("walls", "Walls / free"),
        ("uptime", "Board uptime"),
        ("diagnostics", "Diagnostics"),
        ("age", "Last message"),
        ("dropped", "Dropped lines"),
    )

    def __init__(self, master: tk.Misc) -> None:
        super().__init__(master, bg=COLORS["panel"])

        tk.Label(
            self,
            text="BOARD",
            bg=COLORS["panel"],
            fg=COLORS["text"],
            font=FONT_TITLE,
            anchor=tk.W,
        ).grid(row=0, column=0, columnspan=2, sticky=tk.W, padx=12, pady=(10, 6))

        self._values: dict[str, tk.Label] = {}

        for row, (key, label) in enumerate(self.FIELDS, start=1):
            tk.Label(
                self, text=label, bg=COLORS["panel"], fg=COLORS["muted"], font=FONT_LABEL
            ).grid(row=row, column=0, sticky=tk.W, padx=(12, 8), pady=2)

            value = tk.Label(
                self, text="—", bg=COLORS["panel"], fg=COLORS["text"], font=FONT_MONO
            )
            value.grid(row=row, column=1, sticky=tk.W, padx=(0, 12), pady=2)
            self._values[key] = value

        self.grid_columnconfigure(1, weight=1)

    def set(self, key: str, text: str, color: str | None = None) -> None:
        if key in self._values:
            self._values[key].config(text=text, fg=color or COLORS["text"])

    def reset(self) -> None:
        for key in self._values:
            self.set(key, "—", COLORS["muted"])


class LogPane(tk.Frame):
    """Scrolling view of raw incoming line stream (Pause button removed)."""

    def __init__(self, master: tk.Misc) -> None:
        super().__init__(master, bg=COLORS["panel"])

        header = tk.Frame(self, bg=COLORS["panel"])
        header.pack(fill=tk.X)

        tk.Label(
            header, text="RAW STREAM", bg=COLORS["panel"], fg=COLORS["text"], font=FONT_TITLE
        ).pack(side=tk.LEFT, padx=12, pady=(10, 6))

        body = tk.Frame(self, bg=COLORS["panel"])
        body.pack(fill=tk.BOTH, expand=True, padx=12, pady=(0, 12))

        scrollbar = tk.Scrollbar(body, relief=tk.FLAT, bg=COLORS["panel"])
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

        self.text = tk.Text(
            body,
            height=10,
            bg=COLORS["bg"],
            fg=COLORS["muted"],
            insertbackground=COLORS["text"],
            relief=tk.FLAT,
            font=FONT_MONO,
            wrap=tk.NONE,
            yscrollcommand=scrollbar.set,
        )
        self.text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.config(command=self.text.yview)

        self.text.tag_config("bad", foreground=COLORS["error"])
        self.text.tag_config("event", foreground=COLORS["accent"])
        self.text.config(state=tk.DISABLED)

        self._lines = 0

    def append(self, line: str, tag: str | None = None) -> None:
        self.text.config(state=tk.NORMAL)
        self.text.insert(tk.END, line + "\n", tag or ())
        self._lines += 1

        if self._lines > LOG_MAX_LINES:
            trim = self._lines - LOG_MAX_LINES
            self.text.delete("1.0", f"{trim + 1}.0")
            self._lines = LOG_MAX_LINES

        self.text.see(tk.END)
        self.text.config(state=tk.DISABLED)

    def clear(self) -> None:
        self.text.config(state=tk.NORMAL)
        self.text.delete("1.0", tk.END)
        self.text.config(state=tk.DISABLED)
        self._lines = 0


class PlaybackBar(tk.Frame):
    def __init__(self, master: tk.Misc, **kwargs) -> None:
        super().__init__(master, bg=COLORS["panel"])
    def set_progress(self, index: int, total: int, playing: bool) -> None:
        pass


class BoardControl(tk.Frame):
    def __init__(self, master: tk.Misc, **kwargs) -> None:
        super().__init__(master, bg=COLORS["panel"])
    def set_note(self, text: str, color: str | None = None) -> None:
        pass
