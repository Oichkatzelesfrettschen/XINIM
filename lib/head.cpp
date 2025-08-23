/**
 * @file start.cpp
 * @brief Minimal startup and entry point for the kernel image.
 *
 * This file contains the low-level initialization code for the XINIM kernel.
 * Its primary purpose is to set up the stack and transfer control to the
 * high-level C++ `main` function. It also defines key global symbols that are
 * used by the linker to identify the memory layout of the kernel image.
 *
 * @section Global Symbols
 * - `begtext`, `begdata`, `begbss`: Linker-defined symbols marking the start of
 * the kernel's text, data, and BSS segments.
 * - `brksize`: The program break pointer, initialized to the end of BSS.
 * - `stackpt`: The initial stack pointer for the kernel.
 *
 * @section Entry Point
 * The `start` function, written in C++ with an embedded assembly instruction,
 * is the first function executed after the bootloader loads the kernel image.
 * It loads the initial stack pointer and calls `main`.
 *
 * @ingroup kernel_init
 */
#include <stddef.h>

extern "C" int main(void);
extern void exit(int status);
extern void *stackpt;
extern char endbss;

/* Symbols marking the start of each segment. */
char begtext;
char begdata;
char begbss;

/* Data area for the loader. */
long data_org[] = {0xDADA, 0, 0, 0, 0, 0, 0, 0};

/* Break pointer initialized to the end of the bss. */
char *brksize = &endbss;

/* Stack limit used by the kernel. */
long sp_limit = 0;

/**
 * @brief Kernel entry point.
 *
 * This function is the initial entry point after the bootloader. It sets up
 * the kernel's stack and calls the `main` function. If `main` ever returns,
 * this function enters an infinite loop, effectively halting the system.
 */
void start(void) {
    /* Load the stack pointer from _stackpt and enter main. */
    __asm__ volatile("movq _stackpt(%%rip), %%rsp" ::: "rsp");
    main();
    for (;;) {
        /* Halt if main ever returns. */
    }
}