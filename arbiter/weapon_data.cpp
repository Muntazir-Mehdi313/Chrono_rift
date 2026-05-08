// ============================================================================
// weapon_data.cpp — Weapon Metadata Definitions
// CS 2006 Operating Systems | Spring 2026 | Chrono Rift
//
// This file defines the three weapon metadata arrays declared extern
// in shared_types.h. It is compiled into ALL THREE executables:
//   arbiter, hip, and asp.
//
// WHY NOT IN SHARED MEMORY:
//   These arrays are read-only constants. Every process binary has
//   its own identical copy. No synchronization needed. No pointers
//   in shared memory. Accessing weapon data is just: weapon_name[wid]
//
// ARRAY ORDER — must match weapon ID constants in shared_types.h:
//   [0] WEAPON_SOLAR_CORE
//   [1] WEAPON_LUNAR_BLADE
//   [2] WEAPON_IRON_HALBERD
//   [3] WEAPON_VENOM_DAGGER
//   [4] WEAPON_THUNDERSTAFF
//   [5] WEAPON_OBSIDIAN_AXE
//   [6] WEAPON_FROSTBOW
//   [7] WEAPON_SPLINTER_STICK
// ============================================================================

#include "shared_types.h"

// Number of contiguous inventory slots each weapon occupies.
// Spec values (Section 10, Weapon Inventory and Damage Table):
//   Solar Core:    10 slots   ← artifact; Solar(10) + Lunar(10) = full 20-slot inventory
//   Lunar Blade:   10 slots   ← artifact
//   Iron Halberd:   7 slots
//   Venom Dagger:   4 slots
//   Thunderstaff:   6 slots
//   Obsidian Axe:   5 slots
//   Frostbow:       6 slots
//   Splinter Stick: 2 slots   ← intentionally small; creates fragmentation test cases
const int weapon_slot_size[NUM_WEAPON_TYPES] = {
    10,  // [0] WEAPON_SOLAR_CORE
    10,  // [1] WEAPON_LUNAR_BLADE
     7,  // [2] WEAPON_IRON_HALBERD
     4,  // [3] WEAPON_VENOM_DAGGER
     6,  // [4] WEAPON_THUNDERSTAFF
     5,  // [5] WEAPON_OBSIDIAN_AXE
     6,  // [6] WEAPON_FROSTBOW
     2   // [7] WEAPON_SPLINTER_STICK
};

// Damage output for each weapon (used when player takes ACTION_USE_WEAPON).
// Spec values (Section 10, Weapon Inventory and Damage Table):
const int weapon_damage[NUM_WEAPON_TYPES] = {
    95,  // [0] WEAPON_SOLAR_CORE
    90,  // [1] WEAPON_LUNAR_BLADE
    55,  // [2] WEAPON_IRON_HALBERD
    30,  // [3] WEAPON_VENOM_DAGGER
    50,  // [4] WEAPON_THUNDERSTAFF
    45,  // [5] WEAPON_OBSIDIAN_AXE
    48,  // [6] WEAPON_FROSTBOW
    12   // [7] WEAPON_SPLINTER_STICK
};

// Human-readable weapon names — used for display and action log messages.
// Never stored in shared memory. Only used in process-local code (HIP menu,
// ASP log, Arbiter log, rendering thread display).
const char* weapon_name[NUM_WEAPON_TYPES] = {
    "Solar Core",      // [0]
    "Lunar Blade",     // [1]
    "Iron Halberd",    // [2]
    "Venom Dagger",    // [3]
    "Thunderstaff",    // [4]
    "Obsidian Axe",    // [5]
    "Frostbow",        // [6]
    "Splinter Stick"   // [7]
};