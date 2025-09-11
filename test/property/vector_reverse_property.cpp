//------------------------------------------------------------------------------
// Property-based test example using RapidCheck.
//
// The test verifies that reversing a sequence twice returns the original
// sequence. It demonstrates how RapidCheck explores randomized inputs to
// validate algebraic invariants. This file is intentionally compact yet
// heavily annotated for educational clarity.
//------------------------------------------------------------------------------

#include <algorithm>
#include <vector>

#include <rapidcheck.h>

int main() {
    rc::check("double reverse yields original", [](const std::vector<int> &input) {
        auto copy = input;                      // make a copy to mutate
        std::reverse(copy.begin(), copy.end()); // first reversal
        std::reverse(copy.begin(), copy.end()); // second reversal
        RC_ASSERT(copy == input);               // property: idempotence
    });
    return 0; // rc::check handles failures
}
