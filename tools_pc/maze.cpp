// maze.cpp — CLI-ul de PC al generatorului de labirint.
//
// Toata logica (generare, rezolvare, serializare JSON) traieste
// in nucleul comun ../common/maze_core.cpp, care se compileaza identic si in
// firmware-ul STM32. Fisierul de fata e doar de PC: argumente, ceas ca seed implicit, scriere in fisier.
//
// Dimensiunea NU mai e reglabila: matricea e fixata la 10x10 de hardware
// (100 de casute motorizate; perimetrul e carcasa fixa, in afara matricei).
//
// Compilare:  Windows - build.bat   (sau: g++ -std=c++14 -O2 -o maze maze.cpp ../common/maze_core.cpp)
// Rulare:     maze --seed 42 --out maze.json

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "../common/maze_core.hpp"

// callback-ul de serializare: scrie octetul in FILE-ul din ctx
static void putFile(char ch, void* ctx)
{
    fputc(ch, (FILE*)ctx);
}

int main(int argc, char** argv)
{
    uint32_t seed = (uint32_t)time(nullptr);
    const char* out_path = nullptr;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--seed") && i + 1 < argc) {
            seed = (uint32_t)strtoul(argv[++i], nullptr, 10);
        } else if (!strcmp(argv[i], "--out") && i + 1 < argc) {
            out_path = argv[++i];
        } else if (!strcmp(argv[i], "--size")) {
            fprintf(stderr,
                    "--size nu mai exista: matricea e fixata la %dx%d de hardware\n"
                    "(100 de casute motorizate; vezi EXPLICATIE_MAZE_GENERATOR.md §2).\n",
                    MazeCore::N, MazeCore::N);
            return 1;
        } else if (!strcmp(argv[i], "--help")) {
            printf("Utilizare: maze [--seed S] [--out fisier.json]\n"
                   "Genereaza si rezolva un labirint %dx%d (format tilting-maze-v3).\n",
                   MazeCore::N, MazeCore::N);
            return 0;
        }
    }

    MazeCore::Maze m;
    MazeCore::generate(m, seed);

    FILE* f = stdout;
    if (out_path != nullptr) {
        f = fopen(out_path, "wb");
        if (f == nullptr) {
            fprintf(stderr, "Nu pot scrie in %s\n", out_path);
            return 1;
        }
    }
    MazeCore::writeJson(m, putFile, f);
    if (f != stdout) fclose(f);

    return m.solvable ? 0 : 2;
}
