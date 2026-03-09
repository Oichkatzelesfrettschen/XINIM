# XINIM VFS Low-RAM RCA And Synthesis

Date: 2026-03-08

## Scope

This note resumes two threads at once:

- the current x86_64 COM2 emergency-shell to staged-`xash` handoff work, and
- the broader low-RAM VFS redesign request, using 4.4BSD-Lite/FreeBSD,
  MidnightBSD lineage, and MINIX 3 as reference points.

The companion source index is
`docs/external_sources/VFS_LOW_RAM_SOURCES.md`.

## Current Local State

### What was actually failing on x86_64

The immediate x86_64 handoff failure was not the bare-metal VFS itself.

The staged shell banner printed, but the first `xash$ ` prompt never appeared.
Root cause: the freestanding `xinim` kernel target only disabled
stack-protector at link time, not compile time. Helper functions inside
`src/kernel/x86_64/staged_xash.cpp` still carried stack-protector
instrumentation, and the shell wedged in `init_environment()` before the first
prompt.

Local fix implemented in this pass:

- add compile-time `-fno-stack-protector` to the `xinim` kernel target
- add compile-time `-fno-stack-protector` to `xinim_x86_64_shell`
- keep the staged shell on a pseudo-filesystem path so the COM2 handoff remains
  stable while the fuller VFS lane is redesigned

Verification:

- `cmake --preset x86_64-debug`
- `cmake --build --preset x86_64-debug --target xinim_x86_64_image`
- `python3 test/boot/x86_64_shell_test.py`

The x86_64 COM2 lane now reaches the staged `xash` prompt and exercises
`help`, `ls`, `cat`, `cp`, `env`, `command -v`, and the rescue-shell roundtrip.

First low-RAM implementation slice now landed:

- `XINIM_VFS_PROFILE=auto|default|tiny` is a real CMake knob
- `auto` now resolves to `tiny` on x86_32 lanes and `default` on x86_64
- the `tiny` profile reduces the static bare-metal VFS tables
- the `tiny` profile disables the boot-time buffer-cache path
- a fresh `tiny` x86_64 image now builds and passes the same COM2 shell test
- i486, i586, and i686 now build with the compact default and pass the Ring 3
  `xash` shell test suite
- the compact VFS now has a bounded reclaimable vnode-handle cache
- directory storage now chains additional fixed-size blocks instead of hard
  failing at 32 entries
- shared `bootfs` contents can now be promoted into the compact VFS core by a
  common bridge instead of staying a permanent side lane

Important clarification for parity:

- the 32-bit lanes already had a separate `bootfs` namespace and do not depend
  on the bare-metal VFS for their first shell handoff
- that existing `bootfs` path is now part of the low-RAM story instead of being
  treated as a side lane
- `bootfs::find()` now uses its own bounded positive/negative lookup cache so
  the constrained path is cheap where 32-bit actually lives today

### How XINIM stores VFS metadata today

The current bare-metal VFS is a fully static ramfs-style design centered around
`src/vfs/bare_vfs.hpp`.

Key properties:

- inode table: `MAX_INODES = 1024`
- dirent arena: `MAX_DIRENTS = 8192`
- data arena: `DATA_ARENA_SIZE = 1 MiB`
- buffer cache: `CACHE_BLOCKS = 64`, `CACHE_BLK_SIZE = 512`
- mount table: `MAX_MOUNTS = 8`
- fd table: `MAX_FDS = 64`

Important implementation details:

- `RawInode` is documented as one-cache-line oriented, but the current layout is
  actually 80 bytes in `src/vfs/inode_table.cpp`.
- directory entries are fixed 32-byte records with 26-byte max names in
  `src/vfs/dirent.cpp`.
- each directory currently gets one fixed 32-entry dirent block
  (`DIRENT_BLOCK_SIZE = 32`).
- pathname lookup in `src/vfs/path_walk.cpp` is zero-allocation and simple, but
  fully linear through each directory block and has no positive or negative
  name cache.
