// ============================================================================
// shm_manager.cpp — Shared Memory Creation & Destruction (Arbiter Only)
// CS 2006 Operating Systems | Spring 2026 | Chrono Rift
//
// ONLY the Arbiter calls create_shared_memory() and destroy_shared_memory().
// HIP and ASP use shm_client.cpp (attach / detach).
//
// This file handles:
//   1. Creating the POSIX shared memory object (/chrono_rift_shm)
//   2. Sizing it to exactly sizeof(SharedGameState)
//   3. Mapping it into the Arbiter's address space
//   4. Zero-initializing the entire struct
//   5. Setting WEAPON_NONE (-1) in all inventory / storage slots
//   6. Initializing ALL mutexes with PTHREAD_PROCESS_SHARED
//   7. Initializing ALL semaphores with pshared=1
//   8. Setting all default game-state values
//
// On destruction (game over / quit):
//   1. Destroys all mutexes and semaphores
//   2. Unmaps the memory
//   3. Unlinks the shared memory object (removes /dev/shm/chrono_rift_shm)
// ============================================================================

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <cstdlib>
#include <cerrno>
#include "shared_types.h"

// ── Forward declarations ──────────────────────────────────────────────────
void destroy_shared_memory(SharedGameState* shm);

// ============================================================================
// create_shared_memory()
// Called ONCE by Arbiter at startup, BEFORE HIP or ASP attach.
// Returns a pointer to the mapped SharedGameState.
// Exits the process on any failure — no partial-init state left behind.
// ============================================================================
SharedGameState* create_shared_memory() {

    // ── Step 1: Destroy any stale shared memory from a previous crashed run.
    // shm_unlink returns -1 if the object doesn't exist — that's fine, ignore it.
    shm_unlink(SHM_NAME);

    // ── Step 2: Create the shared memory object (read-write, mode 0666).
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("[ARBITER][SHM] shm_open(O_CREAT) failed");
        exit(EXIT_FAILURE);
    }

    // ── Step 3: Set the size. ftruncate must be called before mmap.
    // The object starts at 0 bytes — without this, mmap gets SIGBUS on access.
    if (ftruncate(fd, sizeof(SharedGameState)) == -1) {
        perror("[ARBITER][SHM] ftruncate failed");
        close(fd);
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    // ── Step 4: Map the shared memory into the Arbiter's virtual address space.
    // MAP_SHARED: writes are visible to all processes that map this object.
    // PROT_READ | PROT_WRITE: Arbiter needs both.
    void* ptr = mmap(
        NULL,                       // let kernel choose the virtual address
        sizeof(SharedGameState),    // exact size — matches ftruncate above
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0                           // offset 0 from start of shm object
    );

    if (ptr == MAP_FAILED) {
        perror("[ARBITER][SHM] mmap failed");
        close(fd);
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    // File descriptor is no longer needed after mmap. Close it to avoid leaks.
    // The mapping stays alive until munmap() or process exit.
    close(fd);

    SharedGameState* shm = (SharedGameState*)ptr;

    // ── Step 5: Zero-initialize the ENTIRE struct.
    // memset to 0 sets all ints to 0, all floats to 0.0f, all chars to '\0'.
    // This alone would be enough for most fields, but inventory slots need
    // WEAPON_NONE (-1) which is NOT 0, so we handle those explicitly below.
    memset(shm, 0, sizeof(SharedGameState));

    // ── Step 6: Initialize inventory slots to WEAPON_NONE (-1).
    // memset set them to 0 which would mean "holds Solar Core" — wrong.
    // We must explicitly set every slot to WEAPON_NONE.
    for (int p = 0; p < MAX_PLAYERS; p++) {
        for (int s = 0; s < INVENTORY_SLOTS; s++) {
            shm->players[p].inventory[s] = WEAPON_NONE;
        }
        for (int s = 0; s < MAX_LT_STORAGE; s++) {
            shm->players[p].lt_storage[s] = WEAPON_NONE;
        }
        shm->players[p].lt_storage_count    = 0;
        shm->players[p].holds_solar_core    = 0;
        shm->players[p].holds_lunar_blade   = 0;
        shm->players[p].waiting_for_resource = WEAPON_NONE;
        shm->players[p].just_swapped_in_weapon = WEAPON_NONE;
        shm->players[p].action_ready        = 0;
        shm->players[p].action_type         = ACTION_NONE;
        shm->players[p].action_target_id    = -1;
        shm->players[p].action_weapon_slot  = -1;
        shm->players[p].action_lt_weapon_id = WEAPON_NONE;
    }

    for (int n = 0; n < MAX_NPCS; n++) {
        for (int s = 0; s < INVENTORY_SLOTS; s++) {
            shm->npcs[n].inventory[s] = WEAPON_NONE;
        }
        // NPCs don't use long-term storage, but zero it for safety
        for (int s = 0; s < MAX_LT_STORAGE; s++) {
            shm->npcs[n].lt_storage[s] = WEAPON_NONE;
        }
        shm->npcs[n].lt_storage_count    = 0;
        shm->npcs[n].waiting_for_resource = WEAPON_NONE;
        shm->npcs[n].action_ready        = 0;
        shm->npcs[n].action_type         = ACTION_NONE;
        shm->npcs[n].action_target_id    = -1;
    }

    // ── Step 7: Initialize synchronization primitives.
    //
    // CRITICAL: mutexes placed inside shared memory MUST use the
    // PTHREAD_PROCESS_SHARED attribute. Without it, the mutex only
    // works within a single process — locking from another process
    // silently fails or causes undefined behavior.
    //
    // CRITICAL: semaphores initialized with pshared=1 are cross-process.
    // pshared=0 would only work between threads in the same process.

    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);

    // Master state mutex — protects all entity stats and game state fields
    pthread_mutex_init(&shm->master_mutex, &mattr);

    // Log mutex — protects action_log[], log_head, log_count
    // (separate from master so rendering thread doesn't block game loop)
    pthread_mutex_init(&shm->log_mutex, &mattr);

    // Resource table mutex — protects GlobalResourceTable holder fields
    pthread_mutex_init(&shm->resources.table_mutex, &mattr);

    // Done with mutex attribute — destroy it (doesn't affect initialized mutexes)
    pthread_mutexattr_destroy(&mattr);

    // Turn handshake semaphores
    // sem_init(sem, pshared, initial_value)
    //   pshared = 1 → cross-process (lives in shared memory)
    //   initial value = 0 → starts blocked; Arbiter posts to wake HIP/ASP
    if (sem_init(&shm->turn.turn_notification, 1, 0) == -1) {
        perror("[ARBITER][SHM] sem_init(turn_notification) failed");
        munmap(shm, sizeof(SharedGameState));
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }
    if (sem_init(&shm->turn.action_submitted, 1, 0) == -1) {
        perror("[ARBITER][SHM] sem_init(action_submitted) failed");
        sem_destroy(&shm->turn.turn_notification);
        munmap(shm, sizeof(SharedGameState));
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    // ── Step 8: Set all default game-state values.
    shm->game_result                    = GAME_ONGOING;
    shm->game_started                   = 0;         // HIP/ASP wait on this
    shm->ultimate_active                = 0;

    shm->total_kills                    = 0;
    shm->total_npcs_spawned             = 0;

    shm->turn.current_entity_id         = -1;
    shm->turn.current_entity_is_player  = -1;
    shm->turn.current_entity_slot       = -1;
    shm->turn.action_in_progress        = 0;
    shm->turn.waiting_for_input         = 0;

    shm->resources.solar_core_holder    = -1;        // -1 = free
    shm->resources.lunar_blade_holder   = -1;
    shm->resources.eclipse_relic_in_game = 0;        // not yet introduced
    shm->resources.eclipse_relic_holder  = -1;

    shm->log_head                       = 0;
    shm->log_count                      = 0;

    shm->dropped_weapon_id              = WEAPON_NONE;
    shm->drop_awaiting_player_choice    = 0;
    shm->eclipse_spawn_checked          = 0;

    shm->arbiter_pid                    = 0;
    shm->hip_pid                        = 0;
    shm->asp_pid                        = 0;

    std::cout << "[ARBITER] Shared memory created: " << SHM_NAME << std::endl;
    std::cout << "[ARBITER] Shared memory size:    " << sizeof(SharedGameState)
              << " bytes (" << sizeof(SharedGameState) / 1024 << " KB)" << std::endl;
    std::cout << "[ARBITER] Entity size:           " << sizeof(Entity)
              << " bytes each" << std::endl;

    return shm;
}

// ============================================================================
// destroy_shared_memory()
// Called by Arbiter at clean shutdown (win, lose, or quit).
// Destroys all sync primitives, unmaps memory, unlinks the shm object.
// After this call, shm pointer is invalid — do not access it again.
// ============================================================================
void destroy_shared_memory(SharedGameState* shm) {
    if (!shm) return;

    std::cout << "[ARBITER] Destroying shared memory..." << std::endl;

    // Destroy synchronization primitives IN REVERSE ORDER of creation.
    // This ensures no other thread/process is mid-operation when we destroy.
    // (In practice, all threads should be joined before calling this.)
    sem_destroy(&shm->turn.action_submitted);
    sem_destroy(&shm->turn.turn_notification);

    pthread_mutex_destroy(&shm->resources.table_mutex);
    pthread_mutex_destroy(&shm->log_mutex);
    pthread_mutex_destroy(&shm->master_mutex);

    // Unmap the shared memory from Arbiter's address space
    if (munmap(shm, sizeof(SharedGameState)) == -1) {
        perror("[ARBITER][SHM] munmap failed");
    }

    // Unlink the shared memory object — removes /dev/shm/chrono_rift_shm
    // After this, new shm_open() calls with this name create a fresh object.
    if (shm_unlink(SHM_NAME) == -1) {
        perror("[ARBITER][SHM] shm_unlink failed");
    }

    std::cout << "[ARBITER] Shared memory destroyed." << std::endl;
}