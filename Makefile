# ─────────────────────────────────────────────────────────────────────────────
#  DLRBHeapVector — Makefile
# ─────────────────────────────────────────────────────────────────────────────

# ── Toolchain ─────────────────────────────────────────────────────────────────
CXX      ?= c++
TARGET    = test_dlrb
DEMO      = draco_queue

# ── Directories ───────────────────────────────────────────────────────────────
SRCDIR    = test
DEMODIR   = demo
INCDIR    = include
BUILDDIR  = build

SRC       = $(SRCDIR)/test_dlrb.cpp
OBJ       = $(BUILDDIR)/test_dlrb.o

DEMO_SRC  = $(DEMODIR)/draco_queue.cpp
DEMO_OBJ  = $(BUILDDIR)/draco_queue.o

# ── Flags shared by all builds ────────────────────────────────────────────────
CXXFLAGS_COMMON  = -std=c++17 -Wall -Wextra -Wpedantic -Wshadow \
                   -Wno-unused-parameter -I$(INCDIR)

# ── Per-mode flags ────────────────────────────────────────────────────────────
# Selectable via:  make MODE=release  /  make MODE=debug  /  make MODE=asan
MODE     ?= debug

ifeq ($(MODE),release)
    CXXFLAGS  = $(CXXFLAGS_COMMON) -O3 -DNDEBUG
    LDFLAGS   =
else ifeq ($(MODE),asan)
    CXXFLAGS  = $(CXXFLAGS_COMMON) -O1 -g -fsanitize=address,undefined
    LDFLAGS   = -fsanitize=address,undefined
else                          # debug (default)
    CXXFLAGS  = $(CXXFLAGS_COMMON) -O0 -g3
    LDFLAGS   =
endif

# ─────────────────────────────────────────────────────────────────────────────
#  Targets
# ─────────────────────────────────────────────────────────────────────────────

.PHONY: all run demo demo_run clean release asan help

all: $(TARGET) $(DEMO)

# ── Test suite ────────────────────────────────────────────────────────────────

$(TARGET): $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^
	@echo "  Linked → $(TARGET)  [MODE=$(MODE)]"

$(OBJ): $(SRC) $(INCDIR)/dlrb_heap_vector.hpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<
	@echo "  Compiled $<"

run: $(TARGET)
	@echo "\n── Running $(TARGET) ──────────────────────────────────"
	./$(TARGET)

# ── Demo — Ministry of Dracological Incident Management ──────────────────────

$(DEMO): $(DEMO_OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^
	@echo "  Linked → $(DEMO)  [MODE=$(MODE)]"

$(DEMO_OBJ): $(DEMO_SRC) $(INCDIR)/dlrb_heap_vector.hpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<
	@echo "  Compiled $<"

demo: $(DEMO)

demo_run: $(DEMO)
	@echo "\n── Running $(DEMO) ──────────────────────────────────"
	./$(DEMO)

# ── Utility ───────────────────────────────────────────────────────────────────

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

release:
	$(MAKE) MODE=release

asan:
	$(MAKE) MODE=asan

clean:
	rm -rf $(BUILDDIR) $(TARGET) $(DEMO)
	@echo "  Cleaned."

help:
	@echo "Targets:"
	@echo "  make              — build tests + demo (debug, default)"
	@echo "  make run          — build + run test suite"
	@echo "  make demo         — build the Ministry of Dracological demo"
	@echo "  make demo_run     — build + run the demo"
	@echo "  make release      — optimised build (MODE=release)"
	@echo "  make asan         — AddressSanitizer + UBSan build (MODE=asan)"
	@echo "  make clean        — remove all build artefacts"
	@echo ""
	@echo "Override compiler:  make CXX=g++"
