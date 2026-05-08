// ============================================================================
// hip/input_handler.cpp — Player Input Menu & Action Reader (Phases 5+11+13)
// CS 2006 Operating Systems | Spring 2026 | Chrono Rift
// Roll Number: 24i-0847  |  Partner: 24i-0650
//
// CHANGES vs Phase 5 version:
//   Phase 11: ACTION_ULTIMATE added as menu option 7
//             Shown only when player holds Solar Core + Lunar Blade
//             Input loop now accepts 0-7
//   Phase 13: handle_weapon_drop_choice() fully implemented
//             Sets drop_awaiting_player_choice = 2 (yes) or 3 (no)
//             game_loop's dispatch_turn reads these states to call
//             player_pickup_weapon() or npc_pickup_weapon()
// ============================================================================

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include "shared_types.h"

#define LINE "  ─────────────────────────────────────────────────"

// ============================================================================
// print_hp_bar()
// ============================================================================
static void print_hp_bar(int current, int max, int bar_width = 16) {
    int filled = (max > 0) ? (current * bar_width / max) : 0;
    std::cout << "  [";
    for (int i = 0; i < bar_width; i++)
        std::cout << (i < filled ? "\u2588" : "\u2591");
    std::cout << "]  " << current << "/" << max;
}

// ============================================================================
// print_stamina_bar()
// ============================================================================
static void print_stamina_bar(float current, int max, int bar_width = 16) {
    int filled = (max > 0) ? (int)(current * bar_width / max) : 0;
    std::cout << "  [";
    for (int i = 0; i < bar_width; i++)
        std::cout << (i < filled ? "\u2593" : "\u2591");
    std::cout << "]  " << (int)current << "/" << max;
}

