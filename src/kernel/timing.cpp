// XINIM Operating System
// Copyright (c) 2025 XINIM Project
//
// Timing Functions Implementation

#include <xinim/kernel/timing.hpp>

namespace xinim::kernel {

// CPU frequency in MHz (calibrated during init)
static uint64_t g_cpu_freq_mhz = 2000;  // Default 2GHz
static bool g_timing_initialized = false;

// Read Time Stamp Counter (TSC)
static inline uint64_t rdtsc() {
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

// Calibrate CPU frequency using PIT or other method
static void calibrate_cpu_freq() {
    // TODO: Proper calibration using PIT (Programmable Interval Timer)
    // For now, assume 2GHz
    g_cpu_freq_mhz = 2000;
}

void timing_init() {
    if (g_timing_initialized) {
        return;
    }

    calibrate_cpu_freq();
    g_timing_initialized = true;
}

void udelay(uint32_t microseconds) {
    if (!g_timing_initialized) {
        timing_init();
    }

    // Calculate TSC ticks for the delay
    // CPU freq in MHz = ticks per microsecond
    uint64_t ticks = static_cast<uint64_t>(microseconds) * g_cpu_freq_mhz;
    uint64_t start = rdtsc();
    uint64_t end = start + ticks;

    // Busy-wait until enough ticks have elapsed
    while (rdtsc() < end) {
        asm volatile("pause");  // Hint to CPU that we're spinning
    }
}

void mdelay(uint32_t milliseconds) {
    // Convert milliseconds to microseconds
    udelay(milliseconds * 1000);
}

uint64_t get_timestamp_us() {
    if (!g_timing_initialized) {
        timing_init();
    }

    uint64_t tsc = rdtsc();
    return tsc / g_cpu_freq_mhz;  // Convert TSC ticks to microseconds
}

uint64_t get_timestamp_ms() {
    return get_timestamp_us() / 1000;
}

} // namespace xinim::kernel
