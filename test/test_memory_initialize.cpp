#include "xinim/mm/memory.hpp"

#include <cstdlib>
#include <iostream>
#include <print>

int main() {
    if (!xinim::mm::initialize()) {
        std::println(std::cerr, "memory initialize returned false");
        return EXIT_FAILURE;
    }

    std::println(std::cout, "memory initialize passed");
    return EXIT_SUCCESS;
}
