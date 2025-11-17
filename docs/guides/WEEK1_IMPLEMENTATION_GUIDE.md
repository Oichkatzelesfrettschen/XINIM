# Week 1 Implementation Guide: Cross-Compiler Bootstrap
**Phase:** Toolchain & libc (Week 1 of 4)
**Timeline:** 5 days
**Goal:** Build x86_64-xinim-elf cross-compiler toolchain

---

## Quick Start

```bash
# Clone XINIM repository
cd /home/user/XINIM

# Run setup script
chmod +x scripts/setup_build_environment.sh
sudo ./scripts/setup_build_environment.sh

# Source environment
source /opt/xinim-toolchain/xinim-env.sh

# Build toolchain (in order)
chmod +x scripts/build_*.sh
./scripts/build_binutils.sh
./scripts/build_gcc_stage1.sh
```

**Estimated Time:** 1-2 hours total

---

## Day-by-Day Breakdown

### Day 1-2: Environment Setup & binutils

#### Prerequisites

**System Requirements:**
- Ubuntu 20.04+ or Debian 11+
- 16 GB RAM (minimum 8 GB)
- 50 GB free disk space
- Internet connection for downloads

**Required Packages:**
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    gcc g++ \
    make cmake \
    wget curl \
    tar xz-utils gzip bzip2 \
    bison flex \
    texinfo help2man \
    gawk \
    libgmp-dev libmpfr-dev libmpc-dev \
    libc6-dev \
    git
```

#### Step 1: Run Setup Script

```bash
cd /home/user/XINIM
chmod +x scripts/setup_build_environment.sh
sudo ./scripts/setup_build_environment.sh
```

**What it does:**
1. Creates `/opt/xinim-toolchain` (toolchain installation)
2. Creates `/opt/xinim-sysroot` (system root for target)
3. Downloads binutils, GCC, musl sources (~500 MB)
4. Extracts sources to `$HOME/xinim-sources/`
5. Creates build directories in `$HOME/xinim-build/`
6. Generates environment script

**Output:**
```
======================================
XINIM Build Environment Summary
======================================
Toolchain prefix:  /opt/xinim-toolchain
Sysroot:           /opt/xinim-sysroot
Target:            x86_64-xinim-elf
...
Next steps:
  1. Source the environment:
     source /opt/xinim-toolchain/xinim-env.sh
  ...
======================================
```

#### Step 2: Source Environment

```bash
source /opt/xinim-toolchain/xinim-env.sh
```

**Verify:**
```bash
echo $XINIM_PREFIX  # Should be /opt/xinim-toolchain
echo $XINIM_TARGET  # Should be x86_64-xinim-elf
```

**Add to `.bashrc` (optional):**
```bash
echo 'source /opt/xinim-toolchain/xinim-env.sh' >> ~/.bashrc
```

#### Step 3: Build binutils

```bash
cd /home/user/XINIM
chmod +x scripts/build_binutils.sh
./scripts/build_binutils.sh
```

**Build Time:** ~5-10 minutes (with 8 cores)

**Output:**
```
[binutils] Configuring binutils 2.41 for x86_64-xinim-elf...
[binutils] Building binutils (using 8 parallel jobs)...
[binutils] Installing binutils to /opt/xinim-toolchain...
[binutils] Verifying installation...
[binutils]   ✓ as: GNU assembler (GNU Binutils) 2.41
[binutils]   ✓ ld: GNU ld (GNU Binutils) 2.41
[binutils]   ✓ ar: GNU ar (GNU Binutils) 2.41
...
================================================
binutils 2.41 build successful!
================================================
```

**Verification:**
```bash
# Check installed tools
ls -lh /opt/xinim-toolchain/bin/
# Should see: x86_64-xinim-elf-as, x86_64-xinim-elf-ld, etc.

# Test assembler
x86_64-xinim-elf-as --version
# Output: GNU assembler (GNU Binutils) 2.41

