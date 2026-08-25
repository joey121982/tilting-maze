"""Tests for the wire format.

The emphasis is on rejection. A dropped line is visible and recoverable; a maze that is
subtly wrong - transposed, or with a path that does not reach the goal - would be drawn
as though it were fine, and nobody would notice.
"""

import json

import pytest

from pi_comp import protocol


# -- helpers ---------------------------------------------------------------------


def make_maze(**overrides):
    """A minimal valid maze: an empty grid and a straight path along row 0 then column 9."""
    walls = [[0] * protocol.SIDE for _ in range(protocol.SIDE)]
    path = [protocol.RIGHT] * (protocol.SIDE - 1) + [protocol.DOWN] * (protocol.SIDE - 1)

    msg = {
        "type": "maze",
        "seed": 42,
        "side": protocol.SIDE,
        "start": [0, 0],
        "goal": [9, 9],
        "solvable": True,
        "walls": walls,
        "path": path,
    }
    msg.update(overrides)
    return msg


def line(msg):
    return json.dumps(msg, separators=(",", ":"))


# -- framing and basic parsing ---------------------------------------------------


def test_blank_lines_are_not_errors():
    assert protocol.parse_line("") is None
    assert protocol.parse_line("   \r\n") is None


def test_trailing_carriage_return_is_tolerated():
    msg = protocol.parse_line('{"type":"status","tick":5}\r')
    assert msg["tick"] == 5


@pytest.mark.parametrize(
    "raw",
    [
        '{"type":"maze"',              # truncated mid-object, the mid-stream connect case
        'e":"status","tick":5}',       # tail of a message, what a late listener lands on
        "not json at all",
        "",                            # handled above, but harmless here
        "\x00\xff garbage",
    ],
)
def test_malformed_lines_raise_or_return_none(raw):
    try:
        assert protocol.parse_line(raw) is None
    except protocol.ProtocolError:
        pass  # the expected outcome for everything except the blank line


def test_non_object_json_is_rejected():
    with pytest.raises(protocol.ProtocolError):
        protocol.parse_line("[1,2,3]")


def test_missing_type_is_rejected():
    with pytest.raises(protocol.ProtocolError):
        protocol.parse_line('{"tick":5}')


def test_unknown_type_passes_through():
    """Forward compatibility: a newer board must not break an older companion."""
    msg = protocol.parse_line('{"type":"imu","ax":1,"ay":2}')
    assert msg["type"] == "imu"


def test_status_without_optional_fields_is_valid():
    """diag and button are omitted until the motor hardware is wired."""
    msg = protocol.parse_line('{"type":"status","tick":1234}')
    assert msg["tick"] == 1234
    assert "diag" not in msg


# -- maze validation -------------------------------------------------------------


def test_valid_maze_round_trips():
    msg = protocol.parse_line(line(make_maze()))
    assert msg["seed"] == 42


def test_wrong_row_count_is_rejected():
    with pytest.raises(protocol.ProtocolError, match="rows"):
        protocol.parse_line(line(make_maze(walls=[[0] * 10] * 9)))


def test_wrong_column_count_is_rejected():
    walls = [[0] * protocol.SIDE for _ in range(protocol.SIDE)]
    walls[3] = [0] * 9
    with pytest.raises(protocol.ProtocolError, match="row 3"):
        protocol.parse_line(line(make_maze(walls=walls)))


def test_non_binary_wall_value_is_rejected():
    walls = [[0] * protocol.SIDE for _ in range(protocol.SIDE)]
    walls[2][7] = 2
    with pytest.raises(protocol.ProtocolError, match="expected 0 or 1"):
        protocol.parse_line(line(make_maze(walls=walls)))


def test_bad_direction_code_is_rejected():
    with pytest.raises(protocol.ProtocolError, match="0..3"):
        protocol.parse_line(line(make_maze(path=[0, 1, 9])))


