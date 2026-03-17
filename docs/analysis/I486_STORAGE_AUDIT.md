# i486 Storage Audit

Date: 2026-03-09

This note records the storage-driver choice for the next i486 QEMU milestone.

## Environment

- QEMU binary: `qemu-system-i386`
- Local version: `10.2.0`
- Active i486 machine profiles:
  - `pc-i440fx-10.2`
  - `isapc`

## QEMU Facts Confirmed Locally

From the installed QEMU 10.2.0 device model set:

- `pc-i440fx-10.2` exposes a PIIX-style southbridge path.
- relevant storage-related devices available include:
  - `ide-hd`
  - `ide-cd`
  - `isa-ide`
  - `piix3-ide`
  - `piix4-ide`
  - `ich9-ahci`
  - `virtio-blk-pci`

## Repo Facts Confirmed Locally

- The repo already contains:
  - a generic block-device abstraction in `include/xinim/block/blockdev.hpp`
  - block-device manager and partition support in `src/block/blockdev.cpp`
  - a full ext2 implementation in `src/vfs/ext2.cpp`
  - block-backed VFS filesystem interfaces in `include/xinim/vfs/filesystem.hpp`
- The repo does not currently expose a usable i486 IDE/ATA driver in the
  active i486 boot lane.
- The only concrete storage driver in-tree today is AHCI-oriented, which is
  not the right first target for QEMU 486 bring-up.

## Chosen Path

The next storage milestone for the i486 lane should be:

1. IDE/ATA PIO
2. primary channel, read-first
3. `pc-i440fx-10.2` as the first machine profile
4. compatibility review for `isapc` after the primary path is working

## Why This Path

- It matches the simplest historically appropriate 486-class QEMU storage path.
- It avoids depending on AHCI, which is not the right baseline for this lane.
- It avoids pulling in `virtio-blk` as the first persistent-rootfs story.
- It matches the repo's current split:
  - low-level block driver still missing
  - block/VFS/ext2 layers already exist

## Explicit Non-Choices

These are not the first persistent-rootfs target for the i486 lane:

- AHCI
- `virtio-blk-pci`
- NVMe
- USB storage

They may still be valid later, but they are not the narrowest honest path to a
real QEMU 486 disk-backed rootfs.

## Step-5 Implementation Contract

The first IDE milestone should provide:

- register-level ATA/IDE PIO definitions for the primary channel
- IDENTIFY DEVICE command support
- sector reads for a single attached disk
- no write path required yet
- no partition parsing required yet
- enough functionality to prove:
  - the driver can see the disk
  - sector reads work
  - the block interface bridge is feasible
