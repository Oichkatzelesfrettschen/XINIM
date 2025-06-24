# Default to clang-18 for building the library
CC ?= clang-18
AR ?= ar
CFLAGS ?= -O2 -march=x86-64-v1 -msse2 -msse3 -mssse3 -msse4.1 -msse4.2
# Object files derived only from C sources.
OBJS := $(patsubst %.cpp,%.o,$(wildcard *.cpp))

lib.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) lib.a
