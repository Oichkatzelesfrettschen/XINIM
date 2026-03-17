# i486 Ext2 Mutation Design

Date: 2026-03-09

This document records the current ext2 mutation design for the freestanding
i486 kernel lane. It is intentionally narrow: it describes what is implemented
now, what invariants that implementation relies on, and which steps come next.

## Scope

The current mutation layer lives in
[ext2_reader.cpp](/home/eirikr/Github/XINIM/src/kernel/i486/ext2_reader.cpp)
and is exposed through:

- [bootfs.cpp](/home/eirikr/Github/XINIM/src/kernel/i486/bootfs.cpp)
- [ring3.cpp](/home/eirikr/Github/XINIM/src/kernel/i486/ring3.cpp)
- [syscall_i386.hpp](/home/eirikr/Github/XINIM/include/xinim/userland/syscall_i386.hpp)

It currently supports:

- file creation
- directory creation
- same-directory rename for files and directories
- cross-directory rename for regular files
- cross-directory rename for non-root directories
- file-over-file destination replacement during rename
- shrink-side truncate with block reclamation and tail zeroing
- zero-filled growth proof through `lseek` + write guest probes
- zero-filled gap extension for existing files through `lseek` + write guest probes
- file unlink
- empty-directory removal
- block allocation on write for newly created files
- reboot-persistent ext2 mutation on the ATA-backed root image

## Design Constraints

- Freestanding only:
  - no hosted STL
  - no exceptions
  - fixed-capacity static buffers
- Small-kernel-first:
  - mutate group 0 only
  - prefer simple predictable allocation over full ext2 placement policy
- QEMU 486-first:
  - primary target is the deterministic ATA ext2 image used by the i486 lane

## Donor Synthesis

The implementation shape is a synthesis of:

- repo-local hosted ext2 logic in [ext2.cpp](/home/eirikr/Github/XINIM/src/vfs/ext2.cpp)
  - bitmap scanning
  - dirent splitting and coalescing
- Linux ext2 donor logic in `~/Playground/Hardware/netgear/openwrt-build/.../fs/ext2`
  - rename ordering
  - `..` rewrite for moved directories
  - parent link-count updates for cross-parent directory moves
- `e2fsprogs` donor logic in `~/Playground/Hardware/netgear/openwrt-build/...`
  - `mkdir.c`
  - `link.c`
  - `unlink.c`
  - `newdir.c`
- compact kernel sequencing from ELKS `namei.c` as a contrast case, not as the
  rename model

The key decision was to reuse the ext2 on-disk rules while refusing the hosted
runtime shape. The freestanding kernel keeps the algorithmic ideas, not the
allocation model or container model.

## Current On-Disk Strategy

### Superblock and group descriptors

- The kernel loads the ext2 superblock and the first group descriptor.
- Mutation updates:
  - superblock free inode count
  - superblock free block count
  - group-0 free inode count
  - group-0 free block count
  - group-0 used directory count

### Allocation policy

- Only group 0 is used for new inode and block allocation.
- Allocation is first-fit by bitmap scan.
- This is acceptable for the current tiny image because:
  - the filesystem is small
  - the current disk image is deterministic
  - predictability matters more than placement quality

### File blocks

- Direct blocks are supported.
- Single-indirect blocks are supported.
- Double and triple indirect blocks are not implemented.

### Directory entries

- Directory insertion uses classic ext2 dirent splitting:
  - find the last live dirent with spare `rec_len`
  - shrink it to its actual aligned length
  - insert the new dirent in the remainder
- If no space exists in current blocks:
  - allocate a new directory block
  - make one dirent consume that whole block
- Directory removal uses classic ext2 coalescing:
  - if the victim is not first in block, merge its `rec_len` into the previous entry
  - otherwise zero the victim inode field

## Current Semantic Contract

### `create`

- Creates a regular file inode.
- Adds a directory entry in the parent.
- Initial file has size 0 and no blocks.
- Blocks are allocated later on write.

### `mkdir`

- Creates a directory inode.
- Allocates the first directory block immediately.
- Emits `.` and `..` entries.
- Adds a parent directory entry.
- Increments parent link count.

### `unlink`

- Refuses directories.
- Removes the parent directory entry.
- Frees direct blocks and single-indirect children.
- Frees the inode bitmap slot.

### `rmdir`

