"""Canvas rendering of the maze board, real-time ball telemetry, and visited path."""

from __future__ import annotations

import tkinter as tk
from typing import Any, Callable

from .. import protocol
from .theme import COLORS

FRAME_FRACTION = 0.35
CELL_PAD = 1.0


def _same_maze(a: dict[str, Any] | None, b: dict[str, Any] | None) -> bool:
    if a is None or b is None:
        return False
    return a.get("seed") == b.get("seed") and a.get("walls") == b.get("walls")


class MazeView(tk.Canvas):
    """Square canvas showing the 10x10 maze grid, real-time ball position, and visited path."""

    def __init__(
        self,
        master: tk.Misc,
        size: int = 520,
    ) -> None:
        super().__init__(
            master,
            width=size,
            height=size,
            bg=COLORS["bg"],
            highlightthickness=0,
            relief=tk.FLAT,
        )

        self._size = size
        self._maze: dict[str, Any] | None = None
        self._ball_pos: tuple[int, int] | None = None
        self._visited_cells: list[tuple[int, int]] = []
        self._tilt: str | None = None
        self._system_state: str | None = None
        self.on_progress: Callable[[int, int, bool], None] | None = None

        self.bind("<Configure>", self._on_resize)

    def set_maze(self, msg: dict[str, Any]) -> None:
        # Only clear visited path and ball position if this is a genuinely NEW maze.
        # Periodic maze re-announcements (keepalive) must NOT clear the active run!
        if not _same_maze(self._maze, msg):
            self._maze = msg
            self._visited_cells.clear()
            self._ball_pos = None
        else:
            self._maze = msg
        self.redraw()

    def set_live_state(
        self,
        ball: list[int] | tuple[int, int] | None = None,
        tilt: str | None = None,
        state: str | None = None,
    ) -> None:
        if ball is not None:
            coord = (ball[0], ball[1])
            self._ball_pos = coord
            start = tuple(self._maze.get("start", protocol.START)) if self._maze else protocol.START
            if not self._visited_cells:
                self._visited_cells = [coord]
            else:
                last = self._visited_cells[-1]
                if last != coord:
                    dist = abs(coord[0] - last[0]) + abs(coord[1] - last[1])
                    if dist > 1 or coord == start:
                        self._visited_cells = [coord]
                    else:
                        self._visited_cells.append(coord)

        if tilt is not None:
            self._tilt = tilt
        if state is not None:
            self._system_state = state
        self.redraw()

    def replay(self) -> None:
        pass
    def toggle_play(self) -> None:
        pass
    def step(self, delta: int) -> None:
        pass
    def set_speed(self, factor: float) -> None:
        pass
    def set_loop(self, enabled: bool) -> None:
        pass
    def total_steps(self) -> int:
        return 0

    def redraw(self) -> None:
        self.delete("all")

        if self._maze is None:
            self._draw_placeholder()
            return

        self._draw_frame()
        self._draw_grid()
        self._draw_live_overlay()

    def _geometry(self) -> tuple[float, float, float, float]:
        width = int(self.winfo_width())
        height = int(self.winfo_height())
        if width <= 1:
            width = self._size
        if height <= 1:
            height = self._size

        span = min(width, height)
        cell = span / (protocol.SIDE + 2 * FRAME_FRACTION)
        frame = cell * FRAME_FRACTION

        return cell, frame, (width - span) / 2, (height - span) / 2

    def _cell_bounds(self, r: int, c: int) -> tuple[float, float, float, float]:
        cell, frame, ox, oy = self._geometry()
        x1 = ox + frame + c * cell
        y1 = oy + frame + r * cell
        return x1 + CELL_PAD, y1 + CELL_PAD, x1 + cell - CELL_PAD, y1 + cell - CELL_PAD

    def _cell_center(self, r: int, c: int) -> tuple[float, float]:
        cell, frame, ox, oy = self._geometry()
        return ox + frame + (c + 0.5) * cell, oy + frame + (r + 0.5) * cell

    def _draw_placeholder(self) -> None:
        cell, frame, ox, oy = self._geometry()
        span = frame * 2 + cell * protocol.SIDE
        self.create_text(
            ox + span / 2,
            oy + span / 2,
            text="waiting for a maze from the board",
            fill=COLORS["muted"],
            font=("Segoe UI", 11),
        )

    def _draw_frame(self) -> None:
        cell, frame, ox, oy = self._geometry()
        span = frame * 2 + cell * protocol.SIDE

        self.create_rectangle(
            ox + frame / 2,
            oy + frame / 2,
            ox + span - frame / 2,
            oy + span - frame / 2,
            outline=COLORS["frame"],
            width=frame,
        )

    def _draw_grid(self) -> None:
        assert self._maze is not None

        walls = self._maze["walls"]
        start = tuple(self._maze.get("start", protocol.START))
        goal = tuple(self._maze.get("goal", protocol.GOAL))
        cell, _, _, _ = self._geometry()

        for r in range(protocol.SIDE):
            for c in range(protocol.SIDE):
                x1, y1, x2, y2 = self._cell_bounds(r, c)

                if (r, c) == start:
                    color = COLORS["start"]
                elif (r, c) == goal:
                    color = COLORS["goal"]
                elif walls[r][c]:
                    color = COLORS["wall"]
                else:
                    color = COLORS["free"]

                self.create_rectangle(x1, y1, x2, y2, fill=color, outline="", width=0)

        for coords, label in ((start, "S"), (goal, "G")):
            x, y = self._cell_center(*coords)
            self.create_text(
                x,
                y,
                text=label,
                fill=COLORS["on_accent"],
                font=("Segoe UI", max(7, int(cell / 3)), "bold"),
                tags="label",
            )

    def _draw_live_overlay(self) -> None:
        cell, frame, ox, oy = self._geometry()

        if len(self._visited_cells) > 1:
            for i in range(len(self._visited_cells) - 1):
                x1, y1 = self._cell_center(*self._visited_cells[i])
                x2, y2 = self._cell_center(*self._visited_cells[i + 1])
                self.create_line(
                    x1,
                    y1,
                    x2,
                    y2,
                    fill=COLORS["path"],
                    width=max(3, int(cell / 5.5)),
                    capstyle=tk.ROUND,
                    joinstyle=tk.ROUND,
                    tags="path",
                )

        if self._ball_pos is not None:
            r, c = self._ball_pos
            if 0 <= r < protocol.SIDE and 0 <= c < protocol.SIDE:
                hx, hy = self._cell_center(r, c)
                radius = max(4, int(cell / 3.5))
                self.create_oval(
                    hx - radius,
                    hy - radius,
                    hx + radius,
                    hy + radius,
                    fill=COLORS["accent"],
                    outline=COLORS["on_accent"],
                    width=2,
                    tags="ball",
                )

        if self._system_state:
            st = str(self._system_state).upper()
            self.create_text(
                ox + frame + 10,
                oy + frame + 10,
                text=f"STATE: {st}",
                anchor=tk.NW,
                fill=COLORS["accent"],
                font=("Segoe UI", 10, "bold"),
            )

        if self._tilt is not None:
            tilt_str = str(self._tilt).upper()
            span = frame * 2 + cell * protocol.SIDE
            self.create_text(
                ox + span - frame - 10,
                oy + frame + 10,
                text=f"TILT: {tilt_str}",
                anchor=tk.NE,
                fill=COLORS["text"],
                font=("Segoe UI", 10, "bold"),
            )

        self.tag_raise("label")

    def _on_resize(self, _event: tk.Event) -> None:
        self.redraw()
