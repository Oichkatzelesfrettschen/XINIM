/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

/**
 * @file mined_main.cpp
 * @brief Main entry point for the modernized MINED editor
 */

#include "mined_editor.hpp"
#include <iostream>
#include <exception>

/**
 * @brief Main function for the modernized MINED editor
 */
int main(int argc, char* argv[]) {
    try {
        return mined::modern::main_editor(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception occurred" << std::endl;
        return 1;
    }
}
