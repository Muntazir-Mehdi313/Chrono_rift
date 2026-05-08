// ============================================================================
// arbiter/deadlock_monitor.cpp — Phase 15: Deadlock Detection & Resolution
// CS 2006 Operating Systems | Spring 2026 | Chrono Rift
// Roll Number: 24i-0847  |  Partner: 24i-0650
//
// WHAT THIS FILE IMPLEMENTS (Phase 15):
//   - Background pthread that runs for the entire game duration
//   - Builds a wait-for graph every second from entity waiting_for_resource fields
//   - Runs DFS cycle detection on the graph
//   - On cycle found: forces one entity to release its held artifact
//   - Logs all detection and resolution events to action log
//
// ALGORITHM: Wait-For Graph + DFS
//   Nodes   = entity IDs (players 0..num_players-1, NPCs by their .id field)
//   Edge    = entity A waits for resource held by entity B → edge A→B
//   Cycle   = deadlock (A waits for B, B waits for A = classic 2-entity deadlock)
//
// RESOLUTION POLICY (spec says "forced release"):
//   Victim selection: the node returned by DFS (first node found in cycle).
//   That entity's HELD artifact is released (not what it's waiting for).
//   This breaks the cycle — the waiting entity can now acquire.
//
// IMPORTANT — LOCKING ORDER:
//   This thread locks: table_mutex → master_mutex (in that order).
//   All other code that needs both must use the SAME order.
//   Reversing the order causes deadlock between the monitor thread itself
//   and the game loop — which is the exact thing we are trying to detect.
// ============================================================================

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <pthread.h>
#include "shared_types.h"

// Forward declarations
void append_action_log(SharedGameState* shm, const char* message);

// ============================================================================
// SECTION 1 — WAIT-FOR GRAPH DATA STRUCTURE
// ============================================================================

#define MAX_ENTITIES  (MAX_PLAYERS + MAX_NPCS)   // 4 + 9 = 13 max entities
#define MAX_EDGES     64                          // more than enough for 13 nodes

typedef struct {
    int from;   // entity ID that is WAITING
    int to;     // entity ID that HOLDS what 'from' wants
} WaitEdge;

// ============================================================================
// build_wait_for_graph()
// Reads waiting_for_resource for every alive entity.
// For each entity waiting for artifact X: finds who holds X, adds edge.
// Returns number of edges added (0 = no waits = no possible deadlock).
//
// Locking: locks table_mutex first, then master_mutex for entity scan.
// Both are released before returning.
// ============================================================================
static int build_wait_for_graph(SharedGameState* shm,
                                 WaitEdge* edges, int max_edges) {
    int edge_count = 0;
    GlobalResourceTable* rt = &shm->resources;

    // Lock order: table_mutex first, then master_mutex
    pthread_mutex_lock(&rt->table_mutex);
    pthread_mutex_lock(&shm->master_mutex);

    // Helper: for a given artifact_id, who is the current holder?
    auto get_holder = [&](int artifact_id) -> int {
        if (artifact_id == WEAPON_SOLAR_CORE)  return rt->solar_core_holder;
        if (artifact_id == WEAPON_LUNAR_BLADE) return rt->lunar_blade_holder;
        if (rt->eclipse_relic_in_game)         return rt->eclipse_relic_holder;
        return -1;
    };

    // Scan players
    for (int i = 0; i < shm->num_players; i++) {
        Entity* e = &shm->players[i];
        if (!e->is_alive) continue;
        if (e->waiting_for_resource == WEAPON_NONE) continue;

        int holder = get_holder(e->waiting_for_resource);
        if (holder != -1 && holder != e->id && edge_count < max_edges) {
            edges[edge_count].from = e->id;
            edges[edge_count].to   = holder;
            edge_count++;
        }
    }

    // Scan NPCs
    for (int i = 0; i < shm->num_npcs_concurrent; i++) {
        Entity* e = &shm->npcs[i];
        if (!e->is_alive) continue;
        if (e->waiting_for_resource == WEAPON_NONE) continue;

        int holder = get_holder(e->waiting_for_resource);
        if (holder != -1 && holder != e->id && edge_count < max_edges) {
            edges[edge_count].from = e->id;
            edges[edge_count].to   = holder;
            edge_count++;
        }
    }

    pthread_mutex_unlock(&shm->master_mutex);
    pthread_mutex_unlock(&rt->table_mutex);

    return edge_count;
}

