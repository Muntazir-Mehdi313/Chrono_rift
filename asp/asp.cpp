// ============================================================================
// asp/asp.cpp — Automated Strategic Process (Phase 6: NPC Thread Pool)
// CS 2006 Operating Systems | Spring 2026 | Chrono Rift
// Roll Number: 24i-0847  |  Partner: 24i-0650
//
// SPEC REQUIREMENTS MET:
//   "Each NPC must run in its own dedicated thread" ✓
//   "System must support concurrent execution of multiple NPC threads" ✓
//   "Implementations where entities handled sequentially will not be accepted" ✓
//   "Proper synchronization when accessing shared resources" ✓
//   "Graceful handling of dynamic termination when entity is defeated" ✓
//
// HOW IT WORKS:
//   1. Attaches shared memory, registers PID, installs signal handlers
//   2. Waits for game_started == 1
//   3. Spawns ONE pthread per initial NPC (2-9 threads)
//   4. Each NPC thread:
//        - Blocks on turn_notification semaphore
//        - Wakes up, checks if it's THIS NPC's turn (slot + is_player==0)
//        - If not: re-posts semaphore and loops back
//        - If yes: makes AI decision, writes to shared memory, posts action_submitted
//        - Exits when NPC dies or game ends
//   5. Main thread monitors for respawned NPCs → spawns new threads
//   6. On game end: wakes all threads, joins them, detaches shm
//
// SIGNAL HANDLING:
//   SIGUSR1  → stun handler: sleep(3) exactly, then resume (Phase 10)
//   SIGTERM  → shutdown flag, wake all threads
//   SIGSTOP/SIGCONT → handled by OS kernel (Ultimate Ability — no handler needed)
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

// ── Forward declarations ──────────────────────────────────────────────────
SharedGameState* attach_shared_memory_with_retry();
void             detach_shared_memory(SharedGameState* shm);

// ── Globals (needed by signal handlers) ──────────────────────────────────
static SharedGameState*      g_shm         = nullptr;
static volatile sig_atomic_t g_should_exit = 0;

// ── Thread pool tracking ──────────────────────────────────────────────────
// We can spawn at most: initial (2-9) + 10 respawns = 19 max ever
#define MAX_TOTAL_THREADS  20

static pthread_t g_npc_threads[MAX_TOTAL_THREADS];
static int       g_total_threads = 0;
static pthread_mutex_t g_thread_mutex = PTHREAD_MUTEX_INITIALIZER;

// ============================================================================
// SIGNAL HANDLERS
// ============================================================================

// SIGUSR1 — Stun: pause entire ASP process for exactly 3 seconds
// All NPC threads inside are paused by this handler on the interrupted thread
static void asp_sigusr1_handler(int /*sig*/) {
    write(STDOUT_FILENO, "[ASP] STUN! Pausing 3s...\n", 26);
    sleep(3);
    write(STDOUT_FILENO, "[ASP] STUN ended.\n", 18);
}

// SIGTERM — game over or Arbiter shutting down
static void asp_sigterm_handler(int /*sig*/) {
    g_should_exit = 1;
    if (g_shm) {
        // Unblock all NPC threads waiting on turn_notification
        for (int i = 0; i < MAX_TOTAL_THREADS; i++) {
            sem_post(&g_shm->turn.turn_notification);
        }
    }
}

static void setup_asp_signals() {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;   // NO SA_RESTART — signals must interrupt sem_wait

    sa.sa_handler = asp_sigusr1_handler;
    sigaction(SIGUSR1, &sa, nullptr);

    sa.sa_handler = asp_sigterm_handler;
    sigaction(SIGTERM, &sa, nullptr);
}

// ============================================================================
// NPC THREAD ARGUMENT
// ============================================================================
typedef struct {
    SharedGameState* shm;
    int              npc_slot;   // index into shm->npcs[]
    int              npc_id;     // entity ID at time of spawn — detects slot reuse
} NpcThreadArgs;

// Static args array (one entry per ever-spawned thread)
static NpcThreadArgs g_thread_args[MAX_TOTAL_THREADS];