# Test linker
x86_64-xinim-elf-ld --version
# Output: GNU ld (GNU Binutils) 2.41
```

**Troubleshooting:**

| Issue | Solution |
|-------|----------|
| "configure: error: C compiler cannot create executables" | Install `build-essential`: `sudo apt-get install build-essential` |
| "makeinfo: command not found" | Install texinfo: `sudo apt-get install texinfo` |
| "Permission denied" for `/opt` | Run with sudo: `sudo ./build_binutils.sh` |

---

### Day 3-4: GCC Stage 1 (Bootstrap Compiler)

#### Step 4: Build GCC Stage 1

```bash
cd /home/user/XINIM
./scripts/build_gcc_stage1.sh
```

**Build Time:** ~20-30 minutes (with 8 cores)

**What is Stage 1?**
- Bootstrap compiler (no libc support)
- Can compile freestanding code only
- Used to build the kernel and musl libc
- Cannot use standard library functions (printf, malloc, etc.)

**Output:**
```
[gcc-stage1] Configuring GCC 13.2.0 Stage 1 for x86_64-xinim-elf...
[gcc-stage1] This is a bootstrap compiler without libc support
[gcc-stage1] Building GCC Stage 1 (using 8 parallel jobs)...
[gcc-stage1] This will take approximately 20-30 minutes...
[gcc-stage1] Installing GCC Stage 1 to /opt/xinim-toolchain...
[gcc-stage1] Verifying installation...
[gcc-stage1]   ✓ gcc: x86_64-xinim-elf-gcc (GCC) 13.2.0
[gcc-stage1]   ✓ g++: x86_64-xinim-elf-g++ (GCC) 13.2.0
[gcc-stage1]   ✓ Can compile freestanding C code
================================================
GCC 13.2.0 Stage 1 build successful!
================================================
```

**Verification:**
```bash
# Check compiler version
x86_64-xinim-elf-gcc --version
# Output: x86_64-xinim-elf-gcc (GCC) 13.2.0

# Test freestanding compilation
cat > /tmp/test.c <<'EOF'
void _start(void) {
    while(1) __asm__("hlt");
}
EOF

x86_64-xinim-elf-gcc -c /tmp/test.c -o /tmp/test.o
# Should succeed

x86_64-xinim-elf-objdump -d /tmp/test.o
# Should show disassembly

rm /tmp/test.c /tmp/test.o
```

**Test with XINIM Kernel:**
```bash
cd /home/user/XINIM

# Try compiling a kernel file (should work now)
x86_64-xinim-elf-g++ -c src/kernel/kernel.cpp -o /tmp/kernel.o \
    -std=c++23 -Iinclude -fno-exceptions -fno-rtti -mno-red-zone

# Check if object file created
ls -lh /tmp/kernel.o
# Should show ~XX KB object file

rm /tmp/kernel.o
```

**Troubleshooting:**

| Issue | Solution |
|-------|----------|
| "build/./gcc/xgcc: No such file or directory" | GMP/MPFR/MPC not found. Run `setup_build_environment.sh` again |
| "makeinfo: command not found" | Install texinfo: `sudo apt-get install texinfo help2man` |
| Build fails after long time | Check disk space: `df -h /opt`. Need ~10 GB free |

---

### Day 5: Verification & Documentation

#### Step 5: Verify Toolchain

```bash
# Run verification script
cd /home/user/XINIM
./scripts/verify_toolchain.sh
```

**Manual Verification:**

**1. Check all tools exist:**
```bash
TOOLS="as ld ar nm objcopy objdump ranlib readelf size strings strip gcc g++ cpp"

for tool in $TOOLS; do
    TOOL_PATH="/opt/xinim-toolchain/bin/x86_64-xinim-elf-$tool"
    if [ -x "$TOOL_PATH" ]; then
        echo "✓ $tool: $($TOOL_PATH --version | head -n1)"
    else
        echo "✗ $tool: NOT FOUND"
    fi
done
```

**2. Check disk usage:**
```bash
du -sh /opt/xinim-toolchain
# Expected: ~500-700 MB

du -sh $HOME/xinim-build
# Expected: ~2-3 GB (can delete after build)

du -sh $HOME/xinim-sources
# Expected: ~500 MB (keep for rebuilds)
```

**3. Test freestanding compilation:**
```bash
cat > /tmp/hello_kernel.c <<'EOF'
// Freestanding program (no libc)
void _start(void) {
    volatile char* video = (volatile char*)0xB8000;
    video[0] = 'H';
    video[1] = 0x0F;  // White on black
    while(1);
}
EOF

x86_64-xinim-elf-gcc -static -nostdlib -ffreestanding \
    -mno-red-zone -o /tmp/hello_kernel /tmp/hello_kernel.c

x86_64-xinim-elf-readelf -h /tmp/hello_kernel
# Should show: ELF 64-bit LSB executable, x86-64

