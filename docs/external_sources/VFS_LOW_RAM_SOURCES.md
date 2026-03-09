# VFS Low-RAM Source Index

Retrieved: 2026-03-08

Purpose: primary sources used for the low-RAM VFS RCA and synthesis in
`docs/analysis/VFS_LOW_RAM_RCA.md`.

## FreeBSD / 4.4BSD lineage

- The Design and Implementation of the 4.4BSD Operating System, VFS chapter:
  https://docs.freebsd.org/en/books/design-44bsd/
  Relevance: canonical vnode/VFS abstraction, mount-independent vnode ops, and
  the directory name lookup cache model carried forward in the BSD lineage.

- FreeBSD `vfs_cache.c`:
  https://cgit.freebsd.org/src/tree/sys/kern/vfs_cache.c
  Relevance: modern FreeBSD lineage reference for bounded name-cache behavior,
  negative cache entries, and vnode cache policy.

- FreeBSD `vfs_subr.c`:
  https://cgit.freebsd.org/src/tree/sys/kern/vfs_subr.c
  Relevance: vnode lifecycle, allocation, reclaim, and global cache-management
  patterns that inform a reclaimable low-RAM metadata layer.

## MidnightBSD lineage

- MidnightBSD About page:
  https://www.midnightbsd.org/about/
  Relevance: establishes that MidnightBSD is derived from FreeBSD 6.1 Beta,
  which justifies using the FreeBSD VFS lineage as the implementation proxy for
  MidnightBSD where direct VFS design notes are sparse.

## MINIX 3

- MINIX 3 VFS internals wiki:
  https://wiki.minix3.org/doku.php?id=developersguide:vfsinternals
  Relevance: VFS-server structure, request flow, worker model, and per-process
  `fproc` state for a small-memory multiserver design.

- MINIX VFS server source tree:
  https://github.com/Stichting-MINIX-Research-Foundation/minix/tree/master/minix/servers/vfs
  Relevance: concrete implementation reference for the split VFS server,
  request handlers, and low-footprint pathname/open-file bookkeeping.