- Refuses `/`.
- Requires the target to be a directory.
- Requires the directory to be empty except for `.` and `..`.
- Removes the parent directory entry.
- Frees the directory block and inode.
- Decrements parent link count.

### `rename`

- Refuses `/`.
- Refuses destination replacement.
- Same-parent rename edits the existing dirent in place when the new name fits.
- Existing regular-file destinations can be replaced by regular-file sources.
- Cross-parent regular-file rename:
  - adds the destination dirent first
  - removes the source dirent second
  - frees the replaced inode and its blocks after the source dirent is removed
- Cross-parent directory rename:
  - rejects moves into the directory's own subtree
  - adds the destination dirent first
  - increments the new parent link count
  - removes the source dirent
  - rewrites the moved directory's `..` entry to the new parent
  - decrements the old parent link count
- The current implementation is still narrower than full POSIX rename:
  - no target-directory overwrite
  - no directory-over-directory replacement
  - no crash-atomic guarantee beyond donor-inspired write ordering

### `truncate`

- The current truncate path is shrink-only.
- Shrinking a file:
  - zeroes the surviving tail fragment in the last kept block
  - frees direct blocks beyond the new EOF
  - frees single-indirect data blocks beyond the new EOF
  - drops the single-indirect table itself when it becomes empty
  - updates inode size and accounted block usage
- This is the path used by `O_TRUNC` through the tiny i486 `writefile` utility
  and the bootfs ext2 open/write path.

### `lseek`-Driven Growth Proof

- The kernel already supports `lseek` through the tiny bootfs fd layer.
- The i486 guest lane now ships two native proof utilities:
  - `/bin/seekwrite`
  - `/bin/holecheck`
- It also now ships two companion utilities for extending existing files:
  - `/bin/seekpatch`
  - `/bin/gapcheck`
- They are used to prove that a write after `lseek(SEEK_SET)` over a new file:
  - preserves a zero-filled prefix
  - produces the expected file size
  - survives reboot without stale-byte leakage
- They are also used to prove that extending an existing file past EOF:
  - preserves the original head bytes
  - zero-fills the synthesized gap
  - writes the tail at the requested offset
  - survives reboot without stale-byte leakage

## Why File And Directory Removal Are Split

The kernel now treats these as separate contracts on purpose:

- `unlink` is for regular files
- `rmdir` is for empty directories

This keeps user-visible semantics closer to POSIX and prevents later
`rename`/`link`/permission work from being built on a muddy mutation model.

## Current Tests

The mutation layer is proven through:

- [x86_32_ext2_mutation_test.py](/home/eirikr/Github/XINIM/test/boot/x86_32_ext2_mutation_test.py)
  - create directory
  - create file
  - same-directory rename
  - cross-directory regular-file rename
  - file-over-file destination replacement
  - cross-directory non-empty directory rename
  - shrink rewrite proving stale-tail removal across reboot
  - reboot proof
  - reject non-empty `rmdir`
  - unlink moved files
  - remove moved empty directory
  - remove emptied parent directories
  - reboot proof again
- [x86_32_persist_test.py](/home/eirikr/Github/XINIM/test/boot/x86_32_persist_test.py)
  - file rewrite persistence
  - short rewrite after long content proves stale-tail removal
  - `seekwrite` plus `holecheck` prove zero-filled prefix growth across reboot
  - `seekpatch` plus `gapcheck` prove zero-filled existing-file extension across reboot
- [x86_32_shell_test.py](/home/eirikr/Github/XINIM/test/boot/x86_32_shell_test.py)
  - command visibility and shell-level smoke coverage
  - `seekwrite`, `holecheck`, `seekpatch`, and `gapcheck` command visibility

## Known Limits

- only group 0 allocation
- no overwrite of existing target directories during `rename`
- no directory-over-directory replacement during `rename`
- no hard links
- no `chmod` or `chown`
- no truncate growth support
- no sparse-file or hole-punch semantics
- no directory compaction beyond standard ext2 `rec_len` coalescing
- no crash-consistency story beyond write ordering
- no fsck integration

## Next Staircase

1. Decide whether cross-parent directory rename needs explicit best-effort rollback.
2. Add overwrite semantics for directory targets only if the tiny lane genuinely needs them.
3. Decide whether truncate growth should stay unsupported or gain a tiny zero-fill model.
4. Add explicit metadata mutation support where needed.
5. Add hard-link support only if a real userland need appears.
6. Move from group-0-only allocation to group-aware placement once the tiny lane is stable.
