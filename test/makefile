# To make  'test0', type:   make f=test0
# To make  'test1', type:   make f=test1
# To make  'test2', type:   make f=test2
# Get the idea?

# Use clang for building the test programs
CC ?= clang
CFLAGS = -O -I../include

# Build the requested test program using the host C compiler.
# The variable 'f' specifies the base filename of the test.
file: $(f).c
	$(CC) $(CFLAGS) -o $(f) $(f).c

