#include <stdint.h>

#if defined(__x86_64__)
#include "xinim/userland/syscall_x86_64.hpp"
#include <stddef.h>
namespace xinim::userland::x86_32 {
using uint32_t = ::uint32_t;
using uint64_t = ::uint64_t;

inline uint32_t read(int fd, void* buffer, uint64_t count) noexcept {
    return static_cast<uint32_t>(xinim::userland::x86_64::read(
        static_cast<uint64_t>(fd),
        buffer,
        count));
}

inline uint32_t write(int fd, const void* buffer, uint64_t count) noexcept {
    return static_cast<uint32_t>(xinim::userland::x86_64::write(
        static_cast<uint64_t>(fd),
        buffer,
        count));
}

extern "C" void* memmove(void* destination, const void* source, size_t count) noexcept {
    auto* const destination_bytes = static_cast<unsigned char*>(destination);
    const auto* const source_bytes = static_cast<const unsigned char*>(source);

    if (destination_bytes == nullptr || source_bytes == nullptr || count == 0U) {
        return destination;
    }
    if (destination_bytes < source_bytes || destination_bytes >= source_bytes + count) {
        for (size_t index = 0U; index < count; ++index) {
            destination_bytes[index] = source_bytes[index];
        }
        return destination;
    }

    for (size_t index = count; index > 0U; --index) {
        destination_bytes[index - 1U] = source_bytes[index - 1U];
    }
    return destination;
}

[[noreturn]] inline void exit(int status) noexcept {
    xinim::userland::x86_64::exit(status);
}

inline uint32_t getpid() noexcept {
    return static_cast<uint32_t>(xinim::userland::x86_64::getpid());
}

inline uint32_t access(const char* path, int mode) noexcept {
    return static_cast<uint32_t>(xinim::userland::x86_64::access(path, mode));
}

inline uint32_t open(const char* path, uint32_t flags = 0, uint32_t mode = 0) noexcept {
    return static_cast<uint32_t>(
        xinim::userland::x86_64::open(path, flags, mode));
}

inline uint32_t close(int fd) noexcept {
    return static_cast<uint32_t>(xinim::userland::x86_64::close(fd));
}

inline uint32_t chdir(const char* path) noexcept {
    return static_cast<uint32_t>(xinim::userland::x86_64::chdir(path));
}

inline uint32_t getcwd(char* buffer, uint32_t size) noexcept {
    return static_cast<uint32_t>(xinim::userland::x86_64::getcwd(buffer, size));
}

inline uint32_t fork() noexcept {
    return static_cast<uint32_t>(xinim::userland::x86_64::fork());
}

inline uint32_t execve(const char* path, char* const* argv, char* const* envp) noexcept {
    return static_cast<uint32_t>(xinim::userland::x86_64::execve(path, argv, envp));
}

inline uint32_t wait4(int pid, int* status, int options, void* rusage) noexcept {
    return static_cast<uint32_t>(xinim::userland::x86_64::wait4(pid, status, options, rusage));
}
}
#elif defined(__i686__)
// i686 fast path: SYSENTER stubs in the same xinim::userland::x86_32 namespace.
#include "xinim/userland/syscall_i686.hpp"
#else
#include "xinim/userland/syscall_i386.hpp"
#endif

#ifndef XINIM_BOOT_LANE_NAME
#define XINIM_BOOT_LANE_NAME "i486"
#endif

