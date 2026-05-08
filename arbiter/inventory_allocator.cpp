// ============================================================================
// arbiter/inventory_allocator.cpp — Contiguous Inventory Memory Manager
// CS 2006 Operating Systems | Spring 2026 | Chrono Rift
// Roll Number: 24i-0847  |  Partner: 24i-0650
//
// PHASE 12 — Inventory Allocator (Contiguous Memory Management)
//   - Primary inventory: 20-slot linear array per entity
//   - Weapons occupy CONTIGUOUS slots (e.g. Iron Halberd = 7 consecutive slots)
//   - find_contiguous_free(): first-fit search for N free slots
//   - inventory_add_weapon(): places weapon, triggers eviction if needed
//   - Eviction: swaps out MINIMUM weapons needed to create space
//   - Fragmentation: handled by the sliding-window eviction strategy
//   - Solar Core (10) + Lunar Blade (10) = full 20 slots (hard constraint)
//
// PHASE 13 — Weapon Drops, Pickup & Swap In
//   - handle_weapon_drop(): called by Arbiter after NPC dies
//   - present_drop_to_players(): sets shm flags so HIP shows the prompt
//   - npc_pickup_weapon(): guarantees an NPC picks it up if player declines
//   - execute_swap_in(): retrieves weapon from lt_storage into inventory
//
// MARKS COVERED:
//   Memory Management (40): contiguous alloc, fragmentation, swap in/out,
//   lt_storage, Solar+Lunar constraint
//   Gameplay (5): weapon drop mechanics
// ============================================================================

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <pthread.h>
#include "shared_types.h"

// ── Forward declarations ──────────────────────────────────────────────────
void append_action_log(SharedGameState* shm, const char* message);

// ============================================================================
// SECTION 1 — LOW-LEVEL INVENTORY PRIMITIVES
// ============================================================================

// ----------------------------------------------------------------------------
// find_contiguous_free()
// Scans inventory[0..INVENTORY_SLOTS-1] for 'needed' consecutive WEAPON_NONE
// slots. Returns the starting index, or -1 if not found.
// This is a first-fit search — simple and matches spec requirements.
// ----------------------------------------------------------------------------
int find_contiguous_free(const int* inventory, int needed) {
    int start = -1, count = 0;
    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        if (inventory[i] == WEAPON_NONE) {
            if (count == 0) start = i;
            if (++count >= needed) return start;
        } else {
            count = 0; start = -1;
        }
    }
    return -1;
}

// ----------------------------------------------------------------------------
// count_total_free()
// Returns the total number of WEAPON_NONE slots in the inventory.
// Used to determine if eviction is even theoretically possible.
// ----------------------------------------------------------------------------
static int count_total_free(const int* inventory) {
    int n = 0;
    for (int i = 0; i < INVENTORY_SLOTS; i++)
        if (inventory[i] == WEAPON_NONE) n++;
    return n;
}

// ----------------------------------------------------------------------------
// find_weapon_start()
// Given a slot index that holds a weapon, finds the first slot of that
// contiguous weapon block. Needed because weapons span multiple slots.
// ----------------------------------------------------------------------------
static int find_weapon_start(const int* inventory, int slot) {
    int wid = inventory[slot];
    if (wid == WEAPON_NONE) return slot;
    int s = slot;
    while (s > 0 && inventory[s - 1] == wid) s--;
    return s;
}

// ----------------------------------------------------------------------------
// place_weapon()
// Fills 'needed' consecutive slots starting at 'start' with weapon_id.
// Caller must ensure enough free space exists.
// ----------------------------------------------------------------------------
static void place_weapon(int* inventory, int start, int weapon_id) {
    int needed = weapon_slot_size[weapon_id];
    for (int i = start; i < start + needed; i++)
        inventory[i] = weapon_id;
}

