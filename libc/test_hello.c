/*
 * Simple test program to validate dietlibc-XINIM syscall integration
 *
 * This program tests that:
 * 1. write() calls XINIM syscall number 6 (SYS_write) not Linux number 1
 * 2. getpid() calls XINIM syscall number 29 (SYS_getpid) not Linux number 39
 * 3. exit() calls XINIM syscall number 25 (SYS_exit) not Linux number 60
 *
 * Compile with:
 *   /home/user/XINIM/libc/dietlibc-xinim/bin-x86_64/diet gcc -o test_hello test_hello.c
 *
 * To verify syscall numbers, use:
 *   strace -e trace=syscall ./test_hello
 */

#include <unistd.h>

int main(void) {
    const char *msg = "Hello from XINIM syscalls!\n";

    /* Test write() - should use XINIM SYS_write = 6 */
    write(1, msg, 28);

    /* Test getpid() - should use XINIM SYS_getpid = 29 */
    int pid = getpid();

    /* Suppress unused variable warning */
    (void)pid;

    /* Test exit() - should use XINIM SYS_exit = 25 */
    return 0;
}
