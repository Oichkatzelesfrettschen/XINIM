#include "xinim/userland/syscall_i386.hpp"

namespace {

[[maybe_unused]] uint32_t exercise_wrappers() noexcept {
    const char text[] = "x";
    const uint32_t pid = xinim::userland::i386::getpid();
    const uint32_t wrote = xinim::userland::i386::write(1, text, 1U);
    return pid ^ wrote;
}

} // namespace
