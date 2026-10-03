/**
 * @file test_strlen_property.cpp
 * @brief Property-based tests for the XINIM custom strlen implementation.
 */

#include <algorithm>
#include <rapidcheck.h>
#include <string>

#define strlen xinim_strlen
#include "../lib/strlen.cpp"
#undef strlen

/// \brief Entry point executing RapidCheck properties for ::xinim_strlen.
/// \return Zero upon successful property verification.
int main() {
    rc::check("xinim_strlen matches std::string::size for strings without nulls",
              [](std::string s) {
                  std::erase(s, '\0');
                  RC_ASSERT(static_cast<std::size_t>(xinim_strlen(s.data())) == s.size());
              });
    return 0;
}
