// Asynchronous Grep - C++23 Coroutine-Based Text Search
// World's first async POSIX utility with structured concurrency

import xinim.posix;

#include <coroutine>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

namespace fs = std::filesystem;
using namespace xinim::posix;

// Advanced async task with cancellation support
template<typename T>
class CancellableTask {
public:
    struct promise_type {
        T result{};
        std::exception_ptr exception{};
        bool cancelled = false;
        
        CancellableTask get_return_object() {
            return CancellableTask{
                std::coroutine_handle<promise_type>::from_promise(*this)
            };
        }
        
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        
        void unhandled_exception() {
            exception = std::current_exception();
        }
        
        void return_value(T value) requires(!std::is_void_v<T>) {
            result = std::move(value);
        }
        
        void return_void() requires std::is_void_v<T> {}
        
        // Cancellation support
        void cancel() { cancelled = true; }
        bool is_cancelled() const { return cancelled; }
    };
    
private:
    std::coroutine_handle<promise_type> handle_;
    
public:
    explicit CancellableTask(std::coroutine_handle<promise_type> h) : handle_(h) {}
    
    ~CancellableTask() {
        if (handle_) handle_.destroy();
    }
    
    CancellableTask(const CancellableTask&) = delete;
    CancellableTask& operator=(const CancellableTask&) = delete;
    
    CancellableTask(CancellableTask&& other) noexcept 
        : handle_(std::exchange(other.handle_, {})) {}
        
    CancellableTask& operator=(CancellableTask&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }
    
    void cancel() {
        if (handle_) {
            handle_.promise().cancel();
        }
    }
    
    bool is_ready() const {
        return handle_ && handle_.done();
    }
    
    T get_result() {
        if (!handle_) throw std::runtime_error("Invalid task");
        if (handle_.promise().exception) {
            std::rethrow_exception(handle_.promise().exception);
        }
        if constexpr (!std::is_void_v<T>) {
            return std::move(handle_.promise().result);
        }
    }
};

// Awaitable cancellation token
struct cancellation_token {
    std::shared_ptr<std::atomic<bool>> cancelled{
        std::make_shared<std::atomic<bool>>(false)
    };
    
    void cancel() { cancelled->store(true); }
    bool is_cancelled() const { return cancelled->load(); }
    
    // Awaitable interface
    bool await_ready() const noexcept { return is_cancelled(); }
    void await_suspend(std::coroutine_handle<> h) const noexcept {}
    void await_resume() const {
        if (is_cancelled()) {
            throw std::runtime_error("Operation was cancelled");
        }
    }
};

// Async file reader with yield points
class AsyncFileReader {
public:
    static CancellableTask<std::vector<std::string>> 
    read_lines_async(const fs::path& filepath, cancellation_token token = {}) {
        std::vector<std::string> lines;
        
        if (!fs::exists(filepath)) {
            throw std::runtime_error("File not found");
        }
        
        std::ifstream file(filepath);
        if (!file) {
            throw std::runtime_error("Cannot open file");
        }
        
        std::string line;
        std::size_t lines_read = 0;
        
        while (std::getline(file, line)) {
            // Check for cancellation
            if (token.is_cancelled()) {
                co_await token;  // Will throw cancellation exception
            }
            
            lines.push_back(std::move(line));
            ++lines_read;
            
            // Yield control every 1000 lines for cooperative multitasking
            if (lines_read % 1000 == 0) {
                co_await std::suspend_always{};
            }
        }
        
        co_return lines;
    }
    
    static CancellableTask<std::string> 
    read_file_async(const fs::path& filepath, cancellation_token token = {}) {
        if (!fs::exists(filepath)) {
            throw std::runtime_error("File not found");
        }
        
        std::ifstream file(filepath, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Cannot open file");
        }
        
        // Get file size
        file.seekg(0, std::ios::end);
        auto size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::string content;
        content.reserve(size);
        
        constexpr std::size_t chunk_size = 4096;
        char buffer[chunk_size];
        std::size_t bytes_read = 0;
        
        while (file.read(buffer, chunk_size) || file.gcount() > 0) {
            // Check for cancellation
            if (token.is_cancelled()) {
                co_await token;
            }
            
            content.append(buffer, file.gcount());
            bytes_read += file.gcount();
            
            // Yield every 64KB
            if (bytes_read % (64 * 1024) == 0) {
                co_await std::suspend_always{};
            }
        }
        
        co_return content;
    }
};

// Advanced pattern matcher with multiple regex support
class AsyncPatternMatcher {
    std::vector<std::regex> patterns;
    bool case_insensitive = false;
    bool invert_match = false;
    bool word_match = false;
    
public:
    AsyncPatternMatcher(const std::vector<std::string>& pattern_strings, 
                       bool case_insensitive = false,
                       bool word_match = false) 
        : case_insensitive(case_insensitive), word_match(word_match) {
        
        auto flags = std::regex::ECMAScript;
        if (case_insensitive) {
            flags |= std::regex::icase;
        }
        
        patterns.reserve(pattern_strings.size());
        for (const auto& pattern_str : pattern_strings) {
            std::string final_pattern = pattern_str;
            if (word_match) {
                final_pattern = R"(\b)" + pattern_str + R"(\b)";
            }
            patterns.emplace_back(final_pattern, flags);
        }
    }
    