// ============================================================================
// SECTION 2 — DFS CYCLE DETECTION
// ============================================================================

// detect_cycle_dfs()
// Standard iterative DFS using visited[] and in_stack[] arrays.
// Arrays are indexed by entity ID (IDs are small — max ~22 in practice).
// Returns the entity ID of a node in a cycle, or -1 if no cycle from 'node'.
// ============================================================================
static int detect_cycle_dfs(const WaitEdge* edges, int edge_count,
                             int* visited, int* in_stack, int node) {
    visited[node]  = 1;
    in_stack[node] = 1;

    for (int i = 0; i < edge_count; i++) {
        if (edges[i].from != node) continue;

        int next = edges[i].to;

        if (!visited[next]) {
            int result = detect_cycle_dfs(edges, edge_count,
                                          visited, in_stack, next);
            if (result != -1) return result;
        } else if (in_stack[next]) {
            // Back edge — cycle found
            return next;
        }
    }

    in_stack[node] = 0;
    return -1;
}

// run_cycle_detection()
// Collects all unique node IDs from edges, runs DFS from each unvisited node.
// Returns entity ID of a node in a cycle, or -1 if no deadlock.
static int run_cycle_detection(const WaitEdge* edges, int edge_count) {
    if (edge_count == 0) return -1;

    // Collect unique node IDs
    int nodes[MAX_ENTITIES * 2];
    int node_count = 0;

    for (int i = 0; i < edge_count; i++) {
        bool found_from = false, found_to = false;
        for (int j = 0; j < node_count; j++) {
            if (nodes[j] == edges[i].from) found_from = true;
            if (nodes[j] == edges[i].to)   found_to   = true;
        }
        if (!found_from && node_count < MAX_ENTITIES * 2) nodes[node_count++] = edges[i].from;
        if (!found_to   && node_count < MAX_ENTITIES * 2) nodes[node_count++] = edges[i].to;
    }

    // DFS — use arrays sized for max possible entity ID (IDs can go up to total_npcs_spawned)
    // Safe upper bound: 256 (well beyond any game session)
    int visited [256] = {};
    int in_stack[256] = {};

    for (int i = 0; i < node_count; i++) {
        int id = nodes[i];
        if (id >= 0 && id < 256 && !visited[id]) {
            int result = detect_cycle_dfs(edges, edge_count,
                                          visited, in_stack, id);
            if (result != -1) return result;
        }
    }

    return -1;
}

// ============================================================================
// SECTION 3 — DEADLOCK RESOLUTION
// ============================================================================

// resolve_deadlock()
// Given the entity ID of a node in the deadlock cycle (victim):
//   1. Find what artifact the victim currently HOLDS (not what it's waiting for)
//   2. Force-release that artifact from the resource table
//   3. Clear the victim's waiting_for_resource
//   4. Update the victim entity's holds_* flags
//   5. Log the event
//
// Why release what it HOLDS (not what it's waiting for)?
//   If A waits for X (held by B) and B waits for Y (held by A):
//   We pick A as victim, release Y (what A holds).
//   Now B can acquire Y → B's wait is resolved → B can proceed.
//   A is still waiting for X, but it's no longer in a cycle.
// ============================================================================
static void resolve_deadlock(SharedGameState* shm, int victim_id) {
    GlobalResourceTable* rt = &shm->resources;
    char log_msg[ACTION_LOG_WIDTH];

    pthread_mutex_lock(&rt->table_mutex);

    // Find what the victim currently holds
    int released_artifact = WEAPON_NONE;
    const char* artifact_name = "unknown";

    if (rt->solar_core_holder == victim_id) {
        rt->solar_core_holder = -1;
        released_artifact = WEAPON_SOLAR_CORE;
        artifact_name = "Solar Core";
    } else if (rt->lunar_blade_holder == victim_id) {
        rt->lunar_blade_holder = -1;
        released_artifact = WEAPON_LUNAR_BLADE;
        artifact_name = "Lunar Blade";
    } else if (rt->eclipse_relic_holder == victim_id) {
        rt->eclipse_relic_holder = -1;
        released_artifact = 8; // ECLIPSE_RELIC_ID
        artifact_name = "Eclipse Relic";
    }

    pthread_mutex_unlock(&rt->table_mutex);

    if (released_artifact == WEAPON_NONE) {
        // Victim holds nothing — clear its wait and return
        snprintf(log_msg, ACTION_LOG_WIDTH,
                 "[DEADLOCK RESOLVED] Entity %d wait cleared (held nothing)", victim_id);
        append_action_log(shm, log_msg);
        std::cout << "[ARBITER] " << log_msg << std::endl;
    } else {
        snprintf(log_msg, ACTION_LOG_WIDTH,
                 "[DEADLOCK RESOLVED] %s forcibly taken from Entity %d",
                 artifact_name, victim_id);
        append_action_log(shm, log_msg);
        std::cout << "\n[ARBITER] " << log_msg << std::endl;
    }

    // Clear victim's waiting_for and holds_* flags
    pthread_mutex_lock(&shm->master_mutex);
    for (int i = 0; i < shm->num_players; i++) {
        if (shm->players[i].id == victim_id) {
            shm->players[i].waiting_for_resource = WEAPON_NONE;
            if (released_artifact == WEAPON_SOLAR_CORE)  shm->players[i].holds_solar_core  = 0;
            if (released_artifact == WEAPON_LUNAR_BLADE) shm->players[i].holds_lunar_blade = 0;
            break;
        }
    }
    for (int i = 0; i < shm->num_npcs_concurrent; i++) {
        if (shm->npcs[i].id == victim_id) {
            shm->npcs[i].waiting_for_resource = WEAPON_NONE;
            break;
        }
    }
    pthread_mutex_unlock(&shm->master_mutex);
}

