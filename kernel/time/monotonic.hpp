#pragma once
#include <stdint.h>

namespace xinim::time {

using monotonic_fn = uint64_t(*)();

void    monotonic_install(monotonic_fn fn);
uint64_t monotonic_ns();

}

