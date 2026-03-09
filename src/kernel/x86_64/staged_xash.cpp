#include "staged_xash.hpp"

#include <cstddef>
#include <cstdint>

#include <xinim/boot/bootinfo.hpp>

#include "../bootfs.hpp"
#include "../early/serial_16550.hpp"
#include "../proc.hpp"

extern xinim::early::Serial16550 kshell_serial;

namespace xinim::kernel::x86_64 {
namespace {

constexpr std::size_t kLineCapacity = 256;
constexpr std::size_t kTokenCapacity = 24;
constexpr std::size_t kPathCapacity = 160;
constexpr std::size_t kEnvCapacity = 16;
constexpr std::size_t kEnvEntryCapacity = 128;
constexpr std::size_t kDirCapacity = 64;
constexpr std::size_t kFileDataCapacity = 2048;

const xinim::boot::BootInfo* g_boot_info = nullptr;
char g_cwd[kPathCapacity] = "/";
char g_line[kLineCapacity]{};
char g_edit[kFileDataCapacity]{};
char g_env[kEnvCapacity][kEnvEntryCapacity]{};
std::size_t g_env_count = 0U;
int g_last_status = 0;

[[nodiscard]] std::size_t string_length(const char* text) noexcept {
    if (text == nullptr) {
        return 0U;
    }
    std::size_t length = 0U;
    while (text[length] != '\0') {
        ++length;
    }
    return length;
}

[[nodiscard]] bool string_equals(const char* lhs, const char* rhs) noexcept {
    if (lhs == nullptr || rhs == nullptr) {
        return lhs == rhs;
    }
    std::size_t index = 0U;
    while (lhs[index] != '\0' && rhs[index] != '\0') {
        if (lhs[index] != rhs[index]) {
            return false;
        }
        ++index;
    }
    return lhs[index] == rhs[index];
}

[[nodiscard]] const char* find_char(const char* text, char needle) noexcept {
    if (text == nullptr) {
        return nullptr;
    }
    for (const char* cursor = text; *cursor != '\0'; ++cursor) {
        if (*cursor == needle) {
            return cursor;
        }
    }
    return nullptr;
}

void write_char(char value) noexcept {
    kshell_serial.write_char(value);
}

void write_text(const char* text) noexcept {
    if (text != nullptr) {
        kshell_serial.write(text);
    }
}

void write_buffer(const char* text, std::size_t length) noexcept {
    for (std::size_t index = 0U; index < length; ++index) {
        write_char(text[index]);
    }
}

void write_line(const char* text) noexcept {
    write_text(text);
    write_text("\n");
}

void write_dec(std::uint64_t value) noexcept {
    if (value == 0U) {
        write_char('0');
        return;
    }

    char buffer[32];
    std::size_t used = 0U;
    while (value != 0U && used < sizeof(buffer)) {
        buffer[used++] = static_cast<char>('0' + (value % 10U));
        value /= 10U;
    }
    while (used > 0U) {
        write_char(buffer[--used]);
    }
}

void set_env_entry(const char* assignment) noexcept {
    if (assignment == nullptr) {
        return;
    }

    const char* equals = find_char(assignment, '=');
    if (equals == nullptr || equals == assignment) {
        return;
    }

    const std::size_t key_length = static_cast<std::size_t>(equals - assignment);
    for (std::size_t index = 0U; index < g_env_count; ++index) {
        if (__builtin_memcmp(g_env[index], assignment, key_length) == 0 &&
            g_env[index][key_length] == '=') {
            __builtin_memset(g_env[index], 0, sizeof(g_env[index]));
            const std::size_t limit = string_length(assignment);
            const std::size_t copy_length =
                (limit < (kEnvEntryCapacity - 1U)) ? limit : (kEnvEntryCapacity - 1U);
            __builtin_memcpy(g_env[index], assignment, copy_length);
            g_env[index][copy_length] = '\0';
            return;
        }
    }

    if (g_env_count >= kEnvCapacity) {
        return;
    }

    const std::size_t limit = string_length(assignment);
    const std::size_t copy_length =
        (limit < (kEnvEntryCapacity - 1U)) ? limit : (kEnvEntryCapacity - 1U);
    __builtin_memcpy(g_env[g_env_count], assignment, copy_length);
    g_env[g_env_count][copy_length] = '\0';
    ++g_env_count;
}

void remove_env_entry(const char* name) noexcept {
    if (name == nullptr || name[0] == '\0') {
        return;
    }

    const std::size_t key_length = string_length(name);
    for (std::size_t index = 0U; index < g_env_count; ++index) {
        if (__builtin_memcmp(g_env[index], name, key_length) == 0 &&
            g_env[index][key_length] == '=') {
            for (std::size_t cursor = index; cursor + 1U < g_env_count; ++cursor) {
                __builtin_memcpy(g_env[cursor], g_env[cursor + 1U], kEnvEntryCapacity);
            }
            __builtin_memset(g_env[g_env_count - 1U], 0, kEnvEntryCapacity);
            --g_env_count;
            return;
        }
    }
}

[[nodiscard]] const char* get_env_value(const char* name) noexcept {
    if (name == nullptr || name[0] == '\0') {
        return nullptr;
    }

    const std::size_t key_length = string_length(name);
    for (std::size_t index = 0U; index < g_env_count; ++index) {
        if (__builtin_memcmp(g_env[index], name, key_length) == 0 &&
            g_env[index][key_length] == '=') {
            return g_env[index] + key_length + 1U;
        }
    }
    return nullptr;
}

void sync_pwd_env() noexcept {
    char entry[kEnvEntryCapacity];
    __builtin_memset(entry, 0, sizeof(entry));
    __builtin_memcpy(entry, "PWD=", 4U);
    const std::size_t cwd_length = string_length(g_cwd);
    const std::size_t copy_length =
        (cwd_length < (sizeof(entry) - 5U)) ? cwd_length : (sizeof(entry) - 5U);
    __builtin_memcpy(entry + 4U, g_cwd, copy_length);
    set_env_entry(entry);
}

void init_environment() noexcept {
    g_env_count = 0U;
    set_env_entry("PATH=/bin");
    set_env_entry("SHELL=/bin/xash");
    sync_pwd_env();
}

[[nodiscard]] bool append_component(char components[][27],
                                    std::size_t& count,
                                    const char* start,
                                    std::size_t length) noexcept {
    if (length == 0U || length > 26U || count >= kDirCapacity) {
        return false;
    }
    __builtin_memset(components[count], 0, 27U);
    __builtin_memcpy(components[count], start, length);
    ++count;
    return true;
}

[[nodiscard]] bool resolve_path(const char* input,
                                char* output,
                                std::size_t capacity) noexcept {
    if (input == nullptr || output == nullptr || capacity < 2U) {
        return false;
    }

    char components[kDirCapacity][27];
    std::size_t count = 0U;

    auto ingest = [&](const char* path) noexcept -> bool {
        const char* cursor = path;
        while (*cursor != '\0') {
            while (*cursor == '/') {
                ++cursor;
            }
            if (*cursor == '\0') {
                break;
            }

            const char* start = cursor;
            while (*cursor != '\0' && *cursor != '/') {
                ++cursor;
            }
            const std::size_t length = static_cast<std::size_t>(cursor - start);
            if (length == 1U && start[0] == '.') {
                continue;
            }
            if (length == 2U && start[0] == '.' && start[1] == '.') {
                if (count > 0U) {
                    --count;
                }
                continue;
            }
            if (!append_component(components, count, start, length)) {
                return false;
            }
        }
        return true;
    };

    if (input[0] != '/') {
        if (!ingest(g_cwd)) {
            return false;
        }
    }
    if (!ingest(input)) {
        return false;
    }

    std::size_t position = 0U;
    output[position++] = '/';
    if (count == 0U) {
        output[position] = '\0';
        return true;
    }

    for (std::size_t index = 0U; index < count; ++index) {
        const std::size_t length = string_length(components[index]);
        if (position + length + 1U >= capacity) {
            return false;
        }
        __builtin_memcpy(output + position, components[index], length);
        position += length;
        if (index + 1U < count) {
            output[position++] = '/';
        }
    }
    output[position] = '\0';
    return true;
}

[[nodiscard]] bool is_directory_path(const char* path) noexcept {
    return xinim::kernel::bootfs::is_directory(path);
}

void init_staged_filesystem() noexcept {
    if (g_boot_info != nullptr) {
        xinim::kernel::bootfs::initialize(*g_boot_info);
    }
}

[[nodiscard]] bool file_exists(const char* target, char* resolved) noexcept {
    if (!resolve_path(target, resolved, kPathCapacity)) {
        return false;
    }
    return xinim::kernel::bootfs::access(resolved) == 0;
}

int read_bootfs_file(const char* path, char* buffer, std::size_t capacity, std::size_t* out_size) noexcept {
    if (buffer == nullptr || out_size == nullptr || capacity == 0U) {
        return -1;
    }

    const int fd = xinim::kernel::bootfs::open(path, xinim::kernel::bootfs::O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    std::size_t used = 0U;
    while (used + 1U < capacity) {
        const int chunk = xinim::kernel::bootfs::read(
            fd,
            buffer + used,
            static_cast<uint32_t>(capacity - used - 1U));
        if (chunk < 0) {
            (void)xinim::kernel::bootfs::close(fd);
            return -1;
        }
        if (chunk == 0) {
            break;
        }
        used += static_cast<std::size_t>(chunk);
    }

    buffer[used] = '\0';
    *out_size = used;
    (void)xinim::kernel::bootfs::close(fd);
    return 0;
}

int write_bootfs_file(const char* path, const char* data, std::size_t size) noexcept {
    const int fd = xinim::kernel::bootfs::open(
        path,
        xinim::kernel::bootfs::O_WRONLY |
            xinim::kernel::bootfs::O_CREAT |
            xinim::kernel::bootfs::O_TRUNC);
    if (fd < 0) {
        return -1;
    }

    std::size_t written = 0U;
    while (written < size) {
        const int chunk = xinim::kernel::bootfs::write(
            fd,
            data + written,
            static_cast<uint32_t>(size - written));
        if (chunk < 0) {
            (void)xinim::kernel::bootfs::close(fd);
            return -1;
        }
        if (chunk == 0) {
            break;
        }
        written += static_cast<std::size_t>(chunk);
    }

    (void)xinim::kernel::bootfs::close(fd);
    return written == size ? 0 : -1;
}

void print_prompt() noexcept {
    write_text("xash$ ");
}

std::size_t read_line() noexcept {
    std::size_t position = 0U;
    g_line[0] = '\0';
    while (true) {
        const char value = kshell_serial.read_char();
        if (value == '\r' || value == '\n') {
            g_line[position] = '\0';
            write_text("\n");
            return position;
        }
        if (value == '\b' || value == 127) {
            if (position > 0U) {
                --position;
                write_text("\b \b");
            }
            continue;
        }
        if (position + 1U >= sizeof(g_line)) {
            continue;
        }
        g_line[position++] = value;
        write_char(value);
    }
}

int tokenize(char* line, char** argv, int max_args) noexcept {
    if (line == nullptr || argv == nullptr || max_args <= 1) {
        return 0;
    }

    int argc = 0;
    char* write = line;
    char* token = nullptr;
    bool in_single = false;
    bool in_double = false;
    bool escaped = false;

    for (char* read = line; *read != '\0'; ++read) {
        const char value = *read;
        if (escaped) {
            if (token == nullptr) {
                token = write;
            }
            *write++ = value;
            escaped = false;
            continue;
        }
        if (value == '\\' && !in_single) {
            if (token == nullptr) {
                token = write;
            }
            escaped = true;
            continue;
        }
        if (value == '\'' && !in_double) {
            if (token == nullptr) {
                token = write;
            }
            in_single = !in_single;
            continue;
        }
        if (value == '"' && !in_single) {
            if (token == nullptr) {
                token = write;
            }
            in_double = !in_double;
            continue;
        }
        if (!in_single && !in_double && (value == ' ' || value == '\t')) {
            if (token != nullptr) {
                *write++ = '\0';
                argv[argc++] = token;
                if (argc >= max_args - 1) {
                    argv[argc] = nullptr;
                    return argc;
                }
                token = nullptr;
            }
            continue;
        }
        if (token == nullptr) {
            token = write;
        }
        *write++ = value;
    }

    if (escaped || in_single || in_double) {
        write_line("parse error: unmatched quote or escape");
        return -1;
    }
    if (token != nullptr) {
        *write++ = '\0';
        argv[argc++] = token;
    }
    argv[argc] = nullptr;
    return argc;
}

int builtin_help() noexcept {
    write_line("Built-in commands:");
    write_line("  help  pwd  cd  ls  cat  cp  vi  check  test");
    write_line("  echo  env  export  unset  status  pid  boot  cmdline");
    write_line("  command -v NAME  rescue  halt  reboot");
    return 0;
}

int builtin_pwd() noexcept {
    write_line(g_cwd);
    return 0;
}

int builtin_pid() noexcept {
    write_text("pid: 1\n");
    return 0;
}

int builtin_status() noexcept {
    write_dec(static_cast<std::uint64_t>(g_last_status));
    write_text("\n");
    return 0;
}

int builtin_boot() noexcept {
    if (g_boot_info == nullptr) {
        write_line("boot info unavailable");
        return 1;
    }

    write_text("modules: ");
    write_dec(g_boot_info->modules_count);
    write_text("\ncmdline: ");
    if (g_boot_info->cmdline && g_boot_info->cmdline[0] != '\0') {
        write_text(g_boot_info->cmdline);
    } else {
        write_text("(none)");
    }
    write_text("\n");
    return 0;
}

int builtin_cmdline() noexcept {
    if (g_boot_info == nullptr || g_boot_info->cmdline == nullptr ||
        g_boot_info->cmdline[0] == '\0') {
        write_line("(none)");
        return 0;
    }
    write_line(g_boot_info->cmdline);
    return 0;
}

int builtin_cd(char** argv, int argc) noexcept {
    const char* target = (argc >= 2 && argv[1] != nullptr) ? argv[1] : "/";
    char resolved[kPathCapacity];
    if (!resolve_path(target, resolved, sizeof(resolved)) || !is_directory_path(resolved)) {
        write_line("cd: path not found");
        return 1;
    }
    __builtin_memset(g_cwd, 0, sizeof(g_cwd));
    const std::size_t length = string_length(resolved);
    __builtin_memcpy(g_cwd, resolved, length);
    sync_pwd_env();
    write_text("cd ");
    write_text(g_cwd);
    write_text(": ok\n");
    return 0;
}

int builtin_check(const char* target) noexcept {
    char resolved[kPathCapacity];
    write_text("check ");
    write_text(target);
    write_text(": ");
    if (!resolve_path(target, resolved, sizeof(resolved))) {
        write_line("missing");
        return 1;
    }
    if (is_directory_path(resolved)) {
        write_line("directory");
        return 0;
    }
    if (xinim::kernel::bootfs::access(resolved) == 0) {
        write_line("ok");
        return 0;
    }
    write_line("missing");
    return 1;
}

int builtin_test(char** argv, int argc) noexcept {
    if (argc == 3 && argv[1] != nullptr && string_equals(argv[1], "-f")) {
        char resolved[kPathCapacity];
        const bool ok = file_exists(argv[2], resolved);
        const bool file_ok = ok && !is_directory_path(resolved);
        write_line(file_ok ? "true" : "false");
        return file_ok ? 0 : 1;
    }
    write_line("test: supported form is 'test -f PATH'");
    return 2;
}

int builtin_ls(const char* target) noexcept {
    char resolved[kPathCapacity];
    if (!resolve_path(target, resolved, sizeof(resolved)) || !is_directory_path(resolved)) {
        write_line("ls: path not found");
        return 1;
    }

    char listing[1024];
    std::size_t used = 0U;
    if (read_bootfs_file(resolved, listing, sizeof(listing), &used) != 0) {
        write_line("ls: path not found");
        return 1;
    }
    if (used > 0U) {
        write_buffer(listing, used);
        if (listing[used - 1U] != '\n') {
            write_text("\n");
        }
    }
    return 0;
}

int builtin_cat(const char* target) noexcept {
    char resolved[kPathCapacity];
    if (!resolve_path(target, resolved, sizeof(resolved))) {
        write_line("cat: path not found");
        return 1;
    }
    char buffer[kFileDataCapacity];
    std::size_t used = 0U;
    if (read_bootfs_file(resolved, buffer, sizeof(buffer), &used) != 0) {
        write_line("cat: path not found");
        return 1;
    }
    if (used > 0U) {
        write_buffer(buffer, used);
    }
    if (used == 0U || buffer[used - 1U] != '\n') {
        write_text("\n");
    }
    return 0;
}

int builtin_cp(const char* source, const char* destination) noexcept {
    char resolved_source[kPathCapacity];
    if (!resolve_path(source, resolved_source, sizeof(resolved_source))) {
        write_line("cp: failed to open source");
        return 1;
    }

    char resolved_destination[kPathCapacity];
    if (!resolve_path(destination, resolved_destination, sizeof(resolved_destination))) {
        write_line("cp: invalid destination");
        return 1;
    }

    char buffer[kFileDataCapacity];
    std::size_t used = 0U;
    if (read_bootfs_file(resolved_source, buffer, sizeof(buffer), &used) != 0 ||
        write_bootfs_file(resolved_destination, buffer, used) != 0) {
        write_line("cp: transfer error");
        return 1;
    }
    return 0;
}

int builtin_vi(const char* path) noexcept {
    if (path == nullptr) {
        write_line("vi: missing file path");
        return 1;
    }

    char resolved[kPathCapacity];
    if (!resolve_path(path, resolved, sizeof(resolved))) {
        write_line("vi: invalid path");
        return 1;
    }

    write_line("vi: line mode, '.' alone saves and exits");
    std::size_t used = 0U;
    while (true) {
        write_text("vi> ");
        const std::size_t line_length = read_line();
        if (line_length == 1U && g_line[0] == '.') {
            break;
        }

        if (used + line_length + 1U >= sizeof(g_edit)) {
            write_line("vi: buffer full");
            return 1;
        }
        __builtin_memcpy(g_edit + used, g_line, line_length);
        used += line_length;
        g_edit[used++] = '\n';
    }

    if (write_bootfs_file(resolved, g_edit, used) != 0) {
        write_line("vi: write failed");
        return 1;
    }
    write_line("vi: saved");
    return 0;
}

int builtin_echo(char** argv, int argc) noexcept {
    for (int index = 1; index < argc; ++index) {
        const char* value = argv[index];
        if (value == nullptr) {
            continue;
        }
        if (index > 1) {
            write_char(' ');
        }
        if (string_equals(value, "$?")) {
            write_dec(static_cast<std::uint64_t>(g_last_status));
            continue;
        }
        if (string_equals(value, "$$")) {
            write_dec(1U);
            continue;
        }
        if (value[0] == '$' && value[1] != '\0') {
            const char* env_value = get_env_value(value + 1);
            if (env_value != nullptr) {
                write_text(env_value);
            }
            continue;
        }
        write_text(value);
    }
    write_text("\n");
    return 0;
}

int builtin_env() noexcept {
    for (std::size_t index = 0U; index < g_env_count; ++index) {
        write_line(g_env[index]);
    }
    return 0;
}

int builtin_export(char** argv, int argc) noexcept {
    if (argc == 1) {
        return builtin_env();
    }
    for (int index = 1; index < argc; ++index) {
        if (find_char(argv[index], '=') == nullptr) {
            write_line("export: expected NAME=VALUE");
            return 1;
        }
        set_env_entry(argv[index]);
    }
    return 0;
}

int builtin_unset(char** argv, int argc) noexcept {
    if (argc < 2) {
        write_line("unset: expected NAME");
        return 1;
    }
    for (int index = 1; index < argc; ++index) {
        remove_env_entry(argv[index]);
    }
    return 0;
}

int builtin_command(char** argv, int argc) noexcept {
    if (argc != 3 || !string_equals(argv[1], "-v")) {
        write_line("usage: command -v NAME");
        return 2;
    }

    if (string_equals(argv[2], "help") || string_equals(argv[2], "ls") ||
        string_equals(argv[2], "cat") || string_equals(argv[2], "cp") ||
        string_equals(argv[2], "vi")) {
        write_line(argv[2]);
        return 0;
    }

    const char* path_value = get_env_value("PATH");
    if (path_value == nullptr) {
        return 1;
    }

    const char* cursor = path_value;
    while (*cursor != '\0') {
        char prefix[kPathCapacity];
        std::size_t prefix_length = 0U;
        while (cursor[prefix_length] != '\0' && cursor[prefix_length] != ':') {
            if (prefix_length + 1U >= sizeof(prefix)) {
                return 1;
            }
            prefix[prefix_length] = cursor[prefix_length];
            ++prefix_length;
        }
        prefix[prefix_length] = '\0';
        if (prefix_length == 0U) {
            prefix[0] = '.';
            prefix[1] = '\0';
        }

        char candidate[kPathCapacity];
        if (resolve_path(prefix, candidate, sizeof(candidate))) {
            const std::size_t base_length = string_length(candidate);
            const std::size_t name_length = string_length(argv[2]);
            const std::size_t offset = string_equals(candidate, "/") ? 1U : (base_length + 1U);
            if (offset + name_length + 1U < sizeof(candidate)) {
                if (!string_equals(candidate, "/")) {
                    candidate[base_length] = '/';
                    candidate[base_length + 1U] = '\0';
                }
                __builtin_memcpy(candidate + offset, argv[2], name_length + 1U);
                if (xinim::kernel::bootfs::access(candidate) == 0) {
                    write_line(candidate);
                    return 0;
                }
            }
        }

        cursor += prefix_length;
        if (*cursor == ':') {
            ++cursor;
        }
    }
    return 1;
}

int builtin_rescue() noexcept {
    write_line("Entering COM2 rescue shell. Type 'continue' to return to staged xash.");
    (void)kshell_serial.shell(g_boot_info, true);
    return 0;
}

int builtin_halt() noexcept {
    ::halt();
    return 0;
}

int builtin_reboot() noexcept {
    ::reboot();
    return 0;
}

int builtin_exec_placeholder(const char* command) noexcept {
    char resolved[kPathCapacity];
    if (file_exists(command, resolved)) {
        write_text("external exec staged but ring3 handoff is not wired yet: ");
        write_line(resolved);
        return 126;
    }

    const char* path_value = get_env_value("PATH");
    if (path_value != nullptr) {
        const char* cursor = path_value;
        while (*cursor != '\0') {
            char prefix[kPathCapacity];
            std::size_t prefix_length = 0U;
            while (cursor[prefix_length] != '\0' && cursor[prefix_length] != ':') {
                prefix[prefix_length] = cursor[prefix_length];
                ++prefix_length;
            }
            prefix[prefix_length] = '\0';
            if (prefix_length == 0U) {
                prefix[0] = '.';
                prefix[1] = '\0';
            }

            char candidate[kPathCapacity];
            if (resolve_path(prefix, candidate, sizeof(candidate))) {
                const std::size_t base_length = string_length(candidate);
                const std::size_t command_length = string_length(command);
                const std::size_t offset =
                    string_equals(candidate, "/") ? 1U : (base_length + 1U);
                if (offset + command_length + 1U < sizeof(candidate)) {
                    if (!string_equals(candidate, "/")) {
                        candidate[base_length] = '/';
                        candidate[base_length + 1U] = '\0';
                    }
                    __builtin_memcpy(candidate + offset, command, command_length + 1U);
                    if (xinim::kernel::bootfs::access(candidate) == 0) {
                        write_text("external exec staged but ring3 handoff is not wired yet: ");
                        write_line(candidate);
                        return 126;
                    }
                }
            }

            cursor += prefix_length;
            if (*cursor == ':') {
                ++cursor;
            }
        }
    }

    write_text("unknown command: ");
    write_line(command);
    return 127;
}

int dispatch_command(char** argv, int argc) noexcept {
    if (argc <= 0 || argv[0] == nullptr) {
        return 0;
    }

    const char* command = argv[0];
    if (string_equals(command, "help")) {
        return builtin_help();
    }
    if (string_equals(command, "pwd")) {
        return builtin_pwd();
    }
    if (string_equals(command, "pid")) {
        return builtin_pid();
    }
    if (string_equals(command, "status")) {
        return builtin_status();
    }
    if (string_equals(command, "boot")) {
        return builtin_boot();
    }
    if (string_equals(command, "cmdline")) {
        return builtin_cmdline();
    }
    if (string_equals(command, "cd")) {
        return builtin_cd(argv, argc);
    }
    if (string_equals(command, "check")) {
        if (argc < 2) {
            write_line("usage: check PATH");
            return 2;
        }
        return builtin_check(argv[1]);
    }
    if (string_equals(command, "test")) {
        return builtin_test(argv, argc);
    }
    if (string_equals(command, "ls")) {
        return builtin_ls((argc >= 2 && argv[1] != nullptr) ? argv[1] : g_cwd);
    }
    if (string_equals(command, "cat")) {
        if (argc < 2) {
            write_line("usage: cat PATH");
            return 2;
        }
        return builtin_cat(argv[1]);
    }
    if (string_equals(command, "cp")) {
        if (argc < 3) {
            write_line("usage: cp SRC DST");
            return 2;
        }
        return builtin_cp(argv[1], argv[2]);
    }
    if (string_equals(command, "vi")) {
        if (argc < 2) {
            write_line("usage: vi FILE");
            return 2;
        }
        return builtin_vi(argv[1]);
    }
    if (string_equals(command, "echo")) {
        return builtin_echo(argv, argc);
    }
    if (string_equals(command, "env")) {
        return builtin_env();
    }
    if (string_equals(command, "export")) {
        return builtin_export(argv, argc);
    }
    if (string_equals(command, "unset")) {
        return builtin_unset(argv, argc);
    }
    if (string_equals(command, "command")) {
        return builtin_command(argv, argc);
    }
    if (string_equals(command, "rescue")) {
        return builtin_rescue();
    }
    if (string_equals(command, "halt")) {
        return builtin_halt();
    }
    if (string_equals(command, "reboot")) {
        return builtin_reboot();
    }
    return builtin_exec_placeholder(command);
}

} // namespace

void set_staged_xash_boot_info(const xinim::boot::BootInfo* boot_info) noexcept {
    g_boot_info = boot_info;
}

[[gnu::no_stack_protector]] [[noreturn]] void run_staged_xash_init() noexcept {
    write_text("\n");
    write_line("xash x86_64 staged init shell");
    write_line("COM2 handoff is live; type 'help' for commands.");
    write_line("Use 'rescue' to jump back into the emergency shell.");
    init_staged_filesystem();
    init_environment();

    for (;;) {
        print_prompt();
        const std::size_t length = read_line();
        if (length == 0U && g_line[0] == '\0') {
            continue;
        }

        char* argv[kTokenCapacity];
        const int argc = tokenize(g_line, argv, static_cast<int>(kTokenCapacity));
        if (argc < 0) {
            g_last_status = 2;
            continue;
        }
        if (argc == 0) {
            g_last_status = 0;
            continue;
        }
        g_last_status = dispatch_command(argv, argc);
    }
}

} // namespace xinim::kernel::x86_64
