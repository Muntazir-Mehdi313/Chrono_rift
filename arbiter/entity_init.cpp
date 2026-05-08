// ============================================================================
// arbiter/entity_init.cpp — Entity Initialization with Roll Number Seed
// CS 2006 Operating Systems | Spring 2026 | Chrono Rift
//
// Roll Number: 24i-0847  |  Partner: 24i-0650
//
// SPEC FORMULAS (Section 10):
//   Player HP:     RollNumber numeric + random(100, 1000)  → 240847 + rand
//   Player Damage: last digit of RollNumber + 10           → 7 + 10 = 17
//   Player Speed:  100 / num_players
//   Player Stamina: max = 100, starts at 0
//
//   NPC HP:        last 2 digits of RollNumber + random(50, 200) → 47 + rand
//   NPC Damage:    second-to-last digit + 10                     → 4 + 10 = 14
//   NPC Speed:     random(10, 30)
//   NPC Stamina:   max = 150, starts at 0
// ============================================================================

#include <iostream>
#include <cstdlib>
#include <cstring>
#include "shared_types.h"

// ── Roll Number Constants ─────────────────────────────────────────────────
#define ROLL_NUMBER      240847
#define ROLL_LAST_DIGIT  7       // player base_damage = 7 + 10 = 17
#define ROLL_SECOND_LAST 4       // NPC base_damage    = 4 + 10 = 14
#define ROLL_LAST_TWO    47      // NPC HP base        = 47 + random(50,200)

// ── Internal helper ───────────────────────────────────────────────────────
static void print_entity_stats(const Entity* e) {
    if (e->is_player) {
        std::cout << "  [Player " << e->id << "]"
                  << "  HP="         << e->hp
                  << "  Damage="     << e->base_damage
                  << "  Speed="      << e->speed
                  << "  MaxStamina=" << e->max_stamina
                  << std::endl;
    } else {
        std::cout << "  [NPC " << e->id << "]"
                  << "  HP="         << e->hp
                  << "  Damage="     << e->base_damage
                  << "  Speed="      << e->speed
                  << "  MaxStamina=" << e->max_stamina
                  << "  ArrivalTime=" << (e->max_stamina / e->speed) << "s"
                  << std::endl;
    }
}

// ============================================================================
// init_player()
// ============================================================================
void init_player(Entity* p, int player_index, int num_players) {
    // Identity
    p->id         = player_index;
    p->is_player  = 1;
    p->slot_index = player_index;

    // Lifecycle
    p->is_alive   = STATUS_ALIVE;
    p->is_stunned = STATUS_NOT_STUNNED;

    // OS IDs (filled when HIP registers)
    p->managing_pid = 0;
    p->thread_slot  = player_index;

    // HP: ROLL_NUMBER + random(100, 1000)
    p->max_hp = rand() % 901 + 100;
    p->hp     = p->max_hp;

    // Damage: last digit of roll + 10  →  17
    p->base_damage = ROLL_LAST_DIGIT + 10;

    // Speed: 100 / num_players
    p->speed = 100 / num_players;

    // Stamina: starts at 0, max = 100
    p->max_stamina = 100;
    p->stamina     = 0.0f;

    // Inventory: all empty
    for (int s = 0; s < INVENTORY_SLOTS; s++) p->inventory[s] = WEAPON_NONE;
    for (int s = 0; s < MAX_LT_STORAGE;  s++) p->lt_storage[s] = WEAPON_NONE;
    p->lt_storage_count = 0;

    // Artifact flags
    p->holds_solar_core  = 0;
    p->holds_lunar_blade = 0;

    // Deadlock tracking
    p->waiting_for_resource = WEAPON_NONE;

    // Action buffer
    p->action_ready           = 0;
    p->action_type            = ACTION_NONE;
    p->action_target_id       = -1;
    p->action_weapon_slot     = -1;
    p->action_lt_weapon_id    = WEAPON_NONE;
    p->just_swapped_in_weapon = WEAPON_NONE;
}

