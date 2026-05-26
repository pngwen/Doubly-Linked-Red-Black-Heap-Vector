# ─────────────────────────────────────────────────────────────────────────────
#  DLRBHeapVector — Makefile
# ─────────────────────────────────────────────────────────────────────────────

# ── Toolchain ──────────────────────────────────────────────────────────────────
CXX      ?= c++
TARGET    = test_dlrb

# ── Directories ───────────────────────────────────────────────────────────────
SRCDIR    = test
INCDIR    = include
BUILDDIR  = build

SRC       = $(SRCDIR)/test_dlrb.cpp
OBJ       = $(BUILDDIR)/test_dlrb.o

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

.PHONY: all run clean release asan help

all: $(TARGET)

# Link
$(TARGET): $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^
	@echo "  Linked → $(TARGET)  [MODE=$(MODE)]"

# Compile
$(OBJ): $(SRC) $(INCDIR)/dlrb_heap_vector.hpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<
	@echo "  Compiled $<"

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Convenience shortcuts
run: all
	@echo "\n── Running $(TARGET) ──────────────────────────────────"
	./$(TARGET)

release:
	$(MAKE) MODE=release

asan:
	$(MAKE) MODE=asan

clean:
	rm -rf $(BUILDDIR) $(TARGET)
	@echo "  Cleaned."

help:
	@echo "Targets:"
	@echo "  make            — debug build (default)"
	@echo "  make run        — build + run tests"
	@echo "  make release    — optimised build (MODE=release)"
	@echo "  make asan       — AddressSanitizer + UBSan build (MODE=asan)"
	@echo "  make clean      — remove build artefacts"
	@echo ""
	@echo "Override compiler:  make CXX=g++"