// ----------------------------------------------------------------------------
// remove_weapon_at_start()
// Clears all slots belonging to the weapon starting at 'start_slot'.
// Returns the weapon_id that was removed.
// ----------------------------------------------------------------------------
static int remove_weapon_at_start(int* inventory, int start_slot) {
    int wid = inventory[start_slot];
    if (wid == WEAPON_NONE) return WEAPON_NONE;
    int needed = weapon_slot_size[wid];
    for (int i = start_slot; i < start_slot + needed && i < INVENTORY_SLOTS; i++)
        inventory[i] = WEAPON_NONE;
    return wid;
}

// ----------------------------------------------------------------------------
// lt_storage_push()
// Adds weapon_id to the entity's long-term storage array.
// Returns 1 on success, 0 if storage is full (MAX_LT_STORAGE).
// ----------------------------------------------------------------------------
static int lt_storage_push(Entity* e, int weapon_id) {
    if (e->lt_storage_count >= MAX_LT_STORAGE) return 0;
    e->lt_storage[e->lt_storage_count++] = weapon_id;
    return 1;
}

// ----------------------------------------------------------------------------
// lt_storage_remove()
// Removes the first occurrence of weapon_id from lt_storage.
// Shifts remaining entries left to keep the array compact.
// Returns 1 if found and removed, 0 if not found.
// ----------------------------------------------------------------------------
static int lt_storage_remove(Entity* e, int weapon_id) {
    for (int i = 0; i < e->lt_storage_count; i++) {
        if (e->lt_storage[i] == weapon_id) {
            for (int j = i; j < e->lt_storage_count - 1; j++)
                e->lt_storage[j] = e->lt_storage[j + 1];
            e->lt_storage[--e->lt_storage_count] = WEAPON_NONE;
            return 1;
        }
    }
    return 0;
}

// ----------------------------------------------------------------------------
// update_artifact_flags()
// Keeps holds_solar_core / holds_lunar_blade in sync with inventory contents.
// Called after any add or remove from primary inventory.
// ----------------------------------------------------------------------------
static void update_artifact_flags(Entity* e) {
    e->holds_solar_core  = 0;
    e->holds_lunar_blade = 0;
    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        if (e->inventory[i] == WEAPON_SOLAR_CORE)  e->holds_solar_core  = 1;
        if (e->inventory[i] == WEAPON_LUNAR_BLADE) e->holds_lunar_blade = 1;
    }
}

