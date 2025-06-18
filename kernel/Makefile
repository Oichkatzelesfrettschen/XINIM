#The kernel directory contains PC / XT and AT wini driver sources.Set the
# `WINI_DRIVER` variable to `pc` or `at` when invoking `make` to select the
#correct driver.If not specified the PC / XT driver is used.
#Use clang to compile the kernel
CC ? = clang CFLAGS
           ? = -O INC
                   ? =../ include LIB
                          ? =../ lib / lib.a LDFLAGS
                                 ? = CFLAGS += -I$(INC) h =
                                       ../
                                       h l =../ lib

#Determine which driver source file to use
                                                        WINI_DRIVER
                                                ? = pc ifeq(
                                                      $(DRIVER_AT),
                                                      ON) ifeq($(DRIVER_PC), ON)
                                                      $(error Select only one of DRIVER_AT or
                                                        DRIVER_PC) endif WINI_DRIVER =
                                                          at else ifeq($(DRIVER_PC),
                                                                       ON) WINI_DRIVER = pc endif

                                                              ifeq($(WINI_DRIVER), at) WINI_SRC =
                                                                  at_wini.cpp else WINI_SRC =
                                                                      xt_wini.cpp endif

                                                                          obj =
                                                                          mpx64.o idt64
                                       .o main.o proc.o system.o tty.o clock.o memory.o floppy
                                       .o wini.o printer.o table.o klib64.o dmp.o paging.o syscall.o

                                                                              kernel
                                                : Makefile $(obj) $(LIB) @echo
                                                      "Start linking Kernel" @ld $(LDFLAGS) -
                                                      o kernel $(obj) $(LIB) $l / end
                                       .o @echo "Kernel done"

#Compile klib64 from its C source
                                                                                  klib64.o
                                 : klib64.cpp $(CC) $(CFLAGS) - c - o $ @$ <

                                       mpx64.o
                          : mpx64.cpp $(CC) $(CFLAGS) - c - o $ @$ <

                                idt64.o
                   : idt64.cpp $(CC) $(CFLAGS) - c -
                         o idt64.o idt64
                             .cpp

                                 clock.o
           : const.hpp type.hpp $h / const.hpp $h / type.hpp clock.o
   : $h / callnr.hpp clock.o : $h / com.hpp clock.o : $h / error.hpp clock.o : $h /
             signal.h clock.o : glo.h clock.o : proc.h

                                                    floppy.o : const.hpp type.hpp $h /
             const.hpp $h / type.hpp floppy.o : $h / callnr.hpp floppy.o : $h / com.hpp floppy.o : $h /
             error.hpp floppy.o : glo.h floppy.o : proc.h

                                                       dmp.o : const.hpp type.hpp $h /
             const.hpp $h / type.hpp dmp.o : $h / callnr.hpp dmp.o : $h / com.hpp dmp.o : $h /
             error.hpp dmp.o : glo.h dmp.o : proc.h

                                                 paging.o : const.hpp type.hpp../
             include / paging.h main.o : const.hpp type.hpp $h / const.hpp $h / type.hpp main.o : $h /
             callnr.hpp main.o : $h / com.hpp main.o : $h /
             error.hpp main.o : glo.h main.o : proc.h

                                                   memory.o : const.hpp type.hpp $h /
             const.hpp $h / type.hpp memory.o : $h / callnr.hpp memory.o : $h / com.hpp memory.o : $h /
             error.hpp memory.o : proc.h

                                      printer.o : const.hpp type.hpp $h /
             const.hpp $h / type.hpp printer.o : $h / callnr.hpp printer.o : $h / com.hpp printer.o : $h /
             error
                 .hpp

                     proc.o : const.hpp type.hpp $h /
             const.hpp $h / type.hpp proc.o : $h / callnr.hpp proc.o : $h / com.hpp proc.o : $h /
             error.hpp proc.o : glo.h proc.o : proc.h

                                                   system.o : const.hpp type.hpp $h /
             const.hpp $h / type.hpp system.o : $h / callnr.hpp system.o : $h / com.hpp system.o : $h /
             error.hpp system.o : $h /
             signal.h system.o : glo.h system.o : proc.h

                                                      table.o : const.hpp type.hpp $h /
             const.hpp $h /
             type.hpp table.o : glo.h table.o : proc.h

                                                  tty.o : const.hpp type.hpp $h /
             const.hpp $h / type.hpp tty.o : $h / callnr.hpp tty.o : $h / com.hpp tty.o : $h /
             error.hpp tty.o : $(INC) / sgtty.hpp tty.o : $h /
             signal.h tty.o : glo.h tty.o : proc.h

                                                wini.o : $(WINI_SRC) const.hpp type.hpp $h /
             const.hpp $h / type.hpp wini.o : $h / callnr.hpp wini.o : $h / com.hpp wini.o : $h /
             error.hpp wini.o : proc.h $(CC) $(CFLAGS) -
         c $(WINI_SRC) - o $ @
