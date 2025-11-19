// XINIM Operating System
// Copyright (c) 2025 XINIM Project
//
// Timing Functions Implementation

#include <xinim/kernel/timing.hpp>

namespace xinim::kernel {

// PIT (Programmable Interval Timer) I/O ports
constexpr uint16_t PIT_CHANNEL0 = 0x40;
constexpr uint16_t PIT_COMMAND = 0x43;
constexpr uint32_t PIT_FREQUENCY = 1193182;  // PIT base frequency in Hz

// CPU frequency in MHz (calibrated during init)
static uint64_t g_cpu_freq_mhz = 2000;  // Default 2GHz (fallback)
static bool g_timing_initialized = false;

// Port I/O functions (x86_64)
static inline void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" :: "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

// Read Time Stamp Counter (TSC)
static inline uint64_t rdtsc() {
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

// Calibrate CPU frequency using PIT (Programmable Interval Timer)
// The PIT runs at a known frequency of 1.193182 MHz, making it perfect for calibration
static void calibrate_cpu_freq() {
    // Calibration period: measure for ~50ms (50000 microseconds)
    // PIT divisor = (PIT_FREQUENCY * ms) / 1000
    constexpr uint32_t CALIBRATE_MS = 50;
    constexpr uint16_t PIT_DIVISOR = (PIT_FREQUENCY * CALIBRATE_MS) / 1000;

    // Disable interrupts during calibration (using cli/sti)
    asm volatile("cli");

    // Program PIT channel 0 for one-shot countdown mode
    // Command: Channel 0, Access mode lobyte/hibyte, Mode 0 (interrupt on terminal count), Binary
    outb(PIT_COMMAND, 0x30);

    // Write divisor (low byte, then high byte)
    outb(PIT_CHANNEL0, PIT_DIVISOR & 0xFF);
    outb(PIT_CHANNEL0, (PIT_DIVISOR >> 8) & 0xFF);

    // Wait for PIT to latch the value
    // Read back command to ensure write completed
    outb(PIT_COMMAND, 0x00);
    inb(PIT_CHANNEL0);

    // Read TSC at start
    uint64_t tsc_start = rdtsc();

    // Wait for PIT countdown to complete by polling the status
    // PIT channel 0 status bit 7 = output pin state (goes high when count reaches 0)
    // We use a read-back command to check status
    bool countdown_complete = false;
    uint32_t timeout = 1000000;  // Prevent infinite loop

    while (!countdown_complete && timeout > 0) {
        // Read-back command: Read status of channel 0
        outb(PIT_COMMAND, 0xE2);  // Read-back command, latch status, channel 0
        uint8_t status = inb(PIT_CHANNEL0);

        // Bit 7 of status = OUT pin state (1 when count reached terminal count)
        if (status & 0x80) {
            countdown_complete = true;
        }
        timeout--;
    }

    // Read TSC at end
    uint64_t tsc_end = rdtsc();

    // Re-enable interrupts
    asm volatile("sti");

    // Check if calibration succeeded
    if (!countdown_complete || tsc_end <= tsc_start) {
        // Calibration failed, use default 2GHz
        g_cpu_freq_mhz = 2000;
        return;
    }

    // Calculate CPU frequency
    // TSC ticks elapsed during the known PIT period
    uint64_t tsc_elapsed = tsc_end - tsc_start;

    // Known time period in microseconds
    uint64_t time_us = (static_cast<uint64_t>(PIT_DIVISOR) * 1000000) / PIT_FREQUENCY;

    // CPU frequency in MHz = (TSC ticks / microseconds)
    g_cpu_freq_mhz = tsc_elapsed / time_us;

    // Sanity check: CPU frequency should be between 100 MHz and 10 GHz
    if (g_cpu_freq_mhz < 100 || g_cpu_freq_mhz > 10000) {
        // Calibration gave unreasonable result, use default
        g_cpu_freq_mhz = 2000;
    }
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
