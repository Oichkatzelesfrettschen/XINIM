/**
 * @file gic.cpp
 * @brief ARM Generic Interrupt Controller (GIC) v3 implementation
 *
 * Complete implementation of the ARM GICv3 interrupt controller for ARM64
 * architecture, providing interrupt routing, priority management, and
 * CPU interface configuration.
 */

#include <cstdint>
#include <array>
#include <atomic>

namespace xinim::hal::arm64 {

/**
 * @brief GICv3 Distributor registers
 */
struct GicDistributor {
    volatile uint32_t CTLR;              // 0x000 Distributor Control Register
    volatile uint32_t TYPER;             // 0x004 Interrupt Controller Type Register
    volatile uint32_t IIDR;              // 0x008 Distributor Implementer ID
    volatile uint32_t TYPER2;            // 0x00C Type Register 2
    volatile uint32_t STATUSR;           // 0x010 Error Reporting Status Register
    uint32_t reserved1[3];               // 0x014-0x01C
    volatile uint32_t IMP_DEF[8];        // 0x020-0x03C Implementation Defined
    volatile uint32_t SETSPI_NSR;        // 0x040 Set SPI Register
    uint32_t reserved2;                  // 0x044
    volatile uint32_t CLRSPI_NSR;        // 0x048 Clear SPI Register
    uint32_t reserved3;                  // 0x04C
    volatile uint32_t SETSPI_SR;         // 0x050 Set SPI Secure Register
    uint32_t reserved4;                  // 0x054
    volatile uint32_t CLRSPI_SR;         // 0x058 Clear SPI Secure Register
    uint32_t reserved5[9];               // 0x05C-0x07C
    volatile uint32_t IGROUPR[32];       // 0x080-0x0FC Interrupt Group Registers
    volatile uint32_t ISENABLER[32];     // 0x100-0x17C Interrupt Set-Enable Registers
    volatile uint32_t ICENABLER[32];     // 0x180-0x1FC Interrupt Clear-Enable Registers
    volatile uint32_t ISPENDR[32];       // 0x200-0x27C Interrupt Set-Pending Registers
    volatile uint32_t ICPENDR[32];       // 0x280-0x2FC Interrupt Clear-Pending Registers
    volatile uint32_t ISACTIVER[32];     // 0x300-0x37C Interrupt Set-Active Registers
    volatile uint32_t ICACTIVER[32];     // 0x380-0x3FC Interrupt Clear-Active Registers
    volatile uint8_t  IPRIORITYR[1020];  // 0x400-0x7FC Interrupt Priority Registers
    uint32_t reserved6;                  // 0x7FC
    volatile uint32_t ITARGETSR[255];    // 0x800-0xBFC Interrupt Target Registers
    uint32_t reserved7;                  // 0xBFC
    volatile uint32_t ICFGR[64];         // 0xC00-0xCFC Interrupt Configuration Registers
    volatile uint32_t IGRPMODR[32];      // 0xD00-0xD7C Interrupt Group Modifier Registers
    uint32_t reserved8[32];              // 0xD80-0xDFC
    volatile uint32_t NSACR[64];         // 0xE00-0xEFC Non-secure Access Control Registers
};

/**
 * @brief GICv3 CPU Interface registers
 */
struct GicCpuInterface {
    volatile uint32_t CTLR;              // 0x000 CPU Interface Control Register
    volatile uint32_t PMR;               // 0x004 Interrupt Priority Mask Register
    volatile uint32_t BPR;               // 0x008 Binary Point Register
    volatile uint32_t IAR;               // 0x00C Interrupt Acknowledge Register
    volatile uint32_t EOIR;              // 0x010 End of Interrupt Register
    volatile uint32_t RPR;               // 0x014 Running Priority Register
    volatile uint32_t HPPIR;             // 0x018 Highest Priority Pending Interrupt Register
    volatile uint32_t ABPR;              // 0x01C Aliased Binary Point Register
    volatile uint32_t AIAR;              // 0x020 Aliased Interrupt Acknowledge Register
    volatile uint32_t AEOIR;             // 0x024 Aliased End of Interrupt Register
    volatile uint32_t AHPPIR;            // 0x028 Aliased Highest Priority Pending Interrupt Register
    uint32_t reserved1[41];              // 0x02C-0x0CC
    volatile uint32_t APR[4];            // 0x0D0-0x0DC Active Priority Registers
    volatile uint32_t NSAPR[4];          // 0x0E0-0x0EC Non-secure Active Priority Registers
    uint32_t reserved2[3];               // 0x0F0-0x0F8
    volatile uint32_t IIDR;              // 0x0FC CPU Interface Identification Register
    uint32_t reserved3[960];             // 0x100-0xFFC
    volatile uint32_t DIR;               // 0x1000 Deactivate Interrupt Register
};

/**
 * @brief GICv3 Redistributor registers
 */
struct GicRedistributor {
    volatile uint32_t CTLR;              // Control Register
    volatile uint32_t IIDR;              // Identification Register
    volatile uint32_t TYPER[2];          // Type Register (64-bit)
    volatile uint32_t STATUSR;           // Status Register
    volatile uint32_t WAKER;             // Wake Register
    volatile uint32_t MPAMIDR;           // MPAM ID Register
    volatile uint32_t PARTIDR;           // Partition ID Register
    uint32_t reserved1[8];
    volatile uint32_t SETLPIR[2];        // Set LPI Register (64-bit)
    volatile uint32_t CLRLPIR[2];        // Clear LPI Register (64-bit)
    uint32_t reserved2[8];
    volatile uint32_t PROPBASER[2];      // Properties Base Address Register (64-bit)
    volatile uint32_t PENDBASER[2];      // Pending Table Base Address Register (64-bit)
    uint32_t reserved3[8];
    volatile uint32_t INVLPIR[2];        // Invalidate LPI Register (64-bit)
    uint32_t reserved4[2];
    volatile uint32_t INVALLR[2];        // Invalidate All Register (64-bit)
    uint32_t reserved5[2];
    volatile uint32_t SYNCR;             // Synchronize Register
};

/**
 * @brief ARM Generic Interrupt Controller v3
 */
class Gic {
public:
    static constexpr uint32_t MAX_INTERRUPTS = 1020;
    static constexpr uint32_t SGI_BASE = 0;       // Software Generated Interrupts (0-15)
    static constexpr uint32_t PPI_BASE = 16;      // Private Peripheral Interrupts (16-31)
    static constexpr uint32_t SPI_BASE = 32;      // Shared Peripheral Interrupts (32-1019)
    