// ============================================================================
// init_npc()
// ============================================================================
void init_npc(Entity* n, int slot_index, int total_npcs_spawned) {
    // Identity: id = global counter so IDs stay unique after respawn
    n->id         = total_npcs_spawned;
    n->is_player  = 0;
    n->slot_index = slot_index;

    // Lifecycle
    n->is_alive   = STATUS_ALIVE;
    n->is_stunned = STATUS_NOT_STUNNED;

    // OS IDs (filled when ASP registers)
    n->managing_pid = 0;
    n->thread_slot  = slot_index;

    // HP: last 2 digits of roll + random(50, 200)  →  47 + rand(50-200)
    n->max_hp = ROLL_LAST_TWO + (rand() % 151 + 50);
    n->hp     = n->max_hp;

    // Damage: second-to-last digit + 10  →  14
    n->base_damage = ROLL_SECOND_LAST + 10;

    // Speed: random 10 to 30
    n->speed = rand() % 21 + 10;

    // Stamina: starts at 0, max = 150
    n->max_stamina = 150;
    n->stamina     = 0.0f;

    // Inventory: all empty
    for (int s = 0; s < INVENTORY_SLOTS; s++) n->inventory[s] = WEAPON_NONE;
    for (int s = 0; s < MAX_LT_STORAGE;  s++) n->lt_storage[s] = WEAPON_NONE;
    n->lt_storage_count = 0;

    // Artifact flags
    n->holds_solar_core  = 0;
    n->holds_lunar_blade = 0;

    // Deadlock tracking
    n->waiting_for_resource = WEAPON_NONE;

    // Action buffer
    n->action_ready           = 0;
    n->action_type            = ACTION_NONE;
    n->action_target_id       = -1;
    n->action_weapon_slot     = -1;
    n->action_lt_weapon_id    = WEAPON_NONE;
    n->just_swapped_in_weapon = WEAPON_NONE;
}

// ============================================================================
// init_all_entities()
// Called by arbiter.cpp after setting num_players and num_npcs_concurrent.
// srand(ROLL_NUMBER) must be called before this function.
// ============================================================================
void init_all_entities(SharedGameState* shm) {
    std::cout << "\n[ARBITER] Initializing entities with seed " << ROLL_NUMBER
              << " (Roll: 24i-0847)" << std::endl;

    // Players
    std::cout << "[ARBITER] Player Party (" << shm->num_players << " character(s)):" << std::endl;
    std::cout << "[ARBITER]   HP     = rand(100-1000)" << std::endl;
    std::cout << "[ARBITER]   Damage = " << ROLL_LAST_DIGIT << " + 10 = "
              << (ROLL_LAST_DIGIT + 10) << std::endl;
    std::cout << "[ARBITER]   Speed  = 100 / " << shm->num_players
              << " = " << (100 / shm->num_players) << std::endl;

    for (int i = 0; i < shm->num_players; i++) {
        init_player(&shm->players[i], i, shm->num_players);
        print_entity_stats(&shm->players[i]);
    }

    // NPCs
    std::cout << "[ARBITER] Enemy Force (" << shm->num_npcs_concurrent << " concurrent):" << std::endl;
    std::cout << "[ARBITER]   HP     = " << ROLL_LAST_TWO << " + rand(50-200)" << std::endl;
    std::cout << "[ARBITER]   Damage = " << ROLL_SECOND_LAST << " + 10 = "
              << (ROLL_SECOND_LAST + 10) << std::endl;
    std::cout << "[ARBITER]   Speed  = rand(10-30)" << std::endl;

    for (int i = 0; i < shm->num_npcs_concurrent; i++) {
        init_npc(&shm->npcs[i], i, shm->total_npcs_spawned);
        shm->total_npcs_spawned++;
        print_entity_stats(&shm->npcs[i]);
    }

    std::cout << "[ARBITER] All entities initialized.\n" << std::endl;
}

// ============================================================================
// respawn_npc()
// Called when an NPC dies and total_kills < 10.
// ============================================================================
void respawn_npc(SharedGameState* shm, int slot_index) {
    init_npc(&shm->npcs[slot_index], slot_index, shm->total_npcs_spawned);
    shm->total_npcs_spawned++;
    std::cout << "[ARBITER] NPC respawned in slot " << slot_index
              << " (ID=" << shm->npcs[slot_index].id << ")"
              << "  HP="    << shm->npcs[slot_index].hp
              << "  Speed=" << shm->npcs[slot_index].speed
              << std::endl;
}