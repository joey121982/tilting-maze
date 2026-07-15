#ifndef MAZE_CORE_HPP
#define MAZE_CORE_HPP

#include <stdint.h>

/**
 * @file
 * @brief generatorul + rezolvatorul de labirint al proiectului
 *        tilting-maze: nucleu portabil, fara dependinte de platforma.
 *
 * Aici e doar contractul public; modelul si conventiile sunt documentate
 * pe namespace-ul MazeCore (mai jos), iar detaliile de implementare si
 * sursele algoritmilor in maze_core.cpp.
 */

/**
 * @namespace MazeCore
 * @brief Generare de labirint din seed + rezolvare BFS + serializare JSON,
 *        fara STL/heap/exceptii
 *
 *  - matricea e doar interiorul de N x N = 100 de casute motorizate;
 *    perimetrul NU e in matrice - e carcasa fizica, mereu ridicata;
 *  - o celula: 0 = perete jos (bila poate sta), 1 = perete ridicat de motor;
 *  - adresare cu index liniar, pe randuri: idx = rand * N + coloana;
 *  - start = (0,0), final = (9,9) - colturi opuse;
 *  - solutia = un sir de coduri de inclinare a tablei, un cod per pas de o
 *    celula: 0 = jos, 1 = sus, 2 = dreapta, 3 = stanga.
 *  - determinist bit-cu-bit: acelasi seed da acelasi labirint si acelasi
 *    JSON pe orice platforma.
 */
namespace MazeCore {

/** @brief Latura interiorului motorizat, in casute. */
constexpr int N     = 10;

/** @brief Numarul de casute motorizate. */
constexpr int CELLS = N * N;

/** @brief Startul bilei, ca index liniar: casuta (0,0). */
constexpr int START = 0;

/** @brief Finalul, ca index liniar: casuta (9,9), coltul opus. */
constexpr int GOAL  = CELLS - 1;

/**
 * @brief Cati pasi poate avea, cel mult, solutia. (BFS)
 *
 * @warning Nume identic cu macro-ul MAX_PATH din <windows.h> (= 260).
 *          Constanta e in namespace, dar un .cpp de PC care include
 *          windows.h INAINTEA acestui header ar sparge textual declaratia
 *          (preprocesorul nu vede namespace-uri). Pe firmware nu e cazul.
 */
constexpr int MAX_PATH = CELLS - 1;

/**
 * @brief Un labirint generat si rezolvat. sizeof == 48 de bytes, fix.
 *
 * 4 (seed) + 16 (walls) + 25 (path) + 1 (path_len) + 1 (solvable) = 47 de bytes utili, + 1 byte padding
 *
 * POD, fara constructor: generate() umple toate campurile, inclusiv bitii
 * nefolositi din path (zerouiti).
 */
struct Maze {
    /** @brief Seedul efectiv folosit (0 se inlocuieste cu 1 - vezi generate()). */
    uint32_t seed;

    /**
     * @brief Grila de pereti: 100 de casute x 1 bit = 4 cuvinte de 32 (16B).
     *
     * Casuta idx sta in bitul (idx & 31) al cuvantului [idx >> 5];
     * 1 = perete ridicat. Se citeste cu cellWall().
     */
    uint32_t walls[(CELLS + 31) / 32];

    /**
     * @brief Solutia impachetata: 2 biti per pas (codurile-s 0..3).
     *
     * Pasul i sta in cei 2 biti de la pozitia (i & 3) * 2 din byte-ul
     * [i >> 2]. Se citeste cu pathStep(), nu direct.
     */
    uint8_t  path[(MAX_PATH + 3) / 4];

    /** @brief Cati pasi are solutia (0..MAX_PATH); 0 si cand nu exista drum. */
    uint8_t  path_len;

    /**
     * @brief 1 = BFS a atins finalul; 0 = nu.
     *
     * Dupa generate() e mereu 1 prin constructie (finalul nu se zideste
     * niciodata, iar spatiul liber ramane conectat), dar campul se masoara
     * cu BFS la fiecare generare - nu se presupune.
     */
    uint8_t  solvable;
};

/**
 * @brief Starea peretelui de la idx.
 *
 * @param[in] m   labirint umplut de generate()
 * @param[in] idx index liniar de casuta, 0..CELLS-1
 * @return true = perete ridicat
 *
 * @warning NO SANITY CHECK!!!!!!
 */
bool cellWall(const Maze& m, int idx);

/**
 * @brief Codul de inclinare al pasului i din solutie.
 *
 * @param[in] m labirint umplut de generate()
 * @param[in] i pozitia pasului, 0..path_len-1
 * @return codul directiei: 0 = jos, 1 = sus, 2 = dreapta, 3 = stanga
 *
 * @warning NO SANITY CHECK!!!!!!
 */
int pathStep(const Maze& m, int i);

/**
 * @brief Genereaza labirintul din seed si il rezolva.
 *
 * Algoritmul, pe scurt (detaliile si sursele, in maze_core.cpp): toate
 * casutele pornesc libere; o permutare Fisher-Yates peste xorshift32 da
 * ordinea in care se incearca zidirea lor, iar un zid ramane doar daca
 * dupa el tot spatiul liber ramane conectat (verificat cu flood-fill) -
 * deci finalul ramane mereu accesibil. La sfarsit, un BFS scrie drumul
 * minim in path / path_len / solvable.
 *
 * Determinist
 *
 * @param[out] m
 * @param[in] seed 
 */
void generate(Maze& m, uint32_t seed);

/**
 * @brief Consumator de cate un octet.
 *
 * Decupleaza serializarea de I/O: nucleul nu depinde de stdio si nu cere
 * niciun buffer.
 *
 * @param c   octetul de scris
 * @param ctx contextul primit de writeJson(), pasat inapoi neatins
 */
typedef void (*PutFn)(char c, void* ctx);

/**
 * @brief Serializarea, octet cu
 *        octet prin put(), fara stdio si fara buffer.
 *
 * @code{.json}
 * {
 *   "format": "tilting-maze-v3",
 *   "size": 10,
 *   "seed": 42,
 *   "start": [0, 0],
 *   "goal": [9, 9],
 *   "solvable": true,
 *   "perimeter": "fixed",
 *   "matrix": [
 *     [0,1,0, ...],
 *     ...
 *   ],
 *   "solution": [2,0,0, ...],
 *   "stats": {"walls": 43, "free": 57, "path_length": 18,
 *             "tle94112_chips_needed": 15}
 * }
 * @endcode
 *
 * "perimeter": "fixed" marcheaza ca perimetrul nu apare in matrice - e
 * carcasa fizica, mereu ridicata. "tle94112_chips_needed" =
 * ceil(walls / 3), la 3 motoare comandate de un TLE94112 (schema de
 * comanda a proiectului). Iesirea se incheie cu un newline.
 *
 * @param[in] m   labirint umplut de generate()
 * @param[in] put apelat o data pentru fiecare octet, in ordinea emisa
 * @param[in] ctx pasat neatins fiecarui apel put()
 */
void writeJson(const Maze& m, PutFn put, void* ctx);

}

#endif /* MAZE_CORE_HPP */