// ============================================================================
// SECTION 2 — MAIN ALLOCATOR: inventory_add_weapon()
//
// Attempts to add weapon_id to entity e's primary inventory.
//
// Algorithm:
//   1. Check if weapon fits at all (needed <= INVENTORY_SLOTS).
//   2. Try direct first-fit placement (no eviction needed).
//   3. If fragmented/no space: find the smallest window of INVENTORY that,
//      when cleared, gives 'needed' contiguous free slots.
//      Evict ONLY the weapons in that window (minimum eviction).
//   4. Place the weapon.
//   5. Update artifact flags.
//
// Returns 1 on success, 0 if impossible (weapon larger than inventory).
// CALLER must hold master_mutex.
// ============================================================================
int inventory_add_weapon(SharedGameState* shm, Entity* e,
                         int player_slot, int weapon_id) {
    int needed = weapon_slot_size[weapon_id];
    char log_msg[ACTION_LOG_WIDTH];

    if (needed > INVENTORY_SLOTS) return 0;   // weapon physically cannot fit

    // ── Step 1: Try direct placement ────────────────────────────────────
    int start = find_contiguous_free(e->inventory, needed);
    if (start != -1) {
        place_weapon(e->inventory, start, weapon_id);
        update_artifact_flags(e);
        snprintf(log_msg, ACTION_LOG_WIDTH,
                 "Player %d picked up %s (slots %d-%d)",
                 player_slot, weapon_name[weapon_id], start, start + needed - 1);
        append_action_log(shm, log_msg);
        std::cout << "[ALLOC] " << log_msg << "\n";
        return 1;
    }

    // ── Step 2: Not enough contiguous space — need eviction ──────────────
    // Check if total free space is enough (considering all slots as one block)
    // If even total free < needed, we must evict occupied slots too.

    // Strategy: sliding window of 'needed' consecutive slots.
    // For each window position, count how many DISTINCT weapon instances
    // overlap it. Pick the window with the MINIMUM evictions needed.
    // This ensures we swap out only the minimum necessary weapons.

    int best_window_start = -1;
    int best_evict_count  = INT_MAX;

    for (int ws = 0; ws <= INVENTORY_SLOTS - needed; ws++) {
        // Count distinct weapon instances (by their start slot) in window [ws, ws+needed)
        int evict_count = 0;
        int counted[INVENTORY_SLOTS] = {0};  // tracks which weapon starts we've counted

        for (int i = ws; i < ws + needed; i++) {
            if (e->inventory[i] != WEAPON_NONE) {
                int wstart = find_weapon_start(e->inventory, i);
                if (!counted[wstart]) {
                    counted[wstart] = 1;
                    evict_count++;
                }
            }
        }

        if (evict_count < best_evict_count) {
            best_evict_count  = evict_count;
            best_window_start = ws;
        }
    }

    if (best_window_start == -1) return 0;   // should never happen

    // ── Step 3: Evict weapons in the best window ─────────────────────────
    int window_end = best_window_start + needed;
    int i = best_window_start;
    while (i < window_end) {
        if (e->inventory[i] != WEAPON_NONE) {
            int wstart   = find_weapon_start(e->inventory, i);
            int evicted  = remove_weapon_at_start(e->inventory, wstart);
            lt_storage_push(e, evicted);
            update_artifact_flags(e);

            snprintf(log_msg, ACTION_LOG_WIDTH,
                     "Player %d: %s evicted to long-term storage (making room for %s)",
                     player_slot, weapon_name[evicted], weapon_name[weapon_id]);
            append_action_log(shm, log_msg);
            std::cout << "[ALLOC] " << log_msg << "\n";

            // After removal, slots shifted — re-scan from window start
            i = best_window_start;
        } else {
            i++;
        }
    }

    // ── Step 4: Place the new weapon ────────────────────────────────────
    start = find_contiguous_free(e->inventory, needed);
    if (start == -1) {
        // Fallback: try the exact window start (should be clear now)
        start = best_window_start;
    }
    place_weapon(e->inventory, start, weapon_id);
    update_artifact_flags(e);

    snprintf(log_msg, ACTION_LOG_WIDTH,
             "Player %d picked up %s (slots %d-%d, after eviction)",
             player_slot, weapon_name[weapon_id], start, start + needed - 1);
    append_action_log(shm, log_msg);
    std::cout << "[ALLOC] " << log_msg << "\n";
    return 1;
}

// ============================================================================
// SECTION 3 — SWAP IN ACTION (Phase 13)
//
// Player uses ACTION_SWAP_IN to retrieve a weapon from long-term storage.
// Steps:
//   1. Verify weapon is in lt_storage.
//   2. Remove it from lt_storage.
//   3. Add it to primary inventory (allocator handles eviction if needed).
//   4. Mark just_swapped_in_weapon so HIP knows it can't be used this turn.
//   5. Update artifact flags.
//
// CALLER must hold master_mutex.
// ============================================================================
int execute_swap_in(SharedGameState* shm, int player_slot, int weapon_id) {
    Entity* e = &shm->players[player_slot];
    char log_msg[ACTION_LOG_WIDTH];

    // Step 1: Verify weapon exists in lt_storage
    int found = 0;
    for (int i = 0; i < e->lt_storage_count; i++) {
        if (e->lt_storage[i] == weapon_id) { found = 1; break; }
    }
    if (!found) {
        snprintf(log_msg, ACTION_LOG_WIDTH,
                 "Player %d Swap In FAILED: %s not in long-term storage",
                 player_slot, weapon_name[weapon_id]);
        append_action_log(shm, log_msg);
        return 0;
    }

    // Step 2: Remove from lt_storage
    lt_storage_remove(e, weapon_id);

    // Step 3: Add to primary inventory (may trigger eviction)
    int result = inventory_add_weapon(shm, e, player_slot, weapon_id);

    if (result) {
        // Step 4: Mark as just-swapped — cannot be used this turn
        e->just_swapped_in_weapon = weapon_id;
        update_artifact_flags(e);

        snprintf(log_msg, ACTION_LOG_WIDTH,
                 "Player %d swapped in %s from storage (cannot use this turn)",
                 player_slot, weapon_name[weapon_id]);
    } else {
        // Failed — put it back in storage
        lt_storage_push(e, weapon_id);
        snprintf(log_msg, ACTION_LOG_WIDTH,
                 "Player %d Swap In FAILED: no space for %s even after eviction",
                 player_slot, weapon_name[weapon_id]);
    }
    append_action_log(shm, log_msg);
    std::cout << "[ALLOC] " << log_msg << "\n";
    return result;
}

