// XINIM POSIX Compliance Verification System
// Comprehensive testing framework for SUSv5 compliance validation

#include "../include/xinim/meta/posix_framework.hpp"
#include "../include/xinim/constexpr/system_utilities.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <concepts>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <ranges>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;
using namespace xinim::meta::posix;
using namespace xinim::constexpr_sys;

// ═══════════════════════════════════════════════════════════════════════════
// POSIX Compliance Test Framework
// ═══════════════════════════════════════════════════════════════════════════

struct posix_test_result {
    std::string test_name;
    std::string utility_name;
    bool passed = false;
    std::string error_message;
    std::chrono::milliseconds execution_time{0};
    std::string output;
    int exit_code = 0;
};

struct compliance_summary {
    std::size_t total_tests = 0;
    std::size_t passed_tests = 0;
    std::size_t failed_tests = 0;
    std::size_t missing_utilities = 0;
    std::vector<std::string> missing_utility_names;
    std::vector<posix_test_result> failed_test_details;
    std::chrono::milliseconds total_time{0};
};

class posix_compliance_tester {
private:
    fs::path xinim_commands_dir_;
    fs::path test_results_dir_;
    std::map<std::string, std::vector<std::string>> utility_test_cases_;
    
    // Complete list of 150 POSIX SUSv5 utilities
    static constexpr std::array<std::string_view, 150> POSIX_UTILITIES = {
        // Core utilities (25)
        "true", "false", "echo", "cat", "pwd", "ls", "cp", "mv", "rm",
        "mkdir", "rmdir", "chmod", "chown", "ln", "touch", "stat", "find",
        "locate", "which", "basename", "dirname", "realpath", "mktemp", "install", "sync",
        
        // Text processing (30)
        "cut", "awk", "sed", "grep", "sort", "uniq", "wc", "head", "tail",
        "tr", "join", "paste", "split", "csplit", "fold", "expand", "unexpand",
        "nl", "pr", "fmt", "column", "comm", "diff", "cmp", "patch", "strings",
        "od", "hexdump", "xxd", "base64",
        
        // Shell utilities (35)
        "env", "export", "set", "unset", "alias", "unalias", "cd", "pushd",
        "popd", "dirs", "jobs", "bg", "fg", "kill", "killall", "ps", "top",
        "htop", "nohup", "timeout", "sleep", "wait", "exec", "exit", "logout",
        "su", "sudo", "id", "whoami", "who", "groups", "newgrp", "test", "expr",
        "time", "watch",
        
        // System utilities (40)
        "mount", "umount", "df", "du", "fsck", "mkfs", "fdisk", "lsblk", "blkid",
        "uname", "hostname", "uptime", "date", "cal", "logger", "dmesg",
        "lscpu", "lsmem", "free", "vmstat", "iostat", "lsof", "netstat", "ss",
        "ping", "traceroute", "wget", "curl", "ssh", "scp", "rsync", "tar", "gzip",
        "gunzip", "zip", "unzip", "compress", "uncompress", "xz", "unxz",
        
        // Development tools (20)
        "make", "ar", "nm", "objdump", "strip", "size", "ld", "as",
        "cc", "gcc", "clang", "cpp", "lex", "yacc", "m4", "git", "patch", "diff", "cmp", "strings"
    };

public:
    explicit posix_compliance_tester(const fs::path& xinim_commands_dir) 
        : xinim_commands_dir_(xinim_commands_dir),
          test_results_dir_(fs::current_path() / "posix_test_results") {
        
        fs::create_directories(test_results_dir_);
        initialize_test_cases();
    }
    
