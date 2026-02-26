# PHASE 2: ULTRA-GRANULAR EXECUTION ROADMAP

**Date:** 2025-11-19
**Status:** 🔴 **CRITICAL BLOCKERS IDENTIFIED**
**Branch:** `claude/phase2-drivers-debt-018NNYQuW6XJrQJL2LLUPS45`
**Previous Commit:** ae01cfd (Infrastructure Foundation)

---

## SANITY CHECK RESULTS

### ⚠️ CRITICAL BLOCKERS (MUST FIX IMMEDIATELY)

| # | Blocker | Severity | Impact | Fix Time |
|---|---------|----------|--------|----------|
| 1 | **AHCI structures undefined** | 🔴 CRITICAL | Won't compile | 2-3 hours |
| 2 | **No IRQ management** | 🔴 CRITICAL | Drivers can't receive interrupts | 4-6 hours |
| 3 | **Hardcoded CPU frequency** | 🟠 HIGH | Timing inaccuracies | 2-3 hours |
| 4 | **DMA not integrated with PMM** | 🟠 HIGH | Memory conflicts | 4-6 hours |
| 5 | **VFS has zero security** | 🟠 HIGH | Security vulnerability | 3-4 hours |

### 📊 Current Completion Status

| Component | Completeness | Functional? | Blockers |
|-----------|--------------|-------------|----------|
| **DMA Allocator** | 70% | ✅ YES (limited) | PMM integration |
| **PCI Subsystem** | 85% | ✅ YES | MMU mapping |
| **Timing** | 40% | ⚠️ INACCURATE | PIT calibration |
| **E1000 Driver** | 45% | ❌ NO | Descriptor setup, TX/RX |
| **AHCI Driver** | 25% | ❌ WON'T COMPILE | Missing structures |
| **VFS Security** | 0% | ❌ NO | All checks missing |

---

## ULTRA-GRANULAR TASK BREAKDOWN

### 🔥 PRIORITY 1: FIX COMPILATION BLOCKERS (3 hours)

#### Task 1.1: Define AHCI Memory-Mapped Structures (2 hours)

**File:** `include/xinim/drivers/ahci.hpp`

**Subtasks:**
1. [ ] Add `HBAMemory` structure (Generic Host Control registers)
   - CAP, GHC, IS, PI, VS, CCC_CTL, CCC_PORTS, EM_LOC, EM_CTL, CAP2, BOHC
   - 32 port register arrays
   - Reserved space padding

2. [ ] Add `HBAPort` structure (Per-port registers)
   - CLB, CLBU, FB, FBU, IS, IE, CMD, TFD, SIG, SSTS, SCTL, SERR, SACT, CI, SNTF, FBS
   - Reserved space

3. [ ] Add `HBACommandHeader` structure (Command List Entry)
   - Flags (CFL, A, W, P, R, B, C, PMP, PRDTL)
   - PRDBC, CTBA, CTBAU
   - Reserved

4. [ ] Add `HBACommandTable` structure (Command Table)
   - CFIS[64], ACMD[16], RSRVD[48]
   - PRDT entries array

5. [ ] Add `HBAFIS` structure (Received FIS)
   - DMA Setup FIS
   - PIO Setup FIS
   - D2H Register FIS
   - Set Device Bits FIS
   - Unknown FIS
   - Reserved

6. [ ] Add `HBAPRDTEntry` structure (Physical Region Descriptor)
   - DBA, DBAU, Reserved, DBC

7. [ ] Add FIS type structures
   - `FIS_REG_H2D` (Host to Device Register FIS)
   - `FIS_REG_D2H` (Device to Host Register FIS)
   - `FIS_DMA_SETUP` (DMA Setup FIS)
   - `FIS_PIO_SETUP` (PIO Setup FIS)

8. [ ] Add proper packing attributes (`__attribute__((packed))`)

9. [ ] Add size assertions to verify structure layouts

10. [ ] Test compilation

**Estimated LOC:** ~200 lines

**Dependencies:** None

