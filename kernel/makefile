# The kernel directory contains PC/XT and AT wini driver sources.  Set the
# `WINI_DRIVER` variable to `pc` or `at` when invoking `make` to select the
# correct driver.  If not specified the PC/XT driver is used.
CFLAGS = -O
ASM ?= nasm
ASMFLAGS ?= -f elf64
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
WINI_SRC = at_wini.c
else
WINI_SRC = xt_wini.c
endif

obj = mpx64.o idt64.o main.o proc.o system.o tty.o clock.o memory.o floppy.o wini.o \
      printer.o table.o klib64.o dmp.o paging.o

kernel:	makefile $(obj) $l/libc.a
	@echo "Start linking Kernel"
	@ld -o kernel  $(obj) $l/libc.a $l/end.o
	@echo "Kernel done"

klib64.o:	klib64.s
	$(ASM) $(ASMFLAGS) -o $@ $<

mpx64.o:        mpx64.s
	$(ASM) $(ASMFLAGS) -o $@ $<

idt64.o:        idt64.c
	$(CC) $(CFLAGS) -c -o idt64.o idt64.c


clock.o:	const.h type.h $h/const.h $h/type.h
clock.o:	$h/callnr.h
clock.o:	$h/com.h
clock.o:	$h/error.h
clock.o:	$h/signal.h
clock.o:	glo.h
clock.o:	proc.h

floppy.o:	const.h type.h $h/const.h $h/type.h
floppy.o:	$h/callnr.h
floppy.o:	$h/com.h
floppy.o:	$h/error.h
floppy.o:	glo.h
floppy.o:	proc.h


dmp.o:		const.h type.h $h/const.h $h/type.h
dmp.o:		$h/callnr.h
dmp.o:		$h/com.h
dmp.o:		$h/error.h
dmp.o:		glo.h
dmp.o:		proc.h

paging.o:       const.h type.h ../include/paging.h
main.o:		const.h type.h $h/const.h $h/type.h
main.o:		$h/callnr.h
main.o:		$h/com.h
main.o:		$h/error.h
main.o:		glo.h
main.o:		proc.h

memory.o:	const.h type.h $h/const.h $h/type.h
memory.o:	$h/callnr.h
memory.o:	$h/com.h
memory.o:	$h/error.h
memory.o:	proc.h

printer.o:	const.h type.h $h/const.h $h/type.h
printer.o:	$h/callnr.h
printer.o:	$h/com.h
printer.o:	$h/error.h

proc.o:		const.h type.h $h/const.h $h/type.h
proc.o:		$h/callnr.h
proc.o:		$h/com.h
proc.o:		$h/error.h
proc.o:		glo.h
proc.o:		proc.h

system.o:	const.h type.h $h/const.h $h/type.h
system.o:	$h/callnr.h
system.o:	$h/com.h
system.o:	$h/error.h
system.o:	$h/signal.h
system.o:	glo.h
system.o:	proc.h

table.o:	const.h type.h $h/const.h $h/type.h
table.o:	glo.h
table.o:	proc.h

tty.o:	const.h type.h $h/const.h $h/type.h
tty.o:	$h/callnr.h
tty.o:	$h/com.h
tty.o:	$h/error.h
tty.o:	$h/sgtty.h
tty.o:	$h/signal.h
tty.o:	glo.h
tty.o:	proc.h

wini.o: $(WINI_SRC) const.h type.h $h/const.h $h/type.h
wini.o: $h/callnr.h
wini.o: $h/com.h
wini.o: $h/error.h
wini.o: proc.h
	$(CC) $(CFLAGS) -c $(WINI_SRC) -o $@
