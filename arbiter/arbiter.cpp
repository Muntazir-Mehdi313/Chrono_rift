#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include "shared_types.h"

SharedGameState* create_shared_memory();
void             destroy_shared_memory(SharedGameState* shm);
void             init_all_entities(SharedGameState* shm);
void             main_game_loop(SharedGameState* shm);
void             setup_arbiter_signals(SharedGameState* shm);

#define ROLL_NUMBER 240847

int main(int, char*[]) {
    std::cout << "============================================\n"
              << "  CHRONO RIFT — Game Arbiter\n"
              << "  CS 2006 OS Project | Phase 4 Build\n"
              << "  Roll: 24i-0847\n"
              << "============================================\n";

    SharedGameState* shm = create_shared_memory();
    shm->arbiter_pid = getpid();
    setup_arbiter_signals(shm);
    srand(ROLL_NUMBER);

    // ── Read party size from command-line arg OR stdin ──────────────────
    int num_players = 0;

    // Check if running non-interactively (piped input)
    if (!isatty(STDIN_FILENO)) {
        std::cin >> num_players;
        if (std::cin.fail() || num_players < 1 || num_players > 4) {
            num_players = 1;  // default to 1 in automated/test mode
        }
    } else {
        while (num_players < 1 || num_players > 4) {
            std::cout << "[ARBITER] Enter party size (1-4): " << std::flush;
            std::cin >> num_players;
            if (std::cin.fail() || num_players < 1 || num_players > 4) {
                std::cin.clear();
                std::cin.ignore(10000, '\n');
                std::cout << "[ARBITER] Invalid. Enter 1-4.\n";
                num_players = 0;
            }
        }
    }

    int num_npcs = rand() % 8 + 2;

    pthread_mutex_lock(&shm->master_mutex);
    shm->num_players         = num_players;
    shm->num_npcs_concurrent = num_npcs;
    pthread_mutex_unlock(&shm->master_mutex);

    std::cout << "[ARBITER] Party=" << num_players << " NPCs=" << num_npcs << "\n";

    init_all_entities(shm);

    std::cout << "[ARBITER] Waiting for HIP and ASP...\n";
    int dots = 0;
    while (shm->hip_pid == 0 || shm->asp_pid == 0) {
        usleep(100000);
        if (++dots % 10 == 0) std::cout << "." << std::flush;
    }
    std::cout << "\n[ARBITER] HIP=" << shm->hip_pid << " ASP=" << shm->asp_pid << "\n";

    pthread_mutex_lock(&shm->master_mutex);
    shm->game_started = 1;
    pthread_mutex_unlock(&shm->master_mutex);
    std::cout << "[ARBITER] game_started=1\n";

    main_game_loop(shm);
    destroy_shared_memory(shm);
    std::cout << "[ARBITER] Clean exit.\n";
    return 0;
}
