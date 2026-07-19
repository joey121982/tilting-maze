// UTILITARY FILE USED FOR TESTING

#include <iostream>
#include "maze_core.h"

namespace {

const char* stepName(int step)
{
    switch (step) {
    case 0: return "D";
    case 1: return "U";
    case 2: return "R";
    case 3: return "L";
    default: return "?";
    }
}

void printMaze(const MazeCore::Maze& maze)
{
    std::cout << "Maze seed: " << maze.seed << '\n';
    for (int r = 0; r < MazeCore::MAZE_SIDE_LEN; ++r) {
        for (int c = 0; c < MazeCore::MAZE_SIDE_LEN; ++c) {
            MazeCore::vec2 coords(r, c);
            std::cout << (MazeCore::cellWall(maze, coords) ? '+' : ' ');
        }
        std::cout << '\n';
    }

    std::cout << "Solution: ";
    if (!maze.solvable) {
        std::cout << "no solution" << '\n';
        return;
    }

    for (int i = 0; i < maze.path_len; ++i) {
        std::cout << stepName(MazeCore::pathStep(maze, i));
        if (i + 1 < maze.path_len) {
            std::cout << ' ';
        }
    }
    std::cout << '\n';
}

}  // namespace

int main(int argc, char* argv[])
{
    uint32_t seed;
    if (argc < 2) {
        seed = 41u;
    } else {
        seed = static_cast<uint32_t>(std::atoi(argv[1]));
    }
    
    MazeCore::Maze maze{};
    MazeCore::generate(maze, seed);
    printMaze(maze);
    
    return 0;
}