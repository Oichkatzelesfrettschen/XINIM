#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t xinim_syscall_dispatch(uint64_t number, uint64_t argument0, uint64_t argument1,
                                uint64_t argument2, uint64_t argument3, uint64_t argument4,
                                uint64_t argument5);

#ifdef __cplusplus
}
#endif
