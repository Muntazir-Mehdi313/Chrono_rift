#pragma once

// ============================================================================
// shared_types.h — Master Shared Memory Layout for Chrono Rift
// CS 2006 Operating Systems | Spring 2026 | NUCES
//
// THIS FILE IS THE SINGLE SOURCE OF TRUTH FOR ALL SHARED DATA.
// It is compiled into arbiter, hip, and asp — all three must use
// the EXACT same version. The Makefile copies this from arbiter/
// to hip/ and asp/ before every build.
//
// CRITICAL RULES FOR THIS FILE:
//   1. NO raw pointers inside any struct — shared memory maps at
//      different virtual addresses in each process. Use array indices.
//   2. ALL arrays must be fixed compile-time sizes — no dynamic alloc.
//   3. pthread_mutex_t and sem_t live INSIDE SharedGameState so they
//      are physically in the shared memory block, not on any process stack.
//   4. Every mutex must be initialized with PTHREAD_PROCESS_SHARED.
//   5. Every semaphore must be initialized with pshared = 1.
// ============================================================================

#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>   // pid_t

// ============================================================================
// SECTION 1 — COMPILE-TIME CONSTANTS
// ============================================================================

// Shared memory object name (POSIX — must start with /)
#define SHM_NAME                "/chrono_rift_shm"

// Entity limits
#define MAX_PLAYERS             4       // spec: 1-4 player characters
#define MAX_NPCS                9       // spec: 2-9 concurrent enemies on screen

// Inventory
#define INVENTORY_SLOTS         20      // spec: primary inventory is a linear array of 20 slots
#define MAX_LT_STORAGE          50      // per-player long-term weapon storage (unbounded in spec,
                                        // 50 is a safe practical ceiling — no weapon type > 8,
                                        // and you can't fill 50 slots faster than you swap)

// Action log (for rendering thread)
#define ACTION_LOG_LINES        30      // circular buffer — last 30 actions visible
#define ACTION_LOG_WIDTH        200     // max chars per log line (including null terminator)

// ============================================================================
// SECTION 2 — WEAPON DEFINITIONS
// ============================================================================
// Weapon IDs are array indices into the weapon metadata arrays.
// WEAPON_NONE (-1) means "empty" in inventory slots and storage.
// These IDs are used throughout shared memory — never store weapon names
// or pointers in shared mem, only these integer IDs.

#define WEAPON_NONE             -1

#define WEAPON_SOLAR_CORE        0      // 10 slots | 95 dmg | ARTIFACT (1 instance)
#define WEAPON_LUNAR_BLADE       1      // 10 slots | 90 dmg | ARTIFACT (1 instance)
#define WEAPON_IRON_HALBERD      2      //  7 slots | 55 dmg
#define WEAPON_VENOM_DAGGER      3      //  4 slots | 30 dmg
#define WEAPON_THUNDERSTAFF      4      //  6 slots | 50 dmg
#define WEAPON_OBSIDIAN_AXE      5      //  5 slots | 45 dmg
#define WEAPON_FROSTBOW          6      //  6 slots | 48 dmg
#define WEAPON_SPLINTER_STICK    7      //  2 slots | 12 dmg | intentional fragmentation weapon

#define NUM_WEAPON_TYPES         8

// Weapon metadata — NOT in shared memory (same in every process binary).
// Defined in weapon_data.cpp, compiled into all three executables.
// Use:  weapon_slot_size[WEAPON_IRON_HALBERD]  →  7
//       weapon_damage[WEAPON_SOLAR_CORE]        → 95
//       weapon_name[WEAPON_VENOM_DAGGER]        → "Venom Dagger"
extern const int   weapon_slot_size[NUM_WEAPON_TYPES];
extern const int   weapon_damage[NUM_WEAPON_TYPES];
extern const char* weapon_name[NUM_WEAPON_TYPES];