// ============================================================================
// display_player_menu()
// Shows the full combat state and available actions for one player turn.
// ============================================================================
void display_player_menu(SharedGameState* shm, int player_index) {
    Entity* p = &shm->players[player_index];

    std::cout << "\n\n";
    std::cout << "  ╔══════════════════════════════════════════════════╗\n";
    std::cout << "  ║         CHRONO RIFT — Player " << player_index
              << "'s Turn              ║\n";
    std::cout << "  ╚══════════════════════════════════════════════════╝\n";

    // ── Player stats ──────────────────────────────────────────────────────
    std::cout << "\n  ► Player " << player_index
              << (p->is_stunned ? "  [STUNNED]" : "") << "\n";
    std::cout << "    HP:      ";
    print_hp_bar(p->hp, p->max_hp);
    std::cout << "\n";
    std::cout << "    Stamina: ";
    print_stamina_bar(p->stamina, p->max_stamina);
    std::cout << "\n";
    std::cout << "    Damage:  " << p->base_damage << " (base strike)\n";

    // ── Inventory summary ─────────────────────────────────────────────────
    std::cout << "\n  ► Inventory (20 slots):\n";
    bool has_weapon = false;
    for (int s = 0; s < INVENTORY_SLOTS; s++) {
        int wid = p->inventory[s];
        if (wid != WEAPON_NONE && (s == 0 || p->inventory[s-1] != wid)) {
            std::cout << "      Slot " << s << ": [" << weapon_name[wid] << "]"
                      << "  " << weapon_slot_size[wid] << " slots"
                      << "  " << weapon_damage[wid] << " dmg";
            if (wid == p->just_swapped_in_weapon)
                std::cout << "  (JUST SWAPPED IN — cannot use this turn)";
            std::cout << "\n";
            has_weapon = true;
        }
    }
    if (!has_weapon) std::cout << "      (empty)\n";

    // ── Long-term storage summary ─────────────────────────────────────────
    if (p->lt_storage_count > 0) {
        std::cout << "\n  ► Long-term storage (" << p->lt_storage_count << " weapons):\n";
        for (int i = 0; i < p->lt_storage_count; i++) {
            int wid = p->lt_storage[i];
            if (wid != WEAPON_NONE)
                std::cout << "      [" << i << "] " << weapon_name[wid]
                          << "  (" << weapon_slot_size[wid] << " slots, "
                          << weapon_damage[wid] << " dmg)\n";
        }
    }

    // ── Enemy status ──────────────────────────────────────────────────────
    std::cout << "\n  ► Enemies on screen:\n";
    bool any_alive = false;
    for (int i = 0; i < shm->num_npcs_concurrent; i++) {
        Entity* n = &shm->npcs[i];
        if (!n->is_alive) continue;
        any_alive = true;
        std::cout << "      [" << i << "] NPC " << n->id
                  << "  HP: " << n->hp << "/" << n->max_hp
                  << "  Stamina: " << (int)n->stamina << "/" << n->max_stamina
                  << "  DMG: " << n->base_damage
                  << (n->is_stunned ? "  [STUNNED]" : "") << "\n";
    }
    if (!any_alive) std::cout << "      (none alive — all killed!)\n";

    // ── Party status (multiplayer) ────────────────────────────────────────
    if (shm->num_players > 1) {
        std::cout << "\n  ► Your party:\n";
        for (int i = 0; i < shm->num_players; i++) {
            Entity* ally = &shm->players[i];
            std::cout << "      Player " << i
                      << (i == player_index ? " (YOU)" : "      ")
                      << "  HP: " << ally->hp << "/" << ally->max_hp
                      << (ally->is_alive   ? "" : "  [DEAD]")
                      << (ally->is_stunned ? "  [STUNNED]" : "") << "\n";
        }
    }

    // ── Kill counter ──────────────────────────────────────────────────────
    std::cout << "\n  ► Kills: " << shm->total_kills << " / 10\n";

    // ── Action menu ───────────────────────────────────────────────────────
    std::cout << "\n" << LINE << "\n";
    std::cout << "  Choose action:\n";
    std::cout << "    1. Attack (Strike)   — deal " << p->base_damage << " HP damage\n";
    std::cout << "    2. Attack (Exhaust)  — deal " << p->base_damage << " stamina damage\n";

    // Option 3: Use Weapon
    bool has_usable = false;
    for (int s = 0; s < INVENTORY_SLOTS; s++) {
        int wid = p->inventory[s];
        if (wid != WEAPON_NONE && wid != p->just_swapped_in_weapon &&
            (s == 0 || p->inventory[s-1] != wid)) {
            has_usable = true; break;
        }
    }
    if (has_usable)
        std::cout << "    3. Use Weapon        — use a weapon from your inventory\n";
    else
        std::cout << "    3. Use Weapon        — (no usable weapon in inventory)\n";

    // Option 4: Swap In
    if (p->lt_storage_count > 0)
        std::cout << "    4. Swap In           — retrieve a weapon from long-term storage\n";
    else
        std::cout << "    4. Swap In           — (long-term storage is empty)\n";

    std::cout << "    5. Heal              — restore " << (p->max_hp / 10) << " HP (10%)\n";
    std::cout << "    6. Skip              — skip turn; stamina → 50%\n";

    // ── Phase 11: Option 7 — Ultimate Ability ─────────────────────────────
    // Only shown (and usable) when player holds BOTH Solar Core AND Lunar Blade
    // in their primary inventory (not long-term storage).
    bool can_ultimate = (p->holds_solar_core && p->holds_lunar_blade);
    if (can_ultimate)
        std::cout << "    7. ULTIMATE ABILITY  — *** Solar Core + Lunar Blade: SUSPEND ALL NPCS 10s ***\n";
    else
        std::cout << "    7. Ultimate Ability  — (requires Solar Core + Lunar Blade in inventory)\n";

    std::cout << "    0. Quit game\n";
    std::cout << LINE << "\n";
    std::cout << "  > ";
}

