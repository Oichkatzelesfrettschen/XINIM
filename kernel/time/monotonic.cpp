#include "monotonic.hpp"

namespace xinim::time {

static monotonic_fn g_mono = nullptr;

void monotonic_install(monotonic_fn fn) { g_mono = fn; }

uint64_t monotonic_ns() { return g_mono ? g_mono() : 0; }

}