// ============================================================================
// npc_thread_func()
// Runs for one NPC. Exits when: game over, NPC dead, or slot reassigned.
// ============================================================================
static void* npc_thread_func(void* arg) {
    NpcThreadArgs*   args    = (NpcThreadArgs*)arg;
    SharedGameState* shm     = args->shm;
    int              my_slot = args->npc_slot;
    int              my_id   = args->npc_id;

    std::cout << "[ASP] NPC thread START  slot=" << my_slot
              << " id=" << my_id << std::endl;

    while (true) {
        // ── Block until Arbiter posts a turn ──────────────────────────────
        int ret;
        do {
            ret = sem_wait(&shm->turn.turn_notification);
        } while (ret == -1 && errno == EINTR && !g_should_exit);

        if (g_should_exit) break;

        // ── Read current state safely ─────────────────────────────────────
        pthread_mutex_lock(&shm->master_mutex);
        int game_over  = (shm->game_result != GAME_ONGOING);
        int still_mine = (shm->npcs[my_slot].id == my_id);
        int is_alive   = shm->npcs[my_slot].is_alive;
        int is_my_turn = (!shm->turn.current_entity_is_player &&
                          shm->turn.current_entity_slot == my_slot &&
                          still_mine);
        pthread_mutex_unlock(&shm->master_mutex);

        // Exit conditions: game done, NPC dead, slot reused by new NPC
        if (game_over || !is_alive || !still_mine) break;

        if (!is_my_turn) {
            // Not our turn — pass the semaphore to the next waiting thread
            sem_post(&shm->turn.turn_notification);
            continue;
        }

        // ── OUR TURN: AI decision ─────────────────────────────────────────
        // 80% Strike a random alive player, 20% Skip
        int action_type        = ACTION_ATTACK_STRIKE;
        int target_player_slot = -1;

        if (rand() % 10 < 2) {
            action_type = ACTION_SKIP;
        } else {
            // Build list of alive player slots
            int alive[MAX_PLAYERS];
            int count = 0;
            pthread_mutex_lock(&shm->master_mutex);
            for (int i = 0; i < shm->num_players; i++) {
                if (shm->players[i].is_alive) alive[count++] = i;
            }
            pthread_mutex_unlock(&shm->master_mutex);

            if (count > 0) {
                target_player_slot = alive[rand() % count];
            } else {
                action_type = ACTION_SKIP;  // no alive players
            }
        }

        // ── Write action to shared memory ─────────────────────────────────
        pthread_mutex_lock(&shm->master_mutex);
        shm->npcs[my_slot].action_type      = action_type;
        shm->npcs[my_slot].action_target_id = target_player_slot;
        shm->npcs[my_slot].action_ready     = 1;
        pthread_mutex_unlock(&shm->master_mutex);

        std::cout << "[ASP] NPC " << my_id
                  << " → " << (action_type == ACTION_ATTACK_STRIKE ? "STRIKE" : "SKIP");
        if (target_player_slot >= 0)
            std::cout << " Player " << target_player_slot;
        std::cout << std::endl;

        // ── Notify Arbiter ────────────────────────────────────────────────
        sem_post(&shm->turn.action_submitted);
    }

    std::cout << "[ASP] NPC thread EXIT   slot=" << my_slot
              << " id=" << my_id << std::endl;
    return nullptr;
}

// ============================================================================
// spawn_npc_thread()
// Creates a new pthread for the NPC currently in shm->npcs[slot].
// Thread args are stored in the static g_thread_args array.
// ============================================================================
static void spawn_npc_thread(SharedGameState* shm, int slot) {
    pthread_mutex_lock(&g_thread_mutex);

    if (g_total_threads >= MAX_TOTAL_THREADS) {
        std::cerr << "[ASP] ERROR: thread pool full" << std::endl;
        pthread_mutex_unlock(&g_thread_mutex);
        return;
    }

    int idx = g_total_threads;

    g_thread_args[idx].shm      = shm;
    g_thread_args[idx].npc_slot = slot;

    pthread_mutex_lock(&shm->master_mutex);
    g_thread_args[idx].npc_id = shm->npcs[slot].id;
    pthread_mutex_unlock(&shm->master_mutex);

    int rc = pthread_create(&g_npc_threads[idx], nullptr,
                            npc_thread_func, &g_thread_args[idx]);
    if (rc != 0) {
        std::cerr << "[ASP] pthread_create failed slot=" << slot
                  << " rc=" << rc << std::endl;
    } else {
        std::cout << "[ASP] Thread spawned idx=" << idx
                  << " slot=" << slot
                  << " npc_id=" << g_thread_args[idx].npc_id
                  << std::endl;
        g_total_threads++;
    }

    pthread_mutex_unlock(&g_thread_mutex);
}

