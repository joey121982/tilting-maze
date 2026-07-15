#ifndef MAZE_CORE_HPP
#define MAZE_CORE_HPP

#include <stdint.h>

/*
 * MazeCore - generatorul + rezolvatorul de labirint, nucleu comun PC + STM32.
 *   - matricea e doar interiorul 10x10 = 100 de casute motorizate
 *   - celula 0 = perete jos (bila poate sta), 1 = perete ridicat de motor;
 *   - start = (0,0), final = (9,9) - colturi opuse, conventie pastrata din v2;
 *   - solutia = un vector de coduri de inclinare, un cod per pas de o celula:
 *     0 = jos, 1 = sus, 2 = dreapta, 3 = stanga.
 */
namespace MazeCore {

constexpr int N     = 10;
constexpr int CELLS = N * N;

/* celulele ca index liniar: idx = rand * N + coloana */
constexpr int START = 0;             /* (0,0) */
constexpr int GOAL  = CELLS - 1;     /* (9,9) */

constexpr int MAX_PATH = CELLS - 1;

/*
 * Rezultatul: 48 de bytes in total.
 *   walls: 100 de celule x 1 bit = 4 cuvinte de 32 (16B); bit i = celula i.
 *   path:  fiecare pas e un cod 0..3, deci incape pe 2 biti -> 4 pasi/byte,
 *          ceil(99/4) = 25B. Se citeste cu pathStep(), nu direct.
 */
struct Maze {
    uint32_t seed;                        /* seedul efectiv folosit */
    uint32_t walls[(CELLS + 31) / 32];    /* 16B: 1 = perete ridicat */
    uint8_t  path[(MAX_PATH + 3) / 4];    /* 25B: solutia, 2 biti/pas */
    uint8_t  path_len;                    /* numarul de pasi din solutie */
    uint8_t  solvable;                    /* 1 = BFS a atins finalul */
};

/* citire: e ridicat peretele celulei idx (0..99)? */
bool cellWall(const Maze& m, int idx);

/* citire: codul de directie (0..3) al pasului i (0..path_len-1) */
int pathStep(const Maze& m, int i);

/*
 * Genereaza un labirint din seed si il rezolva (umple toata structura).
 * seed == 0 e inlocuit cu 1: starea 0 e punct fix al xorshift32 (ar produce
 * numai zerouri), deci 0 si 1 dau acelasi labirint - documentat, acceptat.
 *
 * RAM tranzitoriu pe stiva apelantului: ~270B
 */
void generate(Maze& m, uint32_t seed);

/*
 * Serializare JSON caracter cu caracter printr-un
 * callback, ca sa nu depinda de stdio sau de vreun buffer: pe PC callback-ul
 * scrie in fisier, pe placa scrie octetul in UART.
 */
typedef void (*PutFn)(char c, void* ctx);
void writeJson(const Maze& m, PutFn put, void* ctx);

}

#endif
