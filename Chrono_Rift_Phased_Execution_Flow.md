# Chrono Rift — Phased Implementation Execution Flow
## CS 2006 Operating Systems | Spring 2026 | Complete Build Guide (18 Phases)

---

> **This document breaks the entire Chrono Rift project into 18 sequential implementation phases. Each phase contains a full description of what to build, exactly how to build it, all implementation details, the data structures involved, the code patterns to follow, how each component connects to the rest of the system, what to test before moving to the next phase, and which rubric marks it covers. Read and complete each phase in order. Do not skip phases.**

---

## Master Phase Overview

| Phase | Name | Marks Covered | Estimated Effort |
|---|---|---|---|
| Phase 1 | Project Scaffold & Docker Setup | Environment (20) | 2–3 hours |
| Phase 2 | Shared Memory Design & Layout | IPC (10), Sync (40) | 4–5 hours |
| Phase 3 | Shared Memory Initialization & Cross-Process Sync | IPC (10), Sync (40) | 3–4 hours |
| Phase 4 | Arbiter Process — Skeleton & Main Loop | Process Arch (40) | 3–4 hours |
| Phase 5 | Human Interfacing Process — Multi-Threaded Input | Threading (40) | 4–5 hours |
| Phase 6 | Automated Strategic Process — NPC Thread Pool | Threading (40) | 4–5 hours |
| Phase 7 | Stamina Accumulation Engine & Turn Scheduler | Scheduling (40) | 5–6 hours |
| Phase 8 | Player Action Execution (All 6 Actions) | Gameplay (40) | 4–5 hours |
| Phase 9 | NPC Action Execution & Timeout Logic | Gameplay (40) | 3–4 hours |
| Phase 10 | Signal Infrastructure — SIGUSR1 Stun Mechanic | Signals (40) | 4–5 hours |
| Phase 11 | Signal Infrastructure — Ultimate Ability (SIGSTOP/SIGCONT/SIGALRM) | Signals (40) | 4–5 hours |
| Phase 12 | Inventory Allocator — Contiguous Memory Management | Memory (40) | 5–6 hours |
| Phase 13 | Weapon Drop, Pickup & Long-Term Storage | Memory (40), Gameplay (40) | 3–4 hours |
| Phase 14 | Global Resource Table & Artifact Locking | Deadlock (40) | 3–4 hours |
| Phase 15 | Deadlock Detection & Resolution Thread | Deadlock (40) | 4–5 hours |
| Phase 16 | Rendering Thread — Real-Time TUI/GUI | Rendering (30) | 5–6 hours |
| Phase 17 | Game Conditions, Lifecycle & Graceful Shutdown | Gameplay (40), Process (40) | 3–4 hours |
| Phase 18 | Integration Testing, Report & Submission | All sections | 4–6 hours |

**Total Core Marks: 360 | Bonus (Multiplayer): 30**

---

## Phase 1 — Project Scaffold & Docker Setup

### 1.1 What This Phase Accomplishes

This phase sets up the entire project skeleton: the Docker environment, the exact folder structure the teacher requires, the Makefile, and three skeleton `.cpp` files that compile but do nothing yet. After this phase, running `docker build` and `make` should produce three executables (`arbiter`, `hip`, `asp`) without errors. This is the foundation everything else is built upon.

**Marks covered:** Environment, Build & Constraints (20 marks)

### 1.2 Why This Phase First

The Docker environment is Ubuntu 22.04. If you code outside Docker first and then try to port, you will encounter shared memory, signal, and pthread inconsistencies between your OS and Linux. Building inside Docker from day one means your code always compiles in the grading environment. Additionally, the folder structure is a graded constraint — wrong folder names cost 20% deduction.

### 1.3 Folder Structure to Create

Create this exact structure on your machine:

```
submission/
├── Dockerfile
├── Makefile
├── requirements.txt
├── arbiter/
│   ├── arbiter.cpp
│   ├── shared_types.h        ← shared across all three processes
│   └── utils.h               ← optional helper declarations
├── hip/
│   ├── hip.cpp
│   └── shared_types.h        ← symlink or copy from arbiter/
└── asp/
    ├── asp.cpp
    └── shared_types.h        ← symlink or copy from arbiter/
report.pdf                    ← added last before submission
```

> **Important:** `shared_types.h` defines all the structs that live in shared memory. All three processes must use the exact same struct definitions. The safest approach is to keep one master copy and either symlink it or copy it during build via the Makefile.

### 1.4 The Dockerfile

Copy this file exactly into `submission/Dockerfile`. Do **not** rename it, do **not** add an extension, do **not** change any contents:

```dockerfile
FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    gdb \
    libsfml-dev \
    libsdl2-dev \
    libglfw3-dev \
    libncurses-dev \
    && rm -rf /var/lib/apt/lists/*

COPY requirements.txt /tmp/requirements.txt
RUN grep -v '^#' /tmp/requirements.txt | grep -v '^$' | \
    xargs -r apt-get install -y && rm -rf /var/lib/apt/lists/*

WORKDIR /app
CMD ["bash"]
```

### 1.5 The requirements.txt

```
# requirements.txt
# Leave blank unless you need extra apt packages beyond:
# build-essential, cmake, gdb, libsfml-dev, libsdl2-dev, libglfw3-dev, libncurses-dev
```

If you use ncurses (recommended), it is already included — leave this file with only comments.

### 1.6 The Makefile

```makefile
CXX      = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -pthread

# Uncomment EXACTLY ONE line that matches your GUI choice:
# LIBS = -lsfml-graphics -lsfml-window -lsfml-audio -lsfml-network -lsfml-system -lrt
# LIBS = $(shell sdl2-config --libs) -lrt
# LIBS = -lglfw -lGL -lrt
LIBS = -lncurses -lrt

TARGETS = arbiter hip asp

all: clean $(TARGETS)
	@echo "Build complete."

arbiter: arbiter/arbiter.cpp
	$(CXX) $(CXXFLAGS) arbiter/*.cpp -o $@ $(LIBS)

hip: hip/hip.cpp
	$(CXX) $(CXXFLAGS) hip/*.cpp -o $@ $(LIBS)

asp: asp/asp.cpp
	$(CXX) $(CXXFLAGS) asp/*.cpp -o $@ $(LIBS)

clean:
	rm -f $(TARGETS)

.PHONY: all clean
```

> **CRITICAL TAB WARNING:** The lines starting with `$(CXX)` and `rm -f` and `@echo` MUST be indented with a **real tab character** (press Tab on keyboard), NOT spaces. If you use spaces, `make` will throw `missing separator` and nothing will compile. If you paste from a text editor, verify the indentation character.

### 1.7 Skeleton Source Files

**arbiter/arbiter.cpp** — skeleton only, will grow every phase:

```cpp
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include "shared_types.h"

int main(int argc, char* argv[]) {
    std::cout << "[ARBITER] Starting up..." << std::endl;
    // Phase-by-phase implementation goes here
    std::cout << "[ARBITER] Shutting down." << std::endl;
    return 0;
}
```

**hip/hip.cpp** — skeleton:

```cpp
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include "shared_types.h"

int main(int argc, char* argv[]) {
    std::cout << "[HIP] Starting up..." << std::endl;
    std::cout << "[HIP] Shutting down." << std::endl;
    return 0;
}
```

**asp/asp.cpp** — skeleton:

```cpp
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include "shared_types.h"

int main(int argc, char* argv[]) {
    std::cout << "[ASP] Starting up..." << std::endl;
    std::cout << "[ASP] Shutting down." << std::endl;
    return 0;
}
```

**arbiter/shared_types.h** — empty shell for now (will be filled in Phase 2):

```cpp
#pragma once
// shared_types.h — All structs that live in shared memory
// Included by arbiter, hip, and asp
```

### 1.8 Building & Testing This Phase

```bash
# From inside your submission folder on the host machine:
docker build -t chrono-rift-env .
# Wait 2-5 minutes

# Run the container with your source mounted:
docker run -it --rm -v $(pwd):/app chrono-rift-env

# Inside the container:
make
# Expected output:
# g++ -Wall -Wextra -std=c++17 -pthread arbiter/*.cpp -o arbiter -lncurses -lrt
# g++ -Wall -Wextra -std=c++17 -pthread hip/*.cpp -o hip -lncurses -lrt
# g++ -Wall -Wextra -std=c++17 -pthread asp/*.cpp -o asp -lncurses -lrt
# Build complete.

ls -la arbiter hip asp
# Should show three executables

./arbiter
# [ARBITER] Starting up...
# [ARBITER] Shutting down.

./hip
# [HIP] Starting up...
# [HIP] Shutting down.

./asp
# [ASP] Starting up...
# [ASP] Shutting down.
```

**Phase 1 is complete when:** All three executables compile without warnings inside Docker, and each one prints its startup/shutdown message.

---

## Phase 2 — Shared Memory Design & Layout

### 2.1 What This Phase Accomplishes

This is the most architecturally critical phase of the entire project. You design and declare every single struct that will live in shared memory. Every piece of game state — entity stats, turn management, action buffers, artifact ownership, inventory state, action log — must be designed right here. Getting this wrong means rewriting everything downstream. This phase produces only `shared_types.h` but it defines the entire game's data model.

**Marks covered:** IPC (Correct Use of Shared Memory — 10), Synchronization (all 40), process communication design (10)

### 2.2 Why Shared Memory Design Is Hard

Shared memory has strict constraints that normal heap or stack data doesn't:
- **No pointers.** Shared memory is mapped at different virtual addresses in each process. A pointer valid in the Arbiter will be garbage in HIP. Every array must be fixed-size, every "reference" must be an index into a fixed array.
- **Synchronization primitives must live inside the shared memory block** with `PTHREAD_PROCESS_SHARED` attribute. You cannot have a mutex in Arbiter's stack and use it from HIP.
- **Fixed maximum sizes.** Since there are no pointers, arrays must have compile-time maximum sizes. Design conservatively (e.g., `MAX_NPCS = 9`, `MAX_PLAYERS = 4`).
- **Alignment.** The struct fields should be aligned properly. Using `int` (4 bytes) and `char` arrays with appropriate sizes avoids most alignment issues.

### 2.3 Entity Structure

Each entity (player or NPC) has the following attributes. Both player characters and NPCs use the same struct — a flag differentiates them:

