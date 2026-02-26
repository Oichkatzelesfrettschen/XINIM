# XINIM Phase 2 Execution Plan

**Status:** IN PROGRESS  
**Started:** 2025-11-06  
**Target Completion:** End of Phase 2  

---

## Overview

Phase 2 focuses on completing the driver implementations, adding critical system servers, and achieving full QEMU compatibility with maximal performance.

---

## Components to Implement

### 1. Reincarnation Server Implementation

**Status:** Header complete, implementation needed  
**Priority:** CRITICAL  
**Estimated Effort:** 3-4 days

**Tasks:**
- [ ] Implement core RS singleton
- [ ] Process spawning and monitoring
- [ ] Heartbeat system
- [ ] SIGCHLD handling for crash detection
- [ ] Automatic restart logic
- [ ] Dependency graph management
- [ ] State checkpointing
- [ ] Policy enforcement
- [ ] Statistics tracking
- [ ] Testing with mock drivers

**Dependencies:** Process management, IPC system

---

### 2. E1000 Driver Completion

**Status:** Framework 80% complete  
**Priority:** HIGH  
**Estimated Effort:** 2-3 days

**Tasks:**
- [x] Register definitions
- [x] Hardware initialization
- [x] EEPROM access
- [x] Link detection
- [ ] DMA buffer allocation integration
- [ ] RX descriptor ring setup
- [ ] TX descriptor ring setup
- [ ] Packet receive implementation
- [ ] Packet transmit implementation
- [ ] Interrupt handling completion
- [ ] Integration with network stack
- [ ] QEMU testing

**Current File:** `src/drivers/net/e1000.cpp`

**Next Steps:**
1. Integrate with DMA allocator
2. Setup RX/TX descriptor rings
3. Implement send_packet() fully
4. Implement receive_packet() fully
5. Test with ping/iperf

---

### 3. AHCI Driver Implementation

**Status:** Header complete, implementation needed  
**Priority:** HIGH  
**Estimated Effort:** 4-5 days

**Tasks:**
- [x] Register definitions
- [x] Port structures
- [x] FIS definitions
- [ ] Create ahci.cpp implementation
- [ ] HBA initialization
- [ ] Port detection and initialization
- [ ] Command list setup
- [ ] Received FIS setup
- [ ] Read sector implementation
- [ ] Write sector implementation
- [ ] NCQ support (optional)
- [ ] Hot-plug handling
- [ ] Error handling
- [ ] Integration with block layer
- [ ] QEMU testing

**New File:** `src/drivers/block/ahci.cpp`

**Next Steps:**
1. Implement HBA reset and initialization
2. Detect and initialize ports
3. Setup command/FIS structures
4. Implement read_sectors()
5. Implement write_sectors()
6. Test with disk operations

---

### 4. VirtIO-Net Driver Implementation

**Status:** Framework complete  
**Priority:** HIGH  
**Estimated Effort:** 3-4 days

**Tasks:**
- [x] VirtIO core framework
- [x] Virtqueue management
- [x] Feature negotiation
- [x] Network device class
- [ ] Create virtio_net.cpp implementation
- [ ] PCI capability parsing
- [ ] Queue setup (RX, TX, control)
- [ ] Feature negotiation implementation
- [ ] MAC address reading
- [ ] Packet transmission
- [ ] Packet reception
- [ ] Control queue operations
- [ ] Offload configuration
- [ ] Multi-queue support (optional)
- [ ] QEMU testing

**New File:** `src/drivers/virtio/virtio_net.cpp`

**Next Steps:**
1. Implement PCI transport
2. Setup RX/TX queues
3. Negotiate features
4. Implement packet send/receive
5. Test performance vs E1000

---

### 5. VirtIO-Block Driver Implementation

**Status:** Framework complete  
**Priority:** HIGH  
**Estimated Effort:** 3-4 days

**Tasks:**
- [x] VirtIO core framework
- [x] Virtqueue management
- [x] Block device class
- [ ] Create virtio_blk.cpp implementation
- [ ] PCI capability parsing
- [ ] Queue setup
- [ ] Feature negotiation
- [ ] Capacity reading
- [ ] Read sectors implementation
- [ ] Write sectors implementation
- [ ] Flush support
- [ ] TRIM/discard support
- [ ] Error handling
- [ ] QEMU testing

**New File:** `src/drivers/virtio/virtio_blk.cpp`

**Next Steps:**
1. Implement PCI transport
2. Setup request queue
3. Read capacity
4. Implement read/write operations
5. Test performance vs AHCI

---

### 6. DMA Allocator Implementation

**Status:** Header complete  
**Priority:** CRITICAL (blocks driver completion)  
**Estimated Effort:** 2-3 days

**Tasks:**
- [x] DMA buffer interface
- [x] DMA pool interface
- [x] Scatter-gather lists
- [ ] Create dma.cpp implementation
- [ ] Physical page allocator
- [ ] Virtual-physical mapping
- [ ] Cache coherency operations
- [ ] DMA pool implementation
- [ ] Buffer allocation/free
- [ ] Address translation
- [ ] Testing with drivers

**New File:** `src/mm/dma.cpp`

**Dependencies:** Memory management subsystem

---

### 7. VFS (Virtual File System) Layer

**Status:** Not started  
**Priority:** MEDIUM-HIGH  
**Estimated Effort:** 5-7 days

**Tasks:**
- [ ] Design VFS interface
- [ ] VNode abstraction
- [ ] Mount point management
- [ ] Path resolution
- [ ] File descriptor table
- [ ] Directory operations
- [ ] File operations (open, close, read, write)
- [ ] Integration with block devices
- [ ] MINIX filesystem support
- [ ] Testing

