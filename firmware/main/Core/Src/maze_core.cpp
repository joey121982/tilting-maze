// TODO:    changing the maze size from 10x10 to 12x12 in the generator seems to mostly fix
//          the "thick exterior walls" issue. look into this further

#include "maze_core.h"

namespace MazeCore {


/** @brief Returns the bit representing the given coordinate.
 *
 * @param[in] words array of words representing the maze
 * @param[in] coords the cell's coordinates
 * 
 *  Example of how it works:
 *  Let's say we want to see if (4, 5) has a wall. We get the linear index of (4, 5), which is 45.
 *  We divide 45 by 32, which gives us 1. So the bit of cell (4, 5) is somewhere inside the second word (walls[1]). Then, we modulo by 32 to get the position
 *  of the target bit inside our word, and we move the target bit to the end of the word through a right shift. Then we cancel every other bit except the last one
 *  through a bitwise AND with 1u. If the result is 1, then the wall is present, otherwise it is not.
 */
static inline bool testCell(const uint32_t* words, vec2 coords)
{
    uint8_t idx = toLinear(coords);
    return (words[idx >> 5] >> (idx & 31)) & 1u;
}

static inline void setCell(uint32_t* words, vec2 coords)
{
    uint8_t idx = toLinear(coords);
    words[idx >> 5] |= (1u << (idx & 31));
}

static inline void clearCell(uint32_t* words, vec2 coords)
{
    uint8_t idx = toLinear(coords);
    words[idx >> 5] &= ~(1u << (idx & 31));
}

vec2 fromLinear(const uint8_t idx)
{
    return vec2((int8_t)(idx / MAZE_SIDE_LEN), (int8_t)(idx % MAZE_SIDE_LEN));
}

uint8_t toLinear(const vec2& coords)
{
    return (uint8_t)(coords.r * MAZE_SIDE_LEN + coords.c);
}

bool cellWall(const Maze& maze, vec2 coords)
{
    return testCell(maze.walls, coords);
}

uint8_t pathStep(const Maze& maze, uint8_t i)
{
    return (maze.path[i >> 2] >> ((i & 3) * 2)) & 3;
}

/** @brief Sets the direction code for a step in the path.
 *
 *  @param[in] maze maze object
 *  @param[in] idx index of the step in the path
 *  @param[in] code direction code to set (0 = down, 1 = up, 2 = right, 3 = left)
 *
 *  Similar bit manipulation logic to the testCell function. The path step is represented on a 2-bit code, so we need to shift by 2 bits for each step.
 */
static inline void setPathStep(Maze& maze, int idx, int code)
{
    uint8_t& byte = maze.path[idx >> 2];
    int shift_amount = (idx & 3) * 2;
    byte = (uint8_t)((byte & ~(3u << shift_amount)) | ((unsigned)code << shift_amount));
}

static const vec2 DIRECTION_VECTOR[4] = { vec2(1, 0), vec2(-1, 0), vec2(0, 1), vec2(0, -1) };

static inline uint32_t xorshift32(uint32_t& seed)
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}
/** @brief checks if all free cells are connected. it checks if the free cells that can be reached from the start match the number of expected free cells, instead of checking
 * if the goal is reachable from the start. a better logic, which eliminates the possibility of disconencted free cells.
 */
static bool allFreeConnected(const Maze& maze, int expected_free)
{
    uint32_t    visited[WORD_COUNT]; // visited cell history bitset
    vec2        queue[CELL_COUNT];
    uint8_t     head = 0;
    uint8_t     tail = 0;

    for (uint8_t w = 0; w < WORD_COUNT; ++w) visited[w] = 0u;
    queue[tail++] = START_COORDS;
    setCell(visited, START_COORDS);
    uint8_t seen = 1;

    while (head < tail) { // basic BFS
        vec2 curr_cell = queue[head++];
        for (uint8_t d = 0; d < 4; ++d) {
            vec2 next_cell = curr_cell + DIRECTION_VECTOR[d];
            if (next_cell.r < 0 || next_cell.r >= MAZE_SIDE_LEN || next_cell.c < 0 || next_cell.c >= MAZE_SIDE_LEN) continue;
            if (testCell(maze.walls, next_cell) || testCell(visited, next_cell)) continue;
            setCell(visited, next_cell); // marks as visited
            queue[tail++] = next_cell;
            ++seen;
        }
    }
    return seen == expected_free;
}

/** @brief solves the maze */
static void solve(Maze& maze)
{
    uint8_t  prevdir[CELL_COUNT];
    uint32_t visited[WORD_COUNT];
    vec2     queue[CELL_COUNT]; // BFS queue
    uint8_t head = 0;
    uint8_t tail = 0;

    for (uint8_t w = 0; w < WORD_COUNT; ++w) visited[w] = 0u;
    queue[tail++] = START_COORDS;
    setCell(visited, START_COORDS);

    bool reached = false;
    while (head < tail) {
        vec2 curr_cell = queue[head++];
        if (curr_cell == GOAL_COORDS) {
            reached = true;
            break;
        }

        for (uint8_t d = 0; d < 4; ++d) {
            vec2 next_cell = curr_cell + DIRECTION_VECTOR[d];
            if (next_cell.r < 0 || next_cell.r >= MAZE_SIDE_LEN || next_cell.c < 0 || next_cell.c >= MAZE_SIDE_LEN) continue;
            if (testCell(maze.walls, next_cell) || testCell(visited, next_cell)) continue;
            setCell(visited, next_cell);
            prevdir[toLinear(next_cell)] = (uint8_t)d;
            queue[tail++] = next_cell;
        }
    }

    maze.solvable = reached;
    maze.path_len = 0u;
    for (unsigned i = 0; i < sizeof(maze.path); ++i) maze.path[i] = 0u;
    if (!reached) return;

    uint8_t len = 0;
    vec2 curr_cell = GOAL_COORDS;
    while (!(curr_cell == START_COORDS)) {
        curr_cell = curr_cell - DIRECTION_VECTOR[prevdir[toLinear(curr_cell)]];
        ++len;
    }
    maze.path_len = (uint8_t)len;

    curr_cell = GOAL_COORDS;
    uint8_t i = len;
    while (!(curr_cell == START_COORDS)) {
        int direction = prevdir[toLinear(curr_cell)];
        setPathStep(maze, --i, direction);
        curr_cell = curr_cell - DIRECTION_VECTOR[direction];
    }
}

/** @brief it initializes (builds and solves) a maze struct */
void generate(Maze& maze, uint32_t seed)
{
    if (seed == 0u) seed = 1u;
    maze.seed = seed;
    uint32_t rng = seed;

    uint8_t order[CELL_COUNT];
    for (uint8_t i = 0; i < CELL_COUNT; ++i) order[i] = (uint8_t)i;
    for (uint8_t i = CELL_COUNT - 1; i > 0; --i) {
        uint32_t j = (uint32_t)(xorshift32(rng) % (uint32_t)(i + 1));
        uint8_t t = order[i]; 
        order[i] = order[j]; 
        order[j] = t;
    }

    for (uint8_t w = 0; w < WORD_COUNT; ++w) maze.walls[w] = 0u;

    uint8_t freeCells = CELL_COUNT;
    for (uint8_t k = 0; k < CELL_COUNT; ++k) {
        vec2 coords = fromLinear(order[k]);
        if (coords == START_COORDS || coords == GOAL_COORDS) continue;
        setCell(maze.walls, coords);
        if (allFreeConnected(maze, freeCells - 1)) {
            --freeCells;
        } else {
            clearCell(maze.walls, coords);
        }
    }

    solve(maze);
}

}   // namespace MazeCore
