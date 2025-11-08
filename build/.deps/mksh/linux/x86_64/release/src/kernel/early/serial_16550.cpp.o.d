{
    files = {
        "src/kernel/early/serial_16550.cpp"
    },
    depfiles = "build/.objs/mksh/linux/x86_64/release/src/kernel/early/serial_16550.cpp.o:  src/kernel/early/serial_16550.cpp src/kernel/early/serial_16550.hpp\
",
    depfiles_format = "gcc",
    values = {
        "/usr/bin/g++",
        {
            "-m64",
            "-fvisibility=hidden",
            "-fvisibility-inlines-hidden",
            "-Werror",
            "-O3",
            "-Iinclude",
            "-Iinclude/xinim",
            "-Iinclude/xinim/drivers",
            "-Ithird_party/limine",
            "-Ithird_party/limine-protocol/include",
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
    }
}