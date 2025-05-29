# Choose the correct wini driver source
if(WINI_DRIVER STREQUAL "at")
  set(WINI_SRC at_wini.c)
else()
  set(WINI_SRC xt_wini.c)
endif()

set(KERNEL_SRC
    clock.c dmp.c floppy.c main.c memory.c printer.c proc.c system.c
    table.c tty.c ${WINI_SRC})

file(GLOB KERNEL_ASM CONFIGURE_DEPENDS *.s *.asm)

add_executable(kernel ${KERNEL_SRC} ${KERNEL_ASM})

target_link_libraries(kernel PRIVATE minixlib)

