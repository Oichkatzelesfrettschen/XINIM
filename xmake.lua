-- XINIM: Modern C++23 Post-Quantum Microkernel Operating System
-- Complete xmake Build Configuration

set_project("XINIM")
set_version("1.0.0")
set_languages("c++23")

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
add_includedirs("third_party/limine")

-- Dependencies
add_requires("libsodium")

-- Main XINIM kernel target
target("xinim")
    set_kind("binary")
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
    add_files("src/kernel/dmp.cpp")
    add_files("src/kernel/wormhole.cpp")
    add_files("src/kernel/lattice_ipc.cpp")
    add_files("src/kernel/wait_graph.cpp")
    add_files("src/kernel/net_driver.cpp")

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

    -- Commands/utilities
    add_files("src/commands/*.cpp")

    -- Common utilities
    add_files("src/common/math/*.cpp")

    -- Tools
    add_files("src/tools/*.cpp")

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
    set_kind("phony")
    on_build(function (target)
        import("core.project.task")
        import("core.base.global")

        -- Run the GNU POSIX test suite
        local gnu_test_dir = path.join(os.projectdir(), "third_party/gpl/posixtestsuite-main")
        if os.isdir(gnu_test_dir) then
            os.cd(gnu_test_dir)
            os.exec("./run_tests")
        else
            print("GNU POSIX test suite not found at: " .. gnu_test_dir)
        end
    end)

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