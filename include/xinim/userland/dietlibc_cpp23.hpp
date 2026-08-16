#pragma once

// dietlibc 0.35 hides __u64 when a C++ compiler defines __STRICT_ANSI__.
// Supply that external-header implementation type before unistd.h reaches
// asm/x86_64-sigcontext.h. The adapter keeps the vendored source unchanged.

extern "C" {
#include <stdint.h>
typedef uint64_t __u64;
#include <unistd.h>
}
