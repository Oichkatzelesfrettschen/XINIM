CC ?= cc
AS ?= as
AR ?= ar
CFLAGS ?= -O2
OBJS := $(patsubst %.c,%.o,$(wildcard *.c)) \
         $(patsubst %.s,%.o,$(wildcard *.s)) \
         $(patsubst %.asm,%.o,$(wildcard *.asm))

lib.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

%.o: %.s
	$(AS) -o $@ $<

%.o: %.asm
	$(AS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) lib.a