**Validation:**
```cpp
static_assert(sizeof(HBAMemory) == 0x1100, "HBAMemory size mismatch");
static_assert(sizeof(HBAPort) == 0x80, "HBAPort size mismatch");
static_assert(sizeof(HBACommandHeader) == 32, "CommandHeader size mismatch");
```

#### Task 1.2: Verify E1000 Compilation (30 min)

**Subtasks:**
1. [ ] Add missing includes if needed
2. [ ] Fix any unused variable warnings in stubs
3. [ ] Verify no link errors

#### Task 1.3: Test Build System (30 min)

**Subtasks:**
1. [ ] Run `xmake config`
2. [ ] Run `xmake build` and fix any errors
3. [ ] Document any warnings

**Deliverable:** Clean compilation with no errors

---

### 🔥 PRIORITY 2: IMPLEMENT IRQ MANAGEMENT (6 hours)

#### Task 2.1: Create IRQ Subsystem (4 hours)

**Files to Create:**
- `include/xinim/kernel/irq.hpp`
- `src/kernel/irq.cpp`

**Subtasks:**
1. [ ] Design IRQ handler interface
   ```cpp
   using IRQHandler = void (*)(void* context);
   int allocate_irq(IRQHandler handler, void* context);
   void free_irq(int irq);
   ```

2. [ ] Implement IRQ allocation bitmap (IRQs 0-255)

3. [ ] Create IRQ handler dispatch mechanism

4. [ ] Add IRQ masking/unmasking (PIC/IOAPIC)

5. [ ] Add statistics tracking (interrupt count per IRQ)

6. [ ] Thread-safe with spinlocks

7. [ ] Integrate with existing IDT

**Estimated LOC:** ~300 lines

#### Task 2.2: Add MSI/MSI-X Support to PCI (2 hours)

**File:** `include/xinim/pci/pci.hpp` + `.cpp`

**Subtasks:**
1. [ ] Add PCI capability parsing
2. [ ] Find MSI capability (0x05) and MSI-X capability (0x11)
3. [ ] Implement `enable_msi(device, handler)`
4. [ ] Implement `enable_msix(device, handler)`
5. [ ] Program Message Address/Data registers
6. [ ] Return allocated IRQ number

**Estimated LOC:** ~200 lines

**Deliverable:** Drivers can register interrupt handlers

---

### 🔥 PRIORITY 3: FIX TIMING ACCURACY (3 hours)

#### Task 3.1: Implement PIT-Based TSC Calibration (2.5 hours)

**File:** `src/kernel/timing.cpp`

**Subtasks:**
1. [ ] Add PIT (Programmable Interval Timer) initialization
   ```cpp
   void calibrate_tsc_with_pit() {
       // Program PIT for known interval (e.g., 10ms)
       // Read TSC at start
       // Wait for PIT to fire
       // Read TSC at end
       // Calculate CPU frequency
   }
   ```

2. [ ] Add CPUID-based frequency detection (if available)
   ```cpp
   uint64_t get_cpu_freq_from_cpuid() {
       // Check if CPUID leaf 0x15 supported
       // Read TSC/crystal ratio
       // Calculate frequency
   }
   ```

3. [ ] Add sanity checking (1-10 GHz range)

4. [ ] Fallback chain: CPUID → PIT → Hardcoded default

5. [ ] Add debug output showing detected frequency

**Estimated LOC:** ~150 lines

#### Task 3.2: Test Timing Accuracy (30 min)

**Subtasks:**
1. [ ] Create test: udelay(1000) should take ~1ms
2. [ ] Verify using external timing source
3. [ ] Add tolerance checking (±10%)

**Deliverable:** Accurate timing on all CPUs

---

### 🔧 PRIORITY 4: COMPLETE E1000 DRIVER (20 hours)

#### Task 4.1: Integrate Infrastructure (2 hours)

**File:** `src/drivers/net/e1000.cpp`

**Subtasks:**
1. [ ] Replace Line 68 TODO: Use `PCI::find_device()` to locate E1000
   ```cpp
   const PCIDevice* pci_dev = PCI::find_device(E1000_VENDOR_ID, device_id_);
   BAR bar0;
   PCI::read_bar(*pci_dev, 0, bar0);
   mmio_base_ = reinterpret_cast<volatile uint32_t*>(PCI::map_bar(bar0));
   ```

