#include "../i486/ext2_reader.hpp"
#include "../i486/console.hpp"

namespace xinim::i486::ext2_reader {

void probe() noexcept {}

bool register_bootfs_mount() noexcept {
    return false;
}

bool query_runtime_path(const char*,
                        NodeInfo& info) noexcept {
    info = {};
    return false;
}

bool query_persist_path(const char*,
                        NodeInfo& info) noexcept {
    info = {};
    return false;
}

bool load_runtime_executable(const char*,
                             const uint8_t** image,
                             uint32_t* size) noexcept {
    if (image != nullptr) {
        *image = nullptr;
    }
    if (size != nullptr) {
        *size = 0U;
    }
    return false;
}

bool load_persist_executable(const char*,
                             const uint8_t** image,
                             uint32_t* size) noexcept {
    if (image != nullptr) {
        *image = nullptr;
    }
    if (size != nullptr) {
        *size = 0U;
    }
    return false;
}

bool create_runtime_file(const char*, uint16_t) noexcept { return false; }
bool mkdir_runtime_directory(const char*, uint16_t) noexcept { return false; }
bool rename_runtime_path(const char*, const char*) noexcept { return false; }
bool unlink_runtime_path(const char*) noexcept { return false; }
bool rmdir_runtime_directory(const char*) noexcept { return false; }
bool truncate_runtime_file(const char*, uint32_t) noexcept { return false; }

int read_runtime_file(const char*, uint32_t, uint8_t*, uint32_t) noexcept { return -1; }
int write_runtime_file(const char*, uint32_t, const uint8_t*, uint32_t) noexcept { return -1; }
int read_persist_file(const char*, uint32_t, uint8_t*, uint32_t) noexcept { return -1; }

uint32_t build_runtime_directory_listing(const char*, char*, uint32_t) noexcept { return 0U; }
uint32_t build_persist_directory_listing(const char*, char*, uint32_t) noexcept { return 0U; }

} // namespace xinim::i486::ext2_reader

namespace xinim::i486::console {

void initialize() noexcept {}
void write_char(char) noexcept {}
void write_string(const char*) noexcept {}
void tty_write_char(char) noexcept {}
void tty_write_string(const char*) noexcept {}
void debug_write_char(char) noexcept {}
void debug_write_string(const char*) noexcept {}
char debug_read_char() noexcept { return '\0'; }
char tty_read_char() noexcept { return '\0'; }
void tty_poll_input() noexcept {}
bool tty_has_input() noexcept { return false; }
bool tty_try_read_char(char*) noexcept { return false; }
void write_bool(bool) noexcept {}
void write_dec32(uint32_t) noexcept {}
void write_hex32(uint32_t) noexcept {}
void write_hex64(uint64_t) noexcept {}
void newline() noexcept {}

} // namespace xinim::i486::console
