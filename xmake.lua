-- XINIM: Modern C++23 Post-Quantum Microkernel Operating System
-- Complete xmake Build Configuration

set_project("XINIM")
set_version("1.0.0")
set_languages("c17", "cxx23")

-- Basic configuration
add_rules("mode.debug", "mode.release")
set_warnings("all", "error")
set_policy("check.auto_ignore_flags", false)

-- x86_64 architecture configuration
add_defines("__XINIM_X86_64__")
add_cxflags("-march=x86-64", "-mtune=generic")

-- POSIX compliance flags
add_cxflags("-D_XOPEN_SOURCE=700", "-D_GNU_SOURCE", "-pthread", "-Wall", "-Wextra", "-Wpedantic")
add_ldflags("-pthread", "-lrt")

-- Include paths
add_includedirs("include")
add_includedirs("include/xinim")
add_includedirs("include/xinim/drivers")
add_includedirs("third_party/limine")

-- Dependencies
add_requires("libsodium")

-- Main XINIM kernel target
target("xinim")
    set_kind("binary")
    set_languages("cxx23")
    add_cxflags("-std=c++23")
    add_files("src/main.cpp")

    -- Kernel subsystem
    add_files("src/kernel/kernel.cpp")
    add_files("src/kernel/system.cpp")
    add_files("src/kernel/syscall.cpp")
    add_files("src/kernel/proc.cpp")
    add_files("src/kernel/schedule.cpp")
    add_files("src/kernel/service.cpp")
    add_files("src/kernel/tty.cpp")
    add_files("src/kernel/panic.cpp")
    add_files("src/kernel/interrupts.cpp")
    add_files("src/kernel/idt64.cpp")
    add_files("src/kernel/mpx64.cpp")
    add_files("src/kernel/klib64.cpp")
    add_files("src/kernel/paging.cpp")
    add_files("src/kernel/memory.cpp")
    add_files("src/kernel/clock.cpp")
    add_files("src/kernel/timing.cpp")  -- udelay/mdelay for drivers
    add_files("src/kernel/irq.cpp")     -- IRQ management subsystem
    add_files("src/kernel/dmp.cpp")
    add_files("src/kernel/wormhole.cpp")
    add_files("src/kernel/lattice_ipc.cpp")
    add_files("src/kernel/wait_graph.cpp")
    add_files("src/kernel/net_driver.cpp")

    -- Week 8 Phase 2: Preemptive scheduling and context switching
    add_files("src/kernel/server_spawn.cpp")
    add_files("src/kernel/scheduler.cpp")
    add_files("src/kernel/timer.cpp")
    add_files("src/kernel/arch/x86_64/idt.cpp")

    -- Week 8 Phase 3: Ring 3 transition (GDT, TSS)
    add_files("src/kernel/arch/x86_64/gdt.cpp")
    add_files("src/kernel/arch/x86_64/tss.cpp")

    -- Week 8 Phase 4: Syscall infrastructure
    add_files("src/kernel/syscall_table.cpp")
    add_files("src/kernel/arch/x86_64/syscall_init.cpp")
    add_files("src/kernel/syscalls/basic.cpp")

    -- Week 9 Phase 1: VFS integration and file operations
    add_files("src/kernel/uaccess.cpp")
    add_files("src/kernel/fd_table.cpp")
    add_files("src/kernel/vfs_interface.cpp")
    add_files("src/kernel/syscalls/file_ops.cpp")

    -- Week 9 Phase 2: Process management (fork, wait, getppid)
    add_files("src/kernel/syscalls/process_mgmt.cpp")

    -- Week 9 Phase 3: Advanced FD operations and pipes (dup, dup2, pipe, fcntl)
    add_files("src/kernel/pipe.cpp")
    add_files("src/kernel/syscalls/fd_advanced.cpp")

    -- Week 10 Phase 1: Program execution (execve, ELF loading, stack setup)
    add_files("src/kernel/elf_loader.cpp")
    add_files("src/kernel/exec_stack.cpp")
    add_files("src/kernel/syscalls/exec.cpp")

    -- Week 10 Phase 2: Signal framework (POSIX signals, signal delivery, signal syscalls)
    add_files("src/kernel/signal.cpp")
    add_files("src/kernel/syscalls/signal.cpp")

    -- Week 10 Phase 3: Process groups and sessions (job control)
    add_files("src/kernel/process_group.cpp")
    add_files("src/kernel/tty_signals.cpp")

    -- Week 8: Assembly files for context switching, interrupts, GDT, TSS, syscalls
    add_files("src/arch/x86_64/context_switch.S")
    add_files("src/arch/x86_64/interrupts.S")
    add_files("src/arch/x86_64/gdt_load.S")
    add_files("src/arch/x86_64/tss_load.S")
    add_files("src/arch/x86_64/syscall_handler.S")

    -- HAL subsystem (generic HAL + x86_64-specific implementation)
    add_files("src/hal/hal.cpp")
    add_files("src/hal/x86_64/hal/*.cpp")

    -- Memory management
    add_files("src/mm/memory.cpp")
    add_files("src/mm/alloc.cpp")
    add_files("src/mm/paging.cpp")
    add_files("src/mm/vm.cpp")
    add_files("src/mm/exec.cpp")
    add_files("src/mm/forkexit.cpp")
    add_files("src/mm/getset.cpp")
    add_files("src/mm/signal.cpp")
    add_files("src/mm/break.cpp")
    add_files("src/mm/utility.cpp")
    add_files("src/mm/process_slot.cpp")
    add_files("src/mm/table.cpp")
    add_files("src/mm/main.cpp")

    -- Filesystem
    add_files("src/fs/filesystem.cpp")
    add_files("src/fs/main.cpp")
    add_files("src/fs/open.cpp")
    add_files("src/fs/read.cpp")
    add_files("src/fs/write.cpp")
    add_files("src/fs/cache.cpp")
    add_files("src/fs/device.cpp")
    add_files("src/fs/inode.cpp")
    add_files("src/fs/super.cpp")
    add_files("src/fs/table.cpp")
    add_files("src/fs/pipe.cpp")
    add_files("src/fs/path.cpp")
    add_files("src/fs/link.cpp")
    add_files("src/fs/mount.cpp")
    add_files("src/fs/protect.cpp")
    add_files("src/fs/stadir.cpp")
    add_files("src/fs/misc.cpp")
    add_files("src/fs/time.cpp")
    add_files("src/fs/utility.cpp")
    add_files("src/fs/compat.cpp")

    -- Crypto subsystem
    add_files("src/crypto/crypto.cpp")
    add_files("src/crypto/kyber.cpp")
    add_files("src/crypto/kyber_cpp23_simd.cpp")
    add_files("src/crypto/pqcrypto_shared.cpp")
    add_files("src/crypto/kyber_impl/*.cpp")

    -- Network subsystem
    add_files("src/net/network.cpp")

    -- PCI subsystem (required for drivers)
    add_files("src/pci/pci.cpp")

    -- Modern drivers for x86_64
    add_files("src/drivers/net/e1000.cpp")
    add_files("src/drivers/block/ahci.cpp")
    -- TODO: Add VirtIO drivers
    -- add_files("src/drivers/virtio/*.cpp")
    
    -- Microkernel servers
    add_files("src/servers/reincarnation_server.cpp")
    
    -- VFS layer
    add_files("src/vfs/vfs.cpp")
    
    -- DMA management
    add_files("src/mm/dma.cpp")
    add_files("src/mm/dma_allocator.cpp")  -- New complete DMA allocator for drivers

    -- Commands/utilities
    add_files("src/commands/*.cpp")

    -- Common utilities
    add_files("src/common/math/*.cpp")

    -- Tools
    add_files("src/tools/*.cpp")