2. [ ] Replace Line 176 TODO: Use `mdelay(10)` instead of busy loop

3. [ ] Line 142 TODO: Free DMA buffers in destructor
   ```cpp
   if (rx_buffers_) { DMAAllocator::free(rx_buffers_); }
   if (tx_buffers_) { DMAAllocator::free(tx_buffers_); }
   ```

4. [ ] Enable PCI bus master, memory space

**Estimated LOC:** ~50 lines modified

#### Task 4.2: Allocate Descriptor Rings (4 hours)

**Subtasks:**
1. [ ] Line 239: Implement RX descriptor ring allocation
   ```cpp
   void E1000Driver::init_rx() {
       // Allocate descriptor ring: 256 descriptors * 16 bytes = 4KB
       rx_descriptors_ = DMAAllocator::allocate(
           256 * sizeof(RxDescriptor),
           DMAFlags::BELOW_4GB | DMAFlags::ZERO
       );

       // Allocate RX buffers: 256 * 2KB = 512KB
       for (int i = 0; i < 256; i++) {
           rx_buffers_[i] = DMAAllocator::allocate(2048, DMAFlags::BELOW_4GB);
           // Set up descriptor
           auto* desc = &rx_descriptors_[i];
           desc->buffer_addr = rx_buffers_[i].physical_addr;
           desc->length = 0;
           desc->status = 0;
       }

       // Program hardware registers
       write_reg(REG_RDBAL, rx_descriptors_.physical_addr & 0xFFFFFFFF);
       write_reg(REG_RDBAH, rx_descriptors_.physical_addr >> 32);
       write_reg(REG_RDLEN, 256 * 16);
       write_reg(REG_RDH, 0);
       write_reg(REG_RDT, 255);  // All descriptors available
   }
   ```

2. [ ] Line 269: Implement TX descriptor ring allocation (similar to RX)

3. [ ] Line 349: Implement `setup_rx_descriptors()` (link descriptors)

4. [ ] Line 353: Implement `setup_tx_descriptors()` (link descriptors)

**Estimated LOC:** ~200 lines

#### Task 4.3: Implement Packet Transmission (6 hours)

**Subtasks:**
1. [ ] Line 286: Implement `send_packet(data, length)`
   ```cpp
   bool E1000Driver::send_packet(const uint8_t* data, uint16_t length) {
       // Get next TX descriptor
       uint32_t tail = get_tx_tail();
       auto* desc = &tx_descriptors_[tail];

       // Check if descriptor is available (DD bit set)
       if (!(desc->status & TXDESC_STATUS_DD)) {
           return false;  // Queue full
       }

       // Copy data to TX buffer
       memcpy(tx_buffers_[tail].virtual_addr, data, length);
       DMAAllocator::sync_for_device(tx_buffers_[tail]);

       // Set up descriptor
       desc->length = length;
       desc->cmd = TXDESC_CMD_EOP | TXDESC_CMD_IFCS | TXDESC_CMD_RS;
       desc->status = 0;

       // Advance tail
       tail = (tail + 1) % 256;
       write_reg(REG_TDT, tail);

       return true;
   }
   ```

2. [ ] Add TX timeout handling

3. [ ] Add TX queue management (head/tail tracking)

4. [ ] Line 339: Implement TX completion in interrupt handler
   ```cpp
   // In handle_interrupt():
   if (status & ICR_TXDW) {  // TX descriptor written back
       // Process completed TX descriptors
       uint32_t head = read_reg(REG_TDH);
       while (tx_head_ != head) {
           // TX completed, free resources if needed
           tx_head_ = (tx_head_ + 1) % 256;
       }
   }
   ```

**Estimated LOC:** ~150 lines

#### Task 4.4: Implement Packet Reception (6 hours)

