file(GLOB LIB_SRC CONFIGURE_DEPENDS *.c minix/*.c)
file(GLOB LIB_ASM CONFIGURE_DEPENDS *.s)
add_library(minixlib STATIC ${LIB_SRC} ${LIB_ASM})

