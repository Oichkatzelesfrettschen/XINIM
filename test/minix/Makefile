# To make  'test0', type:   make f=test0
# To make  'test1', type:   make f=test1
# To make  'test2', type:   make f=test2
# Get the idea?
# Use clang for the 16-bit tests
CC ?= clang

l=../lib

INC ?= ../include
LIB ?= ../lib/lib.a
LDFLAGS ?=
CFLAGS ?= -F
CFLAGS += -I$(INC)

file:	$(LIB) $(f).s
	@asld $(LDFLAGS) -o $f $(l)/crtso.s  $(f).s $(LIB) $(l)/end.s

