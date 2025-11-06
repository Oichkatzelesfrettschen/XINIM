# Hardware Support Matrix

Targeting modern x86_64 with QEMU parity and commodity PC hardware.

## CPU/Platform
- Architecture: x86_64 (64-bit Intel/AMD) only
- SMP: APIC/IOAPIC; HPET; TSC as clocksource; ACPI v2+ tables
- Boot: BIOS/UEFI via Limine, Multiboot2 alternative
- Features: AVX2/AVX512 SIMD support

## Storage
- AHCI (SATA); NVMe (later); ATAPI CD (optional)

## Network
- e1000/e1000e; virtio-net (later); Intel I219 (later)

## Console/Debug
- Serial (16550A), VGA text; early printk

## Priority Order
1) QEMU devices: q35 (modern PCIe) + AHCI + e1000 + serial
2) Common desktops/laptops: APIC/HPET/ACPI variations

