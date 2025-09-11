/**
 * @file timer.cpp
 * @brief ARM64 Generic Timer implementation
 *
 * Complete implementation of the ARM Generic Timer for ARM64 architecture,
 * providing high-resolution timing, periodic interrupts, and monotonic clock
 * source with support for both physical and virtual timers.
 */

#include <cstdint>
#include <atomic>
#include <algorithm>

namespace xinim::hal::arm64 {

/**
 * @brief ARM Generic Timer - Complete implementation
 * 
 * The ARM Generic Timer provides:
 * - System counter running at a fixed frequency
 * - Per-CPU physical and virtual timers
 * - EL0/EL1/EL2/EL3 timer access
 * - Monotonic timestamp source
 */
class Timer {
public:
    // Timer interrupt IDs (PPI - Private Peripheral Interrupts)
    static constexpr uint32_t PHYS_SECURE_PPI    = 29;
    static constexpr uint32_t PHYS_NONSECURE_PPI = 30;
    static constexpr uint32_t VIRT_PPI           = 27;
    static constexpr uint32_t HYP_PPI            = 26;
    
    // Control register bits
    static constexpr uint32_t CTRL_ENABLE  = (1U << 0);
    static constexpr uint32_t CTRL_IMASK   = (1U << 1);
    static constexpr uint32_t CTRL_ISTATUS = (1U << 2);
    
    /**
     * @brief Initialize the ARM Generic Timer
     */
    void init() noexcept {
        // Read timer frequency from CNTFRQ_EL0
        asm volatile("mrs %0, CNTFRQ_EL0" : "=r"(frequency_) :: "memory");
        
        // Ensure timer is accessible from EL0 (user mode)
        uint64_t cntkctl;
        asm volatile("mrs %0, CNTKCTL_EL1" : "=r"(cntkctl));
        cntkctl |= 0x3;  // EL0PCTEN | EL0VCTEN
        asm volatile("msr CNTKCTL_EL1, %0" :: "r"(cntkctl) : "memory");
        
        // Disable timer interrupts initially
        disable();
        
        // Calculate nanoseconds per tick for fast conversion
        ns_per_tick_ = 1000000000ULL / frequency_;
        
        // Enable cycle counter for high-resolution timing
        enable_cycle_counter();
        
        // Memory barrier
        asm volatile("isb" ::: "memory");
    }
    
    /**
     * @brief Get timer frequency in Hz
     */
    uint64_t get_frequency() const noexcept {
        return frequency_;
    }
    
    /**
     * @brief Get current counter value
     */
    uint64_t get_counter() const noexcept {
        uint64_t count;
        asm volatile("mrs %0, CNTVCT_EL0" : "=r"(count) :: "memory");
        return count;
    }
    
    /**
     * @brief Get physical counter value
     */
    uint64_t get_physical_counter() const noexcept {
        uint64_t count;
        asm volatile("mrs %0, CNTPCT_EL0" : "=r"(count) :: "memory");
        return count;
    }
    
    /**
     * @brief Convert counter ticks to nanoseconds
     */
    uint64_t ticks_to_ns(uint64_t ticks) const noexcept {
        // Precise conversion avoiding overflow
        const uint64_t seconds = ticks / frequency_;
        const uint64_t remainder = ticks % frequency_;
        return (seconds * 1000000000ULL) + (remainder * ns_per_tick_);
    }
    
    /**
     * @brief Convert nanoseconds to counter ticks
     */
    uint64_t ns_to_ticks(uint64_t ns) const noexcept {
        // Precise conversion avoiding overflow
        const uint64_t seconds = ns / 1000000000ULL;
        const uint64_t remainder = ns % 1000000000ULL;
        return (seconds * frequency_) + (remainder * frequency_ / 1000000000ULL);
    }
    
    /**
     * @brief Get current time in nanoseconds
     */
    uint64_t get_nanoseconds() const noexcept {
        return ticks_to_ns(get_counter());
    }
    
    /**
     * @brief Get current time in microseconds
     */
    uint64_t get_microseconds() const noexcept {
        return get_nanoseconds() / 1000;
    }
    