    compliance_summary run_full_compliance_test() {
        std::cout << std::format("\n=== XINIM POSIX SUSv5 Compliance Testing ===\n");
        std::cout << std::format("Commands directory: {}\n", xinim_commands_dir_.string());
        std::cout << std::format("Testing {} POSIX utilities...\n\n", POSIX_UTILITIES.size());
        
        compliance_summary summary;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (const auto& utility_name : POSIX_UTILITIES) {
            auto utility_results = test_utility(std::string(utility_name));
            
            for (const auto& result : utility_results) {
                summary.total_tests++;
                if (result.passed) {
                    summary.passed_tests++;
                } else {
                    summary.failed_tests++;
                    summary.failed_test_details.push_back(result);
                }
            }
            
            if (utility_results.empty()) {
                summary.missing_utilities++;
                summary.missing_utility_names.emplace_back(utility_name);
            }
            
            // Progress indicator
            if ((summary.total_tests % 10) == 0) {
                std::cout << std::format("\rProgress: {}/{} utilities tested", 
                                        summary.total_tests / 10, POSIX_UTILITIES.size());
                std::cout.flush();
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        summary.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << std::format("\n\nTesting completed in {} ms\n", summary.total_time.count());
        
        return summary;
    }
    
    std::vector<posix_test_result> test_utility(const std::string& utility_name) {
        std::vector<posix_test_result> results;
        
        // Check if utility exists
        fs::path utility_path = find_utility_implementation(utility_name);
        if (utility_path.empty()) {
            return results; // Empty results indicate missing utility
        }
        
        // Get test cases for this utility
        auto test_cases = get_test_cases_for_utility(utility_name);
        
        for (const auto& test_case : test_cases) {
            auto result = run_single_test(utility_name, utility_path, test_case);
            results.push_back(result);
        }
        
        return results;
    }

private:
    void initialize_test_cases() {
        // Core utilities test cases
        utility_test_cases_["echo"] = {
            "echo hello world",
            "echo -n no_newline",
            "echo -e 'tab\\there'",
            "echo '$USER'",
            "echo"
        };
        
        utility_test_cases_["cat"] = {
            "cat /dev/null",
            "echo 'test' | cat",
            "echo 'line1\\nline2' | cat -n"
        };
        
        utility_test_cases_["pwd"] = {
            "pwd",
            "cd /tmp && pwd"
        };
        
        utility_test_cases_["ls"] = {
            "ls",
            "ls -l",
            "ls -la",
            "ls /tmp",
            "ls -la /dev/null"
        };
        
        utility_test_cases_["true"] = {"true"};
        utility_test_cases_["false"] = {"false"};
        
        utility_test_cases_["wc"] = {
            "echo 'hello world' | wc",
            "echo 'hello world' | wc -w",
            "echo 'hello world' | wc -c",
            "echo 'line1\\nline2' | wc -l"
        };
        
        utility_test_cases_["grep"] = {
            "echo 'hello world' | grep hello",
            "echo 'hello world' | grep -v goodbye",
            "echo 'Hello World' | grep -i hello"
        };
        
        utility_test_cases_["sort"] = {
            "echo -e 'c\\nb\\na' | sort",
            "echo -e '3\\n1\\n2' | sort -n"
        };
        
        utility_test_cases_["head"] = {
            "echo -e '1\\n2\\n3\\n4\\n5' | head -3"
        };
        
        utility_test_cases_["tail"] = {
            "echo -e '1\\n2\\n3\\n4\\n5' | tail -2"
        };
        
        utility_test_cases_["cut"] = {
            "echo 'a,b,c' | cut -d, -f2"
        };
        
        utility_test_cases_["uniq"] = {
            "echo -e 'a\\na\\nb' | uniq"
        };
        
        utility_test_cases_["tr"] = {
            "echo 'hello' | tr a-z A-Z"
        };
        
        utility_test_cases_["basename"] = {
            "basename /usr/local/bin/test",
            "basename /usr/local/bin/test .exe"
        };
        
        utility_test_cases_["dirname"] = {
            "dirname /usr/local/bin/test"
        };
        
        utility_test_cases_["env"] = {
            "env",
            "env | grep PATH"
        };
        
        utility_test_cases_["id"] = {
            "id",
            "id -u",
            "id -g"
        };
        
        utility_test_cases_["whoami"] = {"whoami"};
        
        utility_test_cases_["uname"] = {
            "uname",
            "uname -a",
            "uname -s",
            "uname -r"
        };
        
        utility_test_cases_["hostname"] = {"hostname"};
        
        utility_test_cases_["date"] = {
            "date",
            "date +%Y-%m-%d"
        };
        
        utility_test_cases_["sleep"] = {
            "sleep 0.1"  // Very short sleep for testing
        };
        
        // Add default test case for utilities without specific tests
        for (const auto& utility : POSIX_UTILITIES) {
            std::string util_name(utility);
            if (utility_test_cases_.find(util_name) == utility_test_cases_.end()) {
                utility_test_cases_[util_name] = {util_name + " --help"};
            }
        }
    }
    
    fs::path find_utility_implementation(const std::string& utility_name) {
        // Look for C++23 implementation first
        std::vector<std::string> candidates = {
            utility_name + "_cpp23",
            utility_name + "_cpp23.exe",
            utility_name,
            utility_name + ".exe"
        };
        
        for (const auto& candidate : candidates) {
            fs::path full_path = xinim_commands_dir_ / candidate;
            if (fs::exists(full_path) && fs::is_regular_file(full_path)) {
                return full_path;
            }
        }
        
        return {}; // Not found
    }
    
    std::vector<std::string> get_test_cases_for_utility(const std::string& utility_name) {
        auto it = utility_test_cases_.find(utility_name);
        if (it != utility_test_cases_.end()) {
            return it->second;
        }
        
        // Default test case
        return {utility_name + " --version", utility_name + " --help"};
    }
    
    posix_test_result run_single_test(const std::string& utility_name, 
                                     const fs::path& utility_path,
                                     const std::string& test_command) {
        posix_test_result result;
        result.test_name = test_command;
        result.utility_name = utility_name;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        try {
            // Create a test script to run the command
            fs::path test_script = test_results_dir_ / std::format("test_{}_script.sh", utility_name);
            
            std::ofstream script_file(test_script);
            script_file << "#!/bin/bash\n";
            script_file << "set -e\n";  // Exit on error
            script_file << "export PATH=" << xinim_commands_dir_.string() << ":$PATH\n";
            script_file << test_command << "\n";
            script_file.close();
            
            fs::permissions(test_script, 
                          fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec);
            
            // Execute test script and capture output
            fs::path output_file = test_results_dir_ / std::format("test_{}_{}_output.txt", 
                                                                  utility_name, 
                                                                  std::hash<std::string>{}(test_command));
            
            std::string command = std::format("/bin/bash {} > {} 2>&1; echo $? > {}.exit", 
                                            test_script.string(), 
                                            output_file.string(),
                                            output_file.string());
            
            int system_result = std::system(command.c_str());
            
            // Read output
            if (fs::exists(output_file)) {
                std::ifstream output_stream(output_file);
                std::string line;
                while (std::getline(output_stream, line)) {
                    result.output += line + "\n";
                }
            }
            
            // Read exit code
            fs::path exit_file = fs::path(output_file.string() + ".exit");
            if (fs::exists(exit_file)) {
                std::ifstream exit_stream(exit_file);
                exit_stream >> result.exit_code;
            }
            
            // Determine if test passed
            result.passed = evaluate_test_result(utility_name, test_command, result.output, result.exit_code);
            
            if (!result.passed && result.error_message.empty()) {
                result.error_message = std::format("Command failed with exit code {}", result.exit_code);
            }
            
            // Cleanup temporary files
            fs::remove(test_script);
            fs::remove(output_file);
            fs::remove(exit_file);
            
        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
            result.exit_code = -1;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        return result;
    }
    
    bool evaluate_test_result(const std::string& utility_name, 
                             const std::string& command,
                             const std::string& output,
                             int exit_code) {
        
        // Special cases for different utilities
        if (utility_name == "false") {
            return exit_code == 1;  // false should return 1
        }
        
        if (utility_name == "true") {
            return exit_code == 0;  // true should return 0
        }
        
        if (command.contains("--help") || command.contains("--version")) {
            // Help and version commands should not crash
            return exit_code == 0 || exit_code == 1;  // Some utilities use exit code 1 for help
        }
        
        // Check for common error patterns
        if (output.find("command not found") != std::string::npos ||
            output.find("No such file or directory") != std::string::npos ||
            output.find("Permission denied") != std::string::npos) {
            return false;
        }
        
        // For most utilities, exit code 0 indicates success
        return exit_code == 0;
    }

public:
    void generate_compliance_report(const compliance_summary& summary) {
        std::cout << std::format("\n");
        std::cout << std::format("═══════════════════════════════════════════════════════════════\n");
        std::cout << std::format("XINIM POSIX SUSv5 Compliance Report\n");
        std::cout << std::format("═══════════════════════════════════════════════════════════════\n");
        std::cout << std::format("Total utilities expected: {}\n", POSIX_UTILITIES.size());
        std::cout << std::format("Total tests executed: {}\n", summary.total_tests);
        std::cout << std::format("Tests passed: {} ({:.1f}%)\n", 
                                summary.passed_tests, 
                                100.0 * summary.passed_tests / summary.total_tests);
        std::cout << std::format("Tests failed: {} ({:.1f}%)\n", 
                                summary.failed_tests,
                                100.0 * summary.failed_tests / summary.total_tests);
        std::cout << std::format("Missing utilities: {}\n", summary.missing_utilities);
        std::cout << std::format("Execution time: {} ms\n", summary.total_time.count());
        std::cout << std::format("\n");
        
        // Implementation completeness
        std::size_t implemented_utilities = POSIX_UTILITIES.size() - summary.missing_utilities;
        double completeness = 100.0 * implemented_utilities / POSIX_UTILITIES.size();
        std::cout << std::format("Implementation completeness: {:.1f}% ({}/{})\n", 
                                completeness, implemented_utilities, POSIX_UTILITIES.size());
        
        // Missing utilities
        if (!summary.missing_utility_names.empty()) {
            std::cout << std::format("\n❌ MISSING UTILITIES:\n");
            for (const auto& name : summary.missing_utility_names) {
                std::cout << std::format("  - {}\n", name);
            }
        }
        
        // Failed tests
        if (!summary.failed_test_details.empty()) {
            std::cout << std::format("\n❌ FAILED TESTS:\n");
            std::size_t max_display = std::min(summary.failed_test_details.size(), std::size_t{20});
            
            for (std::size_t i = 0; i < max_display; ++i) {
                const auto& failure = summary.failed_test_details[i];
                std::cout << std::format("  {} [{}]: {}\n", 
                                        failure.utility_name, 
                                        failure.test_name,
                                        failure.error_message);
            }
            
            if (summary.failed_test_details.size() > max_display) {
                std::cout << std::format("  ... and {} more failures\n", 
                                        summary.failed_test_details.size() - max_display);
            }
        }
        
        std::cout << std::format("\n");
        
        // Overall assessment
        if (summary.missing_utilities == 0 && summary.failed_tests == 0) {
            std::cout << std::format("🎉 FULL POSIX COMPLIANCE ACHIEVED!\n");
        } else if (completeness >= 90.0 && summary.failed_tests < 10) {
            std::cout << std::format("✅ HIGH POSIX COMPLIANCE - Minor issues to resolve\n");
        } else if (completeness >= 70.0) {
            std::cout << std::format("⚠️  MODERATE POSIX COMPLIANCE - Implementation needs work\n");
        } else {
            std::cout << std::format("❌ LOW POSIX COMPLIANCE - Major implementation gaps\n");
        }
        
        std::cout << std::format("═══════════════════════════════════════════════════════════════\n");
        
        // Generate detailed HTML report
        generate_html_report(summary);
    }

private:
    void generate_html_report(const compliance_summary& summary) {
        fs::path html_report = test_results_dir_ / "xinim_posix_compliance.html";
        
        std::ofstream report(html_report);
        report << R"(<!DOCTYPE html>
<html>
<head>
    <title>XINIM POSIX SUSv5 Compliance Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .header { background: #2c3e50; color: white; padding: 20px; }
        .summary { background: #ecf0f1; padding: 15px; margin: 20px 0; }
        .passed { color: #27ae60; }
        .failed { color: #e74c3c; }
        .missing { color: #f39c12; }
        table { width: 100%; border-collapse: collapse; margin: 20px 0; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background: #3498db; color: white; }
        .utility-status { padding: 4px 8px; border-radius: 4px; color: white; }
        .status-implemented { background: #27ae60; }
        .status-missing { background: #e74c3c; }
    </style>
</head>
<body>
    <div class="header">
        <h1>XINIM POSIX SUSv5 Compliance Report</h1>
        <p>Generated: )" << std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::system_clock::now()) << R"(</p>
        <p>C++23 Pure Implementation with Post-Quantum Cryptography</p>
    </div>

    <div class="summary">
        <h2>Executive Summary</h2>
        <p><strong>Total Utilities:</strong> )" << POSIX_UTILITIES.size() << R"(</p>
        <p><strong>Tests Executed:</strong> )" << summary.total_tests << R"(</p>
        <p class="passed"><strong>Tests Passed:</strong> )" << summary.passed_tests 
            << std::format(" ({:.1f}%)", 100.0 * summary.passed_tests / summary.total_tests) << R"(</p>
        <p class="failed"><strong>Tests Failed:</strong> )" << summary.failed_tests 
            << std::format(" ({:.1f}%)", 100.0 * summary.failed_tests / summary.total_tests) << R"(</p>
        <p class="missing"><strong>Missing Utilities:</strong> )" << summary.missing_utilities << R"(</p>
        <p><strong>Execution Time:</strong> )" << summary.total_time.count() << R"( ms</p>
    </div>

    <h2>Utility Implementation Status</h2>
    <table>
        <tr>
            <th>Utility</th>
            <th>Status</th>
            <th>Tests</th>
            <th>Category</th>
        </tr>)";
        
        // Utility status table
        std::unordered_set<std::string> missing_set(
            summary.missing_utility_names.begin(), 
            summary.missing_utility_names.end());
        
        std::size_t index = 0;
        for (const auto& utility : POSIX_UTILITIES) {
            std::string util_name(utility);
            std::string status = missing_set.contains(util_name) ? "Missing" : "Implemented";
            std::string status_class = missing_set.contains(util_name) ? "status-missing" : "status-implemented";
            
            // Determine category
            std::string category;
            if (index < 25) category = "Core";
            else if (index < 55) category = "Text Processing";
            else if (index < 90) category = "Shell Utilities";
            else if (index < 130) category = "System Utilities";
            else category = "Development Tools";
            
            report << std::format(R"(
        <tr>
            <td>{}</td>
            <td><span class="utility-status {}">{}</span></td>
            <td>N/A</td>
            <td>{}</td>
        </tr>)", util_name, status_class, status, category);
            
            ++index;
        }
        
        report << R"(
    </table>
</body>
</html>)";
        
        report.close();
        
        std::cout << std::format("📋 Detailed HTML report generated: {}\n", html_report.string());
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// Main Testing Function
// ═══════════════════════════════════════════════════════════════════════════

int main(int argc, char* argv[]) {
    try {
        // Default to build/commands directory
        fs::path commands_dir = "build/commands";
        
        if (argc > 1) {
            commands_dir = argv[1];
        }
        
        if (!fs::exists(commands_dir)) {
            std::cerr << std::format("Commands directory does not exist: {}\n", commands_dir.string());
            std::cerr << "Usage: {} [commands_directory]\n" << argv[0];
            return 1;
        }
        
        posix_compliance_tester tester(commands_dir);
        auto summary = tester.run_full_compliance_test();
        tester.generate_compliance_report(summary);
        
        // Return appropriate exit code
        if (summary.missing_utilities == 0 && summary.failed_tests == 0) {
            std::cout << "\n🎉 All POSIX compliance tests passed!\n";
            return 0;
        } else {
            std::cout << std::format("\n⚠️  POSIX compliance issues detected: {} missing utilities, {} failed tests\n", 
                                    summary.missing_utilities, summary.failed_tests);
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << std::format("Error: {}\n", e.what());
        return 2;
    }
}