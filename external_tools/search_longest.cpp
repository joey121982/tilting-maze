// Vibecoded file that returns the seeds for the longest paths.
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <string>
#include "maze_core.h"

// Structură pentru a stoca rezultatele
struct SeedResult {
    uint8_t path_len;
    uint32_t seed;
};

// Funcția executată de fiecare thread în paralel
void searchWorker(uint64_t start_seed, uint64_t end_seed, std::vector<SeedResult>& local_top, size_t keep_top_n, std::atomic<uint64_t>& global_processed) {
    // Alocăm o singură structură Maze pe stiva fiecărui thread pentru a evita overhead-ul alocărilor dinamice repetate
    MazeCore::Maze maze;
    uint64_t local_count = 0;

    for (uint64_t seed = start_seed; seed < end_seed; ++seed) {
        // Rulăm generarea și rezolvarea pe seed-ul curent
        MazeCore::generate(maze, static_cast<uint32_t>(seed));
        
        if (maze.solvable) {
            // Dacă drumul curent este mai lung decât cel mai scurt din top-ul nostru local (sau dacă top-ul nu e plin)
            if (local_top.size() < keep_top_n || maze.path_len > local_top.back().path_len) {
                local_top.push_back({maze.path_len, static_cast<uint32_t>(seed)});
                
                // Sortăm descrescător după lungime
                std::sort(local_top.begin(), local_top.end(), [](const SeedResult& a, const SeedResult& b) {
                    return a.path_len > b.path_len;
                });
                
                // Menținem doar top N
                if (local_top.size() > keep_top_n) {
                    local_top.pop_back();
                }
            }
        }

        // Raportăm progresul în batch-uri de 50.000 pentru a preveni cache thrashing 
        // și blocajele pe variabila atomică globală
        if (++local_count >= 50000) {
            global_processed += local_count;
            local_count = 0;
        }
    }
    global_processed += local_count;
}

// Thread separat pentru afișarea progresului în consolă
void monitorProgress(std::atomic<uint64_t>& global_processed, uint64_t total_seeds, std::atomic<bool>& running) {
    auto start_time = std::chrono::high_resolution_clock::now();
    while (running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        uint64_t processed = global_processed.load();
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = now - start_time;
        
        double speed = elapsed.count() > 0 ? (double)processed / elapsed.count() : 0;
        double percent = total_seeds > 0 ? (double)processed / total_seeds * 100.0 : 0;
        
        std::cout << "\rProcessed: " << processed << " / " << total_seeds 
                  << " (" << std::fixed << std::setprecision(2) << percent << "%) | "
                  << "Speed: " << std::fixed << std::setprecision(0) << speed << " seeds/sec" << std::flush;
        
        if (processed >= total_seeds) break;
    }
}

int main(int argc, char* argv[]) {
    uint64_t total_seeds = 10000000; // Valoare implicită: 10 milioane de căutări
    size_t keep_top_n = 10;          // Câte rezultate păstrăm în top

    if (argc >= 2) {
        try {
            total_seeds = std::stoull(argv[1]);
        } catch (...) {
            std::cerr << "Utilizare: " << argv[0] << " [numar_total_seeduri_de_testat]\n";
            return 1;
        }
    }

    // Detecția automată a numărului de nuclee logice disponibile (Hyper-Threading inclus)
    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;

    std::cout << "Starting search across " << total_seeds << " seeds using " << num_threads << " threads...\n";
    std::cout << "Grid size is currently set to: " << (int)MazeCore::MAZE_SIDE_LEN << "x" << (int)MazeCore::MAZE_SIDE_LEN << "\n\n";

    std::vector<std::thread> threads;
    std::vector<std::vector<SeedResult>> thread_tops(num_threads);
    std::atomic<uint64_t> global_processed(0);
    std::atomic<bool> monitor_running(true);

    // Împărțim intervalul total de seed-uri în mod egal între firele de execuție
    uint64_t seeds_per_thread = total_seeds / num_threads;
    
    // Lansăm thread-ul de monitorizare a vitezei
    std::thread monitor_thread(monitorProgress, std::ref(global_processed), total_seeds, std::ref(monitor_running));

    auto start_time = std::chrono::high_resolution_clock::now();

    // Lansăm thread-urile de calcul
    for (unsigned int i = 0; i < num_threads; ++i) {
        uint64_t start = i * seeds_per_thread + 1; // Începem de la seed-ul 1
        uint64_t end = (i == num_threads - 1) ? (total_seeds + 1) : (start + seeds_per_thread);
        threads.emplace_back(searchWorker, start, end, std::ref(thread_tops[i]), keep_top_n, std::ref(global_processed));
    }

    // Așteptăm finalizarea calculelor
    for (auto& t : threads) {
        t.join();
    }

    // Oprim monitorul de progres
    monitor_running = false;
    if (monitor_thread.joinable()) {
        monitor_thread.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> total_elapsed = end_time - start_time;

    // Reducem (Reduce phase) top-urile locale din thread-uri într-un singur top global
    std::vector<SeedResult> global_top;
    for (const auto& t_top : thread_tops) {
        global_top.insert(global_top.end(), t_top.begin(), t_top.end());
    }

    // Sortăm topul global rezultat
    std::sort(global_top.begin(), global_top.end(), [](const SeedResult& a, const SeedResult& b) {
        return a.path_len > b.path_len;
    });

    // Menținem dimensiunea finală cerută
    if (global_top.size() > keep_top_n) {
        global_top.resize(keep_top_n);
    }

    // Afișăm rezultatele
    std::cout << "\n=========================================\n";
    std::cout << " SEARCH COMPLETED in " << std::fixed << std::setprecision(2) << total_elapsed.count() << " seconds\n";
    std::cout << " Average speed: " << std::fixed << std::setprecision(0) << (double)total_seeds / total_elapsed.count() << " seeds/sec\n";
    std::cout << "=========================================\n";
    std::cout << "TOP " << keep_top_n << " LONGEST SOLVABLE PATHS:\n";
    std::cout << "-----------------------------------------\n";
    for (size_t i = 0; i < global_top.size(); ++i) {
        std::cout << "  #" << (i + 1) << " | Seed: " << std::setw(10) << global_top[i].seed 
                  << " | Path Length: " << (int)global_top[i].path_len << " steps\n";
    }
    std::cout << "=========================================\n";

    return 0;
}

// HOW TO RUN
// In directory firmare/main
// g++ -O3 -ICore/Inc search_longest.cpp Core/Src/maze_core.cpp -o search_longest -pthread
// ./search_longest