	# SPDX-License-Identifier: MIT
# Simplified and modernized build system for the kernel.
#
# Use `make WINI_DRIVER=at` or `make WINI_DRIVER=pc` to select the driver
# implementation. The default is the PC/XT driver (xt_wini.cpp).

# -----------------------------------------------------------------------------
# Toolchain configuration
# -----------------------------------------------------------------------------
CXX = clang++
CXXFLAGS ?= -O2 -std=c++23 -ffreestanding -fno-exceptions -fno-rtti \
            -Wall -Wextra -Werror -I../include -I../h -I.
LDFLAGS  ?= -static -nostdlib -T ../linker.ld

LIB      := ../lib/lib.a

# -----------------------------------------------------------------------------
# Driver selection
# -----------------------------------------------------------------------------
WINI_DRIVER ?= pc
ifeq ($(WINI_DRIVER),at)
WINI_SRC := at_wini.cpp
else
WINI_SRC := xt_wini.cpp
endif

# -----------------------------------------------------------------------------
# Sources and objects
# -----------------------------------------------------------------------------
SRCS := \
    clock.cpp dmp.cpp floppy.cpp main.cpp memory.cpp printer.cpp proc.cpp \
    system.cpp table.cpp tty.cpp idt64.cpp mpx64.cpp klib64.cpp klib88.cpp \
    mpx88.cpp paging.cpp wormhole.cpp lattice_ipc.cpp net_driver.cpp \
    wait_graph.cpp pqcrypto.cpp syscall.cpp service.cpp $(WINI_SRC)

OBJS := $(SRCS:.cpp=.o)
DEPS := $(OBJS:.o=.d)

# -----------------------------------------------------------------------------
# Default target
# -----------------------------------------------------------------------------
all: kernel

kernel: $(OBJS) $(LIB)
	@echo "[LD] $@"
	$(CXX) $(LDFLAGS) $(OBJS) $(LIB) -o $@

-include $(DEPS)

# -----------------------------------------------------------------------------
# Pattern rules
# -----------------------------------------------------------------------------
%.o: %.cpp
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

# -----------------------------------------------------------------------------
# Maintenance targets
# -----------------------------------------------------------------------------
.PHONY: clean
clean:
	rm -f $(OBJS) $(DEPS) kernel
