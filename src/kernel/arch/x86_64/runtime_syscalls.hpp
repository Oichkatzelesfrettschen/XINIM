#pragma once

#include <cstddef>
#include <cstdint>

namespace xinim::kernel::x86_64 {

    [[nodiscard]] int64_t process_time(uintptr_t output) noexcept;
    [[nodiscard]] int64_t process_gettimeofday(uintptr_t time_value, uintptr_t timezone) noexcept;
    [[nodiscard]] int64_t process_clock_gettime(int clock_id, uintptr_t time_value) noexcept;
    [[nodiscard]] int64_t process_nanosleep(uintptr_t request, uintptr_t remaining) noexcept;
    [[nodiscard]] int64_t process_alarm(unsigned int seconds) noexcept;
    [[nodiscard]] int64_t process_getrlimit(int resource, uintptr_t output) noexcept;
    [[nodiscard]] int64_t process_setrlimit(int resource, uintptr_t input) noexcept;
    [[nodiscard]] int64_t process_getrusage(int who, uintptr_t output) noexcept;
    [[nodiscard]] int64_t process_umask(uint32_t mask) noexcept;
    [[nodiscard]] int64_t process_getpriority(int which, int who) noexcept;
    [[nodiscard]] int64_t process_setpriority(int which, int who, int priority) noexcept;
    [[nodiscard]] int64_t process_setgroups(size_t count, uintptr_t groups) noexcept;
    [[nodiscard]] int64_t process_setresuid(int real, int effective, int saved) noexcept;
    [[nodiscard]] int64_t process_setresgid(int real, int effective, int saved) noexcept;

} // namespace xinim::kernel::x86_64