    enum class Priority : uint8_t {
        HIGHEST = 0x00,
        HIGH    = 0x40,
        NORMAL  = 0x80,
        LOW     = 0xC0,
        LOWEST  = 0xF0
    };
    
    enum class Trigger {
        LEVEL = 0,
        EDGE  = 1
    };
    
    /**
     * @brief Initialize the GIC with given base addresses
     */
    void init(uintptr_t dist_base, uintptr_t cpu_base, uintptr_t redist_base = 0) noexcept {
        dist_ = reinterpret_cast<GicDistributor*>(dist_base);
        cpu_ = reinterpret_cast<GicCpuInterface*>(cpu_base);
        
        if (redist_base) {
            redist_ = reinterpret_cast<GicRedistributor*>(redist_base);
        }
        
        // Disable distributor during configuration
        dist_->CTLR = 0;
        
        // Get number of supported interrupts
        uint32_t typer = dist_->TYPER;
        uint32_t it_lines = (typer & 0x1F) + 1;
        num_interrupts_ = it_lines * 32;
        
        if (num_interrupts_ > MAX_INTERRUPTS) {
            num_interrupts_ = MAX_INTERRUPTS;
        }
        
        // Disable all interrupts
        for (uint32_t i = 0; i < num_interrupts_ / 32; ++i) {
            dist_->ICENABLER[i] = 0xFFFFFFFF;
            dist_->ICPENDR[i] = 0xFFFFFFFF;
            dist_->ICACTIVER[i] = 0xFFFFFFFF;
        }
        
        // Set all interrupts to lowest priority
        for (uint32_t i = 0; i < num_interrupts_; ++i) {
            dist_->IPRIORITYR[i] = static_cast<uint8_t>(Priority::LOWEST);
        }
        
        // Set all SPIs to target CPU 0
        for (uint32_t i = SPI_BASE / 4; i < num_interrupts_ / 4; ++i) {
            dist_->ITARGETSR[i] = 0x01010101;  // Target CPU 0
        }
        
        // Configure all interrupts as level-triggered
        for (uint32_t i = SPI_BASE / 16; i < num_interrupts_ / 16; ++i) {
            dist_->ICFGR[i] = 0;
        }
        
        // Enable distributor
        dist_->CTLR = 1;
        
        // Initialize CPU interface
        cpu_->PMR = 0xFF;     // Allow all priorities
        cpu_->BPR = 0;        // No binary point (no preemption)
        cpu_->CTLR = 1;       // Enable CPU interface
        
        // Memory barrier to ensure configuration is complete
        asm volatile("dsb sy" ::: "memory");
        asm volatile("isb" ::: "memory");
    }
    
    /**
     * @brief Enable an interrupt
     */
    void enable_interrupt(uint32_t irq) noexcept {
        if (irq >= num_interrupts_) return;
        
        uint32_t reg = irq / 32;
        uint32_t bit = irq % 32;
        
        dist_->ISENABLER[reg] = (1U << bit);
        
        asm volatile("dsb sy" ::: "memory");
    }
    
