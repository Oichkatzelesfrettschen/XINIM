/**
 * @file log.hpp
 * @brief Kernel Logging Facility
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 */

#pragma once

#include <cstdio>
#include <cstdarg>

namespace xinim {

/**
 * @brief Log levels
 */
enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

/**
 * @brief Kernel logger
 */
class KernelLogger {
public:
    static KernelLogger& instance();

    void log(LogLevel level, const char* format, ...);
    void set_level(LogLevel level) { min_level_ = level; }

private:
    KernelLogger() : min_level_(LogLevel::INFO) {}
    LogLevel min_level_;
};

} // namespace xinim

// Convenience macros
#define LOG_DEBUG(...) xinim::KernelLogger::instance().log(xinim::LogLevel::DEBUG, __VA_ARGS__)
#define LOG_INFO(...)  xinim::KernelLogger::instance().log(xinim::LogLevel::INFO, __VA_ARGS__)
#define LOG_WARN(...)  xinim::KernelLogger::instance().log(xinim::LogLevel::WARNING, __VA_ARGS__)
#define LOG_ERROR(...) xinim::KernelLogger::instance().log(xinim::LogLevel::ERROR, __VA_ARGS__)
#define LOG_CRIT(...)  xinim::KernelLogger::instance().log(xinim::LogLevel::CRITICAL, __VA_ARGS__)
