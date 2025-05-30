# To make  'test0', type:   make f=test0
# To make  'test1', type:   make f=test1
# To make  'test2', type:   make f=test2
# Get the idea?

# Use clang and clang++ for building the test programs
CC ?= clang
CXX ?= clang++
# Base compiler flags; users may extend these when invoking make
CFLAGS ?= -O
CXXFLAGS ?= -std=c++23 -O
# Always search the shared include directory
CFLAGS += -I$(INC)
CXXFLAGS += -I$(INC)
INC ?= ../include
LIB ?= ../lib/lib.a
LDFLAGS ?=

# Build the requested test program. The variable 'f' specifies the base
# filename and may correspond to either a .c or a .cpp file.
file: $(LIB)
	@if [ -f $(f).cpp ]; then \
		$(CXX) $(CXXFLAGS) -o $(f) $(f).cpp $(LIB) $(LDFLAGS); \
	else \
		$(CC) $(CFLAGS) -o $(f) $(f).c $(LIB) $(LDFLAGS); \
	fi

