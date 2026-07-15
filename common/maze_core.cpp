#include "maze_core.hpp"

/*
 * Implementarea nucleului. Vezi maze_core.hpp pentru model si constrangeri
 * (fara STL/heap/exceptii; RAM minim in schimbul vitezei - cerinta proiect).
 * Algoritmul si motivele lui, in README.
 */
namespace MazeCore {

constexpr int WORDS = (CELLS + 31) / 32;   /* 4 cuvinte de 32 biti pentru grila */

/* ------------------------------------------------------------------ */
/* biti in grila de pereti                                            */
/* ------------------------------------------------------------------ */

static inline bool testBit(const uint32_t* w, int idx)
{
    return (w[idx >> 5] >> (idx & 31)) & 1u;
}

static inline void setBit(uint32_t* w, int idx)
{
    w[idx >> 5] |= (1u << (idx & 31));
}

static inline void clearBit(uint32_t* w, int idx)
{
    w[idx >> 5] &= ~(1u << (idx & 31));
}

bool cellWall(const Maze& m, int idx)
{
    return testBit(m.walls, idx);
}

/* ------------------------------------------------------------------ */
/* solutia impachetata: 2 biti per pas, 4 pasi per byte               */
/* ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ */
/* directiile, in ordinea codurilor de inclinare (fixate de proiect): */
/* 0 = jos (+1,0), 1 = sus (-1,0), 2 = dreapta (0,+1), 3 = stanga     */
/* Ordinea de parcurgere a vecinilor e MEREU ordinea codurilor, ca    */
/* BFS-ul sa fie determinist si identic cu portul de referinta Python */
/* ------------------------------------------------------------------ */

static const int8_t DR[4] = { 1, -1,  0,  0 };
static const int8_t DC[4] = {  0,  0, 1, -1 };

/* ----------------------------------------------------------------------- */
/* xorshift32 - Sursa noastra: (G. Marsaglia, "Xorshift RNGs", J. Software */
/* 8(14), 2003, p. 4) - 4 bytes de stare, perioada 2^32 - 1.               */
/* Starea 0 e punct fix (ar produce doar 0), de aceea generate()           */
/* inlocuieste seed==0 cu 1.                                               */
/* ----------------------------------------------------------------------- */

static inline uint32_t xorshift32(uint32_t& s)
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

/* ------------------------------------------------------------------ */
/* flood-fill din START peste celulele libere.                        */
/* Intoarce true daca a atins exact `expectedFree` celule, adica      */
/* tot spatiul liber e conectat (deci si finalul e accesibil).        */
/* ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ */
/* BFS start -> final; scrie solutia impachetata si path_len.         */
/* Reconstructia drumului se face in 2 treceri inapoi (intai          */
/* masoara lungimea, apoi scrie codul fiecarui pas direct la pozitia  */
/* lui finala), ca sa nu fie nevoie de un buffer intermediar          */
/* ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ */
/* generare                                                           */
/*                                                                    */
/* Pornim cu toate cele 100 de celule libere si incercam sa zidim     */
/* celulele una cate una, intr-o ordine amestecata (Fisher-Yates).    */
/* Un zid ramane doar daca dupa el TOT spatiul liber ramane conectat  */
/* (verificat cu flood-fill). Start si final nu se zidesc niciodata.  */
/*                                                                    */
/* Cost: O(CELLS) flood-fill-uri a cate O(CELLS) pasi ~ 10^4 operatii */
/* simple;                                                            */
/* ------------------------------------------------------------------ */

void generate(Maze& m, uint32_t seed)
{
    if (seed == 0u) seed = 1u;      /* vezi comentariul xorshift32 */
    m.seed = seed;
    uint32_t rng = seed;

    /* permutarea ordinii de zidire: Fisher-Yates modern (Knuth, TAOCP
     * vol. 2, alg. P). Indexul aleator e luat cu `% (i+1)`: biasul de
     * modulo exista, dar la i<=99 e de ordinul 10^-8 (2^32 % 100 = 96
     * resturi in plus la 4,3 miliarde) - irelevant aici, documentat. */
    uint8_t order[CELLS];
    for (int i = 0; i < CELLS; ++i) order[i] = (uint8_t)i;
    for (int i = CELLS - 1; i > 0; --i) {
        int j = (int)(xorshift32(rng) % (uint32_t)(i + 1));
        uint8_t t = order[i]; order[i] = order[j]; order[j] = t;
    }

    for (int w = 0; w < WORDS; ++w) m.walls[w] = 0u;

    int freeCells = CELLS;
    for (int k = 0; k < CELLS; ++k) {
        int idx = order[k];
        if (idx == START || idx == GOAL) continue;
        setBit(m.walls, idx);
        if (allFreeConnected(m, freeCells - 1)) {
            --freeCells;
        } else {
            clearBit(m.walls, idx);
        }
    }

    solve(m);
}

/* ------------------------------------------------------------------ */
/* serializare JSON "tilting-maze" prin callback, fara stdio.         */
/* ------------------------------------------------------------------ */

static void putStr(const char* s, PutFn put, void* ctx)
{
    while (*s) put(*s++, ctx);
}

static void putU32(uint32_t v, PutFn put, void* ctx)
{
    char buf[10];
    int n = 0;
    do {
        buf[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v != 0u);
    while (n > 0) put(buf[--n], ctx);
}

static int countWalls(const Maze& m)
{
    int count = 0;
    for (int i = 0; i < CELLS; ++i)
        if (testBit(m.walls, i)) ++count;
    return count;
}

void writeJson(const Maze& m, PutFn put, void* ctx)
{
    int walls = countWalls(m);
    int chips = (walls + 2) / 3;    /* ceil(walls/3): 3 motoare per TLE94112 */

    putStr("{\n  \"format\": \"tilting-maze-v3\",\n  \"size\": ", put, ctx);
    putU32((uint32_t)N, put, ctx);
    putStr(",\n  \"seed\": ", put, ctx);
    putU32(m.seed, put, ctx);

    putStr(",\n  \"start\": [", put, ctx);
    putU32((uint32_t)(START / N), put, ctx);
    putStr(", ", put, ctx);
    putU32((uint32_t)(START % N), put, ctx);
    putStr("],\n  \"goal\": [", put, ctx);
    putU32((uint32_t)(GOAL / N), put, ctx);
    putStr(", ", put, ctx);
    putU32((uint32_t)(GOAL % N), put, ctx);
    putStr("],\n  \"solvable\": ", put, ctx);
    putStr(m.solvable ? "true" : "false", put, ctx);

    /* perimetrul nu e in matrice: carcasa fizica, mereu ridicata */
    putStr(",\n  \"perimeter\": \"fixed\",\n  \"matrix\": [", put, ctx);
    for (int r = 0; r < N; ++r) {
        putStr(r ? ",\n    [" : "\n    [", put, ctx);
        for (int c = 0; c < N; ++c) {
            if (c) put(',', ctx);
            put(cellWall(m, r * N + c) ? '1' : '0', ctx);
        }
        put(']', ctx);
    }

    putStr("\n  ],\n  \"solution\": [", put, ctx);
    for (int i = 0; i < (int)m.path_len; ++i) {
        if (i) put(',', ctx);
        putU32((uint32_t)pathStep(m, i), put, ctx);
    }

    putStr("],\n  \"stats\": {\"walls\": ", put, ctx);
    putU32((uint32_t)walls, put, ctx);
    putStr(", \"free\": ", put, ctx);
    putU32((uint32_t)(CELLS - walls), put, ctx);
    putStr(", \"path_length\": ", put, ctx);
    putU32((uint32_t)m.path_len, put, ctx);
    putStr(", \"tle94112_chips_needed\": ", put, ctx);
    putU32((uint32_t)chips, put, ctx);
    putStr("}\n}\n", put, ctx);
}

}
