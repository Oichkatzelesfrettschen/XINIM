#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "../src/kernel/i486/ide.cpp"

namespace {

enum class TransferMode { Ready, Busy, MissingDrq, DataError, CompletionBusy, CompletionError };

struct PortWrite {
    uint16_t port;
    uint8_t value;
};

struct DeviceScript {
    TransferMode mode = TransferMode::Ready;
    uint8_t command = 0U;
    uint32_t words = 0U;
    uint32_t busy_reads = 0U;
    uint32_t status_reads = 0U;
    uint32_t task_file_writes = 0U;
    uint16_t counter = 3U;
    uint16_t latched_counter = 0U;
    uint16_t tick_step = 1U;
    bool high_byte = false;
    std::vector<PortWrite> writes;
} script;

void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

void initialize_device() {
    using namespace xinim::i486;
    script = {};
    ide::g_initialized = false;
    ide::g_io_available = false;
    ide::g_primary_master = {};
    storage::clear_boot_storage();
    ide::initialize();
    require(ide::primary_master_present(), "IDENTIFY detects scripted ATA device");
    require(script.writes.size() >= 5U, "PIT initialization emits register writes");
    const std::array<PortWrite, 5> expected = {{{0x61U, 0xA0U}, {0x43U, 0xB4U},
                                             {0x42U, 0U}, {0x42U, 0U}, {0x61U, 0xA1U}}};
    for (size_t index = 0U; index < expected.size(); ++index) {
        require(script.writes[index].port == expected[index].port &&
                    script.writes[index].value == expected[index].value,
                "PIT2 mode, gate and speaker state follow the device contract");
    }
    for (const PortWrite& write : script.writes) {
        require(write.port != 0x40U, "PIT0 counter retains scheduler ownership");
        require(write.port != 0x43U || write.value == 0xB4U || write.value == 0x80U,
                "PIT control writes address channel 2 exclusively");
    }
    script.status_reads = 0U;
}

void require_retired_device() {
    using namespace xinim::i486;
    require(!ide::primary_master_present(), "timeout retires ATA device");
    require(storage::boot_device() == nullptr && storage::boot_partition() == nullptr,
            "timeout revokes boot storage registration");
    const uint32_t writes_before = script.task_file_writes;
    std::array<uint8_t, 512> sector{};
    require(!ide::read_primary_master_sector(0U, sector.data()), "retired read fails");
    require(!ide::write_primary_master_sector(0U, sector.data()), "retired write fails");
    require(!ide::read_sector_internal(0U, sector.data()), "retained read callback fails");
    require(!ide::write_sector_internal(0U, sector.data()), "retained write callback fails");
    require(script.task_file_writes == writes_before,
            "retired callbacks leave the outstanding task file untouched");
}

} // namespace

namespace xinim::i486::io_port {

void outb(uint16_t port, uint8_t value) noexcept {
    script.writes.push_back({port, value});
    if (port >= 0x1F0U && port <= 0x1F7U) {
        ++script.task_file_writes;
    }
    if (port == 0x43U && value == 0x80U) {
        script.counter = static_cast<uint16_t>(script.counter - script.tick_step);
        script.latched_counter = script.counter;
        script.high_byte = false;
    }
    if (port == 0x1F7U) {
        script.command = value;
        script.words = 0U;
    }
}

uint8_t inb(uint16_t port) noexcept {
    if (port == 0x61U) {
        return 0xA3U;
    }
    if (port == 0x42U) {
        const uint8_t value = static_cast<uint8_t>(script.high_byte
                                                      ? script.latched_counter >> 8U
                                                      : script.latched_counter & 0xFFU);
        script.high_byte = !script.high_byte;
        return value;
    }
    if (port != 0x1F7U) {
        return 0U;
    }
    ++script.status_reads;
    if (script.busy_reads > 0U) {
        --script.busy_reads;
        return 0xD0U;
    }
    if (script.mode == TransferMode::Busy ||
        (script.mode == TransferMode::CompletionBusy && script.words == 256U)) {
        return 0xD0U;
    }
    if (script.mode == TransferMode::DataError ||
        (script.mode == TransferMode::CompletionError && script.words == 256U)) {
        return 0x41U;
    }
    if (script.mode == TransferMode::MissingDrq || script.words == 256U) {
        return 0x40U;
    }
    return 0x48U;
}

uint16_t inw(uint16_t port) noexcept {
    require(port == 0x1F0U, "ATA reads use the data port");
    const uint32_t word_index = script.words++;
    if (script.command == 0xECU) {
        return word_index == 60U ? 8192U : 0U;
    }
    return 0xA55AU;
}

void outw(uint16_t port, uint16_t value) noexcept {
    require(port == 0x1F0U && value == 0xA55AU, "ATA writes preserve sector words");
    ++script.words;
}

} // namespace xinim::i486::io_port

namespace xinim::i486::console {
void write_string(const char*) noexcept {}
void write_char(char) noexcept {}
void write_dec32(uint32_t) noexcept {}
void write_hex32(uint32_t) noexcept {}
void newline() noexcept {}
} // namespace xinim::i486::console

int main() {
    using namespace xinim::i486;
    std::array<uint8_t, 512> sector{};
    initialize_device();
    script.busy_reads = 200001U;
    require(ide::read_primary_master_sector(7U, sector.data()),
            "I/O completes beyond the removed 100000-iteration bound");
    require(script.status_reads > 200000U && sector.front() == 0x5AU && sector.back() == 0xA5U,
            "delayed completion transfers real sector data across PIT wraps");
    require(ide::write_primary_master_sector(7U, sector.data()), "ready write completes");

    initialize_device();
    script.mode = TransferMode::Busy;
    script.tick_step = 60000U;
    require(!ide::read_primary_master_sector(7U, sector.data()), "busy deadline expires");
    require(script.status_reads == 60U, "busy timeout follows three seconds of PIT ticks");
    require_retired_device();

    initialize_device();
    script.busy_reads = 30U;
    script.mode = TransferMode::MissingDrq;
    script.tick_step = 60000U;
    require(!ide::read_primary_master_sector(7U, sector.data()), "missing DRQ deadline expires");
    require(script.status_reads == 61U, "busy and DRQ waits share one command budget");
    require_retired_device();

    initialize_device();
    script.mode = TransferMode::CompletionBusy;
    script.tick_step = 60000U;
    require(!ide::write_primary_master_sector(7U, sector.data()), "write completion deadline expires");
    require_retired_device();

    initialize_device();
    script.mode = TransferMode::DataError;
    require(!ide::read_primary_master_sector(7U, sector.data()), "ATA error rejects read");
    require(ide::primary_master_present(), "completed ATA error leaves the device available");
    script.mode = TransferMode::CompletionError;
    require(!ide::write_primary_master_sector(7U, sector.data()), "write completion error is reported");

    std::puts("ATA hardware deadline and fail-closed tests passed");
}
