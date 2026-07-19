#pragma once

#include <stdint.h>

namespace MazeCore {

struct vec2 {
    int8_t r;   
    int8_t c;

    constexpr vec2(int8_t row, int8_t col) : r(row), c(col) {};
    vec2() {}; // we dont need constexpr here, it doesnt run at compile time

    vec2 operator+(const vec2& other) const { return vec2(r + other.r, c + other.c); }
    vec2 operator-(const vec2& other) const { return vec2(r - other.r, c - other.c); }
    bool operator==(const vec2& other) const { return r == other.r && c == other.c; }
};

constexpr uint8_t MAZE_SIDE_LEN = 10;
constexpr uint8_t CELL_COUNT = MAZE_SIDE_LEN * MAZE_SIDE_LEN;

/** @brief Number of words needed to represent the maze.
 *  Our maze is comprised of 100 reachable cells, so we need 100 bits to represent the walls. We use a 32-bit (unsigned) integer, known as a word from now on,
 *  to represent 32 cells out of our 100. Therefore, we need 4 words (4 * 32 = 128 bits) to hold the entire maze. As you can see, we have 28 extra bits left,
 *  which are left unused.
 */
constexpr uint8_t WORD_COUNT = (CELL_COUNT + 31) / 32;
constexpr vec2 START_COORDS = vec2(0, 0);
constexpr vec2 GOAL_COORDS  = vec2(MAZE_SIDE_LEN - 1, MAZE_SIDE_LEN - 1);

struct Maze {
    uint32_t seed;
    uint32_t walls[WORD_COUNT];
    uint8_t  path[(CELL_COUNT - 1 + 3) / 4];
    uint8_t  path_len;
    bool     solvable;
};

vec2 fromLinear(const uint8_t idx);
uint8_t toLinear(const vec2& v);

/**
 * @brief Reads the wall state of a cell.
 *
 * @param maze Maze filled in by generate()
 * @param coords Cell coordinates, row and column in 0..MAZE_SIDE_LEN-1
 * @return true if the cell is a wall
 *
 * @warning No bounds checking on coords.
 */
bool cellWall(const Maze& maze, vec2 coords);

/**
 * @brief Reads step i of the solution path.
 *
 * @param maze Maze filled in by generate()
 * @param i Step position, 0..path_len-1
 * @return Direction code: 0 = down, 1 = up, 2 = right, 3 = left
 *
 * @warning No bounds checking on i.
 */
uint8_t pathStep(const Maze& maze, uint8_t i);

/**
 * @brief Generate a random maze using seed
 *
 * @param maze Reference to maze object, containing the walls, path, solution length, and solvable flag
 * @param seed Seed used for xorshift rng
 */
void generate(Maze& maze, uint32_t seed);

}   // namespace MazeCore

