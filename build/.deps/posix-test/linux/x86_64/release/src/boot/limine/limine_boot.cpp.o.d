{
    files = {
        "src/boot/limine/limine_boot.cpp"
    },
    values = {
        "/usr/bin/g++",
        {
            "-m64",
            "-fvisibility=hidden",
            "-fvisibility-inlines-hidden",
            "-Werror",
            "-O3",
            "-std=c++23",
            "-Iinclude",
            "-Iinclude/xinim",
            "-Iinclude/xinim/drivers",
            "-Ithird_party/limine",
            "-D__XINIM_X86_64__",
            "-march=x86-64",
            "-mtune=generic",
            "-D_XOPEN_SOURCE=700",
            "-D_GNU_SOURCE",
            "-pthread",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-DNDEBUG"
        }
    },
    depfiles = "limine_boot.o: src/boot/limine/limine_boot.cpp  include/xinim/boot/bootinfo.hpp third_party/limine/limine_protocol.h\
",
    depfiles_format = "gcc"
}