```cpp
// In shared_types.h

#pragma once
#include <pthread.h>
#include <semaphore.h>

// ── Constants ──────────────────────────────────────────────────────────────
#define MAX_PLAYERS         4
#define MAX_NPCS            9
#define INVENTORY_SLOTS     20
#define MAX_LT_STORAGE      50     // long-term storage slots per player
#define ACTION_LOG_LINES    30
#define ACTION_LOG_WIDTH    200
#define SHM_NAME            "/chrono_rift_shm"

// ── Weapon IDs ─────────────────────────────────────────────────────────────
// These are the canonical weapon IDs used throughout the system
#define WEAPON_NONE         -1
#define WEAPON_SOLAR_CORE    0     // 10 slots, 95 dmg
#define WEAPON_LUNAR_BLADE   1     // 10 slots, 90 dmg
#define WEAPON_IRON_HALBERD  2     //  7 slots, 55 dmg
#define WEAPON_VENOM_DAGGER  3     //  4 slots, 30 dmg
#define WEAPON_THUNDERSTAFF  4     //  6 slots, 50 dmg
#define WEAPON_OBSIDIAN_AXE  5     //  5 slots, 45 dmg
#define WEAPON_FROSTBOW      6     //  6 slots, 48 dmg
#define WEAPON_SPLINTER_STICK 7    //  2 slots, 12 dmg
#define NUM_WEAPON_TYPES     8

// ── Weapon Metadata (read-only, not in shared mem — embed in code) ──────────
// Access via: weapon_slot_size[weapon_id], weapon_damage[weapon_id]
// These arrays will be defined in a .cpp file and declared extern here
extern const int weapon_slot_size[NUM_WEAPON_TYPES];
extern const int weapon_damage[NUM_WEAPON_TYPES];
extern const char* weapon_name[NUM_WEAPON_TYPES];

// ── Action Types ───────────────────────────────────────────────────────────
#define ACTION_NONE           0
#define ACTION_ATTACK_STRIKE  1    // reduce target HP by damage stat
#define ACTION_ATTACK_EXHAUST 2    // reduce target Stamina by damage stat
#define ACTION_USE_WEAPON     3    // reduce target HP by weapon damage
#define ACTION_SWAP_IN        4    // retrieve weapon from long-term storage
#define ACTION_HEAL           5    // restore 10% of own HP
#define ACTION_SKIP           6    // stamina → 50%

// ── Entity Status Flags ────────────────────────────────────────────────────
#define STATUS_ALIVE          1
#define STATUS_DEAD           0
#define STATUS_STUNNED        1
#define STATUS_NOT_STUNNED    0

// ── Game Result ───────────────────────────────────────────────────────────
#define GAME_ONGOING          0
#define GAME_WIN              1    // 10 enemies killed
#define GAME_LOSE             2    // all players dead
#define GAME_QUIT             3    // player sent SIGTERM

// ─────────────────────────────────────────────────────────────────────────
// Entity: represents a single player character or NPC
// ─────────────────────────────────────────────────────────────────────────
typedef struct {
    int  id;                        // unique entity ID (0-based per type)
    int  is_player;                 // 1 = player character, 0 = NPC
    int  is_alive;                  // STATUS_ALIVE or STATUS_DEAD
    int  is_stunned;                // STATUS_STUNNED or STATUS_NOT_STUNNED
    int  pid;                       // OS process ID of the process managing this entity
    int  thread_index;              // index of thread managing this entity (within HIP or ASP)

    // Combat stats
    int  hp;
    int  max_hp;
    float stamina;                  // float for fractional accumulation
    int  max_stamina;               // 100 for player, 150 for NPC
    int  speed;                     // 100/num_players for player, random 10-30 for NPC
    int  base_damage;               // last digit of roll + 10 (player), 2nd last + 10 (NPC)

    // Inventory (primary — 20 slots)
    // Each slot holds WEAPON_NONE (-1) if empty, or the weapon_id of the weapon occupying it.
    // A weapon occupying N slots fills N consecutive slots with the same weapon_id.
    // Example: Iron Halberd at slot 3 → inventory[3..9] = WEAPON_IRON_HALBERD
    int  inventory[INVENTORY_SLOTS];

    // Long-term weapon storage (weapons swapped out of primary inventory)
    // Each entry is a weapon_id. WEAPON_NONE means empty slot in this array.
    int  lt_storage[MAX_LT_STORAGE];
    int  lt_storage_count;         // number of weapons currently in long-term storage

    // Artifact holding flags (separate from inventory for quick checks)
    int  holds_solar_core;         // 1 if Solar Core is in primary inventory
    int  holds_lunar_blade;        // 1 if Lunar Blade is in primary inventory

    // Deadlock tracking: what resource is this entity currently waiting for?
    // WEAPON_NONE = not waiting for anything
    int  waiting_for_resource;     // weapon_id of the artifact being waited for

    // Turn state
    int  action_ready;             // 1 = entity has submitted an action, 0 = still deciding
    int  action_type;              // one of ACTION_* constants
    int  action_target_id;         // entity ID of the target
    int  action_weapon_slot;       // which inventory slot to use (for ACTION_USE_WEAPON)
    int  action_lt_weapon_id;      // weapon_id to swap in (for ACTION_SWAP_IN)

} Entity;

// ─────────────────────────────────────────────────────────────────────────
// GlobalResourceTable: tracks artifact ownership for deadlock detection
// ─────────────────────────────────────────────────────────────────────────
typedef struct {
    // -1 = free (no holder), otherwise = entity ID of the holder
    int  solar_core_holder;
    int  lunar_blade_holder;

    // Eclipse Relic (dynamic — introduced at runtime)
    int  eclipse_relic_in_game;    // 0 = not yet appeared, 1 = active in game
    int  eclipse_relic_holder;     // -1 = free (lying on battlefield), entity_id = held

    // Mutex to protect the entire table during reads/writes
    // MUST be initialized with PTHREAD_PROCESS_SHARED
    pthread_mutex_t table_mutex;

} GlobalResourceTable;

// ─────────────────────────────────────────────────────────────────────────
// TurnState: tracks whose turn it is and scheduling control
// ─────────────────────────────────────────────────────────────────────────
typedef struct {
    int  current_entity_id;        // ID of entity currently taking their turn
    int  current_entity_is_player; // 1 = player, 0 = NPC
    int  action_in_progress;       // 1 = action is being processed by Arbiter
    int  waiting_for_input;        // 1 = Arbiter is waiting for HIP/ASP to submit action
    int  npc_turn_timeout;         // set to 1 by Arbiter timer if NPC takes >3 seconds

    // Arbiter signals HIP/ASP whose turn it is via this field + semaphore below
    // HIP reads this and activates the correct player thread
    // ASP reads this and activates the correct NPC thread
    sem_t turn_notification;       // Arbiter posts this when a turn begins

    // HIP/ASP post this when action is submitted
    sem_t action_submitted;

} TurnState;

// ─────────────────────────────────────────────────────────────────────────
// SharedGameState: the master shared memory structure
// All three processes map this exact struct from /chrono_rift_shm
// ─────────────────────────────────────────────────────────────────────────
typedef struct {

    // ── Global synchronization ──────────────────────────────────────────
    // Master mutex: must be held when reading/writing any entity stats
    // Initialized with PTHREAD_PROCESS_SHARED so all processes can use it
    pthread_mutex_t master_mutex;

    // ── Entity data ─────────────────────────────────────────────────────
    int     num_players;                    // 1–4, set at game start
    int     num_npcs_concurrent;            // 2–9, random per run
    int     total_kills;                    // cumulative kills toward win (target: 10)
    int     total_npcs_spawned;             // total ever spawned (to assign unique IDs)

    Entity  players[MAX_PLAYERS];
    Entity  npcs[MAX_NPCS];                 // only num_npcs_concurrent are active at once

    // ── Turn management ─────────────────────────────────────────────────
    TurnState turn;

    // ── Game state ──────────────────────────────────────────────────────
    int     game_result;                    // GAME_ONGOING / WIN / LOSE / QUIT
    int     game_started;                   // set to 1 once Arbiter finishes init
    int     ultimate_active;               // 1 if Ultimate Ability suspension is active

    // ── Resource table ──────────────────────────────────────────────────
    GlobalResourceTable resources;

    // ── Process IDs (set by each process at startup) ────────────────────
    pid_t   arbiter_pid;
    pid_t   hip_pid;
    pid_t   asp_pid;

    // ── Action log (for rendering thread) ───────────────────────────────
    char    action_log[ACTION_LOG_LINES][ACTION_LOG_WIDTH];
    int     log_head;                       // index of oldest entry (circular buffer)
    int     log_count;                      // number of valid entries (up to LOG_LINES)
    pthread_mutex_t log_mutex;             // protect action log separately for fast UI reads

    // ── Weapon drop state ────────────────────────────────────────────────
    // When an enemy dies, a dropped weapon appears here
    int     dropped_weapon_id;             // WEAPON_NONE if no weapon currently dropped
    int     drop_awaiting_player_choice;   // 1 = player must decide; 0 = no pending drop

} SharedGameState;
```

### 2.4 Why Each Design Decision Was Made

**Float stamina:** Stamina accumulates in real-time. Speed is added every second. With integer stamina and speed, fractional accumulation is lost. Using `float` preserves the exact arrival time formula `Arrival = MaxStamina / Speed`.

**WEAPON_NONE = -1:** The inventory is an array where each slot holds either -1 (empty) or a weapon_id. A weapon occupying N slots fills N consecutive indices with the same weapon_id. This makes the contiguous search trivial: scan for N consecutive -1 slots.

**Separate `lt_storage` inside Entity:** No pointers allowed. Fixed-size array of `MAX_LT_STORAGE = 50` weapon IDs in long-term storage. This is per-player — each player manages their own long-term storage independently.

**Semaphores in TurnState:** `turn_notification` (Arbiter posts → HIP/ASP wakes) and `action_submitted` (HIP/ASP posts → Arbiter wakes) form the turn handshake. This is non-blocking and signal-safe.

**Circular action log:** The UI rendering thread reads the action log and displays the last N actions. A circular buffer with `log_head` and `log_count` allows O(1) insertion and avoids memory allocation.

### 2.5 Weapon Metadata (Non-Shared, Compile-Time Constants)

Add this file as `arbiter/weapon_data.cpp` and a corresponding `weapon_data.h`. All three processes include `weapon_data.h`:

```cpp
// weapon_data.cpp — compile into all three executables
#include "shared_types.h"

const int weapon_slot_size[NUM_WEAPON_TYPES] = {
    10,  // WEAPON_SOLAR_CORE
    10,  // WEAPON_LUNAR_BLADE
     7,  // WEAPON_IRON_HALBERD
     4,  // WEAPON_VENOM_DAGGER
     6,  // WEAPON_THUNDERSTAFF
     5,  // WEAPON_OBSIDIAN_AXE
     6,  // WEAPON_FROSTBOW
     2   // WEAPON_SPLINTER_STICK
};

const int weapon_damage[NUM_WEAPON_TYPES] = {
    95, 90, 55, 30, 50, 45, 48, 12
};

const char* weapon_name[NUM_WEAPON_TYPES] = {
    "Solar Core", "Lunar Blade", "Iron Halberd",
    "Venom Dagger", "Thunderstaff", "Obsidian Axe",
    "Frostbow", "Splinter Stick"
};
```

### 2.6 Phase 2 Testing

There is nothing to run yet — this phase is pure design. Review checklist:
- [ ] All arrays are fixed-size (no pointers in the shared struct)
- [ ] `pthread_mutex_t` fields are present where needed
- [ ] `sem_t` fields are present for turn signaling
- [ ] All entities (players + NPCs) fit within the fixed arrays
- [ ] `shared_types.h` compiles with `#include` in all three skeleton `.cpp` files without errors

**Phase 2 is complete when:** `make` succeeds with `shared_types.h` included in all three processes and no compilation errors.

---

## Phase 3 — Shared Memory Initialization & Cross-Process Synchronization

### 3.1 What This Phase Accomplishes

This phase implements the mechanics of creating and mapping shared memory, and critically, initializing all synchronization primitives with `PTHREAD_PROCESS_SHARED`. You will write the functions that the Arbiter calls to set up shared memory, and that HIP and ASP call to attach to it. After this phase, all three processes can read and write from the same `SharedGameState` struct simultaneously.

**Marks covered:** Correct Use of Shared Memory (10), Mutex/Semaphore Implementation (10), Race Condition Prevention (10)

### 3.2 How POSIX Shared Memory Works

```
Arbiter:                          HIP / ASP:
shm_open(SHM_NAME, O_CREAT|O_RDWR, 0666)   shm_open(SHM_NAME, O_RDWR, 0666)
ftruncate(fd, sizeof(SharedGameState))       (size already set by Arbiter)
mmap(NULL, sizeof(SharedGameState),          mmap(NULL, sizeof(SharedGameState),
     PROT_READ|PROT_WRITE,                        PROT_READ|PROT_WRITE,
     MAP_SHARED, fd, 0)                           MAP_SHARED, fd, 0)
close(fd)                                    close(fd)
↓                                            ↓
shm → points to the SAME physical memory     shm → points to the SAME physical memory
```

Both `mmap` calls return different virtual addresses, but both map to the same physical memory page. This is why pointers don't work — `shm` in Arbiter is at virtual address `0x7f1234000000` but `shm` in HIP might be at `0x7f5678000000`. A pointer stored inside the struct by Arbiter would be meaningless in HIP. All references must use array indices.

### 3.3 Arbiter's Shared Memory Setup Function

Create `arbiter/shm_manager.cpp`:

```cpp
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <cstdlib>
#include "shared_types.h"

SharedGameState* create_shared_memory() {
    // ── Step 1: Clean up any stale shared memory from a previous crashed run ──
    shm_unlink(SHM_NAME);   // ignore error if it doesn't exist

    // ── Step 2: Create the shared memory object ──────────────────────────────
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("[ARBITER] shm_open failed");
        exit(EXIT_FAILURE);
    }

    // ── Step 3: Set the size of the shared memory object ────────────────────
    if (ftruncate(fd, sizeof(SharedGameState)) == -1) {
        perror("[ARBITER] ftruncate failed");
        exit(EXIT_FAILURE);
    }

    // ── Step 4: Map the shared memory into the Arbiter's address space ───────
    void* ptr = mmap(NULL, sizeof(SharedGameState),
                     PROT_READ | PROT_WRITE,
                     MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        perror("[ARBITER] mmap failed");
        exit(EXIT_FAILURE);
    }
    close(fd);  // File descriptor no longer needed after mmap

    SharedGameState* shm = (SharedGameState*)ptr;

    // ── Step 5: Zero-initialize everything ──────────────────────────────────
    memset(shm, 0, sizeof(SharedGameState));

    // ── Step 6: Initialize all inventory slots to WEAPON_NONE (-1) ──────────
    for (int p = 0; p < MAX_PLAYERS; p++) {
        for (int s = 0; s < INVENTORY_SLOTS; s++) {
            shm->players[p].inventory[s] = WEAPON_NONE;
        }
        for (int s = 0; s < MAX_LT_STORAGE; s++) {
            shm->players[p].lt_storage[s] = WEAPON_NONE;
        }
    }
    for (int n = 0; n < MAX_NPCS; n++) {
        for (int s = 0; s < INVENTORY_SLOTS; s++) {
            shm->npcs[n].inventory[s] = WEAPON_NONE;
        }
    }

    // ── Step 7: Initialize all synchronization primitives ───────────────────

    // Master mutex — MUST be PTHREAD_PROCESS_SHARED
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shm->master_mutex, &mutex_attr);
    pthread_mutex_init(&shm->log_mutex, &mutex_attr);
    pthread_mutex_init(&shm->resources.table_mutex, &mutex_attr);
    pthread_mutexattr_destroy(&mutex_attr);

    // Semaphores — pshared = 1 means cross-process, initial value = 0
    sem_init(&shm->turn.turn_notification, 1, 0);
    sem_init(&shm->turn.action_submitted,  1, 0);

    // ── Step 8: Initialize game state fields ─────────────────────────────────
    shm->game_result             = GAME_ONGOING;
    shm->game_started            = 0;
    shm->dropped_weapon_id       = WEAPON_NONE;
    shm->drop_awaiting_player_choice = 0;
    shm->resources.solar_core_holder  = -1;
    shm->resources.lunar_blade_holder = -1;
    shm->resources.eclipse_relic_in_game = 0;
    shm->resources.eclipse_relic_holder  = -1;
    shm->log_head  = 0;
    shm->log_count = 0;

    std::cout << "[ARBITER] Shared memory created at " << SHM_NAME
              << " (" << sizeof(SharedGameState) << " bytes)" << std::endl;

    return shm;
}

void destroy_shared_memory(SharedGameState* shm) {
    // Destroy all synchronization primitives
    pthread_mutex_destroy(&shm->master_mutex);
    pthread_mutex_destroy(&shm->log_mutex);
    pthread_mutex_destroy(&shm->resources.table_mutex);
    sem_destroy(&shm->turn.turn_notification);
    sem_destroy(&shm->turn.action_submitted);

    // Unmap
    munmap(shm, sizeof(SharedGameState));

    // Remove the shared memory object
    shm_unlink(SHM_NAME);
    std::cout << "[ARBITER] Shared memory destroyed." << std::endl;
}
```

### 3.4 HIP and ASP's Attachment Function

Create `hip/shm_client.cpp` and a copy/symlink for `asp/shm_client.cpp`:

```cpp
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <cstdlib>
#include "shared_types.h"

SharedGameState* attach_shared_memory() {
    // The Arbiter must have already created the shared memory.
    // HIP and ASP just open and map it (read-write).

    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd == -1) {
        perror("[CLIENT] shm_open failed — is Arbiter running?");
        exit(EXIT_FAILURE);
    }

    void* ptr = mmap(NULL, sizeof(SharedGameState),
                     PROT_READ | PROT_WRITE,
                     MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        perror("[CLIENT] mmap failed");
        exit(EXIT_FAILURE);
    }
    close(fd);

    return (SharedGameState*)ptr;
}

void detach_shared_memory(SharedGameState* shm) {
    munmap(shm, sizeof(SharedGameState));
    // DO NOT shm_unlink here — only the Arbiter unlinks on shutdown
}
```

### 3.5 Startup Sequence & Timing

The three processes must start in the right order. The Arbiter creates shared memory; HIP and ASP attach to it. If HIP starts before Arbiter, `shm_open` fails. Solutions:

**Option A (Simple — run scripts):** Create a `run.sh` in your submission folder:

```bash
#!/bin/bash
# Start Arbiter first, give it time to initialize shared memory
./arbiter &
sleep 0.5    # Wait 500ms for Arbiter to create shared memory

# Then start HIP and ASP
./hip &
./asp &

# Wait for all background jobs
wait
```

**Option B (Robust — retry loop in HIP/ASP):** In `hip/hip.cpp` and `asp/asp.cpp`, instead of exiting on `shm_open` failure, retry up to 10 times with a 100ms sleep:

