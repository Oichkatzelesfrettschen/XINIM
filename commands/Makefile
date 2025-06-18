# To make  'cp', type:   make f=cp
# To make  'ls', type:   make f=ls
# To make  'cat', type:   make f=cat
# Get the idea?

l=../lib
INC ?= ../include
LIB ?= ../lib/lib.a
LDFLAGS ?=
# Use clang to compile commands
CC ?= clang
 
CFLAGS ?= -F
CFLAGS += -I$(INC)

file:	$(LIB) $(f).s
	@$(CC) $(CFLAGS) $(LDFLAGS) -o bin/$f $f.s $(LIB)
	@chmem =2048 bin/$f >/dev/null
	@echo "$f done ."

mined:	mined1.s mined2.s $(INC)/mined.h
	@asld $(LDFLAGS) -o bin/mined $(l)/crtso.s mined1.s mined2.s $(LIB) $(l)/end.s

shobj = sh1.s sh2.s sh3.s sh4.s sh5.s
sh:	$(shobj) $(INC)/sh.h
	@asld $(LDFLAGS) -o bin/sh $(l)/crtso.s $(shobj) $(LIB) $(l)/end.s

