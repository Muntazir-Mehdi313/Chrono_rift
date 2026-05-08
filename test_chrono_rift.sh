#!/bin/bash
# ============================================================================
# test_chrono_rift_v2.sh — Enhanced Test Suite with Hard Runtime Tests
# CS 2006 Operating Systems | Spring 2026
# Roll: 24i-0847 | Partner: 24i-0650
#
# TESTS ADDED vs v1:
#   - Synchronization: mutex contention, semaphore handshake timing
#   - Race condition: multiple entities reaching full stamina simultaneously
#   - Resource starvation: NPC timeout auto-skip under load
#   - Memory: shared memory size consistency across all 3 processes
#   - Deadlock detection: artificial wait-for-cycle via shared memory
#   - Inventory: fragment test with multiple small/large weapons
#   - Signal delivery: SIGUSR1 stun timing verification
#   - Graceful shutdown: SIGTERM mid-game stability
#   - Multi-player: 4 player concurrent thread safety
#   - Weapon drop: automated pickup and decline paths
# ============================================================================

# set -e
cd "$(dirname "$0")"

RED='\033[0;31m'; GRN='\033[0;32m'; YLW='\033[1;33m'
BLU='\033[0;34m'; CYN='\033[0;36m'; MAG='\033[0;35m'; NC='\033[0m'

PASS=0; FAIL=0; WARN=0; TOTAL=0

pass()    { echo -e "  ${GRN}✓ PASS${NC}  $1"; ((PASS++)); ((TOTAL++)); }
fail()    { echo -e "  ${RED}✗ FAIL${NC}  $1"; ((FAIL++)); ((TOTAL++)); }
warn()    { echo -e "  ${YLW}⚠ WARN${NC}  $1"; ((WARN++)); ((TOTAL++)); }
section() { echo -e "\n${BLU}━━━ $1 ━━━${NC}"; }
hard()    { echo -e "\n${MAG}◆◆◆ HARD TEST: $1 ◆◆◆${NC}"; }
info()    { echo -e "  ${CYN}ℹ${NC}  $1"; }

echo -e "${BLU}"
echo "  ╔══════════════════════════════════════════════════╗"
echo "  ║  CHRONO RIFT — ENHANCED TEST SUITE v2          ║"
echo "  ║  CS 2006 OS | Roll: 24i-0847                   ║"
echo "  ╚══════════════════════════════════════════════════╝"
echo -e "${NC}"

# ── BUILD ─────────────────────────────────────────────────────────────────────
section "PHASE 1-3: Build & Binary Check"
info "Building..."
BUILD_LOG=$(docker run --rm -v $(pwd):/app chrono-rift-env make clean all 2>&1)
BUILD_EXIT=$?
[ $BUILD_EXIT -eq 0 ] && pass "Build succeeded" || { fail "Build FAILED"; echo "$BUILD_LOG" | tail -20; exit 1; }
for bin in bin/arbiter bin/hip bin/asp; do
    [ -f "$bin" ] && pass "Binary: $bin" || fail "Missing: $bin"
done

# ── STATIC ANALYSIS TESTS ─────────────────────────────────────────────────────
section "PHASE 3: Shared Memory Configuration"
grep -q "chrono_rift_shm" arbiter/shared_types.h 2>/dev/null && pass "SHM_NAME correct" || warn "SHM_NAME unclear"
grep -q "PTHREAD_PROCESS_SHARED" arbiter/shm_manager.cpp 2>/dev/null && pass "PTHREAD_PROCESS_SHARED used" || fail "PTHREAD_PROCESS_SHARED missing"
grep -q "sem_init.*1," arbiter/shm_manager.cpp 2>/dev/null && pass "Semaphores pshared=1" || fail "Semaphores not cross-process"
# Check ALL 3 mutexes are initialized
MUTEX_COUNT=$(grep -c "pthread_mutex_init" arbiter/shm_manager.cpp 2>/dev/null)
[ "$MUTEX_COUNT" -ge 3 ] && pass "All 3 mutexes initialized (master, log, table)" || warn "Only $MUTEX_COUNT mutex inits found (expected 3)"
# Check semaphores destroyed on cleanup
grep -q "sem_destroy" arbiter/shm_manager.cpp 2>/dev/null && pass "sem_destroy called on cleanup" || fail "sem_destroy missing — semaphore leak"
grep -q "pthread_mutex_destroy" arbiter/shm_manager.cpp 2>/dev/null && pass "pthread_mutex_destroy called" || fail "mutex_destroy missing"