rm /tmp/hello_kernel.c /tmp/hello_kernel
```

#### Step 6: Build XINIM Kernel (Test)

```bash
cd /home/user/XINIM

# Clean previous build
rm -rf build/

# Configure with new toolchain
export CC=x86_64-xinim-elf-gcc
export CXX=x86_64-xinim-elf-g++
export AR=x86_64-xinim-elf-ar

# Build with xmake
xmake f -c
xmake build xinim

# Check if kernel built
ls -lh build/xinim
# Should show kernel binary
```

**Expected Result:**
```
[ 25%]: compiling.cpp src/kernel/kernel.cpp
[ 50%]: compiling.cpp src/kernel/system.cpp
[ 75%]: linking.executable xinim
[100%]: build ok!
```

**If build fails:**
- Some files may need libc (that's OK, we'll fix in Week 2)
- Check error messages for missing syscalls
- Document which files need libc

#### Step 7: Document Progress

**Create status report:**
```bash
cat > ~/week1_status.md <<'EOF'
# Week 1 Status Report

## Completed
- ✅ Build environment setup
- ✅ binutils 2.41 installed
- ✅ GCC 13.2.0 Stage 1 installed
- ✅ Can compile freestanding code

## Toolchain Verification
- Toolchain: /opt/xinim-toolchain
- Sysroot: /opt/xinim-sysroot
- Target: x86_64-xinim-elf
- Binutils: 2.41
- GCC: 13.2.0 (Stage 1, no libc)

## Tools Installed
- x86_64-xinim-elf-as
- x86_64-xinim-elf-ld
- x86_64-xinim-elf-gcc
- x86_64-xinim-elf-g++
- x86_64-xinim-elf-ar
- x86_64-xinim-elf-nm
- x86_64-xinim-elf-objdump
- x86_64-xinim-elf-strip
- (+ 6 more tools)

## Disk Usage
- Toolchain: XXX MB
- Build directory: XXX GB
- Sources: XXX MB

## Next Steps
- Week 2: Build musl libc
- Week 3: Build GCC Stage 2 (full compiler)
- Week 4: Build mksh shell

## Issues
(List any issues encountered)

EOF

# Fill in disk usage
du -sh /opt/xinim-toolchain >> ~/week1_status.md
du -sh $HOME/xinim-build >> ~/week1_status.md
du -sh $HOME/xinim-sources >> ~/week1_status.md
```

---

## Week 1 Deliverables Checklist

### Required Deliverables
- [x] Build environment setup complete
- [x] binutils 2.41 installed and verified
- [x] GCC 13.2.0 Stage 1 installed and verified
- [x] Can compile freestanding C/C++ code
- [x] Environment script created (`xinim-env.sh`)
- [x] All tools accessible via PATH

### Optional Deliverables
- [ ] XINIM kernel compiles with new toolchain
- [ ] Cleans up build directory (save ~2 GB space)
- [ ] Documentation updated
- [ ] Week 1 status report created

---

## Common Issues & Solutions

### Issue: Out of Disk Space

**Symptom:**
```
No space left on device
```

**Solution:**
```bash
# Check disk usage
df -h

