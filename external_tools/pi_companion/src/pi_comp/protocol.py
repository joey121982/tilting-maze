"""The wire format between the tilting-maze board and this companion.

This module is the single source of truth for the format on the Python side. Every
constant here is derived from live firmware code, and the source is named so the two can
be checked against each other:

    firmware/main/Core/Inc/maze_core.h      maze geometry, direction codes
    firmware/main/Core/Src/maze_core.cpp    writeJson(), row-major ordering
    firmware/main/Core/Src/main.cpp         message types and cadence
    firmware/main/Drivers/TLE94112/Inc/tle94112.h   SYS_DIAG bit values

Framing
-------
The board sends one compact JSON object per line, LF-terminated. The newline is the frame
delimiter, and that is the entire resync mechanism: a companion that connects mid-message
throws away the partial line it landed on and is correctly synced from the next one.

Two rules keep the link forgiving in the directions that matter:

* A line that does not parse is counted and dropped, never fatal. Line noise on a TTL
  link at 115200 is normal, especially at connect time.
* An unknown ``type`` is ignored rather than rejected, and absent optional fields mean
  "not reported" rather than an error. This is what lets the firmware start sending
  ``diag`` and ``button`` later without breaking an older companion.

Messages
--------
``hello``   {"type":"hello","proto":1,"board":"stm32f103c8"}
``maze``    the 10x10 grid, its seed, and the solution path
``status``  {"type":"status","tick":123456}  plus optional "diag" and "button"
``fault``   {"type":"fault","diag":32,"flags":["UNDER_VOLTAGE"]}
``log``     {"type":"log","level":"info","msg":"..."}
"""

from __future__ import annotations

import json
from typing import Any

#: Protocol version announced in the ``hello`` message.
PROTO_VERSION = 1

#: Maze side length. Fixed by hardware at 100 motorised cells (``MAZE_SIDE_LEN``).
SIDE = 10
CELL_COUNT = SIDE * SIDE

#: Opposite corners, in (row, column). ``START_COORDS`` / ``GOAL_COORDS``.
START = (0, 0)
GOAL = (SIDE - 1, SIDE - 1)

# Direction codes, exactly as documented on pathStep() in maze_core.h.
DOWN, UP, RIGHT, LEFT = 0, 1, 2, 3

DIRECTION_VECTORS = {
    DOWN: (1, 0),
    UP: (-1, 0),
    RIGHT: (0, 1),
    LEFT: (0, -1),
}

DIRECTION_NAMES = {DOWN: "down", UP: "up", RIGHT: "right", LEFT: "left"}

#: TLE94112 SYS_DIAG flags, high bit first so the decoded list reads worst-first.
#: A value of 0 is ``Tle94112::STATUS_OK``.
DIAG_FLAGS: tuple[tuple[int, str], ...] = (
    (0x80, "SPI_ERROR"),
    (0x40, "LOAD_ERROR"),
    (0x20, "UNDER_VOLTAGE"),
    (0x10, "OVER_VOLTAGE"),
    (0x08, "POWER_ON_RESET"),
    (0x04, "TEMP_SHUTDOWN"),
    (0x02, "TEMP_WARNING"),
)
DIAG_OK = 0

MSG_HELLO = "hello"
MSG_MAZE = "maze"
MSG_STATUS = "status"
MSG_FAULT = "fault"
MSG_LOG = "log"

KNOWN_TYPES = frozenset({MSG_HELLO, MSG_MAZE, MSG_STATUS, MSG_FAULT, MSG_LOG})

#: Commands the companion sends back to the board, LF-terminated, one per line.
CMD_NEW = "new"
CMD_SEED = "seed"

#: Largest seed the firmware can hold. MazeCore::generate() takes a uint32_t.
SEED_MAX = 0xFFFFFFFF