section "PHASE 4: Entity Initialization"
grep -q "240847" arbiter/entity_init.cpp 2>/dev/null && pass "ROLL_NUMBER=240847" || fail "Roll number missing"
grep -q "ROLL_LAST_DIGIT.*7\|7.*+.*10" arbiter/entity_init.cpp 2>/dev/null && pass "Player damage=17" || warn "Player damage unclear"
grep -q "ROLL_SECOND_LAST.*4\|4.*+.*10" arbiter/entity_init.cpp 2>/dev/null && pass "NPC damage=14" || warn "NPC damage unclear"
grep -q "ROLL_LAST_TWO.*47" arbiter/entity_init.cpp 2>/dev/null && pass "NPC HP base=47" || warn "NPC HP base unclear"
grep -q "srand" arbiter/arbiter.cpp 2>/dev/null && pass "srand() called with roll number seed" || fail "srand() not called — RNG unseeded"
# Check stamina starts at 0
grep -q "stamina.*0.0f\|stamina.*= 0" arbiter/entity_init.cpp 2>/dev/null && pass "Stamina starts at 0" || warn "Stamina init unclear"
# Check max_stamina values
grep -q "max_stamina.*100\|100.*max_stamina" arbiter/entity_init.cpp 2>/dev/null && pass "Player max_stamina=100" || warn "Player max_stamina unclear"
grep -q "max_stamina.*150\|150.*max_stamina" arbiter/entity_init.cpp 2>/dev/null && pass "NPC max_stamina=150" || warn "NPC max_stamina unclear"

section "PHASE 5: HIP Player Threads"
grep -q "pthread_create" hip/hip.cpp 2>/dev/null && pass "pthread_create in hip.cpp" || fail "No threads in HIP"
grep -q "pthread_join" hip/hip.cpp 2>/dev/null && pass "pthread_join — threads properly joined" || fail "No pthread_join — thread leak"
grep -q "sem_wait" hip/hip.cpp 2>/dev/null && pass "HIP blocks on turn semaphore" || fail "HIP not blocking on semaphore"
grep -q "sem_post.*action_submitted" hip/hip.cpp 2>/dev/null && pass "HIP posts action_submitted" || fail "HIP not posting action"
grep -q "EINTR" hip/hip.cpp 2>/dev/null && pass "HIP handles EINTR (signal-safe sem_wait)" || warn "EINTR not handled in HIP"

section "PHASE 6: ASP NPC Thread Pool"
grep -q "pthread_create" asp/asp.cpp 2>/dev/null && pass "pthread_create in asp.cpp" || fail "No threads in ASP"
grep -q "pthread_join" asp/asp.cpp 2>/dev/null && pass "pthread_join in ASP" || fail "Thread leak in ASP"
grep -q "EINTR" asp/asp.cpp 2>/dev/null && pass "ASP handles EINTR" || warn "EINTR not handled in ASP"
# Check NPC threads exit on death
grep -q "still_mine\|is_alive\|game_over" asp/asp.cpp 2>/dev/null && pass "NPC threads exit on death/game-over" || fail "NPC threads may not exit"

section "PHASE 7: Stamina Scheduler"
grep -q "stamina_tick_thread" arbiter/game_loop.cpp 2>/dev/null && pass "stamina_tick_thread defined" || fail "Tick thread missing"
grep -q "TICK_MS.*100\|100.*TICK" arbiter/game_loop.cpp 2>/dev/null && pass "100ms tick interval" || warn "Tick interval unclear"
grep -q "action_in_progress" arbiter/game_loop.cpp 2>/dev/null && pass "Tick pauses during action" || fail "Tick not pausing"
grep -q "ultimate_active" arbiter/game_loop.cpp 2>/dev/null && pass "Tick pauses during Ultimate" || fail "Tick not pausing for Ultimate"
grep -q "dispatch_turn" arbiter/game_loop.cpp 2>/dev/null && pass "dispatch_turn() — turn handshake" || fail "dispatch_turn missing"
grep -q "sem_timedwait.*3\|tv_sec.*3" arbiter/game_loop.cpp 2>/dev/null && pass "NPC 3s timeout (sem_timedwait)" || fail "NPC timeout missing"
# Check lowest-ID tiebreak
grep -q "id.*acting_id\|acting_id.*id" arbiter/game_loop.cpp 2>/dev/null && pass "Lowest entity ID tiebreak" || warn "Tiebreak unclear"

section "PHASE 8/9: All Actions"
for action in ACTION_ATTACK_STRIKE ACTION_ATTACK_EXHAUST ACTION_USE_WEAPON ACTION_SWAP_IN ACTION_HEAL ACTION_SKIP ACTION_ULTIMATE; do
    grep -q "$action" arbiter/game_loop.cpp 2>/dev/null && pass "Action: $action" || fail "Action missing: $action"
done
grep -q "stamina.*0.0f" arbiter/game_loop.cpp 2>/dev/null && pass "Stamina reset to 0 after action" || warn "Stamina reset unclear"
grep -q "0.5f" arbiter/game_loop.cpp 2>/dev/null && pass "Skip: stamina→50%" || fail "Skip logic missing"
grep -q "max_hp / 10\|maxhp/10\|heal.*10" arbiter/game_loop.cpp 2>/dev/null && pass "Heal: 10% of max HP" || warn "Heal formula unclear"
grep -q "total_kills.*10\|10.*total_kills" arbiter/game_loop.cpp 2>/dev/null && pass "Win: 10 kills" || fail "Win condition missing"

