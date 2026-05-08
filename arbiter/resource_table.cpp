// ============================================================================
// arbiter/resource_table.cpp — Phase 14: Global Resource Table & Artifact Locking
// CS 2006 Operating Systems | Spring 2026 | Chrono Rift
// Roll Number: 24i-0847  |  Partner: 24i-0650
//
// WHAT THIS FILE IMPLEMENTS (Phase 14):
//   - acquire_artifact()            : atomically claim Solar Core, Lunar Blade, Eclipse Relic
//   - release_artifact()            : free a held artifact
//   - release_all_artifacts_for_entity(): called on entity death
//   - set_waiting_for()             : mark entity blocked waiting for an artifact
//   - clear_waiting_for()           : clear wait marker after resolution
//   - try_acquire_artifact_for_pickup(): called on weapon pickup
//
// NOTE: introduce_eclipse_relic() and check_eclipse_relic_spawn() live in
//   inventory_allocator.cpp (Section 5) to avoid duplicate definitions.
//
// LOCKING DISCIPLINE:
//   GlobalResourceTable has its OWN mutex (table_mutex, PTHREAD_PROCESS_SHARED).
//   NEVER hold master_mutex while locking table_mutex — always lock table_mutex
//   FIRST then master_mutex if both are needed, to prevent lock-ordering deadlocks.
//   In practice these two functions never need both simultaneously.
//
// ECLIPSE RELIC CONSTANT:
//   The Eclipse Relic is weapon ID 8 (one beyond the normal 0-7 range).
//   It is NOT a standard weapon — it cannot be equipped or used for damage.
//   It exists ONLY as a deadlock-triggering artifact for demonstration purposes.
//   We define its ID here so it doesn't pollute shared_types.h with gameplay code.
// ============================================================================

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include "shared_types.h"

// Eclipse Relic is artifact ID 8 — beyond normal weapon range 0-7
#define ECLIPSE_RELIC_ID  8

// Forward declaration (defined in game_loop.cpp)
void append_action_log(SharedGameState* shm, const char* message);

// ============================================================================
// acquire_artifact()
// Called by Arbiter when a player or NPC picks up an artifact weapon.
// Locks table_mutex, checks if free, claims it, unlocks.
//
// Returns:
//   1  = acquired successfully
//   0  = already held by someone else (caller should set waiting_for_resource)
//  -1  = invalid artifact ID
// ============================================================================
int acquire_artifact(SharedGameState* shm, int entity_id, int artifact_id) {
    GlobalResourceTable* rt = &shm->resources;

    // Only artifacts are tracked in the resource table
    if (artifact_id != WEAPON_SOLAR_CORE  &&
        artifact_id != WEAPON_LUNAR_BLADE &&
        artifact_id != ECLIPSE_RELIC_ID) {
        return -1;  // not an artifact
    }

    pthread_mutex_lock(&rt->table_mutex);

    int* holder = nullptr;
    if      (artifact_id == WEAPON_SOLAR_CORE)  holder = &rt->solar_core_holder;
    else if (artifact_id == WEAPON_LUNAR_BLADE) holder = &rt->lunar_blade_holder;
    else if (artifact_id == ECLIPSE_RELIC_ID) {
        if (!rt->eclipse_relic_in_game) {
            pthread_mutex_unlock(&rt->table_mutex);
            return -1;  // relic not active yet
        }
        holder = &rt->eclipse_relic_holder;
    }

    if (*holder == -1) {
        // Free — claim it
        *holder = entity_id;
        pthread_mutex_unlock(&rt->table_mutex);

        char log_msg[ACTION_LOG_WIDTH];
        snprintf(log_msg, ACTION_LOG_WIDTH,
                 "Entity %d acquired %s",
                 entity_id,
                 artifact_id == WEAPON_SOLAR_CORE  ? "Solar Core"   :
                 artifact_id == WEAPON_LUNAR_BLADE ? "Lunar Blade"  : "Eclipse Relic");
        append_action_log(shm, log_msg);
        std::cout << "[ARBITER] " << log_msg << std::endl;
        return 1;
    } else {
        // Held by someone else — return 0, caller marks entity as waiting
        pthread_mutex_unlock(&rt->table_mutex);
        return 0;
    }
}

