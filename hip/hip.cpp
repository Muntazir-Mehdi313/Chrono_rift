// ============================================================================
// hip/hip.cpp — Human Interfacing Process (Phase 5 + 10 + 11)
// CS 2006 Operating Systems | Spring 2026 | Chrono Rift
// Roll Number: 24i-0847  |  Partner: 24i-0650
// ============================================================================
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <cerrno>
#include "shared_types.h"

SharedGameState* attach_shared_memory_with_retry();
void             detach_shared_memory(SharedGameState* shm);
void             display_player_menu(SharedGameState* shm, int player_index);
void             read_player_action(SharedGameState* shm, int player_index,
                                    int* action_type, int* target_id,
                                    int* weapon_slot,  int* lt_weapon_id);
void             handle_weapon_drop_choice(SharedGameState* shm, int player_index);

static SharedGameState*      g_shm         = nullptr;
static volatile sig_atomic_t g_should_exit = 0;

// SIGUSR1 — Stun: pause exactly 3 seconds (Phase 10)
static void hip_sigusr1_handler(int) {
    const char a[] = "[HIP] STUNNED 3s\n";
    write(STDOUT_FILENO, a, sizeof(a)-1);
    sleep(3);
    const char b[] = "[HIP] Stun ended\n";
    write(STDOUT_FILENO, b, sizeof(b)-1);
}

// SIGTERM — graceful shutdown
static void hip_sigterm_handler(int) {
    g_should_exit = 1;
    if (g_shm) for (int i = 0; i < MAX_PLAYERS; i++) sem_post(&g_shm->turn.turn_notification);
}

static void setup_hip_signals() {
    struct sigaction sa; sigemptyset(&sa.sa_mask); sa.sa_flags = 0;
    sa.sa_handler = hip_sigusr1_handler; sigaction(SIGUSR1, &sa, nullptr);
    sa.sa_handler = hip_sigterm_handler; sigaction(SIGTERM, &sa, nullptr);
}

typedef struct { SharedGameState* shm; int player_index; } PlayerThreadArgs;

static void* player_thread_func(void* arg) {
    PlayerThreadArgs* a = (PlayerThreadArgs*)arg;
    SharedGameState*  shm = a->shm;
    int my = a->player_index;
    std::cout << "[HIP] Player thread " << my << " started\n";

    while (true) {
        int ret;
        do { ret = sem_wait(&shm->turn.turn_notification); }
        while (ret == -1 && errno == EINTR && !g_should_exit);
        if (g_should_exit) break;

        pthread_mutex_lock(&shm->master_mutex);
        int over = (shm->game_result != GAME_ONGOING);
        int mine = (shm->turn.current_entity_is_player == 1 &&
                    shm->turn.current_entity_slot      == my);
        pthread_mutex_unlock(&shm->master_mutex);

        if (over) break;
        if (!mine) { sem_post(&shm->turn.turn_notification); continue; }

        handle_weapon_drop_choice(shm, my);
        if (shm->game_result != GAME_ONGOING || g_should_exit) break;

        int action=ACTION_NONE, target=-1, wslot=-1, ltwid=WEAPON_NONE;
        display_player_menu(shm, my);
        read_player_action(shm, my, &action, &target, &wslot, &ltwid);
        if (action == -1) break;

        pthread_mutex_lock(&shm->master_mutex);
        shm->players[my].action_type           = action;
        shm->players[my].action_target_id      = target;
        shm->players[my].action_weapon_slot     = wslot;
        shm->players[my].action_lt_weapon_id   = ltwid;
        shm->players[my].action_ready          = 1;
        pthread_mutex_unlock(&shm->master_mutex);

        sem_post(&shm->turn.action_submitted);
        std::cout << "[HIP] P" << my << " submitted action=" << action << "\n";
    }
    std::cout << "[HIP] Player thread " << my << " exiting\n";
    return nullptr;
}

int main(int, char*[]) {
    std::cout << "============================================\n"
              << "  CHRONO RIFT — HIP | Phase 5+10+11\n"
              << "  Roll: 24i-0847\n"
              << "============================================\n";

    SharedGameState* shm = attach_shared_memory_with_retry();
    g_shm = shm;
    setup_hip_signals();

    shm->hip_pid = getpid();
    std::cout << "[HIP] PID: " << shm->hip_pid << "\n";

    while (!shm->game_started && !g_should_exit) usleep(50000);
    if (g_should_exit) { detach_shared_memory(shm); return 0; }
    std::cout << "[HIP] game_started!\n";

    pthread_mutex_lock(&shm->master_mutex);
    int np = shm->num_players;
    for (int i = 0; i < np; i++) {
        shm->players[i].managing_pid = getpid();
        shm->players[i].thread_slot  = i;
    }
    pthread_mutex_unlock(&shm->master_mutex);

    pthread_t        threads[MAX_PLAYERS];
    PlayerThreadArgs args[MAX_PLAYERS];
    for (int i = 0; i < np; i++) {
        args[i] = {shm, i};
        pthread_create(&threads[i], nullptr, player_thread_func, &args[i]);
        std::cout << "[HIP] Spawned player thread " << i << "\n";
    }

    while (shm->game_result == GAME_ONGOING && !g_should_exit) usleep(200000);
    g_should_exit = 1;
    for (int i = 0; i < np; i++) sem_post(&shm->turn.turn_notification);
    for (int i = 0; i < np; i++) { pthread_join(threads[i], nullptr); }

    std::cout << "[HIP] Done. Result=" << shm->game_result << "\n";
    detach_shared_memory(shm);
    return 0;
}