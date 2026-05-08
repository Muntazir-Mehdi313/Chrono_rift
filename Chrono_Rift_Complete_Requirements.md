# Chrono Rift — Complete Project Requirements & Technical Reference
## CS 2006 Operating Systems | Spring 2026 | BS-CS | NUCES

---

> **This document is a comprehensive single-source-of-truth for all requirements, constraints, marks breakdown, workflow guidance, classroom clarifications (GCR), and Docker/build setup for the Chrono Rift OS project. Every minute detail from the project statement, rubric, Docker guide, and GCR comments has been captured here.**

---

## Table of Contents

1. [Quick Summary & Deadlines](#1-quick-summary--deadlines)
2. [Submission Guidelines & Penalties](#2-submission-guidelines--penalties)
3. [Marks Breakdown (Full Rubric)](#3-marks-breakdown-full-rubric)
4. [Game Overview & Concept](#4-game-overview--concept)
5. [Process Architecture (Section 2)](#5-process-architecture-section-2)
6. [Temporal Scheduling & Stamina Logic (Section 3)](#6-temporal-scheduling--stamina-logic-section-3)
7. [Global State & Shared Memory (Section 4)](#7-global-state--shared-memory-section-4)
8. [Signals, Stun Mechanic & Async Interrupts (Section 5)](#8-signals-stun-mechanic--async-interrupts-section-5)
9. [Weapons & Inventory Management (Section 6)](#9-weapons--inventory-management-section-6)
10. [Deadlocks, Artifacts & Resource Management (Section 7)](#10-deadlocks-artifacts--resource-management-section-7)
11. [Ultimate Ability & Pause Mechanic (Section 8)](#11-ultimate-ability--pause-mechanic-section-8)
12. [Mandatory Graphical Interface & Rendering Thread (Section 9)](#12-mandatory-graphical-interface--rendering-thread-section-9)
13. [Gameplay Configuration & All Stats (Section 10)](#13-gameplay-configuration--all-stats-section-10)
14. [Weapon Table & Inventory Rules](#14-weapon-table--inventory-rules)
15. [Win / Lose / Quit Conditions](#15-win--lose--quit-conditions)
16. [Bonus: Multiplayer Extension (Section 11)](#16-bonus-multiplayer-extension-section-11)
17. [Allowed vs. Prohibited Technologies](#17-allowed-vs-prohibited-technologies)
18. [Docker Environment Setup (Full Guide)](#18-docker-environment-setup-full-guide)
19. [Folder Structure & Build System](#19-folder-structure--build-system)
20. [GCR Clarifications (All Official Answers)](#20-gcr-clarifications-all-official-answers)
21. [Implementation Workflow & Architecture Blueprint](#21-implementation-workflow--architecture-blueprint)
22. [Shared Memory Layout Design](#22-shared-memory-layout-design)
23. [Signal Map: All Signals Used](#23-signal-map-all-signals-used)
24. [Inventory Allocator Logic (Memory Management Detail)](#24-inventory-allocator-logic-memory-management-detail)
25. [Deadlock Detection Algorithm](#25-deadlock-detection-algorithm)
26. [Edge Cases & Tricky Requirements](#26-edge-cases--tricky-requirements)
27. [Report Requirements](#27-report-requirements)
28. [Common Mistakes to Avoid](#28-common-mistakes-to-avoid)

---

## 1. Quick Summary & Deadlines

| Item | Detail |
|---|---|
| **Course** | CS 2006 — Operating Systems |
| **Semester** | Spring 2026 |
| **Program** | BS-CS |
| **Institution** | NUCES (FAST) |
| **Project Name** | Chrono Rift |
| **Deadline** | **10 May 2026 at 23:59 PST** |
| **Language** | C or C++ (C++ preferred but both allowed per GCR) |
| **Standard** | C++17 (`-std=c++17`) |
| **Environment** | Ubuntu 22.04 inside Docker (Linux-only syscalls) |
| **Group Size** | 1–2 members (solo discouraged, cross-section forbidden) |
| **Total Marks** | 390 marks (360 core + 30 bonus) |

---

## 2. Submission Guidelines & Penalties

### 2.1 Integrity
- **Zero marks** for all involved parties if any plagiarism or solution sharing is detected.
- Use of AI tools to **circumvent** (bypass understanding of) core OS concepts is strictly prohibited.
- All group members must participate equally.

### 2.2 Strict Deadlines
- **No late submissions** under any circumstances.
- No extensions for technical failures or personal reasons.
- Submit early to avoid last-minute portal issues.

### 2.3 Directory Naming Convention
```
BCS Section_RollNumber_FullName
```
**Example:** `BCS A_24i_0000_AliAhmed`

- **Failure to follow this exact format = automatic 20% mark deduction.**
- For groups of two: the GCR confirms only one submission is needed; the naming only requires one roll number and name.

### 2.4 Deliverables (What Must Be in the .zip)
The final `.zip` file must contain:
- All source files (organized in `arbiter/`, `hip/`, `asp/` folders)
- A functional `Makefile`
- An unchanged `Dockerfile`
- A `requirements.txt` file (can be blank)
- A `report.pdf` including:
  - Turnaround time analysis
  - Gameplay screenshots

### 2.5 Pre-Compiled Binaries
- Pre-compiled binaries are **NOT ALLOWED** and will be deleted before grading.

### 2.6 Programming Standards
- Well-indented code
- Clear comments
- Meaningful variable names
- Proper interpretation of game requirements is **part of the evaluation**

### 2.7 Roll Number Seed
- You **must** use your specific Roll Number as the seed for all randomized elements.
- During the demo, you must explain how this seed influences your game's specific behavior.

### 2.8 Group Policy
- Groups of two are allowed and **preferred**.
- Solo projects are **discouraged** (do not promote teamwork skills) but allowed on a case-by-case review.
- Groups of **more than two** are **not permitted**.
- **Cross-section pairs are forbidden under any circumstances.**

---

## 3. Marks Breakdown (Full Rubric)

Total: **390 marks** (360 core + 30 bonus)

### 3.1 Process Architecture & IPC — 40 Marks

| Functionality | Marks |
|---|---|
| Proper Process Isolation (Arbiter, Human, Strategic) | 10 |
| Correct Use of Shared Memory (No Pipes) | 10 |
| Process Lifecycle Management (creation/termination, cleanup) | 10 |
| Correct Communication Design | 10 |

### 3.2 Synchronization & Concurrency Control — 40 Marks

| Functionality | Marks |
|---|---|
| Mutex/Semaphore Implementation | 10 |
| Race Condition Prevention | 10 |
| Concurrent Access Handling (Threads + Processes) | 10 |
| Shared Memory Consistency Under Load | 10 |

### 3.3 Multithreading & Execution Model — 40 Marks

| Functionality | Marks |
|---|---|
| Thread-per-NPC Implementation | 10 |
| Player Thread Handling (Active/Idle Logic) | 10 |
| Thread Synchronization with Shared Memory | 10 |
| Efficient Thread Scheduling & Coordination | 10 |

### 3.4 Scheduling & Temporal Logic — 40 Marks

| Functionality | Marks |
|---|---|
| Stamina-Based Scheduling Logic | 10 |
| Arrival Time Computation Accuracy | 10 |
| Serial Execution Enforcement | 10 |
| Turn Reset & Recalculation Logic | 10 |

### 3.5 Signals & Asynchronous Interrupts — 40 Marks

| Functionality | Marks |
|---|---|
| Signal-Based Stun Mechanism | 10 |
| Correct Stun Duration (exact 3 seconds) | 5 |
| Non-Blocking Interrupt Handling | 5 |
| Ultimate Ability (Signal-Only Suspension) | 10 |
| Correct Ultimate Duration (exact 10 seconds) | 5 |
| Signal Handling Robustness (SIGALRM, custom handlers) | 5 |

### 3.6 Deadlock Handling & Resource Management — 40 Marks

| Functionality | Marks |
|---|---|
| Global Resource Table Implementation | 10 |
| Resource Locking Mechanism | 5 |
| Deadlock Detection (Circular Wait) | 10 |
| Deadlock Resolution Strategy | 10 |
| Correct Handling of Dynamic Artifact (Eclipse Relic) | 5 |

### 3.7 Memory Management — 40 Marks

| Functionality | Marks |
|---|---|
| Contiguous Allocation Strategy | 10 |
| Fragmentation Handling | 10 |
| Swap In / Swap Out Logic | 10 |
| Long-Term Storage Handling & Retrieval | 5 |
| Correct Enforcement of Slot Constraints (Solar + Lunar = full) | 5 |

### 3.8 Gameplay Logic & Mechanics — 40 Marks

| Functionality | Marks |
|---|---|
| Correct Implementation of Player Actions | 10 |
| Enemy Behavior & Decision Logic | 5 |
| Weapon Drop Mechanics (player choice + NPC pickup rule) | 5 |
| Win / Lose / Quit Conditions (including SIGTERM) | 10 |
| Correct Randomization Using Roll Number Seed | 10 |

### 3.9 Rendering & System Integration — 30 Marks

| Functionality | Marks |
|---|---|
| Dedicated Rendering Thread | 5 |
| Safe Shared Memory Reads (UI) | 5 |
| Real-Time State Visualization | 10 |
| Non-Blocking UI Execution | 10 |

### 3.10 Environment, Build & Constraints — 20 Marks

| Functionality | Marks |
|---|---|
| Runs Successfully in Docker Environment | 10 |
| Correct Dockerfile & Required Structure | 5 |
| Functional Makefile | 5 |

### 3.11 Bonus — 30 Marks

| Functionality | Marks |
|---|---|
| Multiplayer Implementation | 20 |
| Stability & Synchronization in Multiplayer Mode | 10 |

---

## 4. Game Overview & Concept

### 4.1 What Is Chrono Rift?
Chrono Rift is a **multi-process tactical game** where a human player (or players) engages in combat against computer-controlled entities (NPCs). It is modeled after **turn-based combat mechanics** found in classic role-playing games (specifically referencing Chrono Trigger's battle system).

### 4.2 What the Game Is (From GCR Clarification)
- The game is **the combat/battle portion only** — no overworld, no map, no player movement.
- Think of it as being permanently inside one Chrono Trigger battle screen.
- It is a **static battle HUD** with mechanics showing when additional enemies appear.
- Concurrent enemies on screen at any time: **2 to 9** (random per run).
- Total enemies to defeat to win: **10** (so enemies respawn/new ones appear over time).

### 4.3 Core OS Concepts the Game Demonstrates
1. **Process Isolation** — Every core component in its own memory space.
2. **Inter-Process Communication (IPC)** — Via shared memory only (no pipes).
3. **Multithreaded Decision Making** — Each NPC and each player character is a thread.
4. **Resource Coordination** — Shared memory + memory-based synchronization primitives.
5. **Scheduling** — Stamina-based temporal scheduling (arrival time logic).
6. **Signals** — Stun mechanic, Ultimate Ability, SIGTERM for quit, SIGALRM for timeouts.
7. **Deadlock Detection & Resolution** — Circular wait on artifact resources.
8. **Memory Management** — Contiguous slot-based inventory allocation with fragmentation.

---

## 5. Process Architecture (Section 2)

### 5.1 Three Processes

The game is divided into **three separate processes**, each with its own memory space:

```
┌────────────────────────────────────────────────────────┐
│                    GAME ARBITER                        │
│  - Central authority                                   │
│  - Manages global game state                           │
│  - Enforces rules of engagement                        │
│  - Background deadlock detection thread                │
│  - Rendering thread                                    │
│  - SIGALRM handler for Ultimate Ability                │
└────────────────────┬───────────────────────────────────┘
                     │ Shared Memory
          ┌──────────┴──────────┐
          ▼                     ▼
┌──────────────────┐  ┌──────────────────────────┐
│  HUMAN           │  │  AUTOMATED STRATEGIC      │
│  INTERFACING     │  │  PROCESS (ASP)            │
│  PROCESS (HIP)   │  │                           │
│                  │  │  - Decision logic for NPCs│
│  - Multi-threaded│  │  - Multi-threaded         │
│  - 1 thread per  │  │  - 1 thread per NPC       │
│    player char   │  │  - Concurrent NPC threads │
│  - Reads user    │  │  - Proper synchronization │
│    input         │  │    on shared resources    │
│  - Sends actions │  │                           │
│    to Arbiter    │  │                           │
│    via shm only  │  │                           │
└──────────────────┘  └──────────────────────────┘
```

### 5.2 Human Interfacing Process (HIP) — Detailed Requirements

- Must be **multi-threaded**.
- A **separate thread must be created for each player-controlled character** (1 to 4 threads depending on party size).
- Player threads **read input from a buffer** when required.
- At any given time, **only the thread corresponding to the currently active player** (as determined by the Arbiter) should process input.
- **All other player threads must remain idle** (not processing, not blocking the system).
- HIP must **NOT directly modify the global game state**.
- HIP only **communicates player actions to the Arbiter** via shared memory.
- The Arbiter is **solely responsible** for updating the game state.

### 5.3 Automated Strategic Process (ASP) — Detailed Requirements

- Handles all NPC decision-making logic.
- Must be **multi-threaded**.
- **Each NPC (enemy entity) must run in its own dedicated thread** within the ASP.
- The system must support **concurrent execution of multiple NPC threads**.
- **Proper synchronization** must be implemented when accessing shared resources.
- **Implementations where multiple entities are handled sequentially within a single thread will not be accepted.** (This is explicitly called out — each NPC must have its own independent thread.)
- Every individual computer-controlled character must be managed by its **own dedicated thread** to simulate independent agents.

### 5.4 Game Arbiter (Arbiter) — Detailed Requirements

- Acts as the **central authority** for all game state.
- Manages the global state stored in shared memory.
- Enforces all rules of engagement (turn order, stamina, actions).
- Must implement a **background deadlock detection thread**.
- Must implement a **dedicated rendering thread**.
- Must handle `SIGALRM` with a custom handler for the Ultimate Ability window.
- Responsible for handling process lifecycle: tracks queues, closes communication channels immediately when a process (NPC) exits.
- Must update tracking queues and close relevant communication channels immediately upon a process exit.

### 5.5 Process Lifecycle Management

- The game must gracefully handle the **dynamic termination of processes** when an entity is defeated.
- Upon NPC death:
  - The Arbiter must remove the NPC from scheduling queues.
  - The Arbiter must close any relevant communication channels.
  - This must happen **immediately** upon process exit.

---

## 6. Temporal Scheduling & Stamina Logic (Section 3)

### 6.1 Stamina Accumulation

- Every entity has a **speed** attribute and a **stamina** value.
- **Every second**, `speed` is added to the entity's current stamina.
- Stamina accumulates **concurrently** for all characters at the same time.
- An entity **may only be scheduled for a move when its stamina is fully filled** (reaches max stamina).

### 6.2 Arrival Time Formula

```
Arrival Time = Max Stamina / Speed
```

- This gives the number of seconds until an entity can first act (assuming starting from 0 stamina).
- For entities with different stamina levels (e.g., after using Skip), the calculation updates accordingly.

### 6.3 Serial Execution

- While stamina accumulates **concurrently** for all characters, **only one character may perform an action at any given moment** (serial execution of actions).
- The **first entity** to reach full stamina is the one permitted to act.

### 6.4 Action Commitment

1. First entity to reach max stamina gets to act.
2. Once action is committed, that entity's stamina is **depleted to zero**.
3. The scheduler **recalculates** the next execution window for all active participants.
4. Concurrency of stamina accumulation resumes.

### 6.5 Player Stats (Speed & Stamina)

- **Max Stamina (Player):** 100
- **Speed (Player):** `100 / Number of Player Characters`
  - Example: 1 player → speed = 100; 2 players → speed = 50; 4 players → speed = 25.

### 6.6 NPC Stats (Speed & Stamina)

- **Max Stamina (NPC):** 150
- **Speed (NPC):** Random number between **10 and 30**.

### 6.7 Stamina After Actions

| Action | Stamina After |
|---|---|
| Attack (Strike) | 0 |
| Attack (Exhaust) | 0 |
| Use Weapon | 0 |
| Swap In | 0 |
| Heal | 0 |
| Skip (Player) | 50% of max (50) |
| Attack Strike (Enemy) | 0 |
| Skip (Enemy) | 50% of max (75) |

### 6.8 NPC Turn Timeout

- If the ASP does not submit a move for its turn within **3 seconds**, the Arbiter assumes the chosen action was **"Skip"**.
- This prevents the Arbiter from blocking indefinitely.

---

## 7. Global State & Shared Memory (Section 4)

### 7.1 Shared Memory Requirement

- The **global game state must be stored in a shared memory segment** accessible to the Arbiter and all player processes.
- All IPC must go through shared memory — **no pipes allowed** (neither named nor unnamed).

### 7.2 Memory-Based Primitives for Synchronization

- Students must implement synchronization using **memory-based primitives only**:
  - **pthreads mutexes** (`pthread_mutex_t`)
  - **Unnamed POSIX semaphores** (`sem_t` used with `sem_init`, NOT `sem_open`)
- These must be placed **within the shared memory area** so they are accessible across process boundaries.

### 7.3 Data Consistency Requirements

- The state must remain consistent even when:
  - Multiple NPC threads **concurrently** read or write to the shared memory.
  - The human player process simultaneously reads or writes.
- Race conditions must be **provably prevented** (this is evaluated).

### 7.4 Technical Constraint Summary

| Allowed | Prohibited |
|---|---|
| `shm_open` / `mmap` | Unnamed pipes (`pipe()`) |
| `pthread_mutex_t` (in shared mem) | Named pipes (`mkfifo`) |
| `sem_t` with `sem_init` (in shared mem) | `std::thread` |
| `pthreads` | `std::mutex` |
| POSIX signals | `std::atomic` |
| `SIGALRM`, `SIGUSR1` | `std::semaphore` |
| — | Any STL threading/sync wrappers |

### 7.5 What Can Go In Shared Memory

- All game entity state (HP, stamina, speed, status effects, etc.)
- Synchronization primitives (mutexes, semaphores) — must be initialized with `PTHREAD_PROCESS_SHARED` attribute
- Player action buffers (HIP writes here; Arbiter reads)
- NPC action results
- Global resource table (artifact states)
- Inventory state for all characters
- Turn/scheduling state

---

## 8. Signals, Stun Mechanic & Async Interrupts (Section 5)

### 8.1 The Stun Rule

- Certain high-tier attacks can **"Stun"** a target.
- If a player or NPC is successfully stunned:
  - Their execution must be **halted immediately** for **exactly 3 seconds**.
  - If the target's stamina was full at the time of stun, their turn is **skipped**.
  - After the 3-second duration, the target resumes with its **previous stamina level intact** (stamina is NOT reset to 0 by stun).

### 8.2 Non-Blocking Signal Delivery

- The stun interruption must occur **asynchronously**.
- The attacking process must deliver a **signal** that forces the target process to **pause its current logic**.
- The target process must **NOT** actively check a flag or a pipe — it must respond to the signal directly.
- Signal to use: `SIGUSR1` (from Docker guide context) — specifically designed for process-to-process interruption.

### 8.3 Stun Recovery

- After exactly **3 seconds**, the target resumes from the exact point of interruption.
- **Previous stamina level is maintained** (not zeroed out by stun).

### 8.4 Signal Handling Implementation

```c
// Example signal handler for stun
void stun_handler(int sig) {
    // pause execution
    sleep(3);  // stun duration — exactly 3 seconds
    // resume — return from handler restores execution point
}

// Register handler
signal(SIGUSR1, stun_handler);
// OR (preferred):
struct sigaction sa;
sa.sa_handler = stun_handler;
sigemptyset(&sa.sa_mask);
sa.sa_flags = 0;
sigaction(SIGUSR1, &sa, NULL);
```

---

## 9. Weapons & Inventory Management (Section 6)

### 9.1 Inventory Layout

- Player's primary inventory is a **linear array of 20 slots**.
- Weapons occupy **contiguous slots** in this array.
- The space allocator must place weapons at an **appropriate free location with enough contiguous slots**.

### 9.2 Contiguous Allocation

- When a weapon is picked up, the allocator must find a **free contiguous region** large enough for the weapon's slot size.
- If no contiguous region is large enough, the **Out of Space Scenario** applies.

### 9.3 Out of Space Scenario

- If there is not enough **contiguous** space for a weapon:
  - The space allocator must **swap out some weapons** from the primary inventory to long-term storage.
  - The allocator must **only swap out as many weapons as are necessary** (minimum necessary — not more).
  - The freed space must then accommodate the new weapon.

### 9.4 Long-Term Storage

- A separate storage area (can be any data structure) for swapped-out weapons.
- Players can retrieve weapons from long-term storage using the **Swap In** action.

### 9.5 Swap In Action

- Player chooses a weapon from long-term storage to bring back.
- If there is no space in inventory for the swapped-in item: same Out of Space protocol applies.
- **Swap In costs a complete turn** (stamina depleted to 0).
- The retrieved weapon **cannot be used until the next turn**.

### 9.6 Weapon Drop Mechanic (GCR Clarified)

- When a player defeats an enemy: a **random weapon** is dropped (not necessarily the one the NPC was holding).
- **If an NPC holds a weapon, that weapon will NOT be dropped when it dies.** (Section 10 rule)
- Instead, a **random weapon from the game world** is dropped as a reward.
- The player has a choice: **pick it up** or **leave it**.
- If the player does **not pick it up**: an enemy is **guaranteed to pick it up**.
- If an enemy picks up a weapon, that weapon **will not be dropped** when that enemy dies either.

### 9.7 Player May Hold Copies of Same Weapon

- A player **may hold multiple copies** of the same weapon.

### 9.8 Fragmentation Handling

- The **Splinter Stick** (2 slots) is intentionally designed to create **fragmentation edge cases**.
- The allocator must handle scenarios where total free space ≥ weapon size but no single contiguous region is large enough.
- In such cases, weapons must be swapped out to create a contiguous free region.

---

## 10. Deadlocks, Artifacts & Resource Management (Section 7)

### 10.1 The Two Exclusive Artifacts

| Artifact | Requirement |
|---|---|
| Solar Core | Only 1 instance exists in the entire game |
| Lunar Blade | Only 1 instance exists in the entire game |

- To perform the **Ultimate Ability**, a character must **simultaneously hold both** the Solar Core and the Lunar Blade in their **active primary inventory**.
- These are **resource-contended** items — only one process/character can hold each at a time.

### 10.2 The Eclipse Relic (Dynamic Artifact)

- A **third artifact** called the **Eclipse Relic** is introduced **dynamically at runtime**.
- It appears when a character picks it up from the environment.
- Once introduced, it becomes part of the **global artifact pool** and follows the **same exclusivity and locking rules** as Solar Core and Lunar Blade.
- The Global Resource Table must be able to accommodate this dynamic addition.

### 10.3 Global Resource Table

- Students must maintain a **shared resource table** that tracks the state of all global artifacts.
- Must track: Solar Core, Lunar Blade, Eclipse Relic (if present).
- For each artifact, the table must indicate:
  - Whether the resource is **free**.
  - If held, which **specific entity** currently holds it.
- This table must live in **shared memory**.
- Access to this table must be **protected** by synchronization mechanisms.

### 10.4 Resource Locking Protocol

When a process wants to acquire or release an artifact:
1. **Lock** the resource table (prevent concurrent updates).
2. Consult the table — check if artifact is free.
3. If free: acquire it, update the table.
4. **Unlock** the resource table immediately after update.
5. Use the artifact.
6. When done: **lock** table again → release artifact → **unlock** table.

### 10.5 Deadlock Scenario

Classic deadlock example in the game:
- **Player A** locks Solar Core, waits for Lunar Blade.
- **NPC B** locks Lunar Blade, waits for Solar Core.
- → **Circular wait** → Deadlock.

### 10.6 Mandatory Deadlock Monitoring

- Students must implement a **background logic thread within the Arbiter** that:
  - Continuously monitors the global resource table.
  - Detects **circular wait conditions** (e.g., using a wait-for graph).
  - If deadlock is detected: forces **one process to release its held item** to break the cycle.
  - The game must then proceed normally.

### 10.7 Wait-For Graph (Suggested Implementation)

```
Nodes = processes/entities
Edges = "Entity A is waiting for Entity B's resource"

Circular wait = cycle in the graph → deadlock detected
Resolution = force one node in the cycle to release its resource
```

---

## 11. Ultimate Ability & Pause Mechanic (Section 8)

### 11.1 Eligibility

- A player character may **only trigger the Ultimate Ability** if:
  - Both the **Solar Core** AND the **Lunar Blade** are in their **active primary inventory simultaneously**.
  - (Not in long-term storage — must be in the 20-slot inventory.)

### 11.2 Effect

- When triggered: the **Automated Strategic Process (ASP) is fully suspended** for **exactly 10 seconds**.
- After 10 seconds, the ASP is **resumed by the Arbiter**.
- This gives the player an uninterrupted tactical window.

### 11.3 Signal-Only Implementation (HARD REQUIREMENT)

- The suspension and resumption **must be achieved exclusively through signals**.
- **Neither the Arbiter nor the ASP may use flags or pipes** to coordinate this state transition.
- Specifically:
  - **Arbiter sends a signal to suspend ASP** (e.g., `SIGSTOP` or `SIGUSR2`).
  - **Arbiter uses `SIGALRM`** with a custom handler to time the 10-second window.
  - **Arbiter sends a signal to resume ASP** once `SIGALRM` fires (e.g., `SIGCONT` or resume signal).

### 11.4 Resumption Protocol

- The Arbiter must use `SIGALRM` and a **custom handler** to manage the 10-second suspension.
- Once the window expires:
  - Arbiter resumes the ASP.
  - Arbiter provides the ASP with **updated staminas of all enemies**.

### 11.5 NPC Turn Timeout (Related)

- To prevent blocking, if the ASP does not submit a move within **3 seconds** of being given its turn, the Arbiter automatically assigns **"Skip"** as the NPC's action.

---

## 12. Mandatory Graphical Interface & Rendering Thread (Section 9)

### 12.1 UI is Mandatory

- A real-time user interface is a **mandatory component** — not optional.
- **Basic CLIs are not allowed.**

### 12.2 What Must Be Displayed

- **Real-time stamina bars** for all entities.
- **Health statistics** for all entities.
- **Action log** showing recent actions taken.

### 12.3 Rendering Thread Architecture

- The rendering logic must be in its **own dedicated thread** (within the Arbiter or a separate rendering process).
- This ensures the UI does **not interfere** with the high-precision timing of the scheduler.
- The rendering thread must:
  - Read the current state **directly from synchronized shared memory**.
  - Update the display **independently** of the main scheduling loop.

### 12.4 Allowed UI Libraries

| Type | Allowed Libraries |
|---|---|
| GUI | SFML, SDL2, RayLib, GLFW |
| TUI | ncurses |

- **ncurses is recommended for macOS** users (SFML has OpenGL conflicts inside Docker on macOS).
- ASCII art UI with ncurses is **explicitly allowed** (confirmed in GCR) — but must have **proper UI elements** rather than simple line-by-line CLI output.
- Reference for good TUI: `htop` animations, or [Curses of War layout](https://libregamewiki.org/images/8/8a/Curses_of_War.png).

### 12.5 Safe Shared Memory Reads in Rendering Thread

- The rendering thread must **safely read** from shared memory (must use appropriate locks to avoid reading mid-update corrupted state).
- Use read locks or snapshot patterns to prevent tearing.

---

## 13. Gameplay Configuration & All Stats (Section 10)

### 13.1 Player Party

- At game start: player is prompted to select **party size**.
- Allowed range: **1 to 4 human-controlled characters**.
- All human characters are controlled via the HIP.

### 13.2 Enemy Configuration

- Number of **concurrent enemies on screen** at any given time: **random between 2 and 9**.
- This number is decided randomly **each run**.
- Each NPC has its own dedicated thread in the ASP.
- Total enemies to defeat to win: **10** (so more enemies appear as some are defeated).

### 13.3 HP Formulas

| Entity | HP Formula |
|---|---|
| Player Character | `RollNumber + random(100, 1000)` |
| Enemy | `last 2 digits of RollNumber + random(50, 200)` |

### 13.4 Damage Formulas

| Entity | Damage Formula |
|---|---|
| Player Character | `last digit of RollNumber + 10` |
| Enemy | `second last digit of RollNumber + 10` |

> **Example:** Roll Number = `24i-0123`
> - Player HP: `24 + random(100,1000)` (or however the number portion is used — use the numeric portion of your roll number as the seed)
> - Enemy HP: `23 + random(50,200)` (last 2 digits)
> - Player Damage: `3 + 10 = 13` (last digit)
> - Enemy Damage: `2 + 10 = 12` (second last digit)

### 13.5 Speed Formulas

| Entity | Speed |
|---|---|
| Player Character | `100 / Number of Player Characters` |
| Enemy | `random(10, 30)` |

### 13.6 Max Stamina

| Entity | Max Stamina |
|---|---|
| Player Character | 100 |
| Enemy | 150 |

### 13.7 Player Actions (Exactly One Per Turn)

| Action | Effect on Enemy | Stamina Cost |
|---|---|---|
| **Attack (Strike)** | Reduces selected enemy's HP by player's damage stat | Depleted to 0 |
| **Attack (Exhaust)** | Reduces selected enemy's **Stamina** by player's damage stat | Depleted to 0 |
| **Use Weapon** | Reduces selected enemy's HP by the weapon's damage stat | Depleted to 0 |
| **Swap In** | Brings a weapon from long-term storage; **cannot use it this turn** | Depleted to 0 |
| **Heal** | Restores **10% of the player's HP** | Depleted to 0 |
| **Skip** | No effect on enemy | Depleted to **50%** (stamina becomes 50) |

### 13.8 Enemy Actions (Exactly One Per Turn)

| Action | Effect | Stamina Cost |
|---|---|---|
| **Attack (Strike)** | Reduces a chosen player's HP by the enemy's damage stat | Depleted to 0 |
| **Skip** | No effect | Depleted to **50%** (stamina becomes 75, since max is 150) |

---

## 14. Weapon Table & Inventory Rules

### 14.1 Complete Weapon Table

| Weapon Name | Slot Size | Damage Output | Notes |
|---|---|---|---|
| Solar Core | 10 | 95 | Artifact — 1 instance only; required for Ultimate |
| Lunar Blade | 10 | 90 | Artifact — 1 instance only; required for Ultimate |
| Iron Halberd | 7 | 55 | — |
| Venom Dagger | 4 | 30 | — |
| Thunderstaff | 6 | 50 | — |
| Obsidian Axe | 5 | 45 | — |
| Frostbow | 6 | 48 | — |
| Splinter Stick | 2 | 12 | Fragmentation edge case weapon |

### 14.2 Inventory Rules

- Primary inventory: **20 slots total** (linear array).
- Solar Core (10) + Lunar Blade (10) = **exactly 20 slots** → no room for anything else.
- This is **by design** and is a **hard constraint** the space allocator must enforce.
- Splinter Stick (2 slots) is intentionally small to create fragmentation scenarios.
- Player **may hold multiple copies** of the same weapon.

### 14.3 NPC Weapon Rules

- NPCs **can pick up dropped weapons**.
- If an NPC holds a weapon, **that weapon will NOT be dropped** when the NPC dies.
- Instead, a **random weapon** (not necessarily the NPC's held weapon) is dropped when an NPC is defeated.

---

## 15. Win / Lose / Quit Conditions

### 15.1 Win Condition
- Players kill a total of **10 enemies** (not 10 concurrent — cumulative total).
- Game completes and exits gracefully.

### 15.2 Lose Condition
- **All Player Characters die**.
- Game completes and exits gracefully.

### 15.3 Quit Condition
- The player chooses to quit.
- The Human Interfacing Process (HIP) sends **SIGTERM** to the Arbiter.
- The Arbiter handles SIGTERM and triggers graceful shutdown of all processes.

### 15.4 Graceful Exit Requirements
- On any exit condition, all processes must terminate cleanly.
- Shared memory segments must be unlinked (`shm_unlink`).
- All threads must be properly joined or cancelled.
- All semaphores/mutexes must be destroyed.

---

## 16. Bonus: Multiplayer Extension (Section 11)

### 16.1 Bonus Task
- Implement a **Local Multiplayer Mode** where:
  - Two separate human-controlled processes compete against each other, OR
  - Two human-controlled processes compete against a shared pool of NPCs.

### 16.2 Evaluation Criteria for Bonus
- Stability of the multi-process architecture (20 marks).
- Synchronization correctness in multiplayer mode (10 marks).

### 16.3 Priority Note
- The final implementation must first focus on **stability and logical correctness** of OS concepts.
- Bonus features should only be attempted after the core game is fully working.

---

## 17. Allowed vs. Prohibited Technologies

### 17.1 ALLOWED

| Category | Allowed |
|---|---|
| **Processes** | `fork()`, `exec()`, `waitpid()` |
| **IPC** | `shm_open()`, `mmap()`, `munmap()`, `shm_unlink()` |
| **Threading** | `pthread_create()`, `pthread_join()`, `pthread_detach()` |
| **Mutex** | `pthread_mutex_t`, `pthread_mutex_lock()`, `pthread_mutex_unlock()`, `pthread_mutex_init()` with `PTHREAD_PROCESS_SHARED` |
| **Semaphores** | `sem_t`, `sem_init()` (unnamed, in shared mem), `sem_wait()`, `sem_post()`, `sem_destroy()` |
| **Signals** | `signal()`, `sigaction()`, `kill()`, `raise()`, `SIGUSR1`, `SIGUSR2`, `SIGALRM`, `SIGTERM`, `SIGSTOP`, `SIGCONT` |
| **Timing** | `sleep()`, `usleep()`, `nanosleep()`, `alarm()` |
| **STL for game logic** | `std::vector`, `std::list`, `std::queue`, `std::map`, etc. |
| **GUI** | SFML, SDL2, RayLib, GLFW, ncurses |
| **Language** | C or C++ (C++17 recommended) |

### 17.2 STRICTLY PROHIBITED (Marks = 0 if used)

| Prohibited Item | Why Prohibited |
|---|---|
| `std::thread` | Must use pthreads |
| `std::mutex` | Must use `pthread_mutex_t` |
| `std::atomic` | Must use memory-based OS primitives |
| `std::semaphore` | Must use `sem_t` with `sem_init` |
| `std::condition_variable` | Higher-level wrapper — prohibited |
| Named pipes (`mkfifo`, `open` for FIFO) | No pipes allowed at all |
| Unnamed pipes (`pipe()`) | No pipes allowed at all |
| Any higher-level library for threading/sync | pthreads + memory primitives only |
| Pre-compiled binaries in submission | Will be deleted before grading |

---

## 18. Docker Environment Setup (Full Guide)

### 18.1 Why Docker

Chrono Rift uses POSIX shared memory, unnamed semaphores, signals (SIGUSR1, SIGALRM), and pthreads — all **Linux-only system calls**. Docker ensures every student compiles and runs inside the **same Ubuntu 22.04 environment**, eliminating dependency issues and guaranteeing consistent grading.

All three processes (arbiter, hip, asp) run inside the same container and share a Linux namespace. Shared memory and signals work without any special Docker flags.

**macOS Note:** Use ncurses (TUI) interface on macOS. SFML GUI is unreliable inside Docker on macOS due to OpenGL conflicts.

### 18.2 Install Docker

Download Docker Desktop from: https://www.docker.com/products/docker-desktop

| OS | Action |
|---|---|
| Linux | Install Docker Desktop, then run: `sudo usermod -aG docker $USER` and log out/back in |
| macOS | Docker Desktop installs and runs without extra configuration |

### 18.3 The Dockerfile (Copy As-Is — Do Not Modify)

```dockerfile
FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive

# Install core build tools and all supported GUI libraries
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    gdb \
    libsfml-dev \
    libsdl2-dev \
    libglfw3-dev \
    libncurses-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy and install any extra packages declared in requirements.txt
COPY requirements.txt /tmp/requirements.txt
RUN grep -v '^#' /tmp/requirements.txt | grep -v '^$' | \
    xargs -r apt-get install -y && rm -rf /var/lib/apt/lists/*

WORKDIR /app
CMD ["bash"]
```

**Packages already provided by the Dockerfile (do NOT add to requirements.txt):**
- `build-essential`
- `cmake`
- `gdb`
- `libsfml-dev`
- `libsdl2-dev`
- `libglfw3-dev`
- `libncurses-dev`

### 18.4 Build the Docker Image

```bash
# Run once from inside your submission folder
docker build -t chrono-rift-env .
# This takes 2–5 minutes the first time.
# Repeat only if you modify Dockerfile or requirements.txt.
```

### 18.5 Running the Container

**Linux:**
```bash
# Run once per terminal session
xhost +local:docker

# Run every time you start the container
docker run -it --rm \
    -e DISPLAY=$DISPLAY \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    -v $(pwd):/app \
    chrono-rift-env
```

**macOS (ncurses — recommended):**
```bash
docker run -it --rm \
    -v $(pwd):/app \
    chrono-rift-env
```

**macOS (SFML — if needed):**
- Install XQuartz first
- Add `-e DISPLAY=host.docker.internal:0` to the run command

### 18.6 Daily Development Workflow

```bash
# Inside container terminal:

# Build all three processes
make

# Run all three in background
./arbiter & ./hip & ./asp &

# Stop all background processes
kill %1 %2 %3

# Clean and rebuild
make clean && make
```

**Tip:** Source files are live-synced via the volume mount. Edit on your host machine, rebuild inside the container without restarting it.

### 18.7 The Makefile

```makefile
CXX      = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -pthread

# Uncomment the ONE LIBS line that matches your GUI choice:
LIBS = -lsfml-graphics -lsfml-window -lsfml-audio -lsfml-network -lsfml-system -lrt
# LIBS = $(shell sdl2-config --libs) -lrt
# LIBS = -lglfw -lGL -lrt
# LIBS = -lncurses -lrt

TARGETS = arbiter hip asp

all: clean $(TARGETS)
    @echo Build complete.

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

> **CRITICAL:** The indentation before each compile command **must be a tab character**, not spaces. If copied incorrectly, `make` will fail with `missing separator`.

### 18.8 Common Docker Errors

| Error | Fix |
|---|---|
| Cannot connect to X server | Run `xhost +local:docker` on the host before starting the container |
| `missing separator (Makefile)` | Tab characters were replaced with spaces — re-copy the Makefile |
| `SFML/Graphics.hpp not found` | Image was built before Dockerfile was saved — re-run `docker build` |
| Segfault on `shm_open` | Stale shared memory from a previous run — call `shm_unlink` at startup |
| `requirements.txt` packages not found | Check spelling — run `apt-cache search <name>` inside container to verify |

---

## 19. Folder Structure & Build System

### 19.1 Required Folder Structure

```
submission/
├── Dockerfile             ← include unchanged
├── Makefile               ← uncomment LIBS line for your GUI choice
├── requirements.txt       ← list any extra apt packages (leave blank if none)
├── arbiter/
│   ├── arbiter.cpp        ← main entry point — produces 'arbiter' executable
│   ├── *.h                ← optional helper headers
│   └── *.cpp              ← optional helper source files
├── hip/
│   ├── hip.cpp            ← main entry point — produces 'hip' executable
│   ├── *.h                ← optional helper headers
│   └── *.cpp              ← optional helper source files
├── asp/
│   ├── asp.cpp            ← main entry point — produces 'asp' executable
│   ├── *.h                ← optional helper headers
│   └── *.cpp              ← optional helper source files
└── report.pdf             ← turnaround analysis + screenshots
```

### 19.2 Requirements

- Each process folder produces **exactly one executable** (arbiter, hip, asp).
- You may split implementation across as many `.h`/`.cpp` files as needed within each folder.
- The filename must be `Dockerfile` with **no extension**.

### 19.3 Submission Checklist

| File/Folder | Requirement |
|---|---|
| `Dockerfile` | Required — include unchanged |
| `Makefile` | Required — must compile all three targets |
| `requirements.txt` | Required — list extra apt packages or leave blank |
| `arbiter/` | Required — must contain `arbiter.cpp` as main entry point |
| `hip/` | Required — must contain `hip.cpp` as main entry point |
| `asp/` | Required — must contain `asp.cpp` as main entry point |
| `report.pdf` | Required — turnaround analysis and screenshots |
| Pre-compiled binaries | NOT ALLOWED — will be deleted before grading |

---

## 20. GCR Clarifications (All Official Answers)

### 20.1 STL Usage (Teacher: Faisal Cheema)

> **Q: Is STL allowed?**

**A:**
- STL **containers** (`std::vector`, `std::list`, `std::queue`) for game logic: **ALLOWED**.
- STL threading/sync wrappers (`std::thread`, `std::mutex`, `std::atomic`, etc.): **STRICTLY PROHIBITED**.
- Must use: pthreads, shared memory, and memory-based primitives taught in class/lab only.
- Any solution that uses higher-level library wrappers to circumvent Linux-specific system calls will be marked **0**, regardless of how well the game functions.

### 20.2 Submission for Groups of Two (Teacher comment inferred)

> **Q: The naming convention requires one name and roll number. Do both members submit separately?**

**A:** Implied from naming convention — one submission per group with one member's roll number and name.

### 20.3 Weapon Drops Clarification (Teacher: Hamdan Nawaz)

> **Q: Section 6 says weapons drop when enemy is defeated; Section 10 says NPC's held weapon won't drop. Which is correct?**

**A:**
- NPCs **can pick up a dropped weapon from the battlefield**.
- However, when an NPC is defeated, a **random weapon** is dropped — **not necessarily the one they picked up** or held.
- In other words: weapon drops are a **game reward** (random weapon given), not the NPC's actual inventory weapon.
- NPC's held weapon never drops — a random weapon from the game world drops instead.

### 20.4 ASCII Art / ncurses UI (Teacher: Hamdan Nawaz)

> **Q: Is ASCII art UI with ncurses allowed?**

**A:** Yes, ncurses ASCII art UI is allowed. However, it must have **proper UI elements** rather than simple line-by-line CLI output. Reference: `htop` animations, [Curses of War layout](https://libregamewiki.org/images/8/8a/Curses_of_War.png).

### 20.5 Game Scope — Battle Screen Only (Teacher: Hamdan Nawaz)

> **Q: Is the game just the battle screen? No overworld, no map, no player movement?**

**A:** Yes, it is just the **combat portion only**. However, the battle screen must show mechanics for when additional enemies appear (since concurrent enemies on screen are fewer than 10, but 10 total must be defeated).

### 20.6 Solo Projects (Teacher: Faisal Cheema)

> **Q: Is solo allowed?**

**A:** Solo projects are **discouraged** but cases will be reviewed on a situational basis. Cross-section pairs are still not allowed.

### 20.7 C vs C++ (Teacher: Hamdan Nawaz)

> **Q: Can we code in C or is C++ mandatory?**

**A:** You can use **C as well**. (C++ is recommended but not mandatory.)

### 20.8 Inventory with Solar Core + Lunar Blade (Student: Talha Iftikhar)

> **Q: If Solar Core + Lunar Blade occupy all 20 slots, can other weapons come to the long-term queue, and can Swap In work to replace one?**

**A:** (Teacher answer pending in thread, but from spec analysis):
- If Solar (10) + Lunar (10) = 20 slots → **no room for any other weapon in primary inventory**.
- Other weapons CAN go into the long-term queue (long-term storage is separate from the 20-slot array).
- A player CAN use Swap In to bring a weapon from long-term storage, but doing so requires the allocator to swap out one of the current weapons (Solar or Lunar) to long-term storage first — freeing enough contiguous space.
- This means to hold Solar + Venom Dagger: you'd need to swap out Lunar → bring in Venom (4 slots) → now inventory has Solar (10) + Venom (4) + 6 free slots.

---

## 21. Implementation Workflow & Architecture Blueprint

### 21.1 Recommended Implementation Order

```
Phase 1: Shared Memory & IPC Foundation
  ├── Design shared memory layout (all structs)
  ├── Implement shm_open / mmap / shm_unlink wrappers
  ├── Initialize mutex attributes (PTHREAD_PROCESS_SHARED)
  └── Initialize semaphores (sem_init with pshared=1)

Phase 2: Process Skeletons
  ├── arbiter.cpp — main loop skeleton
  ├── hip.cpp — main + thread spawn skeleton
  └── asp.cpp — main + NPC thread skeleton

Phase 3: Scheduling Engine
  ├── Stamina accumulation logic (concurrent, 1-second tick)
  ├── Turn detection (who filled stamina first)
  ├── Serial action execution
  └── Stamina reset and recalculation

Phase 4: Player Actions (HIP)
  ├── Input capture per player thread
  ├── Active/Idle thread logic
  ├── Writing actions to shared memory
  └── Arbiter reading and applying player actions

Phase 5: NPC Actions (ASP)
  ├── Thread-per-NPC spawning
  ├── NPC decision logic (Attack or Skip)
  ├── 3-second timeout enforcement (Arbiter side)
  └── Concurrent NPC thread synchronization

Phase 6: Signals
  ├── SIGUSR1 → Stun handler (3-second pause, stamina preserved)
  ├── SIGALRM → Ultimate Ability timer
  ├── SIGSTOP/SIGCONT → ASP suspension/resumption
  └── SIGTERM → Graceful quit from HIP

Phase 7: Inventory Allocator
  ├── 20-slot linear array representation
  ├── First-fit or best-fit contiguous search
  ├── Swap-out minimum weapons to make room
  ├── Long-term storage data structure
  └── Swap In action logic

Phase 8: Artifacts & Deadlock
  ├── Global resource table in shared memory
  ├── Locking protocol for artifact acquisition/release
  ├── Background deadlock detection thread in Arbiter
  ├── Wait-for graph or similar algorithm
  └── Eclipse Relic dynamic addition

Phase 9: Rendering Thread
  ├── Thread creation in Arbiter
  ├── Reading state from shared memory safely
  ├── ncurses or GUI display update loop
  └── Stamina bars, HP, action log

Phase 10: Game Conditions & Polish
  ├── Win condition (10 kills)
  ├── Lose condition (all players dead)
  ├── Quit condition (SIGTERM from HIP)
  └── Graceful shutdown all processes/threads/shm
```

### 21.2 Thread Map

```
ARBITER PROCESS
├── Main Thread          — Scheduling loop, turn management, rule enforcement
├── Deadlock Thread      — Background circular wait detection
└── Render Thread        — UI update from shared memory

HIP PROCESS
├── Main Thread          — Process management, I/O dispatch
├── Player Thread 1      — Input for player character 1
├── Player Thread 2      — Input for player character 2 (if party size ≥ 2)
├── Player Thread 3      — Input for player character 3 (if party size ≥ 3)
└── Player Thread 4      — Input for player character 4 (if party size = 4)

ASP PROCESS
├── Main Thread          — Process management, thread coordination
├── NPC Thread 1         — AI for NPC 1
├── NPC Thread 2         — AI for NPC 2
├── NPC Thread 3         — AI for NPC 3 (if present)
...
└── NPC Thread N         — AI for NPC N (N = 2 to 9, random per run)
```

---

## 22. Shared Memory Layout Design

### 22.1 Suggested Shared Memory Structure

```c
// Character (Player or NPC)
typedef struct {
    int id;
    int hp;
    int max_hp;
    int stamina;
    int max_stamina;
    int speed;
    int damage;
    int is_alive;
    int is_stunned;
    int stun_remaining;    // in seconds
    int is_player;         // 1 = player, 0 = NPC
    int inventory[20];     // slot array: weapon_id or -1 for empty
    // long-term storage handled separately or as extra array
} Entity;

// Action (HIP → Arbiter)
typedef struct {
    int entity_id;
    int action_type;       // ATTACK_STRIKE, ATTACK_EXHAUST, USE_WEAPON, SWAP_IN, HEAL, SKIP
    int target_id;         // which entity to target
    int weapon_slot;       // which inventory slot (for USE_WEAPON)
    int swap_weapon_id;    // for SWAP_IN
    int is_ready;          // flag: HIP has written an action
} PlayerAction;

// Global Resource Table (Artifacts)
typedef struct {
    int solar_core_holder;   // -1 = free, otherwise entity_id
    int lunar_blade_holder;  // -1 = free, otherwise entity_id
    int eclipse_relic_present; // 0 = not in game, 1 = in game
    int eclipse_relic_holder;  // -1 = free, otherwise entity_id
    pthread_mutex_t table_lock;
} ResourceTable;

// Shared Game State
typedef struct {
    // Synchronization
    pthread_mutex_t state_lock;      // main state lock (PTHREAD_PROCESS_SHARED)
    sem_t turn_sem;                   // signals whose turn it is
    
    // Entities
    Entity players[4];               // up to 4 player characters
    Entity npcs[9];                  // up to 9 concurrent NPCs
    int num_players;
    int num_npcs;
    
    // Game state
    int total_kills;                 // toward win condition of 10
    int game_over;                   // 0 = ongoing, 1 = win, 2 = lose, 3 = quit
    
    // Turn management
    int current_turn_entity_id;
    int action_committed;
    
    // Actions
    PlayerAction player_actions[4];  // one per player
    int npc_actions[9];              // action type per NPC
    
    // Resource table
    ResourceTable resources;
    
    // Action log (for rendering)
    char action_log[20][256];        // last 20 actions, 256 chars each
    int log_head;
    
} SharedGameState;
```

### 22.2 Mutex Initialization for Cross-Process Use

```c
// CRITICAL: must use PTHREAD_PROCESS_SHARED for mutexes in shared memory
pthread_mutexattr_t attr;
pthread_mutexattr_init(&attr);
pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
pthread_mutex_init(&shm->state_lock, &attr);
pthread_mutexattr_destroy(&attr);

// Same for semaphores — pshared = 1 for cross-process
sem_init(&shm->turn_sem, 1, 0);  // 1 = shared between processes
```

---

## 23. Signal Map: All Signals Used

| Signal | Direction | Purpose |
|---|---|---|
| `SIGUSR1` | Arbiter → Target Process | Stun a player or NPC (3-second pause) |
| `SIGALRM` | Self (Arbiter) | Timer for Ultimate Ability 10-second window |
| `SIGSTOP` | Arbiter → ASP | Suspend ASP when Ultimate Ability triggers |
| `SIGCONT` | Arbiter → ASP | Resume ASP after Ultimate Ability window |
| `SIGTERM` | HIP → Arbiter | Player quit — triggers graceful shutdown |

### 23.1 Signal Handler Templates

```c
// Stun handler — installed in HIP and ASP processes
void stun_handler(int sig) {
    // Save current state if needed
    sleep(3);  // Exactly 3 seconds
    // Execution resumes here — stamina level unchanged
}

// Ultimate Ability timer — installed in Arbiter
void ultimate_alarm_handler(int sig) {
    // 10 seconds elapsed — resume ASP
    kill(asp_pid, SIGCONT);
    // Update NPC staminas and notify ASP
}

// Quit handler — installed in Arbiter
void sigterm_handler(int sig) {
    // Graceful shutdown sequence
    // cleanup shared memory, signal all processes to exit
    cleanup_and_exit();
}

// Registration (prefer sigaction over signal())
struct sigaction sa;
sigemptyset(&sa.sa_mask);
sa.sa_flags = 0;

sa.sa_handler = stun_handler;
sigaction(SIGUSR1, &sa, NULL);

sa.sa_handler = ultimate_alarm_handler;
sigaction(SIGALRM, &sa, NULL);

sa.sa_handler = sigterm_handler;
sigaction(SIGTERM, &sa, NULL);
```

---

## 24. Inventory Allocator Logic (Memory Management Detail)

### 24.1 The 20-Slot Array

```c
// -1 = empty slot
// weapon_id = slot is occupied by weapon with that ID
int inventory[20];
// Initialize all to -1
memset(inventory, -1, sizeof(inventory));
```

### 24.2 Finding a Contiguous Free Region

```c
int find_contiguous(int* inventory, int size, int weapon_slots) {
    int start = -1;
    int count = 0;
    for (int i = 0; i < size; i++) {
        if (inventory[i] == -1) {
            if (count == 0) start = i;
            count++;
            if (count == weapon_slots) return start;
        } else {
            count = 0;
            start = -1;
        }
    }
    return -1;  // not found
}
```

### 24.3 Swap-Out Strategy

When there is not enough contiguous space:
1. Find the **minimum number of weapons** to swap out to create a contiguous region large enough.
2. Swap those weapons to long-term storage.
3. Place the new weapon in the freed space.

```
Example: Inventory = [SolarCore(0-9), IronHalberd(10-16), free(17-19)]
Want to add: Lunar Blade (10 slots needed)
Free space: 3 contiguous slots at 17-19 — not enough.
→ Must swap out IronHalberd (7 slots) → frees 10-16 + 17-19 = 10 contiguous slots
→ Place Lunar Blade at slots 10-19. ✓
```

### 24.4 Fragmentation Example (Splinter Stick)

```
Inventory = [SplinterStick(0-1), free(2-3), SplinterStick(4-5), free(6-7), ...]
Total free: 14 slots, but all fragmented in 2-slot chunks.
Want to add: Iron Halberd (7 slots).
→ Contiguous search fails.
→ Must swap out enough weapons to create 7 contiguous free slots.
```

### 24.5 Long-Term Storage

- Can be implemented as any data structure (e.g., `std::vector<int>` of weapon IDs in shared memory or a separate segment).
- Player can only access it via **Swap In** action (costs a full turn).
- No limit on long-term storage size is specified — implement as unbounded.

---

## 25. Deadlock Detection Algorithm

### 25.1 Wait-For Graph

```
Nodes: all entities (players + NPCs)
Edges: if Entity A is waiting for a resource held by Entity B,
       draw edge A → B

Deadlock = cycle exists in this graph
```

### 25.2 Cycle Detection (Simple DFS)

```c
// Run in background Arbiter thread periodically
bool detect_deadlock(ResourceTable* rt, Entity* entities, int num_entities) {
    // Build wait-for graph
    // For each entity waiting for Solar Core/Lunar Blade:
    //   add edge from that entity to the holder of the resource they want
    // Run DFS to detect cycle
    // Return true if cycle found
}
```

### 25.3 Resolution

- If deadlock detected: force **one entity** in the cycle to release one of its held artifacts.
- The released artifact goes back to the global resource table (marked free).
- The forced-release entity must then re-acquire if it wants to use the Ultimate Ability.
- Log the deadlock resolution in the action log.

---

## 26. Edge Cases & Tricky Requirements

### 26.1 Stun When Stamina Is Full
- If target's stamina is FULL when stunned: their **turn is skipped** (they don't get to act).
- After stun ends: they still have full stamina (stamina is preserved, not reset).

### 26.2 Stun Recovery — Exact Point
- Recovery must resume from the **exact point** of interruption.
- In practice: a signal handler with `sleep(3)` achieves this — execution returns to exactly where the signal was delivered.

### 26.3 Skip Action — Stamina to 50% Not 0
- **Player Skip:** stamina → 50 (50% of 100).
- **Enemy Skip:** stamina → 75 (50% of 150).
- This means a skipping entity gets to act sooner than a full reset but later than a non-skipping entity.

### 26.4 ASP 3-Second Timeout
- The Arbiter must implement a **timeout mechanism** (likely with `SIGALRM` or a separate timer thread) for NPC turns.
- If ASP doesn't respond within 3 seconds: Arbiter assigns "Skip" and moves on.
- This prevents NPC crashes or bugs from blocking the whole game.

### 26.5 Solar Core + Lunar Blade in Inventory (Not Long-Term Storage)
- For Ultimate Ability: **both must be in the active 20-slot primary inventory**.
- Having one in long-term storage does NOT count.

### 26.6 Concurrent Enemies Respawn Logic
- Max concurrent on screen: 2–9 (determined once per run).
- Total to kill: 10.
- As enemies are defeated, new ones appear to maintain the concurrent count until 10 total are killed.
- Each new NPC gets its own thread in ASP.

### 26.7 PTHREAD_PROCESS_SHARED — Critical
- Mutexes and semaphores placed in shared memory **MUST** be initialized with the `PTHREAD_PROCESS_SHARED` attribute.
- Without this, they will NOT work across process boundaries.

### 26.8 shm_unlink at Startup
- From Docker guide: call `shm_unlink` at startup to clean up any stale shared memory from previous runs that crashed.

### 26.9 Only Thread for Active Player Processes Input
- In HIP: only the thread corresponding to the **currently active player** (as per Arbiter's determination) processes input.
- All other threads must remain **idle** (not busy-waiting — use a condition variable or semaphore to block them efficiently).

### 26.10 Rendering Thread Must Not Block Scheduling
- The rendering thread must be asynchronous — it reads and displays state but never holds locks that would block the game loop.
- Use snapshot reads or try-lock patterns to avoid blocking.

---

## 27. Report Requirements

The `report.pdf` must include:

### 27.1 Turnaround Time Analysis
- Mathematical demonstration of the stamina-based scheduling logic.
- Show arrival times for entities with example stats.
- Formula: `Arrival Time = Max Stamina / Speed`
- Prove that the implementation follows this formula exactly.

### 27.2 Gameplay Screenshots
- Screenshots of the game running inside the Docker container.
- Must show: stamina bars, HP stats, action log.
- Must demonstrate multiple states: normal gameplay, stun event, weapon pickup, etc.

---

## 28. Common Mistakes to Avoid

### 28.1 Architecture Mistakes
- ❌ Using pipes for any IPC — instant 0 on that section.
- ❌ Using `std::thread` / `std::mutex` / `std::atomic` — instant 0 on threading section.
- ❌ All NPCs in a single thread — not accepted.
- ❌ HIP directly modifying shared game state — must go through Arbiter.
- ❌ Using `sem_open` (named semaphores) — must use `sem_init` (unnamed semaphores in shared mem).

### 28.2 Scheduling Mistakes
- ❌ Actions executing in parallel — only one action at a time (serial execution).
- ❌ Skip setting stamina to 0 — it must be 50% of max.
- ❌ Stun resetting stamina — stamina is preserved through stun.

### 28.3 Signal Mistakes
- ❌ Using flags or pipes to coordinate Ultimate Ability — signals only.
- ❌ Active polling (`while(!flag)`) for stun — must be signal-driven.
- ❌ Forgetting `PTHREAD_PROCESS_SHARED` on mutexes/semaphores in shared memory.

### 28.4 Memory Management Mistakes
- ❌ Swapping out more weapons than necessary — must be minimum necessary.
- ❌ Not handling fragmentation — Splinter Stick specifically tests this.
- ❌ Putting Ultimate Ability check on long-term storage weapons — must be in active inventory.

### 28.5 Build Mistakes
- ❌ Spaces instead of tabs in Makefile — will cause `missing separator` error.
- ❌ Including pre-compiled binaries — they will be deleted before grading.
- ❌ Wrong folder naming — 20% automatic deduction.
- ❌ Not calling `shm_unlink` at startup — stale shared memory will cause segfaults.

### 28.6 Gameplay Mistakes
- ❌ Dropping the NPC's actual held weapon on death — only a random weapon drops.
- ❌ Not implementing the "player doesn't pick up → enemy guaranteed picks up" mechanic.
- ❌ Using wrong roll number for seed — must be your specific roll number.
- ❌ Quit not sending SIGTERM from HIP — must use SIGTERM for quit.

---

## Reference Material

### Video References (For Gameplay Inspiration Only)
- https://youtu.be/OQtv2KEGvsw?si=UP6k7p7he7J0vpLA&t=74
- https://youtu.be/33AKougRwew

> **Important:** These are only for understanding general gameplay and environment setup. All implementation must follow this project statement — not the videos.

### UI Reference
- [Curses of War TUI Layout](https://libregamewiki.org/images/8/8a/Curses_of_War.png) — example of a good ncurses UI

---

*Last updated from: Project Statement (PDF), OS_Project_Rubric.docx, Chrono_Rift_Docker_Guide.docx, and GCR Comments thread as of May 2026.*

*Total marks: 390 (360 core + 30 bonus). Deadline: 10 May 2026, 23:59 PST.*