-- Userland components

-- XINIM Shell (xinim-sh) - Week 10 Phase 3
target("xinim-sh")
    set_kind("binary")
    set_languages("cxx23")
    add_files("userland/shell/xinim-sh/*.cpp")
    add_includedirs("include")
    add_links("pthread")
target_end()

-- mksh shell
target("mksh")
    set_kind("binary")
    set_languages("c17")
    add_files("userland/shell/mksh/integration/*.c")
    add_includedirs("include")
    -- TODO: Add mksh source when downloaded
    -- add_files("userland/shell/mksh/mksh.c")
target_end()

-- Userland libraries (libc, libm, libpthread)
-- TODO: Implement in Week 4
-- target("xinim_libc")
--     set_kind("shared")
--     add_files("userland/libc/src/**/*.c")
--     add_files("userland/libc/arch/x86_64/**/*.S")
-- target_end()

    -- Boot and early initialization
    add_files("src/boot/limine/*.cpp")
    add_files("src/kernel/early/*.cpp")

    -- Link with required libraries
    add_links("sodium", "rt", "pthread", "m")

-- POSIX compliance test suite (native wrapper)
target("posix-suite")
    set_kind("binary")
    add_files("test/posix_compliance_suite.cpp")
    add_includedirs("include")
    add_links("rt", "pthread")