// ============================================================================
// SECTION 4 — WEAPON DROP MECHANICS (Phase 13)
//
// When an NPC is defeated, a random weapon MAY drop (spec: "a random weapon
// from the game world", NOT the NPC's held weapon).
// The Arbiter sets shm->dropped_weapon_id and drop_awaiting_player_choice=1.
// HIP reads these flags at the start of the next player turn and shows a prompt.
// If player declines, npc_pickup_weapon() gives it to a random alive NPC.
// ============================================================================

// ----------------------------------------------------------------------------
// roll_weapon_drop()
// Called by game_loop.cpp after handle_npc_death().
// 40% chance to drop a weapon. Solar Core and Lunar Blade are excluded
// (they are unique artifacts — only obtainable via specific game events).
// Returns WEAPON_NONE if no drop, otherwise a random droppable weapon_id.
// ----------------------------------------------------------------------------
int roll_weapon_drop() {
    if (rand() % 10 >= 4) return WEAPON_NONE;   // 60% no drop

    // Droppable weapons: Iron Halberd through Splinter Stick (IDs 2-7)
    // Solar Core (0) and Lunar Blade (1) are NOT random drops
    return (rand() % (NUM_WEAPON_TYPES - 2)) + 2;  // 2..7
}

// ----------------------------------------------------------------------------
// present_drop_to_players()
// Sets shared memory flags so HIP will display the pickup prompt
// on the next player's turn.
// CALLER must hold master_mutex.
// ----------------------------------------------------------------------------
void present_drop_to_players(SharedGameState* shm, int weapon_id) {
    shm->dropped_weapon_id           = weapon_id;
    shm->drop_awaiting_player_choice = 1;

    char log_msg[ACTION_LOG_WIDTH];
    snprintf(log_msg, ACTION_LOG_WIDTH,
             "*** WEAPON DROPPED: %s (%d slots, %d dmg) — waiting for player choice ***",
             weapon_name[weapon_id], weapon_slot_size[weapon_id], weapon_damage[weapon_id]);
    append_action_log(shm, log_msg);
    std::cout << "[ALLOC] " << log_msg << "\n";
}

// ----------------------------------------------------------------------------
// player_pickup_weapon()
// Called by game_loop after HIP signals the player chose to pick it up.
// Adds the weapon to the choosing player's inventory.
// CALLER must hold master_mutex.
// ----------------------------------------------------------------------------
void player_pickup_weapon(SharedGameState* shm, int player_slot, int weapon_id) {
    Entity* e = &shm->players[player_slot];
    inventory_add_weapon(shm, e, player_slot, weapon_id);

    // Clear drop state
    shm->dropped_weapon_id           = WEAPON_NONE;
    shm->drop_awaiting_player_choice = 0;
}

