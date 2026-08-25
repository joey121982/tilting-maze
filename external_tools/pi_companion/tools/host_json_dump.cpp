/**
 * @file host_json_dump.cpp
 * @brief Compiles the firmware's own maze core natively and dumps one maze message.
 *
 * This is the verification harness for the companion app. It links the exact same
 * Core/Src/maze_core.cpp that runs on the STM32, so the JSON it prints is byte-for-byte
 * what the board sends - the only difference is where the character sink writes to.
 * That makes it possible to develop and test the whole receive path with no hardware
 * attached, and it keeps a single implementation of the maze algorithm in the repo.
 *
 * Build and run:
 *     make            # in this directory
 *     ./host_json_dump 42
 *
 * Usage: host_json_dump [seed] [count]
 *     seed   xorshift32 seed, default 42 (generate() rewrites 0 to 1)
 *     count  number of consecutive mazes to emit, default 1; each uses seed + n
 */

#include <cstdio>
#include <cstdlib>

#include "maze_core.h"

/** @brief Character sink that writes straight to stdout. */
static void putStdout(char c, void* /*ctx*/)
{
    std::fputc(c, stdout);
}

int main(int argc, char** argv)
{
    uint32_t seed  = (argc > 1) ? (uint32_t)std::strtoul(argv[1], nullptr, 10) : 42u;
    uint32_t count = (argc > 2) ? (uint32_t)std::strtoul(argv[2], nullptr, 10) : 1u;

    for (uint32_t n = 0; n < count; ++n) {
        MazeCore::Maze maze;
        MazeCore::generate(maze, seed + n);
        MazeCore::writeJson(maze, putStdout, nullptr);
    }

    return 0;
}
