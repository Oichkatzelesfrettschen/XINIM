file(GLOB LIB_SRC CONFIGURE_DEPENDS *.c minix/*.c)
# Build the library solely from C sources.
add_library(minixlib STATIC ${LIB_SRC})
