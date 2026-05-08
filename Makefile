CXX      = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -pthread

# Switched to SFML/RT libs. Removed -lncurses to fix "cannot find -lncurses" error.
LIBS = -lsfml-graphics -lsfml-window -lsfml-system -lrt

BINDIR = bin

# ── Default target ────────────────────────────────────────────────────────────
all: clean sync_headers $(BINDIR)/arbiter $(BINDIR)/hip $(BINDIR)/asp
	@echo ""
	@echo "============================================"
	@echo "  Build complete."
	@echo "  Run: ./bin/arbiter & sleep 0.5 && ./bin/hip & ./bin/asp &"
	@echo "============================================"

$(BINDIR):
	mkdir -p $(BINDIR)

# ── Header sync: arbiter/shared_types.h is the MASTER ────────────────────────
sync_headers: $(BINDIR)
	@echo "[MAKE] Syncing headers..."
	cp arbiter/shared_types.h hip/shared_types.h
	cp arbiter/shared_types.h asp/shared_types.h

# ── Arbiter ───────────────────────────────────────────────────────────────────
# Automatically includes signal_handler.cpp via the wildcard.
$(BINDIR)/arbiter: sync_headers $(wildcard arbiter/*.cpp)
	@echo "[MAKE] Building arbiter..."
	$(CXX) $(CXXFLAGS) arbiter/*.cpp -o $(BINDIR)/arbiter $(LIBS)
	@echo "[MAKE] arbiter built."

# ── HIP ───────────────────────────────────────────────────────────────────────
# Compiles all files in hip/ (including hip.cpp and shm_client.cpp).
$(BINDIR)/hip: sync_headers $(wildcard hip/*.cpp)
	@echo "[MAKE] Building hip..."
	$(CXX) $(CXXFLAGS) hip/*.cpp -o $(BINDIR)/hip $(LIBS)
	@echo "[MAKE] hip built."

# ── ASP ───────────────────────────────────────────────────────────────────────
# Compiles all files in asp/ (including asp.cpp and shm_client.cpp).
$(BINDIR)/asp: sync_headers $(wildcard asp/*.cpp)
	@echo "[MAKE] Building asp..."
	$(CXX) $(CXXFLAGS) asp/*.cpp -o $(BINDIR)/asp $(LIBS)
	@echo "[MAKE] asp built."

clean:
	rm -rf $(BINDIR)
	rm -f hip/shared_types.h asp/shared_types.h

run: all
	./bin/arbiter & sleep 0.5 && ./bin/hip & ./bin/asp & wait

.PHONY: all sync_headers clean run