# Build Graph Audit

Repository root: `/home/eirikr/Github/XINIM`

This report scans `CMakeLists.txt` and `cmake/*.cmake` for first-party
source references and compares that set to on-disk sources under `src`,
`test`, and `userland`.

## Summary

- referenced first-party sources: `228`
- unwired first-party sources: `145`
- unwired `src` sources: `69`
- unwired `test` sources: `76`
- unwired `userland` sources: `0`

## src

- unwired files: `69`
- largest unwired groups:
  - `src/fs`: `23`
  - `src/tools`: `13`
  - `src/vfs`: `8`
  - `src/crypto`: `5`
  - `src/servers`: `4`
  - `src/block`: `3`
  - `src/common`: `3`
  - `src/drivers`: `2`
  - `src/kernel`: `2`
  - `src/mm`: `2`
  - `src/boot`: `1`
  - `src/hal`: `1`
- sample unwired files:
  - `src/block/ahci_blockdev.cpp`
  - `src/block/blockdev.cpp`
  - `src/block/partition.cpp`
  - `src/boot/limine/limine_boot.cpp`
  - `src/common/math/octonion.cpp`
  - `src/common/math/quaternion.cpp`
  - `src/common/math/sedenion.cpp`
  - `src/crypto/crypto.cpp`
  - `src/crypto/kyber.cpp`
  - `src/crypto/kyber_cpp23_simd.cpp`
  - `src/crypto/pqcrypto_shared.cpp`
  - `src/crypto/vendored_sodium/randombytes_kernel.cpp`
- sample groups with examples:
  - `src/fs`: `src/fs/cache.cpp`, `src/fs/compat.cpp`, `src/fs/device.cpp`, `src/fs/filedes.cpp`, `src/fs/filesystem.cpp`
  - `src/tools`: `src/tools/bootblok1.cpp`, `src/tools/build.cpp`, `src/tools/c86/dos2out.cpp`, `src/tools/c86/stub.cpp`, `src/tools/diskio.cpp`
  - `src/vfs`: `src/vfs/ext2.cpp`, `src/vfs/filesystem.cpp`, `src/vfs/fs_init.cpp`, `src/vfs/mount.cpp`, `src/vfs/tmpfs.cpp`
  - `src/crypto`: `src/crypto/crypto.cpp`, `src/crypto/kyber.cpp`, `src/crypto/kyber_cpp23_simd.cpp`, `src/crypto/pqcrypto_shared.cpp`, `src/crypto/vendored_sodium/randombytes_kernel.cpp`
  - `src/servers`: `src/servers/memory_manager/main.cpp`, `src/servers/process_manager/main.cpp`, `src/servers/reincarnation_server.cpp`, `src/servers/vfs_server/main.cpp`
  - `src/block`: `src/block/ahci_blockdev.cpp`, `src/block/blockdev.cpp`, `src/block/partition.cpp`
  - `src/common`: `src/common/math/octonion.cpp`, `src/common/math/quaternion.cpp`, `src/common/math/sedenion.cpp`
  - `src/drivers`: `src/drivers/block/ahci.cpp`, `src/drivers/net/e1000.cpp`
  - `src/kernel`: `src/kernel/minix/boot.S`, `src/kernel/wormhole.cpp`
  - `src/mm`: `src/mm/putc.cpp`, `src/mm/syscall.cpp`
  - `src/boot`: `src/boot/limine/limine_boot.cpp`
  - `src/hal`: `src/hal/x86_64/hal/cpu_x86_64.cpp`

## test

- unwired files: `76`
- largest unwired groups:
  - `test/crypto`: `3`
  - `test/bench`: `1`
  - `test/chaos`: `1`
  - `test/contract`: `1`
  - `test/ipc_basic_test.cpp`: `1`
  - `test/property`: `1`
  - `test/randombytes_stub.cpp`: `1`
  - `test/signal`: `1`
  - `test/sodium_stub.cpp`: `1`
  - `test/t10a.cpp`: `1`
  - `test/t11a.cpp`: `1`
  - `test/t11b.cpp`: `1`
- sample unwired files:
  - `test/bench/lock_benchmark.cpp`
  - `test/chaos/chaos_resilience.cpp`
  - `test/contract/service_contract_invariants.cpp`
  - `test/crypto/test_constant_time_equal.cpp`
  - `test/crypto/test_kyber.cpp`
  - `test/crypto/test_shared_secret_failure.cpp`
  - `test/ipc_basic_test.cpp`
  - `test/property/vector_reverse_property.cpp`
  - `test/randombytes_stub.cpp`
  - `test/signal/test_signal_comprehensive.cpp`
  - `test/sodium_stub.cpp`
  - `test/t10a.cpp`
- sample groups with examples:
  - `test/crypto`: `test/crypto/test_constant_time_equal.cpp`, `test/crypto/test_kyber.cpp`, `test/crypto/test_shared_secret_failure.cpp`
  - `test/bench`: `test/bench/lock_benchmark.cpp`
  - `test/chaos`: `test/chaos/chaos_resilience.cpp`
  - `test/contract`: `test/contract/service_contract_invariants.cpp`
  - `test/ipc_basic_test.cpp`: `test/ipc_basic_test.cpp`
  - `test/property`: `test/property/vector_reverse_property.cpp`
  - `test/randombytes_stub.cpp`: `test/randombytes_stub.cpp`
  - `test/signal`: `test/signal/test_signal_comprehensive.cpp`
  - `test/sodium_stub.cpp`: `test/sodium_stub.cpp`
  - `test/t10a.cpp`: `test/t10a.cpp`
  - `test/t11a.cpp`: `test/t11a.cpp`
  - `test/t11b.cpp`: `test/t11b.cpp`

## userland

- unwired files: `0`
- status: fully referenced by the current CMake graph