// ============================================================================
// SECTION 3 — ACTION TYPE CONSTANTS
// ============================================================================
// These are the only valid values for Entity::action_type.
// HIP writes one of these; Arbiter reads and applies it.

#define ACTION_NONE              0
#define ACTION_ATTACK_STRIKE     1   // reduce target HP by actor's base_damage
#define ACTION_ATTACK_EXHAUST    2   // reduce target STAMINA by actor's base_damage
#define ACTION_USE_WEAPON        3   // reduce target HP by weapon_damage[slot's weapon]
#define ACTION_SWAP_IN           4   // retrieve weapon from lt_storage; can't use this turn
#define ACTION_HEAL              5   // restore 10% of actor's max_hp to actor's hp
#define ACTION_SKIP              6   // no combat effect; stamina → 50% of max instead of 0
#define ACTION_ULTIMATE          7   // trigger Ultimate Ability (requires Solar+Lunar in inv)

// ============================================================================
// SECTION 4 — STATUS FLAGS
// ============================================================================

#define STATUS_ALIVE             1
#define STATUS_DEAD              0

#define STATUS_STUNNED           1
#define STATUS_NOT_STUNNED       0

// ============================================================================
// SECTION 5 — GAME RESULT CONSTANTS
// ============================================================================

#define GAME_ONGOING             0
#define GAME_WIN                 1   // players killed 10 total enemies
#define GAME_LOSE                2   // all player characters died
#define GAME_QUIT                3   // HIP sent SIGTERM to Arbiter

// ============================================================================
// SECTION 6 — ENTITY STRUCT
// Represents a single player character OR a single NPC.
// Both use the same struct — is_player flag differentiates them.
// This struct lives inside SharedGameState arrays — it IS in shared memory.
// ============================================================================

