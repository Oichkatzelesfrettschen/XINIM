#include <stdint.h>

#if defined(__x86_64__)
#include "xinim/userland/syscall_x86_64.hpp"

#include <stddef.h>
namespace xinim::userland::x86_32 {
    using uint32_t = ::uint32_t;
    using uint64_t = ::uint64_t;

    inline uint32_t read(int fd, void *buffer, uint64_t count) noexcept {
        return static_cast<uint32_t>(
            xinim::userland::x86_64::read(static_cast<uint64_t>(fd), buffer, count));
    }

    inline uint32_t write(int fd, const void *buffer, uint64_t count) noexcept {
        return static_cast<uint32_t>(
            xinim::userland::x86_64::write(static_cast<uint64_t>(fd), buffer, count));
    }

    extern "C" void *memmove(void *destination, const void *source, size_t count) noexcept {
        auto *const destination_bytes = static_cast<unsigned char *>(destination);
        const auto *const source_bytes = static_cast<const unsigned char *>(source);

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

    extern "C" void *memset(void *destination, int value, size_t count) noexcept {
        auto *const destination_bytes = static_cast<unsigned char *>(destination);
        const auto byte_value = static_cast<unsigned char>(value);
        for (size_t index = 0U; index < count; ++index) {
            destination_bytes[index] = byte_value;
        }
        return destination;
    }

    [[noreturn]] inline void exit(int status) noexcept {
        xinim::userland::x86_64::exit(status);
    }

    inline uint32_t getpid() noexcept {
        return static_cast<uint32_t>(xinim::userland::x86_64::getpid());
    }

    inline uint32_t access(const char *path, int mode) noexcept {
        return static_cast<uint32_t>(xinim::userland::x86_64::access(path, mode));
    }

    inline uint32_t open(const char *path, uint32_t flags = 0, uint32_t mode = 0) noexcept {
        return static_cast<uint32_t>(xinim::userland::x86_64::open(path, flags, mode));
    }

    inline uint32_t close(int fd) noexcept {
        return static_cast<uint32_t>(xinim::userland::x86_64::close(fd));
    }

    inline uint32_t dup(int old_fd) noexcept {
        return static_cast<uint32_t>(xinim::userland::x86_64::dup(old_fd));
    }

    inline uint32_t dup2(int old_fd, int new_fd) noexcept {
        return static_cast<uint32_t>(xinim::userland::x86_64::dup2(old_fd, new_fd));
    }

    inline uint32_t pipe(int descriptors[2]) noexcept {
        return static_cast<uint32_t>(xinim::userland::x86_64::pipe(descriptors));
    }

    inline uint32_t chdir(const char *path) noexcept {
        return static_cast<uint32_t>(xinim::userland::x86_64::chdir(path));
    }

    inline uint32_t getcwd(char *buffer, uint32_t size) noexcept {
        return static_cast<uint32_t>(xinim::userland::x86_64::getcwd(buffer, size));
    }

    inline uint32_t fork() noexcept {
        return static_cast<uint32_t>(xinim::userland::x86_64::fork());
    }

    inline uint32_t execve(const char *path, char *const *argv, char *const *envp) noexcept {
        return static_cast<uint32_t>(xinim::userland::x86_64::execve(path, argv, envp));
    }

    inline uint32_t wait4(int pid, int *status, int options, void *rusage) noexcept {
        return static_cast<uint32_t>(xinim::userland::x86_64::wait4(pid, status, options, rusage));
    }
} // namespace xinim::userland::x86_32
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
    constexpr char kBanner[] = "xash " XINIM_BOOT_LANE_NAME " Ring 3 shell\r\n"
                               "type 'help' for commands\r\n";
    constexpr char kPrompt[] = "xash$ ";
    constexpr char kNewline[] = "\r\n";
    constexpr uint32_t kLineBufferSize = 128U;
    constexpr uint32_t kPathBufferSize = 128U;
    constexpr uint32_t kIoBufferSize = 64U;
    constexpr uint32_t kViBufferSize = 256U;
    constexpr uint32_t kMaxArgs = 16U;
    constexpr uint32_t kMaxPipelineCommands = 8U;
    constexpr uint32_t kMaxRedirections = 8U;
    constexpr uint32_t kMaxBackgroundProcesses = 32U;
    constexpr uint32_t kMaxEnvEntries = 16U;
    constexpr uint32_t kMaxEnvEntrySize = 128U;
    constexpr uint32_t kTokenStorageSize = 2048U;
    constexpr uint32_t kOpenFlagRDOnly = 0x0000U;
    constexpr uint32_t kOpenFlagWROnly = 0x0001U;
    constexpr uint32_t kOpenFlagCreat = 0x0040U;
    constexpr uint32_t kOpenFlagTrunc = 0x0200U;
    constexpr uint32_t kOpenFlagAppend = 0x0400U;
    constexpr int kWaitNoHang = 1;
    constexpr int kStandardInput = 0;
    constexpr int kStandardOutput = 1;

