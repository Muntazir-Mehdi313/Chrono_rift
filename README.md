# Chrono Rift

**CS 2006 — Operating Systems | Spring 2026 | BS-CS | FAST-NUCES**

> **⚠️ Disclaimer:** I vibe-coded this project to understand the underlying OS concepts (multi-processing, IPC via shared memory, pthreads, signals, scheduling, deadlock detection, and memory allocation). It's a learning exercise, not a production-grade codebase.

---

## Overview

Chrono Rift is a multi-process, turn-based tactical combat game (inspired by Chrono Trigger's battle system) built entirely to demonstrate core Operating Systems concepts. There's no overworld or map — it's purely the battle screen, where a human-controlled party fights waves of NPC enemies until 10 total kills are reached.

The game is split across **three independent processes**, communicating exclusively through **POSIX shared memory** (no pipes), synchronized with **pthread mutexes / unnamed semaphores**, and coordinated with **POSIX signals** for time-critical events like stuns and the Ultimate Ability.

---

## Architecture

```
                GAME ARBITER
   (central authority, global state owner)
   - Scheduling & turn enforcement
   - Background deadlock-detection thread
   - Dedicated rendering thread
   - SIGALRM handler (Ultimate Ability timer)
        │
        │  Shared Memory (shm_open / mmap)
   ┌────┴────┐
   ▼         ▼
 HIP        ASP
(Human      (Automated Strategic Process)
Interfacing  - 1 thread per NPC
Process)     - NPC decision logic
 - 1 thread
   per player
   character
```

| Process     | Responsibility |
|-------------|-----------------|
| **Arbiter** | Owns and mutates all global game state, enforces turn order/stamina rules, runs the deadlock-detection thread and the rendering thread, handles `SIGALRM`/`SIGTERM`. |
| **HIP**     | Multi-threaded — one thread per player character. Reads input from the *active* player only (others stay idle). Sends actions to the Arbiter via shared memory; never mutates state directly. |
| **ASP**     | Multi-threaded — one dedicated thread per NPC (no sequential handling of multiple NPCs in a single thread). Runs enemy decision logic concurrently with proper synchronization. |

---

## Core OS Concepts Demonstrated

- **Process isolation & IPC** — three processes, own address spaces, communicating only through shared memory.
- **Multithreading** — thread-per-player-character (HIP) and thread-per-NPC (ASP).
- **Synchronization** — `pthread_mutex_t` and unnamed `sem_t` (via `sem_init`, `PTHREAD_PROCESS_SHARED`) protecting shared state from race conditions.
- **Scheduling** — stamina-based temporal scheduling with concurrent stamina accumulation and serial action execution.
- **Signals** — `SIGUSR1` (stun), `SIGALRM` (Ultimate Ability timer), `SIGSTOP`/`SIGCONT` (ASP suspension), `SIGTERM` (graceful quit).
- **Deadlock detection & resolution** — wait-for graph over contended artifacts (Solar Core, Lunar Blade, Eclipse Relic), resolved by forcing a release.
- **Memory management** — contiguous 20-slot inventory allocator with fragmentation handling and swap-out/swap-in to long-term storage.

---

## Gameplay Rules Summary

### Scheduling
- Every entity has `speed` and `stamina`; `speed` is added to `stamina` every second, concurrently across all entities.
- `Arrival Time = Max Stamina / Speed`.
- First entity to reach max stamina acts; action execution is strictly serial.
- After acting, stamina resets to 0 (or 50% on Skip).

| Entity | Max Stamina | Speed |
|---|---|---|
| Player | 100 | `100 / number of players` |
| NPC | 150 | random(10, 30) |

### Actions (one per turn)
**Player:** Attack (Strike), Attack (Exhaust), Use Weapon, Swap In, Heal, Skip.
**Enemy:** Attack (Strike), Skip.
Skip drains stamina to 50% instead of 0.

### Stun
- `SIGUSR1` halts the target process asynchronously (no polling) for exactly **3 seconds**.
- Stamina is preserved across the stun, not reset.

### Ultimate Ability
- Requires **both** Solar Core and Lunar Blade in the active 20-slot inventory (not long-term storage) at the same time.
- Suspends the ASP for exactly **10 seconds**, using **signals only** (`SIGSTOP`/`SIGCONT` + `SIGALRM` timer) — no flags, no pipes.

### Inventory & Weapons
- 20-slot contiguous linear array per player.
- Allocator finds contiguous free space; if none is large enough, it swaps the *minimum* number of weapons to long-term storage to make room.
- Swap In costs a full turn and the weapon can't be used until next turn.
- Solar Core (10) + Lunar Blade (10) exactly fill all 20 slots — no room for anything else.
- Splinter Stick (2 slots) is a fragmentation edge case.

| Weapon | Slots | Damage |
|---|---|---|
| Solar Core | 10 | 95 |
| Lunar Blade | 10 | 90 |
| Iron Halberd | 7 | 55 |
| Thunderstaff | 6 | 50 |
| Frostbow | 6 | 48 |
| Obsidian Axe | 5 | 45 |
| Venom Dagger | 4 | 30 |
| Splinter Stick | 2 | 12 |

### Deadlock
- Artifacts (Solar Core, Lunar Blade, dynamic Eclipse Relic) are exclusive, single-instance resources tracked in a shared **Global Resource Table**.
- A background Arbiter thread runs deadlock detection (wait-for graph / cycle detection) and forces a release to break circular waits.

### Win / Lose / Quit
- **Win:** 10 total enemy kills (cumulative, concurrent enemies capped at 2–9 on screen at once).
- **Lose:** all player characters dead.
- **Quit:** HIP sends `SIGTERM` to the Arbiter, which triggers graceful shutdown (shared memory unlinked, threads joined, mutexes/semaphores destroyed).

### Stats (Roll-Number Seeded)
| Entity | HP | Damage |
|---|---|---|
| Player | `RollNumber + random(100,1000)` | `last digit of RollNumber + 10` |
| Enemy | `last 2 digits of RollNumber + random(50,200)` | `second-last digit of RollNumber + 10` |

---

## UI

A real-time graphical/TUI interface is mandatory (plain CLI is not accepted). Supported libraries: **SFML, SDL2, RayLib, GLFW** (GUI) or **ncurses** (TUI, recommended on macOS). The UI runs on a dedicated rendering thread that reads shared memory safely (snapshot/lock pattern) and shows real-time stamina bars, HP, and an action log — without blocking the scheduling loop.

---

## Folder Structure

```
submission/
├── Dockerfile
├── Makefile
├── requirements.txt
├── arbiter/
│   └── arbiter.cpp
├── hip/
│   └── hip.cpp
├── asp/
│   └── asp.cpp
└── report.pdf
```

---

## Build & Run

The project runs inside a Docker container (Ubuntu 22.04) to guarantee access to Linux-only syscalls (POSIX shared memory, unnamed semaphores, signals, pthreads).

```bash
# Build the image (once, or after editing Dockerfile/requirements.txt)
docker build -t chrono-rift-env .

# Linux
xhost +local:docker
docker run -it --rm -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix -v $(pwd):/app chrono-rift-env

# macOS (ncurses recommended)
docker run -it --rm -v $(pwd):/app chrono-rift-env
```

Inside the container:

```bash
make              # builds arbiter, hip, asp
./arbiter & ./hip & ./asp &
kill %1 %2 %3     # stop all
make clean && make
```

> Remember to uncomment the correct `LIBS` line in the Makefile for your chosen GUI/TUI library, and note that Makefile recipe lines must use **tab** indentation.

---

## Allowed vs. Prohibited

**Allowed:** `fork`/`exec`/`waitpid`, `shm_open`/`mmap`/`shm_unlink`, `pthread_create`/`join`/`detach`, `pthread_mutex_t` (with `PTHREAD_PROCESS_SHARED`), unnamed `sem_t` (`sem_init`), signals (`SIGUSR1`, `SIGUSR2`, `SIGALRM`, `SIGTERM`, `SIGSTOP`, `SIGCONT`), STL containers for game logic, SFML/SDL2/RayLib/GLFW/ncurses.

**Prohibited (instant 0 on that section if used):** `std::thread`, `std::mutex`, `std::atomic`, `std::semaphore`, `std::condition_variable`, named or unnamed pipes, any higher-level threading/sync wrapper, pre-compiled binaries in the submission.

---

## Report

`report.pdf` must include a turnaround-time analysis proving the `Arrival Time = Max Stamina / Speed` scheduling formula, plus gameplay screenshots showing stamina bars, HP stats, the action log, and events like a stun and a weapon pickup.

---

## Notes / Gotchas

- Call `shm_unlink` at startup to clear stale shared memory from crashed prior runs.
- All mutexes/semaphores placed in shared memory **must** use `PTHREAD_PROCESS_SHARED`.
- The rendering thread must never hold locks that block the scheduling loop — use snapshot reads.
- NPC's actual held weapon never drops on death; a random weapon drops instead as a reward.
- Roll number is the seed for all randomization — used consistently across HP, damage, and enemy speed generation.
