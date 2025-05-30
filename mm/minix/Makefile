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
break.s:	$h/signal.hpp
break.s:	glo.hpp
break.s:	mproc.hpp
break.s:	param.hpp

exec.s:		const.hpp $h/const.hpp $h/type.hpp
exec.s:		$h/callnr.hpp
exec.s:		$h/error.hpp
exec.s:		$h/stat.hpp
exec.s:		glo.hpp
exec.s:		mproc.hpp
exec.s:		param.hpp

forkexit.s:	const.hpp $h/const.hpp $h/type.hpp
forkexit.s:	$h/callnr.hpp
forkexit.s:	$h/error.hpp
forkexit.s:	glo.hpp
forkexit.s:	mproc.hpp
forkexit.s:	param.hpp

getset.s:	const.hpp $h/const.hpp $h/type.hpp
getset.s:	$h/callnr.hpp
getset.s:	$h/error.hpp
getset.s:	glo.hpp
getset.s:	mproc.hpp
getset.s:	param.hpp

main.s:		const.hpp $h/const.hpp $h/type.hpp
main.s:		$h/callnr.hpp
main.s:		$h/com.hpp
main.s:		$h/error.hpp
main.s:		glo.hpp
main.s:		mproc.hpp
main.s:		param.hpp

putc.s:		$h/const.hpp $h/type.hpp
putc.s:		$h/com.hpp

signal.s:	const.hpp $h/const.hpp $h/type.hpp
signal.s:	$h/callnr.hpp
signal.s:	$h/com.hpp
signal.s:	$h/error.hpp
signal.s:	$h/signal.hpp
signal.s:	$h/stat.hpp
signal.s:	glo.hpp
signal.s:	mproc.hpp
signal.s:	param.hpp

table.s:	const.hpp $h/const.hpp $h/type.hpp
table.s:	$h/callnr.hpp
table.s:	glo.hpp
table.s:	mproc.hpp
table.s:	param.hpp

utility.s:	const.hpp $h/const.hpp $h/type.hpp
utility.s:	$h/callnr.hpp
utility.s:	$h/com.hpp
utility.s:	$h/error.hpp
utility.s:	$h/stat.hpp
utility.s:	glo.hpp
utility.s:	mproc.hpp