-- POSIX compliance verification (comprehensive)
target("posix-verify")
    set_kind("binary")
    add_files("test/posix_compliance_verification.cpp")
    add_includedirs("include")
    add_links("rt", "pthread")

-- POSIX compliance test target (basic)
target("posix-test")
    set_kind("binary")
    add_files("test/posix_compliance_test.cpp")
    add_includedirs("include")
    add_links("rt", "pthread")

-- Comprehensive POSIX compliance test target
target("posix-comprehensive")
    set_kind("binary")
    add_files("test/posix_comprehensive_test.cpp")
    add_includedirs("include")
    add_links("rt", "pthread")

-- GNU POSIX Test Suite integration target
target("posix-gnu-test")
    set_kind("binary")
    on_run(function (target)
        os.cd("third_party/gpl/posixtestsuite-main/")
        os.exec("./run_tests AIO")
    end)

-- Week 10 Phase 3: Signal testing
target("signal-test")
    set_kind("binary")
    add_files("tests/signal/test_signal_comprehensive.cpp")
    add_includedirs("include")
    add_links("pthread")

-- All tests target
target("test-all")
    set_kind("phony")
    add_deps("posix-test", "posix-comprehensive", "posix-suite", "posix-verify")
    on_build(function (target)
        import("core.project.task")
        task.run("posix-test")
        task.run("posix-comprehensive")
        task.run("posix-suite")
        task.run("posix-verify")
    end)
-- =============================================================================
-- XINIM Build System Enhancement
-- Integrated from xmake_enhanced.lua
-- Adds coverage, sanitizers, formal verification, and analysis targets
-- =============================================================================

-- Coverage build target
target("xinim-coverage")
    set_kind("binary")
    set_languages("cxx23")
    add_files("src/main.cpp")
    add_files("src/kernel/*.cpp")
    add_files("src/mm/*.cpp")
    add_files("src/crypto/*.cpp")
    add_files("src/hal/hal.cpp")
    add_files("src/hal/x86_64/hal/*.cpp")
    
    -- Coverage flags
    add_cxflags("-fprofile-arcs", "-ftest-coverage", "-O0", "-g")
    add_ldflags("--coverage")
    add_links("gcov")
    
    -- Include paths
    add_includedirs("include", "include/xinim")
    
    on_run(function (target)
        -- Run tests and generate coverage
        os.exec("lcov --capture --directory . --output-file coverage.info")
        os.exec("lcov --remove coverage.info '/usr/*' '*/third_party/*' --output-file coverage_filtered.info")
        os.exec("genhtml coverage_filtered.info --output-directory coverage_html --demangle-cpp")
        print("Coverage report generated: coverage_html/index.html")
    end)