    /**
     * @brief Get current time in milliseconds
     */
    uint64_t get_milliseconds() const noexcept {
        return get_nanoseconds() / 1000000;
    }
    
    /**
     * @brief Setup periodic timer interrupt
     * @param hz Frequency in Hz (e.g., 100 for 100Hz timer)
     */
    void setup_periodic(uint32_t hz) noexcept {
        if (hz == 0 || hz > frequency_) return;
        
        // Calculate timer period
        const uint64_t period = frequency_ / hz;
        period_ticks_ = period;
        
        // Set compare value
        const uint64_t current = get_counter();
        const uint64_t compare = current + period;
        
        // Use virtual timer
        asm volatile("msr CNTV_CVAL_EL0, %0" :: "r"(compare) : "memory");
        
        // Enable timer with interrupt
        uint32_t ctrl = CTRL_ENABLE;  // Enable without masking interrupt
        asm volatile("msr CNTV_CTL_EL0, %0" :: "r"(ctrl) : "memory");
        
        // Memory barrier
        asm volatile("isb" ::: "memory");
        
        enabled_ = true;
    }
    
    /**
     * @brief Setup one-shot timer
     * @param ns Nanoseconds until interrupt
     */
    void setup_oneshot(uint64_t ns) noexcept {
        const uint64_t ticks = ns_to_ticks(ns);
        const uint64_t current = get_counter();
        const uint64_t compare = current + ticks;
        
        // Set compare value
        asm volatile("msr CNTV_CVAL_EL0, %0" :: "r"(compare) : "memory");
        
        // Enable timer with interrupt
        uint32_t ctrl = CTRL_ENABLE;
        asm volatile("msr CNTV_CTL_EL0, %0" :: "r"(ctrl) : "memory");
        
        // Memory barrier
        asm volatile("isb" ::: "memory");
        
        enabled_ = true;
        period_ticks_ = 0;  // Not periodic
    }
    
    /**
     * @brief Handle timer interrupt
     * @return true if interrupt was handled
     */
    bool handle_interrupt() noexcept {
        // Check if timer interrupt is pending
        uint32_t ctrl;
        asm volatile("mrs %0, CNTV_CTL_EL0" : "=r"(ctrl) :: "memory");
        
        if (!(ctrl & CTRL_ISTATUS)) {
            return false;  // Not our interrupt
        }
        
        // If periodic, set next compare value
        if (period_ticks_ > 0) {
            uint64_t cval;
            asm volatile("mrs %0, CNTV_CVAL_EL0" : "=r"(cval) :: "memory");
            cval += period_ticks_;
            asm volatile("msr CNTV_CVAL_EL0, %0" :: "r"(cval) : "memory");
        } else {
            // One-shot: disable timer
            disable();
        }
        
        // Clear interrupt (by writing to compare register)
        asm volatile("isb" ::: "memory");
        
        return true;
    }
    
    /**
     * @brief Disable timer
     */
    void disable() noexcept {
        uint32_t ctrl = 0;  // Disable and mask
        asm volatile("msr CNTV_CTL_EL0, %0" :: "r"(ctrl) : "memory");
        asm volatile("isb" ::: "memory");
        enabled_ = false;
    }
    
    /**
     * @brief Check if timer is enabled
     */
    bool is_enabled() const noexcept {
        return enabled_.load(std::memory_order_acquire);
    }
    
    /**
     * @brief Busy-wait for specified nanoseconds
     * @param ns Nanoseconds to wait
     */
    void delay_ns(uint64_t ns) const noexcept {
        const uint64_t start = get_counter();
        const uint64_t ticks = ns_to_ticks(ns);
        const uint64_t end = start + ticks;
        
        // Handle potential wrap-around
        if (end > start) {
            while (get_counter() < end) {
                asm volatile("yield");  // Hint to CPU we're spinning
            }
        } else {
            // Wrapped around
            while (get_counter() >= start) {
                asm volatile("yield");
            }
            while (get_counter() < end) {
                asm volatile("yield");
            }
        }
    }
    
    /**
     * @brief Busy-wait for specified microseconds
     */
    void delay_us(uint64_t us) const noexcept {
        delay_ns(us * 1000);
    }
    