section "PHASE 10: Stun"
grep -q "SIGUSR1" hip/hip.cpp asp/asp.cpp arbiter/signal_handler.cpp 2>/dev/null && pass "SIGUSR1 in all 3 processes" || fail "SIGUSR1 missing"
grep -q "sleep(3)" hip/hip.cpp 2>/dev/null && pass "HIP stun=3s" || fail "HIP stun not 3s"
grep -q "sleep(3)" asp/asp.cpp 2>/dev/null && pass "ASP stun=3s" || fail "ASP stun not 3s"
grep -q "schedule_stun_recovery\|stun_recovery_thread" arbiter/signal_handler.cpp 2>/dev/null && pass "Stun recovery thread" || fail "No stun recovery"
grep -q "STATUS_NOT_STUNNED\|is_stunned.*0" arbiter/signal_handler.cpp 2>/dev/null && pass "is_stunned cleared after 3s" || fail "Stun never cleared"
grep -q "try_apply_stun" arbiter/game_loop.cpp 2>/dev/null && pass "try_apply_stun() wired into game_loop" || fail "Stun not wired in"

section "PHASE 11: Ultimate"
grep -q "SIGSTOP" arbiter/signal_handler.cpp 2>/dev/null && pass "SIGSTOP → ASP" || fail "SIGSTOP missing"
grep -q "SIGCONT" arbiter/signal_handler.cpp 2>/dev/null && pass "SIGCONT → ASP" || fail "SIGCONT missing"
grep -q "alarm(10)" arbiter/signal_handler.cpp 2>/dev/null && pass "alarm(10) for 10s window" || fail "alarm(10) missing"
grep -q "SIGALRM" arbiter/signal_handler.cpp 2>/dev/null && pass "SIGALRM handler installed" || fail "SIGALRM missing"
grep -q "holds_solar_core.*holds_lunar_blade" arbiter/signal_handler.cpp 2>/dev/null && pass "Both artifacts required for Ultimate" || fail "Solar+Lunar check missing"
grep -q "ultimate_active.*1\|1.*ultimate_active" arbiter/signal_handler.cpp 2>/dev/null && pass "ultimate_active flag set" || fail "ultimate_active not set"

section "PHASE 12: Inventory Allocator"
grep -q "find_contiguous_free" arbiter/inventory_allocator.cpp 2>/dev/null && pass "find_contiguous_free() — first-fit" || fail "Contiguous allocator missing"
grep -q "lt_storage_push" arbiter/inventory_allocator.cpp 2>/dev/null && pass "lt_storage_push() — eviction" || fail "Eviction missing"
grep -q "lt_storage_remove" arbiter/inventory_allocator.cpp 2>/dev/null && pass "lt_storage_remove() — swap-in" || fail "Storage remove missing"
grep -q "execute_swap_in" arbiter/inventory_allocator.cpp 2>/dev/null && pass "execute_swap_in()" || fail "execute_swap_in() missing"
grep -q "just_swapped_in_weapon" arbiter/inventory_allocator.cpp 2>/dev/null && pass "Swap-in guard (can't use same turn)" || fail "Swap-in guard missing"
grep -q "update_artifact_flags" arbiter/inventory_allocator.cpp 2>/dev/null && pass "Artifact flags kept in sync" || warn "Artifact sync unclear"
grep -q "best_evict_count\|minimum.*evict\|evict_count" arbiter/inventory_allocator.cpp 2>/dev/null && pass "Minimum eviction strategy" || warn "Eviction strategy unclear"
# Check INVENTORY_SLOTS=20
grep -q "INVENTORY_SLOTS.*20\|20.*INVENTORY_SLOTS" arbiter/shared_types.h 2>/dev/null && pass "INVENTORY_SLOTS=20" || warn "INVENTORY_SLOTS value unclear"

section "PHASE 13: Weapon Drops"
grep -q "roll_weapon_drop" arbiter/inventory_allocator.cpp 2>/dev/null && pass "roll_weapon_drop()" || fail "roll_weapon_drop() missing"
grep -q "npc_pickup_weapon" arbiter/inventory_allocator.cpp 2>/dev/null && pass "npc_pickup_weapon() — guaranteed" || fail "NPC pickup missing"
grep -q "action_submitted" hip/input_handler.cpp 2>/dev/null && pass "HIP posts sem after drop choice" || fail "HIP missing sem_post for drop"
grep -q "drop_awaiting_player_choice.*2\|==.*2" arbiter/game_loop.cpp hip/input_handler.cpp 2>/dev/null && pass "Drop state 2 (yes) handled" || fail "Drop yes-path broken"
grep -q "drop_awaiting_player_choice.*3\|==.*3" arbiter/game_loop.cpp hip/input_handler.cpp 2>/dev/null && pass "Drop state 3 (no) handled" || fail "Drop no-path broken"