typedef struct {

    // ── Identity ─────────────────────────────────────────────────────────
    int  id;                // Unique entity ID.
                            //   Players: 0 to (num_players-1)
                            //   NPCs:    unique across all ever spawned
                            //            (total_npcs_spawned counter)
    int  is_player;         // 1 = player character (HIP manages),
                            // 0 = NPC (ASP manages)
    int  slot_index;        // Index in shm->players[] or shm->npcs[]
                            // Stored here so threads can find their own slot

    // ── Lifecycle ─────────────────────────────────────────────────────────
    int  is_alive;          // STATUS_ALIVE (1) or STATUS_DEAD (0)
    int  is_stunned;        // STATUS_STUNNED (1) — stamina tick skips stunned entities
                            // STATUS_NOT_STUNNED (0) — normal accumulation

    // ── OS IDs (for signal delivery) ─────────────────────────────────────
    // Arbiter uses these to send SIGUSR1 (stun), SIGSTOP/SIGCONT (ultimate)
    // to the correct process. Set at startup by HIP/ASP.
    // pid_t is 32-bit on Linux — compatible with int, but use pid_t for clarity
    int  managing_pid;      // PID of the process managing this entity:
                            //   player entities → HIP's pid
                            //   NPC entities    → ASP's pid
    int  thread_slot;       // Which thread in HIP/ASP manages this entity
                            // Used for debug; actual thread mgmt is per-process

    // ── Combat Statistics ─────────────────────────────────────────────────
    int   hp;               // Current hit points
    int   max_hp;           // Maximum hit points (generated at init with roll seed)
    float stamina;          // Current stamina — float for fractional accumulation
                            // Each tick: stamina += speed * TICK_SECONDS (0.1)
                            // Range: 0.0 to max_stamina
    int   max_stamina;      // 100 for players, 150 for NPCs (spec Section 10)
    int   speed;            // Stamina gained per second
                            // Player: 100 / num_players
                            // NPC:    random 10-30
    int   base_damage;      // Damage for basic Strike/Exhaust attacks
                            // Player: (last digit of roll number) + 10
                            // NPC:    (second-to-last digit of roll) + 10

    // ── Primary Inventory (20 Slots) ─────────────────────────────────────
    // Linear array of 20 integers. Each holds WEAPON_NONE(-1) or a weapon_id.
    // A weapon of size N fills N consecutive slots with the SAME weapon_id value.
    //
    // Example — Iron Halberd (7 slots) placed at slot 3:
    //   inventory[0..2]  = WEAPON_NONE  (-1)
    //   inventory[3..9]  = WEAPON_IRON_HALBERD (2)
    //   inventory[10..19] = WEAPON_NONE (-1)
    //
    // To find weapons: scan left-to-right, detect weapon_id transitions.
    // A weapon START is any slot where inventory[i] != WEAPON_NONE
    //   AND (i == 0 OR inventory[i-1] != inventory[i])
    int  inventory[INVENTORY_SLOTS];

    // ── Long-Term Weapon Storage ──────────────────────────────────────────
    // When primary inventory is full and a new weapon must be placed,
    // existing weapons are swapped OUT here (minimum necessary eviction).
    // Players retrieve via ACTION_SWAP_IN (costs a full turn).
    // Each entry is a weapon_id (WEAPON_NONE = empty slot in this array).
    // lt_storage_count tracks how many weapons are currently stored.
    int  lt_storage[MAX_LT_STORAGE];
    int  lt_storage_count;          // 0 to MAX_LT_STORAGE

    // ── Artifact Flags (Quick Access) ────────────────────────────────────
    // Redundant with scanning inventory[] but critical for fast checks:
    //   Ultimate Ability eligibility check (needs both simultaneously)
    //   Deadlock detection (which artifacts does this entity hold)
    // MUST be kept in sync with inventory[] at all times by the allocator.
    int  holds_solar_core;          // 1 if WEAPON_SOLAR_CORE is in inventory[]
    int  holds_lunar_blade;         // 1 if WEAPON_LUNAR_BLADE is in inventory[]

    // ── Deadlock Tracking ────────────────────────────────────────────────
    // Set by Arbiter when entity is waiting for an artifact that's held
    // by someone else. Used by the deadlock monitor background thread
    // to build the wait-for graph.
    // WEAPON_NONE = this entity is not waiting for any artifact.
    int  waiting_for_resource;      // weapon_id of artifact being waited for

    // ── Turn / Action Buffer ─────────────────────────────────────────────
    // HIP writes the player's chosen action here; ASP writes NPC's decision here.
    // Arbiter reads these fields AFTER action_submitted semaphore is posted.
    // Arbiter resets action_ready = 0 at the START of each dispatch.
    int  action_ready;              // 0 = no pending action, 1 = action written
    int  action_type;               // One of ACTION_* constants above
    int  action_target_id;          // ID of the target entity
                                    // For player attacking NPC: NPC's entity id
                                    // For NPC attacking player: player's slot index
    int  action_weapon_slot;        // For ACTION_USE_WEAPON: which inventory slot (0-19)
                                    // The slot value is the START slot of the weapon
    int  action_lt_weapon_id;       // For ACTION_SWAP_IN: weapon_id to retrieve

    // ── Swap-In Guard ────────────────────────────────────────────────────
    // After a Swap In action, the weapon cannot be used until the NEXT turn.
    // Arbiter sets this to the weapon_id that was swapped in this turn.
    // HIP checks this before showing "Use Weapon" menu options.
    // Reset to WEAPON_NONE at start of each turn.
    int  just_swapped_in_weapon;    // WEAPON_NONE = no restriction this turn

} Entity;

// ============================================================================
// SECTION 7 — GLOBAL RESOURCE TABLE
// Tracks which entity holds each exclusive artifact.
// ONLY accessed via acquire_artifact() / release_artifact() functions.
// Table mutex MUST be locked before ANY read or write to this struct.
// ============================================================================

