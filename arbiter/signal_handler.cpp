// ============================================================================
// arbiter/signal_handler.cpp — Signal Infrastructure (Phases 10 + 11)
// CS 2006 Operating Systems | Spring 2026 | Chrono Rift
// Roll Number: 24i-0847  |  Partner: 24i-0650
//
// PHASE 10 — SIGUSR1 Stun Mechanic
//   Arbiter sends SIGUSR1 to HIP (for player stuns) or ASP (for NPC stuns).
//   The target process's handler sleeps exactly 3 seconds.
//   A detached recovery thread in Arbiter clears the is_stunned flag after 3s.
//   Stamina is PRESERVED through stun (tick thread skips stunned entities).
//
// PHASE 11 — Ultimate Ability (SIGSTOP / SIGCONT / SIGALRM)
//   When a player holds BOTH Solar Core AND Lunar Blade in primary inventory:
//     1. Arbiter sends SIGSTOP to ASP (kernel suspends the entire process).
//     2. Arbiter calls alarm(10) — SIGALRM fires after exactly 10 seconds.
//     3. SIGALRM handler sends SIGCONT to ASP, clears ultimate_active flag.
//   No flags or pipes coordinate this — signals only, as required by spec.
//
// STUN TRIGGER RULES (defined here — justify in report):
//   Solar Core  (95 dmg): 50% stun chance
//   Lunar Blade (90 dmg): 50% stun chance
//   Iron Halberd(55 dmg): 25% stun chance
//   All others          :  0% stun chance
// ============================================================================

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <cerrno>
#include "shared_types.h"

// ── Globals needed by signal handlers (signal handlers can't have params) ──
static SharedGameState*      g_shm            = nullptr;
static volatile sig_atomic_t g_quit_requested = 0;
static pid_t                 g_asp_pid        = 0;

// ── Forward declarations ──────────────────────────────────────────────────
void append_action_log(SharedGameState* shm, const char* message);
void check_game_conditions(SharedGameState* shm);

// ============================================================================
// SIGNAL HANDLERS (Arbiter-side)
// ============================================================================

// SIGTERM — sent by HIP when player chooses Quit
// Sets a flag; the main game loop detects it and sets GAME_QUIT
static void arbiter_sigterm_handler(int /*sig*/) {
    const char msg[] = "[ARBITER] SIGTERM received — graceful shutdown initiated\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    g_quit_requested = 1;
}

// SIGALRM — fires 10 seconds after alarm(10) was called during Ultimate Ability
// Resumes ASP and clears the ultimate_active flag
static void arbiter_sigalrm_handler(int /*sig*/) {
    const char msg[] = "[ARBITER] SIGALRM: Ultimate window expired — resuming ASP\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);

    if (g_asp_pid > 0) {
        kill(g_asp_pid, SIGCONT);
    }

    if (g_shm) {
        g_shm->ultimate_active = 0;
        // Post the turn_notification so NPC threads can resume checking turns
        sem_post(&g_shm->turn.turn_notification);
    }
}

// ============================================================================
// setup_arbiter_signals()
// Called once in arbiter main() after shared memory is created.
// Installs all Arbiter-side signal handlers.
// ============================================================================
void setup_arbiter_signals(SharedGameState* shm) {
    g_shm     = shm;
    g_asp_pid = 0;   // will be set when ASP registers its PID

    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;   // NO SA_RESTART — signals must interrupt blocking calls

    sa.sa_handler = arbiter_sigterm_handler;
    sigaction(SIGTERM, &sa, nullptr);

    sa.sa_handler = arbiter_sigalrm_handler;
    sigaction(SIGALRM, &sa, nullptr);

    // SIGPIPE — ignore (defensive: no pipes used, but just in case)
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, nullptr);

    std::cout << "[ARBITER] Signal handlers installed (SIGTERM, SIGALRM)" << std::endl;
}

// ============================================================================
// arbiter_check_quit()
// Called from main_game_loop() every iteration.
// Returns 1 if quit was requested via SIGTERM.
// ============================================================================
int arbiter_check_quit() {
    return g_quit_requested;
}

// ============================================================================
// Phase 10 — STUN MECHANIC
// ============================================================================

// Args for the stun-recovery background thread
typedef struct {
    SharedGameState* shm;
    int              is_player;   // 1 = player entity, 0 = NPC
    int              slot;        // index in shm->players[] or shm->npcs[]
    int              entity_id;   // for log message
} StunRecoveryArgs;

// Stun recovery thread — runs detached, clears is_stunned after 3 seconds
static void* stun_recovery_thread(void* arg) {
    StunRecoveryArgs* a = (StunRecoveryArgs*)arg;

    sleep(3);   // exactly 3 seconds, as per spec

    pthread_mutex_lock(&a->shm->master_mutex);
    if (a->is_player) {
        // Only clear if still the same entity (no respawn concern for players)
        a->shm->players[a->slot].is_stunned = STATUS_NOT_STUNNED;
    } else {
        // For NPCs: only clear if the slot still holds the same entity
        if (a->shm->npcs[a->slot].id == a->entity_id) {
            a->shm->npcs[a->slot].is_stunned = STATUS_NOT_STUNNED;
        }
    }
    pthread_mutex_unlock(&a->shm->master_mutex);

    char log_msg[ACTION_LOG_WIDTH];
    snprintf(log_msg, ACTION_LOG_WIDTH,
             "%s %d stun ended — resuming",
             a->is_player ? "Player" : "NPC", a->entity_id);
    append_action_log(a->shm, log_msg);
    std::cout << "[ARBITER] " << log_msg << std::endl;

    free(a);
    return nullptr;
}

