# Use clang-18 for the legacy tooling
CC ?= clang-18
l=../lib
INC ?= ../include
LIB ?= ../lib/lib.a
LDFLAGS ?=
CFLAGS ?= -F
CFLAGS += -I$(INC) -march=x86-64-v1 -msse2 -msse3 -mssse3 -msse4.1 -msse4.2

all:
	make init
	make bootblok
	make build
	make mkfs
	make fsck

init:	$(LIB) init.s $l/head.s
	asld $(LDFLAGS) -o init  $l/head.s init.s $(LIB)  $l/end.s
	@echo init done.

# bootblok.cpp is the source of the MINIX boot block.  The bootblock is the
# first 512 bytes on the image file and on the boot diskette.  When bootblok.cpp
# is assembled, it generates a short binary file (less than 400 bytes) that
# must be stripped of its header and copied to the file bootblok.  The dd
# command below does this.  If everything has been done right, the bootblok
# file should begin with the following 8 words, in hex:
# c0b8 8e07 33d8 b8f6 2fe0 c08e ff33 00b9
# The exact procedure for going from bootblok.cpp to the binary file stripped of
# its header is somewhat operating system dependent.  Some assemblers make
# an object (.s) file; others make an a.sut file directly. If your boot 
# diskette does not start out by printing 'Booting MINIX 1.0' the problem is
# probably that you have not made a good boot block.
bootblok:	bootblok.cpp
	$(CC) $(CFLAGS) $(LDFLAGS) -o bootblok bootblok.cpp
	@echo bootblok done.

build:	build.cpp
	$(CC) $(CFLAGS) $(LDFLAGS) -o build build.cpp
	@echo build done.
mkfs: mkfs.cpp
	$(CC) $(CFLAGS) $(LDFLAGS) -DUNIX -o mkfs mkfs.cpp
	@echo mkfs done.
fsck:	fsck.s fsck1.s
	@echo "Start linking fsck.  /lib/cem will be removed to make space on RAM disk"
	asld $(LDFLAGS) -o fsck  fsck1.s fsck.s $(LIB) $l/end.s
	@echo fsck done. Please restore /lib/cem manually.
fsck.s: fsck.cpp
	$(CC) $(CFLAGS) -c fsck.cpp
	
# 'make image'  combines the bootblock, kernel, memory manager, file 
# system, init and fsck into a single file, called image.  Each of these pieces
# appears in the image file just as the original does on the disk, except that
# the header at the front is removed, and each piece is padded out to an
# integral multiple of 16 bytes.  Build also prints a message telling how big
# the various pieces (except fsck) are.
image:	build bootblok kernel mm fs init fsck
	/etc/umount /dev/fd0
	@echo "Insert blank disk in drive 0 and hit return"
	@getlf
	build bootblok kernel mm fs init fsck /dev/fd0

# 'make usr' builds a file system on a file called 'usr', and then
# copies this file to the diskette (9 sectors per track).  It builds the file
# system using the program mkfs, which is functionally the same as the standard
# UNIX mkfs program, except that sizes (in blocks) for block special files 
# must be provided.
usr:	mkfs proto.usr
	/etc/umount /dev/fd0
	@echo insert blank disk in drive 0 and hit return
	@getlf
	mkfs -l /dev/fd0 proto.usr >log.usr

user:	mkfs proto.user
	/etc/umount /dev/fd0
	@echo insert blank disk in drive 0 and hit return
	@getlf
	mkfs -l /dev/fd0 proto.user >log.user


# 'make ram' builds the root file system on a file called 'ram'.  This file
# will be copied to the boot diskette when 'image' is made.  During system boot
# it is copied from 128K on the boot diskette to the RAM disk.
ram:	mkfs proto.ram rc 
	/etc/umount /dev/fd0
	@echo "insert blank disk in drive 0 and hit return"
	mkfs -l /dev/fd0 proto.ram >log.ram
	getlf