    /**
     * @brief Disable an interrupt
     */
    void disable_interrupt(uint32_t irq) noexcept {
        if (irq >= num_interrupts_) return;
        
        uint32_t reg = irq / 32;
        uint32_t bit = irq % 32;
        
        dist_->ICENABLER[reg] = (1U << bit);
        
        asm volatile("dsb sy" ::: "memory");
    }
    
    /**
     * @brief Set interrupt priority
     */
    void set_priority(uint32_t irq, Priority priority) noexcept {
        if (irq >= num_interrupts_) return;
        
        dist_->IPRIORITYR[irq] = static_cast<uint8_t>(priority);
        
        asm volatile("dsb sy" ::: "memory");
    }
    
    /**
     * @brief Configure interrupt trigger mode
     */
    void set_trigger(uint32_t irq, Trigger trigger) noexcept {
        if (irq < SPI_BASE || irq >= num_interrupts_) return;
        
        uint32_t reg = irq / 16;
        uint32_t offset = (irq % 16) * 2;
        
        uint32_t cfg = dist_->ICFGR[reg];
        cfg &= ~(3U << offset);
        
        if (trigger == Trigger::EDGE) {
            cfg |= (2U << offset);
        }
        
        dist_->ICFGR[reg] = cfg;
        
        asm volatile("dsb sy" ::: "memory");
    }
    
    /**
     * @brief Set interrupt target CPUs
     */
    void set_target(uint32_t irq, uint8_t cpu_mask) noexcept {
        if (irq < SPI_BASE || irq >= num_interrupts_) return;
        
        uint32_t reg = irq / 4;
        uint32_t offset = (irq % 4) * 8;
        
        uint32_t target = dist_->ITARGETSR[reg];
        target &= ~(0xFF << offset);
        target |= (static_cast<uint32_t>(cpu_mask) << offset);
        
        dist_->ITARGETSR[reg] = target;
        
        asm volatile("dsb sy" ::: "memory");
    }
    
    /**
     * @brief Acknowledge interrupt (get highest priority pending)
     */
    uint32_t acknowledge_interrupt() noexcept {
        uint32_t irq = cpu_->IAR;
        asm volatile("dsb sy" ::: "memory");
        return irq & 0x3FF;  // Extract IRQ number (bits 0-9)
    }
    
    /**
     * @brief End of interrupt (signal completion)
     */
    void end_of_interrupt(uint32_t irq) noexcept {
        cpu_->EOIR = irq;
        asm volatile("dsb sy" ::: "memory");
    }
    
    /**
     * @brief Send Software Generated Interrupt (SGI)
     */
    void send_sgi(uint32_t sgi, uint8_t target_list, bool target_all = false) noexcept {
        if (sgi >= 16) return;  // SGIs are 0-15
        
        uint64_t icc_sgi1r_val = sgi & 0xF;
        
        if (target_all) {
            icc_sgi1r_val |= (1ULL << 40);  // IRM bit for all except self
        } else {
            icc_sgi1r_val |= (static_cast<uint64_t>(target_list) << 16);
        }
        
        // Write to ICC_SGI1R_EL1 system register
        asm volatile("msr ICC_SGI1R_EL1, %0" :: "r"(icc_sgi1r_val) : "memory");
        asm volatile("dsb sy" ::: "memory");
    }
    
    /**
     * @brief Get number of supported interrupts
     */
    uint32_t get_num_interrupts() const noexcept {
        return num_interrupts_;
    }
    
    /**
     * @brief Check if interrupt is pending
     */
    bool is_pending(uint32_t irq) const noexcept {
        if (irq >= num_interrupts_) return false;
        
        uint32_t reg = irq / 32;
        uint32_t bit = irq % 32;
        
        return (dist_->ISPENDR[reg] & (1U << bit)) != 0;
    }
    
    /**
     * @brief Check if interrupt is active
     */
    bool is_active(uint32_t irq) const noexcept {
        if (irq >= num_interrupts_) return false;
        
        uint32_t reg = irq / 32;
        uint32_t bit = irq % 32;
        
        return (dist_->ISACTIVER[reg] & (1U << bit)) != 0;
    }
    
    /**
     * @brief Get current running priority
     */
    uint8_t get_running_priority() const noexcept {
        return static_cast<uint8_t>(cpu_->RPR);
    }
    
    /**
     * @brief Set priority mask (interrupts below this priority are masked)
     */
    void set_priority_mask(uint8_t mask) noexcept {
        cpu_->PMR = mask;
        asm volatile("dsb sy" ::: "memory");
    }

private:
    GicDistributor* dist_ = nullptr;
    GicCpuInterface* cpu_ = nullptr;
    GicRedistributor* redist_ = nullptr;
    uint32_t num_interrupts_ = 0;
};

} // namespace xinim::hal::arm64