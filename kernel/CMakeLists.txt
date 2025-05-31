# Choose the correct wini driver source
if(WINI_DRIVER STREQUAL "at")
  set(WINI_SRC at_wini.cpp)
else()
  set(WINI_SRC xt_wini.cpp)
endif()

set(KERNEL_SRC
    clock.cpp dmp.cpp floppy.cpp main.cpp memory.cpp printer.cpp proc.cpp system.cpp
    table.cpp tty.cpp idt64.cpp mpx64.cpp klib64.cpp syscall.cpp globals.cpp ${WINI_SRC})

set(KERNEL_ASM)

add_executable(kernel ${KERNEL_SRC} ${KERNEL_ASM})

target_link_libraries(kernel PRIVATE minixlib)