```cpp
SharedGameState* attach_shared_memory_with_retry() {
    int fd = -1;
    for (int attempt = 0; attempt < 10 && fd == -1; attempt++) {
        fd = shm_open(SHM_NAME, O_RDWR, 0666);
        if (fd == -1) {
            std::cout << "[CLIENT] Waiting for Arbiter to create shared memory..." << std::endl;
            usleep(100000);  // 100ms
        }
    }
    if (fd == -1) {
        perror("[CLIENT] Could not attach to shared memory after 10 attempts");
        exit(EXIT_FAILURE);
    }
    // ... rest of mmap as before
}
```

### 3.6 Synchronization Pattern for All Data Access

Every time any process reads or writes `SharedGameState` fields (other than action log which has its own mutex), it must follow this pattern:

```cpp
// Reading or writing shared state:
pthread_mutex_lock(&shm->master_mutex);
{
    // ← safe to read/write shm->players[i], shm->npcs[j], etc.
}
pthread_mutex_unlock(&shm->master_mutex);
```

The only exception is when the data being accessed is atomically trivially sized (a single int read), but even then, always use the mutex for correctness under evaluation. The teacher checks for race condition prevention.

### 3.7 Phase 3 Testing

Update `arbiter/arbiter.cpp` main to call `create_shared_memory()`, write a test value, then read it back. Update `hip/hip.cpp` to call `attach_shared_memory_with_retry()` and read the test value:

```cpp
// In arbiter main:
SharedGameState* shm = create_shared_memory();
shm->num_players = 99;  // test value
shm->game_started = 1;
sleep(3);   // keep alive while HIP reads
destroy_shared_memory(shm);

// In hip main:
SharedGameState* shm = attach_shared_memory_with_retry();
while (!shm->game_started) usleep(10000);
std::cout << "[HIP] Read num_players = " << shm->num_players << std::endl;
// Should print: [HIP] Read num_players = 99
```

**Phase 3 is complete when:** Arbiter creates shared memory, HIP attaches and reads data written by Arbiter, all without crashes or memory errors. Mutex init with `PTHREAD_PROCESS_SHARED` must work without assertion failures.

---

## Phase 4 — Arbiter Process: Main Loop, Game Init & Party Selection

### 4.1 What This Phase Accomplishes