    char g_line[kLineBufferSize]{};
    char g_path[kPathBufferSize]{};
    char g_io[kIoBufferSize]{};
    char g_vi[kViBufferSize]{};
    char *g_argv[kMaxArgs]{};
    char *g_envp_current[kMaxEnvEntries + 1]{};
    char g_env_storage[kMaxEnvEntries][kMaxEnvEntrySize]{};
    char g_token_storage[kTokenStorageSize]{};
    char g_current_path[kLineBufferSize]{};
    struct EnvironmentFrame {
        uint32_t count;
        char entries[kMaxEnvEntries][kMaxEnvEntrySize];
    };

    enum class RedirectionKind {
        Input,
        OutputTruncate,
        OutputAppend,
    };

    struct Redirection {
        RedirectionKind kind;
        char *path;
    };

    struct SimpleCommand {
        char *arguments[kMaxArgs];
        uint32_t argument_count;
        Redirection redirections[kMaxRedirections];
        uint32_t redirection_count;
    };

    struct Pipeline {
        SimpleCommand commands[kMaxPipelineCommands];
        uint32_t command_count;
        bool background;
    };

    void copy_environment_frame(EnvironmentFrame &destination,
                                const EnvironmentFrame &source) noexcept;
    uint32_t g_token_storage_used = 0U;
    uint32_t g_env_count = 0U;
    uint32_t g_last_status = 0U;
    uint32_t g_background_processes[kMaxBackgroundProcesses]{};
    uint32_t g_background_process_count = 0U;
    uint32_t g_last_background_process = 0U;

    [[nodiscard]] bool set_environment_entry(const char *assignment) noexcept;
    [[nodiscard]] bool is_assignment_token(const char *token) noexcept;
    void write_string(const char *text) noexcept;
    void write_line(const char *text) noexcept;

