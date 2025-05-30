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
             signal.hpp clock.o : glo.hpp clock.o : proc.hpp

                                                        floppy.o : const.hpp type.hpp $h /
             const.hpp $h / type.hpp floppy.o : $h / callnr.hpp floppy.o : $h /
             com.hpp floppy.o : $h /
             error.hpp floppy.o : glo.hpp floppy.o : proc.hpp

                                                         dmp.o : const.hpp type.hpp $h /
             const.hpp $h / type.hpp dmp.o : $h / callnr.hpp dmp.o : $h / com.hpp dmp.o : $h /
             error.hpp dmp.o : glo.hpp dmp.o : proc.hpp

                                                   paging.o : const.hpp type.hpp../
             include / paging.hpp main.o : const.hpp type.hpp $h / const.hpp $h /
             type.hpp main.o : $h / callnr.hpp main.o : $h / com.hpp main.o : $h /
             error.hpp main.o : glo.hpp main.o : proc.hpp

                                                     memory.o : const.hpp type.hpp $h /
             const.hpp $h / type.hpp memory.o : $h / callnr.hpp memory.o : $h /
             com.hpp memory.o : $h /
             error.hpp memory.o : proc.hpp

                                      printer.o : const.hpp type.hpp $h /
             const.hpp $h / type.hpp printer.o : $h / callnr.hpp printer.o : $h /
             com.hpp printer.o : $h /
             error
                 .hpp

                     proc.o : const.hpp type.hpp $h /
             const.hpp $h / type.hpp proc.o : $h / callnr.hpp proc.o : $h / com.hpp proc.o : $h /
             error.hpp proc.o : glo.hpp proc.o : proc.hpp

                                                     system.o : const.hpp type.hpp $h /
             const.hpp $h / type.hpp system.o : $h / callnr.hpp system.o : $h /
             com.hpp system.o : $h / error.hpp system.o : $h /
             signal.hpp system.o : glo.hpp system.o : proc.hpp

                                                          table.o : const.hpp type.hpp $h /
             const.hpp $h /
             type.hpp table.o : glo.hpp table.o : proc.hpp

                                                      tty.o : const.hpp type.hpp $h /
             const.hpp $h / type.hpp tty.o : $h / callnr.hpp tty.o : $h / com.hpp tty.o : $h /
             error.hpp tty.o : $(INC) / sgtty.hpp tty.o : $h /
             signal.hpp tty.o : glo.hpp tty.o : proc.hpp

                                                    wini.o : $(WINI_SRC) const.hpp type.hpp $h /
             const.hpp $h / type.hpp wini.o : $h / callnr.hpp wini.o : $h / com.hpp wini.o : $h /
             error.hpp wini.o : proc.hpp $(CC) $(CFLAGS) -
         c $(WINI_SRC) - o $ @
