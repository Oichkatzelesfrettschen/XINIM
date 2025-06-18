#Use clang for the 16 - bit mm build
CC ? = clang INC
     ? =../ include LIB
       ? =../ lib / lib.a LDFLAGS
         ? = CFLAGS
                 ? = -w - F - T.CFLAGS +=
                   -I$(INC) h =../ h l =../ lib

                                                obj =
                                            main.s forkexit.s break.s exec.s signal.s getset.s alloc
                                                .s utility.s table.s putc.s

                                                    mm
                 : Makefile $l / head.s $(obj) $(LIB) $l /
                           end.s @echo
                           "Start linking MM.  /lib/cem will be removed to make space on RAM disk"
                           @rm -
                       f / lib / cem / tmp/*
	@asld $(LDFLAGS) -o mm $l/head.s $(obj) $(LIB) $l/end.s
	@echo "MM done.  Please restore /lib/cem manually"


alloc.s:	const.hpp $h/const.hpp $h/type.hpp

break.s:	const.hpp $h/const.hpp $h/type.hpp
break.s:	$h/error.hpp
break.s:	$h/signal.h
break.s:	glo.h
break.s:	mproc.h
break.s:	param.h

exec.s:		const.hpp $h/const.hpp $h/type.hpp
exec.s:		$h/callnr.hpp
exec.s:		$h/error.hpp
exec.s:		$h/stat.h
exec.s:		glo.h
exec.s:		mproc.h
exec.s:		param.h

forkexit.s:	const.hpp $h/const.hpp $h/type.hpp
forkexit.s:	$h/callnr.hpp
forkexit.s:	$h/error.hpp
forkexit.s:	glo.h
forkexit.s:	mproc.h
forkexit.s:	param.h

getset.s:	const.hpp $h/const.hpp $h/type.hpp
getset.s:	$h/callnr.hpp
getset.s:	$h/error.hpp
getset.s:	glo.h
getset.s:	mproc.h
getset.s:	param.h

main.s:		const.hpp $h/const.hpp $h/type.hpp
main.s:		$h/callnr.hpp
main.s:		$h/com.hpp
main.s:		$h/error.hpp
main.s:		glo.h
main.s:		mproc.h
main.s:		param.h

putc.s:		$h/const.hpp $h/type.hpp
putc.s:		$h/com.hpp

signal.s:	const.hpp $h/const.hpp $h/type.hpp
signal.s:	$h/callnr.hpp
signal.s:	$h/com.hpp
signal.s:	$h/error.hpp
signal.s:	$h/signal.h
signal.s:	$h/stat.h
signal.s:	glo.h
signal.s:	mproc.h
signal.s:	param.h

table.s:	const.hpp $h/const.hpp $h/type.hpp
table.s:	$h/callnr.hpp
table.s:	glo.h
table.s:	mproc.h
table.s:	param.h

utility.s:	const.hpp $h/const.hpp $h/type.hpp
utility.s:	$h/callnr.hpp
utility.s:	$h/com.hpp
utility.s:	$h/error.hpp
utility.s:	$h/stat.h
utility.s:	glo.h
utility.s:	mproc.h
