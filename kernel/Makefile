# SPDX-License-Identifier: MIT
# ---------------------------------------------------------------------------
#  Kernel Makefile (reconciled 2025-06-23)
#
#  • Default driver: PC/XT  (xt_wini.cpp)
#  • Override with:   make WINI_DRIVER=at
#                     make WINI_DRIVER=pc
#  • Legacy knobs     DRIVER_AT=ON / DRIVER_PC=ON are still honoured.
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# 1. Tool-chain
# ---------------------------------------------------------------------------
CXX      ?= clang++
CXXFLAGS ?= -O2 -std=c++23 -ffreestanding -fno-exceptions -fno-rtti \
            -Wall -Wextra -Werror -I../include -I../h -I.
LDFLAGS  ?= -static -nostdlib -T ../linker.ld
LIB       = ../lib/lib.a

# ---------------------------------------------------------------------------
# 2. Driver selection logic
# ---------------------------------------------------------------------------
WINI_DRIVER ?= pc          # default
ifeq ($(DRIVER_AT),ON)
  ifeq ($(DRIVER_PC),ON)
    $(error Select only one of DRIVER_AT or DRIVER_PC)
  endif
  WINI_DRIVER := at
else ifeq ($(DRIVER_PC),ON)
  WINI_DRIVER := pc
endif

ifeq ($(WINI_DRIVER),at)
  WINI_SRC := at_wini.cpp
else ifeq ($(WINI_DRIVER),pc)
  WINI_SRC := xt_wini.cpp
else
  $(error Invalid WINI_DRIVER '$(WINI_DRIVER)'; use 'pc' or 'at')
endif

# ---------------------------------------------------------------------------
# 3. Source lists
# ---------------------------------------------------------------------------
SRCS := \
  clock.cpp dmp.cpp floppy.cpp main.cpp memory.cpp printer.cpp proc.cpp \
  system.cpp table.cpp tty.cpp idt64.cpp mpx64.cpp klib64.cpp klib88.cpp \
  mpx88.cpp paging.cpp wormhole.cpp lattice_ipc.cpp net_driver.cpp \
  wait_graph.cpp pqcrypto.cpp syscall.cpp service.cpp $(WINI_SRC)

OBJS := $(SRCS:.cpp=.o)
DEPS := $(OBJS:.o=.d)

# ---------------------------------------------------------------------------
# 4. Targets
# ---------------------------------------------------------------------------
.PHONY: all clean
all: kernel

kernel: $(OBJS) $(LIB)
	@echo "[LD]  $@"
	$(CXX) $(LDFLAGS) $(OBJS) $(LIB) -o $@

-include $(DEPS)

# ---------------------------------------------------------------------------
# 5. Pattern rule (C++ → object + depfile)
# ---------------------------------------------------------------------------
%.o: %.cpp
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

# ---------------------------------------------------------------------------
# 6. Maintenance
# ---------------------------------------------------------------------------
clean:
	rm -f $(OBJS) $(DEPS) kernel