// ============================================================================
// read_player_action()
// Reads the player's choice from stdin and populates the action fields.
// Sets *action_type = -1 to signal quit was chosen.
//
// PHASE 11 CHANGE: Input loop now accepts 0-7 (was 0-6).
//   Choice 7 maps to ACTION_ULTIMATE. No target needed — Arbiter handles it.
// ============================================================================
void read_player_action(SharedGameState* shm, int player_index,
                        int* action_type, int* target_id,
                        int* weapon_slot,  int* lt_weapon_id) {
    *action_type  = ACTION_NONE;
    *target_id    = -1;
    *weapon_slot  = -1;
    *lt_weapon_id = WEAPON_NONE;

    Entity* p = &shm->players[player_index];

    // ── Read main choice (0-7) ────────────────────────────────────────────
    int choice = -1;
    while (choice < 0 || choice > 7) {
        if (!(std::cin >> choice)) {
            std::cin.clear();
            choice = 0;
            break;
        }
        if (choice < 0 || choice > 7)
            std::cout << "  Invalid choice. Enter 0-7: ";
    }

    // ── Quit ─────────────────────────────────────────────────────────────
    if (choice == 0) {
        std::cout << "\n  [HIP] Quitting — sending SIGTERM to Arbiter...\n";
        kill(shm->arbiter_pid, SIGTERM);
        *action_type = -1;
        return;
    }

    // ── Map choice to ACTION constant ─────────────────────────────────────
    switch (choice) {
        case 1: *action_type = ACTION_ATTACK_STRIKE;  break;
        case 2: *action_type = ACTION_ATTACK_EXHAUST; break;
        case 3: *action_type = ACTION_USE_WEAPON;     break;
        case 4: *action_type = ACTION_SWAP_IN;        break;
        case 5: *action_type = ACTION_HEAL;           break;
        case 6: *action_type = ACTION_SKIP;           break;
        case 7: *action_type = ACTION_ULTIMATE;       break;
        default: *action_type = ACTION_SKIP;          break;
    }

    // ── Phase 11: Ultimate — no target needed, just validate eligibility ──
    if (*action_type == ACTION_ULTIMATE) {
        if (!p->holds_solar_core || !p->holds_lunar_blade) {
            std::cout << "  Cannot use Ultimate — need Solar Core + Lunar Blade in inventory.\n";
            std::cout << "  Action changed to Skip.\n";
            *action_type = ACTION_SKIP;
        }
        goto done;   // no target, no weapon slot needed
    }

    // ── Actions that need a target enemy ─────────────────────────────────
    if (*action_type == ACTION_ATTACK_STRIKE  ||
        *action_type == ACTION_ATTACK_EXHAUST ||
        *action_type == ACTION_USE_WEAPON) {

        bool any_alive = false;
        for (int i = 0; i < shm->num_npcs_concurrent; i++)
            if (shm->npcs[i].is_alive) { any_alive = true; break; }

        if (!any_alive) {
            std::cout << "  No alive enemies — action changed to Skip.\n";
            *action_type = ACTION_SKIP;
            goto done;
        }

        std::cout << "  Select target (enter slot number): ";
        int t = -1;
        while (t < 0) {
            if (!(std::cin >> t)) { std::cin.clear(); t = -1; break; }
            if (t < 0 || t >= shm->num_npcs_concurrent || !shm->npcs[t].is_alive) {
                std::cout << "  Invalid target. Enter a valid enemy slot: ";
                t = -1;
            }
        }
        *target_id = t;
    }

    // ── Use Weapon: also need inventory slot ──────────────────────────────
    if (*action_type == ACTION_USE_WEAPON) {
        std::cout << "  Select weapon slot:\n";
        bool any_shown = false;
        for (int s = 0; s < INVENTORY_SLOTS; s++) {
            int wid = p->inventory[s];
            if (wid != WEAPON_NONE && wid != p->just_swapped_in_weapon &&
                (s == 0 || p->inventory[s-1] != wid)) {
                std::cout << "    Slot " << s << ": " << weapon_name[wid]
                          << "  (" << weapon_damage[wid] << " dmg)\n";
                any_shown = true;
            }
        }
        if (!any_shown) {
            std::cout << "  No usable weapons — action changed to Strike.\n";
            *action_type = ACTION_ATTACK_STRIKE;
            goto done;
        }
        std::cout << "  > ";
        int ws = -1;
        while (ws < 0) {
            if (!(std::cin >> ws)) { std::cin.clear(); ws = -1; break; }
            if (ws < 0 || ws >= INVENTORY_SLOTS ||
                p->inventory[ws] == WEAPON_NONE ||
                p->inventory[ws] == p->just_swapped_in_weapon) {
                std::cout << "  Invalid slot. Try again: ";
                ws = -1;
            }
        }
        *weapon_slot = ws;
    }

    // ── Swap In: need weapon from long-term storage ───────────────────────
    if (*action_type == ACTION_SWAP_IN) {
        if (p->lt_storage_count == 0) {
            std::cout << "  Long-term storage is empty — action changed to Skip.\n";
            *action_type = ACTION_SKIP;
            goto done;
        }
        std::cout << "  Select weapon to retrieve (enter index):\n";
        for (int i = 0; i < p->lt_storage_count; i++) {
            int wid = p->lt_storage[i];
            if (wid != WEAPON_NONE)
                std::cout << "    [" << i << "] " << weapon_name[wid]
                          << "  (" << weapon_slot_size[wid] << " slots)\n";
        }
        std::cout << "  > ";
        int idx = -1;
        while (idx < 0) {
            if (!(std::cin >> idx)) { std::cin.clear(); idx = -1; break; }
            if (idx < 0 || idx >= p->lt_storage_count ||
                p->lt_storage[idx] == WEAPON_NONE) {
                std::cout << "  Invalid index. Try again: ";
                idx = -1;
            }
        }
        *lt_weapon_id = p->lt_storage[idx];
    }

done:
    const char* action_names[] = {
        "None", "Strike", "Exhaust", "Use Weapon",
        "Swap In", "Heal", "Skip", "Ultimate"
    };
    if (*action_type >= 0 && *action_type <= 7) {
        std::cout << "\n  ✓ Action chosen: " << action_names[*action_type];
        if (*target_id    >= 0) std::cout << "  Target slot: " << *target_id;
        if (*weapon_slot  >= 0) std::cout << "  Weapon slot: " << *weapon_slot;
        if (*lt_weapon_id != WEAPON_NONE)
            std::cout << "  Weapon: " << weapon_name[*lt_weapon_id];
        std::cout << "\n";
    }
}

