# YANIX Source Index

Date: 2026-03-11

This index records the external `yanix` repository used for the Tier I x86
audit and design comparison work.

## Upstream

- Repository: `https://github.com/RobbeDGreef/yanix`
- Owner: `RobbeDGreef`
- Default branch: `master`
- Snapshot commit used for this audit:
  `f20a34e9d086404188eeade8ef2f094cbe732149`

## Cached Local Artifacts

- Repo metadata API snapshot:
  `data/external/yanix/repo_api.json`
- Upstream README snapshot:
  `data/external/yanix/README_master.md`
- Upstream TODO snapshot:
  `data/external/yanix/TODO_master.md`
- Machine-readable provenance:
  `data/external/yanix/PROVENANCE.json`

## Key Upstream Facts Used

- `yanix` describes itself as a UNIX-like OS built from scratch with a goal of
  strong POSIX alignment.
- The upstream README declares current support for `x86`, but not `x86-64`.
- The upstream tree layout includes:
  - `bootloader/`
  - `kernel/`
  - `libs/`
  - `sysroot/`
  - `system/`
  - `apps/`
  - `tools/`
- The upstream kernel tree includes dedicated areas for:
  - `kernel/drivers/ata`
  - `kernel/drivers/pci`
  - `kernel/fs/ext2`
  - `kernel/fs/vfs`
  - `kernel/proc/syscalls`
  - `kernel/arch/i386`

## Notes

- This source was used as a design donor and comparison target only.
- No upstream code was imported into XINIM as part of this audit.
- Any future port or translation into C++ should be selective and interface-led,
  not a line-by-line rewrite.
