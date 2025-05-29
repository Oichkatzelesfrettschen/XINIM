file(GLOB LIB_SRC CONFIGURE_DEPENDS *.c minix/*.c)
set(LIB_ASM
    catchsig.s
    csv.s
    getutil.s
    sendrec.s
    setjmp.s
    minix/setjmp.s)
add_library(minixlib STATIC ${LIB_SRC} ${LIB_ASM})
