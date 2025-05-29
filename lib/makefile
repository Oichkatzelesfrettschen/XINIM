CC ?= cc
AS ?= nasm
AR ?= ar
CFLAGS ?= -O2
ASMFLAGS ?= -f elf64
OBJS := $(patsubst %.c,%.o,$(wildcard *.c)) \
         $(patsubst %.s,%.o,$(wildcard *.s))

lib.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

%.o: %.s
	$(AS) $(ASMFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) lib.a