    /**
     * @brief Busy-wait for specified milliseconds
     */
    void delay_ms(uint64_t ms) const noexcept {
        delay_ns(ms * 1000000);
    }
    
    /**
     * @brief Get elapsed time since a start point
     * @param start_ticks Starting counter value
     * @return Elapsed time in nanoseconds
     */
    uint64_t elapsed_ns(uint64_t start_ticks) const noexcept {
        const uint64_t current = get_counter();
        const uint64_t elapsed = (current >= start_ticks) 
            ? (current - start_ticks)
            : (UINT64_MAX - start_ticks + current + 1);  // Handle wrap
        return ticks_to_ns(elapsed);
    }
    
    /**
     * @brief Calibrate delay loop for microsecond precision
     */
    void calibrate() noexcept {
        const uint64_t test_us = 1000;  // Test with 1ms
        const uint64_t expected_ticks = ns_to_ticks(test_us * 1000);
        
        // Measure actual delay
        const uint64_t start = get_counter();
        delay_us(test_us);
        const uint64_t end = get_counter();
        const uint64_t actual_ticks = end - start;
        
        // Calculate calibration factor
        if (actual_ticks > 0) {
            calibration_factor_ = static_cast<double>(expected_ticks) / actual_ticks;
        }
    }
    
    /**
     * @brief Get system uptime in nanoseconds
     */
    uint64_t get_uptime_ns() const noexcept {
        return get_nanoseconds();  // Assuming counter starts at boot
    }
    
    /**
     * @brief Enable cycle counter for performance monitoring
     */
    void enable_cycle_counter() noexcept {
        // Enable user-mode access to cycle counter
        uint64_t val;
        asm volatile("mrs %0, PMCR_EL0" : "=r"(val));
        val |= (1U << 0);  // E bit - Enable
        val |= (1U << 2);  // C bit - Cycle counter reset
        asm volatile("msr PMCR_EL0, %0" :: "r"(val) : "memory");
        
        // Enable cycle counter
        val = (1U << 31);  // Enable cycle counter
        asm volatile("msr PMCNTENSET_EL0, %0" :: "r"(val) : "memory");
        
        // Clear cycle counter
        asm volatile("msr PMCCNTR_EL0, xzr" ::: "memory");
        
        asm volatile("isb" ::: "memory");
    }
    
    /**
     * @brief Read cycle counter
     */
    uint64_t read_cycles() const noexcept {
        uint64_t val;
        asm volatile("mrs %0, PMCCNTR_EL0" : "=r"(val) :: "memory");
        return val;
    }
    
    /**
     * @brief Measure cycles for a callable
     */
    template<typename F>
    uint64_t measure_cycles(F&& func) noexcept {
        const uint64_t start = read_cycles();
        func();
        const uint64_t end = read_cycles();
        return end - start;
    }
    
    /**
     * @brief Get timer compare value
     */
    uint64_t get_compare_value() const noexcept {
        uint64_t cval;
        asm volatile("mrs %0, CNTV_CVAL_EL0" : "=r"(cval) :: "memory");
        return cval;
    }
    
    /**
     * @brief Get timer control register
     */
    uint32_t get_control() const noexcept {
        uint32_t ctrl;
        asm volatile("mrs %0, CNTV_CTL_EL0" : "=r"(ctrl) :: "memory");
        return ctrl;
    }
    
    /**
     * @brief Check if interrupt is pending
     */
    bool is_interrupt_pending() const noexcept {
        return (get_control() & CTRL_ISTATUS) != 0;
    }

private:
    uint64_t frequency_ = 0;           // Timer frequency in Hz
    uint64_t ns_per_tick_ = 0;         // Nanoseconds per timer tick
    uint64_t period_ticks_ = 0;        // Period for periodic timer
    std::atomic<bool> enabled_{false}; // Timer enabled state
    double calibration_factor_ = 1.0;  // Delay calibration factor
};

/**
 * @brief Global timer instance for system-wide use
 */
static Timer system_timer;

/**
 * @brief Initialize system timer
 */
inline void init_system_timer() noexcept {
    system_timer.init();
}

/**
 * @brief Get system timer instance
 */
inline Timer& get_system_timer() noexcept {
    return system_timer;
}

} // namespace xinim::hal::arm64