-- AddressSanitizer build target
target("xinim-asan")
    set_kind("binary")
    set_languages("cxx23")
    add_files("src/main.cpp")
    add_files("src/kernel/*.cpp")
    add_files("src/mm/*.cpp")
    
    -- ASAN flags
    add_cxflags("-fsanitize=address", "-fno-omit-frame-pointer", "-O1", "-g")
    add_ldflags("-fsanitize=address")
    
    add_includedirs("include", "include/xinim")

-- ThreadSanitizer build target
target("xinim-tsan")
    set_kind("binary")
    set_languages("cxx23")
    add_files("src/main.cpp")
    add_files("src/kernel/*.cpp")
    add_files("src/mm/*.cpp")
    
    -- TSAN flags
    add_cxflags("-fsanitize=thread", "-O1", "-g")
    add_ldflags("-fsanitize=thread")
    
    add_includedirs("include", "include/xinim")

-- UndefinedBehaviorSanitizer build target
target("xinim-ubsan")
    set_kind("binary")
    set_languages("cxx23")
    add_files("src/main.cpp")
    add_files("src/kernel/*.cpp")
    add_files("src/mm/*.cpp")
    
    -- UBSAN flags
    add_cxflags("-fsanitize=undefined", "-O1", "-g")
    add_ldflags("-fsanitize=undefined")
    
    add_includedirs("include", "include/xinim")

-- MemorySanitizer build target
target("xinim-msan")
    set_kind("binary")
    set_languages("cxx23")
    add_files("src/main.cpp")
    add_files("src/kernel/*.cpp")
    add_files("src/mm/*.cpp")
    
    -- MSAN flags
    add_cxflags("-fsanitize=memory", "-fno-omit-frame-pointer", "-O1", "-g")
    add_ldflags("-fsanitize=memory")
    
    add_includedirs("include", "include/xinim")

-- Documentation generation target
target("docs")
    set_kind("phony")
    on_build(function (target)
        print("Generating Doxygen documentation...")
        os.exec("doxygen Doxyfile")
        print("Documentation generated: docs/html/index.html")
    end)

-- Analysis target (runs all static analysis tools)
target("analyze")
    set_kind("phony")
    on_build(function (target)
        print("Running comprehensive code analysis...")
        os.exec("bash tools/run_analysis.sh")
    end)

-- Advanced analysis target (includes formal verification)
target("analyze-advanced")
    set_kind("phony")
    on_build(function (target)
        print("Running advanced analysis suite...")
        os.exec("bash tools/run_advanced_analysis.sh")
    end)

-- Format target (runs clang-format on all source files)
target("format")
    set_kind("phony")
    on_build(function (target)
        print("Formatting all C++ files...")
        local files = {}
        for _, dir in ipairs({"src", "include"}) do
            for _, ext in ipairs({"cpp", "hpp", "h"}) do
                for _, file in ipairs(os.files(path.join(dir, "**." .. ext))) do
                    table.insert(files, file)
                end
            end
        end
        for _, file in ipairs(files) do
            os.exec("clang-format -i " .. file)
        end
        print("Formatting complete!")
    end)

-- Lint target (runs clang-tidy)
target("lint")
    set_kind("phony")
    on_build(function (target)
        print("Running clang-tidy...")
        os.exec("bash tools/run_clang_tidy.sh")
    end)

-- Security scan target
target("security-scan")
    set_kind("phony")
    on_build(function (target)
        print("Running security analysis...")
        os.exec("flawfinder --quiet src/ include/ > docs/analysis/reports/static_analysis/flawfinder_latest.txt")
        print("Security scan complete: docs/analysis/reports/static_analysis/flawfinder_latest.txt")
    end)

-- Formal verification target
target("verify")
    set_kind("phony")
    on_build(function (target)
        print("Running formal verification (Z3)...")
        os.exec("python3 specs/verify_all.py")
    end)

-- All checks target
target("check-all")
    set_kind("phony")
    add_deps("analyze", "lint", "security-scan", "verify")
    on_build(function (target)
        print("All checks complete!")
    end)