- file payloads smaller than `sizeof(RawInode::inline_data)` stay inline;
  larger files consume the global data arena through a simple bump allocator
  plus a small free-list.

### Why this is too expensive for a 4 MiB target

The comments in `bare_vfs.hpp` estimate about 1.35 MiB of resident VFS memory.
That estimate is directionally correct, but the true inode cost is slightly
higher because `RawInode` is currently 80 bytes, not 64.

Rough fixed cost before meaningful userland:

- inodes: about 80 KiB
- dirents: 256 KiB
- data arena: 1024 KiB
- buffer cache: about 33 KiB
- mount/fd tables and bitmaps: small

That is already around 1.35 MiB to 1.40 MiB of always-resident VFS state.
On a 4 MiB system, that is too much of the machine budget for a boot-time ramfs,
especially once kernel text/data, page tables, stacks, and shell/userland state
are included.

## Current RCA For The Real VFS Lane

### Strengths in the current design

- zero-allocation path walk is cheap and deterministic
- inline file storage avoids extra allocation for tiny files
- fixed arrays are easy to reason about during bring-up
- no dynamic allocator is required to mount a tiny ramfs

### Current blockers and risks

- `vfs_seed_file()` in `src/vfs/vfs_server.cpp` still uses synchronous path
  walking, inode allocation, dirent insertion, and payload writes during early
  bring-up
- the boot-time VFS path and the steady-state VFS path are not cleanly split
- directory capacity is hard-capped at 32 entries per directory block
- there is no name cache, so repeated path resolution is directory-scan heavy
- there is no reclaimable vnode-like metadata tier; the global tables are fully
  resident whether or not the workload needs them
- the buffer cache is allocated even for a pure ramfs boot path where it buys
  little and costs memory
- the current design uses one global 1 MiB data arena even when the lane only
  needs a few tiny boot files

## External Synthesis

### 4.4BSD-Lite / FreeBSD lineage

The 4.4BSD VFS model centers on the vnode as a mount-independent object with an
operation vector. That is the big architectural lesson to keep: separate the
common lookup/open/stat namespace logic from the backend filesystem logic.

The same lineage also relies on a directory name lookup cache. Later FreeBSD
code keeps bounded vnode/namecache structures and reclaims inactive entries
instead of making every filesystem object permanently resident.

What matters for XINIM:

- keep a small common namespace layer
- make cached names bounded and reclaimable
- keep backend filesystem code behind a very small ops table
- do not let the boot filesystem force all future metadata to stay resident

### MidnightBSD

MidnightBSD's official about page states that it is based on FreeBSD 6.1 Beta.
Direct VFS design prose is sparse, so the right reading is an explicit
inference: MidnightBSD inherits the classic FreeBSD vnode/namecache VFS shape
closely enough that the FreeBSD lineage is the right implementation proxy here.

The useful takeaway is not novelty, but conservatism:

- small vnode-like objects
- a bounded cache, not a giant static namespace image
- reuse of common VFS lookup/open/stat machinery across filesystems

### MINIX 3

MINIX 3 takes a different path: VFS is its own server. The official VFS
internals documentation describes a main thread that receives requests and
worker threads that process them, with global per-process state in `fproc` and
the actual filesystem backends split from the VFS mediation layer.

What matters for XINIM:

- separate policy from storage
- keep global metadata compact
- keep per-process open-file/path state explicit and small
- do not require the full filesystem implementation to be present before basic
  shell/control flow works

MINIX pays IPC costs for that split, but on a low-RAM machine it wins by
keeping the always-resident control structures smaller and more explicit.

## Recommended XINIM Synthesis

### 1. Split bootfs from the real VFS

Keep the current staged-shell pseudo-filesystem idea, but make it formal.

Proposed layers:

- Stage 0 `bootfs`: tiny static table for `/bin`, `/etc`, `/tmp`, boot modules,
  and serial-shell survival
- Stage 1 compact VFS core: lookup, fd table, mount table, small name cache,
  vnode-like metadata handles
