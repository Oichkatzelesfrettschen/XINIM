/**
 * @file hello.c
 * @brief Simple "Hello World" test for XINIM
 *
 * This test validates basic system functionality:
 * - Process execution
 * - Write syscall to stdout
 * - Process exit
 *
 * Expected output:
 *   Hello from XINIM userspace!
 *   Syscall test: write() to stdout
 *   My PID: <number>
 *
 * Build:
 *   x86_64-linux-gnu-gcc -c hello.c -o hello.o \
 *       -I../../libc/dietlibc-xinim/include \
 *       -nostdlib -fno-builtin -static
 *
 *   x86_64-linux-gnu-ld hello.o \
 *       ../../libc/dietlibc-xinim/bin-x86_64/dietlibc.a \
 *       -o hello \
 *       -nostdlib -static
 *
 * @author XINIM Test Suite
 * @date November 2025
 */

#include <stdio.h>
#include <unistd.h>

int main() {
    // Test 1: printf (buffered I/O via dietlibc)
    printf("Hello from XINIM userspace!\n");

    // Test 2: Direct write syscall
    const char* msg = "Syscall test: write() to stdout\n";
    write(1, msg, 33);

    // Test 3: getpid (Process Manager IPC)
    pid_t my_pid = getpid();
    printf("My PID: %d\n", my_pid);

    // Test 4: Normal exit
    return 0;
}