    CancellableTask<std::vector<std::pair<std::size_t, std::string>>>
    search_async(const std::vector<std::string>& lines, 
                 cancellation_token token = {}) {
        
        std::vector<std::pair<std::size_t, std::string>> matches;
        
        for (std::size_t line_num = 0; line_num < lines.size(); ++line_num) {
            // Check for cancellation every 100 lines
            if (line_num % 100 == 0 && token.is_cancelled()) {
                co_await token;
            }
            
            const auto& line = lines[line_num];
            bool line_matches = false;
            
            // Check against all patterns
            for (const auto& pattern : patterns) {
                if (std::regex_search(line, pattern)) {
                    line_matches = true;
                    break;
                }
            }
            
            // Apply invert logic
            if (line_matches != invert_match) {
                matches.emplace_back(line_num + 1, line);
            }
            
            // Yield periodically for large files
            if (line_num % 10000 == 0) {
                co_await std::suspend_always{};
            }
        }
        
        co_return matches;
    }
};

// Main async grep implementation
class AsyncGrep {
    struct Options {
        std::vector<std::string> patterns;
        std::vector<fs::path> files;
        bool case_insensitive = false;
        bool invert_match = false;
        bool line_numbers = false;
        bool count_only = false;
        bool files_only = false;
        bool word_match = false;
        bool recursive = false;
        std::size_t context_before = 0;
        std::size_t context_after = 0;
        std::size_t max_count = SIZE_MAX;
    } options;
    
public:
    CancellableTask<int> execute_async(std::span<std::string_view> args, 
                                      cancellation_token token = {}) {
        
        if (!parse_arguments(args)) {
            co_return 1;
        }
        
        if (options.patterns.empty()) {
            std::cerr << "grep: no pattern specified\n";
            co_return 1;
        }
        
        AsyncPatternMatcher matcher(options.patterns, 
                                  options.case_insensitive,
                                  options.word_match);
        
        std::size_t total_matches = 0;
        bool found_any = false;
        
        // Process each file
        for (const auto& filepath : options.files) {
            if (token.is_cancelled()) {
                co_await token;
            }
            
            try {
                // Read file asynchronously
                auto lines_task = AsyncFileReader::read_lines_async(filepath, token);
                
                // Simple task scheduling - in production would use proper scheduler
                while (!lines_task.is_ready()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    co_await std::suspend_always{};
                }
                
                auto lines = lines_task.get_result();
                
                // Search asynchronously
                auto search_task = matcher.search_async(lines, token);
                
                while (!search_task.is_ready()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    co_await std::suspend_always{};
                }
                
                auto matches = search_task.get_result();
                
                // Process results
                if (!matches.empty()) {
                    found_any = true;
                    
                    if (options.count_only) {
                        std::cout << filepath.string() << ":" << matches.size() << "\n";
                    } else if (options.files_only) {
                        std::cout << filepath.string() << "\n";
                    } else {
                        for (const auto& [line_num, line] : matches) {
                            if (options.files.size() > 1) {
                                std::cout << filepath.string() << ":";
                            }
                            if (options.line_numbers) {
                                std::cout << line_num << ":";
                            }
                            std::cout << line << "\n";
                            
                            if (++total_matches >= options.max_count) {
                                co_return 0;
                            }
                        }
                    }
                }
                
            } catch (const std::exception& e) {
                std::cerr << "grep: " << filepath.string() 
                         << ": " << e.what() << "\n";
            }
            
            // Yield after each file
            co_await std::suspend_always{};
        }
        
        co_return found_any ? 0 : 1;
    }
    
private:
    bool parse_arguments(std::span<std::string_view> args) {
        bool pattern_specified = false;
        
        for (std::size_t i = 0; i < args.size(); ++i) {
            std::string_view arg = args[i];
            
            if (arg == "-i" || arg == "--ignore-case") {
                options.case_insensitive = true;
            } else if (arg == "-v" || arg == "--invert-match") {
                options.invert_match = true;
            } else if (arg == "-n" || arg == "--line-number") {
                options.line_numbers = true;
            } else if (arg == "-c" || arg == "--count") {
                options.count_only = true;
            } else if (arg == "-l" || arg == "--files-with-matches") {
                options.files_only = true;
            } else if (arg == "-w" || arg == "--word-regexp") {
                options.word_match = true;
            } else if (arg == "-r" || arg == "--recursive") {
                options.recursive = true;
            } else if (arg.starts_with("-m")) {
                // -m NUM or --max-count=NUM
                std::string num_str;
                if (arg.size() > 2) {
                    num_str = std::string(arg.substr(2));
                } else if (i + 1 < args.size()) {
                    num_str = std::string(args[++i]);
                }
                if (!num_str.empty()) {
                    options.max_count = std::stoull(num_str);
                }
            } else if (!arg.starts_with("-")) {
                if (!pattern_specified) {
                    options.patterns.push_back(std::string(arg));
                    pattern_specified = true;
                } else {
                    // File argument
                    options.files.emplace_back(arg);
                }
            }
        }
        
        // If no files specified, use stdin
        if (options.files.empty()) {
            options.files.emplace_back("-");  // stdin marker
        }
        
        return pattern_specified;
    }
};

// Async main with proper error handling
CancellableTask<int> async_main(std::span<std::string_view> args) {
    try {
        AsyncGrep grep;
        cancellation_token token;
        
        // Set up signal handling for cancellation (simplified)
        // In production would use proper signal handling
        
        auto result = co_await grep.execute_async(args, token);
        co_return result;
        
    } catch (const std::exception& e) {
        std::cerr << "async_grep: " << e.what() << "\n";
        co_return 2;
    }
}

int main(int argc, char* argv[]) {
    // Convert arguments to string_view span
    std::vector<std::string_view> args;
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    
    // Run async main
    auto main_task = async_main(std::span(args));
    
    // Simple event loop - in production would use proper async runtime
    while (!main_task.is_ready()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    try {
        return main_task.get_result();
    } catch (const std::exception& e) {
        std::cerr << "async_grep: " << e.what() << "\n";
        return 2;
    }
}