- Stage 2 backend filesystems: ramfs first, block-backed filesystems later

This removes the current failure mode where one early-boot VFS path can block
the entire shell/control lane.

### 2. Replace permanent large arrays with bounded compact caches

For the 4 MiB target, the resident metadata budget should be cut aggressively.

Suggested tiny profile:

- inodes/vnodes: 256 to 384 active metadata slots
- dirents: 1024 to 2048 entries, not 8192
- no ramfs buffer cache at boot
- ramfs data arena: start at 128 KiB to 256 KiB, not 1 MiB
- small positive/negative name cache: 64 to 128 entries

This keeps the low-memory lane centered on the working set instead of the worst
case.

### 3. Keep zero-allocation path walk, but add a tiny negative cache

The current path walker is already suitable for small machines. Keep the
zero-allocation slice model, but add a bounded cache for recent name lookups.

A good first step is a tiny ring or direct-mapped cache keyed by:

- parent inode
- name hash
- hit type: positive or negative
- child inode when positive

Why this matters:

- repeated shell lookups like `/bin/xash`, `/bin/sh`, `/tmp/foo` get cheaper
- failed lookups stop rescanning the same directory block every time
- the memory cost is tiny compared to the current global arenas

### 4. Make directory storage chainable instead of fixed at 32 entries

The current 32-entry fixed directory block is easy to boot, but brittle.
Replace the "one directory == one fixed block" model with:

- one small inline/primary block for small directories
- optional chained extension blocks only when needed

That preserves the small-directory fast path while avoiding waste and hard
capacity cliffs.

### 5. Separate ramfs data storage policy from VFS metadata policy

The current design ties metadata scale and data scale together too early.

Recommended change:

- VFS metadata stays compact and reclaimable
- ramfs data pages/blocks grow separately
- buffer cache is enabled only for block-backed filesystems, not for the boot
  ramfs path

This is closer to the BSD separation between namespace/vnode state and backing
storage behavior, while still fitting XINIM's freestanding constraints.

## Concrete Next Steps

### Immediate

- keep the staged x86_64 shell on the pseudo-filesystem path until the real VFS
  seed path is reintroduced safely
- remove temporary `early_serial` debug prints from `vfs_seed_file()` once the
  deeper VFS bring-up pass starts
- keep the COM2 rescue shell as the invariant control path

### Near-term implementation

- keep `XINIM_VFS_PROFILE=auto|default|tiny` as the common low-memory lane
  contract, with `auto` favoring x86_32 constraints by default
- keep `MAX_INODES`, `MAX_DIRENTS`, `DATA_ARENA_SIZE`, and cache residency
  profile-dependent
- keep the buffer cache disabled for boot ramfs / tiny profile until a
  block-backed filesystem needs it
- grow the bounded name-cache model further in both bare-metal VFS and `bootfs`
- keep the shared `bootfs` namespace common across x86_32 and x86_64 while the
  richer VFS promotion path is still being built out
- use the shared `bootfs` promotion bridge to seed the compact VFS early while
  still keeping the direct bootfs shell path as the control-path fallback
- keep the low-RAM lane dependency-light; no extra Conan runtime packages are
  warranted for this freestanding path today

### Follow-on implementation

- add reclaimable vnode-like handles above raw inode storage
- change directories from one fixed 32-slot block to small primary blocks plus
  optional extensions
- carry the same bootfs-to-real-VFS promotion model into i486, i586, i686, and
  x86_64 so shell behavior stays consistent across lanes

## Bottom Line

The current XINIM VFS is simple and boot-friendly, but it is too static and too
large for a serious 4 MiB target. The best synthesis is:

- MINIX-style separation for boot/control-path robustness
- BSD-style bounded cached namespace objects
- XINIM-specific fixed-layout, no-heap freestanding data structures

In practical terms: keep a tiny always-live bootfs for shell/control, then
bring up a compact reclaimable VFS core, and only then grow into richer ramfs
or block-backed behavior when the lane can afford it.
