#include "elf32_loader.hpp"

namespace xinim::i486::elf32 {
namespace {

constexpr uint8_t kElfMagic0 = 0x7FU;
constexpr uint8_t kElfMagic1 = 'E';
constexpr uint8_t kElfMagic2 = 'L';
constexpr uint8_t kElfMagic3 = 'F';
constexpr uint8_t kElfClass32 = 1U;
constexpr uint8_t kElfDataLittle = 1U;
constexpr uint16_t kElfTypeExec = 2U;
constexpr uint16_t kElfMachine386 = 3U;
constexpr uint32_t kProgramTypeLoad = 1U;

struct Elf32Header {
    uint8_t ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t phoff;
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
};

struct Elf32ProgramHeader {
    uint32_t type;
    uint32_t offset;
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t filesz;
    uint32_t memsz;
    uint32_t flags;
    uint32_t align;
};

[[nodiscard]] bool validate_header(const Elf32Header& header, uint32_t size) noexcept {
    if (size < sizeof(Elf32Header)) {
        return false;
    }
    if (header.ident[0] != kElfMagic0 ||
        header.ident[1] != kElfMagic1 ||
        header.ident[2] != kElfMagic2 ||
        header.ident[3] != kElfMagic3) {
        return false;
    }
    if (header.ident[4] != kElfClass32 || header.ident[5] != kElfDataLittle) {
        return false;
    }
    if (header.type != kElfTypeExec || header.machine != kElfMachine386) {
        return false;
    }
    if (header.phentsize != sizeof(Elf32ProgramHeader)) {
        return false;
    }
    if (header.phnum == 0U) {
        return false;
    }
    const uint32_t table_size =
        static_cast<uint32_t>(header.phnum) * static_cast<uint32_t>(sizeof(Elf32ProgramHeader));
    return header.phoff <= size && table_size <= (size - header.phoff);
}

void zero_region(uint8_t* base, uint32_t size) noexcept {
    for (uint32_t index = 0U; index < size; ++index) {
        base[index] = 0U;
    }
}

void copy_region(uint8_t* out, const uint8_t* in, uint32_t size) noexcept {
    for (uint32_t index = 0U; index < size; ++index) {
        out[index] = in[index];
    }
}

} // namespace

bool load_static_image(const uint8_t* image,
                       uint32_t size,
                       uint8_t* address_space,
                       uint32_t address_space_size,
                       UserImage* out) noexcept {
    if (image == nullptr || address_space == nullptr || out == nullptr) {
        return false;
    }
    if (address_space_size < 4096U) {
        return false;
    }

    const auto* header = reinterpret_cast<const Elf32Header*>(image);
    if (!validate_header(*header, size)) {
        return false;
    }

    const auto* program_headers =
        reinterpret_cast<const Elf32ProgramHeader*>(image + header->phoff);

    for (uint16_t index = 0U; index < header->phnum; ++index) {
        const Elf32ProgramHeader& program = program_headers[index];
        if (program.type != kProgramTypeLoad) {
            continue;
        }
        if (program.memsz == 0U && program.filesz == 0U) {
            continue;
        }
        if (program.memsz < program.filesz || program.offset > size ||
            program.filesz > (size - program.offset)) {
            return false;
        }
        if (program.vaddr < kUserVirtualBase) {
            return false;
        }
        const uint32_t region_offset = program.vaddr - kUserVirtualBase;
        if (region_offset > address_space_size || program.memsz > (address_space_size - region_offset)) {
            return false;
        }

        auto* destination = address_space + region_offset;
        zero_region(destination, program.memsz);
        copy_region(destination, image + program.offset, program.filesz);
    }

    out->entry_point = header->entry;
    out->stack_top = kUserVirtualBase + address_space_size - 16U;
    return true;
}

} // namespace xinim::i486::elf32
