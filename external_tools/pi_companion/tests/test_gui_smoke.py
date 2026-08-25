"""Smoke test for the window itself.

Builds the real window against a simulated board and pumps the Tk event loop by hand, so
widget construction, the drain timer, and the message-to-widget path are all executed. It
does not assert anything about appearance - only that a maze arriving on the queue ends
up in the view without raising.

Needs a display. On a Pi that is the desktop session; headless, use xvfb-run.
"""

import os
import time

import pytest

from pi_comp import protocol, simulator

pytestmark = pytest.mark.skipif(
    not (os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY")),
    reason="needs a display; try xvfb-run",
)

#: Generous, because the first maze waits on the oracle subprocess.
READY_TIMEOUT_S = 20.0


@pytest.fixture
def app():
    try:
        simulator.find_oracle()
    except simulator.OracleNotFound as exc:
        pytest.skip(str(exc))

    tk = pytest.importorskip("tkinter")
    from pi_comp.gui.app import CompanionApp

    try:
        window = CompanionApp(simulate=True, seed=42)
    except tk.TclError as exc:
        pytest.skip(f"no usable display: {exc}")

    try:
        yield window
    finally:
        window._on_close()


def pump_until(window, predicate, timeout=READY_TIMEOUT_S):
    """Run the Tk event loop until predicate() holds or time runs out."""
    deadline = time.monotonic() + timeout

    while time.monotonic() < deadline:
        window.update()
        if predicate():
            return True
        time.sleep(0.02)

    return False


def test_window_builds_and_receives_a_maze(app):
    assert pump_until(app, lambda: app._last_maze is not None), "no maze reached the window"

    maze = app._last_maze
    assert maze["type"] == protocol.MSG_MAZE
    assert len(maze["walls"]) == protocol.SIDE


def test_maze_view_draws_something(app):
    assert pump_until(app, lambda: app._last_maze is not None)

    # The canvas should hold the grid, the frame, and the start/goal labels.
    items = app.maze_view.find_all()
    assert len(items) >= protocol.CELL_COUNT


def test_status_panel_is_populated(app):
    assert pump_until(app, lambda: app._last_maze is not None)
    app.update()

    seed_label = app.status._values["seed"].cget("text")
    assert seed_label == str(app._last_maze["seed"])

    assert app.status._values["solvable"].cget("text") in ("yes", "no")


def test_raw_log_receives_lines(app):
    assert pump_until(app, lambda: app._last_maze is not None)
    app.update()

    contents = app.log.text.get("1.0", "end-1c")
    assert '"type":"maze"' in contents


def test_no_lines_are_dropped_from_a_clean_source(app):
    """The simulator emits exactly what the parser expects; nothing should be rejected."""
    assert pump_until(app, lambda: app._last_maze is not None)

    # Let a few status messages through as well.
    pump_until(app, lambda: len(app._recent) > 5, timeout=5.0)

    assert app._dropped == 0