// ============================================================================
// release_artifact()
// Called when an entity dies or voluntarily drops an artifact.
// Also called by deadlock resolution (Phase 15) to force a release.
// ============================================================================
void release_artifact(SharedGameState* shm, int entity_id, int artifact_id) {
    GlobalResourceTable* rt = &shm->resources;

    pthread_mutex_lock(&rt->table_mutex);

    char log_msg[ACTION_LOG_WIDTH];
    bool released = false;

    if (artifact_id == WEAPON_SOLAR_CORE && rt->solar_core_holder == entity_id) {
        rt->solar_core_holder = -1;
        released = true;
        snprintf(log_msg, ACTION_LOG_WIDTH,
                 "Entity %d released Solar Core", entity_id);
    } else if (artifact_id == WEAPON_LUNAR_BLADE && rt->lunar_blade_holder == entity_id) {
        rt->lunar_blade_holder = -1;
        released = true;
        snprintf(log_msg, ACTION_LOG_WIDTH,
                 "Entity %d released Lunar Blade", entity_id);
    } else if (artifact_id == ECLIPSE_RELIC_ID && rt->eclipse_relic_holder == entity_id) {
        rt->eclipse_relic_holder = -1;
        released = true;
        snprintf(log_msg, ACTION_LOG_WIDTH,
                 "Entity %d released Eclipse Relic", entity_id);
    }

    pthread_mutex_unlock(&rt->table_mutex);

    if (released) {
        append_action_log(shm, log_msg);
        std::cout << "[ARBITER] " << log_msg << std::endl;
    }
}

// ============================================================================
// release_all_artifacts_for_entity()
// Called when any entity dies — releases everything it holds at once.
// Checks all three artifacts and releases any the entity holds.
// ============================================================================
void release_all_artifacts_for_entity(SharedGameState* shm, int entity_id) {
    GlobalResourceTable* rt = &shm->resources;

    pthread_mutex_lock(&rt->table_mutex);

    char log_msg[ACTION_LOG_WIDTH];
    if (rt->solar_core_holder   == entity_id) {
        rt->solar_core_holder = -1;
        snprintf(log_msg, ACTION_LOG_WIDTH, "Entity %d died — Solar Core released", entity_id);
        pthread_mutex_unlock(&rt->table_mutex);
        append_action_log(shm, log_msg);
        pthread_mutex_lock(&rt->table_mutex);
    }
    if (rt->lunar_blade_holder  == entity_id) {
        rt->lunar_blade_holder = -1;
        snprintf(log_msg, ACTION_LOG_WIDTH, "Entity %d died — Lunar Blade released", entity_id);
        pthread_mutex_unlock(&rt->table_mutex);
        append_action_log(shm, log_msg);
        pthread_mutex_lock(&rt->table_mutex);
    }
    if (rt->eclipse_relic_holder == entity_id) {
        rt->eclipse_relic_holder = -1;
        snprintf(log_msg, ACTION_LOG_WIDTH, "Entity %d died — Eclipse Relic released", entity_id);
        pthread_mutex_unlock(&rt->table_mutex);
        append_action_log(shm, log_msg);
        return;  // already unlocked
    }

    pthread_mutex_unlock(&rt->table_mutex);
}

