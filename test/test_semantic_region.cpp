#include "../include/psd/vm/semantic_memory.hpp"
#include <cassert>

int main() {
    using namespace psd::vm;
    semantic_region<semantic_code_tag> code_region(0x1001, 4096);
    auto aligned = code_region.base() % semantic_traits<semantic_code_tag>::alignment == 0;
    assert(aligned);
    return 0;
}