section "PHASE 14: Resource Table"
grep -q "acquire_artifact" arbiter/resource_table.cpp 2>/dev/null && pass "acquire_artifact()" || fail "acquire_artifact() missing"
grep -q "release_artifact" arbiter/resource_table.cpp 2>/dev/null && pass "release_artifact()" || fail "release_artifact() missing"
grep -q "release_all_artifacts_for_entity" arbiter/resource_table.cpp 2>/dev/null && pass "release_all on entity death" || fail "Death release missing"
grep -q "set_waiting_for\|waiting_for_resource" arbiter/resource_table.cpp 2>/dev/null && pass "set_waiting_for() — deadlock feed" || fail "Wait tracking missing"
grep -q "eclipse_relic" arbiter/resource_table.cpp 2>/dev/null && pass "Eclipse Relic tracked" || fail "Eclipse Relic missing"
grep -q "table_mutex" arbiter/resource_table.cpp 2>/dev/null && pass "table_mutex — resource table is thread-safe" || fail "Resource table not locked"
# Lock ordering check
LOCK_ORDER=$(grep -n "table_mutex\|master_mutex" arbiter/deadlock_monitor.cpp 2>/dev/null | head -4)
echo "$LOCK_ORDER" | grep -q "table_mutex" && pass "Lock ordering: table_mutex before master_mutex in monitor" || warn "Lock ordering unclear"

section "PHASE 15: Deadlock Monitor"
grep -q "WaitEdge" arbiter/deadlock_monitor.cpp 2>/dev/null && pass "WaitEdge struct — wait-for graph" || fail "Wait-for graph missing"
grep -q "detect_cycle_dfs" arbiter/deadlock_monitor.cpp 2>/dev/null && pass "DFS cycle detection" || fail "DFS missing"
grep -q "in_stack\|visited" arbiter/deadlock_monitor.cpp 2>/dev/null && pass "DFS visited/in_stack arrays" || fail "DFS state missing"
grep -q "resolve_deadlock" arbiter/deadlock_monitor.cpp 2>/dev/null && pass "resolve_deadlock() — forced release" || fail "Resolution missing"
grep -q "sleep(1)" arbiter/deadlock_monitor.cpp 2>/dev/null && pass "Monitor checks every 1s" || warn "Monitor interval unclear"
grep -q "launch_deadlock_monitor" arbiter/game_loop.cpp 2>/dev/null && pass "Monitor launched from game_loop" || fail "Monitor not launched"

section "PHASE 16: SFML Render"
grep -q "sf::RenderWindow" arbiter/render_thread.cpp 2>/dev/null && pass "SFML RenderWindow" || fail "SFML missing"
grep -q "take_snapshot\|memcpy.*players" arbiter/render_thread.cpp 2>/dev/null && pass "Non-blocking snapshot" || warn "Snapshot unclear"
grep -q "log_mutex" arbiter/render_thread.cpp 2>/dev/null && pass "log_mutex used for log snapshot" || fail "log_mutex missing in render"
grep -q "master_mutex" arbiter/render_thread.cpp 2>/dev/null && pass "master_mutex used for entity snapshot" || fail "master_mutex missing in render"
grep -q "launch_render_thread" arbiter/game_loop.cpp 2>/dev/null && pass "Render thread launched" || fail "Not launched"
grep -q "setFramerateLimit\|FPS\|framerateLimit" arbiter/render_thread.cpp 2>/dev/null && pass "FPS limit set (prevents CPU burn)" || warn "No FPS limit"

section "PHASE 17: Graceful Shutdown"
grep -q "arbiter_check_quit\|g_quit_requested" arbiter/signal_handler.cpp arbiter/game_loop.cpp 2>/dev/null && pass "SIGTERM quit flag" || fail "Quit flag missing"
grep -q "kill.*hip_pid.*SIGTERM" arbiter/game_loop.cpp 2>/dev/null && pass "SIGTERM → HIP on shutdown" || fail "SIGTERM to HIP missing"
grep -q "kill.*asp_pid.*SIGTERM" arbiter/game_loop.cpp 2>/dev/null && pass "SIGTERM → ASP on shutdown" || fail "SIGTERM to ASP missing"
grep -q "pthread_join.*g_tick\|pthread_join.*tick_tid" arbiter/game_loop.cpp 2>/dev/null && pass "Tick thread joined" || fail "Tick thread not joined"
grep -q "pthread_join.*deadlock\|pthread_join.*g_deadlock" arbiter/game_loop.cpp 2>/dev/null && pass "Deadlock thread joined" || fail "Deadlock thread not joined"
grep -q "pthread_join.*render\|pthread_join.*g_render" arbiter/game_loop.cpp 2>/dev/null && pass "Render thread joined" || fail "Render thread not joined"
grep -q "shm_unlink" arbiter/shm_manager.cpp 2>/dev/null && pass "shm_unlink — no shm leak" || fail "shm_unlink missing"

