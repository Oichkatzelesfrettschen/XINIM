# QEMU Pentium i386 Configuration for Xinim

## Host System
- **OS:** CachyOS (Arch Linux)
- **Hypervisor:** QEMU/KVM (using TCG for strict Pentium emulation if KVM doesn't support old CPU models, but KVM preferred for speed if flags allow).
- **Tools:** `qemu-system-i386`, `libvirt`, `virt-manager`

## Virtual Machine Specification
- **Architecture:** i386 (32-bit x86)
- **CPU Model:** Pentium III (`-cpu pentium3`)
    - *Reason:* Provides a good balance of legacy compatibility and slightly more modern instruction sets (SSE) often utilized by "modern" 32-bit OS projects, while still remaining in the Pentium era.
- **RAM:** 256 MB
- **Storage:** 2 GB Raw Disk Image (`xinim_disk.img`)
- **Network:** User Mode Networking (SLIRP) with PCNet NIC
    - *Flag:* `-netdev user,id=net0 -device pcnet,netdev=net0`
    - *Reason:* Safest and most compatible legacy NIC emulation without requiring root privileges or bridge configuration.
- **Graphics:** Cirrus Logic (`-vga cirrus`) or Standard VGA.

## Quick Start
To launch the environment:
```bash
./scripts/run_qemu_pentium.sh
```

## Manual Launch Command
```bash
qemu-system-i386 \
    -M pc \
    -cpu pentium3 \
    -m 256M \
    -drive file=xinim_disk.img,format=raw,index=0,media=disk \
    -netdev user,id=net0 -device pcnet,netdev=net0 \
    -vga cirrus \
    -serial stdio \
    -boot menu=on
```
