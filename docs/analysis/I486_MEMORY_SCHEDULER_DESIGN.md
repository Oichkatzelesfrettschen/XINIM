# i486 process memory and scheduler design

The i486 boot repair exposes a separate design pressure: fixed process images
consume substantial RAM, while segmentation and scheduling require independent
correctness checks. The candidates below combine existing mechanisms from
small operating systems. The bounded process-backing candidate is now
implemented; paging and executable reclamation remain proposals. The
comparison describes engineering synthesis rather than a claim of invention.

## Evidence boundary

The source review uses XINIM commit
`739d7764e2a0c1419d66401e8cb2839c780faeac`. The active build investigation
reported `g_processes` at `0x003da1c0`, symbol size `0x02049600`, and kernel
load end `0x02481f40`. Those addresses belong to that build observation;
relinking can move them. Reproduce the symbol and load-boundary measurements
with `llvm-nm -S -n <kernel-elf>` and `llvm-readelf -lW <kernel-elf>`, retaining
the ELF hash and resolved compiler configuration with the output.

The source independently establishes eight process slots, each embedding a
4 MiB user image: `src/kernel/i486/ring3_internal.hpp`, `Process` and
`kMaxProcesses`; `src/kernel/i486/elf32_loader.hpp`, `kUserAddressSpaceSize`.
The images reserve 32 MiB before metadata and kernel stacks.
`process.cpp::allocate_process` clears the entire process object;
`ring3.cpp::sys_fork` copies the entire user image.

The observations establish linked storage and source behavior. Guest memory
pressure and isolation failures require their own executable probes.
The scheduler finding below includes a controlled guest reproduction.
DMA ownership repair and its boot results belong to the boot repair
evidence; successful boot alone leaves the memory-design questions open.

## Source-derived correctness residuals

**User segmentation admits offsets below the declared user range.**
`process.cpp::compute_segment_base` subtracts `0x00400000` from the process
image address. `hw_init.cpp::set_user_segment_base` installs ordinary
expand-up user segments whose inclusive offset limit is `0x007fffff`.
The segments therefore admit offsets below `elf32::kUserVirtualBase`, mapping
them to memory preceding the process image. `dma_pages.cpp` documents the
i486 identity-address assumption with paging disabled. Syscall pointer
validation covers kernel-mediated accesses; direct Ring 3 loads and stores
use the CPU protection mechanism.

A decisive probe runs a small Ring 3 binary that reads a controlled kernel
sentinel through an offset below `0x00400000`, records the fault or value,
and verifies the sentinel location from the linked image. A CPU protection
fault before the access would falsify the proposed exposure for that address
and configuration. A returned sentinel would demonstrate exposure. A repair
must also test the lowest and highest valid user addresses and neighboring
process storage.

**Quantum expiry requires an independent scan origin.**
The baseline `sched.cpp::i486_handle_timer_irq` cleared the preferred process
on expiry. `select_next_runnable(nullptr)` consequently began at slot zero,
and an equal-priority candidate retained the first matching slot.
`src/kernel/scheduler_policy.hpp::demote_priority` saturates ordinary
demotion at `PRIO_IDLE`, value 48. Two continuously runnable processes at that
priority therefore repeatedly selected the earliest slot.

The compiler reproduction confirmed starvation under QEMU instruction-count
timing: two runnable priority-48 tasks retained the earlier selected slot
over 88,821 observed timer ticks. Changing the support task's priority only
in that diagnostic guest released the child. Production selection now uses
`g_current_process` as the scan origin when the preferred process is absent.
The existing current-process pointer carries rotation state without another
global cursor. The production timer regression checks equal-priority
rotation, stronger-priority preemption, and wakeup selection.

## Bounded design candidates

1. Separate fixed process descriptors from admitted user storage. Reserve a
   descriptor, allocate its kernel stack and user backing, initialize the
   context, and publish runnable state only after every allocation succeeds.
   Release each acquired resource on failure. Keep the eight-slot limit as
   an independent policy. A pointer replacing an embedded array alone
   changes ownership but saves RAM only when backing allocation becomes
   demand-dependent; allocating eight full images reproduces the same cost.
2. Preserve the existing user addresses and syscall ABI while evaluating
   classic i486 two-level paging with 4 KiB pages. Supervisor mappings would
   protect kernel storage; user mappings would cover admitted executable,
   data, heap, and stack pages. Account for page-table memory and TLB work.
   Trace DMA physical addresses, syscall copies, ELF loading, fork, exec,
   faults, and teardown before implementation. PAE, NX, and later CPU
   facilities are outside this baseline. A smaller contiguous-image design
   remains a separate candidate only after resolving its protection and
   address-layout requirements.