**Subtasks:**
1. [ ] Line 293: Implement `receive_packet(buffer, length)`
   ```cpp
   bool E1000Driver::receive_packet(uint8_t* buffer, uint16_t& length) {
       uint32_t tail = get_rx_tail();
       auto* desc = &rx_descriptors_[tail];

       // Check if descriptor has received packet (DD bit set)
       if (!(desc->status & RXDESC_STATUS_DD)) {
           return false;  // No packet
       }

       // Check for errors
       if (desc->errors) {
           // Reset descriptor and return error
           desc->status = 0;
           advance_rx_tail();
           return false;
       }

       // Copy packet data
       DMAAllocator::sync_for_cpu(rx_buffers_[tail]);
       length = desc->length;
       memcpy(buffer, rx_buffers_[tail].virtual_addr, length);

       // Reset descriptor for reuse
       desc->status = 0;
       advance_rx_tail();

       return true;
   }
   ```

2. [ ] Line 334: Implement RX processing in interrupt handler
   ```cpp
   if (status & ICR_RXT0) {  // RX timer interrupt
       // Process all available packets
       uint8_t packet[2048];
       uint16_t length;
       while (receive_packet(packet, length)) {
           // TODO: Send to network stack
           // For now, just count received packets
           rx_packets_++;
       }
   }
   ```

3. [ ] Add RX buffer refill logic

4. [ ] Add statistics tracking (packets, bytes, errors)

**Estimated LOC:** ~150 lines

#### Task 4.5: Integrate IRQ Handler (2 hours)

**Subtasks:**
1. [ ] Allocate IRQ for E1000
   ```cpp
   irq_num_ = allocate_irq(
       [](void* ctx) { static_cast<E1000Driver*>(ctx)->handle_interrupt(); },
       this
   );
   ```

2. [ ] Enable interrupts in hardware
   ```cpp
   write_reg(REG_IMS, ICR_LSC | ICR_RXT0 | ICR_TXDW);
   ```

3. [ ] Add interrupt statistics

**Estimated LOC:** ~50 lines

**Total E1000 Effort:** ~550 new/modified lines

---

### 🔧 PRIORITY 5: COMPLETE AHCI DRIVER (25 hours)

#### Task 5.1: Allocate AHCI Memory Structures (6 hours)

**Subtasks:**
1. [ ] Line 155: Allocate command list (32 entries * 32 bytes = 1KB per port)
   ```cpp
   void AHCIDriver::init_port(uint32_t port_num) {
       HBAPort* port = &hba_->ports[port_num];

       // Allocate command list
       DMABuffer cmd_list = DMAAllocator::allocate(
           32 * sizeof(HBACommandHeader),
           DMAFlags::BELOW_4GB | DMAFlags::ZERO
       );

       // Set CLB/CLBU registers
       port->clb = cmd_list.physical_addr & 0xFFFFFFFF;
       port->clbu = cmd_list.physical_addr >> 32;

       // Allocate FIS receive area (256 bytes)
       DMABuffer fis_area = DMAAllocator::allocate(
           256,
           DMAFlags::BELOW_4GB | DMAFlags::ZERO
       );

       port->fb = fis_area.physical_addr & 0xFFFFFFFF;
       port->fbu = fis_area.physical_addr >> 32;

       // Allocate command tables (one per slot)
       for (int i = 0; i < 32; i++) {
           DMABuffer cmd_table = DMAAllocator::allocate(
               sizeof(HBACommandTable) + (8 * sizeof(HBAPRDTEntry)),
               DMAFlags::BELOW_4GB | DMAFlags::ZERO
           );

           HBACommandHeader* cmd_hdr = &((HBACommandHeader*)cmd_list.virtual_addr)[i];
           cmd_hdr->ctba = cmd_table.physical_addr & 0xFFFFFFFF;
           cmd_hdr->ctbau = cmd_table.physical_addr >> 32;
       }
   }
   ```

**Estimated LOC:** ~200 lines

#### Task 5.2: Implement Sector Read (8 hours)