# Clean build directories (safe to delete)
rm -rf $HOME/xinim-build/*

# Clean GCC build directory only (saves ~1.5 GB)
rm -rf $HOME/xinim-build/gcc-stage1-build
```

### Issue: GCC Build Fails - Missing Dependencies

**Symptom:**
```
configure: error: Building GCC requires GMP 4.2+, MPFR 3.1.0+ and MPC 0.8.0+
```

**Solution:**
```bash
cd $HOME/xinim-sources/gcc-13.2.0
./contrib/download_prerequisites

# Or install system packages
sudo apt-get install libgmp-dev libmpfr-dev libmpc-dev
```

### Issue: Permission Denied on /opt

**Symptom:**
```
mkdir: cannot create directory '/opt/xinim-toolchain': Permission denied
```

**Solution:**
```bash
# Either run with sudo
sudo ./scripts/setup_build_environment.sh

# Or change ownership
sudo mkdir -p /opt/xinim-toolchain /opt/xinim-sysroot
sudo chown -R $USER:$USER /opt/xinim-toolchain /opt/xinim-sysroot
```

### Issue: Slow Compilation

**Symptom:**
Build takes > 1 hour

**Solution:**
```bash
# Check CPU cores
nproc
# Ensure using all cores

# Edit build scripts to use more jobs
# Change: make -j$(nproc)
# To: make -j16  (or your core count)

# Use ccache for faster rebuilds
sudo apt-get install ccache
export PATH="/usr/lib/ccache:$PATH"
```

---

## Performance Tips

### Build Time Optimization

**Use all CPU cores:**
```bash
# In build scripts, always use:
make -j$(nproc)

# Or explicitly:
make -j8  # For 8-core system
```

**Use faster filesystem:**
```bash
# If possible, build on SSD not HDD
# Or use tmpfs (RAM disk) for builds
mkdir /tmp/xinim-fast-build
export XINIM_BUILD_DIR=/tmp/xinim-fast-build
```

**Enable ccache:**
```bash
sudo apt-get install ccache
export CC="ccache gcc"
export CXX="ccache g++"

# Check ccache stats
ccache -s
```

### Disk Space Optimization

**Clean build directories after install:**
```bash
# binutils build (saves ~500 MB)
rm -rf $HOME/xinim-build/binutils-build

# GCC build (saves ~2 GB)
rm -rf $HOME/xinim-build/gcc-stage1-build

# Keep sources for potential rebuilds
# Don't delete: $HOME/xinim-sources/
```

**Use compressed sources:**
```bash
# After extraction, delete tarballs
rm $HOME/xinim-sources/*.tar.xz
rm $HOME/xinim-sources/*.tar.gz
# Saves ~500 MB
```

---

## Next Week Preview: Week 2

### Week 2 Goals
1. Build musl libc 1.2.4
2. Install to sysroot
3. Test compilation with libc
4. Fix XINIM syscall adapter

### Week 2 Commands
```bash
# Build musl
./scripts/build_musl.sh

# Verify libc
ls -lh /opt/xinim-sysroot/lib/libc.a

# Test with simple program
cat > test.c <<'EOF'
#include <stdio.h>
int main() {
    printf("Hello from musl!\n");
    return 0;
}
EOF

x86_64-xinim-elf-gcc --sysroot=/opt/xinim-sysroot -static test.c -o test
```

**Estimated Time:** 30 minutes

---

## Success Criteria

### Week 1 is successful if:

1. ✅ **binutils installed** - All 11 tools work
2. ✅ **GCC Stage 1 installed** - Can compile C and C++23
3. ✅ **Freestanding code compiles** - Test program works
4. ✅ **Environment script works** - Can source and use tools
5. ✅ **Documentation complete** - Status report written

### Verification Commands

```bash
# Quick verification
x86_64-xinim-elf-gcc --version | grep 13.2.0
x86_64-xinim-elf-ld --version | grep 2.41
echo | x86_64-xinim-elf-gcc -dM -E - | grep __x86_64__

# All should return 0 (success)
echo $?
```

---

## Resources

### Documentation
- GCC Installation Guide: https://gcc.gnu.org/install/
- binutils Documentation: https://sourceware.org/binutils/docs/
- OSDev Wiki: https://wiki.osdev.org/GCC_Cross-Compiler

### XINIM Documentation
- [Toolchain Specification](/home/user/XINIM/docs/specs/TOOLCHAIN_SPECIFICATION.md)
- [SUSv4 Compliance Audit](/home/user/XINIM/docs/SUSV4_POSIX_2017_COMPLIANCE_AUDIT.md)
- [Roadmap](/home/user/XINIM/docs/ROADMAP_SUSV4_COMPLIANCE.md)

### Support
- XINIM Issues: https://github.com/Oichkatzelesfrettschen/XINIM/issues
- OSDev Forums: https://forum.osdev.org/

---

## Appendix: Full Command Sequence

**Complete Week 1 in one go:**

```bash
#!/bin/bash
set -euo pipefail

# Week 1: Cross-Compiler Bootstrap
cd /home/user/XINIM

# Setup environment
sudo ./scripts/setup_build_environment.sh
source /opt/xinim-toolchain/xinim-env.sh

# Build binutils (~10 min)
./scripts/build_binutils.sh

# Build GCC Stage 1 (~30 min)
./scripts/build_gcc_stage1.sh

# Verify
x86_64-xinim-elf-gcc --version
x86_64-xinim-elf-ld --version

echo "Week 1 complete! Total time: ~40-50 minutes"
echo "Next: ./scripts/build_musl.sh (Week 2)"
```

**Save as:** `scripts/run_week1.sh`

---

**Guide Status:** ✅ COMPLETE
**Ready for:** Immediate implementation
**Next Guide:** Week 2 - musl libc Integration
