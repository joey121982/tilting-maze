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

/**
 * @brief Scrie codul (0..3) pasului i in solutia impachetata.
 *
 * Read-modify-write pe byte-ul [i >> 2]: sterge intai cei 2 biti ai
 * pasului, apoi ii pune codul - corect indiferent de valoarea dinainte.
 */
static inline void setPathStep(Maze& m, int i, int code)
{
    uint8_t& b = m.path[i >> 2];
    int sh = (i & 3) * 2;
    b = (uint8_t)((b & ~(3u << sh)) | ((unsigned)code << sh));
}

/**
 * @name Directiile, in ordinea codurilor de inclinare (fixate de proiect)
 *
 * 0 = jos (+1,0), 1 = sus (-1,0), 2 = dreapta (0,+1), 3 = stanga (0,-1);
 * DR = rand, DC = coloana
 * @{
 */
static const int8_t DR[4] = { 1, -1,  0,  0 };
static const int8_t DC[4] = {  0,  0, 1, -1 };
/** @} */

/**
 * @brief xorshift32 - 4 bytes de stare, perioada 2^32 - 1.
 *
 * Sursa noastra: G. Marsaglia, "Xorshift RNGs", Journal of Statistical
 * Software 8(14), 2003, p. 4 - tripletul de shift-uri (13, 17, 5).
 * Starea 0 e punct fix (ar produce numai zerouri); de aceea generate()
 * inlocuieste seed == 0 cu 1.
 *
 * @param[in,out] s starea, avansata pe loc
 * @return noua stare, folosita direct ca numarul aleator curent
 */
static inline uint32_t xorshift32(uint32_t& s)
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

/**
 * @brief Flood-fill (BFS) din (0,0) peste casutele libere
 *
 * @param[in] m            grila curenta
 * @param[in] expectedFree cate casute libere exista in total in grila
 * @return true daca BFS-ul a atins exact expectedFree casute - adica tot
 *         spatiul liber e conectat, deci si finalul ramane accesibil din start
 */
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

/**
 * @brief BFS start -> final: umple path (impachetat), path_len si solvable.
 *
 * prevdir[c] = codul directiei prin care BFS a atins prima data casuta c
 * (adica c = casuta-precedenta + DELTA[cod]); ramane nescris pentru START.
 * BFS pe graf neponderat gaseste un drum minim; intre drumuri minime de
 * lungime egala alege determinist, dupa ordinea vecinilor (vezi DR/DC).
 *
 * Reconstructia merge inapoi de la GOAL, prin cur -= DELTA[prevdir[cur]],
 * si se face in 2 treceri (intai masoara lungimea, apoi scrie codul
 * fiecarui pas direct la pozitia lui finala), ca sa nu fie nevoie de un
 * buffer intermediar de inca MAX_PATH bytes.
 *
 * path se zerouieste integral inainte de scriere: bitii de dupa ultimul
 * pas raman 0, deci structura e comparabila byte-la-byte intre platforme.
 *
 * Daca finalul n-ar fi accesibil (dupa generate() nu se intampla, prin
 * constructie): solvable = 0, path_len = 0, path ramane tot 0.
 *
 * Stiva proprie: prevdir (100B) + visited (16B) + queue (100B) + scalari.
 */
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

/**
 * Implementarea generarii - contractul e pe declaratie, in maze_core.hpp.
 *
 * Pornim cu toate cele 100 de casute libere si incercam sa le zidim una
 * cate una, in ordinea amestecata din order; un zid ramane doar daca dupa
 * el tot spatiul liber ramane conectat (allFreeConnected), altfel se
 * retrage imediat. Start si final nu se zidesc niciodata - deci labirintul
 * iese mereu rezolvabil.
 *
 * Cost: O(CELLS) flood-fill-uri a cate O(CELLS) pasi ~ 10^4 operatii
 * simple. Stiva de varf: 408B masurati (sursa in nota din header) =
 * order (100B) + frame-urile ajunse inline ale lui allFreeConnected /
 * solve + scalari.
 */
void generate(Maze& m, uint32_t seed)
{
    if (seed == 0u) seed = 1u;      /* vezi comentariul xorshift32 */
    m.seed = seed;
    uint32_t rng = seed;

    /* permutarea ordinii de zidire (Fisher-Yates) */
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

/** @brief Emite sirul s, octet cu octet, prin put. */
static void putStr(const char* s, PutFn put, void* ctx)
{
    while (*s) put(*s++, ctx);
}

/**
 * @brief Emite v in zecimal, fara semn si fara zerouri de umplutura.
 *
 * Cifrele se strang in buf de la coada si se emit in ordine inversa.
 */
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

/** @brief Numara peretii ridicati: populatia de biti 1 din walls. */
static int countWalls(const Maze& m)
{
    int count = 0;
    for (int i = 0; i < CELLS; ++i)
        if (testBit(m.walls, i)) ++count;
    return count;
}

/**
 * Implementarea serializarii - formatul complet, cu exemplu, e pe
 * declaratie in maze_core.hpp. Totul iese prin put(), octet cu octet.
 */
void writeJson(const Maze& m, PutFn put, void* ctx)
{
    int walls = countWalls(m);
    int chips = (walls + 2) / 3;

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
