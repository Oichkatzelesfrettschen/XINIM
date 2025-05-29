# Default to clang for building the library
CC ?= clang
AR ?= ar
CFLAGS ?= -O2
# Object files derived only from C sources.
OBJS := $(patsubst %.c,%.o,$(wildcard *.c))

lib.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) lib.a
