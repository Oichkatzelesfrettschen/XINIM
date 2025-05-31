#Use clang to compile mm
CC ? = clang CFLAGS
           ? = -O INC
                   ? =../ include LIB
                          ? =../ lib / lib.a LDFLAGS
                                 ? = CFLAGS +=
                                   -I$(INC) h =../ h l =../ lib

                                                                obj =
                                                            main.o forkexit.o break.o exec.o signal
                                                                .o getset.o alloc.o utility.o table
                                                                .o paging.o vm.o putc.o

                                                                    mm
                                 : Makefile $l / head.o $(obj) $(LIB) $l /
                                           end.o @echo "Start linking MM" @ld $(LDFLAGS) -
                                       o mm $l / head.o $(obj) $(LIB) $l /
                                           end.o @echo "MM done"

                                           alloc.o
                          : const.hpp $h / const.hpp $h /
                                type.hpp

                                break.o
                   : const.hpp $h / const.hpp $h / type.hpp break.o
           : $h / error.hpp break.o
   : $h /
         signal.h break.o : glo.h break.o : mproc.h break.o : param
                                                                  .h

                                                                      exec.o : const.hpp $h /
         const.hpp $h / type.hpp exec.o : $h / callnr.hpp exec.o : $h / error.hpp exec.o : $h /
         stat.h exec.o : glo.h exec.o : mproc.h exec.o : param
                                                             .h

                                                                 forkexit.o : const.hpp $h /
         const.hpp $h / type.hpp forkexit.o : $h / callnr.hpp forkexit.o : $h /
         error.hpp forkexit.o : glo.h forkexit.o : mproc.h forkexit.o : param
                                                                            .h

                                                                                getset.o
    : const.hpp $h /
         const.hpp $h / type.hpp getset.o : $h / callnr.hpp getset.o : $h /
         error.hpp getset.o : glo.h getset.o : mproc.h getset.o : param
                                                                      .h

                                                                          main.o : const.hpp $h /
         const.hpp $h / type.hpp main.o : $h / callnr.hpp main.o : $h / com.hpp main.o : $h /
         error.hpp main.o : glo.h main.o : mproc.h main.o : param.h paging.o : const.hpp../ include /
         paging.h vm.o : const.hpp../ include /
         vm.h mproc
             .h

                 putc.o : $h /
         const.hpp $h / type.hpp putc.o : $h /
         com.hpp

             signal.o : const.hpp $h /
         const.hpp $h / type.hpp signal.o : $h / callnr.hpp signal.o : $h / com.hpp signal.o : $h /
         error.hpp signal.o : $h / signal.h signal.o : $h /
         stat.h signal.o : glo.h signal.o : mproc.h signal.o : param
                                                                   .h

                                                                       table.o : const.hpp $h /
         const.hpp $h / type.hpp table.o : $h /
         callnr.hpp table.o : glo.h table.o : mproc.h table.o : param
                                                                  .h

                                                                      utility.o : const.hpp $h /
         const.hpp $h / type.hpp utility.o : $h / callnr.hpp utility.o : $h / com.hpp utility.o : $h /
         error.hpp utility.o : $h / stat.h utility.o : glo.h utility.o : mproc.h
