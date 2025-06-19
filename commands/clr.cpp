/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

/**
 * @file clr.cpp
 * @brief Clear the terminal screen.
 * @author Andy Tanenbaum (original author)
 * @date 2023-10-27 (modernization)
 *
 * @copyright Copyright (c) 2023, The XINIM Project. All rights reserved.
 *
 * This program is a C++23 modernization of the original `clr` utility.
 * It clears the terminal screen by printing a standard ANSI escape sequence.
 */

#include <iostream>

/**
 * @brief Main entry point for the clr command.
 * @return 0 on success.
 */
int main() {
    // The ANSI escape sequence for clearing the screen and moving the cursor
    // to the home position (top-left corner).
    // \033[2J : Erase entire screen
    // \033[H  : Move cursor to home position
    std::cout << "\033[2J\033[H" << std::flush;
    return 0;
}
