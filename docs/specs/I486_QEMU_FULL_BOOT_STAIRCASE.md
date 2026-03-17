# i486 QEMU Full-Boot Staircase

Date: 2026-03-09

This document defines the concrete staircase from the current i486 QEMU lane
to a fuller local-console and persistent-rootfs OS image. It is deliberately
acceptance-driven: each step is only "done" when its gate is automated or
otherwise reproducible in-tree.

The supervision vocabulary used here is aligned with
`docs/specs/I486_SERVICE_SUPERVISION_SYNTHESIS.md`.

## Current Baseline

- QEMU target: `qemu-system-i386` 10.2.0
- CPU target: `-cpu 486`
- Machine profiles under active test:
  - `pc-i440fx-10.2`
  - `isapc`
- Boot path:
  - GRUB + Multiboot2 ISO
  - Ring 3 supervised init service runs native `mksh`
  - `mksh` is linked against the repo-retargeted `dietlibc`
  - supervised init-service restart handles clean shell exit instead of
    dropping into rescue
- Current user interaction:
  - COM2 automation path is working
  - VGA text output is working
  - local tty now accepts keyboard input through the i486 console path
- Current filesystem reality:
  - bootfs seeding and bootfs-to-VFS promotion are working
  - a live read-only ext2 namespace is mounted at `/persist`
  - canonical `/bin` and `/etc` paths can resolve to the ext2 disk with bootfs fallback
  - mksh can source disk-backed config and execute disk-backed i486 ELF payloads
  - narrow write persistence is now wired for existing ext2 files with allocated blocks
  - general writable persistent rootfs is not wired yet

## Definition Of "Full Boot" For i486

For this lane, "full boot" does not mean every future subsystem exists. It
means the image behaves like a small real OS image on QEMU 486 hardware:

1. The system boots to a supervised native Ring 3 `mksh` init service.
2. The shell is reachable on the local console, not only on COM2.
3. The local console provides VGA text output and keyboard input.
4. The shell can execute the shipped utility baseline from `/bin`.
5. The root filesystem is mounted from a persistent disk image, not only from
   boot modules.
6. Basic write persistence survives reboot.
7. Rescue remains a failure path, not the normal lifecycle.

## Acceptance Contract

### A. Supervised Init-Service Contract

- Required:
  - `/bin/mksh` is the selected supervised init payload
  - `/bin/sh` resolves to the same native shell payload
  - shell `exit` respawns the supervised init service
  - service launch failure falls back to rescue
- Current status:
  - implemented

### B. Console Contract

- Required:
  - kernel boot banners appear on VGA text
  - shell I/O works on the local console
  - COM2 remains available for automation and debugging
  - keyboard enter, backspace, letters, digits, and common punctuation work
- Current status:
  - VGA text output: implemented
  - keyboard-backed tty input: implemented
  - formal local-console interactive test: pending

### C. Userland Contract

- Required baseline commands:
  - `echo`
  - `pwd`
  - `ls`
  - `cat`
  - `hello`
  - `false`
  - `heapprobe`
- Required shell behaviors:
  - quoting
  - environment export
  - `fork` / `execve` / `wait4`
  - command lookup
  - exit-status propagation
- Current status:
  - implemented and QEMU-tested over COM2

### D. Persistent Rootfs Contract

- Required:
  - an i486 disk image is attached in QEMU
  - a block driver reads sectors from the attached disk
  - ext2 rootfs mounts during boot
  - `/bin`, `/etc`, and writable directories are served from the mounted rootfs
  - creating and editing files persists across reboot
- Current status:
  - partially implemented:
    - deterministic ATA ext2 disk image exists
    - live read-only ext2 namespace is available at `/persist`
    - mksh can read disk-backed files, source a disk-backed profile, and exec a
      disk-backed i486 binary from `/persist/bin`
  - not implemented:
    - mounted root replacing `/bin` and `/etc`
    - write persistence across reboot

### E. Driver Contract For The Next Stage

- Must exist for the next milestone:
  - IDE/ATA PIO read path on the QEMU 486 baseline
  - block-device adapter usable by the existing VFS/ext2 code
- Explicitly not claimed yet:
  - AHCI
  - virtio-blk
  - advanced graphics
  - broad networking