**Subtasks:**
1. [ ] Line 176-183: Implement `read_sectors()` following the 6-step plan
   ```cpp
   bool AHCIDriver::read_sectors(uint32_t port_num, uint64_t lba,
                                  uint16_t count, void* buffer) {
       // 1. Find free command slot
       HBAPort* port = &hba_->ports[port_num];
       uint32_t slots = (port->sact | port->ci);
       int slot = -1;
       for (int i = 0; i < 32; i++) {
           if (!(slots & (1 << i))) {
               slot = i;
               break;
           }
       }
       if (slot == -1) return false;  // No free slots

       // 2. Build command FIS (H2D Register FIS)
       HBACommandTable* cmd_table = get_command_table(port_num, slot);
       FIS_REG_H2D* fis = (FIS_REG_H2D*)cmd_table->cfis;
       memset(fis, 0, sizeof(FIS_REG_H2D));

       fis->fis_type = FIS_TYPE_REG_H2D;
       fis->c = 1;  // Command
       fis->command = ATA_CMD_READ_DMA_EX;
       fis->lba0 = lba & 0xFF;
       fis->lba1 = (lba >> 8) & 0xFF;
       fis->lba2 = (lba >> 16) & 0xFF;
       fis->device = 1 << 6;  // LBA mode
       fis->lba3 = (lba >> 24) & 0xFF;
       fis->lba4 = (lba >> 32) & 0xFF;
       fis->lba5 = (lba >> 40) & 0xFF;
       fis->countl = count & 0xFF;
       fis->counth = (count >> 8) & 0xFF;

       // 3. Set up PRDT for DMA
       DMABuffer data_buffer = DMAAllocator::allocate(
           count * 512,
           DMAFlags::BELOW_4GB
       );

       HBAPRDTEntry* prdt = &cmd_table->prdt_entry[0];
       prdt->dba = data_buffer.physical_addr & 0xFFFFFFFF;
       prdt->dbau = data_buffer.physical_addr >> 32;
       prdt->dbc = (count * 512) - 1;  // Byte count
       prdt->i = 1;  // Interrupt on completion

       // 4. Set up command header
       HBACommandHeader* cmd_hdr = get_command_header(port_num, slot);
       cmd_hdr->cfl = sizeof(FIS_REG_H2D) / 4;  // FIS length in DWORDs
       cmd_hdr->w = 0;  // Read from device
       cmd_hdr->prdtl = 1;  // One PRDT entry

       // 5. Issue command
       port->ci = 1 << slot;

       // 6. Wait for completion (with timeout)
       uint32_t timeout = 1000;  // 1 second
       while (timeout--) {
           if (!(port->ci & (1 << slot))) {
               // Command completed
               DMAAllocator::sync_for_cpu(data_buffer);
               memcpy(buffer, data_buffer.virtual_addr, count * 512);
               DMAAllocator::free(data_buffer);
               return true;
           }
           mdelay(1);
       }

       // Timeout
       DMAAllocator::free(data_buffer);
       return false;
   }
   ```

**Estimated LOC:** ~300 lines

#### Task 5.3: Implement Sector Write (6 hours)

**Subtasks:**
1. [ ] Line 194: Implement `write_sectors()` (similar to read, but W=1 in cmd_hdr)

2. [ ] Add data buffer setup before issuing command

3. [ ] Add cache flush after write

**Estimated LOC:** ~250 lines

#### Task 5.4: Implement Error Handling (3 hours)

**Subtasks:**
1. [ ] Line 229: Implement error interrupt handling
   ```cpp
   if (port->is & (PORT_IS_TFES | PORT_IS_HBFS | PORT_IS_HBDS | PORT_IS_IFS)) {
       // Task file error, host bus error, etc.
       // Reset port
       // Log error
       // Notify upper layer
   }
   ```

2. [ ] Add port reset on error

3. [ ] Add error recovery state machine

**Estimated LOC:** ~150 lines

#### Task 5.5: Implement Completion Handling (2 hours)

**Subtasks:**
1. [ ] Line 235: Implement completion interrupt handling
   ```cpp
   if (port->is & PORT_IS_DHRS) {  // D2H Register FIS received
       // Command completed successfully
       // Check status
       // Notify upper layer
   }
   ```

**Estimated LOC:** ~100 lines

**Total AHCI Effort:** ~1,000 new/modified lines

