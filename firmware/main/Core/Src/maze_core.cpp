// TODO:    rename ALL variables used in this file to more descriptive names,
//          remove all unnecessary doxygen comments

// TODO:    changing the maze size from 10x10 to 12x12 in the generator seems to mostly fix
//          the "thick exterior walls" issue. look into this further

#include "maze_core.hpp"

/**
 * @file
 * @brief Implementare MazeCore.
 */

namespace MazeCore {

/** @brief Cuvinte de 32 de biti pentru grila: ceil(CELLS / 32) = 4. */
constexpr int WORDS = (CELLS + 31) / 32;

/** @brief Bitul idx din vectorul w: cuvantul [idx >> 5], bitul (idx & 31). */
static inline bool testBit(const uint32_t* w, int idx)
{
    return (w[idx >> 5] >> (idx & 31)) & 1u;
}

/** @brief Ridica bitul idx (perete pus / casuta vizitata). */
static inline void setBit(uint32_t* w, int idx)
{
    w[idx >> 5] |= (1u << (idx & 31));
}

/** @brief Coboara bitul idx (retrage un zid respins la verificare). */
static inline void clearBit(uint32_t* w, int idx)
{
    w[idx >> 5] &= ~(1u << (idx & 31));
}

bool cellWall(const Maze& m, int idx)
{
    return testBit(m.walls, idx);
}

int pathStep(const Maze& m, int i)
{
    return (m.path[i >> 2] >> ((i & 3) * 2)) & 3;
}

static inline void setPathStep(Maze& m, int i, int code)
{
    uint8_t& b = m.path[i >> 2];
    int sh = (i & 3) * 2;
    b = (uint8_t)((b & ~(3u << sh)) | ((unsigned)code << sh));
}

static const int8_t DR[4] = { 1, -1,  0,  0 };
static const int8_t DC[4] = {  0,  0, 1, -1 };

static inline uint32_t xorshift32(uint32_t& s)
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

static bool allFreeConnected(const Maze& m, int expectedFree)
{
    uint32_t visited[WORDS] = { 0u, 0u, 0u, 0u };
    uint8_t  queue[CELLS];
    int head = 0, tail = 0;

    queue[tail++] = (uint8_t)START;
    setBit(visited, START);
    int seen = 1;

    while (head < tail) {
        int cur = queue[head++];
        int r = cur / N, c = cur % N;
        for (int d = 0; d < 4; ++d) {
            int nr = r + DR[d], nc = c + DC[d];
            if (nr < 0 || nr >= N || nc < 0 || nc >= N) continue;
            int nidx = nr * N + nc;
            if (testBit(m.walls, nidx) || testBit(visited, nidx)) continue;
            setBit(visited, nidx);
            queue[tail++] = (uint8_t)nidx;
            ++seen;
        }
    }
    return seen == expectedFree;
}

static void solve(Maze& m)
{
    uint8_t  prevdir[CELLS];
    uint32_t visited[WORDS] = { 0u, 0u, 0u, 0u };
    uint8_t  queue[CELLS];
    int head = 0, tail = 0;

    queue[tail++] = (uint8_t)START;
    setBit(visited, START);

    bool reached = false;
    while (head < tail) {
        int cur = queue[head++];
        if (cur == GOAL) {
            reached = true;
            break;
        }
        int r = cur / N, c = cur % N;
        for (int d = 0; d < 4; ++d) {
            int nr = r + DR[d], nc = c + DC[d];
            if (nr < 0 || nr >= N || nc < 0 || nc >= N) continue;
            int nidx = nr * N + nc;
            if (testBit(m.walls, nidx) || testBit(visited, nidx)) continue;
            setBit(visited, nidx);
            prevdir[nidx] = (uint8_t)d;
            queue[tail++] = (uint8_t)nidx;
        }
    }

    m.solvable = reached ? 1u : 0u;
    m.path_len = 0u;
    for (unsigned i = 0; i < sizeof(m.path); ++i) m.path[i] = 0u;
    if (!reached) return;

    const int DELTA[4] = { N, -N, 1, -1 };

    int len = 0;
    int cur = GOAL;
    while (cur != START) {
        cur -= DELTA[prevdir[cur]];
        ++len;
    }
    m.path_len = (uint8_t)len;

    cur = GOAL;
    int i = len;
    while (cur != START) {
        int d = prevdir[cur];
        setPathStep(m, --i, d);
        cur -= DELTA[d];
    }
}

void generate(Maze& maze, uint32_t seed = 1)
{
    if (seed == 0u) seed = 1u;      /* vezi comentariul xorshift32 */
    maze.seed = seed;
    uint32_t rng = seed;

    /* permutarea ordinii de zidire (Fisher-Yates) */
    uint8_t order[CELLS];
    for (int i = 0; i < CELLS; ++i) order[i] = (uint8_t)i;
    for (int i = CELLS - 1; i > 0; --i) {
        int j = (int)(xorshift32(rng) % (uint32_t)(i + 1));
        uint8_t t = order[i]; order[i] = order[j]; order[j] = t;
    }

    for (int w = 0; w < WORDS; ++w) maze.walls[w] = 0u;

    int freeCells = CELLS;
    for (int k = 0; k < CELLS; ++k) {
        int idx = order[k];
        if (idx == START || idx == GOAL) continue;
        setBit(maze.walls, idx);
        if (allFreeConnected(maze, freeCells - 1)) {
            --freeCells;
        } else {
            clearBit(maze.walls, idx);
        }
    }

    solve(maze);
}

}   // namespace MazeCore