namespace {

// POSIX-shell development lane: tokenize, expand, and dispatch shell primitives.
constexpr char kBanner[] =
    "xash " XINIM_BOOT_LANE_NAME " Ring 3 shell\r\n"
    "type 'help' for commands\r\n";
constexpr char kPrompt[] = "xash$ ";
constexpr char kNewline[] = "\r\n";
constexpr uint32_t kLineBufferSize = 128U;
constexpr uint32_t kPathBufferSize = 128U;
constexpr uint32_t kIoBufferSize = 64U;
constexpr uint32_t kViBufferSize = 256U;
constexpr uint32_t kMaxArgs = 16U;
constexpr uint32_t kMaxEnvEntries = 16U;
constexpr uint32_t kMaxEnvEntrySize = 128U;
constexpr uint32_t kTokenStorageSize = 2048U;
constexpr uint32_t kOpenFlagRDOnly = 0x0000U;
constexpr uint32_t kOpenFlagWROnly = 0x0001U;
constexpr uint32_t kOpenFlagCreat = 0x0040U;
constexpr uint32_t kOpenFlagTrunc = 0x0200U;

char g_line[kLineBufferSize]{};
char g_path[kPathBufferSize]{};
char g_io[kIoBufferSize]{};
char g_vi[kViBufferSize]{};
char* g_argv[kMaxArgs]{};
char* g_envp_current[kMaxEnvEntries + 1]{};
char g_env_storage[kMaxEnvEntries][kMaxEnvEntrySize]{};
char g_token_storage[kTokenStorageSize]{};
char g_current_path[kLineBufferSize]{};
struct EnvironmentFrame {
    uint32_t count;
    char entries[kMaxEnvEntries][kMaxEnvEntrySize];
};

void copy_environment_frame(EnvironmentFrame& destination,
                           const EnvironmentFrame& source) noexcept;
uint32_t g_token_storage_used = 0U;
uint32_t g_env_count = 0U;
uint32_t g_last_status = 0U;

[[nodiscard]] bool set_environment_entry(const char* assignment) noexcept;
[[nodiscard]] bool is_assignment_token(const char* token) noexcept;
void write_string(const char* text) noexcept;
void write_line(const char* text) noexcept;

[[nodiscard]] uint32_t string_length(const char* text) noexcept {
    if (text == nullptr) {
        return 0U;
    }
    uint32_t length = 0U;
    while (text[length] != '\0') {
        ++length;
    }
    return length;
}

void write_char(char value) noexcept {
    static_cast<void>(xinim::userland::x86_32::write(1, &value, 1U));
}

void write_string(const char* text) noexcept {
    const uint32_t length = string_length(text);
    if (length == 0U) {
        return;
    }
    static_cast<void>(xinim::userland::x86_32::write(1, text, length));
}

void write_line(const char* text) noexcept {
    write_string(text);
    write_string(kNewline);
}

bool copy_string(char* destination, uint32_t capacity, const char* source) noexcept {
    if (destination == nullptr || source == nullptr || capacity == 0U) {
        return false;
    }
    uint32_t index = 0U;
    while (source[index] != '\0' && index + 1U < capacity) {
        destination[index] = source[index];
        ++index;
    }
    if (source[index] != '\0') {
        return false;
    }
    destination[index] = '\0';
    return true;
}

void write_dec(uint32_t value) noexcept {
    if (value == 0U) {
        write_char('0');
        return;
    }

    char buffer[10];
    uint32_t index = 0U;
    while (value != 0U) {
        buffer[index] = static_cast<char>('0' + (value % 10U));
        value /= 10U;
        ++index;
    }

    while (index > 0U) {
        --index;
        write_char(buffer[index]);
    }
}

[[nodiscard]] bool string_equals(const char* lhs, const char* rhs) noexcept {
    if (lhs == nullptr || rhs == nullptr) {
        return lhs == rhs;
    }

    uint32_t index = 0U;
    while (lhs[index] != '\0' && rhs[index] != '\0') {
        if (lhs[index] != rhs[index]) {
            return false;
        }
        ++index;
    }
    return lhs[index] == rhs[index];
}

[[nodiscard]] bool string_contains(const char* text, char needle) noexcept {
    if (text == nullptr) {
        return false;
    }

    while (*text != '\0') {
        if (*text == needle) {
            return true;
        }
        ++text;
    }
    return false;
}

[[nodiscard]] bool is_space(char ch) noexcept {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

[[nodiscard]] char* skip_spaces(char* text) noexcept {
    if (text == nullptr) {
        return nullptr;
    }
    while (is_space(*text)) {
        ++text;
    }
    return text;
}

[[nodiscard]] bool is_env_name_char(char ch) noexcept {
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
           (ch >= '0' && ch <= '9') || ch == '_';
}

[[nodiscard]] bool is_env_name_first_char(char ch) noexcept {
    return is_env_name_char(ch) && !(ch >= '0' && ch <= '9');
}

void reset_token_storage() noexcept {
    g_token_storage_used = 0U;
    g_token_storage[0] = '\0';
}

[[nodiscard]] char* alloc_token(uint32_t length) noexcept {
    if (length == 0U || g_token_storage_used + length >= kTokenStorageSize) {
        return nullptr;
    }
    char* storage = &g_token_storage[g_token_storage_used];
    g_token_storage_used += length;
    storage[0] = '\0';
    return storage;
}

[[nodiscard]] bool append_char_to_buffer(char* buffer,
                                        uint32_t capacity,
                                        uint32_t& length,
                                        char value) noexcept {
    if (length + 1U >= capacity) {
        return false;
    }
    buffer[length] = value;
    ++length;
    buffer[length] = '\0';
    return true;
}

[[nodiscard]] bool append_string_to_buffer(char* buffer,
                                          uint32_t capacity,
                                          uint32_t& length,
                                          const char* value) noexcept {
    if (value == nullptr) {
        return true;
    }
    for (uint32_t index = 0U; value[index] != '\0'; ++index) {
        if (!append_char_to_buffer(buffer, capacity, length, value[index])) {
            return false;
        }
    }
    return true;
}

void clear_argv() noexcept {
    for (uint32_t index = 0U; index < kMaxArgs; ++index) {
        g_argv[index] = nullptr;
    }
}

void emit_environment_vector() noexcept {
    g_env_count = 0U;
    for (uint32_t index = 0U; index < kMaxEnvEntries + 1U; ++index) {
        g_envp_current[index] = nullptr;
    }
}

void sync_environment_vector() noexcept {
    for (uint32_t index = 0U; index < g_env_count && index < kMaxEnvEntries; ++index) {
        g_envp_current[index] = g_env_storage[index];
    }
    g_envp_current[g_env_count] = nullptr;
}

void snapshot_environment(EnvironmentFrame& frame) noexcept {
    frame.count = g_env_count;
    for (uint32_t index = 0U; index < kMaxEnvEntries; ++index) {
        frame.entries[index][0] = '\0';
        if (index < g_env_count) {
            copy_string(frame.entries[index], kMaxEnvEntrySize, g_env_storage[index]);
        }
    }
}

void restore_environment(const EnvironmentFrame& frame) noexcept {
    for (uint32_t index = 0U; index < kMaxEnvEntries; ++index) {
        if (index < frame.count && frame.entries[index][0] != '\0') {
            copy_string(g_env_storage[index], kMaxEnvEntrySize, frame.entries[index]);
        } else {
            g_env_storage[index][0] = '\0';
        }
    }
    g_env_count = frame.count > kMaxEnvEntries ? kMaxEnvEntries : frame.count;
    sync_environment_vector();
}

void copy_environment_frame(EnvironmentFrame& destination,
                           const EnvironmentFrame& source) noexcept {
    destination.count = source.count;
    for (uint32_t index = 0U; index < kMaxEnvEntries; ++index) {
        copy_string(destination.entries[index], kMaxEnvEntrySize, source.entries[index]);
    }
}

[[nodiscard]] bool apply_environment_entry(EnvironmentFrame& frame,
                                          const char* assignment) noexcept {
    if (assignment == nullptr || !is_assignment_token(assignment)) {
        return false;
    }

    uint32_t key_length = 0U;
    while (assignment[key_length] != '\0' && assignment[key_length] != '=') {
        ++key_length;
    }

    for (uint32_t index = 0U; index < frame.count; ++index) {
        uint32_t match = 0U;
        while (match < key_length &&
               frame.entries[index][match] == assignment[match]) {
            ++match;
        }
        if (match == key_length && frame.entries[index][match] == '=') {
            return copy_string(frame.entries[index], kMaxEnvEntrySize, assignment);
        }
    }

    if (frame.count >= kMaxEnvEntries) {
        return false;
    }
    if (!copy_string(frame.entries[frame.count], kMaxEnvEntrySize, assignment)) {
        return false;
    }
    ++frame.count;
    return true;
}

bool is_assignment_token(const char* token) noexcept {
    if (token == nullptr || token[0] == '\0') {
        return false;
    }
    if (!is_env_name_first_char(token[0])) {
        return false;
    }

    uint32_t index = 1U;
    while (token[index] != '\0' && token[index] != '=') {
        if (!is_env_name_char(token[index])) {
            return false;
        }
        ++index;
    }
    return token[index] == '=';
}

const char* find_environment_value(const char* key) noexcept {
    if (key == nullptr) {
        return nullptr;
    }
    const uint32_t key_length = string_length(key);
    for (uint32_t index = 0U; index < g_env_count; ++index) {
        const char* entry = g_env_storage[index];
        uint32_t match = 0U;
        while (match < key_length && entry[match] == key[match]) {
            ++match;
        }
        if (match == key_length && entry[match] == '=') {
            return entry + key_length + 1U;
        }
    }
    return nullptr;
}

void initialize_environment(char** envp) noexcept {
    emit_environment_vector();
    if (envp == nullptr) {
        static const char* kDefaults[] = {"PATH=/bin", "SHELL=/bin/xash", nullptr};
        for (uint32_t index = 0U; kDefaults[index] != nullptr; ++index) {
            (void)set_environment_entry(kDefaults[index]);
        }
        sync_environment_vector();
        return;
    }

    for (uint32_t index = 0U; index < kMaxEnvEntries && envp[index] != nullptr; ++index) {
        (void)set_environment_entry(envp[index]);
    }
    sync_environment_vector();
}

bool set_environment_entry(const char* assignment) noexcept {
    if (assignment == nullptr) {
        return false;
    }
    if (!is_assignment_token(assignment)) {
        return false;
    }
    uint32_t key_length = 0U;
    while (assignment[key_length] != '\0' && assignment[key_length] != '=') {
        ++key_length;
    }

    for (uint32_t index = 0U; index < g_env_count; ++index) {
        uint32_t match = 0U;
        const char* entry = g_env_storage[index];
        while (match < key_length && entry[match] == assignment[match]) {
            ++match;
        }
        if (match == key_length && entry[match] == '=') {
            return copy_string(g_env_storage[index], kMaxEnvEntrySize, assignment);
        }
    }

    if (g_env_count >= kMaxEnvEntries) {
        return false;
    }
    if (!copy_string(g_env_storage[g_env_count], kMaxEnvEntrySize, assignment)) {
        return false;
    }
    ++g_env_count;
    sync_environment_vector();
    return true;
}

bool unset_environment_entry(const char* name) noexcept {
    if (name == nullptr || !is_env_name_first_char(name[0])) {
        return false;
    }
    for (uint32_t index = 0U; index < g_env_count; ++index) {
        uint32_t name_length = 0U;
        while (name[name_length] != '\0' && name[name_length] != '=') {
            if (!is_env_name_char(name[name_length])) {
                return false;
            }
            ++name_length;
        }
        if (name[name_length] != '\0') {
            return false;
        }
        uint32_t match = 0U;
        while (match < name_length && g_env_storage[index][match] == name[match]) {
            ++match;
        }
        if (match == name_length && g_env_storage[index][match] == '=') {
            for (uint32_t mover = index; mover + 1U < g_env_count; ++mover) {
                for (uint32_t c = 0U; c < kMaxEnvEntrySize; ++c) {
                    g_env_storage[mover][c] = g_env_storage[mover + 1U][c];
                }
            }
            g_env_storage[g_env_count - 1U][0] = '\0';
            --g_env_count;
            sync_environment_vector();
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool is_position_unquoted(const char* text, const char* position) noexcept {
    bool single = false;
    bool dquote = false;
    bool escaped = false;
    for (const char* cursor = text; cursor < position; ++cursor) {
        const char ch = *cursor;
        if (escaped) {
            escaped = false;
            continue;
        }
        if (!single && ch == '\\') {
            escaped = true;
            continue;
        }
        if (!single && ch == '"') {
            dquote = !dquote;
            continue;
        }
        if (!dquote && ch == '\'') {
            single = !single;
            continue;
        }
    }
    return !single && !dquote && !escaped;
}

[[nodiscard]] char* find_segment_end(char* text) noexcept {
    bool single = false;
    bool dquote = false;
    bool escaped = false;
    char* cursor = text;
    while (*cursor != '\0') {
        const char ch = *cursor;
        if (escaped) {
            escaped = false;
            ++cursor;
            continue;
        }
        if (!single && ch == '\\') {
            escaped = true;
            ++cursor;
            continue;
        }
        if (!single && ch == '"') {
            dquote = !dquote;
            ++cursor;
            continue;
        }
        if (!dquote && ch == '\'') {
            single = !single;
            ++cursor;
            continue;
        }
        if (!single && !dquote && ch == ';') {
            break;
        }
        ++cursor;
    }
    return cursor;
}

[[nodiscard]] bool strip_trailing_ampersand(char* segment, bool& background) noexcept {
    background = false;
    if (segment == nullptr) {
        return false;
    }

    uint32_t length = string_length(segment);
    while (length > 0U && is_space(segment[length - 1U])) {
        --length;
    }
    if (length == 0U || segment[length - 1U] != '&') {
        return false;
    }

    char* ampersand = &segment[length - 1U];
    if (!is_position_unquoted(segment, ampersand)) {
        return false;
    }
    uint32_t amp_index = static_cast<uint32_t>(ampersand - segment);
    if (amp_index == 0U) {
        return false;
    }
    segment[amp_index] = '\0';
    background = true;
    return true;
}

[[nodiscard]] bool append_u32_to_buffer(
    char* buffer,
    uint32_t capacity,
    uint32_t& length,
    uint32_t value) noexcept {
    char temporary[11];
    uint32_t temporary_length = 0U;
    if (value == 0U) {
        return append_char_to_buffer(buffer, capacity, length, '0');
    }

    while (value != 0U && temporary_length < sizeof(temporary)) {
        temporary[temporary_length] = static_cast<char>('0' + (value % 10U));
        value /= 10U;
        ++temporary_length;
    }
    while (temporary_length > 0U) {
        --temporary_length;
        if (!append_char_to_buffer(
                buffer, capacity, length, temporary[temporary_length])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] const char* find_environment_value_by_range(
    const char* name,
    uint32_t length) noexcept {
    if (name == nullptr || length == 0U) {
        return nullptr;
    }
    for (uint32_t index = 0U; index < g_env_count; ++index) {
        uint32_t match = 0U;
        while (match < length && g_env_storage[index][match] == name[match]) {
            ++match;
        }
        if (match == length && g_env_storage[index][match] == '=') {
            return g_env_storage[index] + length + 1U;
        }
    }
    return nullptr;
}

[[nodiscard]] bool tokenize_segment(char* segment,
                                   char* arguments[],
                                   uint32_t& argument_count,
                                   bool& unsupported_syntax) noexcept {
    unsupported_syntax = false;
    argument_count = 0U;
    clear_argv();
    reset_token_storage();

    char* cursor = segment;
    while (*cursor != '\0') {
        cursor = skip_spaces(cursor);
        if (*cursor == '\0') {
            break;
        }

        if (argument_count + 1U >= kMaxArgs) {
            return false;
        }

        char* token = alloc_token(kLineBufferSize);
        if (token == nullptr) {
            return false;
        }
        uint32_t token_length = 0U;
        bool in_single = false;
        bool in_double = false;
        bool escaped = false;

        while (*cursor != '\0') {
            char ch = *cursor;
            if (escaped) {
                if (!append_char_to_buffer(token, kLineBufferSize, token_length, ch)) {
                    return false;
                }
                escaped = false;
                ++cursor;
                continue;
            }

            if (!in_single && ch == '\\') {
                escaped = true;
                ++cursor;
                continue;
            }
            if (!in_single && ch == '"') {
                in_double = !in_double;
                ++cursor;
                continue;
            }
            if (!in_double && ch == '\'') {
                in_single = !in_single;
                ++cursor;
                continue;
            }
            if (!in_single && !in_double) {
                if (ch == '&' || ch == '|' || ch == '<' || ch == '>' ) {
                    unsupported_syntax = true;
                    return false;
                }
                if (is_space(ch)) {
                    break;
                }
                if (ch == ';') {
                    break;
                }
            }

            if (ch == '$' && !in_single) {
                ++cursor;
                if (*cursor == '\0') {
                    if (!append_char_to_buffer(token, kLineBufferSize, token_length, '$')) {
                        return false;
                    }
                    continue;
                }

                if (*cursor == '$') {
                    if (!append_u32_to_buffer(token, kLineBufferSize, token_length,
                                              xinim::userland::x86_32::getpid())) {
                        return false;
                    }
                    ++cursor;
                    continue;
                }

                if (*cursor == '?') {
                    if (!append_u32_to_buffer(token, kLineBufferSize, token_length,
                                              g_last_status)) {
                        return false;
                    }
                    ++cursor;
                    continue;
                }

                if (*cursor == '{') {
                    ++cursor;
                    uint32_t var_length = 0U;
                    char variable[32];
                    while (*cursor != '\0' && *cursor != '}' && var_length + 1U < sizeof(variable)) {
                        variable[var_length] = *cursor;
                        ++cursor;
                        ++var_length;
                    }
                    if (*cursor == '}') {
                        ++cursor;
                        variable[var_length] = '\0';
                        const char* value = find_environment_value_by_range(variable, var_length);
                        if (!append_string_to_buffer(token, kLineBufferSize, token_length, value)) {
                            return false;
                        }
                        continue;
                    }
                    if (!append_char_to_buffer(token, kLineBufferSize, token_length, '$') ||
                        !append_char_to_buffer(token, kLineBufferSize, token_length, '{') ||
                        !append_string_to_buffer(token, kLineBufferSize, token_length, variable)) {
                        return false;
                    }
                    continue;
                }

                if (!is_env_name_first_char(*cursor)) {
                    if (!append_char_to_buffer(token, kLineBufferSize, token_length, '$')) {
                        return false;
                    }
                    continue;
                }

                char variable[32];
                uint32_t var_length = 0U;
                while (is_env_name_char(*cursor) && var_length + 1U < sizeof(variable)) {
                    variable[var_length] = *cursor;
                    ++cursor;
                    ++var_length;
                }
                variable[var_length] = '\0';
                const char* value = find_environment_value_by_range(variable, var_length);
                if (!append_string_to_buffer(token, kLineBufferSize, token_length, value)) {
                    return false;
                }
                continue;
            }

            if (!append_char_to_buffer(token, kLineBufferSize, token_length, ch)) {
                return false;
            }
            ++cursor;
        }

        token[token_length] = '\0';
        arguments[argument_count] = token;
        ++argument_count;
        arguments[argument_count] = nullptr;

        while (is_space(*cursor)) {
            ++cursor;
        }
    }
    arguments[argument_count] = nullptr;
    return argument_count != 0U;
}

[[nodiscard]] bool is_syscall_error(uint32_t value) noexcept {
    return value == static_cast<uint32_t>(-1);
}

[[nodiscard]] uint32_t decode_wait_status(int status) noexcept {
    const uint32_t encoded = static_cast<uint32_t>(status);
    if ((encoded & 0x7FU) == 0U) {
        return (encoded >> 8U) & 0xFFU;
    }
    return 128U + (encoded & 0x7FU);
}

void write_prompt() noexcept {
    write_string(kPrompt);
}

void print_help() noexcept {
    write_line("Built-in commands:");
    write_line("  help                     show this help");
    write_line("  ls [PATH]                list directory contents");
    write_line("  pid                      print current process id");
    write_line("  pwd                      print current working directory");
    write_line("  cd PATH                  change current working directory");
    write_line("  status                   print last command exit status");
    write_line("  export [NAME=VALUE] ...  set environment entries");
    write_line("  env                      print environment");
    write_line("  unset NAME ...           remove environment entries");
    write_line("  echo [TEXT]              print text, expands $$ and $? tokens");
    write_line("  check PATH               test path access");
    write_line("  test -f PATH             test file exists");
    write_line("  command -v NAME          resolve a command path");
    write_line("  cp SRC DST               copy file (supports /bin and /tmp)");
    write_line("  vi FILE                  tiny line editor (writes /tmp files too)");
    write_line("  cat PATH                 print a bootfs file");
    write_line("  exit [N]                 exit with status");
}

void print_ro_root_paths() noexcept {
    write_line("/");
    write_line("/bin");
    write_line("/boot");
    write_line("/etc");
}

void list_directory(const char* path) noexcept;

uint32_t read_command() noexcept {
    const uint32_t result = xinim::userland::x86_32::read(0, g_line, kLineBufferSize - 1U);
    if (result == 0U || is_syscall_error(result)) {
        write_line("read failed");
        return 0U;
    }

    uint32_t length = result;
    if (length >= kLineBufferSize) {
        length = kLineBufferSize - 1U;
    }
    g_line[length] = '\0';

    while (length > 0U) {
        const char ch = g_line[length - 1U];
        if (ch != '\r' && ch != '\n') {
            break;
        }
        --length;
        g_line[length] = '\0';
    }
    write_string(kNewline);
    return length;
}

void print_pwd() noexcept {
    const uint32_t result = xinim::userland::x86_32::getcwd(g_path, kPathBufferSize);
    if (is_syscall_error(result)) {
        g_last_status = 1U;
        write_line("getcwd failed");
        return;
    }
    g_last_status = 0U;
    g_path[kPathBufferSize - 1U] = '\0';
    write_line(g_path);
}

void print_test_result(bool result) noexcept {
    write_line(result ? "true" : "false");
}

void cat_file(const char* path) noexcept {
    const uint32_t fd = xinim::userland::x86_32::open(path);
    if (is_syscall_error(fd)) {
        g_last_status = 1U;
        write_string("cat ");
        write_string(path);
        write_line(": missing");
        return;
    }

    bool read_failed = false;
    for (;;) {
        const uint32_t read_result = xinim::userland::x86_32::read(static_cast<int>(fd), g_io, kIoBufferSize);
        if (read_result == 0U || is_syscall_error(read_result)) {
            if (is_syscall_error(read_result)) {
                read_failed = true;
            }
            break;
        }
        static_cast<void>(xinim::userland::x86_32::write(1, g_io, read_result));
    }
    static_cast<void>(xinim::userland::x86_32::close(static_cast<int>(fd)));
    g_last_status = read_failed ? 1U : 0U;
    write_string(kNewline);
}

void list_directory(const char* path) noexcept {
    const uint32_t fd = xinim::userland::x86_32::open(path, kOpenFlagRDOnly, 0U);
    if (is_syscall_error(fd)) {
        g_last_status = 1U;
        write_line("ls: unavailable");
        return;
    }

    bool read_failed = false;
    for (;;) {
        const uint32_t result = xinim::userland::x86_32::read(
            static_cast<int>(fd),
            g_io,
            kIoBufferSize - 1U);
        if (result == 0U) {
            break;
        }
        if (is_syscall_error(result)) {
            read_failed = true;
            break;
        }
        for (uint32_t index = 0U; index < result; ++index) {
            write_char(g_io[index]);
        }
    }
    static_cast<void>(xinim::userland::x86_32::close(static_cast<int>(fd)));

    g_last_status = read_failed ? 1U : 0U;
}

void copy_file(const char* source, const char* destination) noexcept {
    if (source == nullptr || destination == nullptr) {
        g_last_status = 1U;
        write_line("cp: missing arguments");
        return;
    }

    const uint32_t source_fd = xinim::userland::x86_32::open(
        source,
        kOpenFlagRDOnly,
        0U);
    if (is_syscall_error(source_fd)) {
        g_last_status = 1U;
        write_line("cp: failed to open source");
        return;
    }

    const uint32_t destination_fd = xinim::userland::x86_32::open(
        destination,
        kOpenFlagWROnly | kOpenFlagCreat | kOpenFlagTrunc,
        0644U);
    if (is_syscall_error(destination_fd)) {
        g_last_status = 1U;
        static_cast<void>(xinim::userland::x86_32::close(static_cast<int>(source_fd)));
        write_line("cp: failed to open destination");
        return;
    }

    bool failed = false;
    for (;;) {
        const uint32_t read_result = xinim::userland::x86_32::read(
            static_cast<int>(source_fd),
            g_io,
            kIoBufferSize - 1U);
        if (read_result == 0U) {
            break;
        }
        if (is_syscall_error(read_result)) {
            failed = true;
            break;
        }
        uint32_t written_total = 0U;
        while (written_total < read_result) {
            const uint32_t written = xinim::userland::x86_32::write(
                static_cast<int>(destination_fd),
                g_io + written_total,
                read_result - written_total);
            if (is_syscall_error(written) || written == 0U) {
                failed = true;
                break;
            }
            written_total += written;
        }
        if (failed) {
            break;
        }
    }

    static_cast<void>(xinim::userland::x86_32::close(static_cast<int>(source_fd)));
    static_cast<void>(xinim::userland::x86_32::close(static_cast<int>(destination_fd)));
    g_last_status = failed ? 1U : 0U;
    if (failed) {
        write_line("cp: transfer error");
    }
}

void vi_edit_file(const char* path) noexcept {
    if (path == nullptr || path[0] == '\0') {
        g_last_status = 1U;
        write_line("vi: missing file path");
        return;
    }

    write_line("vi: line mode - enter text, line with only '.' to save and exit");
    char current[kViBufferSize];
    bool failed = false;

    const uint32_t existing_fd = xinim::userland::x86_32::open(
        path,
        kOpenFlagRDOnly,
        0U);
    if (!is_syscall_error(existing_fd)) {
        for (;;) {
            const uint32_t result = xinim::userland::x86_32::read(
                static_cast<int>(existing_fd),
                current,
                sizeof(current) - 1U);
            if (is_syscall_error(result) || result == 0U) {
                break;
            }
            for (uint32_t index = 0U; index < result; ++index) {
                write_char(current[index]);
            }
            if (result < sizeof(current) - 1U) {
                break;
            }
        }
        static_cast<void>(xinim::userland::x86_32::close(static_cast<int>(existing_fd)));
    }

    const uint32_t destination_fd = xinim::userland::x86_32::open(
        path,
        kOpenFlagWROnly | kOpenFlagCreat | kOpenFlagTrunc,
        0644U);
    if (is_syscall_error(destination_fd)) {
        g_last_status = 1U;
        write_line("vi: failed to create/write file");
        return;
    }

    for (;;) {
        write_string("vi> ");
        const uint32_t length = read_command();
        if (is_syscall_error(length)) {
            failed = true;
            break;
        }
        if (string_length(g_line) == 1U && g_line[0] == '.') {
            break;
        }
        uint32_t payload = length;
        if (payload + 1U < sizeof(g_vi)) {
            g_vi[length] = '\n';
            ++payload;
        } else {
            payload = sizeof(g_vi) - 1U;
        }
        for (uint32_t index = 0U; index < payload; ++index) {
            g_vi[index] = (index < length) ? g_line[index] : '\n';
        }

        g_vi[payload] = '\0';
        const uint32_t written = xinim::userland::x86_32::write(
            static_cast<int>(destination_fd),
            g_vi,
            payload);
        if (is_syscall_error(written) || written != payload) {
            failed = true;
            break;
        }
    }

    static_cast<void>(xinim::userland::x86_32::close(static_cast<int>(destination_fd)));
    g_last_status = failed ? 1U : 0U;
    if (failed) {
        write_line("vi: write failed");
    }
}

[[nodiscard]] bool resolve_command_path(
    const char* name,
    char* buffer,
    uint32_t capacity) noexcept {
    if (name == nullptr || buffer == nullptr || capacity == 0U) {
        return false;
    }

    if (name[0] == '\0') {
        return false;
    }
    if (string_contains(name, '/')) {
        if (!copy_string(buffer, capacity, name)) {
            return false;
        }
        return xinim::userland::x86_32::access(buffer, 0) == 0U;
    }

    const char* path_variable = find_environment_value("PATH");
    if (path_variable == nullptr || path_variable[0] == '\0') {
        path_variable = "/bin";
    }

    uint32_t path_index = 0U;
    while (path_variable[path_index] != '\0') {
        while (path_variable[path_index] == ':') {
            ++path_index;
        }
        if (path_variable[path_index] == '\0') {
            break;
        }

        g_current_path[0] = '\0';
        while (path_variable[path_index] != '\0' && path_variable[path_index] != ':') {
            uint32_t path_len = string_length(g_current_path);
            if (path_len + 2U >= kLineBufferSize) {
                break;
            }
            g_current_path[path_len] = path_variable[path_index];
            ++path_len;
            g_current_path[path_len] = '\0';
            ++path_index;
        }

        if (g_current_path[0] == '\0') {
            continue;
        }

        if (!copy_string(buffer, capacity, g_current_path)) {
            return false;
        }
        uint32_t slash_len = string_length(buffer);
        if (slash_len + 1U < capacity && buffer[slash_len - 1U] != '/') {
            buffer[slash_len] = '/';
            buffer[slash_len + 1U] = '\0';
            ++slash_len;
        }
        if (!append_string_to_buffer(buffer, capacity, slash_len, name)) {
            return false;
        }
        if (xinim::userland::x86_32::access(buffer, 0) == 0U) {
            return true;
        }
    }

    if (copy_string(buffer, capacity, "/bin/")) {
        uint32_t fallback_length = string_length(buffer);
        if (!append_string_to_buffer(buffer, capacity, fallback_length, name)) {
            return false;
        }
        return xinim::userland::x86_32::access(buffer, 0) == 0U;
    }
    return false;
}

void print_command_path(const char* name) noexcept {
    if (name == nullptr || name[0] == '\0') {
        g_last_status = 2U;
        write_line("usage: command -v NAME");
        return;
    }

    if (string_equals(name, "help") || string_equals(name, "pid") ||
        string_equals(name, "pwd") || string_equals(name, "status") ||
        string_equals(name, "exit") || string_equals(name, "cd") ||
        string_equals(name, "check") || string_equals(name, "ls") ||
        string_equals(name, "cp") || string_equals(name, "vi") ||
        string_equals(name, "test") || string_equals(name, "command") ||
        string_equals(name, "cat") || string_equals(name, "echo") ||
        string_equals(name, "export") || string_equals(name, "env") ||
        string_equals(name, "unset")) {
        g_last_status = 0U;
        write_string(name);
        write_line(": builtin");
        return;
    }

    if (resolve_command_path(name, g_path, kPathBufferSize)) {
        g_last_status = 0U;
        write_line(g_path);
        return;
    }
    g_last_status = 1U;
    write_line("not found");
}

void run_external_command(char* command, bool background) noexcept {
    (void)background;
    if (background) {
        write_line("background execution is not supported yet");
        g_last_status = 1U;
        return;
    }

    if (!resolve_command_path(command, g_path, kPathBufferSize)) {
        g_last_status = 127U;
        write_string("unknown command: ");
        write_line(command);
        return;
    }

    const uint32_t pid = xinim::userland::x86_32::fork();
    if (pid == static_cast<uint32_t>(-1)) {
        g_last_status = 255U;
        write_line("fork failed");
        return;
    }
    if (pid == 0U) {
        static_cast<void>(xinim::userland::x86_32::execve(g_path, g_argv, g_envp_current));
        write_string("execve failed: ");
        write_line(g_path);
        xinim::userland::x86_32::exit(127);
    }

    int status = 0;
    const uint32_t waited = xinim::userland::x86_32::wait4(static_cast<int>(pid), &status, 0, nullptr);
    if (waited == static_cast<uint32_t>(-1)) {
        g_last_status = 255U;
        write_line("wait4 failed");
        return;
    }
    g_last_status = decode_wait_status(status);
}

void list_env() noexcept {
    for (uint32_t index = 0U; index < g_env_count; ++index) {
        write_line(g_env_storage[index]);
    }
    if (g_env_count == 0U) {
        write_line("(empty)");
    }
}

[[nodiscard]] bool execute_builtin(char* argv0,
                                  uint32_t argc,
                                  bool& done) noexcept {
    done = true;
    if (string_equals(argv0, "help")) {
        g_last_status = 0U;
        print_help();
        return true;
    }

    if (string_equals(argv0, "pid")) {
        g_last_status = 0U;
        write_string("pid: ");
        write_dec(xinim::userland::x86_32::getpid());
        write_string(kNewline);
        return true;
    }

    if (string_equals(argv0, "pwd")) {
        print_pwd();
        return true;
    }

    if (string_equals(argv0, "status")) {
        write_string("status: ");
        write_dec(g_last_status);
        write_string(kNewline);
        return true;
    }

    if (string_equals(argv0, "exit")) {
        uint32_t status = g_last_status;
        if (argc > 1U && g_argv[1] != nullptr) {
            uint32_t parsed = 0U;
            char* text = g_argv[1];
            if (*text == '-') {
                ++text;
            }
            while (*text >= '0' && *text <= '9') {
                parsed = parsed * 10U + static_cast<uint32_t>(*text - '0');
                ++text;
            }
            status = parsed;
        }
        xinim::userland::x86_32::exit(static_cast<int>(status));
        return true;
    }

    if (string_equals(argv0, "cd")) {
        if (argc < 2U || g_argv[1] == nullptr) {
            g_last_status = 2U;
            write_line("usage: cd PATH");
            return true;
        }
        const uint32_t result = xinim::userland::x86_32::chdir(g_argv[1]);
        g_last_status = result == 0U ? 0U : 1U;
        write_string("cd ");
        write_string(g_argv[1]);
        write_string(": ");
        write_line(result == 0U ? "ok" : "missing");
        return true;
    }

    if (string_equals(argv0, "check")) {
        if (argc < 2U || g_argv[1] == nullptr) {
            g_last_status = 2U;
            write_line("usage: check PATH");
            return true;
        }
        const uint32_t result = xinim::userland::x86_32::access(g_argv[1], 0);
        g_last_status = result == 0U ? 0U : 1U;
        write_string("check ");
        write_string(g_argv[1]);
        write_string(": ");
        write_line(result == 0U ? "ok" : "missing");
        return true;
    }

    if (string_equals(argv0, "ls")) {
        const char* target = (argc < 2U || g_argv[1] == nullptr) ? "/bin" : g_argv[1];
        if (string_equals(target, "/") || string_equals(target, "/.")) {
            print_ro_root_paths();
            g_last_status = 0U;
            return true;
        }
        if (string_equals(target, "bin") || string_equals(target, "./bin") ||
            string_equals(target, "/bin/")) {
            list_directory("/bin");
            g_last_status = 0U;
            return true;
        }
        if (string_equals(target, "tmp") || string_equals(target, "./tmp") ||
            string_equals(target, "/tmp/") || string_equals(target, "/tmp")) {
            list_directory("/tmp");
            return true;
        }
        list_directory(target);
        return true;
    }

    if (string_equals(argv0, "cp")) {
        if (argc < 3U || g_argv[1] == nullptr || g_argv[2] == nullptr) {
            g_last_status = 2U;
            write_line("usage: cp SRC DST");
            return true;
        }
        copy_file(g_argv[1], g_argv[2]);
        return true;
    }

    if (string_equals(argv0, "vi")) {
        if (argc < 2U || g_argv[1] == nullptr) {
            g_last_status = 2U;
            write_line("usage: vi FILE");
            return true;
        }
        vi_edit_file(g_argv[1]);
        return true;
    }

    if (string_equals(argv0, "test")) {
        if (argc < 3U || g_argv[1] == nullptr || g_argv[2] == nullptr) {
            write_line("usage: test -f PATH");
            return true;
        }
        if (!string_equals(g_argv[1], "-f")) {
            g_last_status = 2U;
            write_line("usage: test -f PATH");
            return true;
        }
        const bool exists = xinim::userland::x86_32::access(g_argv[2], 0) == 0U;
        g_last_status = exists ? 0U : 1U;
        print_test_result(exists);
        return true;
    }

    if (string_equals(argv0, "command")) {
        if (argc < 3U || g_argv[1] == nullptr || !string_equals(g_argv[1], "-v") ||
            g_argv[2] == nullptr) {
            g_last_status = 2U;
            write_line("usage: command -v NAME");
            return true;
        }
        print_command_path(g_argv[2]);
        return true;
    }

    if (string_equals(argv0, "cat")) {
        if (argc < 2U || g_argv[1] == nullptr) {
            g_last_status = 2U;
            write_line("usage: cat PATH");
            return true;
        }
        cat_file(g_argv[1]);
        return true;
    }

    if (string_equals(argv0, "echo")) {
        for (uint32_t index = 1U; index < argc; ++index) {
            if (index != 1U) {
                write_char(' ');
            }
            write_string(g_argv[index]);
        }
        g_last_status = 0U;
        write_string(kNewline);
        return true;
    }

    if (string_equals(argv0, "export")) {
        if (argc == 1U) {
            list_env();
            g_last_status = 0U;
            return true;
        }
        for (uint32_t index = 1U; index < argc; ++index) {
            if (g_argv[index] == nullptr) {
                break;
            }
            if (!is_assignment_token(g_argv[index])) {
                g_last_status = 2U;
                write_line("export: assignment required");
                return true;
            }
            if (!set_environment_entry(g_argv[index])) {
                g_last_status = 1U;
                write_string("export: failed ");
                write_line(g_argv[index]);
                return true;
            }
        }
        g_last_status = 0U;
        return true;
    }

    if (string_equals(argv0, "env")) {
        list_env();
        g_last_status = 0U;
        return true;
    }

    if (string_equals(argv0, "unset")) {
        if (argc < 2U || g_argv[1] == nullptr) {
            g_last_status = 2U;
            write_line("usage: unset NAME");
            return true;
        }
        g_last_status = 0U;
        for (uint32_t index = 1U; index < argc; ++index) {
            if (g_argv[index] == nullptr) {
                continue;
            }
            if (!unset_environment_entry(g_argv[index])) {
                g_last_status = 1U;
            }
        }
        return true;
    }

    done = false;
    return false;
}

[[nodiscard]] bool apply_prefix_assignments(uint32_t& argc,
                                            EnvironmentFrame& frame,
                                            bool& has_prefix_assignments) noexcept {
    has_prefix_assignments = false;
    while (argc > 0U && is_assignment_token(g_argv[0])) {
        has_prefix_assignments = true;
        if (!apply_environment_entry(frame, g_argv[0])) {
            return false;
        }
        const uint32_t shift_count = argc;
        for (uint32_t index = 0U; index + 1U < shift_count; ++index) {
            g_argv[index] = g_argv[index + 1U];
        }
        if (shift_count > 0U) {
            g_argv[shift_count - 1U] = nullptr;
            --argc;
            g_argv[argc] = nullptr;
        }
    }
    return true;
}

void run_command_segment(char* segment) noexcept {
    if (segment == nullptr) {
        return;
    }

    bool background = false;
    const bool had_ampersand = strip_trailing_ampersand(segment, background);
    (void)had_ampersand;

    uint32_t argument_count = 0U;
    bool unsupported = false;
    if (!tokenize_segment(segment, g_argv, argument_count, unsupported) || unsupported) {
        g_last_status = 2U;
        write_line(unsupported ? "unsupported syntax in command segment" : "command too long");
        return;
    }
    if (argument_count == 0U) {
        g_last_status = 0U;
        return;
    }

    uint32_t effective_argument_count = argument_count;

    EnvironmentFrame original_environment;
    snapshot_environment(original_environment);

    EnvironmentFrame command_environment;
    copy_environment_frame(command_environment, original_environment);
    bool used_temporary_environment = false;
    if (!apply_prefix_assignments(effective_argument_count,
                                  command_environment,
                                  used_temporary_environment)) {
        g_last_status = 1U;
        write_line("too many environment assignments");
        restore_environment(original_environment);
        return;
    }
    if (used_temporary_environment) {
        restore_environment(command_environment);
    }

    if (effective_argument_count == 0U) {
        g_last_status = 0U;
        if (used_temporary_environment) {
            restore_environment(command_environment);
        }
        return;
    }

    bool builtin_done = false;
    (void)execute_builtin(g_argv[0], effective_argument_count, builtin_done);
    if (builtin_done) {
        if (used_temporary_environment) {
            restore_environment(original_environment);
        }
        return;
    }

    run_external_command(g_argv[0], background);
    if (used_temporary_environment) {
        restore_environment(original_environment);
    }
}

void run_command_line() noexcept {
    char* cursor = g_line;
    bool segment_executed = false;
    while (*cursor != '\0') {
        cursor = skip_spaces(cursor);
        if (*cursor == '\0') {
            break;
        }

        char* segment_end = find_segment_end(cursor);
        char* next_segment = segment_end;
        if (*segment_end == ';') {
            *segment_end = '\0';
            next_segment = segment_end + 1;
        }

        run_command_segment(cursor);
        segment_executed = true;
        if (*segment_end == '\0') {
            break;
        }
        cursor = next_segment;
    }
    if (!segment_executed) {
        g_last_status = 0U;
    }
}

} // namespace

extern "C" int xinim_user_main(int argc, char** argv, char** envp) noexcept {
    (void)argc;
    (void)argv;

    initialize_environment(envp);
    write_string(kBanner);
    clear_argv();

    for (;;) {
        write_prompt();
        const uint32_t length = read_command();
        if (length == 0U && g_line[0] == '\0') {
            continue;
        }
        run_command_line();
    }
}