typedef struct {

    // Holder entity IDs for each artifact.
    // -1 = free (no one holds it or it hasn't appeared yet)
    // ≥0 = entity ID of the holder (matches Entity::id)
    int  solar_core_holder;         // -1 = free | entity_id = held
    int  lunar_blade_holder;        // -1 = free | entity_id = held

    // Eclipse Relic — dynamically introduced at runtime.
    // The Arbiter introduces it randomly during gameplay.
    // Once introduced, follows same exclusivity rules as Solar/Lunar.
    int  eclipse_relic_in_game;     // 0 = not yet introduced, 1 = active in world
    int  eclipse_relic_holder;      // -1 = free on battlefield, entity_id = held

    // Protects ALL fields in this struct.
    // Lock this BEFORE reading or writing any holder field.
    // Release IMMEDIATELY after the read/write completes.
    // MUST be initialized with PTHREAD_PROCESS_SHARED.
    pthread_mutex_t table_mutex;

} GlobalResourceTable;

// ============================================================================
// SECTION 8 — TURN STATE
// Controls the scheduling handshake between Arbiter and HIP/ASP.
//
// Turn flow:
//   1. Arbiter detects entity X has full stamina
//   2. Arbiter writes current_entity_id, current_entity_is_player
//   3. Arbiter posts turn_notification semaphore
//   4. HIP or ASP wakes, checks if it's their entity's turn
//   5. If yes: reads input / makes AI decision, writes to Entity action fields
//   6. HIP/ASP posts action_submitted semaphore
//   7. Arbiter wakes, reads action, applies it to game state
//   8. Arbiter resets stamina, clears action_in_progress, continues loop
// ============================================================================

typedef struct {

    // Set by Arbiter before posting turn_notification.
    // HIP and ASP threads check these to determine if IT IS THEIR TURN.
    int  current_entity_id;         // Entity::id of the entity whose turn it is
    int  current_entity_is_player;  // 1 = it's a player's turn (HIP handles)
                                    // 0 = it's an NPC's turn (ASP handles)
    int  current_entity_slot;       // shm->players[slot] or shm->npcs[slot]
                                    // Pre-computed by Arbiter for convenience

    // Flags for the Arbiter's internal state machine
    int  action_in_progress;        // 1 = Arbiter is currently processing a turn
                                    // Stamina tick thread checks this and PAUSES
                                    // accumulation while 1 (action is atomic)
    int  waiting_for_input;         // 1 = Arbiter posted turn_notification and is
                                    // waiting for action_submitted

    // ── Semaphores for turn handshake ─────────────────────────────────────
    // These MUST be initialized with pshared=1 (cross-process).
    // Initial value = 0 for both (Arbiter controls when to wake others).

    // Arbiter posts this → HIP or ASP wakes to take its turn
    sem_t turn_notification;

    // HIP or ASP posts this → Arbiter wakes to apply the action
    sem_t action_submitted;

} TurnState;

// ============================================================================
// SECTION 9 — MASTER SHARED GAME STATE
// This is THE struct. All three processes map this exact struct from
// the POSIX shared memory segment /chrono_rift_shm.
//
// Memory layout is fixed at compile time — sizeof(SharedGameState) bytes
// are allocated via ftruncate() by the Arbiter and mmap()'d by all.
//
// Size estimate (rough):
//   Entity = ~400 bytes × (4 players + 9 NPCs) = ~5.2 KB
//   Action log = 30 × 200 = 6 KB
//   Sync primitives = ~500 bytes
//   Total: well under 64 KB — well within Linux shared memory limits
// ============================================================================