## The Next 15 Concrete Steps

1. Codify this i486 full-boot acceptance contract in repo docs and keep it in
   sync with tests.
2. Add a local-console smoke path that proves VGA text output and live shell
   availability without relying solely on COM2.
3. Split bootstrap-shell language from init-shell language across i486 docs,
   scripts, and test names.
4. Audit and choose the narrowest i486-compatible storage path:
   IDE/ATA PIO for QEMU 486 first.
5. Implement ATA IDENTIFY plus sector reads for the primary IDE channel.
6. Wrap the IDE driver in the repo block-device interface.
7. Build a deterministic ext2 disk image for the i486 lane.
8. Attach that disk image in the i486 QEMU flow.
9. Mount ext2 rootfs during i486 boot while retaining bootfs fallback.
10. Switch executable and config lookup to the mounted rootfs.
11. Add write-path shell tests on the mounted rootfs.
12. Add reboot-persistence tests for the same image.
13. Define `/dev` policy for the i486 lane after rootfs mount.
14. Improve tty behavior beyond raw key intake where needed by mksh and
    utilities.
15. Begin the post-rootfs driver staircase with `ne2k_isa` and the multiarch
    driver HAL alignment.

## Immediate Execution Order

The next implementation sequence should be:

1. local-console smoke coverage
2. IDE/ATA PIO storage audit
3. IDE read driver
4. block-device bridge
5. ext2 disk image and mount path
6. persistence tests

This order keeps the current mksh-capable i486 lane stable while growing it
into a real QEMU 486 OS image one subsystem at a time.

## Persistent-Root Phase: Next 10 Steps

Status snapshot as of 2026-03-09:

- completed:
  - reproducible ATA disk image generation
  - IDE/ATA PIO IDENTIFY and sector reads
  - MBR partition probe
  - ext2 superblock probe
  - fixed-path ext2 file probe for `/etc/persist.txt`
  - live ext2 path resolution for files and directories under `/persist`
  - mount-style `/persist` namespace registration instead of imported overlay blobs
  - single-indirect ext2 file reads for larger guest payloads
  - native i486 `ls` utility staged in `/bin`
  - QEMU smoke and mksh assertions for disk-backed persistence fixtures
  - disk-backed config sourcing through `/persist/etc/persist-profile`
  - disk-backed executable launch through `/persist/bin/persist-hello`
  - canonical `/etc/motd` and `/etc/persist-profile` now resolve from ext2 when present
  - canonical `/bin/persist-hello` now resolves and executes through ext2-backed lookup
  - ATA-backed in-place ext2 writes for existing files survive reboot on both `pc-i440fx`
    and `isapc`
  - ext2 file creation now works for the freestanding i486 lane
  - ext2 directory creation now works for the freestanding i486 lane
  - ext2 rename now works for:
    - same-directory file and directory rename
    - cross-directory regular-file rename
    - regular-file destination replacement during rename
    - cross-directory non-empty directory rename with `..` rewrite
  - shrink-side truncate now:
    - zeroes surviving tail bytes in the last kept block
    - frees direct and single-indirect blocks past EOF
    - drops an empty indirect table
  - ext2 file unlink now works for the freestanding i486 lane
  - ext2 empty-directory removal now works for the freestanding i486 lane
- in progress:
  - freestanding storage handoff for later mount integration
  - broadening mounted-root lookup beyond `/bin` and `/etc`
- not started:
  - metadata mutation
  - multi-group allocation

The next 10 task-level steps are:

1. Lock the persistent-root acceptance contract.
   Required proof:
   - `/etc/persist.txt` remains readable from the ATA ext2 image
   - persistent root mount is distinct from bootfs fallback
   - shell lookup precedence is explicit and tested

2. Keep the freestanding ext2 reader narrow and stable.
   Required proof:
   - superblock load
   - group descriptor load
   - root inode read
   - root directory lookup
   - fixed-path file read

3. Promote the ext2 reader into a tiny read-only block adapter.
   Required proof:
   - block reads are partition-relative
   - no hosted STL or libc++ runtime assumptions enter the i486 kernel
   - all large buffers remain off the hot kernel stack