// ============================================================================
// set_waiting_for()
// Mark an entity as blocked waiting for an artifact.
// Called when acquire_artifact() returns 0.
// The deadlock monitor (Phase 15) reads this to build the wait-for graph.
// ============================================================================
void set_waiting_for(SharedGameState* shm, int entity_id, int artifact_id) {
    pthread_mutex_lock(&shm->master_mutex);

    for (int i = 0; i < shm->num_players; i++) {
        if (shm->players[i].id == entity_id) {
            shm->players[i].waiting_for_resource = artifact_id;
            pthread_mutex_unlock(&shm->master_mutex);
            return;
        }
    }
    for (int i = 0; i < shm->num_npcs_concurrent; i++) {
        if (shm->npcs[i].id == entity_id) {
            shm->npcs[i].waiting_for_resource = artifact_id;
            pthread_mutex_unlock(&shm->master_mutex);
            return;
        }
    }

    pthread_mutex_unlock(&shm->master_mutex);
}

// ============================================================================
// clear_waiting_for()
// Called after an entity successfully acquires what it was waiting for,
// or after deadlock resolution forcibly resolves its wait.
// ============================================================================
void clear_waiting_for(SharedGameState* shm, int entity_id) {
    pthread_mutex_lock(&shm->master_mutex);

    for (int i = 0; i < shm->num_players; i++) {
        if (shm->players[i].id == entity_id) {
            shm->players[i].waiting_for_resource = WEAPON_NONE;
            pthread_mutex_unlock(&shm->master_mutex);
            return;
        }
    }
    for (int i = 0; i < shm->num_npcs_concurrent; i++) {
        if (shm->npcs[i].id == entity_id) {
            shm->npcs[i].waiting_for_resource = WEAPON_NONE;
            pthread_mutex_unlock(&shm->master_mutex);
            return;
        }
    }

    pthread_mutex_unlock(&shm->master_mutex);
}

// ============================================================================
// NOTE: introduce_eclipse_relic() and check_eclipse_relic_spawn() are
// defined in inventory_allocator.cpp (Section 5) — do NOT duplicate here.
// ============================================================================
// try_acquire_artifact_for_pickup()
// Called by player_pickup_weapon() and npc_pickup_weapon() in inventory_allocator.cpp
// when a player or NPC picks up an artifact weapon from the battlefield.
//
// If the artifact is free: acquires it, returns 1.
// If held by another: sets waiting_for_resource, returns 0.
//   The deadlock monitor will detect the resulting circular wait if any.
// ============================================================================
int try_acquire_artifact_for_pickup(SharedGameState* shm, int entity_id, int weapon_id) {
    // Only handle artifact weapons
    if (weapon_id != WEAPON_SOLAR_CORE  &&
        weapon_id != WEAPON_LUNAR_BLADE &&
        weapon_id != ECLIPSE_RELIC_ID) {
        return 1;  // not an artifact — always succeeds
    }

    int result = acquire_artifact(shm, entity_id, weapon_id);

    if (result == 0) {
        // Artifact is held — mark entity as waiting (feeds deadlock detection)
        set_waiting_for(shm, entity_id, weapon_id);

        char log_msg[ACTION_LOG_WIDTH];
        snprintf(log_msg, ACTION_LOG_WIDTH,
                 "Entity %d waiting for %s (held by another entity)",
                 entity_id,
                 weapon_id == WEAPON_SOLAR_CORE  ? "Solar Core"  :
                 weapon_id == WEAPON_LUNAR_BLADE ? "Lunar Blade" : "Eclipse Relic");
        append_action_log(shm, log_msg);
        std::cout << "[ARBITER] " << log_msg << std::endl;
    } else if (result == 1) {
        // Successfully acquired — clear any prior wait
        clear_waiting_for(shm, entity_id);

        // Update entity's holds_* flags for Ultimate Ability check
        pthread_mutex_lock(&shm->master_mutex);
        for (int i = 0; i < shm->num_players; i++) {
            if (shm->players[i].id == entity_id) {
                if (weapon_id == WEAPON_SOLAR_CORE)  shm->players[i].holds_solar_core  = 1;
                if (weapon_id == WEAPON_LUNAR_BLADE) shm->players[i].holds_lunar_blade = 1;
                break;
            }
        }
        pthread_mutex_unlock(&shm->master_mutex);
    }

    return (result == 1) ? 1 : 0;
}