**New Files:**
- `include/xinim/vfs/vfs.hpp`
- `src/vfs/vfs.cpp`
- `src/vfs/vnode.cpp`
- `src/vfs/mount.cpp`

---

### 8. Network Stack Integration

**Status:** Not started  
**Priority:** MEDIUM  
**Estimated Effort:** 4-5 days

**Tasks:**
- [ ] Evaluate lwIP vs custom stack
- [ ] Integrate lwIP (recommended)
- [ ] Socket API implementation
- [ ] TCP/UDP support
- [ ] IP layer
- [ ] ICMP (ping)
- [ ] ARP
- [ ] Interface with network drivers
- [ ] Testing (ping, TCP connection)

**New Files:**
- `include/xinim/net/socket.hpp`
- `src/net/lwip_integration.cpp`
- `third_party/lwip/` (external)

---

### 9. Block Device Abstraction Layer

**Status:** Not started  
**Priority:** MEDIUM  
**Estimated Effort:** 2-3 days

**Tasks:**
- [ ] Block device interface
- [ ] Request queue
- [ ] I/O scheduler
- [ ] Partition table parsing (GPT, MBR)
- [ ] Integration with drivers
- [ ] Testing

**New Files:**
- `include/xinim/block/blockdev.hpp`
- `src/block/blockdev.cpp`
- `src/block/partition.cpp`

---

## Implementation Schedule

### Week 1 (Current)
- [x] Research and design (Phase 1)
- [x] Framework creation
- [x] Reincarnation Server design
- [x] QEMU compatibility matrix
- [ ] DMA allocator implementation
- [ ] Begin E1000 completion

### Week 2
- [ ] Complete E1000 driver
- [ ] Test E1000 in QEMU
- [ ] Begin AHCI implementation
- [ ] VirtIO PCI transport

### Week 3
- [ ] Complete AHCI driver
- [ ] Test AHCI in QEMU
- [ ] Complete VirtIO-Net
- [ ] Complete VirtIO-Block

### Week 4
- [ ] VFS layer implementation
- [ ] Network stack integration
- [ ] Block device layer
- [ ] Comprehensive testing

### Week 5 (Buffer)
- [ ] Bug fixes
- [ ] Performance optimization
- [ ] Documentation updates
- [ ] Final validation

---

## Testing Strategy

### Unit Tests
- DMA allocator correctness
- Descriptor ring management
- State machine transitions
- Error handling

### Integration Tests
- E1000 in QEMU (packet TX/RX)
- AHCI in QEMU (disk I/O)
- VirtIO-Net (performance vs E1000)
- VirtIO-Block (performance vs AHCI)
- Reincarnation Server (driver crashes)

### Performance Tests
- Network throughput (iperf)
- Disk sequential I/O (dd, fio)
- Disk random I/O (fio)
- Latency measurements
- CPU utilization

### Stress Tests
- High packet rate
- Concurrent disk I/O
- Driver crash recovery
- Memory exhaustion
- Long-running stability

---

## Success Criteria

### Phase 2 Complete When:
- [ ] All drivers functional in QEMU
- [ ] Network connectivity working
- [ ] Disk I/O working
- [ ] VFS can mount filesystems
- [ ] Network stack can ping
- [ ] Reincarnation Server can recover crashed drivers
- [ ] Performance targets met:
  - VirtIO-Net: > 5 Gbps
  - E1000: > 800 Mbps
  - VirtIO-Block: > 500 MB/s sequential
  - AHCI: > 80 MB/s sequential
- [ ] All tests passing
- [ ] Documentation complete

---

## Risk Management

### High Risk Items
1. **DMA allocator complexity** - Critical path, many dependencies
   - Mitigation: Start immediately, prototype first
2. **VirtIO PCI transport** - Complex specification
   - Mitigation: Reference Linux/MINIX implementations
3. **Network stack integration** - Large external dependency
   - Mitigation: Use well-tested lwIP

### Medium Risk Items
1. **AHCI command processing** - Complex state machine
2. **VFS path resolution** - Edge cases in Unix paths
3. **Performance targets** - May need optimization

### Low Risk Items
1. **E1000 completion** - Well-understood hardware
2. **Reincarnation Server** - Clear design from MINIX
3. **Testing** - Standard tools available

---

## Dependencies

### External
- lwIP network stack (well-maintained)
- QEMU 5.x+ for testing
- Build tools (xmake, clang-18)

### Internal
- DMA allocator (blocks all drivers)
- Process management (for RS)
- IPC system (for RS)
- Memory management (for DMA)

---

## Metrics Tracking

### Code Metrics
- Total LOC added
- Test coverage %
- Documentation coverage
- Code review comments addressed

### Performance Metrics
- Network throughput (Mbps/Gbps)
- Disk throughput (MB/s)
- IOPS (operations/sec)
- Latency (μs/ms)
- CPU utilization (%)

### Quality Metrics
- Bugs found
- Bugs fixed
- Crashes/stability issues
- Recovery success rate

---

## Resources

### Documentation
- Intel E1000 datasheet
- AHCI 1.3 specification
- VirtIO 1.0+ specification
- MINIX 3 source code
- OpenBSD/NetBSD driver sources

### Tools
- QEMU 10.x
- GDB for debugging
- iperf for network testing
- fio for disk testing
- Valgrind for memory checks

---

**Status:** ACTIVE  
**Last Updated:** 2025-11-06  
**Next Milestone:** DMA allocator + E1000 completion
