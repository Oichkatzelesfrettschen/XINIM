file(GLOB LIB_SRC CONFIGURE_DEPENDS *.c)
add_library(minixlib STATIC ${LIB_SRC})