def test_path_leaving_the_board_is_rejected():
    with pytest.raises(protocol.ProtocolError, match="leaves the board"):
        protocol.parse_line(line(make_maze(path=[protocol.UP])))


def test_path_not_reaching_the_goal_is_rejected():
    with pytest.raises(protocol.ProtocolError, match="not at the goal"):
        protocol.parse_line(line(make_maze(path=[protocol.RIGHT])))


def test_unsolvable_maze_with_empty_path_is_valid():
    msg = protocol.parse_line(line(make_maze(solvable=False, path=[])))
    assert msg["solvable"] is False


def test_wrong_side_is_rejected():
    with pytest.raises(protocol.ProtocolError, match="expected side"):
        protocol.parse_line(line(make_maze(side=12)))


# -- derived helpers -------------------------------------------------------------


def test_walk_path_counts_cells():
    msg = make_maze()
    cells = protocol.walk_path(msg)
    assert len(cells) == len(msg["path"]) + 1
    assert cells[0] == (0, 0)
    assert cells[-1] == (9, 9)


def test_decode_diag():
    assert protocol.decode_diag(protocol.DIAG_OK) == []
    assert protocol.decode_diag(0x20) == ["UNDER_VOLTAGE"]
    # Worst-first ordering, so the most serious flag reads first.
    assert protocol.decode_diag(0x82) == ["SPI_ERROR", "TEMP_WARNING"]
    assert len(protocol.decode_diag(0xFF)) == len(protocol.DIAG_FLAGS)


def test_wall_counts():
    walls = [[0] * protocol.SIDE for _ in range(protocol.SIDE)]
    walls[0][0] = walls[0][1] = walls[5][5] = 1
    assert protocol.wall_counts(walls) == (3, protocol.CELL_COUNT - 3)


def test_pack_walls_matches_the_firmware_bit_layout():
    """Cell (r, c) is bit r*10+c, little-endian within each 32-bit word.

    Mirrors setCell()/testCell() in maze_core.cpp.
    """
    walls = [[0] * protocol.SIDE for _ in range(protocol.SIDE)]

    walls[0][0] = 1        # index 0   -> word 0, bit 0
    walls[0][5] = 1        # index 5   -> word 0, bit 5
    walls[3][2] = 1        # index 32  -> word 1, bit 0
    walls[9][9] = 1        # index 99  -> word 3, bit 3

    words = protocol.pack_walls(walls)

    assert words[0] == (1 << 0) | (1 << 5)
    assert words[1] == (1 << 0)
    assert words[2] == 0
    assert words[3] == (1 << 3)


def test_pack_walls_round_trips():
    walls = [[(r * protocol.SIDE + c) % 3 == 0 for c in range(protocol.SIDE)] for r in range(protocol.SIDE)]
    walls = [[int(v) for v in row] for row in walls]

    words = protocol.pack_walls(walls)
    rebuilt = [
        [(words[(r * protocol.SIDE + c) >> 5] >> ((r * protocol.SIDE + c) & 31)) & 1 for c in range(protocol.SIDE)]
        for r in range(protocol.SIDE)
    ]

    assert rebuilt == walls


# -- status validation -----------------------------------------------------------


def test_status_with_realtime_fields():
    raw = '{"type":"status","tick":123,"diagnostics":0,"button":true,"ball":[2,4],"tilt":"left","state":"solving"}'
    msg = protocol.parse_line(raw)
    assert msg["ball"] == [2, 4]
    assert msg["tilt"] == "left"
    assert msg["state"] == "solving"
    assert msg["button"] is True
    assert msg["diagnostics"] == 0


def test_status_invalid_ball_coordinates():
    raw = '{"type":"status","tick":123,"ball":[10,0]}'
    with pytest.raises(protocol.ProtocolError, match="off board"):
        protocol.parse_line(raw)


def test_status_invalid_tilt_type():
    raw = '{"type":"status","tick":123,"tilt":123}'
    with pytest.raises(protocol.ProtocolError, match="tilt"):
        protocol.parse_line(raw)