    [[nodiscard]] uint32_t string_length(const char *text) noexcept {
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

    void write_string(const char *text) noexcept {
        const uint32_t length = string_length(text);
        if (length == 0U) {
            return;
        }
        static_cast<void>(xinim::userland::x86_32::write(1, text, length));
    }

    void write_line(const char *text) noexcept {
        write_string(text);
        write_string(kNewline);
    }

    bool copy_string(char *destination, uint32_t capacity, const char *source) noexcept {
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

    [[nodiscard]] bool string_equals(const char *lhs, const char *rhs) noexcept {
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

    [[nodiscard]] bool string_contains(const char *text, char needle) noexcept {
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

    [[nodiscard]] char *skip_spaces(char *text) noexcept {
        if (text == nullptr) {
            return nullptr;
        }
        while (is_space(*text)) {
            ++text;
        }
        return text;
    }

    [[nodiscard]] bool is_env_name_char(char ch) noexcept {
        return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
               ch == '_';
    }

    [[nodiscard]] bool is_env_name_first_char(char ch) noexcept {
        return is_env_name_char(ch) && !(ch >= '0' && ch <= '9');
    }

    void reset_token_storage() noexcept {
        g_token_storage_used = 0U;
        g_token_storage[0] = '\0';
    }

    [[nodiscard]] char *alloc_token(uint32_t length) noexcept {
        if (length == 0U || g_token_storage_used + length >= kTokenStorageSize) {
            return nullptr;
        }
        char *storage = &g_token_storage[g_token_storage_used];
        g_token_storage_used += length;
        storage[0] = '\0';
        return storage;
    }

    [[nodiscard]] bool append_char_to_buffer(char *buffer, uint32_t capacity, uint32_t &length,
                                             char value) noexcept {
        if (length + 1U >= capacity) {
            return false;
        }
        buffer[length] = value;
        ++length;
        buffer[length] = '\0';
        return true;
    }

    [[nodiscard]] bool append_string_to_buffer(char *buffer, uint32_t capacity, uint32_t &length,
                                               const char *value) noexcept {
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

    void snapshot_environment(EnvironmentFrame &frame) noexcept {
        frame.count = g_env_count;
        for (uint32_t index = 0U; index < kMaxEnvEntries; ++index) {
            frame.entries[index][0] = '\0';
            if (index < g_env_count) {
                copy_string(frame.entries[index], kMaxEnvEntrySize, g_env_storage[index]);
            }
        }
    }

    void restore_environment(const EnvironmentFrame &frame) noexcept {
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

    void copy_environment_frame(EnvironmentFrame &destination,
                                const EnvironmentFrame &source) noexcept {
        destination.count = source.count;
        for (uint32_t index = 0U; index < kMaxEnvEntries; ++index) {
            copy_string(destination.entries[index], kMaxEnvEntrySize, source.entries[index]);
        }
    }

    [[nodiscard]] bool apply_environment_entry(EnvironmentFrame &frame,
                                               const char *assignment) noexcept {
        if (assignment == nullptr || !is_assignment_token(assignment)) {
            return false;
        }

        uint32_t key_length = 0U;
        while (assignment[key_length] != '\0' && assignment[key_length] != '=') {
            ++key_length;
        }

        for (uint32_t index = 0U; index < frame.count; ++index) {
            uint32_t match = 0U;
            while (match < key_length && frame.entries[index][match] == assignment[match]) {
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

    bool is_assignment_token(const char *token) noexcept {
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

    const char *find_environment_value(const char *key) noexcept {
        if (key == nullptr) {
            return nullptr;
        }
        const uint32_t key_length = string_length(key);
        for (uint32_t index = 0U; index < g_env_count; ++index) {
            const char *entry = g_env_storage[index];
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

    void initialize_environment(char **envp) noexcept {
        emit_environment_vector();
        if (envp == nullptr) {
            static const char *kDefaults[] = {"PATH=/bin", "SHELL=/bin/xash", nullptr};
            for (uint32_t index = 0U; kDefaults[index] != nullptr; ++index) {
                (void) set_environment_entry(kDefaults[index]);
            }
            sync_environment_vector();
            return;
        }

        for (uint32_t index = 0U; index < kMaxEnvEntries && envp[index] != nullptr; ++index) {
            (void) set_environment_entry(envp[index]);
        }
        sync_environment_vector();
    }

    bool set_environment_entry(const char *assignment) noexcept {
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
            const char *entry = g_env_storage[index];
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

    bool unset_environment_entry(const char *name) noexcept {
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

    [[nodiscard]] bool is_position_unquoted(const char *text, const char *position) noexcept {
        bool single = false;
        bool dquote = false;
        bool escaped = false;
        for (const char *cursor = text; cursor < position; ++cursor) {
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

    [[nodiscard]] char *find_segment_end(char *text) noexcept {
        bool single = false;
        bool dquote = false;
        bool escaped = false;
        char *cursor = text;
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

    [[nodiscard]] bool strip_trailing_ampersand(char *segment, bool &background) noexcept {
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

        char *ampersand = &segment[length - 1U];
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

    [[nodiscard]] bool append_u32_to_buffer(char *buffer, uint32_t capacity, uint32_t &length,
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
            if (!append_char_to_buffer(buffer, capacity, length, temporary[temporary_length])) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] const char *find_environment_value_by_range(const char *name,
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

    [[nodiscard]] bool tokenize_segment(char *segment, char *arguments[], uint32_t &argument_count,
                                        bool &unsupported_syntax) noexcept {
        unsupported_syntax = false;
        argument_count = 0U;
        for (uint32_t index = 0U; index < kMaxArgs; ++index) {
            arguments[index] = nullptr;
        }

        char *cursor = segment;
        while (*cursor != '\0') {
            cursor = skip_spaces(cursor);
            if (*cursor == '\0') {
                break;
            }

            if (argument_count + 1U >= kMaxArgs) {
                return false;
            }

            if (*cursor == '<' || *cursor == '>') {
                char *operator_token = alloc_token(3U);
                if (operator_token == nullptr) {
                    return false;
                }
                operator_token[0] = *cursor;
                operator_token[1] = '\0';
                if (*cursor == '>' && cursor[1] == '>') {
                    operator_token[1] = '>';
                    operator_token[2] = '\0';
                    ++cursor;
                }
                ++cursor;
                arguments[argument_count] = operator_token;
                ++argument_count;
                arguments[argument_count] = nullptr;
                continue;
            }

            char *token = alloc_token(kLineBufferSize);
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
                    if (ch == '&' || ch == '|') {
                        unsupported_syntax = true;
                        return false;
                    }
                    if (ch == '<' || ch == '>') {
                        break;
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

                    if (*cursor == '!') {
                        if (!append_u32_to_buffer(token, kLineBufferSize, token_length,
                                                  g_last_background_process)) {
                            return false;
                        }
                        ++cursor;
                        continue;
                    }

                    if (*cursor == '{') {
                        ++cursor;
                        uint32_t var_length = 0U;
                        char variable[32];
                        while (*cursor != '\0' && *cursor != '}' &&
                               var_length + 1U < sizeof(variable)) {
                            variable[var_length] = *cursor;
                            ++cursor;
                            ++var_length;
                        }
                        if (*cursor == '}') {
                            ++cursor;
                            variable[var_length] = '\0';
                            const char *value =
                                find_environment_value_by_range(variable, var_length);
                            if (!append_string_to_buffer(token, kLineBufferSize, token_length,
                                                         value)) {
                                return false;
                            }
                            continue;
                        }
                        if (!append_char_to_buffer(token, kLineBufferSize, token_length, '$') ||
                            !append_char_to_buffer(token, kLineBufferSize, token_length, '{') ||
                            !append_string_to_buffer(token, kLineBufferSize, token_length,
                                                     variable)) {
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
                    const char *value = find_environment_value_by_range(variable, var_length);
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

            if (in_single || in_double || escaped) {
                unsupported_syntax = true;
                return false;
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

    [[nodiscard]] bool is_redirection_token(const char *token) noexcept {
        return string_equals(token, "<") || string_equals(token, ">") || string_equals(token, ">>");
    }

    [[nodiscard]] bool parse_simple_command(char *text, SimpleCommand &command,
                                            bool &unsupported_syntax) noexcept {
        command.argument_count = 0U;
        command.redirection_count = 0U;
        uint32_t token_count = 0U;
        if (!tokenize_segment(text, command.arguments, token_count, unsupported_syntax)) {
            return false;
        }

        uint32_t output_count = 0U;
        for (uint32_t index = 0U; index < token_count; ++index) {
            char *const token = command.arguments[index];
            if (!is_redirection_token(token)) {
                command.arguments[output_count++] = token;
                continue;
            }
            if (command.redirection_count >= kMaxRedirections || index + 1U >= token_count ||
                is_redirection_token(command.arguments[index + 1U])) {
                unsupported_syntax = true;
                return false;
            }
            Redirection &redirection = command.redirections[command.redirection_count++];
            if (string_equals(token, "<")) {
                redirection.kind = RedirectionKind::Input;
            } else if (string_equals(token, ">>")) {
                redirection.kind = RedirectionKind::OutputAppend;
            } else {
                redirection.kind = RedirectionKind::OutputTruncate;
            }
            redirection.path = command.arguments[++index];
        }
        for (uint32_t index = output_count; index < kMaxArgs; ++index) {
            command.arguments[index] = nullptr;
        }
        command.argument_count = output_count;
        if (output_count == 0U) {
            unsupported_syntax = true;
            return false;
        }
        return true;
    }

    [[nodiscard]] bool parse_pipeline(char *segment, Pipeline &pipeline,
                                      bool &unsupported_syntax) noexcept {
        pipeline.command_count = 0U;
        pipeline.background = false;
        unsupported_syntax = false;
        if (segment == nullptr) {
            return false;
        }

        static_cast<void>(strip_trailing_ampersand(segment, pipeline.background));
        reset_token_storage();

        char *stages[kMaxPipelineCommands]{};
        uint32_t stage_count = 1U;
        stages[0] = segment;
        bool in_single = false;
        bool in_double = false;
        bool escaped = false;
        for (char *cursor = segment; *cursor != '\0'; ++cursor) {
            const char value = *cursor;
            if (escaped) {
                escaped = false;
                continue;
            }
            if (!in_single && value == '\\') {
                escaped = true;
                continue;
            }
            if (!in_single && value == '"') {
                in_double = !in_double;
                continue;
            }
            if (!in_double && value == '\'') {
                in_single = !in_single;
                continue;
            }
            if (!in_single && !in_double && value == '|') {
                if (stage_count >= kMaxPipelineCommands) {
                    unsupported_syntax = true;
                    return false;
                }
                *cursor = '\0';
                stages[stage_count++] = cursor + 1;
            }
        }
        if (in_single || in_double || escaped) {
            unsupported_syntax = true;
            return false;
        }

        for (uint32_t index = 0U; index < stage_count; ++index) {
            char *const stage = skip_spaces(stages[index]);
            if (stage == nullptr || *stage == '\0' ||
                !parse_simple_command(stage, pipeline.commands[index], unsupported_syntax)) {
                unsupported_syntax = true;
                return false;
            }
        }
        pipeline.command_count = stage_count;
        return true;
    }

    void load_command_arguments(const SimpleCommand &command) noexcept {
        clear_argv();
        for (uint32_t index = 0U; index < command.argument_count; ++index) {
            g_argv[index] = command.arguments[index];
        }
        g_argv[command.argument_count] = nullptr;
    }

    [[nodiscard]] bool is_syscall_error(uint32_t value) noexcept {
        constexpr uint32_t kMaximumErrno = 4095U;
        return value >= static_cast<uint32_t>(-static_cast<int32_t>(kMaximumErrno));
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
        write_line("  jobs                     list running background processes");
        write_line("  wait [PID]               wait for background processes");
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

    void list_directory(const char *path) noexcept;

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

    void cat_file(const char *path) noexcept {
        const bool use_standard_input = path == nullptr;
        const uint32_t fd = use_standard_input ? static_cast<uint32_t>(kStandardInput)
                                               : xinim::userland::x86_32::open(path);
        if (is_syscall_error(fd)) {
            g_last_status = 1U;
            write_string("cat ");
            write_string(path == nullptr ? "stdin" : path);
            write_line(": missing");
            return;
        }

        bool read_failed = false;
        for (;;) {
            const uint32_t read_result =
                xinim::userland::x86_32::read(static_cast<int>(fd), g_io, kIoBufferSize);
            if (read_result == 0U || is_syscall_error(read_result)) {
                if (is_syscall_error(read_result)) {
                    read_failed = true;
                }
                break;
            }
            static_cast<void>(xinim::userland::x86_32::write(1, g_io, read_result));
        }
        if (!use_standard_input) {
            static_cast<void>(xinim::userland::x86_32::close(static_cast<int>(fd)));
        }
        g_last_status = read_failed ? 1U : 0U;
    }

    void list_directory(const char *path) noexcept {
        const uint32_t fd = xinim::userland::x86_32::open(path, kOpenFlagRDOnly, 0U);
        if (is_syscall_error(fd)) {
            g_last_status = 1U;
            write_line("ls: unavailable");
            return;
        }

        bool read_failed = false;
        for (;;) {
            const uint32_t result =
                xinim::userland::x86_32::read(static_cast<int>(fd), g_io, kIoBufferSize - 1U);
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

    void copy_file(const char *source, const char *destination) noexcept {
        if (source == nullptr || destination == nullptr) {
            g_last_status = 1U;
            write_line("cp: missing arguments");
            return;
        }

        const uint32_t source_fd = xinim::userland::x86_32::open(source, kOpenFlagRDOnly, 0U);
        if (is_syscall_error(source_fd)) {
            g_last_status = 1U;
            write_line("cp: failed to open source");
            return;
        }

        const uint32_t destination_fd = xinim::userland::x86_32::open(
            destination, kOpenFlagWROnly | kOpenFlagCreat | kOpenFlagTrunc, 0644U);
        if (is_syscall_error(destination_fd)) {
            g_last_status = 1U;
            static_cast<void>(xinim::userland::x86_32::close(static_cast<int>(source_fd)));
            write_line("cp: failed to open destination");
            return;
        }

        bool failed = false;
        for (;;) {
            const uint32_t read_result = xinim::userland::x86_32::read(static_cast<int>(source_fd),
                                                                       g_io, kIoBufferSize - 1U);
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
                    static_cast<int>(destination_fd), g_io + written_total,
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

    void vi_edit_file(const char *path) noexcept {
        if (path == nullptr || path[0] == '\0') {
            g_last_status = 1U;
            write_line("vi: missing file path");
            return;
        }

        write_line("vi: line mode - enter text, line with only '.' to save and exit");
        char current[kViBufferSize];
        bool failed = false;

        const uint32_t existing_fd = xinim::userland::x86_32::open(path, kOpenFlagRDOnly, 0U);
        if (!is_syscall_error(existing_fd)) {
            for (;;) {
                const uint32_t result = xinim::userland::x86_32::read(
                    static_cast<int>(existing_fd), current, sizeof(current) - 1U);
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
            path, kOpenFlagWROnly | kOpenFlagCreat | kOpenFlagTrunc, 0644U);
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
            const uint32_t written =
                xinim::userland::x86_32::write(static_cast<int>(destination_fd), g_vi, payload);
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

    [[nodiscard]] bool resolve_command_path(const char *name, char *buffer,
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

        const char *path_variable = find_environment_value("PATH");
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

    void print_command_path(const char *name) noexcept {
        if (name == nullptr || name[0] == '\0') {
            g_last_status = 2U;
            write_line("usage: command -v NAME");
            return;
        }

        if (string_equals(name, "help") || string_equals(name, "pid") ||
            string_equals(name, "pwd") || string_equals(name, "status") ||
            string_equals(name, "exit") || string_equals(name, "cd") ||
            string_equals(name, "check") || string_equals(name, "ls") ||
            string_equals(name, "cp") || string_equals(name, "vi") || string_equals(name, "test") ||
            string_equals(name, "command") || string_equals(name, "cat") ||
            string_equals(name, "echo") || string_equals(name, "export") ||
            string_equals(name, "env") || string_equals(name, "unset") ||
            string_equals(name, "jobs") || string_equals(name, "wait")) {
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

    void run_external_command(char *command, bool background) noexcept {
        (void) background;
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
        const uint32_t waited =
            xinim::userland::x86_32::wait4(static_cast<int>(pid), &status, 0, nullptr);
        if (waited == static_cast<uint32_t>(-1)) {
            g_last_status = 255U;
            write_line("wait4 failed");
            return;
        }
        g_last_status = decode_wait_status(status);
    }

    void remove_background_process(uint32_t process_id) noexcept {
        for (uint32_t index = 0U; index < g_background_process_count; ++index) {
            if (g_background_processes[index] != process_id) {
                continue;
            }
            for (uint32_t mover = index; mover + 1U < g_background_process_count; ++mover) {
                g_background_processes[mover] = g_background_processes[mover + 1U];
            }
            --g_background_process_count;
            g_background_processes[g_background_process_count] = 0U;
            return;
        }
    }

    [[nodiscard]] bool record_background_process(uint32_t process_id) noexcept {
        if (g_background_process_count >= kMaxBackgroundProcesses) {
            return false;
        }
        g_background_processes[g_background_process_count++] = process_id;
        g_last_background_process = process_id;
        return true;
    }

    void reap_background_processes() noexcept {
        uint32_t index = 0U;
        while (index < g_background_process_count) {
            int status = 0;
            const uint32_t process_id = g_background_processes[index];
            const uint32_t waited = xinim::userland::x86_32::wait4(static_cast<int>(process_id),
                                                                   &status, kWaitNoHang, nullptr);
            if (waited == process_id || is_syscall_error(waited)) {
                remove_background_process(process_id);
                continue;
            }
            ++index;
        }
    }

    [[nodiscard]] bool parse_process_id(const char *text, uint32_t &process_id) noexcept {
        if (text == nullptr || *text == '\0') {
            return false;
        }
        uint32_t value = 0U;
        while (*text != '\0') {
            if (*text < '0' || *text > '9') {
                return false;
            }
            value = value * 10U + static_cast<uint32_t>(*text - '0');
            ++text;
        }
        process_id = value;
        return value != 0U;
    }

    void print_background_processes() noexcept {
        reap_background_processes();
        for (uint32_t index = 0U; index < g_background_process_count; ++index) {
            write_string("[running] ");
            write_dec(g_background_processes[index]);
            write_string(kNewline);
        }
        g_last_status = 0U;
    }

    void wait_for_background_processes(uint32_t argc) noexcept {
        if (argc > 2U) {
            g_last_status = 2U;
            write_line("usage: wait [PID]");
            return;
        }

        if (argc == 2U) {
            uint32_t process_id = 0U;
            if (!parse_process_id(g_argv[1], process_id)) {
                g_last_status = 2U;
                write_line("wait: invalid process id");
                return;
            }
            int status = 0;
            const uint32_t waited =
                xinim::userland::x86_32::wait4(static_cast<int>(process_id), &status, 0, nullptr);
            if (waited != process_id) {
                g_last_status = 127U;
                write_line("wait: process is not a child");
                return;
            }
            remove_background_process(process_id);
            g_last_status = decode_wait_status(status);
            return;
        }

        g_last_status = 0U;
        while (g_background_process_count > 0U) {
            const uint32_t process_id = g_background_processes[0];
            int status = 0;
            const uint32_t waited =
                xinim::userland::x86_32::wait4(static_cast<int>(process_id), &status, 0, nullptr);
            remove_background_process(process_id);
            if (waited == process_id) {
                g_last_status = decode_wait_status(status);
            } else {
                g_last_status = 127U;
            }
        }
    }

    void list_env() noexcept {
        for (uint32_t index = 0U; index < g_env_count; ++index) {
            write_line(g_env_storage[index]);
        }
        if (g_env_count == 0U) {
            write_line("(empty)");
        }
    }

    [[nodiscard]] bool execute_builtin(char *argv0, uint32_t argc, bool &done) noexcept {
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
                char *text = g_argv[1];
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
            const char *target = (argc < 2U || g_argv[1] == nullptr) ? "/bin" : g_argv[1];
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
            cat_file(argc < 2U ? nullptr : g_argv[1]);
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

        if (string_equals(argv0, "jobs")) {
            print_background_processes();
            return true;
        }

        if (string_equals(argv0, "wait")) {
            wait_for_background_processes(argc);
            return true;
        }

        done = false;
        return false;
    }

    [[nodiscard]] bool apply_prefix_assignments(uint32_t &argc, EnvironmentFrame &frame,
                                                bool &has_prefix_assignments) noexcept {
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

    [[nodiscard]] bool apply_redirections(const SimpleCommand &command) noexcept {
        for (uint32_t index = 0U; index < command.redirection_count; ++index) {
            const Redirection &redirection = command.redirections[index];
            uint32_t flags = kOpenFlagRDOnly;
            int target = kStandardInput;
            if (redirection.kind != RedirectionKind::Input) {
                target = kStandardOutput;
                flags = kOpenFlagWROnly | kOpenFlagCreat;
                flags |= redirection.kind == RedirectionKind::OutputAppend ? kOpenFlagAppend
                                                                           : kOpenFlagTrunc;
            }

            const uint32_t descriptor =
                xinim::userland::x86_32::open(redirection.path, flags, 0666U);
            if (is_syscall_error(descriptor)) {
                write_string("redirection: cannot open ");
                write_line(redirection.path);
                return false;
            }
            const uint32_t duplicate =
                xinim::userland::x86_32::dup2(static_cast<int>(descriptor), target);
            static_cast<void>(xinim::userland::x86_32::close(static_cast<int>(descriptor)));
            if (is_syscall_error(duplicate)) {
                write_line("redirection: dup2 failed");
                return false;
            }
        }
        return true;
    }

    struct SavedDescriptors {
        int input;
        int output;
    };

    [[nodiscard]] bool save_redirection_targets(const SimpleCommand &command,
                                                SavedDescriptors &saved) noexcept {
        saved.input = -1;
        saved.output = -1;
        bool need_input = false;
        bool need_output = false;
        for (uint32_t index = 0U; index < command.redirection_count; ++index) {
            if (command.redirections[index].kind == RedirectionKind::Input) {
                need_input = true;
            } else {
                need_output = true;
            }
        }
        if (need_input) {
            const uint32_t duplicate = xinim::userland::x86_32::dup(kStandardInput);
            if (is_syscall_error(duplicate)) {
                return false;
            }
            saved.input = static_cast<int>(duplicate);
        }
        if (need_output) {
            const uint32_t duplicate = xinim::userland::x86_32::dup(kStandardOutput);
            if (is_syscall_error(duplicate)) {
                if (saved.input >= 0) {
                    static_cast<void>(xinim::userland::x86_32::close(saved.input));
                    saved.input = -1;
                }
                return false;
            }
            saved.output = static_cast<int>(duplicate);
        }
        return true;
    }

    void restore_redirection_targets(SavedDescriptors &saved) noexcept {
        if (saved.input >= 0) {
            static_cast<void>(xinim::userland::x86_32::dup2(saved.input, kStandardInput));
            static_cast<void>(xinim::userland::x86_32::close(saved.input));
            saved.input = -1;
        }
        if (saved.output >= 0) {
            static_cast<void>(xinim::userland::x86_32::dup2(saved.output, kStandardOutput));
            static_cast<void>(xinim::userland::x86_32::close(saved.output));
            saved.output = -1;
        }
    }

    void close_pipeline_descriptors(int descriptors[kMaxPipelineCommands - 1U][2],
                                    uint32_t pipe_count) noexcept {
        for (uint32_t index = 0U; index < pipe_count; ++index) {
            if (descriptors[index][0] >= 0) {
                static_cast<void>(xinim::userland::x86_32::close(descriptors[index][0]));
                descriptors[index][0] = -1;
            }
            if (descriptors[index][1] >= 0) {
                static_cast<void>(xinim::userland::x86_32::close(descriptors[index][1]));
                descriptors[index][1] = -1;
            }
        }
    }

    [[noreturn]] void execute_child_command(SimpleCommand &command) noexcept {
        load_command_arguments(command);
        uint32_t argument_count = command.argument_count;
        EnvironmentFrame original_environment;
        snapshot_environment(original_environment);
        EnvironmentFrame command_environment;
        copy_environment_frame(command_environment, original_environment);
        bool used_temporary_environment = false;
        if (!apply_prefix_assignments(argument_count, command_environment,
                                      used_temporary_environment)) {
            write_line("too many environment assignments");
            xinim::userland::x86_32::exit(1);
        }
        if (used_temporary_environment) {
            restore_environment(command_environment);
        }
        if (argument_count == 0U) {
            xinim::userland::x86_32::exit(0);
        }

        bool builtin_done = false;
        static_cast<void>(execute_builtin(g_argv[0], argument_count, builtin_done));
        if (builtin_done) {
            xinim::userland::x86_32::exit(static_cast<int>(g_last_status));
        }

        if (!resolve_command_path(g_argv[0], g_path, kPathBufferSize)) {
            write_string("unknown command: ");
            write_line(g_argv[0]);
            xinim::userland::x86_32::exit(127);
        }
        static_cast<void>(xinim::userland::x86_32::execve(g_path, g_argv, g_envp_current));
        write_string("execve failed: ");
        write_line(g_path);
        xinim::userland::x86_32::exit(127);
    }

    void execute_forked_pipeline(Pipeline &pipeline) noexcept {
        const uint32_t pipe_count = pipeline.command_count - 1U;
        int pipe_descriptors[kMaxPipelineCommands - 1U][2]{};
        for (uint32_t index = 0U; index < kMaxPipelineCommands - 1U; ++index) {
            pipe_descriptors[index][0] = -1;
            pipe_descriptors[index][1] = -1;
        }
        uint32_t process_ids[kMaxPipelineCommands]{};

        if (pipeline.background &&
            g_background_process_count + pipeline.command_count > kMaxBackgroundProcesses) {
            g_last_status = 1U;
            write_line("background process table is full");
            return;
        }

        for (uint32_t index = 0U; index < pipe_count; ++index) {
            if (is_syscall_error(xinim::userland::x86_32::pipe(pipe_descriptors[index]))) {
                close_pipeline_descriptors(pipe_descriptors, pipe_count);
                g_last_status = 1U;
                write_line("pipe failed");
                return;
            }
        }

        uint32_t spawned = 0U;
        for (uint32_t index = 0U; index < pipeline.command_count; ++index) {
            const uint32_t process_id = xinim::userland::x86_32::fork();
            if (is_syscall_error(process_id)) {
                g_last_status = 1U;
                write_line("fork failed");
                break;
            }
            if (process_id == 0U) {
                if (index > 0U && is_syscall_error(xinim::userland::x86_32::dup2(
                                      pipe_descriptors[index - 1U][0], kStandardInput))) {
                    xinim::userland::x86_32::exit(1);
                }
                if (index < pipe_count && is_syscall_error(xinim::userland::x86_32::dup2(
                                              pipe_descriptors[index][1], kStandardOutput))) {
                    xinim::userland::x86_32::exit(1);
                }
                close_pipeline_descriptors(pipe_descriptors, pipe_count);
                if (!apply_redirections(pipeline.commands[index])) {
                    xinim::userland::x86_32::exit(1);
                }
                execute_child_command(pipeline.commands[index]);
            }
            process_ids[index] = process_id;
            ++spawned;
        }

        close_pipeline_descriptors(pipe_descriptors, pipe_count);
        if (pipeline.background) {
            for (uint32_t index = 0U; index < spawned; ++index) {
                static_cast<void>(record_background_process(process_ids[index]));
            }
            if (spawned > 0U) {
                write_string("[background] ");
                write_dec(process_ids[spawned - 1U]);
                write_string(kNewline);
                g_last_status = 0U;
            }
            return;
        }

        uint32_t last_status = g_last_status;
        for (uint32_t index = 0U; index < spawned; ++index) {
            int status = 0;
            const uint32_t waited = xinim::userland::x86_32::wait4(
                static_cast<int>(process_ids[index]), &status, 0, nullptr);
            if (waited != process_ids[index]) {
                last_status = 255U;
            } else if (index + 1U == pipeline.command_count) {
                last_status = decode_wait_status(status);
            }
        }
        g_last_status = last_status;
    }

    void execute_standalone_command(SimpleCommand &command) noexcept {
        load_command_arguments(command);
        uint32_t argument_count = command.argument_count;
        EnvironmentFrame original_environment;
        snapshot_environment(original_environment);
        EnvironmentFrame command_environment;
        copy_environment_frame(command_environment, original_environment);
        bool used_temporary_environment = false;
        if (!apply_prefix_assignments(argument_count, command_environment,
                                      used_temporary_environment)) {
            g_last_status = 1U;
            write_line("too many environment assignments");
            return;
        }
        if (used_temporary_environment) {
            restore_environment(command_environment);
        }
        if (argument_count == 0U) {
            g_last_status = 0U;
            return;
        }

        SavedDescriptors saved{};
        if (!save_redirection_targets(command, saved) || !apply_redirections(command)) {
            restore_redirection_targets(saved);
            g_last_status = 1U;
            if (used_temporary_environment) {
                restore_environment(original_environment);
            }
            return;
        }

        bool builtin_done = false;
        static_cast<void>(execute_builtin(g_argv[0], argument_count, builtin_done));
        if (!builtin_done) {
            run_external_command(g_argv[0], false);
        }
        restore_redirection_targets(saved);
        if (used_temporary_environment) {
            restore_environment(original_environment);
        }
    }

    void run_command_segment(char *segment) noexcept {
        if (segment == nullptr) {
            return;
        }

        Pipeline pipeline{};
        bool unsupported = false;
        if (!parse_pipeline(segment, pipeline, unsupported) || unsupported) {
            g_last_status = 2U;
            write_line(unsupported ? "invalid command syntax" : "command too long");
            return;
        }
        if (pipeline.command_count == 1U && !pipeline.background) {
            execute_standalone_command(pipeline.commands[0]);
        } else {
            execute_forked_pipeline(pipeline);
        }
    }

    void run_command_line() noexcept {
        char *cursor = g_line;
        bool segment_executed = false;
        while (*cursor != '\0') {
            cursor = skip_spaces(cursor);
            if (*cursor == '\0') {
                break;
            }

            char *segment_end = find_segment_end(cursor);
            char *next_segment = segment_end;
            const bool has_separator = *segment_end == ';';
            if (has_separator) {
                *segment_end = '\0';
                next_segment = segment_end + 1;
            }

            run_command_segment(cursor);
            segment_executed = true;
            if (!has_separator) {
                break;
            }
            cursor = next_segment;
        }
        if (!segment_executed) {
            g_last_status = 0U;
        }
    }

} // namespace

extern "C" int xinim_user_main(int argc, char **argv, char **envp) noexcept {
    (void) argc;
    (void) argv;

    initialize_environment(envp);
    write_string(kBanner);
    clear_argv();

    for (;;) {
        reap_background_processes();
        write_prompt();
        const uint32_t length = read_command();
        if (length == 0U && g_line[0] == '\0') {
            continue;
        }
        run_command_line();
    }
}
