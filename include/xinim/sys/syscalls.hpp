#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum xinim_syscall_no {
  SYS_debug_write = 1,
  SYS_monotonic_ns = 2,
};

typedef uint64_t (*xinim_syscall_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

#ifdef __cplusplus
}
#endif

