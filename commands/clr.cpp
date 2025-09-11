/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

/**
 * @file clr.cpp
 * @brief Utility for clearing the terminal screen.
 * @details The program emits the canonical ANSI escape sequence to reset the
 * terminal display and reposition the cursor at the origin. It intentionally
 * writes directly to standard output rather than relying on termcap or
 * terminfo databases, mirroring the minimalist design of the historical
 * MINIX command.
 *
 * @author Andy Tanenbaum (original author)
 * @date 2023-10-27 (modernization)
 *
 * @copyright Copyright (c) 2023, The XINIM Project. All rights reserved.
 */

#include <iostream>

/**
 * @brief Clears the terminal and resets the cursor.
 * @details Outputs the escape sequence `\033[2J\033[H`, which erases the
 * entire screen (`2J`) and moves the cursor to the home position (`H`).
 *
 * @return Zero on success.
 */
int main() {
    // Emit the control sequence and flush to ensure immediate effect.
    std::cout << "\033[2J\033[H" << std::flush;
    return 0;
}