// ----------------------------------------------------------------------------
// npc_pickup_weapon()
// Called when a player DECLINES the weapon drop.
// Spec: "If the player does not pick it up, an enemy is GUARANTEED to pick it up."
// Spec: "If an NPC holds a weapon, that weapon will NOT drop when it dies."
// We give it to the first alive NPC that has inventory space.
// CALLER must hold master_mutex.
// ----------------------------------------------------------------------------
void npc_pickup_weapon(SharedGameState* shm, int weapon_id) {
    char log_msg[ACTION_LOG_WIDTH];
    int needed = weapon_slot_size[weapon_id];

    for (int i = 0; i < shm->num_npcs_concurrent; i++) {
        Entity* n = &shm->npcs[i];
        if (!n->is_alive) continue;

        int start = find_contiguous_free(n->inventory, needed);
        if (start != -1) {
            place_weapon(n->inventory, start, weapon_id);

            snprintf(log_msg, ACTION_LOG_WIDTH,
                     "NPC %d picked up dropped %s (will NOT drop on death)",
                     n->id, weapon_name[weapon_id]);
            append_action_log(shm, log_msg);
            std::cout << "[ALLOC] " << log_msg << "\n";

            shm->dropped_weapon_id           = WEAPON_NONE;
            shm->drop_awaiting_player_choice = 0;
            return;
        }
    }

    // No NPC had space — weapon disappears (edge case, very rare)
    snprintf(log_msg, ACTION_LOG_WIDTH,
             "Dropped %s: no NPC had inventory space — weapon lost",
             weapon_name[weapon_id]);
    append_action_log(shm, log_msg);
    shm->dropped_weapon_id           = WEAPON_NONE;
    shm->drop_awaiting_player_choice = 0;
}

// ============================================================================
// SECTION 5 — ECLIPSE RELIC INTRODUCTION (Phase 14 hook)
//
// The Eclipse Relic is introduced dynamically at runtime.
// We check once after kill #5 (halfway through the game) whether to
// introduce it. This gives enough game time for deadlock scenarios to occur.
// ============================================================================
void check_eclipse_relic_spawn(SharedGameState* shm) {
    // CRITICAL: table_mutex FIRST, then master_mutex (locking order discipline)
    // to prevent deadlock with deadlock_monitor thread
    pthread_mutex_lock(&shm->resources.table_mutex);
    pthread_mutex_lock(&shm->master_mutex);

    if (shm->eclipse_spawn_checked ||
        shm->total_kills < 5 ||
        shm->resources.eclipse_relic_in_game) {
        pthread_mutex_unlock(&shm->master_mutex);
        pthread_mutex_unlock(&shm->resources.table_mutex);
        return;
    }

    shm->eclipse_spawn_checked = 1;

    // 50% chance to introduce Eclipse Relic at kill #5
    if (rand() % 2 == 0) {
        shm->resources.eclipse_relic_in_game  = 1;
        shm->resources.eclipse_relic_holder   = -1;  // free on battlefield
        pthread_mutex_unlock(&shm->master_mutex);
        pthread_mutex_unlock(&shm->resources.table_mutex);

        append_action_log(shm,
            "*** ECLIPSE RELIC has appeared on the battlefield! "
            "Whoever holds it gains immense power. ***");
        std::cout << "\n[ARBITER] *** ECLIPSE RELIC APPEARED! ***\n\n";
    } else {
        pthread_mutex_unlock(&shm->master_mutex);
        pthread_mutex_unlock(&shm->resources.table_mutex);
    }
}

// ============================================================================
// SECTION 6 — DEBUG UTILITY
// ============================================================================

// ----------------------------------------------------------------------------
// print_inventory()
// Prints a visual representation of the inventory to stdout.
// Useful for testing and the demo.
// ----------------------------------------------------------------------------
void print_inventory(const Entity* e, const char* label) {
    std::cout << "[INVENTORY] " << label << " | ";
    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        if (e->inventory[i] == WEAPON_NONE) {
            std::cout << ".";
        } else {
            // Show first letter of weapon name
            std::cout << weapon_name[e->inventory[i]][0];
        }
    }
    std::cout << " | LT: " << e->lt_storage_count << " weapons\n";
}