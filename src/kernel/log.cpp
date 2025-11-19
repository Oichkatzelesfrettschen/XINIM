/**
 * @file log.cpp
 * @brief Kernel Logging Facility Implementation
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 */

#include <xinim/log.hpp>
#include <cstdio>
#include <cstdarg>

namespace xinim {

KernelLogger& KernelLogger::instance() {
    static KernelLogger instance;
    return instance;
}

void KernelLogger::log(LogLevel level, const char* format, ...) {
    // Check minimum level
    if (level < min_level_) {
        return;
    }

    // Print level prefix
    const char* level_str = "";
    switch (level) {
        case LogLevel::DEBUG:    level_str = "[DEBUG] "; break;
        case LogLevel::INFO:     level_str = "[INFO]  "; break;
        case LogLevel::WARNING:  level_str = "[WARN]  "; break;
        case LogLevel::ERROR:    level_str = "[ERROR] "; break;
        case LogLevel::CRITICAL: level_str = "[CRIT]  "; break;
    }

    printf("%s", level_str);

    // Print formatted message
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
}

} // namespace xinim
