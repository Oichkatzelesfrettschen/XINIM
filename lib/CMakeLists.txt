file(GLOB LIB_SRC CONFIGURE_DEPENDS *.c minix/*.c)
# Use only C sources for the standard library
add_library(minixlib STATIC ${LIB_SRC})
