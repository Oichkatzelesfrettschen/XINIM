#Use clang for the 16 - bit fs build
CC ? = clang INC
     ? =../ include LIB
       ? =../ lib / lib.a LDFLAGS
         ? = CFLAGS
                 ? = -w - F - T.CFLAGS +=
                   -I$(INC)
                       h =../ h l =../ lib

                                           obj =
                                       main.s open.s read.s write.s pipe.s device.s path.s mount
                                           .s link.s super.s inode.s cache.s filedes.s stadir
                                           .s protect.s time.s misc.s utility.s table.s putc.s

                                               fs
                 : Makefile $l / head.s $(obj) $(LIB) $l /
                           end.s @echo
                           "Start linking FS.  /lib/cem will be removed to make space on RAM disk"
                           @rm -
                       f / lib / cem / tmp/*
	asld $(LDFLAGS) -o fs $l/head.s $(obj) $(LIB) $l/end.s
	@echo "FS done.  Please restore /lib/cem manually"

cache.s:	const.hpp type.hpp $h/const.hpp $h/type.hpp
cache.s:	$h/error.hpp
cache.s:	buf.hpp
cache.s:	file.hpp
cache.s:	fproc.hpp
cache.s:	glo.hpp
cache.s:	inode.hpp
cache.s:	super.hpp

device.s:	const.hpp type.hpp $h/const.hpp $h/type.hpp
device.s:	$h/com.hpp
device.s:	$h/error.hpp
device.s:	dev.hpp
device.s:	file.hpp
device.s:	fproc.hpp
device.s:	glo.hpp
device.s:	inode.hpp
device.s:	param.hpp

filedes.s:	const.hpp type.hpp $h/const.hpp $h/type.hpp
filedes.s:	$h/error.hpp
filedes.s:	file.hpp
filedes.s:	fproc.hpp
filedes.s:	glo.hpp
filedes.s:	inode.hpp

inode.s:	const.hpp type.hpp $h/const.hpp $h/type.hpp
inode.s:	$h/error.hpp
inode.s:	buf.hpp
inode.s:	file.hpp
inode.s:	fproc.hpp
inode.s:	glo.hpp
inode.s:	inode.hpp
inode.s:	super.hpp

link.s:		const.hpp type.hpp $h/const.hpp $h/type.hpp
link.s:		$h/error.hpp
link.s:		buf.hpp
link.s:		file.hpp
link.s:		fproc.hpp
link.s:		glo.hpp
link.s:		inode.hpp
link.s:		param.hpp

main.s:		const.hpp type.hpp $h/const.hpp $h/type.hpp
main.s:		$h/callnr.hpp
main.s:		$h/com.hpp
main.s:		$h/error.hpp
main.s:		buf.hpp
main.s:		file.hpp
main.s:		fproc.hpp
main.s:		glo.hpp
main.s:		inode.hpp
main.s:		param.hpp
main.s:		super.hpp

misc.s:		const.hpp type.hpp $h/const.hpp $h/type.hpp
misc.s:		$h/callnr.hpp
misc.s:		$h/com.hpp
misc.s:		$h/error.hpp
misc.s:		buf.hpp
misc.s:		file.hpp
misc.s:		fproc.hpp
misc.s:		glo.hpp
misc.s:		inode.hpp
misc.s:		param.hpp
misc.s:		super.hpp

mount.s:	const.hpp type.hpp $h/const.hpp $h/type.hpp
mount.s:	$h/error.hpp
mount.s:	buf.hpp
mount.s:	file.hpp
mount.s:	fproc.hpp
mount.s:	glo.hpp
mount.s:	inode.hpp
mount.s:	param.hpp
mount.s:	super.hpp

open.s:		const.hpp type.hpp $h/const.hpp $h/type.hpp
open.s:		$h/callnr.hpp
open.s:		$h/error.hpp
open.s:		buf.hpp
open.s:		file.hpp
open.s:		fproc.hpp
open.s:		glo.hpp
open.s:		inode.hpp
open.s:		param.hpp

path.s:		const.hpp type.hpp $h/const.hpp $h/type.hpp
path.s:		$h/error.hpp
path.s:		buf.hpp
path.s:		file.hpp
path.s:		fproc.hpp
path.s:		glo.hpp
path.s:		inode.hpp
path.s:		super.hpp

pipe.s:		const.hpp type.hpp $h/const.hpp $h/type.hpp
pipe.s:		$h/callnr.hpp
pipe.s:		$h/com.hpp
pipe.s:		$h/error.hpp
pipe.s:		$h/signal.hpp
pipe.s:		file.hpp
pipe.s:		fproc.hpp
pipe.s:		glo.hpp
pipe.s:		inode.hpp
pipe.s:		param.hpp

protect.s:	const.hpp type.hpp $h/const.hpp $h/type.hpp
protect.s:	$h/error.hpp
protect.s:	buf.hpp
protect.s:	file.hpp
protect.s:	fproc.hpp
protect.s:	glo.hpp
protect.s:	inode.hpp
protect.s:	param.hpp
protect.s:	super.hpp

putc.s:		const.hpp type.hpp $h/const.hpp $h/type.hpp
putc.s:		$h/com.hpp

read.s:		const.hpp type.hpp $h/const.hpp $h/type.hpp
read.s:		$h/com.hpp
read.s:		$h/error.hpp
read.s:		buf.hpp
read.s:		file.hpp
read.s:		fproc.hpp
read.s:		glo.hpp
read.s:		inode.hpp
read.s:		param.hpp
read.s:		super.hpp

stadir.s:	const.hpp type.hpp $h/const.hpp $h/type.hpp
stadir.s:	$h/error.hpp
stadir.s:	$h/stat.hpp
stadir.s:	file.hpp
stadir.s:	fproc.hpp
stadir.s:	glo.hpp
stadir.s:	inode.hpp
stadir.s:	param.hpp

super.s:	const.hpp type.hpp $h/const.hpp $h/type.hpp
super.s:	$h/error.hpp
super.s:	buf.hpp
super.s:	inode.hpp
super.s:	super.hpp

table.s:	const.hpp type.hpp $h/const.hpp $h/type.hpp
table.s:	$h/com.hpp
table.s:	$h/callnr.hpp
table.s:	$h/error.hpp
table.s:	$h/stat.hpp
table.s:	buf.hpp
table.s:	dev.hpp
table.s:	file.hpp
table.s:	fproc.hpp
table.s:	glo.hpp
table.s:	inode.hpp
table.s:	super.hpp

time.s:		const.hpp type.hpp $h/const.hpp $h/type.hpp
time.s:		$h/callnr.hpp
time.s:		$h/com.hpp
time.s:		$h/error.hpp
time.s:		file.hpp
time.s:		fproc.hpp
time.s:		glo.hpp
time.s:		inode.hpp
time.s:		param.hpp

utility.s:	const.hpp type.hpp $h/const.hpp $h/type.hpp
utility.s:	$h/com.hpp
utility.s:	$h/error.hpp
utility.s:	buf.hpp
utility.s:	file.hpp
utility.s:	fproc.hpp
utility.s:	glo.hpp
utility.s:	inode.hpp
utility.s:	param.hpp
utility.s:	super.hpp

write.s:	const.hpp type.hpp $h/const.hpp $h/type.hpp
write.s:	$h/error.hpp
write.s:	buf.hpp
write.s:	file.hpp
write.s:	fproc.hpp
write.s:	glo.hpp
write.s:	inode.hpp
write.s:	super.hpp
