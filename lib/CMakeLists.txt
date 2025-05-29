file(GLOB LIB_SRC CONFIGURE_DEPENDS *.c minix/*.c)
# Use only C sources for the standard library
# Build the library solely from C sources.
add_library(minixlib STATIC ${LIB_SRC})