// ============================================================================
// handle_weapon_drop_choice()
// Phase 13 — FULLY IMPLEMENTED (was stub before)
//
// Called at the START of each player turn (before menu is shown).
// If drop_awaiting_player_choice == 1, a weapon was dropped and the player
// must decide to pick it up or decline.
//
// PROTOCOL with game_loop's dispatch_turn:
//   This function sets drop_awaiting_player_choice to:
//     2 = player said YES  → dispatch_turn calls player_pickup_weapon()
//     3 = player said NO   → dispatch_turn calls npc_pickup_weapon()
//   dispatch_turn then resets it to 0 after resolving the pickup.
//
// WHY state 2/3 instead of calling pickup directly:
//   HIP cannot modify game state directly (spec rule).
//   Only the Arbiter (via game_loop) applies state changes.
//   HIP signals intent via shared memory; Arbiter acts on it.
// ============================================================================
void handle_weapon_drop_choice(SharedGameState* shm, int player_index) {
    // Only the current player should handle the drop
    pthread_mutex_lock(&shm->master_mutex);
    int pending = shm->drop_awaiting_player_choice;
    int wid     = shm->dropped_weapon_id;
    pthread_mutex_unlock(&shm->master_mutex);

    if (pending != 1) return;   // no pending drop, or already handled

    std::cout << "\n  ╔══════════════════════════════════════════════════╗\n";
    std::cout << "  ║              *** WEAPON DROPPED! ***             ║\n";
    std::cout << "  ╚══════════════════════════════════════════════════╝\n";
    std::cout << "  A " << weapon_name[wid] << " appeared!\n";
    std::cout << "    Size:   " << weapon_slot_size[wid] << " inventory slots\n";
    std::cout << "    Damage: " << weapon_damage[wid] << "\n";

    // Check if player has room (informational — allocator decides for real)
    Entity* p = &shm->players[player_index];
    int free_slots = 0;
    for (int s = 0; s < INVENTORY_SLOTS; s++)
        if (p->inventory[s] == WEAPON_NONE) free_slots++;
    std::cout << "    Your free inventory slots: " << free_slots << "\n";
    if (free_slots < weapon_slot_size[wid])
        std::cout << "    (Not enough room — existing weapons may be moved to long-term storage)\n";

    std::cout << "  Pick it up? (1=Yes, 0=No): ";

    int choice = 0;
    if (!(std::cin >> choice)) { std::cin.clear(); choice = 0; }

    // ── Signal intent to Arbiter via shared memory state ─────────────────
    // DO NOT call pickup functions here — HIP cannot modify game state.
    // Set state 2 (yes) or 3 (no) — dispatch_turn in game_loop will act.
    pthread_mutex_lock(&shm->master_mutex);
    if (choice == 1) {
        shm->drop_awaiting_player_choice = 2;
        sem_post(&shm->turn.action_submitted);
        std::cout << "  [HIP] Signalled YES to Arbiter.\n";
    } else {
        shm->drop_awaiting_player_choice = 3;
        sem_post(&shm->turn.action_submitted);
        std::cout << "  [HIP] Signalled NO — weapon goes to an NPC.\n";
    }
    pthread_mutex_unlock(&shm->master_mutex);
}