#ifndef XINIM_TEST_I486_CHECK_HPP
#define XINIM_TEST_I486_CHECK_HPP

#include <cstdio>
#include <cstdlib>
#include <print>

namespace xinim::test {

    inline void require_check(bool condition, const char *expression, int line) {
        if (!condition) {
            std::println(stderr, "FAIL: line {}: {}", line, expression);
            std::exit(EXIT_FAILURE);
        }
    }

} // namespace xinim::test

#define CHECK(condition) \
    ::xinim::test::require_check(static_cast<bool>(condition), #condition, __LINE__)

#endif // XINIM_TEST_I486_CHECK_HPP