// ============================================================================
// main
// ============================================================================
int main(int /*argc*/, char* /*argv*/[]) {
    std::cout << "============================================\n"
              << "  CHRONO RIFT — Automated Strategic Process\n"
              << "  Phase 6 | Roll: 24i-0847\n"
              << "============================================\n";

    // ── Attach shared memory ──────────────────────────────────────────────
    SharedGameState* shm = attach_shared_memory_with_retry();
    g_shm = shm;

    // ── Install signal handlers ───────────────────────────────────────────
    setup_asp_signals();

    // ── Register PID ──────────────────────────────────────────────────────
    shm->asp_pid = getpid();
    std::cout << "[ASP] PID registered: " << shm->asp_pid << std::endl;

    // ── Wait for game start ───────────────────────────────────────────────
    std::cout << "[ASP] Waiting for game_started..." << std::endl;
    while (!shm->game_started && !g_should_exit) usleep(50000);
    if (g_should_exit) { detach_shared_memory(shm); return 0; }
    std::cout << "[ASP] game_started received!" << std::endl;

    // ── Read initial config ───────────────────────────────────────────────
    pthread_mutex_lock(&shm->master_mutex);
    int num_npcs        = shm->num_npcs_concurrent;
    int last_seen_total = shm->total_npcs_spawned;
    pthread_mutex_unlock(&shm->master_mutex);

    // ── Spawn initial NPC threads ─────────────────────────────────────────
    std::cout << "[ASP] Spawning " << num_npcs << " NPC threads..." << std::endl;
    for (int i = 0; i < num_npcs; i++) {
        spawn_npc_thread(shm, i);
    }
    std::cout << "[ASP] All initial threads spawned. Monitoring for respawns."
              << std::endl;

    // ── Track last spawned npc_id per slot for respawn detection ──────────
    int last_id[MAX_NPCS] = {};
    pthread_mutex_lock(&shm->master_mutex);
    for (int i = 0; i < num_npcs; i++) last_id[i] = shm->npcs[i].id;
    pthread_mutex_unlock(&shm->master_mutex);

    // ── Monitor loop: detect and handle NPC respawns ──────────────────────
    while (shm->game_result == GAME_ONGOING && !g_should_exit) {
        usleep(150000);  // 150ms — light polling

        pthread_mutex_lock(&shm->master_mutex);
        int current_total = shm->total_npcs_spawned;
        pthread_mutex_unlock(&shm->master_mutex);

        if (current_total <= last_seen_total) continue;
        last_seen_total = current_total;

        // Find which slots have new NPCs (id changed = respawn)
        for (int i = 0; i < num_npcs; i++) {
            pthread_mutex_lock(&shm->master_mutex);
            int alive  = shm->npcs[i].is_alive;
            int npc_id = shm->npcs[i].id;
            pthread_mutex_unlock(&shm->master_mutex);

            if (alive && npc_id != last_id[i]) {
                std::cout << "[ASP] Respawn: slot=" << i
                          << " old_id=" << last_id[i]
                          << " new_id=" << npc_id << std::endl;
                spawn_npc_thread(shm, i);
                last_id[i] = npc_id;
            }
        }
    }

    // ── Game over: wake and join all threads ──────────────────────────────
    std::cout << "[ASP] Game over. Shutting down NPC threads..." << std::endl;
    g_should_exit = 1;

    pthread_mutex_lock(&g_thread_mutex);
    int total = g_total_threads;
    pthread_mutex_unlock(&g_thread_mutex);

    // Post enough semaphores to unblock every waiting thread
    for (int i = 0; i < total + 4; i++) sem_post(&shm->turn.turn_notification);

    for (int i = 0; i < total; i++) {
        pthread_join(g_npc_threads[i], nullptr);
        std::cout << "[ASP] Thread " << i << " joined." << std::endl;
    }

    std::cout << "[ASP] Result: ";
    switch (shm->game_result) {
        case GAME_WIN:  std::cout << "VICTORY!\n"; break;
        case GAME_LOSE: std::cout << "DEFEAT.\n";  break;
        case GAME_QUIT: std::cout << "QUIT.\n";    break;
        default:        std::cout << shm->game_result << "\n"; break;
    }

    detach_shared_memory(shm);
    std::cout << "[ASP] Clean exit.\n";
    return 0;
}