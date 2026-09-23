#include "elf32_loader.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

    constexpr std::size_t kImageSize = 512U;
    constexpr std::size_t kProgramHeaderOffset = 52U;
    constexpr std::size_t kProgramHeaderSize = 32U;
    constexpr std::uint32_t kUserBase = xinim::i486::elf32::kUserVirtualBase;

    using ElfBytes = std::array<std::uint8_t, kImageSize>;

    void write_u16(ElfBytes &image, std::size_t offset, std::uint16_t value) {
        image[offset] = static_cast<std::uint8_t>(value);
        image[offset + 1U] = static_cast<std::uint8_t>(value >> 8U);
    }

    void write_u32(ElfBytes &image, std::size_t offset, std::uint32_t value) {
        for (std::size_t byte_index = 0U; byte_index < 4U; ++byte_index) {
            image[offset + byte_index] = static_cast<std::uint8_t>(value >> (byte_index * 8U));
        }
    }

    void write_load_segment(ElfBytes &image, std::size_t header_offset, std::uint32_t file_offset,
                            std::uint32_t virtual_address, std::uint32_t file_size,
                            std::uint32_t memory_size, std::uint32_t flags) {
        write_u32(image, header_offset, 1U);
        write_u32(image, header_offset + 4U, file_offset);
        write_u32(image, header_offset + 8U, virtual_address);
        write_u32(image, header_offset + 12U, virtual_address);
        write_u32(image, header_offset + 16U, file_size);
        write_u32(image, header_offset + 20U, memory_size);
        write_u32(image, header_offset + 24U, flags);
        write_u32(image, header_offset + 28U, 1U);
    }

    [[nodiscard]] ElfBytes valid_image() {
        ElfBytes image{};
        image[0] = 0x7FU;
        image[1] = 'E';
        image[2] = 'L';
        image[3] = 'F';
        image[4] = 1U;
        image[5] = 1U;
        write_u16(image, 16U, 2U);
        write_u16(image, 18U, 3U);
        write_u32(image, 24U, kUserBase);
        write_u32(image, 28U, static_cast<std::uint32_t>(kProgramHeaderOffset));
        write_u16(image, 42U, static_cast<std::uint16_t>(kProgramHeaderSize));
        write_u16(image, 44U, 2U);
        write_load_segment(image, kProgramHeaderOffset, 256U, kUserBase, 4U, 4U, 5U);
        write_load_segment(image, kProgramHeaderOffset + kProgramHeaderSize, 260U, kUserBase + 256U,
                           4U, 8U, 6U);
        image[256] = 0x90U;
        image[260] = 0x11U;
        return image;
    }

    [[nodiscard]] bool rejected(const ElfBytes &image) {
        xinim::i486::elf32::UserImage layout{};
        return !xinim::i486::elf32::inspect_static_image(
            image.data(), static_cast<std::uint32_t>(image.size()), &layout);
    }

} // namespace

int main() {
    alignas(4) ElfBytes image = valid_image();
    xinim::i486::elf32::UserImage layout{};
    if (!xinim::i486::elf32::inspect_static_image(
            image.data(), static_cast<std::uint32_t>(image.size()), &layout)) {
        std::fputs("valid ELF was rejected\n", stderr);
        return 1;
    }
    std::vector<std::uint8_t> address_space(xinim::i486::elf32::kUserAddressSpaceSize);
    if (!xinim::i486::elf32::load_static_image(
            image.data(), static_cast<std::uint32_t>(image.size()), address_space.data(),
            static_cast<std::uint32_t>(address_space.size()), &layout) ||
        address_space[0] != 0x90U || address_space[256] != 0x11U || address_space[260] != 0U) {
        std::fputs("valid ELF load corrupted a segment\n", stderr);
        return 1;
    }

    write_u32(image, kProgramHeaderOffset + kProgramHeaderSize + 8U, kUserBase + 2U);
    if (!rejected(image)) {
        std::fputs("overlapping PT_LOAD segments were accepted\n", stderr);
        return 1;
    }
    image = valid_image();
    write_u32(image, 24U, kUserBase + 256U);
    if (!rejected(image)) {
        std::fputs("entry in writable data was accepted\n", stderr);
        return 1;
    }
    image = valid_image();
    write_u32(image, kProgramHeaderOffset + 24U, 4U);
    if (!rejected(image)) {
        std::fputs("entry in non-executable text was accepted\n", stderr);
        return 1;
    }
    std::puts("PASS: i486 ELF32 segment ownership");
    return 0;
}