4. Add a tiny read-only inode-to-vnode bridge for the mounted ext2 root.
   Required proof:
   - `lookup("/")`
   - `lookup("/etc")`
   - `lookup("/etc/persist.txt")`
   - directory iteration for the root and `/etc`

5. Mount the ext2 partition at a dedicated path first.
   Required proof:
   - boot logs show a successful ext2 mount point
   - shell-visible reads from that mount path return the persistent file
   - bootfs remains untouched as fallback
   Current status:
   - satisfied for the dedicated read-only namespace:
     `/persist` is a live ext2-backed mount-style namespace
   - not yet the primary mounted root

6. Add shell-visible persistence fixtures.
   Required proof:
   - `cat` of a file that exists only in the ext2 image
   - directory existence checks under the dedicated persistence path
   - command resolution can distinguish bootfs and mounted-root origins
   Current status:
   - `cat /persist/etc/persist.txt`, `cat /persist/etc/issue`, and
     `cat /persist/var/disk-marker` are live from the ATA ext2 image
   - `test -d /persist`, `test -d /persist/etc`, and `test -d /persist/var`
     are verified from mksh
   - `ls /`, `ls /persist`, `ls /persist/bin`, `ls /persist/etc`, and
     `ls /persist/var` are verified from mksh
   - `. /persist/etc/persist-profile` and `PATH=/persist/bin:/bin persist-hello`
     are verified from mksh

7. Switch executable lookup to prefer mounted root over bootfs.
   Required proof:
   - `execve` can launch `/bin/persist-hello` from the ext2 image with `PATH=/bin`
   - bootfs still works when the ext2 path is missing or invalid
   Current status:
   - implemented for the first canonical mounted-root prefixes: `/bin` and `/etc`
   - broader precedence policy for additional prefixes remains future work

8. Switch config and data lookup to mounted root where safe.
   Required proof:
   - disk-backed config sourcing works through canonical `/etc/persist-profile`
   - canonical `/etc/motd` resolves to the ext2 root when present
   Current status:
   - implemented for `/etc`
   - remaining work is expanding the policy beyond the initial canonical prefix set

9. Grow the writable ext2 mutation contract without abandoning tiny-kernel constraints.
   Required proof:
   - create, write, rename, unlink, and `rmdir` survive reboot on both `pc-i440fx`
     and `isapc`
   - cross-parent directory moves keep nested files reachable
   - parent link counts and the moved directory's `..` entry remain coherent enough
     for continued lookup
   Current status:
   - implemented for the current narrow mutation set
   - still missing directory-target overwrite, metadata mutation, and multi-group policy

10. Decide the next mutation staircase explicitly instead of silently widening semantics.
    Required proof:
    - design doc records which mutation features are real
    - tests match the implemented semantics
    - deferred pieces stay listed as deferred
    Current status:
    - active and documented

## Mutation Hardening Staircase

The remaining ext2 mutation work is now small enough to track as adjacent-file
substeps instead of one vague hardening bucket:

1. Keep shrink-side truncate honest in [ext2_reader.cpp](/home/eirikr/Github/XINIM/src/kernel/i486/ext2_reader.cpp).
   Required proof:
   - short rewrite after a longer file does not leak stale tail bytes
   - direct blocks past EOF are reclaimed
   - single-indirect blocks past EOF are reclaimed
   Current status:
   - implemented and covered by the persistence test

2. Decide whether truncate growth belongs in the tiny lane.
   Required proof:
   - either explicit rejection remains documented
   - or a zero-fill growth model is implemented and tested
   Current status:
   - shrink-only by design

3. Decide whether rename needs best-effort rollback for mid-operation failures.
   Required proof:
   - code and docs agree on failure ordering
   - target/source reachability tradeoffs are explicit
   Current status:
   - donor-inspired ordering only, no rollback repair path

4. Add directory-target overwrite only if a concrete userland need appears.
   Required proof:
   - empty-target checks
   - link-count updates
   - `..` rewrite for replaced directories
   Current status:
   - deferred

5. Expand allocation policy beyond group 0 only when the tiny lane needs it.
   Required proof:
   - deterministic group selection
   - updated free-count accounting
   - reboot coverage with the larger allocation surface
   Current status:
   - deferred

## Executed 25-Step Batch

This was the next fully executed batch after rename and shrink-side truncate:

1. Audit the remaining adjacent ext2 gaps after shrink-truncate hardening.
2. Audit the kernel-side `lseek` path in [ring3.cpp](/home/eirikr/Github/XINIM/src/kernel/i486/ring3.cpp).
3. Audit the fd seek path in [bootfs.cpp](/home/eirikr/Github/XINIM/src/kernel/i486/bootfs.cpp).
4. Audit the i386 guest syscall surface in [syscall_i386.hpp](/home/eirikr/Github/XINIM/include/xinim/userland/syscall_i386.hpp).
5. Audit the existing persistence proof in [x86_32_persist_test.py](/home/eirikr/Github/XINIM/test/boot/x86_32_persist_test.py).
6. Confirm that newly allocated ext2 blocks are zeroed by allocation policy.
7. Choose a user-visible proof instead of a kernel-only assumption.
8. Add the missing i386 `lseek` wrapper.
9. Design a tiny native gap-write utility.
10. Implement `/bin/seekwrite`.
11. Design a tiny native zero-prefix verifier.
12. Implement `/bin/holecheck`.
13. Add both guest utilities to the i486 build graph.
14. Add both guest utilities to the staged ISO image.
15. Add both guest utilities to GRUB module staging.
16. Extend the image layout test for the new binaries.
17. Extend the shell smoke test for command visibility.
18. Expand persistence coverage for long-to-short rewrite trimming.
19. Add a dirty-block seed file scenario.
20. Reclaim that seed file to encourage deterministic block reuse.
21. Create a hole-style file with `seekwrite`.
22. Verify the zero-filled prefix with `holecheck` on first boot.
23. Verify the same zero-filled prefix again after reboot.
24. Rebuild the i486 ISO and ATA image.
25. Rerun the focused `pc-i440fx` and `isapc` QEMU matrix and fold the result back into docs.

Status:
- executed and green

## Next Hypergranular Batch

The next adjacent-file batch after the executed 25-step proof run is:

1. Decide whether `truncate` growth should become a first-class tiny-lane primitive.
2. If yes, choose a single syscall surface:
   - `truncate`
   - `ftruncate`
   - or both
3. Add syscall numbers only if the chosen surface is real.
4. Extend the i386 guest syscall wrapper header.
5. Extend the i486 syscall dispatcher in [ring3.cpp](/home/eirikr/Github/XINIM/src/kernel/i486/ring3.cpp).
6. Keep the bootfs fd layer as the single policy owner for file-size mutation.
7. Reuse the ext2 gap-zeroing helpers instead of re-implementing growth logic.
8. Decide whether growth beyond current allocated capacity is allowed.
9. If allowed, define the zero-fill rule for the full grown range.
10. If rejected, make the rejection explicit in docs and tests.
11. Add one tiny native growth utility rather than widening shell scripts first.
12. Stage that utility into the i486 image.
13. Add layout coverage for the staged binary.
14. Add shell visibility coverage.
15. Add first-boot functional coverage.
16. Add reboot-persistence coverage.
17. Add a failure-path test for impossible growth requests if supported.
18. Decide whether rename needs best-effort rollback.
19. If rollback stays deferred, state the exact failure-ordering contract.
20. Decide whether directory-target overwrite is worth implementing.
21. If not, keep it explicitly deferred.
22. Audit allocator assumptions for filesystems with more than one group.
23. Decide whether the i486 ATA image should remain single-group for now.
24. Keep docs and tests aligned with whichever path is chosen.
25. Only then widen the mutation surface again.

9. Add write-path design and staging, but only after read-only mount is green.
   Required proof:
   - a clear write policy for the tiny VFS
   - no accidental writes through a read-only path
   - a dedicated persistence test image or fixture plan
   Current status:
   - implemented for the first narrow slice:
     existing ext2 files with already-allocated blocks can be truncated and rewritten
   - verified with a reboot-persistence test using `/etc/persist-write-slot`
   - not yet implemented for file creation, directory mutation, or block allocation

10. Add reboot-persistence tests once write support exists.
   Required proof:
   - create or edit a file
   - reboot the same image
   - verify content survived and is read from disk, not from a boot module
   Current status:
   - implemented for the first edit-in-place slice on both `pc-i440fx` and `isapc`
