# The kernel directory contains PC/XT and AT wini driver sources.
# Set the WINI_DRIVER variable to 'pc' or 'at' when invoking make to
# select the correct driver. If not specified the PC/XT driver is used.
# Use clang to compile the kernel

CC ?= clang
CFLAGS ?= -O2
INC ?= ../include
LIB ?= ../lib/lib.a
LDFLAGS ?=

CFLAGS += -I$(INC)
h=../h
l=../lib

# Determine which driver source file to use
WINI_DRIVER ?= pc
ifeq ($(DRIVER_AT),ON)
	ifeq ($(DRIVER_PC),ON)
		$(error Select only one of DRIVER_AT or DRIVER_PC)
	endif
	WINI_DRIVER = at
else ifeq ($(DRIVER_PC),ON)
	WINI_DRIVER = pc
endif

ifeq ($(WINI_DRIVER),at)
WINI_SRC = at_wini.cpp
else
WINI_SRC = xt_wini.cpp
endif

obj = mpx64.o idt64.o main.o proc.o system.o tty.o clock.o memory.o floppy.o \
	wini.o printer.o table.o klib64.o dmp.o paging.o syscall.o

kernel: Makefile $(obj) $(LIB)
	@echo "Start linking Kernel"
	@ld $(LDFLAGS) -o kernel $(obj) $(LIB) $(l)/end.o
	@echo "Kernel done"

# Compile klib64 from its C++ source
klib64.o: klib64.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

mpx64.o: mpx64.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

idt64.o: idt64.cpp
	$(CC) $(CFLAGS) -c -o idt64.o idt64.cpp

clock.o: const.hpp type.hpp $(h)/const.hpp $(h)/type.hpp \
	$(h)/callnr.hpp $(h)/com.hpp $(h)/error.hpp $(h)/signal.hpp \
	glo.hpp proc.hpp

floppy.o: const.hpp type.hpp $(h)/const.hpp $(h)/type.hpp \
	$(h)/callnr.hpp $(h)/com.hpp $(h)/error.hpp \
	glo.hpp proc.hpp

dmp.o: const.hpp type.hpp $(h)/const.hpp $(h)/type.hpp \
	$(h)/callnr.hpp $(h)/com.hpp $(h)/error.hpp \
	glo.hpp proc.hpp

paging.o: const.hpp type.hpp ../include/paging.hpp

main.o: const.hpp type.hpp $(h)/const.hpp $(h)/type.hpp \
	$(h)/callnr.hpp $(h)/com.hpp $(h)/error.hpp \
	glo.hpp proc.hpp

memory.o: const.hpp type.hpp $(h)/const.hpp $(h)/type.hpp \
	$(h)/callnr.hpp $(h)/com.hpp $(h)/error.hpp \
	proc.hpp

printer.o: const.hpp type.hpp $(h)/const.hpp $(h)/type.hpp \
	$(h)/callnr.hpp $(h)/com.hpp $(h)/error.hpp

proc.o: const.hpp type.hpp $(h)/const.hpp $(h)/type.hpp \
	$(h)/callnr.hpp $(h)/com.hpp $(h)/error.hpp \
	glo.hpp proc.hpp

system.o: const.hpp type.hpp $(h)/const.hpp $(h)/type.hpp \
	$(h)/callnr.hpp $(h)/com.hpp $(h)/error.hpp $(h)/signal.hpp \
	glo.hpp proc.hpp

table.o: const.hpp type.hpp $(h)/const.hpp $(h)/type.hpp \
	glo.hpp proc.hpp

tty.o: const.hpp type.hpp $(h)/const.hpp $(h)/type.hpp \
	$(h)/callnr.hpp $(h)/com.hpp $(h)/error.hpp \
	$(INC)/sgtty.hpp $(h)/signal.hpp glo.hpp proc.hpp

wini.o: $(WINI_SRC) const.hpp type.hpp $(h)/const.hpp $(h)/type.hpp \
	$(h)/callnr.hpp $(h)/com.hpp $(h)/error.hpp proc.hpp
	$(CC) $(CFLAGS) -c $(WINI_SRC) -o $@
