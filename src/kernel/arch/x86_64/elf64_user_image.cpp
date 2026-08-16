#include "elf64_user_image.hpp"

#include "../../elf_loader.hpp"
#include "../../i486/bootfs.hpp"
#include "exec_arguments.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace xinim::kernel::x86_64 {
    namespace {

        constexpr uint16_t kMaximumProgramHeaders = 32U;
        constexpr uint64_t kAuxiliaryTypeNull = 0U;
        constexpr uint64_t kAuxiliaryTypePageSize = 6U;

        [[nodiscard]] bool add_overflows(uint64_t left, uint64_t right) noexcept {
            return left > std::numeric_limits<uint64_t>::max() - right;
        }

        [[nodiscard]] bool multiply_overflows(uint64_t left, uint64_t right) noexcept {
            return right != 0U && left > std::numeric_limits<uint64_t>::max() / right;
        }

        [[nodiscard]] uint64_t align_down(uint64_t value) noexcept {
            return value & ~(kPageSize - 1U);
        }

        [[nodiscard]] bool align_up(uint64_t value, uint64_t &aligned) noexcept {
            if (add_overflows(value, kPageSize - 1U)) {
                return false;
            }
            aligned = (value + kPageSize - 1U) & ~(kPageSize - 1U);
            return true;
        }

        [[nodiscard]] bool is_power_of_two(uint64_t value) noexcept {
            return value != 0U && (value & (value - 1U)) == 0U;
        }

        [[nodiscard]] size_t string_length(const char *text) noexcept {
            size_t length = 0U;
            if (text == nullptr) {
                return 0U;
            }
            while (text[length] != '\0') {
                ++length;
            }
            return length;
        }

        [[nodiscard]] size_t bounded_string_size(const char *text, size_t capacity) noexcept {
            if (text == nullptr) {
                return 0U;
            }
            for (size_t index = 0U; index < capacity; ++index) {
                if (text[index] == '\0') {
                    return index + 1U;
                }
            }
            return 0U;
        }

        [[nodiscard]] bool measure_vector(const char *const *vector, size_t &count,
                                          size_t &string_bytes) noexcept {
            count = 0U;
            while (count <= kMaximumExecVectorEntries) {
                const char *entry = vector[count];
                if (entry == nullptr) {
                    return true;
                }
                if (count == kMaximumExecVectorEntries ||
                    string_bytes >= kExecArgumentEnvironmentByteLimit) {
                    return false;
                }
                const size_t remaining = kExecArgumentEnvironmentByteLimit - string_bytes;
                const size_t entry_size = bounded_string_size(entry, remaining);
                if (entry_size == 0U) {
                    return false;
                }
                string_bytes += entry_size;
                ++count;
            }
            return false;
        }

        [[nodiscard]] bool read_program_header(const bootfs::FileRecord &file,
                                               const Elf64_Ehdr &header, uint16_t index,
                                               Elf64_Phdr &program_header) noexcept {
            const uint64_t offset =
                header.e_phoff + static_cast<uint64_t>(index) * sizeof(Elf64_Phdr);
            if (offset > file.size || sizeof(Elf64_Phdr) > file.size - offset) {
                return false;
            }
            std::memcpy(&program_header, file.data + offset, sizeof(program_header));
            return true;
        }

        [[nodiscard]] bool validate_header(const bootfs::FileRecord &file,
                                           Elf64_Ehdr &header) noexcept {
            if (file.data == nullptr || file.size < sizeof(header)) {
                return false;
            }
            std::memcpy(&header, file.data, sizeof(header));
            if (header.e_ident[0] != ELFMAG0 || header.e_ident[1] != ELFMAG1 ||
                header.e_ident[2] != ELFMAG2 || header.e_ident[3] != ELFMAG3 ||
                header.e_ident[4] != ELFCLASS64 || header.e_ident[5] != ELFDATA2LSB ||
                header.e_ident[6] != EV_CURRENT || header.e_type != ET_EXEC ||
                header.e_machine != EM_X86_64 || header.e_version != EV_CURRENT ||
                header.e_ehsize != sizeof(Elf64_Ehdr) || header.e_phentsize != sizeof(Elf64_Phdr) ||
                header.e_phnum == 0U || header.e_phnum > kMaximumProgramHeaders) {
                return false;
            }
            if (multiply_overflows(header.e_phnum, sizeof(Elf64_Phdr))) {
                return false;
            }
            const uint64_t table_size = static_cast<uint64_t>(header.e_phnum) * sizeof(Elf64_Phdr);
            return header.e_phoff <= file.size && table_size <= file.size - header.e_phoff;
        }

        [[nodiscard]] bool validate_load_segment(const bootfs::FileRecord &file,
                                                 const Elf64_Phdr &segment, uint64_t &page_start,
                                                 uint64_t &page_end) noexcept {
            if (segment.p_filesz > segment.p_memsz || segment.p_offset > file.size ||
                segment.p_filesz > file.size - segment.p_offset ||
                add_overflows(segment.p_vaddr, segment.p_memsz) ||
                segment.p_vaddr + segment.p_memsz > kUserCanonicalLimit ||
                ((segment.p_flags & PF_W) != 0U && (segment.p_flags & PF_X) != 0U)) {
                return false;
            }
            if (segment.p_memsz == 0U) {
                page_start = 0U;
                page_end = 0U;
                return true;
            }
            if (segment.p_align > 1U &&
                (!is_power_of_two(segment.p_align) ||
                 ((segment.p_vaddr - segment.p_offset) & (segment.p_align - 1U)) != 0U)) {
                return false;
            }
            page_start = align_down(segment.p_vaddr);
            return align_up(segment.p_vaddr + segment.p_memsz, page_end) &&
                   page_end <= kUserCanonicalLimit;
        }

        [[nodiscard]] bool load_segment(const bootfs::FileRecord &file, const Elf64_Phdr &segment,
                                        UserAddressSpace &address_space) noexcept {
            uint64_t page_start = 0U;
            uint64_t page_end = 0U;
            if (!validate_load_segment(file, segment, page_start, page_end)) {
                return false;
            }
            if (segment.p_memsz == 0U) {
                return true;
            }

            UserPageFlags flags = UserPageFlags::ReadOnly;
            if ((segment.p_flags & PF_W) != 0U) {
                flags = flags | UserPageFlags::Writable;
            }
            if ((segment.p_flags & PF_X) != 0U) {
                flags = flags | UserPageFlags::Executable;
            }
            for (uint64_t page = page_start; page < page_end; page += kPageSize) {
                if (!map_zeroed_user_page(address_space, page, flags)) {
                    return false;
                }
            }
            return copy_to_user_address_space(address_space, segment.p_vaddr,
                                              file.data + segment.p_offset,
                                              static_cast<size_t>(segment.p_filesz));
        }

        [[nodiscard]] Elf64LoadError map_initial_stack(UserAddressSpace &address_space,
                                                       const char *pathname,
                                                       const char *const *arguments,
                                                       const char *const *environment,
                                                       uint64_t &stack_pointer) noexcept {
            const char *default_arguments[] = {pathname, nullptr};
            if (arguments == nullptr) {
                arguments = default_arguments;
            }

            constexpr char kDefaultPath[] = "PATH=/bin:/usr/bin";
            constexpr char kDefaultTerm[] = "TERM=xinim";
            const char *default_environment[] = {kDefaultPath, kDefaultTerm, nullptr};
            if (environment == nullptr) {
                environment = default_environment;
            }

            size_t argument_count = 0U;
            size_t environment_count = 0U;
            size_t string_bytes = 0U;
            if (!measure_vector(arguments, argument_count, string_bytes) ||
                !measure_vector(environment, environment_count, string_bytes)) {
                return Elf64LoadError::ArgumentsTooLarge;
            }
            size_t required_bytes = 0U;
            if (!exec_stack_layout_fits(argument_count, environment_count, string_bytes,
                                        required_bytes)) {
                return Elf64LoadError::ArgumentsTooLarge;
            }

            const uint64_t stack_bottom = kInitialUserStackBottom;
            for (uint64_t page = stack_bottom; page < kInitialUserStackTop; page += kPageSize) {
                if (!map_zeroed_user_page(address_space, page, UserPageFlags::Writable)) {
                    return Elf64LoadError::OutOfMemory;
                }
            }

            uint64_t cursor = kInitialUserStackTop;
            uint64_t argument_addresses[kMaximumExecVectorEntries]{};
            uint64_t environment_addresses[kMaximumExecVectorEntries]{};
            for (size_t index = argument_count; index > 0U; --index) {
                const char *argument = arguments[index - 1U];
                const size_t argument_size = string_length(argument) + 1U;
                cursor -= argument_size;
                argument_addresses[index - 1U] = cursor;
                if (!copy_to_user_address_space(address_space, cursor, argument, argument_size)) {
                    return Elf64LoadError::OutOfMemory;
                }
            }
            for (size_t index = environment_count; index > 0U; --index) {
                const char *entry = environment[index - 1U];
                const size_t entry_size = string_length(entry) + 1U;
                cursor -= entry_size;
                environment_addresses[index - 1U] = cursor;
                if (!copy_to_user_address_space(address_space, cursor, entry, entry_size)) {
                    return Elf64LoadError::OutOfMemory;
                }
            }

            uint64_t initial_words[2U * kMaximumExecVectorEntries + kExecFixedLayoutWords]{};
            size_t word_count = 0U;
            initial_words[word_count++] = argument_count;
            for (size_t index = 0U; index < argument_count; ++index) {
                initial_words[word_count++] = argument_addresses[index];
            }
            initial_words[word_count++] = 0U;
            for (size_t index = 0U; index < environment_count; ++index) {
                initial_words[word_count++] = environment_addresses[index];
            }
            initial_words[word_count++] = 0U;
            initial_words[word_count++] = kAuxiliaryTypePageSize;
            initial_words[word_count++] = kPageSize;
            initial_words[word_count++] = kAuxiliaryTypeNull;
            initial_words[word_count++] = 0U;
            const size_t vector_size = word_count * sizeof(uint64_t);
            if (cursor < stack_bottom + vector_size) {
                return Elf64LoadError::ArgumentsTooLarge;
            }
            cursor = (cursor - vector_size) & ~0xFULL;
            if (cursor < stack_bottom ||
                !copy_to_user_address_space(address_space, cursor, initial_words, vector_size)) {
                return Elf64LoadError::OutOfMemory;
            }
            stack_pointer = cursor;
            return Elf64LoadError::None;
        }

    } // namespace

    Elf64LoadError load_bootfs_elf64_user_image(const char *pathname, Elf64UserImage &image,
                                                const char *const *arguments,
                                                const char *const *environment) noexcept {
        image = {};
        const bootfs::FileRecord *file = bootfs::find(pathname);
        if (file == nullptr || file->is_directory || !file->executable) {
            return Elf64LoadError::NotFound;
        }

        Elf64_Ehdr header{};
        if (!validate_header(*file, header) || header.e_entry >= kUserCanonicalLimit) {
            return Elf64LoadError::InvalidImage;
        }

        bool entry_is_executable = false;
        uint64_t highest_segment_end = 0U;
        for (uint16_t index = 0U; index < header.e_phnum; ++index) {
            Elf64_Phdr segment{};
            if (!read_program_header(*file, header, index, segment)) {
                return Elf64LoadError::InvalidImage;
            }
            if (segment.p_type == PT_INTERP || segment.p_type == PT_DYNAMIC) {
                return Elf64LoadError::UnsupportedImage;
            }
            if (segment.p_type != PT_LOAD) {
                continue;
            }
            uint64_t page_start = 0U;
            uint64_t page_end = 0U;
            if (!validate_load_segment(*file, segment, page_start, page_end)) {
                return Elf64LoadError::InvalidImage;
            }
            if (segment.p_memsz == 0U) {
                continue;
            }
            for (uint16_t previous_index = 0U; previous_index < index; ++previous_index) {
                Elf64_Phdr previous{};
                if (!read_program_header(*file, header, previous_index, previous) ||
                    previous.p_type != PT_LOAD) {
                    continue;
                }
                uint64_t previous_start = 0U;
                uint64_t previous_end = 0U;
                if (!validate_load_segment(*file, previous, previous_start, previous_end) ||
                    (page_start < previous_end && previous_start < page_end)) {
                    return Elf64LoadError::InvalidImage;
                }
            }
            const uint64_t segment_end = segment.p_vaddr + segment.p_memsz;
            if (segment_end > highest_segment_end) {
                highest_segment_end = segment_end;
            }
            if ((segment.p_flags & PF_X) != 0U && header.e_entry >= segment.p_vaddr &&
                header.e_entry < segment_end) {
                entry_is_executable = true;
            }
        }
        if (!entry_is_executable || highest_segment_end == 0U) {
            return Elf64LoadError::InvalidImage;
        }

        if (!create_user_address_space(image.address_space)) {
            return Elf64LoadError::OutOfMemory;
        }
        for (uint16_t index = 0U; index < header.e_phnum; ++index) {
            Elf64_Phdr segment{};
            if (!read_program_header(*file, header, index, segment) ||
                (segment.p_type == PT_LOAD && !load_segment(*file, segment, image.address_space))) {
                destroy_user_address_space(image.address_space);
                return Elf64LoadError::OutOfMemory;
            }
        }
        const Elf64LoadError stack_error = map_initial_stack(
            image.address_space, pathname, arguments, environment, image.stack_pointer);
        if (stack_error != Elf64LoadError::None) {
            destroy_user_address_space(image.address_space);
            return stack_error;
        }

        uint64_t aligned_break = 0U;
        if (!align_up(highest_segment_end, aligned_break)) {
            destroy_user_address_space(image.address_space);
            return Elf64LoadError::InvalidImage;
        }
        image.entry_point = header.e_entry;
        image.program_break = aligned_break;
        return Elf64LoadError::None;
    }

} // namespace xinim::kernel::x86_64