def encode_command(action: str, value: int | None = None) -> bytes:
    """Encode one command for the board.

    ``new``          generate a fresh maze, seeding it the usual way
    ``seed <n>``     generate the maze for exactly this seed

    Deliberately plain text rather than the JSON the board sends back. The direction
    matters: the firmware *writes* JSON with putStr() and has no parser, so anything
    JSON-shaped would mean putting one on an STM32F103 to read two commands. A
    newline-terminated line that splits on the first space is a strcmp and an atoi.

    NOTE: nothing consumes these yet. The firmware has no receive path at all, and only
    PL2303 RX -> PA9 is wired - see "Board control" in the companion README for both the
    missing pieces and the RX roadmap. A command sent today goes into the void.

    Raises:
        ValueError: on an unknown action or a seed outside the uint32 range.
    """
    if action == CMD_NEW:
        return b"new\n"

    if action == CMD_SEED:
        if value is None:
            raise ValueError("the seed command needs a value")
        if not 0 <= value <= SEED_MAX:
            raise ValueError(f"seed must be 0..{SEED_MAX}, got {value}")
        return f"seed {value}\n".encode("ascii")

    raise ValueError(f"unknown command {action!r}")


class ProtocolError(ValueError):
    """A line arrived that could not be understood as a message.

    Raised for malformed JSON and for structurally invalid messages of a known type.
    Callers are expected to count these and carry on, not to treat them as fatal.
    """


def decode_diag(diag: int) -> list[str]:
    """Expand a raw SYS_DIAG byte into flag names.

    Returns an empty list when ``diag`` is 0, which is the healthy case.
    """
    return [name for bit, name in DIAG_FLAGS if diag & bit]


def parse_line(line: str) -> dict[str, Any] | None:
    """Parse one received line into a message.

    Returns ``None`` for a blank line, which is not an error - it is what a stray CR or a
    keepalive newline looks like.

    Raises:
        ProtocolError: the line is not JSON, is not a JSON object, has no usable
            ``type``, or is a known type whose contents do not hold up.
    """
    line = line.strip()
    if not line:
        return None

    try:
        msg = json.loads(line)
    except (ValueError, UnicodeDecodeError) as exc:
        raise ProtocolError(f"not valid JSON: {exc}") from exc

    if not isinstance(msg, dict):
        raise ProtocolError(f"expected a JSON object, got {type(msg).__name__}")

    kind = msg.get("type")
    if not isinstance(kind, str):
        raise ProtocolError("message has no string 'type' field")

    # Unknown types pass through untouched so a newer board does not break this app.
    if kind == MSG_MAZE:
        validate_maze(msg)
    elif kind == MSG_STATUS:
        validate_status(msg)
    elif kind == MSG_FAULT:
        _validate_fault(msg)

    return msg


def validate_maze(msg: dict[str, Any]) -> None:
    """Check that a ``maze`` message is structurally sound.

    Deliberately strict, because a maze message that is subtly wrong - a transposed grid,
    a path that leaves the board - would otherwise be drawn as if it were fine, and a
    silently wrong picture is worse than a dropped line.

    Raises:
        ProtocolError: on any structural problem.
    """
    side = msg.get("side", SIDE)
    if side != SIDE:
        raise ProtocolError(f"expected side {SIDE}, got {side!r}")

    walls = msg.get("walls")
    if not isinstance(walls, list) or len(walls) != SIDE:
        raise ProtocolError(f"'walls' must be {SIDE} rows, got {_shape(walls)}")

    for r, row in enumerate(walls):
        if not isinstance(row, list) or len(row) != SIDE:
            raise ProtocolError(f"'walls' row {r} must be {SIDE} values, got {_shape(row)}")
        for value in row:
            if value not in (0, 1):
                raise ProtocolError(f"'walls' row {r} holds {value!r}, expected 0 or 1")

    path = msg.get("path")
    if not isinstance(path, list):
        raise ProtocolError(f"'path' must be a list, got {type(path).__name__}")
    for step in path:
        if step not in DIRECTION_VECTORS:
            raise ProtocolError(f"'path' holds direction {step!r}, expected 0..3")

    for key in ("start", "goal"):
        coords = msg.get(key, START)
        if not isinstance(coords, list) or len(coords) != 2:
            raise ProtocolError(f"'{key}' must be [row, col], got {coords!r}")
        if not all(isinstance(v, int) and 0 <= v < SIDE for v in coords):
            raise ProtocolError(f"'{key}' is off the board: {coords!r}")

    solvable = msg.get("solvable")
    if solvable is not None and not isinstance(solvable, bool):
        raise ProtocolError(f"'solvable' must be a boolean, got {solvable!r}")

    # The grid and the path have to agree with each other. Checking them separately is
    # not enough: a row/column transposition leaves both individually well-formed - right
    # shape, right value range, and the path still ends on the goal, because transposing
    # the grid does not touch the path at all. The only thing that catches it is walking
    # the path across the grid and finding it passing through a raised wall.
    if path:
        cells = walk_path(msg)

        goal = tuple(msg.get("goal", GOAL))
        if cells[-1] != goal:
            raise ProtocolError(f"'path' ends at {cells[-1]}, not at the goal {goal}")

        for i, (r, c) in enumerate(cells):
            if walls[r][c]:
                raise ProtocolError(f"'path' step {i} passes through a wall at ({r}, {c})")