// schedule_stun_recovery() — spawns a detached recovery thread
static void schedule_stun_recovery(SharedGameState* shm,
                                   int is_player, int slot, int entity_id) {
    StunRecoveryArgs* args = (StunRecoveryArgs*)malloc(sizeof(StunRecoveryArgs));
    args->shm       = shm;
    args->is_player = is_player;
    args->slot      = slot;
    args->entity_id = entity_id;

    pthread_t t;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&t, &attr, stun_recovery_thread, args);
    pthread_attr_destroy(&attr);
}

// ============================================================================
// stun_chance()
// Returns 1 if a weapon attack should trigger a stun, 0 otherwise.
// Probabilities defined here — document in report.
// ============================================================================
static int stun_chance(int weapon_id) {
    switch (weapon_id) {
        case WEAPON_SOLAR_CORE:    return (rand() % 2 == 0) ? 1 : 0;   // 50%
        case WEAPON_LUNAR_BLADE:   return (rand() % 2 == 0) ? 1 : 0;   // 50%
        case WEAPON_IRON_HALBERD:  return (rand() % 4 == 0) ? 1 : 0;   // 25%
        default:                   return 0;
    }
}

// ============================================================================
// try_apply_stun()
// Called by the Arbiter after a weapon attack lands.
// weapon_id   = weapon used
// target_is_player = 1 if target is a player, 0 if NPC
// target_slot = slot in shm->players[] or shm->npcs[]
//
// If stun triggers:
//   - Sets is_stunned flag in shared memory
//   - Sends SIGUSR1 to the target's process (HIP or ASP)
//   - Spawns a recovery thread to clear the flag after 3 seconds
// ============================================================================
void try_apply_stun(SharedGameState* shm,
                    int weapon_id,
                    int target_is_player, int target_slot) {
    if (!stun_chance(weapon_id)) return;   // no stun this time

    // Set stunned flag
    pthread_mutex_lock(&shm->master_mutex);
    int entity_id;
    if (target_is_player) {
        shm->players[target_slot].is_stunned = STATUS_STUNNED;
        entity_id = shm->players[target_slot].id;
    } else {
        shm->npcs[target_slot].is_stunned = STATUS_STUNNED;
        entity_id = shm->npcs[target_slot].id;
    }
    pthread_mutex_unlock(&shm->master_mutex);

    // Send SIGUSR1 to the target process
    pid_t target_pid = target_is_player ? shm->hip_pid : shm->asp_pid;
    if (target_pid > 0) {
        kill(target_pid, SIGUSR1);
    }

    // Log and schedule recovery
    char log_msg[ACTION_LOG_WIDTH];
    snprintf(log_msg, ACTION_LOG_WIDTH,
             "%s %d STUNNED for 3 seconds! (weapon: %s)",
             target_is_player ? "Player" : "NPC",
             entity_id, weapon_name[weapon_id]);
    append_action_log(shm, log_msg);
    std::cout << "[ARBITER] " << log_msg << std::endl;

    schedule_stun_recovery(shm, target_is_player, target_slot, entity_id);
}

// ============================================================================
// Phase 11 — ULTIMATE ABILITY
// ============================================================================

// ============================================================================
// can_use_ultimate()
// Returns 1 if the player at player_slot holds BOTH Solar Core AND Lunar Blade
// in their active primary inventory (not long-term storage).
// ============================================================================
int can_use_ultimate(SharedGameState* shm, int player_slot) {
    pthread_mutex_lock(&shm->master_mutex);
    int has_solar = shm->players[player_slot].holds_solar_core;
    int has_lunar = shm->players[player_slot].holds_lunar_blade;
    pthread_mutex_unlock(&shm->master_mutex);
    return (has_solar && has_lunar) ? 1 : 0;
}

// ============================================================================
// trigger_ultimate_ability()
// Called from apply_player_action() when player selects the Ultimate action
// and can_use_ultimate() returns 1.
//
// Spec requirement (hard): suspension/resumption via signals ONLY.
//   - SIGSTOP → ASP (kernel suspends immediately)
//   - alarm(10) → SIGALRM fires in Arbiter after 10s
//   - SIGALRM handler → SIGCONT to ASP
// ============================================================================
void trigger_ultimate_ability(SharedGameState* shm) {
    pthread_mutex_lock(&shm->master_mutex);
    if (shm->ultimate_active) {
        pthread_mutex_unlock(&shm->master_mutex);
        append_action_log(shm, "Ultimate already active — cannot trigger again");
        return;
    }
    shm->ultimate_active = 1;
    g_asp_pid = shm->asp_pid;   // store for SIGALRM handler
    pthread_mutex_unlock(&shm->master_mutex);

    append_action_log(shm, "*** ULTIMATE ABILITY ACTIVATED! ASP suspended for 10 seconds ***");
    std::cout << "\n[ARBITER] *** ULTIMATE ABILITY! Suspending ASP ("
              << g_asp_pid << ") for 10 seconds ***\n" << std::endl;

    // Suspend the entire ASP process — all NPC threads stop immediately
    kill(g_asp_pid, SIGSTOP);

    // Set a 10-second alarm — handler sends SIGCONT when it fires
    alarm(10);
}

// ============================================================================
// Phase 17 hook — expose quit flag to game loop
// ============================================================================
// arbiter_check_quit() is declared above and implemented inline.
// The main_game_loop() in game_loop.cpp calls this each iteration.