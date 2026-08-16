#ifndef XINIM_ABI_EXEC_LIMITS_H
#define XINIM_ABI_EXEC_LIMITS_H

/*
 * The x86_64 exec limit includes argument and environment string terminators,
 * pointer tables, auxiliary-vector words, and worst-case stack alignment.
 * Keep this C23-compatible boundary consumable by the dietlibc build adapter.
 */
#define XINIM_X86_64_INITIAL_STACK_SIZE_BYTES 65536U
#define XINIM_EXEC_MAX_VECTOR_ENTRIES 32U
#define XINIM_EXEC_AUXILIARY_VECTOR_WORDS 4U
#define XINIM_EXEC_STACK_ALIGNMENT_BYTES 16U
#define XINIM_EXEC_ARGUMENT_ENVIRONMENT_BYTES 64953U

#endif
