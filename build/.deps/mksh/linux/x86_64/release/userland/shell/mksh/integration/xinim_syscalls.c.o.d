{
    files = {
        "userland/shell/mksh/integration/xinim_syscalls.c"
    },
    depfiles = "build/.objs/mksh/linux/x86_64/release/userland/shell/mksh/integration/xinim_syscalls.c.o:  userland/shell/mksh/integration/xinim_syscalls.c\
",
    depfiles_format = "gcc",
    values = {
        "/usr/bin/gcc",
        {
            "-m64",
            "-fvisibility=hidden",
            "-Werror",
            "-O3",
            "-std=c17",
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