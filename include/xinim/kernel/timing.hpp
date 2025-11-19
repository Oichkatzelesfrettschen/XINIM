// XINIM Operating System
// Copyright (c) 2025 XINIM Project
//
// Timing Functions
// Provides microsecond and millisecond delay functions for drivers

#pragma once

#include <cstdint>

namespace xinim::kernel {

// Delay for specified microseconds
// Uses busy-wait loop calibrated to CPU frequency
void udelay(uint32_t microseconds);

// Delay for specified milliseconds
// Uses busy-wait loop calibrated to CPU frequency
void mdelay(uint32_t milliseconds);

// Initialize timing subsystem (calibrate delays)
void timing_init();

// Get current timestamp in microseconds (if TSC available)
uint64_t get_timestamp_us();

// Get current timestamp in milliseconds (if TSC available)
uint64_t get_timestamp_ms();

} // namespace xinim::kernel