typedef struct {

    // ── Master Synchronization Primitive ────────────────────────────────
    // Held when reading/writing ANY entity stats, game state fields,
    // or turn state fields. The one exception is the action log which
    // has its own log_mutex to avoid blocking the rendering thread.
    //
    // MUST be initialized with PTHREAD_PROCESS_SHARED.
    // Pattern: lock → read/write → unlock. Never hold across sleep/wait.
    pthread_mutex_t master_mutex;

    // ── Game Configuration (set at init, never changes during play) ──────
    int     num_players;            // 1-4 (player chose at startup prompt)
    int     num_npcs_concurrent;    // 2-9 (random per run with roll seed)

    // ── Entity Arrays ─────────────────────────────────────────────────────
    Entity  players[MAX_PLAYERS];   // Indexed 0..(num_players-1)
    Entity  npcs[MAX_NPCS];         // Indexed 0..(num_npcs_concurrent-1)
                                    // When an NPC dies and respawns, its slot
                                    // is reused with a new entity (new id,
                                    // new stats, new thread in ASP)

    // ── Kill / Spawn Counters ─────────────────────────────────────────────
    int     total_kills;            // Cumulative kills toward win condition (10)
    int     total_npcs_spawned;     // Ever spawned (used to assign unique NPC IDs)
                                    // Starts at num_npcs_concurrent, increments
                                    // with each respawn

    // ── Turn State ────────────────────────────────────────────────────────
    TurnState turn;

    // ── Global Game Flags ─────────────────────────────────────────────────
    int     game_result;            // GAME_ONGOING / WIN / LOSE / QUIT
    int     game_started;           // 0 = init phase, 1 = game loop active
                                    // HIP and ASP spin-wait on this before doing anything
    int     ultimate_active;        // 1 = Ultimate Ability suspension is in effect
                                    // Stamina tick thread pauses ALL ticks while 1

    // ── Artifact Resource Table ───────────────────────────────────────────
    GlobalResourceTable resources;

    // ── Process IDs ───────────────────────────────────────────────────────
    // Written by each process at startup.
    // Arbiter uses hip_pid / asp_pid for kill() (stun, ultimate, shutdown).
    // HIP uses arbiter_pid to send SIGTERM on quit.
    pid_t   arbiter_pid;
    pid_t   hip_pid;
    pid_t   asp_pid;

    // ── Action Log (Circular Buffer) ──────────────────────────────────────
    // The rendering thread reads this; game logic writes via append_action_log().
    // Uses its own mutex (log_mutex) so rendering never blocks the game loop.
    //
    // Circular buffer mechanics:
    //   log_head = index of the OLDEST valid entry (next to be overwritten)
    //   log_count = number of valid entries (0 to ACTION_LOG_LINES)
    //   New entry slot = (log_head + log_count) % ACTION_LOG_LINES
    //   When full: advance log_head (overwrite oldest), log_count stays at max
    char    action_log[ACTION_LOG_LINES][ACTION_LOG_WIDTH];
    int     log_head;
    int     log_count;
    pthread_mutex_t log_mutex;      // PTHREAD_PROCESS_SHARED — separate from master

    // ── Weapon Drop State ─────────────────────────────────────────────────
    // When an NPC dies and a weapon drop is triggered:
    //   dropped_weapon_id = the weapon that dropped (random from world, NOT NPC's held weapon)
    //   drop_awaiting_player_choice = 1
    //
    // HIP detects drop_awaiting_player_choice == 1 at the start of any player turn
    // and prompts the player to pick it up or decline.
    //
    // If player declines: Arbiter gives weapon to a random alive NPC.
    // After resolution: both fields reset.
    //
    // NOTE: Only one drop can be pending at a time (simplification — spec
    // doesn't specify simultaneous drops, and only one NPC dies per turn).
    int     dropped_weapon_id;          // WEAPON_NONE = no drop pending
    int     drop_awaiting_player_choice; // 0 = no pending drop, 1 = waiting

    // ── Eclipse Relic Spawn Timer ─────────────────────────────────────────
    // Arbiter introduces the Eclipse Relic randomly during gameplay.
    // This counter tracks kills since last check — Arbiter rolls to introduce
    // the relic after every 3rd kill if it hasn't appeared yet.
    int     eclipse_spawn_checked;  // 1 = Arbiter already introduced relic (or rolled)

} SharedGameState;