// ============================================================================
// shm_client.cpp — Shared Memory Attachment (HIP and ASP)
// CS 2006 Operating Systems | Spring 2026 | Chrono Rift
//
// HIP and ASP do NOT create shared memory — only the Arbiter creates it.
// These processes call attach_shared_memory_with_retry() to open and map
// the existing shared memory object created by the Arbiter.
//
// The retry loop (up to 20 attempts, 100ms apart = 2 seconds total) handles
// the race condition where HIP/ASP start before Arbiter finishes creating shm.
//
// IMPORTANT: Only the Arbiter calls shm_unlink(). HIP and ASP just munmap().
// If HIP/ASP called shm_unlink(), the Arbiter would lose its mapping on some
// systems (though on Linux the object persists until the last munmap — still
// bad practice to unlink from client processes).
// ============================================================================

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <cstdlib>
#include <cerrno>
#include "shared_types.h"

// ============================================================================
// attach_shared_memory_with_retry()
// Tries to open and map /chrono_rift_shm up to 20 times.
// Returns a pointer to the SharedGameState on success.
// Exits the process after 20 failed attempts.
//
// Called by HIP and ASP at the very start of main(), before anything else.
// ============================================================================
SharedGameState* attach_shared_memory_with_retry() {
    const int MAX_ATTEMPTS  = 20;
    const int RETRY_DELAY_US = 100000;  // 100ms between attempts

    int fd = -1;
    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        // O_RDWR — both HIP and ASP need to write actions back to shared state
        // No O_CREAT — the object must already exist (Arbiter created it)
        fd = shm_open(SHM_NAME, O_RDWR, 0666);
        if (fd != -1) break;   // success

        if (attempt == 1) {
            std::cout << "[SHM CLIENT] Waiting for Arbiter to create shared memory";
        }
        std::cout << "." << std::flush;
        usleep(RETRY_DELAY_US);
    }
    std::cout << std::endl;

    if (fd == -1) {
        std::cerr << "[SHM CLIENT] ERROR: Could not attach to " << SHM_NAME
                  << " after " << MAX_ATTEMPTS << " attempts." << std::endl;
        std::cerr << "[SHM CLIENT] Is the Arbiter running? Start arbiter first."
                  << std::endl;
        exit(EXIT_FAILURE);
    }

    // Map the shared memory into this process's address space.
    // sizeof(SharedGameState) MUST match what Arbiter used in ftruncate().
    // If they differ (e.g., different builds with different struct sizes),
    // mmap will succeed but accesses will be wrong or crash. Always build
    // from the same shared_types.h (Makefile sync_headers ensures this).
    void* ptr = mmap(
        NULL,
        sizeof(SharedGameState),
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );

    if (ptr == MAP_FAILED) {
        perror("[SHM CLIENT] mmap failed");
        close(fd);
        exit(EXIT_FAILURE);
    }

    close(fd);  // fd no longer needed after mmap

    std::cout << "[SHM CLIENT] Attached to shared memory: " << SHM_NAME
              << " (" << sizeof(SharedGameState) << " bytes)" << std::endl;

    return (SharedGameState*)ptr;
}

// ============================================================================
// detach_shared_memory()
// Called by HIP and ASP at shutdown — unmaps the memory from THIS process.
// Does NOT unlink the shared memory object (Arbiter does that).
// ============================================================================
void detach_shared_memory(SharedGameState* shm) {
    if (!shm) return;
    if (munmap(shm, sizeof(SharedGameState)) == -1) {
        perror("[SHM CLIENT] munmap failed");
    }
    std::cout << "[SHM CLIENT] Detached from shared memory." << std::endl;
}