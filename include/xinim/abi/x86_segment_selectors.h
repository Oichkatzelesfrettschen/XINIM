#pragma once

/* GDT selector ownership shared by x86 assembly and C or C++ code. */

#define XINIM_X86_KERNEL_CS_SELECTOR 0x08
#define XINIM_X86_KERNEL_DS_SELECTOR 0x10
#define XINIM_X86_USER_CS_SELECTOR 0x1B
#define XINIM_X86_USER_DS_SELECTOR 0x23
#define XINIM_X86_TSS_SELECTOR 0x28
