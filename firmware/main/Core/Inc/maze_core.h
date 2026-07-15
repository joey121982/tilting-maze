#pragma once

#include <stdint.h>

namespace MazeCore {

// TODO: rename to MAZE_SIDE_LEN
constexpr int N     = 10;

// TODO: rename to CELL_COUNT
constexpr int CELLS = N * N;

// TODO: rename to START_COORDS, replace type with a vec2 struct
constexpr int START = 0;

// TODO: rename to GOAL_COORDS, replace type with a vec2 struct
constexpr int GOAL  = CELLS - 1;

// TODO: remove this, why is this even needed, its referenced once 6 lines below... 
constexpr int MAX_PATH = CELLS - 1;

struct Maze {
    uint32_t seed;
    uint32_t walls[(CELLS + 31) / 32];
    uint8_t  path[(MAX_PATH + 3) / 4];
    uint8_t  path_len;
    uint8_t  solvable;  // TODO: swap to boolean
};

// TODO: rewrite doxygen to english and simplify
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

// TODO: rewrite doxygen to english and simplify
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
 * @brief Generate a random maze using seed
 * 
 * @param maze Reference to maze object
 * @param seed Seed used for xorshift rng
 */
void generate(Maze& maze, uint32_t seed);

}   // namespace MazeGen

