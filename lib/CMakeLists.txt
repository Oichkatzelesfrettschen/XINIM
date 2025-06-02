# Explicitly list the source files for the library
set(LIB_SOURCES
    abort.cpp
    abs.cpp
    access.cpp
    alarm.cpp
    atoi.cpp
    atol.cpp
    bcopy.cpp
    brk.cpp
    brk2.cpp
    brksize.cpp
    call.cpp
    catchsig.cpp
    chdir.cpp
    chmod.cpp
    chown.cpp
    chroot.cpp
    cleanup.cpp
    close.cpp
    creat.cpp
    crt0.cpp
    crtso.cpp
    crypt.cpp
    csv.cpp
    ctype.cpp
    # doprintf.cpp was removed, replaced by io/src/format.cpp
    doscanf.cpp
    dup.cpp
    dup2.cpp
    end.cpp
    exec.cpp
    exit.cpp
    # fclose.cpp was removed
    # fflush.cpp was removed
    fgets.cpp # Refactored
    # fopen.cpp was removed
    fork.cpp
    # fprintf.cpp was removed (functionality covered by io/src/format.cpp)
    fputs.cpp # Refactored
    fread.cpp # Refactored
    freopen.cpp # Keep, will be refactored later
    # fseek.cpp was removed
    fstat.cpp # Keep for now
    # ftell.cpp was removed
    fwrite.cpp # Refactored
    getc.cpp # Refactored
    getegid.cpp
    getenv.cpp
    # printdat.cpp was removed (functionality covered by io/src/standard_streams.cpp)
    setbuf.cpp # Refactored
    # ... (other files from the lib/ directory)
    io/stream_operations.cpp
    io/src/syscall.cpp
    io/src/file_stream.cpp
    io/src/standard_streams.cpp
    io/src/format.cpp
    # Add files from minix subdirectory if any were previously included by minix/*.cpp
    # e.g., minix/somefile.cpp
)

# Build the library
add_library(minixlib STATIC ${LIB_SOURCES})

# Ensure header files in ../include are accessible
target_include_directories(minixlib PUBLIC ../include)