def validate_status(msg: dict[str, Any]) -> None:
    """Check that a status message has valid fields if present."""
    if "ball" in msg and msg["ball"] is not None:
        ball = msg["ball"]
        if not isinstance(ball, list) or len(ball) != 2:
            raise ProtocolError(f"'ball' must be [row, col], got {ball!r}")
        if not all(isinstance(v, int) and 0 <= v < SIDE for v in ball):
            raise ProtocolError(f"'ball' coordinates off board: {ball!r}")

    if "tilt" in msg and msg["tilt"] is not None:
        tilt = msg["tilt"]
        if not isinstance(tilt, str):
            raise ProtocolError(f"'tilt' must be a single string, got {type(tilt).__name__}")

    if "state" in msg and msg["state"] is not None:
        state = msg["state"]
        if not isinstance(state, str):
            raise ProtocolError(f"'state' must be a string, got {state!r}")

    diag = msg.get("diagnostics") if "diagnostics" in msg else msg.get("diag")
    if diag is not None:
        if not isinstance(diag, int) or not 0 <= diag <= 0xFF:
            raise ProtocolError(f"'diagnostics' must be a byte, got {diag!r}")

    if "button" in msg and msg["button"] is not None:
        button = msg["button"]
        if not isinstance(button, bool):
            raise ProtocolError(f"'button' must be a boolean, got {button!r}")


def _validate_fault(msg: dict[str, Any]) -> None:
    diag = msg.get("diagnostics", msg.get("diag"))
    if not isinstance(diag, int) or not 0 <= diag <= 0xFF:
        raise ProtocolError(f"'diag' must be a byte, got {diag!r}")


def walk_path(msg: dict[str, Any]) -> list[tuple[int, int]]:
    """Turn a maze message's direction codes into the cells the ball visits.

    The returned list starts at ``start`` and has one more entry than ``path``.

    Raises:
        ProtocolError: a step walks off the board.
    """
    row, col = tuple(msg.get("start", START))
    cells = [(row, col)]

    for i, step in enumerate(msg.get("path", [])):
        dr, dc = DIRECTION_VECTORS[step]
        row, col = row + dr, col + dc
        if not (0 <= row < SIDE and 0 <= col < SIDE):
            raise ProtocolError(f"'path' step {i} ({DIRECTION_NAMES[step]}) leaves the board")
        cells.append((row, col))

    return cells


def pack_walls(walls: list[list[int]]) -> list[int]:
    """Repack an expanded grid back into the firmware's bitfield layout.

    Mirrors ``setCell``/``testCell`` in maze_core.cpp: cell (r, c) is bit ``r * 10 + c``,
    stored little-endian within a word. Only used for verification - if this round-trips,
    the row-major ordering on the wire matches what the board actually holds.
    """
    words = [0] * ((CELL_COUNT + 31) // 32)

    for r, row in enumerate(walls):
        for c, value in enumerate(row):
            if value:
                idx = r * SIDE + c
                words[idx >> 5] |= 1 << (idx & 31)

    return words


def wall_counts(walls: list[list[int]]) -> tuple[int, int]:
    """Return (raised, free) cell counts for a grid."""
    raised = sum(sum(row) for row in walls)
    return raised, CELL_COUNT - raised


def _shape(value: Any) -> str:
    """Describe a value's shape for an error message, without dumping its contents."""
    if isinstance(value, list):
        return f"a list of {len(value)}"
    return type(value).__name__
