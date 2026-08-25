"""Cross-checks this parser against the firmware's own serialiser.

These tests run the host oracle, which links the same Core/Src/maze_core.cpp that runs on
the STM32. If they pass, the board's output and this app agree - without a board.

Skipped when the oracle has not been built. Build it with:

    make -C tools
"""

import subprocess

import pytest

from pi_comp import protocol, simulator


@pytest.fixture(scope="module")
def oracle():
    try:
        return simulator.find_oracle()
    except simulator.OracleNotFound as exc:
        pytest.skip(str(exc))


def dump(oracle, seed, count=1):
    result = subprocess.run(
        [str(oracle), str(seed), str(count)],
        capture_output=True,
        text=True,
        check=True,
    )
    return [ln for ln in result.stdout.splitlines() if ln.strip()]


# -- the firmware's output is accepted -------------------------------------------

@pytest.mark.parametrize("seed", [1, 42, 1000, 65535, 4294967295])
def test_firmware_output_parses(oracle, seed):
    (line,) = dump(oracle, seed)
    msg = protocol.parse_line(line)

    assert msg["type"] == protocol.MSG_MAZE
    assert msg["side"] == protocol.SIDE
    assert len(msg["walls"]) == protocol.SIDE


def test_many_consecutive_seeds_all_parse(oracle):
    """Broad sweep: every maze the generator can produce must survive validation."""
    lines = dump(oracle, 1, 200)
    assert len(lines) == 200

    for line in lines:
        msg = protocol.parse_line(line)
        assert msg["walls"] is not None


def test_seed_zero_is_rewritten_to_one(oracle):
    """generate() maps seed 0 to 1, since xorshift32 cannot leave zero."""
    (zero,) = dump(oracle, 0)
    (one,) = dump(oracle, 1)
    assert protocol.parse_line(zero) == protocol.parse_line(one)


def test_solution_reaches_the_goal(oracle):
    (line,) = dump(oracle, 42)
    msg = protocol.parse_line(line)

    cells = protocol.walk_path(msg)
    assert cells[0] == tuple(msg["start"])
    assert cells[-1] == tuple(msg["goal"])


def test_solution_never_crosses_a_wall(oracle):
    """The path and the grid have to agree, or one of the two is being read wrong."""
    for line in dump(oracle, 7, 50):
        msg = protocol.parse_line(line)
        walls = msg["walls"]

        for r, c in protocol.walk_path(msg):
            assert walls[r][c] == 0, f"seed {msg['seed']}: path crosses a wall at {(r, c)}"


def test_start_and_goal_are_always_free(oracle):
    for line in dump(oracle, 100, 50):
        msg = protocol.parse_line(line)
        walls = msg["walls"]

        assert walls[msg["start"][0]][msg["start"][1]] == 0
        assert walls[msg["goal"][0]][msg["goal"][1]] == 0


# -- orientation -----------------------------------------------------------------

@pytest.mark.parametrize("seed", [42, 43, 44, 45, 46, 100, 777, 31337])
def test_transposed_grid_is_rejected(oracle, seed):
    """The guard against a row/column mix-up.

    Reading the wire format column-major instead of row-major produces a grid that is
    still well-formed on its own - right shape, right value range - and the path still
    ends on the goal, because transposing the grid does not touch the path. Only walking
    the path across the grid catches it. Run over several seeds so this does not pass by
    luck on one lucky maze.
    """
    (line,) = dump(oracle, seed)
    msg = protocol.parse_line(line)

    walls = msg["walls"]
    transposed = [[walls[r][c] for r in range(protocol.SIDE)] for c in range(protocol.SIDE)]

    # If a maze happened to be symmetric, transposing it would prove nothing.
    assert transposed != walls, f"seed {seed} is symmetric; pick another"

    msg["walls"] = transposed
    with pytest.raises(protocol.ProtocolError, match="through a wall"):
        protocol.validate_maze(msg)


def test_bitfield_round_trip_matches_firmware_layout(oracle):
    """Re-pack the grid the way the firmware stores it and expand it again.

    Confirms the wire ordering can reconstruct maze.walls exactly, which is what makes
    the expanded form a faithful view of the packed one rather than a reinterpretation.
    """
    for line in dump(oracle, 500, 20):
        msg = protocol.parse_line(line)
        walls = msg["walls"]

        words = protocol.pack_walls(walls)
        assert len(words) == (protocol.CELL_COUNT + 31) // 32

        rebuilt = []
        for r in range(protocol.SIDE):
            row = []
            for c in range(protocol.SIDE):
                idx = r * protocol.SIDE + c
                row.append((words[idx >> 5] >> (idx & 31)) & 1)
            rebuilt.append(row)

        assert rebuilt == walls
