#pragma once

/**
 * @file abort.hpp
 * @brief Cross-language wrapper for program termination.
 */

#include <stdlib.h>

#ifdef __cplusplus
extern "C" [[noreturn]] inline void xinim_abort(void) noexcept { ::exit(99); }
namespace xinim {
[[noreturn]] inline void abort() noexcept { ::xinim_abort(); }
} // namespace xinim
#else
static inline _Noreturn void xinim_abort(void) { exit(99); }
#endif