// ============================================================================
// SECTION 4 — BACKGROUND MONITOR THREAD
// ============================================================================

// deadlock_monitor_thread()
// Launched by Arbiter as a detached background pthread after game_started=1.
// Checks for deadlocks every 1 second.
// Exits when game_result != GAME_ONGOING.
// ============================================================================
void* deadlock_monitor_thread(void* arg) {
    SharedGameState* shm = (SharedGameState*)arg;

    std::cout << "[DEADLOCK] Monitor thread started." << std::endl;
    append_action_log(shm, "[DEADLOCK] Monitor active — checking every 1s");

    while (shm->game_result == GAME_ONGOING) {
        sleep(1);   // check once per second — lightweight

        if (shm->game_result != GAME_ONGOING) break;

        // Build the wait-for graph
        WaitEdge edges[MAX_EDGES];
        int edge_count = build_wait_for_graph(shm, edges, MAX_EDGES);

        if (edge_count == 0) continue;  // no waits — no possible deadlock

        // Log current wait state for debugging
        char log_msg[ACTION_LOG_WIDTH];
        snprintf(log_msg, ACTION_LOG_WIDTH,
                 "[DEADLOCK] Wait graph: %d edges detected", edge_count);
        std::cout << "[ARBITER] " << log_msg << std::endl;

        // Run DFS cycle detection
        int deadlock_node = run_cycle_detection(edges, edge_count);

        if (deadlock_node == -1) continue;  // no cycle — no deadlock

        // ── DEADLOCK DETECTED ──────────────────────────────────────────────
        snprintf(log_msg, ACTION_LOG_WIDTH,
                 "[DEADLOCK DETECTED] Circular wait involving Entity %d — resolving...",
                 deadlock_node);
        append_action_log(shm, log_msg);
        std::cout << "\n[ARBITER] !!! " << log_msg << " !!!\n" << std::endl;

        resolve_deadlock(shm, deadlock_node);
    }

    std::cout << "[DEADLOCK] Monitor thread exiting." << std::endl;
    return nullptr;
}

// ============================================================================
// launch_deadlock_monitor()
// Called from main_game_loop() (game_loop.cpp) after game_started = 1.
// Spawns the monitor as a joinable thread (Arbiter joins it on shutdown).
// ============================================================================
void launch_deadlock_monitor(SharedGameState* shm, pthread_t* out_thread) {
    int rc = pthread_create(out_thread, nullptr, deadlock_monitor_thread, shm);
    if (rc != 0) {
        std::cerr << "[ARBITER] ERROR: Could not create deadlock monitor thread (rc="
                  << rc << ")" << std::endl;
    } else {
        std::cout << "[ARBITER] Deadlock monitor thread launched." << std::endl;
    }
}