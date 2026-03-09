# Next-11 Scope and Execution Log

Date: 2026-03-08
Request: "scope out the next 11 steps and continue"

## Scoped and Executed Steps

1. Audited lattice changer coverage gaps for 32-bit/64-bit parity.
2. Extended native passthrough mappings to include 32-bit lane support for core syscalls.
3. Added `getppid` coverage for native 64-bit and 32-bit lanes.
4. Added Linux64 mapping for `getppid` (`110`).
5. Added Linux32 mappings for `getppid` (`64`), `execve` (`11`), `exit` (`1`), and `wait4` (`114`).
6. Preserved existing Linux32 read/write/open/close/lseek/getpid/fork mappings.
7. Hardened prefix-fallback behavior to require nonzero `arg_shape` for any approximate transform.
8. Added ABI tests for Linux32 `execve`, `exit`, `wait4`, and native32 `getppid` passthrough.
9. Added ABI negative test to ensure unknown Linux32 syscall IDs do not remap via prefix fallback.
10. Extended kernel syscall-table tests for tagged Linux32 `exit` and `getppid` dispatch behavior.
11. Extended kernel syscall-table negative test coverage for unsupported Linux32 tagged calls.

## Notes

- Native fast path behavior is unchanged for untagged syscall IDs.
- Tagged compatibility translation remains explicit and opt-in.
- Prefix fallback is now gated to explicit shaped-call use only (nonzero `arg_shape`).