---

### 🔒 PRIORITY 6: VFS SECURITY (8 hours)

#### Task 6.1: Add Permission Check Infrastructure (3 hours)

**File:** `src/vfs/vfs.cpp`

**Subtasks:**
1. [ ] Add uid/gid to process structure (or use existing)

2. [ ] Implement permission checking function
   ```cpp
   bool VFS::check_permission(const VNode* vnode, uint32_t mode,
                              uint32_t uid, uint32_t gid) {
       // Check owner permissions
       if (vnode->attrs.uid == uid) {
           return (vnode->attrs.mode & (mode << 6)) != 0;
       }

       // Check group permissions
       if (vnode->attrs.gid == gid) {
           return (vnode->attrs.mode & (mode << 3)) != 0;
       }

       // Check other permissions
       return (vnode->attrs.mode & mode) != 0;
   }
   ```

3. [ ] Add capability checking (root bypass)

**Estimated LOC:** ~100 lines

#### Task 6.2: Add Checks to File Operations (5 hours)

**Subtasks:**
1. [ ] Line 215: Add permission check to `open()`
   ```cpp
   if (!check_permission(vnode, requested_mode, current_uid, current_gid)) {
       return nullptr;  // EACCES
   }
   ```

2. [ ] Add checks to create(), mkdir(), unlink(), rmdir()

3. [ ] Add checks to rename(), link(), symlink()

4. [ ] Add parent directory write permission checks

**Estimated LOC:** ~150 lines

**Total VFS Security Effort:** ~250 lines

---

### 📦 PRIORITY 7: CONFIGURATION EXTRACTION (4 hours)

**Files to Create:**
- `include/xinim/config/system.hpp`
- `include/xinim/config/drivers.hpp`
- `include/xinim/config/memory.hpp`

**Subtasks:**
1. [ ] Extract E1000 constants (descriptor counts, buffer sizes)
2. [ ] Extract AHCI constants (command slots, timeouts)
3. [ ] Extract memory constants (DMA zone size, page size)
4. [ ] Extract timing constants (calibration intervals)
5. [ ] Update all code to use config values

**Estimated LOC:** ~300 lines

---

## EXECUTION PLAN

### Week 1: Critical Blockers
- **Day 1-2:** Fix AHCI structures, implement IRQ management
- **Day 3:** Fix timing calibration, test infrastructure
- **Day 4-5:** E1000 descriptor allocation and basic TX/RX

### Week 2: Driver Completion
- **Day 6-7:** Complete E1000 packet handling
- **Day 8-10:** Complete AHCI sector I/O

### Week 3: Security and Polish
- **Day 11-12:** VFS security implementation
- **Day 13:** Configuration extraction
- **Day 14:** Integration testing, documentation

---

## SUCCESS CRITERIA

- [ ] All code compiles without errors or warnings
- [ ] E1000 can send and receive packets in QEMU
- [ ] AHCI can read and write sectors in QEMU
- [ ] VFS has permission checks on all file operations
- [ ] All hardcoded values extracted to config headers
- [ ] IRQ management functional with MSI/MSI-X
- [ ] Timing accurate on different CPU speeds
- [ ] Zero critical TODOs remaining
- [ ] Full test suite passes

---

## ESTIMATED TOTAL EFFORT

| Work Stream | Hours | LOC |
|-------------|-------|-----|
| AHCI Structures | 2 | 200 |
| IRQ Management | 6 | 500 |
| Timing Calibration | 3 | 150 |
| E1000 Completion | 20 | 550 |
| AHCI Completion | 25 | 1,000 |
| VFS Security | 8 | 250 |
| Configuration | 4 | 300 |
| **TOTAL** | **68 hours** | **2,950 LOC** |

**Timeline:** 3 weeks (full-time) or 6 weeks (part-time)

---

**Document Status:** ✅ ULTRA-GRANULAR ROADMAP COMPLETE
**Next Action:** Begin execution with AHCI structure definitions
**Branch:** `claude/phase2-drivers-debt-018NNYQuW6XJrQJL2LLUPS45`
