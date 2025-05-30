# Use clang to compile mm
CC ?= clang
CFLAGS ?= -O
INC ?= ../include
LIB ?= ../lib/lib.a
LDFLAGS ?=
CFLAGS += -I$(INC)
h=../h
l=../lib

obj =	main.o forkexit.o break.o exec.o signal.o getset.o  \
	alloc.o utility.o table.o paging.o vm.o putc.o 

mm:     Makefile  $l/head.o $(obj) $(LIB) $l/end.o
	@echo "Start linking MM"
	@ld $(LDFLAGS) -o mm $l/head.o $(obj) $(LIB) $l/end.o
	@echo "MM done"


alloc.o:	const.hpp $h/const.hpp $h/type.hpp

break.o:	const.hpp $h/const.hpp $h/type.hpp
break.o:	$h/error.hpp
break.o:	$h/signal.hpp
break.o:	glo.hpp
break.o:	mproc.hpppp
break.o:	param.hpp

exec.o:		const.hpp $h/const.hpp $h/type.hpp
exec.o:		$h/callnr.hpp
exec.o:		$h/error.hpp
exec.o:		$h/stat.hpp
exec.o:		glo.hpp
exec.o:		mproc.hpppp
exec.o:		param.hpp

forkexit.o:	const.hpp $h/const.hpp $h/type.hpp
forkexit.o:	$h/callnr.hpp
forkexit.o:	$h/error.hpp
forkexit.o:	glo.hpp
forkexit.o:	mproc.hpppp
forkexit.o:	param.hpp

getset.o:	const.hpp $h/const.hpp $h/type.hpp
getset.o:	$h/callnr.hpp
getset.o:	$h/error.hpp
getset.o:	glo.hpp
getset.o:	mproc.hpppp
getset.o:	param.hpp

main.o:		const.hpp $h/const.hpp $h/type.hpp
main.o:		$h/callnr.hpp
main.o:		$h/com.hpp
main.o:		$h/error.hpp
main.o:		glo.hpp
main.o:		mproc.hpppp
main.o:		param.hpp
paging.o:       const.hpp ../include/paging.hpp
vm.o:           const.hpp ../include/vm.hpp mproc.hpppp

putc.o:		$h/const.hpp $h/type.hpp
putc.o:		$h/com.hpp

signal.o:	const.hpp $h/const.hpp $h/type.hpp
signal.o:	$h/callnr.hpp
signal.o:	$h/com.hpp
signal.o:	$h/error.hpp
signal.o:	$h/signal.hpp
signal.o:	$h/stat.hpp
signal.o:	glo.hpp
signal.o:	mproc.hpppp
signal.o:	param.hpp

table.o:	const.hpp $h/const.hpp $h/type.hpp
table.o:	$h/callnr.hpp
table.o:	glo.hpp
table.o:	mproc.hpppp
table.o:	param.hpp

utility.o:	const.hpp $h/const.hpp $h/type.hpp
utility.o:	$h/callnr.hpp
utility.o:	$h/com.hpp
utility.o:	$h/error.hpp
utility.o:	$h/stat.hpp
utility.o:	glo.hpp
utility.o:	mproc.hpppp
