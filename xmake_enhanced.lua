-- XINIM Build System Enhancement
-- Adds coverage, sanitizers, and advanced analysis targets

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
        os.exec("genhtml coverage_filtered.info --output-directory coverage_html")
        print("Coverage report generated: coverage_html/index.html")
    end)
target_end()

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
target_end()

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
target_end()

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
target_end()

-- MemorySanitizer build target (requires special LLVM build)
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
target_end()

-- Documentation generation target
target("docs")
    set_kind("phony")
    on_build(function (target)
        print("Generating Doxygen documentation...")
        os.exec("doxygen Doxyfile")
        print("Documentation generated: docs/html/index.html")
    end)
target_end()

-- Analysis target (runs all static analysis tools)
target("analyze")
    set_kind("phony")
    on_build(function (target)
        print("Running comprehensive code analysis...")
        os.exec("bash tools/run_analysis.sh")
    end)
target_end()

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
target_end()

-- Lint target (runs clang-tidy)
target("lint")
    set_kind("phony")
    on_build(function (target)
        print("Running clang-tidy...")
        os.exec("bash tools/run_clang_tidy.sh")
    end)
target_end()

-- Security scan target
target("security-scan")
    set_kind("phony")
    on_build(function (target)
        print("Running security analysis...")
        os.exec("flawfinder --quiet src/ include/ > docs/analysis/reports/static_analysis/flawfinder_latest.txt")
        print("Security scan complete: docs/analysis/reports/static_analysis/flawfinder_latest.txt")
    end)
target_end()

-- All analysis and checks
target("check-all")
    set_kind("phony")
    add_deps("analyze", "lint", "security-scan")
    on_build(function (target)
        print("All checks complete!")
    end)
target_end()