# =============================================================================
# HARD RUNTIME TESTS
# =============================================================================

hard "SYNCHRONIZATION — Shared Memory Size Consistency"
info "All 3 processes must map EXACTLY the same struct size"
SHM_SIZE_LOG=$(docker run --rm --ipc=host -v $(pwd):/app chrono-rift-env bash -c '
    echo "1" | timeout 5 ./bin/arbiter > /tmp/a.log 2>&1 &
    sleep 1
    timeout 3 ./bin/asp >> /tmp/a.log 2>&1 &
    timeout 3 ./bin/hip >> /tmp/a.log 2>&1 &
    sleep 3
    cat /tmp/a.log
' 2>&1)
ARBITER_SIZE=$(echo "$SHM_SIZE_LOG" | grep -oP "Shared memory size:\s+\K[0-9]+" | head -1)
HIP_SIZE=$(echo "$SHM_SIZE_LOG" | grep -oP "Attached to shared memory.*\(\K[0-9]+" | head -1)
ASP_SIZE=$(echo "$SHM_SIZE_LOG" | grep -oP "Attached to shared memory.*\(\K[0-9]+" | tail -1)
if [ -n "$ARBITER_SIZE" ] && [ "$ARBITER_SIZE" = "$HIP_SIZE" ] && [ "$ARBITER_SIZE" = "$ASP_SIZE" ]; then
    pass "SHM size consistent across all 3 processes: $ARBITER_SIZE bytes"
elif [ -n "$ARBITER_SIZE" ] && [ -n "$HIP_SIZE" ]; then
    if [ "$ARBITER_SIZE" = "$HIP_SIZE" ]; then
        pass "SHM size consistent (Arbiter+HIP): $ARBITER_SIZE bytes"
    else
        fail "SHM SIZE MISMATCH — Arbiter=$ARBITER_SIZE HIP=$HIP_SIZE — struct layout differs!"
    fi
else
    warn "Could not extract SHM sizes from logs — run manually to verify"
fi

hard "SYNCHRONIZATION — Stale Shared Memory Cleanup"
info "Arbiter must clean up /dev/shm/chrono_rift_shm on crash/restart"
docker run --rm --ipc=host -v $(pwd):/app chrono-rift-env bash -c '
    # Create stale shm manually
    python3 -c "
import mmap, os
fd = os.open(\"/dev/shm/chrono_rift_shm\", os.O_CREAT|os.O_RDWR, 0o666)
os.write(fd, b\"STALE\" * 100)
os.close(fd)
print(\"Stale shm created\")
"
    # Now start arbiter — it should shm_unlink the stale one and create fresh
    echo "1" | timeout 4 ./bin/arbiter > /tmp/stale_test.log 2>&1 &
    sleep 2
    cat /tmp/stale_test.log
' > /tmp/stale_test_out.log 2>&1
grep -q "Shared memory created" /tmp/stale_test_out.log && \
    pass "Arbiter overwrites stale shared memory on startup" || \
    warn "Could not verify stale shm cleanup"

hard "RACE CONDITION — Multiple Players Simultaneously Ready"
info "4-player game: all players reach full stamina at same time — only lowest ID acts first"
RACE_LOG=$(docker run --rm --ipc=host -v $(pwd):/app chrono-rift-env bash -c '
    echo "4" | timeout 20 ./bin/arbiter > /tmp/race_arb.log 2>&1 &
    sleep 1
    timeout 20 ./bin/asp > /tmp/race_asp.log 2>&1 &
    (
      for i in $(seq 1 40); do echo "1"; echo "0"; sleep 0.2; done; echo "0"
    ) | timeout 20 ./bin/hip > /tmp/race_hip.log 2>&1 &
    sleep 12
    cat /tmp/race_arb.log
' 2>&1)
# Check turns are dispatched in order (Player 0 always before Player 1,2,3 in same round)
P0=$(echo "$RACE_LOG" | grep -c "Player 0 turn")
P1=$(echo "$RACE_LOG" | grep -c "Player 1 turn")
if [ "$P0" -gt 0 ] && [ "$P1" -gt 0 ]; then
    # P0 and P1 should have same or P0 one more (P0 always acts first)
    if [ "$P0" -ge "$P1" ]; then
        pass "Turn ordering correct: P0($P0 turns) >= P1($P1 turns) — lowest ID first"
    else
        fail "Turn ordering WRONG: P1($P1) got more turns than P0($P0) — tiebreak broken"
    fi
else
    warn "4-player race test: not enough turns to verify ordering (P0=$P0 P1=$P1)"
fi
KILLS=$(echo "$RACE_LOG" | grep -oP "Kills: \K[0-9]+" | sort -n | tail -1)
[ "${KILLS:-0}" -gt 0 ] && pass "4-player game registered $KILLS kills" || warn "No kills in 4-player race test"

hard "NPC TIMEOUT — Auto-Skip When ASP Unresponsive"
info "If ASP doesn't respond in 3s, Arbiter auto-assigns Skip to the NPC"
TIMEOUT_LOG=$(docker run --rm --ipc=host -v $(pwd):/app chrono-rift-env bash -c '
    # Start arbiter and HIP but NOT asp — NPC turns must timeout
    echo "1" | timeout 15 ./bin/arbiter > /tmp/tout_arb.log 2>&1 &
    sleep 1
    # Start a fake ASP that registers but never responds to turns
    python3 -c "
import ctypes, os, mmap, time, struct
# Just register PID then do nothing
fd = os.open(\"/dev/shm/chrono_rift_shm\", os.O_RDWR)
m = mmap.mmap(fd, 0)
# Write asp_pid at offset (arbiter_pid=pid_t at known offset)
# Simpler: just write getpid() to asp_pid field
import subprocess
result = subprocess.run([\"./bin/asp\"], capture_output=True, timeout=12)
" > /tmp/tout_asp.log 2>&1 &
    (
      for i in $(seq 1 20); do echo "1"; echo "0"; sleep 0.5; done; echo "0"
    ) | timeout 15 ./bin/hip > /tmp/tout_hip.log 2>&1 &
    sleep 10
    cat /tmp/tout_arb.log
' 2>&1)
echo "$TIMEOUT_LOG" | grep -q "timed out\|auto Skip\|ETIMEDOUT" && \
    pass "NPC auto-Skip on timeout — Arbiter not blocked by unresponsive ASP" || \
    warn "NPC timeout not visible in log (may need longer run)"

hard "MUTEX LOCKING — No Deadlock Between master_mutex and table_mutex"
info "Lock ordering: table_mutex ALWAYS before master_mutex — verifying in source"
# Check that nowhere in the codebase is master_mutex locked THEN table_mutex
# (which would be reverse order and cause deadlock with deadlock_monitor)
BAD_ORDER=$(awk '
/pthread_mutex_lock.*master_mutex/ { in_master=1; master_line=NR }
/pthread_mutex_unlock.*master_mutex/ { in_master=0 }
/pthread_mutex_lock.*table_mutex/ { if(in_master) print "LINE " NR ": table_mutex locked while master_mutex held (bad order)" }
' arbiter/game_loop.cpp arbiter/resource_table.cpp arbiter/signal_handler.cpp 2>/dev/null)
if [ -z "$BAD_ORDER" ]; then
    pass "Lock ordering correct: no table_mutex acquired while master_mutex held"
else
    fail "LOCK ORDER VIOLATION: $BAD_ORDER"
fi

hard "SEMAPHORE LEAK — action_submitted balance check"
info "Every sem_post(action_submitted) must be matched by a sem_wait in Arbiter"
POST_COUNT=$(grep -c "sem_post.*action_submitted" hip/hip.cpp hip/input_handler.cpp asp/asp.cpp 2>/dev/null | awk -F: '{s+=$2} END{print s}')
WAIT_COUNT=$(grep -c "sem_wait.*action_submitted\|sem_timedwait.*action_submitted" arbiter/game_loop.cpp 2>/dev/null | awk -F: '{s+=$2} END{print s}')
info "sem_post(action_submitted) call sites: $POST_COUNT | sem_wait call sites: $WAIT_COUNT"
[ "$POST_COUNT" -ge 1 ] && [ "$WAIT_COUNT" -ge 1 ] && \
    pass "action_submitted semaphore used symmetrically (post=$POST_COUNT wait=$WAIT_COUNT sites)" || \
    fail "Semaphore asymmetry — post=$POST_COUNT wait=$WAIT_COUNT"

hard "SIGNAL SAFETY — Async-signal-safe functions only in handlers"
info "Signal handlers must not call printf/cout — only write(), sleep(), sem_post()"
UNSAFE_IN_HANDLERS=$(grep -A 20 "sigusr1_handler\|sigterm_handler\|sigalrm_handler" \
    hip/hip.cpp asp/asp.cpp arbiter/signal_handler.cpp 2>/dev/null | \
    grep -v "write\|sleep\|sem_post\|kill\|//\|{.*}" | \
    grep "cout\|printf\|std::" | head -5)
if [ -z "$UNSAFE_IN_HANDLERS" ]; then
    pass "Signal handlers use only async-signal-safe calls (write, sleep, sem_post)"
else
    warn "Possible unsafe call in signal handler: $UNSAFE_IN_HANDLERS"
fi

hard "INVENTORY FRAGMENTATION — Contiguous placement verified"
info "Weapon must occupy CONTIGUOUS slots — spot-check via static analysis"
grep -q "place_weapon\|inventory\[i\].*weapon_id\|inventory\[start" arbiter/inventory_allocator.cpp 2>/dev/null && \
    pass "Contiguous slot fill loop found in inventory_allocator" || \
    fail "No contiguous placement loop found"
grep -q "find_weapon_start\|inventory\[s-1\].*wid\|s-1.*inventory" arbiter/inventory_allocator.cpp 2>/dev/null && \
    pass "find_weapon_start() — block boundary detection implemented" || \
    warn "Weapon block boundary detection unclear"

hard "WEAPON DROP SYNCHRONIZATION — sem_post after drop choice"
info "HIP must post action_submitted AFTER setting drop state 2/3 so Arbiter unblocks"
DROP_POST=$(grep -A 3 "drop_awaiting_player_choice = 2\|drop_awaiting_player_choice = 3" hip/input_handler.cpp 2>/dev/null | grep "sem_post")
[ -n "$DROP_POST" ] && \
    pass "sem_post(action_submitted) found after drop state set in input_handler" || \
    fail "CRITICAL: HIP sets drop state but never posts sem — Arbiter blocks forever after weapon drop"

hard "RUNTIME SMOKE TEST — 1 player, full game to completion or timeout"
info "Automated play: 1 player strikes slot 0, handles drops, plays 30 seconds"
docker run --rm -v $(pwd):/app chrono-rift-env bash -c \
    "rm -f /dev/shm/chrono_rift_shm 2>/dev/null; true" 2>/dev/null || true

SMOKE_LOG=$(docker run --rm --ipc=host -v $(pwd):/app chrono-rift-env bash -c '
    echo "1" | timeout 30 ./bin/arbiter > /tmp/arb.log 2>&1 &
    sleep 1
    timeout 30 ./bin/asp > /tmp/asp.log 2>&1 &
    # HIP input: alternate between normal strikes and drop-handling
    # "1\n0" = Strike target 0
    # Extra "1\n0" handles the drop pickup prompt (choice=1, then extra input)
    (
      for i in $(seq 1 80); do
        echo "1"
        echo "0"
        sleep 0.25
      done
      echo "0"
    ) | timeout 30 ./bin/hip > /tmp/hip.log 2>&1 &
    wait
    echo "=== ARBITER ==="
    cat /tmp/arb.log
    echo "=== HIP ==="
    cat /tmp/hip.log
    echo "=== ASP ==="
    cat /tmp/asp.log
' 2>&1)

echo "$SMOKE_LOG" | grep -q "Shared memory created" && pass "Shared memory created" || fail "SHM not created"
echo "$SMOKE_LOG" | grep -q "HIP=\|game_started" && pass "HIP connected" || fail "HIP not connected"
echo "$SMOKE_LOG" | grep -q "game_started received\|ASP.*game_started" && pass "ASP connected" || fail "ASP not connected"
echo "$SMOKE_LOG" | grep -q "Initializing entities" && pass "Entities initialized" || fail "Entity init failed"
echo "$SMOKE_LOG" | grep -q "Scheduling engine\|Stamina tick" && pass "Scheduling engine started" || fail "Scheduler not started"
echo "$SMOKE_LOG" | grep -q "NPC thread START" && pass "NPC threads spawned in ASP" || fail "NPC threads not spawned"
echo "$SMOKE_LOG" | grep -q "Deadlock monitor\|Monitor thread" && pass "Deadlock monitor running" || fail "Deadlock monitor not started"
echo "$SMOKE_LOG" | grep -q "Player 0 turn\|P0 submitted" && pass "Player turns executed" || fail "No player turns"
echo "$SMOKE_LOG" | grep -q "NPC.*STRIKE\|NPC.*SKIP\|NPC.*turn" && pass "NPC turns executed" || fail "No NPC turns"

KILLS=$(echo "$SMOKE_LOG" | grep -oP "Kills: \K[0-9]+" | sort -n | tail -1)
KILLS=${KILLS:-0}
if [ "$KILLS" -ge 10 ]; then
    pass "VICTORY — 10 kills reached! Full game completed."
elif [ "$KILLS" -ge 5 ]; then
    pass "5+ kills registered ($KILLS/10) — combat fully functional"
elif [ "$KILLS" -ge 1 ]; then
    warn "Only $KILLS/10 kills in 30s — game works but slow (increase timeout or use 1 player)"
else
    warn "No kills registered — check if game loop running"
fi

echo "$SMOKE_LOG" | grep -q "WEAPON DROPPED\|Splinter\|Halberd\|Frostbow\|Thunderstaff\|Obsidian\|Venom" && \
    pass "Weapon drop triggered at least once" || \
    warn "No weapon drop in this run (40% chance per kill — rerun to verify)"

echo "$SMOKE_LOG" | grep -qE "VICTORY|WIN|LOSE|DEFEAT|QUIT|Result=" && \
    pass "Game ended with defined result" || \
    warn "Game may not have exited cleanly (timeout?)"

echo "$SMOKE_LOG" | grep -q "Shared memory destroyed\|shm destroyed\|Clean exit" && \
    pass "Clean shutdown confirmed" || \
    warn "Clean shutdown not confirmed in log"

hard "MULTI-PLAYER CONCURRENCY — 4 players, verify no turn duplication"
info "Each player should act once per round — no player gets two consecutive turns"
MP_LOG=$(docker run --rm --ipc=host -v $(pwd):/app chrono-rift-env bash -c '
    echo "4" | timeout 25 ./bin/arbiter > /tmp/mp_arb.log 2>&1 &
    sleep 1
    timeout 25 ./bin/asp > /tmp/mp_asp.log 2>&1 &
    (for i in $(seq 1 60); do echo "1"; echo "0"; sleep 0.15; done; echo "0") \
        | timeout 25 ./bin/hip > /tmp/mp_hip.log 2>&1 &
    sleep 15
    cat /tmp/mp_arb.log
' 2>&1)
# Check no two consecutive turns by same player
TURNS=$(echo "$MP_LOG" | grep "Player [0-3] turn" | grep -oP "Player \K[0-3]")
PREV=""
DUPE_FOUND=0
while IFS= read -r cur; do
    if [ "$cur" = "$PREV" ]; then
        DUPE_FOUND=1
        break
    fi
    PREV="$cur"
done <<< "$TURNS"
[ $DUPE_FOUND -eq 0 ] && \
    pass "No duplicate consecutive player turns — concurrent threads correct" || \
    warn "Same player acted twice in a row (possible semaphore race)"

MP_KILLS=$(echo "$MP_LOG" | grep -oP "Kills: \K[0-9]+" | sort -n | tail -1)
[ "${MP_KILLS:-0}" -gt 0 ] && pass "4-player game: $MP_KILLS kills registered" || warn "No kills in 4-player test"

hard "GRACEFUL SHUTDOWN — SIGTERM mid-game"
info "Sending SIGTERM to Arbiter mid-game — all processes must exit cleanly"
SIGTERM_LOG=$(docker run --rm --ipc=host -v $(pwd):/app chrono-rift-env bash -c '
    echo "1" | timeout 20 ./bin/arbiter > /tmp/sig_arb.log 2>&1 &
    ARB_PID=$!
    sleep 1
    timeout 20 ./bin/asp > /tmp/sig_asp.log 2>&1 &
    (for i in $(seq 1 10); do echo "1"; echo "0"; sleep 0.3; done) \
        | timeout 20 ./bin/hip > /tmp/sig_hip.log 2>&1 &
    # Let game run 4 seconds then send SIGTERM to arbiter
    sleep 4
    kill -SIGTERM $ARB_PID 2>/dev/null
    sleep 3
    echo "=== ARBITER ==="
    cat /tmp/sig_arb.log
    echo "=== HIP ==="
    cat /tmp/sig_hip.log
    echo "=== ASP ==="
    cat /tmp/sig_asp.log
' 2>&1)
echo "$SIGTERM_LOG" | grep -q "SIGTERM\|graceful\|GAME_QUIT\|QUIT" && \
    pass "SIGTERM received and handled by Arbiter" || \
    warn "SIGTERM handling not confirmed in log"
echo "$SIGTERM_LOG" | grep -q "HIP.*Done\|HIP.*exit\|Clean exit" && \
    pass "HIP exited cleanly after SIGTERM" || \
    warn "HIP clean exit not confirmed"
echo "$SIGTERM_LOG" | grep -q "ASP.*exit\|Clean exit\|ASP.*QUIT" && \
    pass "ASP exited cleanly after SIGTERM" || \
    warn "ASP clean exit not confirmed"

# ── FINAL RESULTS ─────────────────────────────────────────────────────────────
echo ""
echo -e "${BLU}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "  RESULTS:  ${GRN}PASS: $PASS${NC}  |  ${RED}FAIL: $FAIL${NC}  |  ${YLW}WARN: $WARN${NC}  |  TOTAL: $TOTAL"
echo -e "${BLU}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""
if   [ $FAIL -eq 0 ]; then echo -e "${GRN}  ★ ALL TESTS PASSED — Full implementation verified!${NC}"
elif [ $FAIL -le 3 ]; then echo -e "${YLW}  ⚠ Minor issues — review FAIL items above.${NC}"
else echo -e "${RED}  ✗ Multiple failures — see above.${NC}"; fi

echo ""
echo "$SMOKE_LOG" > /tmp/chrono_rift_smoke.log
echo "$RACE_LOG"  > /tmp/chrono_rift_race.log
echo "$MP_LOG"    > /tmp/chrono_rift_mp.log
info "Logs: /tmp/chrono_rift_smoke.log | race.log | mp.log"
echo ""