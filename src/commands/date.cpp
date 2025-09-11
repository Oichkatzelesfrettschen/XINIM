/**
 * @file date.cpp
 * @brief Print or set the system date and time.
 * @author Adri Koppes (original author)
 * @date 2023-10-28 (modernization)
 *
 * @copyright Copyright (c) 2023, The XINIM Project. All rights reserved.
 *
 * This program is a C++23 modernization of the original `date` utility from MINIX.
 * It can display the current date and time or set the system's date and time.
 * It leverages the C++ std::chrono library for time operations and provides robust
 * argument
 * parsing and error handling.
 *
 * Usage:
 *   date              // Print the current date and time
 *   date MMDDYYhhmmss  // Set the date and time
 */

#include <cerrno>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

// For setting the system time (POSIX)
#include <sys/time.h>

/**
 * @brief Internal helper utilities for the date command.
 *
 * These helper routines encapsulate the core logic for displaying and mutating
 * the system clock.
 */
namespace {

/**
 * @brief Prints the usage message to standard error.
 * @note The message describes the accepted argument format.
 */
void printUsage() { std::cerr << "Usage: date [MMDDYYhhmmss]" << std::endl; }

/**
 * @brief Prints the current date and time to standard output.
 * @note Uses the user's current locale for formatting.
 */
void printCurrentTime() {
    const auto now = std::chrono::system_clock::now();
    const time_t time_now = std::chrono::system_clock::to_time_t(now);

    // Using std::put_time for safe, localized formatting.
    // Format: Www Mmm dd HH:MM:SS YYYY
    std::cout << std::put_time(std::localtime(&time_now), "%a %b %d %T %Y") << std::endl;
}

/**
 * @brief Sets the system time based on a formatted string.
 * @param time_str The time string in MMDDYYhhmmss format.
 * @throws std::runtime_error If the string cannot be parsed.
 * @throws std::system_error  If the underlying system call fails.
 * @note Requires superuser privileges to succeed.
 */
void setSystemTime(std::string_view time_str) {
    if (time_str.length() != 12) {
        throw std::runtime_error("Invalid date format. Required: MMDDYYhhmmss");
    }

    std::tm tm = {};
    std::string s;

    try {
        // MM
        s = time_str.substr(0, 2);
        tm.tm_mon = std::stoi(s) - 1;
        // DD
        s = time_str.substr(2, 2);
        tm.tm_mday = std::stoi(s);
        // YY
        s = time_str.substr(4, 2);
        int year = std::stoi(s);
        // Handle century. Assume 1970-2069.
        tm.tm_year = (year < 70) ? year + 100 : year;
        // hh
        s = time_str.substr(6, 2);
        tm.tm_hour = std::stoi(s);
        // mm
        s = time_str.substr(8, 2);
        tm.tm_min = std::stoi(s);
        // ss
        s = time_str.substr(10, 2);
        tm.tm_sec = std::stoi(s);
    } catch (const std::invalid_argument &e) {
        throw std::runtime_error("Invalid character in date string.");
    } catch (const std::out_of_range &e) {
        throw std::runtime_error("Numeric value out of range in date string.");
    }

    // Let mktime normalize the values and determine tm_wday, tm_yday, etc.
    time_t t = mktime(&tm);
    if (t == -1) {
        throw std::runtime_error("The specified time is not representable.");
    }

    // Use stime() to set the time.
    if (stime(&t) != 0) {
        throw std::system_error(errno, std::system_category(), "Failed to set system time");
    }
}

} // namespace

/**
 * @brief Main entry point for the date command.
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line arguments.
 * @return 0 on success, 1 on error.
 * @see setSystemTime
 */
int main(int argc, char *argv[]) {
    try {
        if (argc == 1) {
            // No arguments, print the current time.
            printCurrentTime();
        } else if (argc == 2) {
            // One argument, attempt to set the time.
            setSystemTime(argv[1]);
        } else {
            // Invalid number of arguments.
            printUsage();
            return 1;
        }
    } catch (const std::exception &e) {
        std::cerr << "date: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
