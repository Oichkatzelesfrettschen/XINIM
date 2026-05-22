#!/usr/bin/env bash
# Audit host tooling for the XINIM 32-bit QEMU/debug/fuzz lanes.

set -euo pipefail

tool_rows=(
  "build|cmake|cmake|CMake configure"
  "build|ninja|ninja|CMake backend"
  "build|clang|clang|C/C++ compiler"
  "build|clang-tidy|clang|static analyzer"
  "build|scan-build|clang|Clang static analyzer driver"
  "build|intercept-build|clang|compile database capture"
  "build|lld|lld|ELF linker"
  "build|llvm-objdump|llvm|disassembly"
  "build|llvm-readelf|llvm|ELF headers"
  "build|llvm-nm|llvm|symbols"
  "build|llvm-dwarfdump|llvm|DWARF inspection"
  "static|semgrep|semgrep|semantic static analysis"
  "static|cppcheck|cppcheck|C/C++ static analysis"
  "static|shellcheck|shellcheck|shell script lint"
  "static|shfmt|shfmt|shell formatter"
  "static|scanelf|pax-utils|ELF hardening scan"
  "static|dumpelf|pax-utils|ELF structural dump"
  "static|checksec|checksec|ELF security properties"
  "binary|gdb|gdb|debugger"
  "binary|rr|rr|record/replay for host repros"
  "binary|radare2|radare2|reverse engineering"
  "binary|rizin|rizin|reverse engineering"
  "binary|ghidra|ghidra|decompiler"
  "binary|retdec-decompiler|retdec|decompiler"
  "binary|binwalk|binwalk|container/firmware inspection"
  "binary|pahole|pahole|DWARF layout inspection"
  "runtime|qemu-system-i386|qemu-system-x86|32-bit system emulation"
  "runtime|qemu-img|qemu-img|image info/check/convert"
  "runtime|qemu-io|qemu-img|image IO probe"
  "runtime|qemu-nbd|qemu-img|NBD image export"
  "runtime|qemu-i386|qemu-user|host user-mode i386 checks"
  "runtime|strace|strace|host syscall trace"
  "runtime|ltrace|ltrace|host library-call trace"
  "runtime|perf|perf|host profiling"
  "runtime|bpftrace|bpftrace|host kernel probes"
  "runtime|valgrind|valgrind|host memory checks"
  "coverage|kcov|kcov|host coverage"
  "coverage|lcov|lcov|coverage reports"
  "coverage|gcovr|gcovr|coverage reports"
  "disk|fsck.ext2|e2fsprogs|ext2 consistency"
  "disk|debugfs|e2fsprogs|ext2 inspection"
  "disk|guestfish|libguestfs|guest disk inspection"
  "disk|guestmount|libguestfs|guest disk mount"
  "disk|virt-inspector|guestfs-tools|guest image inspection"
  "disk|mcopy|mtools|FAT/boot helper"
  "disk|xorriso|xorriso|ISO backend"
  "disk|grub-mkrescue|grub|GRUB rescue image helper"
  "fuzz|afl-fuzz|afl++|coverage-guided fuzzing"
  "fuzz|afl-cmin|afl++|corpus minimization"
  "fuzz|afl-collect|afl-utils|AFL crash collection"
  "fuzz|honggfuzz|honggfuzz|coverage-guided fuzzing"
  "fuzz|radamsa|radamsa|mutation fuzzing"
  "fuzz|syz-manager|syzkaller-git|Linux syscall fuzzer reference"
  "fuzz|trinity|trinity|Linux syscall stress reference"
  "emulator|bochs|bochs|alternate x86 emulator"
  "disk|nbdkit|nbdkit|scriptable NBD plugins"
  "disk|genext2fs|genext2fs|deterministic ext2 image creation"
  "disk|e2cp|e2tools|ext2 file injection"
  "reveng|frida|frida-tools|dynamic instrumentation"
  "reveng|angr|python-angr|symbolic/binary analysis"
)

printf "%-9s %-20s %-18s %-8s %s\n" "category" "tool" "package" "status" "purpose"
printf "%-9s %-20s %-18s %-8s %s\n" "--------" "----" "-------" "------" "-------"

missing=0
for row in "${tool_rows[@]}"; do
  IFS="|" read -r category tool package purpose <<<"$row"
  if command -v "$tool" >/dev/null 2>&1; then
    status="ok"
  else
    status="missing"
    missing=$((missing + 1))
  fi
  printf "%-9s %-20s %-18s %-8s %s\n" "$category" "$tool" "$package" "$status" "$purpose"
done

printf "\nPackage ownership for installed audit targets:\n"
for package in \
  qemu-system-x86 qemu-img qemu-user clang llvm lld gdb rr afl++ afl-utils \
  honggfuzz radamsa syzkaller-git trinity radare2 rizin ghidra retdec binwalk \
  strace ltrace valgrind bpftrace perf semgrep cppcheck shellcheck shfmt \
  libguestfs guestfs-tools pax-utils checksec e2fsprogs mtools xorriso grub \
  bochs nbdkit genext2fs e2tools
do
  if pacman -Q "$package" >/dev/null 2>&1; then
    pacman -Q "$package"
  fi
done

printf "\nMissing tools: %d\n" "$missing"
if (( missing > 0 )); then
  printf "Install candidates should be checked with: paru -Ss '^<package>$'\n"
fi