The Arbiter is the brain of the entire game. This phase implements its main loop, the game initialization sequence (prompting for party size, generating roll-number-seeded stats for all entities, registering PIDs), and the skeleton of the turn-dispatch loop. After this phase, the Arbiter correctly sets up all entities in shared memory and enters the main game loop (which doesn't do anything useful yet — that comes in Phase 7).

**Marks covered:** Process Architecture (40), Gameplay (Roll Number Seed — 10)

### 4.2 Arbiter Responsibilities Summary

```
Arbiter's jobs (all in one process):
├── 1. Create shared memory and initialize it
├── 2. Prompt user for party size (1–4)
├── 3. Generate entity stats using roll number seed
├── 4. Wait for HIP and ASP to attach and register PIDs
├── 5. Signal game start
├── 6. Run main scheduling loop (Phase 7)
├── 7. Apply all actions committed by HIP/ASP (Phase 8, 9)
├── 8. Send stun signals and Ultimate signals (Phase 10, 11)
├── 9. Check win/lose/quit conditions (Phase 17)
├── 10. Run background deadlock detection thread (Phase 15)
└── 11. Run rendering thread (Phase 16)
```

### 4.3 Roll Number Seed

Your roll number (e.g., `24i-0123`) provides the RNG seed. Extract the numeric portion. In code:

```cpp
// Define your roll number at the top of arbiter.cpp
// Replace with your actual roll number numeric portion
#define ROLL_NUMBER         240123   // example: 24i-0123 → 240123
#define ROLL_LAST_DIGIT     3        // last digit
#define ROLL_SECOND_LAST    2        // second-to-last digit
#define ROLL_LAST_TWO       23       // last two digits

// Seed the RNG once at startup:
srand(ROLL_NUMBER);

// All subsequent rand() calls use this seed for reproducibility
```

### 4.4 Entity Stat Generation

```cpp
void init_player(Entity* p, int player_index, int num_players, int roll_number) {
    p->id             = player_index;
    p->is_player      = 1;
    p->is_alive       = STATUS_ALIVE;
    p->is_stunned     = STATUS_NOT_STUNNED;

    // HP: Roll Number + random(100, 1000)
    int roll_numeric  = roll_number;   // full numeric portion
    p->max_hp         = roll_numeric + (rand() % 901 + 100);  // 100 to 1000
    p->hp             = p->max_hp;

    // Damage: last digit of roll + 10
    p->base_damage    = ROLL_LAST_DIGIT + 10;

    // Speed: 100 / number of player characters
    p->speed          = 100 / num_players;

    // Stamina: starts at 0, max = 100
    p->max_stamina    = 100;
    p->stamina        = 0.0f;

    // Inventory: all empty
    for (int s = 0; s < INVENTORY_SLOTS; s++) p->inventory[s] = WEAPON_NONE;
    for (int s = 0; s < MAX_LT_STORAGE; s++) p->lt_storage[s] = WEAPON_NONE;
    p->lt_storage_count = 0;
    p->holds_solar_core  = 0;
    p->holds_lunar_blade = 0;
    p->waiting_for_resource = WEAPON_NONE;
    p->action_ready   = 0;
}

void init_npc(Entity* n, int npc_index, int total_npcs_ever) {
    n->id             = total_npcs_ever;   // unique ID across all spawns
    n->is_player      = 0;
    n->is_alive       = STATUS_ALIVE;
    n->is_stunned     = STATUS_NOT_STUNNED;

    // HP: last 2 digits of roll + random(50, 200)
    n->max_hp         = ROLL_LAST_TWO + (rand() % 151 + 50);   // 50 to 200
    n->hp             = n->max_hp;

    // Damage: second-to-last digit of roll + 10
    n->base_damage    = ROLL_SECOND_LAST + 10;

    // Speed: random 10 to 30
    n->speed          = rand() % 21 + 10;

    // Stamina: starts at 0, max = 150
    n->max_stamina    = 150;
    n->stamina        = 0.0f;

    // Inventory: all empty (NPCs can hold one weapon max — but we keep same struct)
    for (int s = 0; s < INVENTORY_SLOTS; s++) n->inventory[s] = WEAPON_NONE;
    n->action_ready   = 0;
}
```

### 4.5 Arbiter Main Function Structure

```cpp
int main(int argc, char* argv[]) {
    // 1. Create shared memory
    SharedGameState* shm = create_shared_memory();
    shm->arbiter_pid = getpid();

    // 2. Seed RNG
    srand(ROLL_NUMBER);

    // 3. Prompt for party size
    int num_players = 0;
    while (num_players < 1 || num_players > 4) {
        std::cout << "Enter party size (1-4): ";
        std::cin >> num_players;
    }
    shm->num_players = num_players;

    // 4. Determine concurrent NPC count (random 2-9)
    int num_npcs = rand() % 8 + 2;   // 2 to 9
    shm->num_npcs_concurrent = num_npcs;

    // 5. Initialize all player entities
    for (int i = 0; i < num_players; i++) {
        init_player(&shm->players[i], i, num_players, ROLL_NUMBER);
    }

    // 6. Initialize initial NPCs
    for (int i = 0; i < num_npcs; i++) {
        init_npc(&shm->npcs[i], i, i);
        shm->total_npcs_spawned++;
    }

    // 7. Set up signal handlers (fully implemented in Phases 10-11)
    setup_signal_handlers();

    // 8. Wait for HIP and ASP to register PIDs
    std::cout << "[ARBITER] Waiting for HIP and ASP to connect..." << std::endl;
    while (shm->hip_pid == 0 || shm->asp_pid == 0) {
        usleep(50000);   // 50ms polling
    }
    std::cout << "[ARBITER] HIP (PID " << shm->hip_pid
              << ") and ASP (PID " << shm->asp_pid << ") connected." << std::endl;

    // 9. Launch background threads (implemented in later phases)
    // pthread_create(&deadlock_thread, NULL, deadlock_monitor, shm);
    // pthread_create(&render_thread, NULL, render_loop, shm);

    // 10. Signal game start
    shm->game_started = 1;
    std::cout << "[ARBITER] Game started!" << std::endl;

    // 11. Main scheduling loop (Phase 7)
    main_game_loop(shm);

    // 12. Cleanup
    destroy_shared_memory(shm);
    return 0;
}
```

### 4.6 HIP and ASP PID Registration

In `hip/hip.cpp` main, after attaching to shared memory:

```cpp
SharedGameState* shm = attach_shared_memory_with_retry();
shm->hip_pid = getpid();
std::cout << "[HIP] Registered PID " << shm->hip_pid << std::endl;

// Wait for game to start
while (!shm->game_started) usleep(10000);
std::cout << "[HIP] Game started, spawning player threads..." << std::endl;
```

Similarly in `asp/asp.cpp`:

```cpp
SharedGameState* shm = attach_shared_memory_with_retry();
shm->asp_pid = getpid();
```

**Phase 4 is complete when:** Arbiter initializes all entities with correct stats, HIP and ASP attach and register PIDs, and the Arbiter prints confirmation. Verify entity stats match the roll-number formulas.

---

## Phase 5 — Human Interfacing Process: Multi-Threaded Input

### 5.1 What This Phase Accomplishes

HIP is a multi-threaded process where each player character has its own dedicated thread. At any moment, only the thread corresponding to the currently active player (determined by the Arbiter) should be reading input. All other player threads must block. This phase implements the full HIP thread architecture including the active/idle mechanism.

**Marks covered:** Thread-per-Player Implementation (10), Player Thread Handling Active/Idle (10), Thread Synchronization (10), Efficient Thread Scheduling (10)

### 5.2 Thread Architecture in HIP

```
HIP Process
│
├── Main Thread
│   ├── Attaches to shared memory
│   ├── Registers PID
│   ├── Spawns one pthread per player character
│   └── Joins all threads at shutdown
│
├── Player Thread 0  ← for player character 0
│   ├── Blocks on a per-player semaphore
│   ├── When activated: reads input from stdin
│   ├── Writes action to shm->players[0] fields
│   ├── Posts shm->turn.action_submitted
│   └── Goes back to blocking
│
├── Player Thread 1  ← for player character 1 (if num_players >= 2)
│   └── [same structure as Thread 0]
│
... (up to 4 threads)
```

### 5.3 Per-Player Activation Semaphores

Each player thread needs to know when it's its turn. The Arbiter writes `shm->turn.current_entity_id` and `shm->turn.current_entity_is_player = 1`, then posts `shm->turn.turn_notification`. But ALL player threads wake up on the same semaphore — only the correct one should act.

**Solution:** Each player thread checks whether `shm->turn.current_entity_id == my_player_id` after waking. If not, it re-posts the semaphore (wakes the next waiting thread) and goes back to blocking.

```cpp
// Per-thread argument structure
typedef struct {
    SharedGameState* shm;
    int player_index;   // which player character this thread controls
} PlayerThreadArgs;

void* player_thread_func(void* arg) {
    PlayerThreadArgs* args = (PlayerThreadArgs*)arg;
    SharedGameState* shm   = args->shm;
    int my_index           = args->player_index;

    while (true) {
        // Block until the Arbiter posts turn_notification
        sem_wait(&shm->turn.turn_notification);

        // Check if it's our turn
        pthread_mutex_lock(&shm->master_mutex);
        int is_my_turn = (shm->turn.current_entity_is_player == 1 &&
                          shm->turn.current_entity_id == my_index);
        int game_over  = (shm->game_result != GAME_ONGOING);
        pthread_mutex_unlock(&shm->master_mutex);

        if (game_over) break;  // Exit thread

        if (!is_my_turn) {
            // Not our turn — re-post so another thread can check
            sem_post(&shm->turn.turn_notification);
            continue;
        }

        // ── It IS our turn — read player input ──────────────────────────
        display_player_menu(shm, my_index);
        int action_type   = 0;
        int target_id     = -1;
        int weapon_slot   = -1;
        int lt_weapon_id  = WEAPON_NONE;

        // Read input (blocking stdin read)
        read_player_action(shm, my_index,
                           &action_type, &target_id,
                           &weapon_slot, &lt_weapon_id);

        // ── Write action to shared memory ────────────────────────────────
        pthread_mutex_lock(&shm->master_mutex);
        shm->players[my_index].action_type       = action_type;
        shm->players[my_index].action_target_id  = target_id;
        shm->players[my_index].action_weapon_slot = weapon_slot;
        shm->players[my_index].action_lt_weapon_id = lt_weapon_id;
        shm->players[my_index].action_ready      = 1;
        pthread_mutex_unlock(&shm->master_mutex);

        // ── Notify Arbiter that action is ready ──────────────────────────
        sem_post(&shm->turn.action_submitted);
    }

    return NULL;
}
```

### 5.4 Input Reading Function

```cpp
void display_player_menu(SharedGameState* shm, int player_index) {
    Entity* p = &shm->players[player_index];
    std::cout << "\n══ Player " << player_index
              << " | HP: " << p->hp << "/" << p->max_hp
              << " | Stamina: FULL ══" << std::endl;
    std::cout << "Choose action:" << std::endl;
    std::cout << "  1. Attack (Strike)   — damage: " << p->base_damage << std::endl;
    std::cout << "  2. Attack (Exhaust)  — reduce enemy stamina by " << p->base_damage << std::endl;
    std::cout << "  3. Use Weapon        — use a weapon from inventory" << std::endl;
    std::cout << "  4. Swap In           — retrieve weapon from long-term storage" << std::endl;
    std::cout << "  5. Heal              — restore 10% HP" << std::endl;
    std::cout << "  6. Skip              — skip turn (stamina → 50%)" << std::endl;
    std::cout << "  0. Quit game" << std::endl;
    std::cout << "> ";
}

void read_player_action(SharedGameState* shm, int player_index,
                         int* action_type, int* target_id,
                         int* weapon_slot, int* lt_weapon_id) {
    int choice = 0;
    std::cin >> choice;

    if (choice == 0) {
        // Quit — HIP sends SIGTERM to Arbiter
        kill(shm->arbiter_pid, SIGTERM);
        *action_type = ACTION_SKIP;  // dummy, game will end
        return;
    }

    *action_type = choice;   // maps 1→ATTACK_STRIKE, 2→EXHAUST, etc.

    // For actions that need a target:
    if (choice == ACTION_ATTACK_STRIKE || choice == ACTION_ATTACK_EXHAUST
        || choice == ACTION_USE_WEAPON) {
        // Show available enemies
        std::cout << "Select target enemy:" << std::endl;
        for (int i = 0; i < shm->num_npcs_concurrent; i++) {
            if (shm->npcs[i].is_alive) {
                std::cout << "  " << i << ". NPC " << shm->npcs[i].id
                          << " HP: " << shm->npcs[i].hp << std::endl;
            }
        }
        std::cout << "> ";
        std::cin >> *target_id;
    }

    // For USE_WEAPON: show inventory
    if (choice == ACTION_USE_WEAPON) {
        std::cout << "Select inventory slot:" << std::endl;
        for (int s = 0; s < INVENTORY_SLOTS; s++) {
            if (shm->players[player_index].inventory[s] != WEAPON_NONE) {
                int wid = shm->players[player_index].inventory[s];
                std::cout << "  Slot " << s << ": " << weapon_name[wid]
                          << " (dmg " << weapon_damage[wid] << ")" << std::endl;
            }
        }
        std::cout << "> ";
        std::cin >> *weapon_slot;
    }

    // For SWAP_IN: show long-term storage
    if (choice == ACTION_SWAP_IN) {
        Entity* p = &shm->players[player_index];
        std::cout << "Select weapon from long-term storage:" << std::endl;
        for (int i = 0; i < p->lt_storage_count; i++) {
            int wid = p->lt_storage[i];
            if (wid != WEAPON_NONE) {
                std::cout << "  " << wid << ": " << weapon_name[wid]
                          << " (" << weapon_slot_size[wid] << " slots)" << std::endl;
            }
        }
        std::cout << "> ";
        std::cin >> *lt_weapon_id;
    }
}
```

### 5.5 HIP Main Function

```cpp
int main(int argc, char* argv[]) {
    SharedGameState* shm = attach_shared_memory_with_retry();
    shm->hip_pid = getpid();

    // Wait for game to start
    while (!shm->game_started) usleep(10000);

    int num_players = shm->num_players;
    pthread_t player_threads[MAX_PLAYERS];
    PlayerThreadArgs thread_args[MAX_PLAYERS];

    // Spawn one thread per player character
    for (int i = 0; i < num_players; i++) {
        thread_args[i].shm          = shm;
        thread_args[i].player_index = i;
        pthread_create(&player_threads[i], NULL, player_thread_func, &thread_args[i]);
    }

    // Wait for all player threads to finish
    for (int i = 0; i < num_players; i++) {
        pthread_join(player_threads[i], NULL);
    }

    detach_shared_memory(shm);
    return 0;
}
```

**Phase 5 is complete when:** HIP spawns the correct number of threads, the correct thread activates when the Arbiter sets `current_entity_id`, input is read and written to shared memory, and the action_submitted semaphore is posted correctly.

---

## Phase 6 — Automated Strategic Process: NPC Thread Pool

### 6.1 What This Phase Accomplishes

The ASP manages all NPCs. Each NPC gets its own dedicated `pthread`. The NPC thread waits for the Arbiter to signal its turn, makes an AI decision (Attack or Skip), writes the action to shared memory, and posts the action_submitted semaphore. This phase also handles dynamic NPC spawning — as enemies are killed, new ones appear, and new threads are created for them.

**Marks covered:** Thread-per-NPC Implementation (10), Thread Synchronization (10), Efficient Thread Scheduling (10)

### 6.2 NPC Thread Design

NPC threads follow the same wake/check/act/sleep pattern as player threads, with the difference that their "input" is an AI decision rather than human keyboard input.

```cpp
typedef struct {
    SharedGameState* shm;
    int npc_slot_index;    // which slot in shm->npcs[] this thread manages
} NpcThreadArgs;

void* npc_thread_func(void* arg) {
    NpcThreadArgs* args = (NpcThreadArgs*)arg;
    SharedGameState* shm = args->shm;
    int slot = args->npc_slot_index;

    while (true) {
        // Block until turn notification
        sem_wait(&shm->turn.turn_notification);

        pthread_mutex_lock(&shm->master_mutex);
        int is_my_turn = (shm->turn.current_entity_is_player == 0 &&
                          shm->turn.current_entity_id == shm->npcs[slot].id);
        int is_alive   = shm->npcs[slot].is_alive;
        int game_over  = (shm->game_result != GAME_ONGOING);
        pthread_mutex_unlock(&shm->master_mutex);

        if (game_over || !is_alive) break;

        if (!is_my_turn) {
            sem_post(&shm->turn.turn_notification);
            continue;
        }

        // ── AI Decision Logic ────────────────────────────────────────────
        // Simple AI: 80% chance to Attack Strike, 20% chance to Skip
        int action_type = ACTION_ATTACK_STRIKE;
        int target_player_id = -1;

        if (rand() % 10 < 2) {
            action_type = ACTION_SKIP;
        } else {
            // Pick a random alive player as target
            int alive_players[MAX_PLAYERS];
            int count = 0;
            pthread_mutex_lock(&shm->master_mutex);
            for (int i = 0; i < shm->num_players; i++) {
                if (shm->players[i].is_alive) {
                    alive_players[count++] = i;
                }
            }
            pthread_mutex_unlock(&shm->master_mutex);

            if (count > 0) {
                target_player_id = alive_players[rand() % count];
            } else {
                action_type = ACTION_SKIP;  // no alive players — shouldn't happen
            }
        }

        // ── Write action to shared memory ────────────────────────────────
        pthread_mutex_lock(&shm->master_mutex);
        shm->npcs[slot].action_type      = action_type;
        shm->npcs[slot].action_target_id = target_player_id;
        shm->npcs[slot].action_ready     = 1;
        pthread_mutex_unlock(&shm->master_mutex);

        // ── Notify Arbiter ────────────────────────────────────────────────
        sem_post(&shm->turn.action_submitted);
    }

    return NULL;
}
```

### 6.3 ASP Main Function with Dynamic Thread Management

```cpp
// Track threads for all NPCs ever spawned
pthread_t npc_threads[MAX_NPCS * 2];   // extra space for respawned NPCs
NpcThreadArgs npc_args[MAX_NPCS * 2];
int total_threads_spawned = 0;

int main(int argc, char* argv[]) {
    SharedGameState* shm = attach_shared_memory_with_retry();
    shm->asp_pid = getpid();

    // Install stun signal handler (Phase 10)
    // setup_asp_signals();

    while (!shm->game_started) usleep(10000);

    int num_npcs = shm->num_npcs_concurrent;

    // Spawn initial NPC threads
    for (int i = 0; i < num_npcs; i++) {
        npc_args[i].shm            = shm;
        npc_args[i].npc_slot_index = i;
        pthread_create(&npc_threads[i], NULL, npc_thread_func, &npc_args[i]);
        total_threads_spawned++;
    }

    // Monitor for new NPCs spawned by Arbiter (during gameplay)
    while (shm->game_result == GAME_ONGOING) {
        // Check if any new NPCs were added to slots beyond initial count
        pthread_mutex_lock(&shm->master_mutex);
        int current_total = shm->total_npcs_spawned;
        pthread_mutex_unlock(&shm->master_mutex);

        if (current_total > total_threads_spawned) {
            // New NPC spawned — find its slot and create thread
            for (int i = 0; i < MAX_NPCS; i++) {
                pthread_mutex_lock(&shm->master_mutex);
                int alive = shm->npcs[i].is_alive;
                int slot_id = shm->npcs[i].id;
                pthread_mutex_unlock(&shm->master_mutex);

                // Check if this slot has a thread
                // Simplified: track by slot index in a bool array
                // (implement proper tracking as needed)
            }
        }
        usleep(100000);  // 100ms polling
    }

    // Join all threads
    for (int i = 0; i < total_threads_spawned; i++) {
        pthread_join(npc_threads[i], NULL);
    }

    detach_shared_memory(shm);
    return 0;
}
```

**Phase 6 is complete when:** ASP spawns the correct number of NPC threads (2–9), each thread activates only on its own turn, the AI decision is written to shared memory, and the action_submitted semaphore is posted.

---

## Phase 7 — Stamina Accumulation Engine & Turn Scheduler

### 7.1 What This Phase Accomplishes

This is the core scheduling engine of the game. Every second, each entity's speed is added to their stamina. The first entity to reach max stamina gets their turn. Stamina accumulation happens concurrently for all entities (using a single timer thread in Arbiter), but only one entity acts at a time. This phase implements the stamina tick loop, the turn-detection logic, and the Arbiter's turn-dispatch mechanism.

**Marks covered:** Stamina-Based Scheduling (10), Arrival Time Accuracy (10), Serial Execution (10), Turn Reset & Recalculation (10)

### 7.2 The Scheduler Design

The simplest correct design uses a **dedicated stamina tick thread** in the Arbiter that increments stamina every 100ms (0.1s) for all entities by `speed * 0.1`. Every tick, it checks whether any entity has reached max stamina. The first such entity triggers a turn.

This simulates concurrent stamina accumulation faithfully while keeping the scheduler single-threaded (serial action execution).

### 7.3 Stamina Tick Thread

```cpp
// Stamina tick interval: 100ms = 0.1 seconds
#define TICK_MS        100
#define TICK_SECONDS   0.1f

void* stamina_tick_thread(void* arg) {
    SharedGameState* shm = (SharedGameState*)arg;

    while (shm->game_result == GAME_ONGOING) {
        usleep(TICK_MS * 1000);   // sleep 100ms

        pthread_mutex_lock(&shm->master_mutex);

        // Skip ticking if a turn is in progress (action_in_progress flag)
        if (shm->turn.action_in_progress || shm->ultimate_active) {
            pthread_mutex_unlock(&shm->master_mutex);
            continue;
        }

        // Tick all alive players
        for (int i = 0; i < shm->num_players; i++) {
            Entity* p = &shm->players[i];
            if (!p->is_alive || p->is_stunned) continue;
            p->stamina += p->speed * TICK_SECONDS;
            if (p->stamina > p->max_stamina) p->stamina = p->max_stamina;
        }

        // Tick all alive NPCs
        for (int i = 0; i < shm->num_npcs_concurrent; i++) {
            Entity* n = &shm->npcs[i];
            if (!n->is_alive || n->is_stunned) continue;
            n->stamina += n->speed * TICK_SECONDS;
            if (n->stamina > n->max_stamina) n->stamina = n->max_stamina;
        }

        pthread_mutex_unlock(&shm->master_mutex);
    }
    return NULL;
}
```

### 7.4 Turn Detection and Dispatch

The Arbiter's main game loop continuously polls for entities that have reached full stamina. When found, it initiates a turn:

```cpp
void main_game_loop(SharedGameState* shm) {
    // Launch stamina tick thread
    pthread_t tick_thread;
    pthread_create(&tick_thread, NULL, stamina_tick_thread, shm);

    while (shm->game_result == GAME_ONGOING) {
        // ── Find entity with full stamina ────────────────────────────────
        int acting_entity_id     = -1;
        int acting_is_player     = -1;
        int acting_slot_index    = -1;
        float acting_arrival     = -1.0f;

        pthread_mutex_lock(&shm->master_mutex);

        // Check all entities — find who filled stamina first
        // "First" = highest stamina (all who hit max got capped at max)
        // Tiebreak: lowest entity_id wins (deterministic)
        for (int i = 0; i < shm->num_players; i++) {
            Entity* p = &shm->players[i];
            if (p->is_alive && !p->is_stunned &&
                p->stamina >= (float)p->max_stamina) {
                if (acting_entity_id == -1 || p->id < acting_entity_id) {
                    acting_entity_id  = p->id;
                    acting_is_player  = 1;
                    acting_slot_index = i;
                }
            }
        }
        for (int i = 0; i < shm->num_npcs_concurrent; i++) {
            Entity* n = &shm->npcs[i];
            if (n->is_alive && !n->is_stunned &&
                n->stamina >= (float)n->max_stamina) {
                if (acting_entity_id == -1 || n->id < acting_entity_id) {
                    acting_entity_id  = n->id;
                    acting_is_player  = 0;
                    acting_slot_index = i;
                }
            }
        }
        pthread_mutex_unlock(&shm->master_mutex);

        if (acting_entity_id == -1) {
            usleep(10000);   // no one ready yet, poll again in 10ms
            continue;
        }

        // ── Dispatch the turn ────────────────────────────────────────────
        dispatch_turn(shm, acting_entity_id, acting_is_player, acting_slot_index);
    }

    pthread_join(tick_thread, NULL);
}
```

### 7.5 Turn Dispatch Function

```cpp
void dispatch_turn(SharedGameState* shm, int entity_id,
                   int is_player, int slot_index) {
    // 1. Set turn state
    pthread_mutex_lock(&shm->master_mutex);
    shm->turn.current_entity_id       = entity_id;
    shm->turn.current_entity_is_player = is_player;
    shm->turn.action_in_progress      = 1;
    shm->turn.action_ready_flag       = 0;

    // Reset action_ready for this entity
    if (is_player) shm->players[slot_index].action_ready = 0;
    else           shm->npcs[slot_index].action_ready    = 0;
    pthread_mutex_unlock(&shm->master_mutex);

    // Log the turn start
    char log_msg[ACTION_LOG_WIDTH];
    snprintf(log_msg, ACTION_LOG_WIDTH, "%s %d's turn begins",
             is_player ? "Player" : "NPC", entity_id);
    append_action_log(shm, log_msg);

    // 2. Notify HIP or ASP
    sem_post(&shm->turn.turn_notification);

    // 3. Wait for action with 3-second timeout (for NPC)
    if (!is_player) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 3;   // 3-second timeout for NPC

        int result = sem_timedwait(&shm->turn.action_submitted, &ts);
        if (result == -1 && errno == ETIMEDOUT) {
            // NPC timed out → auto-assign Skip
            pthread_mutex_lock(&shm->master_mutex);
            shm->npcs[slot_index].action_type  = ACTION_SKIP;
            shm->npcs[slot_index].action_ready = 1;
            pthread_mutex_unlock(&shm->master_mutex);
            append_action_log(shm, "NPC turn timed out — auto Skip");
        }
    } else {
        // Player — wait indefinitely (player input has no timeout)
        sem_wait(&shm->turn.action_submitted);
    }

    // 4. Apply the action (Phases 8 and 9)
    apply_action(shm, entity_id, is_player, slot_index);

    // 5. Reset stamina
    pthread_mutex_lock(&shm->master_mutex);
    if (is_player) shm->players[slot_index].stamina = 0.0f;
    else           shm->npcs[slot_index].stamina    = 0.0f;
    shm->turn.action_in_progress = 0;
    pthread_mutex_unlock(&shm->master_mutex);

    // 6. Check win/lose conditions (Phase 17)
    check_game_conditions(shm);
}
```

**Phase 7 is complete when:** Stamina accumulates correctly in real-time for all entities, the correct entity takes its turn first, stamina resets to 0 after action, and the scheduler loops correctly.

---

## Phase 8 — Player Action Execution (All 6 Actions)

### 8.1 What This Phase Accomplishes

When a player submits an action (written to shared memory by HIP and signaled via the semaphore), the Arbiter reads it and applies the game effect. This phase implements all six player actions: Attack (Strike), Attack (Exhaust), Use Weapon, Swap In, Heal, and Skip.

**Marks covered:** Correct Implementation of Player Actions (10), Gameplay Logic (partial)

### 8.2 The Action Application Function

```cpp
void apply_player_action(SharedGameState* shm, int player_slot) {
    Entity* actor = &shm->players[player_slot];
    int action    = actor->action_type;
    int target_id = actor->action_target_id;

    pthread_mutex_lock(&shm->master_mutex);

    char log_msg[ACTION_LOG_WIDTH];

    switch (action) {

    case ACTION_ATTACK_STRIKE: {
        // Find target NPC by ID
        Entity* target = find_npc_by_id(shm, target_id);
        if (target && target->is_alive) {
            target->hp -= actor->base_damage;
            if (target->hp <= 0) {
                target->hp       = 0;
                target->is_alive = STATUS_DEAD;
                shm->total_kills++;
                snprintf(log_msg, ACTION_LOG_WIDTH,
                         "Player %d STRIKES NPC %d for %d dmg — KILLED! (%d/10)",
                         player_slot, target_id, actor->base_damage, shm->total_kills);
                pthread_mutex_unlock(&shm->master_mutex);
                handle_npc_death(shm, target);
                pthread_mutex_lock(&shm->master_mutex);
            } else {
                snprintf(log_msg, ACTION_LOG_WIDTH,
                         "Player %d STRIKES NPC %d for %d dmg (HP: %d/%d)",
                         player_slot, target_id, actor->base_damage,
                         target->hp, target->max_hp);
            }
        }
        // stamina → 0 (handled in dispatch_turn after apply_action)
        break;
    }

    case ACTION_ATTACK_EXHAUST: {
        Entity* target = find_npc_by_id(shm, target_id);
        if (target && target->is_alive) {
            target->stamina -= actor->base_damage;
            if (target->stamina < 0) target->stamina = 0;
            snprintf(log_msg, ACTION_LOG_WIDTH,
                     "Player %d EXHAUSTS NPC %d (stamina: %.0f/%d)",
                     player_slot, target_id, target->stamina, target->max_stamina);
        }
        break;
    }

    case ACTION_USE_WEAPON: {
        int slot    = actor->action_weapon_slot;
        int wid     = actor->inventory[slot];
        if (wid != WEAPON_NONE) {
            Entity* target = find_npc_by_id(shm, target_id);
            if (target && target->is_alive) {
                int dmg = weapon_damage[wid];
                target->hp -= dmg;
                if (target->hp <= 0) {
                    target->hp       = 0;
                    target->is_alive = STATUS_DEAD;
                    shm->total_kills++;
                    pthread_mutex_unlock(&shm->master_mutex);
                    handle_npc_death(shm, target);
                    pthread_mutex_lock(&shm->master_mutex);
                }
                snprintf(log_msg, ACTION_LOG_WIDTH,
                         "Player %d uses %s on NPC %d for %d dmg",
                         player_slot, weapon_name[wid], target_id, dmg);
            }
        }
        break;
    }

    case ACTION_SWAP_IN: {
        // The actual inventory manipulation is handled by the allocator (Phase 12/13)
        // Here we just call it
        int wid = actor->action_lt_weapon_id;
        pthread_mutex_unlock(&shm->master_mutex);
        swap_in_weapon(shm, player_slot, wid);  // Phase 13
        pthread_mutex_lock(&shm->master_mutex);
        snprintf(log_msg, ACTION_LOG_WIDTH,
                 "Player %d swaps in %s from long-term storage (cannot use this turn)",
                 player_slot, weapon_name[wid]);
        // stamina → 0, weapon not usable this turn (enforced by flag)
        break;
    }

    case ACTION_HEAL: {
        int heal_amount = actor->max_hp / 10;  // 10% of max HP
        actor->hp += heal_amount;
        if (actor->hp > actor->max_hp) actor->hp = actor->max_hp;
        snprintf(log_msg, ACTION_LOG_WIDTH,
                 "Player %d HEALS for %d HP (HP: %d/%d)",
                 player_slot, heal_amount, actor->hp, actor->max_hp);
        break;
    }

    case ACTION_SKIP: {
        actor->stamina = actor->max_stamina * 0.5f;  // stamina → 50%
        snprintf(log_msg, ACTION_LOG_WIDTH, "Player %d SKIPs (stamina → 50)", player_slot);
        // NOTE: dispatch_turn will set stamina to 0 after apply_action
        // We need to override that. Set a flag or handle Skip specially
        // Best: set stamina to 50% INSIDE apply_action and return a flag
        // to tell dispatch_turn NOT to reset stamina to 0
        break;
    }

    }  // end switch

    pthread_mutex_unlock(&shm->master_mutex);
    append_action_log(shm, log_msg);
}
```

> **Skip Note:** The `dispatch_turn` function resets stamina to 0 after `apply_action`. For Skip, stamina must be 50% (not 0). Implement a return flag or a special case in `dispatch_turn` to handle Skip correctly.

### 8.3 Find Entity by ID Helper

```cpp
Entity* find_npc_by_id(SharedGameState* shm, int target_id) {
    for (int i = 0; i < shm->num_npcs_concurrent; i++) {
        if (shm->npcs[i].id == target_id) return &shm->npcs[i];
    }
    return NULL;
}
```

**Phase 8 is complete when:** All six player actions apply their correct effects. Skip leaves stamina at 50%. Heal restores exactly 10% HP. Weapon damage matches the weapon table. The action log records every action.

---

## Phase 9 — NPC Action Execution & Timeout Logic

### 9.1 What This Phase Accomplishes

NPC actions are simpler (Strike or Skip) but require the 3-second timeout enforcement already scaffolded in Phase 7's `dispatch_turn`. This phase finalizes NPC action application, the timeout auto-Skip, and NPC death handling including respawn logic.

**Marks covered:** Enemy Behavior & Decision Logic (5), NPC Turn Timeout (5 from signals section)

### 9.2 NPC Action Application

```cpp
void apply_npc_action(SharedGameState* shm, int npc_slot) {
    Entity* actor    = &shm->npcs[npc_slot];
    int action       = actor->action_type;
    int target_id    = actor->action_target_id;
    char log_msg[ACTION_LOG_WIDTH];

    pthread_mutex_lock(&shm->master_mutex);

    switch (action) {

    case ACTION_ATTACK_STRIKE: {
        // Target is a player
        Entity* target = (target_id >= 0 && target_id < shm->num_players)
                         ? &shm->players[target_id] : NULL;
        if (target && target->is_alive) {
            target->hp -= actor->base_damage;
            if (target->hp <= 0) {
                target->hp       = 0;
                target->is_alive = STATUS_DEAD;
                snprintf(log_msg, ACTION_LOG_WIDTH,
                         "NPC %d STRIKES Player %d for %d dmg — PLAYER KILLED!",
                         actor->id, target_id, actor->base_damage);
            } else {
                snprintf(log_msg, ACTION_LOG_WIDTH,
                         "NPC %d STRIKES Player %d for %d dmg (HP: %d/%d)",
                         actor->id, target_id, actor->base_damage,
                         target->hp, target->max_hp);
            }
        }
        break;
    }

    case ACTION_SKIP: {
        actor->stamina = actor->max_stamina * 0.5f;  // 75 for NPC (150*0.5)
        snprintf(log_msg, ACTION_LOG_WIDTH,
                 "NPC %d SKIPs (stamina → 75)", actor->id);
        break;
    }

    }

    pthread_mutex_unlock(&shm->master_mutex);
    append_action_log(shm, log_msg);
}
```

### 9.3 NPC Death and Respawn

```cpp
void handle_npc_death(SharedGameState* shm, Entity* dead_npc) {
    // 1. Log death
    char log_msg[ACTION_LOG_WIDTH];
    snprintf(log_msg, ACTION_LOG_WIDTH, "NPC %d defeated! Total kills: %d/10",
             dead_npc->id, shm->total_kills);
    append_action_log(shm, log_msg);

    // 2. Drop a random weapon (NOT the NPC's held weapon — spec rule)
    // 30% chance to drop a weapon (adjust as desired — spec says "a chance")
    if (rand() % 10 < 3) {
        int dropped_weapon = rand() % NUM_WEAPON_TYPES;
        pthread_mutex_lock(&shm->master_mutex);
        shm->dropped_weapon_id = dropped_weapon;
        shm->drop_awaiting_player_choice = 1;
        pthread_mutex_unlock(&shm->master_mutex);
        // Notify HIP to present pickup choice to player (handled in Phase 13)
    }

    // 3. Release any artifacts held by this NPC
    pthread_mutex_lock(&shm->resources.table_mutex);
    if (shm->resources.solar_core_holder == dead_npc->id) {
        shm->resources.solar_core_holder = -1;
    }
    if (shm->resources.lunar_blade_holder == dead_npc->id) {
        shm->resources.lunar_blade_holder = -1;
    }
    pthread_mutex_unlock(&shm->resources.table_mutex);

    // 4. Respawn a new NPC if total kills < 10 and we need more enemies
    pthread_mutex_lock(&shm->master_mutex);
    if (shm->total_kills < 10) {
        // Find the dead NPC's slot and reuse it
        int slot = (int)(dead_npc - shm->npcs);   // pointer arithmetic for slot index
        init_npc(&shm->npcs[slot], slot, shm->total_npcs_spawned);
        shm->total_npcs_spawned++;
        // The ASP's monitor loop will detect the new NPC and spawn a thread for it
    }
    pthread_mutex_unlock(&shm->master_mutex);
}
```

**Phase 9 is complete when:** NPC Strike applies correct damage, Skip sets stamina to 75, timeout assigns Skip correctly, and NPC death triggers respawn with a new thread in ASP.

---

## Phase 10 — Signal Infrastructure: SIGUSR1 Stun Mechanic

### 10.1 What This Phase Accomplishes

This phase implements the **Stun mechanic** using signals. When a stun attack lands (defined as: certain high-tier attacks trigger stun — you can define "high tier" as attacks with weapons doing over 50 damage, or attacks with Solar Core / Lunar Blade), the Arbiter sends `SIGUSR1` to the target's process (HIP for players, ASP for NPCs). The target's signal handler pauses execution for exactly 3 seconds, then resumes from the exact point of interruption. Stamina is preserved.

**Marks covered:** Signal-Based Stun (10), Correct Stun Duration — 3 seconds (5), Non-Blocking Interrupt (5)

### 10.2 Stun Trigger Definition

From the spec: "Certain high-tier attacks can Stun a target." You need to define what constitutes a high-tier attack. Reasonable definition:
- Using the **Solar Core** (95 dmg): has a 50% chance to stun.
- Using the **Lunar Blade** (90 dmg): has a 50% chance to stun.
- Using **Iron Halberd** (55 dmg): has a 25% chance to stun.
- Other weapons: no stun.

This should be explained in your report.

### 10.3 Signal Handler in HIP and ASP

Both HIP and ASP must install a `SIGUSR1` handler at startup. The handler sleeps for exactly 3 seconds:

```cpp
// stun_handler.cpp (used by both hip and asp)

#include <signal.h>
#include <unistd.h>
#include <iostream>

// Global flag so the handler can access shared memory to mark stun status
// (We use a simple approach: the handler sleeps for exactly 3 seconds)

void stun_signal_handler(int sig) {
    // This runs asynchronously, interrupting whatever the process was doing.
    // sleep(3) will pause the calling thread/process for exactly 3 seconds.
    // When sleep returns, execution resumes from where we were interrupted.
    //
    // NOTE: sleep() is async-signal-safe in practice for this use case.
    // The stamina does NOT change during this time (the tick thread is separate
    // in Arbiter; the stunned process's threads just don't execute).
    //
    // If the NPC/player had full stamina and was about to act,
    // the Arbiter will handle the skip via the 3-second timeout mechanism.

    write(STDOUT_FILENO, "[STUN] Process stunned for 3 seconds!\n", 38);
    sleep(3);
    write(STDOUT_FILENO, "[STUN] Stun duration ended, resuming.\n", 38);
}

void setup_stun_handler() {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags   = 0;   // No SA_RESTART — we want signal to interrupt syscalls
    sa.sa_handler = stun_signal_handler;
    sigaction(SIGUSR1, &sa, NULL);
}
```

> **Important:** Use `write()` not `std::cout` inside signal handlers — `cout` is not async-signal-safe. `write()` is safe.

### 10.4 Arbiter Sends the Stun Signal

In the Arbiter's `apply_player_action` (for weapons that can stun):

```cpp
// After dealing damage with a high-tier weapon:
int should_stun = check_stun_chance(wid);   // returns 1 or 0

if (should_stun && target && target->is_alive) {
    pid_t target_pid;
    if (target_is_npc) {
        target_pid = shm->asp_pid;   // send to ASP process
    } else {
        target_pid = shm->hip_pid;   // send to HIP process
    }

    // Mark entity as stunned in shared memory
    pthread_mutex_lock(&shm->master_mutex);
    target->is_stunned = STATUS_STUNNED;
    pthread_mutex_unlock(&shm->master_mutex);

    // Send SIGUSR1 to the target process
    kill(target_pid, SIGUSR1);

    // After 3 seconds (handled by handler), unmark stun
    // The Arbiter must also unmark is_stunned after 3 seconds
    // Use a timer thread or SIGALRM for this
    schedule_stun_recovery(shm, target, 3);
}

int check_stun_chance(int weapon_id) {
    if (weapon_id == WEAPON_SOLAR_CORE || weapon_id == WEAPON_LUNAR_BLADE)
        return (rand() % 2 == 0) ? 1 : 0;   // 50%
    if (weapon_id == WEAPON_IRON_HALBERD)
        return (rand() % 4 == 0) ? 1 : 0;   // 25%
    return 0;
}
```

### 10.5 Stun Recovery in Arbiter

The Arbiter needs to clear the `is_stunned` flag after 3 seconds. Since the Arbiter cannot block (it runs the game loop), use a background thread:

```cpp
typedef struct {
    SharedGameState* shm;
    Entity* target;
    int duration_seconds;
} StunRecoveryArgs;

void* stun_recovery_thread(void* arg) {
    StunRecoveryArgs* args = (StunRecoveryArgs*)arg;
    sleep(args->duration_seconds);   // exactly 3 seconds

    pthread_mutex_lock(&args->shm->master_mutex);
    args->target->is_stunned = STATUS_NOT_STUNNED;
    pthread_mutex_unlock(&args->shm->master_mutex);

    char log_msg[ACTION_LOG_WIDTH];
    snprintf(log_msg, ACTION_LOG_WIDTH, "Entity recovered from stun");
    append_action_log(args->shm, log_msg);

    free(args);
    return NULL;
}

void schedule_stun_recovery(SharedGameState* shm, Entity* target, int seconds) {
    StunRecoveryArgs* args = (StunRecoveryArgs*)malloc(sizeof(StunRecoveryArgs));
    args->shm     = shm;
    args->target  = target;
    args->duration_seconds = seconds;

    pthread_t t;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&t, &attr, stun_recovery_thread, args);
    pthread_attr_destroy(&attr);
}
```

**Phase 10 is complete when:** Sending SIGUSR1 to HIP/ASP causes those processes to pause for exactly 3 seconds, `is_stunned` is set and then cleared, and the stun entity does not accumulate stamina (the tick thread checks `is_stunned`).

---

## Phase 11 — Signal Infrastructure: Ultimate Ability (SIGSTOP/SIGCONT/SIGALRM)

### 11.1 What This Phase Accomplishes

The Ultimate Ability is the most complex signal mechanic. When triggered, the entire ASP process is suspended for exactly 10 seconds using signals alone — no flags, no pipes. The Arbiter sends `SIGSTOP` to ASP, sets a `SIGALRM` for 10 seconds, and in the alarm handler sends `SIGCONT` to ASP and provides updated staminas.

**Marks covered:** Ultimate Ability Signal-Only Suspension (10), Correct Duration — 10 seconds (5), Signal Handling Robustness (5)

### 11.2 Eligibility Check

```cpp
int can_use_ultimate(SharedGameState* shm, int player_slot) {
    Entity* p = &shm->players[player_slot];
    return (p->holds_solar_core && p->holds_lunar_blade);
    // Both must be in the active primary inventory
}
```

### 11.3 Ultimate Ability Trigger in Arbiter

The Ultimate Ability action is triggered when a player chooses "Use Weapon" with Solar Core or Lunar Blade AND holds both. Or you can add it as a separate menu option in HIP. Add `ACTION_ULTIMATE` as a new action type:

```cpp
// In arbiter.cpp — when Ultimate Ability is triggered:
void trigger_ultimate_ability(SharedGameState* shm) {
    std::cout << "[ARBITER] ULTIMATE ABILITY TRIGGERED — Suspending ASP for 10 seconds!" << std::endl;
    append_action_log(shm, "*** ULTIMATE ABILITY! ASP suspended for 10 seconds ***");

    // Set the flag
    pthread_mutex_lock(&shm->master_mutex);
    shm->ultimate_active = 1;
    pthread_mutex_unlock(&shm->master_mutex);

    // Store ASP PID for use in the alarm handler
    ultimate_asp_pid = shm->asp_pid;
    ultimate_shm     = shm;

    // Suspend the ASP process — signal-only, no flags/pipes
    kill(shm->asp_pid, SIGSTOP);

    // Set alarm for 10 seconds — handler will resume ASP
    alarm(10);

    // The SIGALRM handler will fire after 10 seconds and resume ASP
}
```

### 11.4 SIGALRM Handler in Arbiter

```cpp
// Global variables needed by the signal handler
static pid_t ultimate_asp_pid   = 0;
static SharedGameState* ultimate_shm = NULL;

void sigalrm_handler(int sig) {
    // This fires 10 seconds after alarm(10) was called
    write(STDOUT_FILENO, "[ARBITER] Ultimate window expired — resuming ASP\n", 49);

    if (ultimate_asp_pid > 0) {
        // Resume the ASP process
        kill(ultimate_asp_pid, SIGCONT);
    }

    // Clear the ultimate flag (write to shared memory from signal handler)
    // Note: Writing a single int is effectively atomic on most architectures
    // For full safety, use sig_atomic_t for the flag
    if (ultimate_shm) {
        ultimate_shm->ultimate_active = 0;
    }

    ultimate_asp_pid = 0;
}

void setup_arbiter_signals(SharedGameState* shm) {
    ultimate_shm = shm;

    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags   = 0;

    // SIGALRM — Ultimate Ability timer
    sa.sa_handler = sigalrm_handler;
    sigaction(SIGALRM, &sa, NULL);

    // SIGTERM — graceful quit from HIP
    sa.sa_handler = sigterm_handler;
    sigaction(SIGTERM, &sa, NULL);
}
```

### 11.5 SIGTERM Graceful Quit Handler

```cpp
// Global flag for quit
static volatile sig_atomic_t quit_requested = 0;

void sigterm_handler(int sig) {
    write(STDOUT_FILENO, "[ARBITER] SIGTERM received — initiating graceful shutdown\n", 58);
    quit_requested = 1;
    // The main loop checks quit_requested and sets game_result = GAME_QUIT
}
```

In the main game loop:
```cpp
while (shm->game_result == GAME_ONGOING) {
    if (quit_requested) {
        pthread_mutex_lock(&shm->master_mutex);
        shm->game_result = GAME_QUIT;
        pthread_mutex_unlock(&shm->master_mutex);
        break;
    }
    // ... rest of loop
}
```

**Phase 11 is complete when:** Triggering the Ultimate Ability sends SIGSTOP to ASP (which stops all NPC threads), sets SIGALRM for 10 seconds, and the alarm handler sends SIGCONT resuming ASP with updated state. No flags or pipes are used for the suspension coordination.

---

## Phase 12 — Inventory Allocator: Contiguous Memory Management

### 12.1 What This Phase Accomplishes

The inventory is a 20-slot linear array. Weapons occupy contiguous slots. When a weapon needs to be placed, the allocator finds a free contiguous region. If none exists, it swaps out weapons to long-term storage (minimum necessary). This phase implements the complete allocator. The Splinter Stick (2 slots) is designed to create fragmentation — the allocator must handle this correctly.

**Marks covered:** Contiguous Allocation Strategy (10), Fragmentation Handling (10), Swap In / Swap Out (10), Long-Term Storage (5), Solar+Lunar Constraint (5)

### 12.2 Finding a Contiguous Free Region

```cpp
// Returns the starting slot index where weapon_slots consecutive free slots exist
// Returns -1 if no such region exists
int find_contiguous_free(int* inventory, int weapon_slots) {
    int start = -1;
    int count = 0;
    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        if (inventory[i] == WEAPON_NONE) {
            if (count == 0) start = i;
            count++;
            if (count >= weapon_slots) return start;
        } else {
            count = 0;
            start = -1;
        }
    }
    return -1;   // not enough contiguous free space
}
```

### 12.3 Placing a Weapon Into Inventory

```cpp
// Place weapon_id into inventory starting at slot 'start_slot'
void place_weapon(int* inventory, int start_slot, int weapon_id) {
    int slots = weapon_slot_size[weapon_id];
    for (int i = start_slot; i < start_slot + slots; i++) {
        inventory[i] = weapon_id;
    }
}
```

### 12.4 Removing a Weapon From Inventory

```cpp
// Remove weapon starting at slot 'start_slot' from inventory
// Returns the weapon_id that was removed
int remove_weapon_at(int* inventory, int start_slot) {
    int weapon_id = inventory[start_slot];
    if (weapon_id == WEAPON_NONE) return WEAPON_NONE;
    int slots = weapon_slot_size[weapon_id];
    for (int i = start_slot; i < start_slot + slots; i++) {
        inventory[i] = WEAPON_NONE;
    }
    return weapon_id;
}
```

### 12.5 Main Allocator Function: Pick Up Weapon

```cpp
// Attempt to add weapon_id to the player's inventory.
// If no space: swap out minimum weapons to long-term storage.
// Returns 1 on success, 0 if impossible (e.g., weapon too big for 20 slots).
int inventory_add_weapon(SharedGameState* shm, int player_slot, int weapon_id) {
    Entity* p = &shm->players[player_slot];
    int needed = weapon_slot_size[weapon_id];

    if (needed > INVENTORY_SLOTS) return 0;  // weapon bigger than inventory — impossible

    // ── Step 1: Try to find contiguous space directly ────────────────────
    int start = find_contiguous_free(p->inventory, needed);
    if (start != -1) {
        place_weapon(p->inventory, start, weapon_id);
        update_artifact_flags(p, weapon_id, 1);
        return 1;
    }

    // ── Step 2: No contiguous space — need to swap out weapons ──────────
    // Strategy: scan left to right. Build a candidate region by marking
    // weapons for eviction until we accumulate enough contiguous space.

    // Find the best eviction strategy: minimum weapons to swap out
    // We use a sliding-window approach:
    int best_evict_start  = -1;
    int best_evict_end    = -1;
    int best_evict_count  = INT_MAX;

    for (int window_start = 0; window_start <= INVENTORY_SLOTS - needed; window_start++) {
        // Check if evicting weapons in [window_start, window_start+needed) works
        int evict_count = 0;
        int weapon_ids_in_window[INVENTORY_SLOTS];
        int wcount = 0;

        // Count distinct weapons occupying this window
        for (int i = window_start; i < window_start + needed; i++) {
            if (p->inventory[i] != WEAPON_NONE) {
                // Check if already counted this weapon instance
                int already = 0;
                for (int j = window_start; j < i; j++) {
                    if (p->inventory[j] == p->inventory[i] &&
                        p->inventory[j] != WEAPON_NONE) { already = 1; break; }
                }
                if (!already) { evict_count++; }
            }
        }

        if (evict_count < best_evict_count) {
            best_evict_count  = evict_count;
            best_evict_start  = window_start;
            best_evict_end    = window_start + needed - 1;
        }
    }

    if (best_evict_count == INT_MAX) return 0;  // impossible

    // ── Step 3: Evict the minimum weapons ────────────────────────────────
    // Evict all weapons that overlap the best window
    int i = best_evict_start;
    while (i <= best_evict_end) {
        if (p->inventory[i] != WEAPON_NONE) {
            // Find the true start of this weapon instance
            int weapon_start = i;
            while (weapon_start > 0 && p->inventory[weapon_start-1] == p->inventory[i])
                weapon_start--;

            int evicted_id = remove_weapon_at(p->inventory, weapon_start);
            lt_storage_push(p, evicted_id);   // move to long-term storage
            update_artifact_flags(p, evicted_id, 0);

            char log_msg[ACTION_LOG_WIDTH];
            snprintf(log_msg, ACTION_LOG_WIDTH,
                     "Player %d swapped %s to long-term storage",
                     player_slot, weapon_name[evicted_id]);
            // Note: append_action_log needs shm — pass shm or make it global/context
        }
        i++;
    }

    // ── Step 4: Now place the weapon ─────────────────────────────────────
    start = find_contiguous_free(p->inventory, needed);
    if (start == -1) return 0;   // should not happen after eviction

    place_weapon(p->inventory, start, weapon_id);
    update_artifact_flags(p, weapon_id, 1);
    return 1;
}

void update_artifact_flags(Entity* p, int weapon_id, int adding) {
    if (weapon_id == WEAPON_SOLAR_CORE)  p->holds_solar_core  = adding ? 1 : 0;
    if (weapon_id == WEAPON_LUNAR_BLADE) p->holds_lunar_blade = adding ? 1 : 0;
}

void lt_storage_push(Entity* p, int weapon_id) {
    if (p->lt_storage_count < MAX_LT_STORAGE) {
        p->lt_storage[p->lt_storage_count++] = weapon_id;
    }
}
```

### 12.6 Solar Core + Lunar Blade Hard Constraint

If a player holds both Solar Core (10 slots) and Lunar Blade (10 slots), the inventory is completely full. Any attempt to add another weapon will trigger the swap-out. The allocator handles this automatically since there is no contiguous free space — it will evict one of the artifacts to long-term storage to make room. The `holds_solar_core` / `holds_lunar_blade` flags update accordingly, removing Ultimate Ability eligibility.

**Phase 12 is complete when:** Weapons are placed in contiguous slots, fragmented inventory triggers minimum eviction to long-term storage, Solar+Lunar fills all 20 slots, and Splinter Stick fragmentation scenarios are handled correctly.

---

## Phase 13 — Weapon Drops, Pickups & Swap In Action

### 13.1 What This Phase Accomplishes

When an NPC dies, a random weapon may drop. The player must be given a choice to pick it up. If they don't, an NPC is guaranteed to pick it up. The Swap In action lets players retrieve weapons from long-term storage. This phase implements the complete weapon economy.

**Marks covered:** Weapon Drop Mechanics (5), Long-Term Storage Retrieval (5)

### 13.2 Weapon Drop Flow

```cpp
void present_weapon_drop_to_player(SharedGameState* shm, int dropped_weapon_id) {
    // Notify HIP via shared memory that a weapon drop choice is pending
    pthread_mutex_lock(&shm->master_mutex);
    shm->dropped_weapon_id            = dropped_weapon_id;
    shm->drop_awaiting_player_choice  = 1;
    pthread_mutex_unlock(&shm->master_mutex);

    // HIP will read this flag on the next turn of any player and display the prompt
    // (Or display it immediately via the action log which the rendering thread shows)
}

// In HIP — called at the start of any player's turn if drop is pending
void handle_weapon_drop_choice(SharedGameState* shm, int player_slot) {
    if (!shm->drop_awaiting_player_choice) return;

    int wid = shm->dropped_weapon_id;
    std::cout << "\n*** WEAPON DROPPED: " << weapon_name[wid]
              << " (" << weapon_slot_size[wid] << " slots, "
              << weapon_damage[wid] << " dmg) ***" << std::endl;
    std::cout << "Pick it up? (1=Yes, 0=No): ";
    int choice;
    std::cin >> choice;

    pthread_mutex_lock(&shm->master_mutex);
    shm->drop_awaiting_player_choice = 0;

    if (choice == 1) {
        // Player picks it up — add to their inventory via allocator
        pthread_mutex_unlock(&shm->master_mutex);
        inventory_add_weapon(shm, player_slot, wid);
        shm->dropped_weapon_id = WEAPON_NONE;
    } else {
        // Player declines — NPC is guaranteed to pick it up
        // Find a random alive NPC and give it the weapon
        int npc_slot = -1;
        for (int i = 0; i < shm->num_npcs_concurrent; i++) {
            if (shm->npcs[i].is_alive) { npc_slot = i; break; }
        }
        if (npc_slot != -1) {
            // Give weapon to NPC inventory (simplified — NPCs have same struct)
            int start = find_contiguous_free(shm->npcs[npc_slot].inventory,
                                             weapon_slot_size[wid]);
            if (start != -1) {
                place_weapon(shm->npcs[npc_slot].inventory, start, wid);
            }
        }
        shm->dropped_weapon_id = WEAPON_NONE;
        pthread_mutex_unlock(&shm->master_mutex);
        append_action_log(shm, "NPC picked up the dropped weapon");
    }
}
```

### 13.3 Swap In Action

```cpp
void swap_in_weapon(SharedGameState* shm, int player_slot, int weapon_id_to_swap_in) {
    Entity* p = &shm->players[player_slot];

    // 1. Remove weapon from long-term storage
    int found = -1;
    for (int i = 0; i < p->lt_storage_count; i++) {
        if (p->lt_storage[i] == weapon_id_to_swap_in) { found = i; break; }
    }
    if (found == -1) return;  // weapon not found in long-term storage

    // Remove from lt_storage by shifting
    for (int i = found; i < p->lt_storage_count - 1; i++) {
        p->lt_storage[i] = p->lt_storage[i+1];
    }
    p->lt_storage[--p->lt_storage_count] = WEAPON_NONE;

    // 2. Add to primary inventory (allocator handles eviction if needed)
    inventory_add_weapon(shm, player_slot, weapon_id_to_swap_in);

    // 3. Weapon cannot be used this turn — enforced in HIP by not showing it
    //    in the "Use Weapon" menu until the next turn.
    //    Simple implementation: set a per-weapon "just_swapped_in" flag.
    //    Or: track which slot was just added this turn.
}
```

**Phase 13 is complete when:** Weapon drops prompt player choice, declining guarantees NPC pickup, Swap In retrieves from long-term storage and uses the allocator, and the swapped-in weapon cannot be used until the next turn.

---

## Phase 14 — Global Resource Table & Artifact Locking

### 14.1 What This Phase Accomplishes

The Global Resource Table tracks the Solar Core, Lunar Blade, and Eclipse Relic. Before acquiring or releasing any artifact, a process must lock the table, update it atomically, and unlock. This phase implements the artifact acquisition protocol.

**Marks covered:** Global Resource Table (10), Resource Locking Mechanism (5), Eclipse Relic Handling (5)

### 14.2 Artifact Acquisition

```cpp
// Returns 1 if successfully acquired, 0 if already held by someone else
int acquire_artifact(SharedGameState* shm, int entity_id, int artifact_weapon_id) {
    GlobalResourceTable* rt = &shm->resources;

    pthread_mutex_lock(&rt->table_mutex);

    int* holder = NULL;
    if (artifact_weapon_id == WEAPON_SOLAR_CORE)  holder = &rt->solar_core_holder;
    if (artifact_weapon_id == WEAPON_LUNAR_BLADE) holder = &rt->lunar_blade_holder;
    if (artifact_weapon_id == WEAPON_NONE)        { pthread_mutex_unlock(&rt->table_mutex); return 0; }

    // Eclipse Relic
    if (rt->eclipse_relic_in_game && artifact_weapon_id == /* eclipse_relic_id */ 8)
        holder = &rt->eclipse_relic_holder;

    if (*holder == -1) {
        // Free — acquire it
        *holder = entity_id;
        pthread_mutex_unlock(&rt->table_mutex);
        return 1;
    } else {
        // Held by someone else — mark as waiting (for deadlock detection)
        // Find this entity and set its waiting_for_resource field
        // (Arbiter handles this since it knows all entity states)
        pthread_mutex_unlock(&rt->table_mutex);
        return 0;
    }
}

void release_artifact(SharedGameState* shm, int entity_id, int artifact_weapon_id) {
    GlobalResourceTable* rt = &shm->resources;
    pthread_mutex_lock(&rt->table_mutex);

    if (artifact_weapon_id == WEAPON_SOLAR_CORE && rt->solar_core_holder == entity_id)
        rt->solar_core_holder = -1;
    if (artifact_weapon_id == WEAPON_LUNAR_BLADE && rt->lunar_blade_holder == entity_id)
        rt->lunar_blade_holder = -1;

    pthread_mutex_unlock(&rt->table_mutex);
}
```

### 14.3 Eclipse Relic Dynamic Introduction

```cpp
void introduce_eclipse_relic(SharedGameState* shm) {
    // Called when the relic appears in the environment (e.g., random event)
    pthread_mutex_lock(&shm->resources.table_mutex);
    shm->resources.eclipse_relic_in_game  = 1;
    shm->resources.eclipse_relic_holder   = -1;  // lying free on battlefield
    pthread_mutex_unlock(&shm->resources.table_mutex);
    append_action_log(shm, "*** Eclipse Relic has appeared on the battlefield! ***");
}
```

**Phase 14 is complete when:** Artifact acquisition locks the table, updates the holder, and unlocks. Multiple threads/processes cannot acquire the same artifact simultaneously. Eclipse Relic is tracked in the table.

---

## Phase 15 — Deadlock Detection & Resolution Background Thread

### 15.1 What This Phase Accomplishes

A background thread in the Arbiter continuously scans the `waiting_for_resource` field of all entities against the `GlobalResourceTable` to detect circular waits. When a deadlock is found, it forces one entity to release an artifact.

**Marks covered:** Deadlock Detection — Circular Wait (10), Deadlock Resolution (10)

### 15.2 Deadlock Detection Algorithm

```cpp
// Build a wait-for graph and detect cycles using DFS
// Nodes = entity IDs, Edges = "entity A waits for something held by entity B"

typedef struct { int from; int to; } Edge;

int build_wait_for_graph(SharedGameState* shm, Edge* edges, int max_edges) {
    int edge_count = 0;
    GlobalResourceTable* rt = &shm->resources;

    pthread_mutex_lock(&shm->master_mutex);
    pthread_mutex_lock(&rt->table_mutex);

    // Check all entities for waiting_for_resource
    auto check_entity = [&](Entity* e) {
        if (!e->is_alive) return;
        int waiting_for = e->waiting_for_resource;
        if (waiting_for == WEAPON_NONE) return;

        // Who holds the resource e is waiting for?
        int holder = -1;
        if (waiting_for == WEAPON_SOLAR_CORE)  holder = rt->solar_core_holder;
        if (waiting_for == WEAPON_LUNAR_BLADE) holder = rt->lunar_blade_holder;

        if (holder != -1 && holder != e->id && edge_count < max_edges) {
            edges[edge_count].from = e->id;
            edges[edge_count].to   = holder;
            edge_count++;
        }
    };

    for (int i = 0; i < shm->num_players; i++) check_entity(&shm->players[i]);
    for (int i = 0; i < shm->num_npcs_concurrent; i++) check_entity(&shm->npcs[i]);

    pthread_mutex_unlock(&rt->table_mutex);
    pthread_mutex_unlock(&shm->master_mutex);

    return edge_count;
}

// DFS cycle detection — returns the ID of an entity in a cycle, or -1
int detect_cycle(Edge* edges, int edge_count, int* visited, int* in_stack,
                 int node, int* all_nodes, int node_count) {
    visited[node]  = 1;
    in_stack[node] = 1;

    for (int i = 0; i < edge_count; i++) {
        if (edges[i].from == node) {
            int next = edges[i].to;
            if (!visited[next]) {
                int result = detect_cycle(edges, edge_count, visited, in_stack,
                                          next, all_nodes, node_count);
                if (result != -1) return result;
            } else if (in_stack[next]) {
                return next;  // cycle found — return a node in the cycle
            }
        }
    }
    in_stack[node] = 0;
    return -1;
}

void* deadlock_monitor_thread(void* arg) {
    SharedGameState* shm = (SharedGameState*)arg;

    while (shm->game_result == GAME_ONGOING) {
        sleep(1);  // check for deadlock every second

        Edge edges[64];
        int edge_count = build_wait_for_graph(shm, edges, 64);

        if (edge_count == 0) continue;

        // Collect all unique node IDs
        int all_nodes[64]; int node_count = 0;
        for (int i = 0; i < edge_count; i++) {
            bool found_from = false, found_to = false;
            for (int j = 0; j < node_count; j++) {
                if (all_nodes[j] == edges[i].from) found_from = true;
                if (all_nodes[j] == edges[i].to)   found_to   = true;
            }
            if (!found_from) all_nodes[node_count++] = edges[i].from;
            if (!found_to)   all_nodes[node_count++] = edges[i].to;
        }

        int visited[256]  = {0};
        int in_stack[256] = {0};
        int deadlock_node = -1;

        for (int i = 0; i < node_count && deadlock_node == -1; i++) {
            if (!visited[all_nodes[i]]) {
                deadlock_node = detect_cycle(edges, edge_count, visited, in_stack,
                                             all_nodes[i], all_nodes, node_count);
            }
        }

        if (deadlock_node != -1) {
            // ── Deadlock detected — force release ────────────────────────
            append_action_log(shm, "[DEADLOCK DETECTED] Forcing resource release...");

            // Find the entity in the cycle and force it to release its held resource
            pthread_mutex_lock(&shm->resources.table_mutex);
            if (shm->resources.solar_core_holder == deadlock_node) {
                shm->resources.solar_core_holder = -1;
                append_action_log(shm, "[DEADLOCK RESOLVED] Solar Core forcibly released");
            } else if (shm->resources.lunar_blade_holder == deadlock_node) {
                shm->resources.lunar_blade_holder = -1;
                append_action_log(shm, "[DEADLOCK RESOLVED] Lunar Blade forcibly released");
            }
            pthread_mutex_unlock(&shm->resources.table_mutex);

            // Clear the waiting_for_resource for the forced entity
            pthread_mutex_lock(&shm->master_mutex);
            for (int i = 0; i < shm->num_players; i++) {
                if (shm->players[i].id == deadlock_node)
                    shm->players[i].waiting_for_resource = WEAPON_NONE;
            }
            for (int i = 0; i < shm->num_npcs_concurrent; i++) {
                if (shm->npcs[i].id == deadlock_node)
                    shm->npcs[i].waiting_for_resource = WEAPON_NONE;
            }
            pthread_mutex_unlock(&shm->master_mutex);
        }
    }
    return NULL;
}
```

**Phase 15 is complete when:** The background thread detects circular waits, forces a release, logs the event, and the game continues without hanging.

---

## Phase 16 — Rendering Thread: Real-Time TUI/GUI

### 16.1 What This Phase Accomplishes

A dedicated rendering thread in the Arbiter reads from shared memory and draws the game UI. The UI must show real-time stamina bars, HP stats, and an action log. The rendering thread must not block the game loop — it uses non-blocking reads (snapshot pattern with try-lock or regular lock held briefly).

**Marks covered:** Dedicated Rendering Thread (5), Safe Shared Memory Reads (5), Real-Time Visualization (10), Non-Blocking UI (10)

### 16.2 ncurses Setup and Layout

```cpp
#include <ncurses.h>

// Recommended screen layout:
//
// ┌──────────────────────────────────────────────────────────────────┐
// │                        CHRONO RIFT                               │
// ├────────────────────────────┬─────────────────────────────────────┤
// │   PLAYER PARTY             │   ENEMIES                           │
// │                            │                                     │
// │  P0: ████████░░  HP 450/900│  NPC2: ██░░░░░░░░ HP 89/112        │
// │      Stamina: ████████████ │         Stamina: ██████░░░░░░       │
// │      Inventory: [Solar][..] │                                    │
// │                            │  NPC5: ████░░░░░░ HP 120/150       │
// │  P1: ██████░░░░ HP 800/1200│         Stamina: ████████████       │
// │      Stamina: ████░░░░░░░░ │                                     │
// │                            │                                     │
// ├────────────────────────────┴─────────────────────────────────────┤
// │   ACTION LOG                                                      │
// │  > Player 0 STRIKES NPC 2 for 13 dmg (HP: 89/102)               │
// │  > NPC 5 STRIKES Player 1 for 12 dmg (HP: 800/1200)             │
// │  > Player 1 SKIPs (stamina → 50)                                 │
// └──────────────────────────────────────────────────────────────────┘

WINDOW* main_win;
WINDOW* player_win;
WINDOW* enemy_win;
WINDOW* log_win;

void init_ncurses_ui() {
    initscr();
    cbreak();
    noecho();
    curs_set(0);           // hide cursor
    keypad(stdscr, TRUE);

    // Initialize color pairs if terminal supports colors
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_GREEN,  COLOR_BLACK);  // stamina bar
        init_pair(2, COLOR_RED,    COLOR_BLACK);  // HP bar
        init_pair(3, COLOR_YELLOW, COLOR_BLACK);  // headers
        init_pair(4, COLOR_CYAN,   COLOR_BLACK);  // NPC
        init_pair(5, COLOR_WHITE,  COLOR_BLACK);  // log
    }
}
```

### 16.3 Rendering Thread Function

```cpp
void* render_thread_func(void* arg) {
    SharedGameState* shm = (SharedGameState*)arg;
    init_ncurses_ui();

    int screen_rows, screen_cols;
    getmaxyx(stdscr, screen_rows, screen_cols);

    while (shm->game_result == GAME_ONGOING) {
        // ── Snapshot shared state safely ─────────────────────────────────
        // Take a quick lock to snapshot state, then release before drawing
        Entity players_snap[MAX_PLAYERS];
        Entity npcs_snap[MAX_NPCS];
        char log_snap[ACTION_LOG_LINES][ACTION_LOG_WIDTH];
        int log_count_snap, log_head_snap;
        int kills_snap;

        pthread_mutex_lock(&shm->master_mutex);
        memcpy(players_snap, shm->players, sizeof(players_snap));
        memcpy(npcs_snap,    shm->npcs,    sizeof(npcs_snap));
        kills_snap = shm->total_kills;
        pthread_mutex_unlock(&shm->master_mutex);

        pthread_mutex_lock(&shm->log_mutex);
        memcpy(log_snap, shm->action_log, sizeof(log_snap));
        log_count_snap = shm->log_count;
        log_head_snap  = shm->log_head;
        pthread_mutex_unlock(&shm->log_mutex);

        // ── Draw everything using snapshot (no lock held) ─────────────────
        clear();

        // Title
        attron(COLOR_PAIR(3) | A_BOLD);
        mvprintw(0, screen_cols/2 - 6, "⚔ CHRONO RIFT ⚔");
        mvprintw(1, screen_cols/2 - 10, "Kills: %d/10", kills_snap);
        attroff(COLOR_PAIR(3) | A_BOLD);

        // Players section
        int row = 3;
        attron(COLOR_PAIR(3));
        mvprintw(row++, 2, "── PLAYER PARTY ──");
        attroff(COLOR_PAIR(3));

        for (int i = 0; i < shm->num_players; i++) {
            Entity* p = &players_snap[i];
            if (!p->is_alive) {
                mvprintw(row++, 2, "P%d: [DEAD]", i);
                continue;
            }

            // HP bar
            float hp_pct  = (float)p->hp / p->max_hp;
            int   hp_bars = (int)(hp_pct * 12);
            mvprintw(row, 2, "P%d: ", i);
            attron(COLOR_PAIR(2));
            for (int b = 0; b < 12; b++) printw("%s", b < hp_bars ? "█" : "░");
            attroff(COLOR_PAIR(2));
            printw(" HP %d/%d%s", p->hp, p->max_hp, p->is_stunned ? " [STUNNED]" : "");
            row++;

            // Stamina bar
            float st_pct  = p->stamina / p->max_stamina;
            int   st_bars = (int)(st_pct * 12);
            mvprintw(row, 6, "Stamina: ");
            attron(COLOR_PAIR(1));
            for (int b = 0; b < 12; b++) printw("%s", b < st_bars ? "█" : "░");
            attroff(COLOR_PAIR(1));
            printw(" %.0f/100", p->stamina);
            row++;

            // Inventory summary (first few weapons)
            mvprintw(row, 6, "Inv: ");
            for (int s = 0; s < INVENTORY_SLOTS; s++) {
                if (p->inventory[s] != WEAPON_NONE &&
                    (s == 0 || p->inventory[s] != p->inventory[s-1])) {
                    printw("[%s] ", weapon_name[p->inventory[s]]);
                }
            }
            row += 2;
        }

        // Enemies section
        int enemy_col = screen_cols / 2;
        row = 3;
        attron(COLOR_PAIR(4));
        mvprintw(row++, enemy_col, "── ENEMIES ──");
        attroff(COLOR_PAIR(4));

        for (int i = 0; i < shm->num_npcs_concurrent; i++) {
            Entity* n = &npcs_snap[i];
            if (!n->is_alive) continue;

            float hp_pct  = (float)n->hp / n->max_hp;
            int   hp_bars = (int)(hp_pct * 10);
            mvprintw(row, enemy_col, "NPC%d: ", n->id);
            attron(COLOR_PAIR(2));
            for (int b = 0; b < 10; b++) printw("%s", b < hp_bars ? "█" : "░");
            attroff(COLOR_PAIR(2));
            printw(" HP %d/%d%s", n->hp, n->max_hp, n->is_stunned ? " [STUNNED]" : "");
            row++;

            float st_pct  = n->stamina / n->max_stamina;
            int   st_bars = (int)(st_pct * 10);
            mvprintw(row, enemy_col + 6, "Stm: ");
            attron(COLOR_PAIR(1));
            for (int b = 0; b < 10; b++) printw("%s", b < st_bars ? "█" : "░");
            attroff(COLOR_PAIR(1));
            printw(" %.0f/150", n->stamina);
            row += 2;
        }

        // Action log section
        int log_start_row = screen_rows - ACTION_LOG_LINES - 2;
        mvprintw(log_start_row, 2, "── ACTION LOG ──────────────────────────────────────────");
        for (int i = 0; i < log_count_snap && i < ACTION_LOG_LINES; i++) {
            int idx = (log_head_snap + i) % ACTION_LOG_LINES;
            if (strlen(log_snap[idx]) > 0) {
                mvprintw(log_start_row + 1 + i, 2, "> %s", log_snap[idx]);
            }
        }

        refresh();
        usleep(100000);  // refresh 10 times per second
    }

    // Display game over screen
    clear();
    if (shm->game_result == GAME_WIN) {
        mvprintw(screen_rows/2, screen_cols/2 - 8, "*** VICTORY! 10 enemies defeated! ***");
    } else if (shm->game_result == GAME_LOSE) {
        mvprintw(screen_rows/2, screen_cols/2 - 10, "*** DEFEAT! All players have fallen! ***");
    } else {
        mvprintw(screen_rows/2, screen_cols/2 - 5, "*** Game exited. ***");
    }
    refresh();
    sleep(3);
    endwin();
    return NULL;
}
```

### 16.4 Action Log Append Function

```cpp
void append_action_log(SharedGameState* shm, const char* message) {
    pthread_mutex_lock(&shm->log_mutex);
    int slot = (shm->log_head + shm->log_count) % ACTION_LOG_LINES;
    if (shm->log_count < ACTION_LOG_LINES) {
        shm->log_count++;
    } else {
        // Log is full — overwrite oldest (advance head)
        shm->log_head = (shm->log_head + 1) % ACTION_LOG_LINES;
        slot = (shm->log_head + shm->log_count - 1) % ACTION_LOG_LINES;
    }
    strncpy(shm->action_log[slot], message, ACTION_LOG_WIDTH - 1);
    shm->action_log[slot][ACTION_LOG_WIDTH - 1] = '\0';
    pthread_mutex_unlock(&shm->log_mutex);
}
```

**Phase 16 is complete when:** A separate thread updates the ncurses display at 10 FPS, stamina bars update in real-time, HP decreases are visible immediately after attacks, and the action log scrolls correctly. The rendering thread never holds locks for more than a brief snapshot copy.

---

## Phase 17 — Game Conditions, Process Lifecycle & Graceful Shutdown

### 17.1 What This Phase Accomplishes

The game ends when: 10 enemies are killed (Win), all players die (Lose), or the player sends SIGTERM (Quit). All processes must shut down cleanly: threads joined, shared memory unlinked, semaphores destroyed.

**Marks covered:** Win/Lose/Quit Conditions (10), Process Lifecycle Management (10)

### 17.2 Condition Checking

```cpp
void check_game_conditions(SharedGameState* shm) {
    pthread_mutex_lock(&shm->master_mutex);

    // Win condition: 10 total kills
    if (shm->total_kills >= 10 && shm->game_result == GAME_ONGOING) {
        shm->game_result = GAME_WIN;
        pthread_mutex_unlock(&shm->master_mutex);
        append_action_log(shm, "*** VICTORY! 10 enemies defeated! ***");
        return;
    }

    // Lose condition: all players dead
    int alive_players = 0;
    for (int i = 0; i < shm->num_players; i++) {
        if (shm->players[i].is_alive) alive_players++;
    }
    if (alive_players == 0 && shm->game_result == GAME_ONGOING) {
        shm->game_result = GAME_LOSE;
        pthread_mutex_unlock(&shm->master_mutex);
        append_action_log(shm, "*** DEFEAT! All players have fallen! ***");
        return;
    }

    pthread_mutex_unlock(&shm->master_mutex);
}
```

### 17.3 Graceful Shutdown Sequence

```cpp
void graceful_shutdown(SharedGameState* shm,
                        pthread_t deadlock_thread,
                        pthread_t render_thread,
                        pthread_t tick_thread) {
    std::cout << "[ARBITER] Initiating graceful shutdown..." << std::endl;

    // 1. Post all semaphores to unblock waiting threads
    sem_post(&shm->turn.turn_notification);
    sem_post(&shm->turn.action_submitted);

    // 2. Signal HIP and ASP to terminate
    if (shm->hip_pid > 0) kill(shm->hip_pid, SIGTERM);
    if (shm->asp_pid > 0) kill(shm->asp_pid, SIGTERM);

    // 3. Join background threads
    pthread_join(deadlock_thread, NULL);
    pthread_join(render_thread,   NULL);
    pthread_join(tick_thread,     NULL);

    // 4. Wait for child processes
    // (if HIP and ASP were fork()ed by Arbiter, waitpid here)
    // If they were started by run.sh, skip this

    // 5. Destroy shared memory (Phase 3 function)
    destroy_shared_memory(shm);

    std::cout << "[ARBITER] Shutdown complete." << std::endl;
}
```

### 17.4 HIP and ASP Handling SIGTERM

Both HIP and ASP install a SIGTERM handler:

```cpp
static volatile sig_atomic_t should_exit = 0;

void hip_sigterm_handler(int sig) {
    should_exit = 1;
    // Post the turn_notification semaphore to unblock any waiting thread
    // (need access to shm — use global pointer)
    if (g_shm) sem_post(&g_shm->turn.turn_notification);
}
```

In each player/NPC thread, check `should_exit` at the start of the loop:

```cpp
while (!should_exit && shm->game_result == GAME_ONGOING) {
    // ... thread loop
}
```

**Phase 17 is complete when:** All three exit conditions terminate the game cleanly, all threads are joined, shared memory is unlinked, and no zombie processes or lingering semaphores remain.

---

## Phase 18 — Integration Testing, Report & Submission

### 18.1 Integration Test Checklist

Run through every major system and verify correctness:

**Process Architecture:**
- [ ] Three separate processes (`arbiter`, `hip`, `asp`) confirmed via `ps aux` while game runs
- [ ] No pipes used (verify with `strace` or code review)
- [ ] HIP does NOT directly modify entity stats — only writes to action fields

**Threading:**
- [ ] `ps -L` or `/proc/PID/task` shows multiple threads in HIP and ASP
- [ ] Only one player thread active when it's that player's turn
- [ ] Each NPC has its own thread (confirm by adding debug prints from thread IDs)

**Scheduling:**
- [ ] Verify arrival time formula: entity with speed=50, max stamina=100 → acts every 2 seconds
- [ ] Entity with lower speed acts less frequently
- [ ] Skip gives 50% stamina → acts sooner than full reset but not immediately

**Signals:**
- [ ] Sending SIGUSR1 to HIP while a player thread is active causes 3-second pause
- [ ] Stamina of stunned entity does not change during stun (not stunned = false, tick skips them)
- [ ] Ultimate Ability stops ASP for exactly 10 seconds (test with `time` command)
- [ ] SIGTERM from HIP causes clean shutdown

**Memory Management:**
- [ ] Picking up Solar Core (10 slots) + Lunar Blade (10 slots) → inventory full
- [ ] Adding any 3rd weapon triggers eviction to long-term storage
- [ ] Splinter Stick (2 slots) fragmentation: alternate Splinter Stick + free + Splinter Stick... then try to add 7-slot Iron Halberd → eviction occurs
- [ ] Swap In correctly retrieves from long-term storage

**Deadlock:**
- [ ] Manually create circular wait scenario (modify code to force both player and NPC to want both artifacts)
- [ ] Background thread detects and resolves within 1–2 seconds

**Rendering:**
- [ ] Stamina bars update visibly in real-time
- [ ] HP decreases after attacks are immediately reflected
- [ ] Action log scrolls with each new event
- [ ] Rendering thread does NOT cause the game to slow down or stutter

**Game Conditions:**
- [ ] Killing 10th enemy triggers WIN and clean exit
- [ ] All players dying triggers LOSE and clean exit
- [ ] Selecting quit option triggers SIGTERM → GAME_QUIT → clean exit

### 18.2 Report Structure (report.pdf)

#### Section 1: Architecture Overview
- Diagram of three processes and how they communicate via shared memory.
- Thread map for each process.

#### Section 2: Turnaround Time Analysis
Use the formula `Arrival Time = Max Stamina / Speed` and show worked examples:

| Entity | Speed | Max Stamina | Arrival Time (first turn) |
|---|---|---|---|
| Player (1-person party) | 100 | 100 | 1.0 seconds |
| Player (4-person party) | 25 | 100 | 4.0 seconds |
| NPC (speed=10) | 10 | 150 | 15.0 seconds |
| NPC (speed=30) | 30 | 150 | 5.0 seconds |

Show that your implementation matches these exactly.

#### Section 3: Roll Number Seed Analysis
Show how your specific roll number produces specific entity stats. Example:
- Roll: `24i-0456` → last digit = 6, second last = 5, last two = 56
- Player damage = 6 + 10 = 16
- Enemy damage = 5 + 10 = 15
- Enemy HP = 56 + random(50-200) = 56 to 256

#### Section 4: Memory Allocator Cases
Show worked examples of:
- Normal allocation (empty space available)
- Fragmentation case (Splinter Stick scenario)
- Full inventory eviction (Solar Core + Lunar Blade)

#### Section 5: Deadlock Scenario
Draw the wait-for graph for a detected deadlock case and show how your algorithm resolves it.

#### Section 6: Screenshots
- Normal gameplay (stamina bars updating, action log filling)
- Stun mechanic (stunned entity with counter)
- Ultimate Ability (ASP suspended message)
- Win and Lose screens

### 18.3 Final Submission Structure

```
BCS A_24i_XXXX_YourName.zip
└── BCS A_24i_XXXX_YourName/
    ├── Dockerfile
    ├── Makefile
    ├── requirements.txt
    ├── arbiter/
    │   ├── arbiter.cpp
    │   ├── shm_manager.cpp
    │   ├── scheduler.cpp
    │   ├── action_handler.cpp
    │   ├── signal_handler.cpp
    │   ├── deadlock_monitor.cpp
    │   ├── render_thread.cpp
    │   ├── inventory_allocator.cpp
    │   ├── weapon_data.cpp
    │   └── shared_types.h
    ├── hip/
    │   ├── hip.cpp
    │   ├── shm_client.cpp
    │   ├── input_handler.cpp
    │   ├── weapon_data.cpp
    │   └── shared_types.h
    ├── asp/
    │   ├── asp.cpp
    │   ├── shm_client.cpp
    │   ├── npc_ai.cpp
    │   ├── weapon_data.cpp
    │   └── shared_types.h
    └── report.pdf
```

### 18.4 Pre-Submission Verification

```bash
# Inside Docker container:

# 1. Clean build from scratch
make clean && make

# 2. Confirm three executables exist and have no pre-compiled components
ls -la arbiter hip asp
file arbiter   # Should show: ELF 64-bit LSB executable

# 3. Confirm no pre-compiled binaries were included
find . -name "*.o" -o -name "*.a" -o -name "*.so"   # Should return nothing

# 4. Full game run test
./arbiter & sleep 0.5 && ./hip & ./asp &
# Play through a complete game — Win condition
wait

# 5. Valgrind memory check (optional but impressive)
valgrind --leak-check=full ./arbiter &
# Check for memory leaks

# 6. Verify folder name is exactly correct
# BCS Section_RollNumber_FullName
```

---

## Summary: Phase Dependencies

```
Phase 1  →  Phase 2  →  Phase 3  →  Phase 4
                                        ↓
                               Phase 5 ← Phase 6
                                        ↓
                                    Phase 7 (Scheduler)
                                        ↓
                               Phase 8 ← Phase 9
                                        ↓
                              Phase 10 ← Phase 11 (Signals)
                                        ↓
                              Phase 12 → Phase 13 (Inventory)
                                        ↓
                              Phase 14 → Phase 15 (Deadlock)
                                        ↓
                                    Phase 16 (Rendering)
                                        ↓
                                    Phase 17 (Shutdown)
                                        ↓
                                    Phase 18 (Testing + Report)
```

Every phase builds on those before it. Do not attempt Phase 7 without Phase 3 working correctly — shared memory bugs will cascade and become impossible to debug later.

---

*Deadline: 10 May 2026, 23:59 PST | Total Marks: 390 (360 core + 30 bonus)*
*Language: C or C++ | Environment: Ubuntu 22.04 inside Docker | Seed: Your Roll Number*