3. Evaluate immutable executable backing after memory ownership is sound.
   Recoverable text can reduce writable residency and fork copying.
   Executable lifetime, mutation, corruption, and failed restoration need
   explicit contracts. Compression and swap add workspace and I/O costs;
   measure those costs before adopting either mechanism.

The first candidate now has a bounded implementation in
`src/kernel/i486/user_backing.cpp`. A process descriptor acquires a zeroed
4 MiB image before publication. `fork` copies into a separately owned image;
`exec` prepares a ninth candidate image and stack before replacing the live
image. Failed image validation preserves the previous image, mappings,
break, context, signal handlers, and file descriptors. Process destruction
releases the image slot and outstanding descriptor references. `fork` drops
the child's default console references before inheriting the parent's
descriptors. Released image slots are zeroed on reuse; the DMA bump
allocator retains their physical storage for the lifetime of the boot.
The eight-process policy therefore permits at most nine reserved images, or
36 MiB. `live_bytes()` reports occupied images; `reserved_bytes()` reports
the high-water reservation. Neither count is a physical peak-RAM measurement.

The rebuilt Debug kernel has 3,478,736 B of linked BSS and an eight-entry
`g_processes` array of `0x49680` B. The pre-memory-change build reported
37,033,024 B of BSS and `0x2049600` B for that array. The reduction in
linked BSS is approximately 32 MiB; each admitted image still consumes
4 MiB at runtime. The measured ELF SHA-256 is
`e39f8bb884aac46ca148f63f67fbc1fabfae1219eefbc2a93add67550bd8aa34`.
The allocation-exhaustion and zeroing unit test and the
64 MiB disk and ISO Ring 3 shell tests pass. Their serial logs report two
live images (8,388,608 B) and three reserved images (12,582,912 B) after
service initialization. The disk tests execute an invalid ELF from
`/persist`, then execute `/bin/hello` in the same shell. The final full
i486 selector passes all 31 CTest entries in 808.15 seconds; its output is
retained at `build/i486/Debug/evidence/i486-user-backing-final-ctest.log`.

The segmentation exposure described above remains open. Neither the backing
allocator nor the guest shell tests demonstrate supervisor isolation.
Classic 4 KiB i486 paging and immutable executable backing remain design
candidates with the stated proof obligations.

## Primary-source comparisons and reuse boundaries

- `Oichkatzelesfrettschen/xv6`,
  `fa7208ce8ed31932a8738fbbecd46e29f55a3c6c`:
  `kernel/proc.c::allocproc` reserves an embryo and allocates its stack;
  `kernel/vm.c::allocuvm` allocates pages and unwinds failure; `copyuvm`
  copies the admitted image. `LICENSE` grants MIT-style permission with
  notice retention. The fork contains architecture conditionals; inspect
  the 32-bit path and its CPU assumptions before reuse.
- `Oichkatzelesfrettschen/FUZIX`,
  `776bfc942c0ce3f5b86e4c48848d154a60ff1cb6`:
  `Kernel/process.c::ptab_alloc` publishes `P_FORKING` after
  `pagemap_alloc`; `getproc` retains `getproc_nextp`.
  `CONFIG_PARENT_FIRST` documents avoiding swap churn between fork and
  wait. `LICENCE` carries GPLv2 and identifies LGPL libraries and
  separately licensed files. Use these mechanisms as design evidence;
  copying requires a file-specific license decision. Banking and swap
  assumptions differ from an i486 MMU.
- DiscoBSD source at local `../discobsd-2040-unofficial`,
  `a23a2f4348290e775d42c18f7c8b95e4ff228c0f`:
  `sys/kern/vm_swap.c::swaptext` excludes clean executable text; `swapin`
  restores it from the executable; `swapout` reserves compressed storage
  before committing. Root `LICENSE` is BSD-3-Clause; historical source
  files retain their own Berkeley notices. RP2040 fixed-window, codec,
  and flash assumptions require independent adaptation.
- Local `../BSD_Workspace/211bsd`,
  `0a38e59d742ff4371b78ea65c5bcc50477b31708`:
  `sys/sys/vm_sched.c::sched` separates runnable eligibility from loaded
  residency and evaluates swap candidates. Historical file notices refer
  to the Berkeley software license agreement; review the relevant
  distribution terms before copying. PDP-11 segmentation and swap policy
  provide comparison evidence rather than an i486 implementation.

XINIM-owned implementations and interfaces use C++23. Donor provenance and
file-specific notices remain intact when copying is justified. Architecture
ideas require explicit ABI and ownership contracts even when licensing
permits source reuse.
