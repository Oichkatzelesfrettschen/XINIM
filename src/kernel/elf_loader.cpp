/**
 * @file elf_loader.cpp
 * @brief ELF binary parsing and loading implementation
 *
 * Implements ELF64 binary loading for x86_64.
 * Week 10 Phase 1: Static binaries only
 * Week 11: Dynamic linking support
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#include "elf_loader.hpp"
#include "vfs_interface.hpp"
#include "fd_table.hpp"
#include "../early/serial_16550.hpp"
#include <cerrno>
#include <cstring>
#include <cstdio>

extern xinim::early::Serial16550 early_serial;

namespace xinim::kernel {

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Validate ELF header
 *
 * Performs comprehensive validation of ELF64 header.
 */
bool validate_elf_header(const Elf64_Ehdr* ehdr) {
    // Check magic number (0x7f 'E' 'L' 'F')
    if (ehdr->e_ident[0] != ELFMAG0 ||
        ehdr->e_ident[1] != ELFMAG1 ||
        ehdr->e_ident[2] != ELFMAG2 ||
        ehdr->e_ident[3] != ELFMAG3) {
        char log_buf[128];
        std::snprintf(log_buf, sizeof(log_buf),
                      "[ELF] Invalid magic: 0x%02x 0x%02x 0x%02x 0x%02x\n",
                      ehdr->e_ident[0], ehdr->e_ident[1],
                      ehdr->e_ident[2], ehdr->e_ident[3]);
        early_serial.write(log_buf);
        return false;
    }

    // Check class (must be ELF64)
    if (ehdr->e_ident[4] != ELFCLASS64) {
        early_serial.write("[ELF] Not ELF64 (32-bit not supported)\n");
        return false;
    }

    // Check data encoding (must be little-endian)
    if (ehdr->e_ident[5] != ELFDATA2LSB) {
        early_serial.write("[ELF] Not little-endian\n");
        return false;
    }

    // Check version
    if (ehdr->e_ident[6] != EV_CURRENT) {
        early_serial.write("[ELF] Invalid ELF version\n");
        return false;
    }

    // Check machine (must be x86_64)
    if (ehdr->e_machine != EM_X86_64) {
        char log_buf[128];
        std::snprintf(log_buf, sizeof(log_buf),
                      "[ELF] Unsupported architecture: %u (expected x86_64)\n",
                      ehdr->e_machine);
        early_serial.write(log_buf);
        return false;
    }

    // Check type (must be executable or shared object)
    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
        char log_buf[128];
        std::snprintf(log_buf, sizeof(log_buf),
                      "[ELF] Invalid type: %u (expected ET_EXEC or ET_DYN)\n",
                      ehdr->e_type);
        early_serial.write(log_buf);
        return false;
    }

    // Check version field
    if (ehdr->e_version != EV_CURRENT) {
        early_serial.write("[ELF] Invalid e_version\n");
        return false;
    }

    // Basic sanity checks
    if (ehdr->e_phentsize != sizeof(Elf64_Phdr)) {
        early_serial.write("[ELF] Invalid program header size\n");
        return false;
    }

    if (ehdr->e_phnum == 0 || ehdr->e_phnum > 128) {
        char log_buf[128];
        std::snprintf(log_buf, sizeof(log_buf),
                      "[ELF] Invalid program header count: %u\n",
                      ehdr->e_phnum);
        early_serial.write(log_buf);
        return false;
    }

    return true;
}

/**
 * @brief Convert ELF segment flags to memory protection flags
 */
uint32_t elf_flags_to_prot(uint32_t elf_flags) {
    uint32_t prot = 0;

    if (elf_flags & PF_R) prot |= PROT_READ;
    if (elf_flags & PF_W) prot |= PROT_WRITE;
    if (elf_flags & PF_X) prot |= PROT_EXEC;

    return prot;
}

// ============================================================================
// Segment Loading
// ============================================================================

/**
 * @brief Load PT_LOAD segment into memory
 *
 * Week 10 Phase 1: Simple implementation - load entire segment immediately
 * Week 11: Enhanced with demand paging
 */
int load_segment(void* inode, const Elf64_Phdr* phdr) {
    char log_buf[256];

    // Log segment being loaded
    std::snprintf(log_buf, sizeof(log_buf),
                  "[ELF] Loading segment: vaddr=0x%lx size=0x%lx flags=%c%c%c\n",
                  phdr->p_vaddr, phdr->p_memsz,
                  (phdr->p_flags & PF_R) ? 'R' : '-',
                  (phdr->p_flags & PF_W) ? 'W' : '-',
                  (phdr->p_flags & PF_X) ? 'X' : '-');
    early_serial.write(log_buf);

    // Week 10 Phase 1: Simplified implementation
    // We'll use a simple kernel buffer approach for now
    // Week 11 will implement proper user memory mapping

    // Validate segment alignment
    if (phdr->p_align > 1 && (phdr->p_vaddr % phdr->p_align) != 0) {
        early_serial.write("[ELF] Segment not properly aligned\n");
        return -EINVAL;
    }

    // Allocate kernel buffer for segment (temporary Week 10 approach)
    // In Week 11, this will be replaced with proper user page allocation
    size_t alloc_size = (phdr->p_memsz + 0xFFF) & ~0xFFF;  // Page-aligned
    char* segment_buf = new char[alloc_size];
    if (!segment_buf) {
        early_serial.write("[ELF] Failed to allocate segment buffer\n");
        return -ENOMEM;
    }

    // Initialize to zero (for .bss section)
    std::memset(segment_buf, 0, alloc_size);

    // Read file content if p_filesz > 0
    if (phdr->p_filesz > 0) {
        ssize_t bytes_read = vfs_read(inode, segment_buf, phdr->p_filesz, phdr->p_offset);
        if (bytes_read != (ssize_t)phdr->p_filesz) {
            delete[] segment_buf;
            std::snprintf(log_buf, sizeof(log_buf),
                          "[ELF] Failed to read segment: expected %lu, got %ld\n",
                          phdr->p_filesz, bytes_read);
            early_serial.write(log_buf);
            return -EIO;
        }
    }

    // Week 10 Phase 1: Store pointer in simple table
    // This is a placeholder - Week 11 will implement proper page tables
    // For now, we'll just verify the load and free the buffer
    // Real implementation will map to user address space

    std::snprintf(log_buf, sizeof(log_buf),
                  "[ELF] Segment loaded successfully (Week 10 placeholder)\n");
    early_serial.write(log_buf);

    // TODO Week 10 Phase 1: Map segment_buf to phdr->p_vaddr in user address space
    // TODO Week 11: Implement proper VMA and page table management

    // For now, just free the buffer (temporary until proper mapping implemented)
    delete[] segment_buf;

    return 0;
}

// ============================================================================
// ELF Binary Loading
// ============================================================================

/**
 * @brief Load ELF binary from file
 *
 * Main entry point for ELF loading.
 */
int load_elf_binary(const char* pathname, ElfLoadInfo* load_info) {
    char log_buf[256];

    std::snprintf(log_buf, sizeof(log_buf),
                  "[ELF] Loading binary: %s\n", pathname);
    early_serial.write(log_buf);

    // Initialize load_info
    std::memset(load_info, 0, sizeof(ElfLoadInfo));

    // Open executable file via VFS
    void* inode = vfs_lookup(pathname);
    if (!inode) {
        std::snprintf(log_buf, sizeof(log_buf),
                      "[ELF] File not found: %s\n", pathname);
        early_serial.write(log_buf);
        return -ENOENT;
    }

    // Read ELF header
    Elf64_Ehdr ehdr;
    ssize_t bytes_read = vfs_read(inode, &ehdr, sizeof(ehdr), 0);
    if (bytes_read != sizeof(ehdr)) {
        early_serial.write("[ELF] Failed to read ELF header\n");
        return -ENOEXEC;
    }

    // Validate header
    if (!validate_elf_header(&ehdr)) {
        early_serial.write("[ELF] Invalid ELF header\n");
        return -ENOEXEC;
    }

    std::snprintf(log_buf, sizeof(log_buf),
                  "[ELF] Valid ELF64 binary: entry=0x%lx phnum=%u\n",
                  ehdr.e_entry, ehdr.e_phnum);
    early_serial.write(log_buf);

    // Read program headers
    size_t phdr_table_size = ehdr.e_phnum * sizeof(Elf64_Phdr);
    Elf64_Phdr* phdrs = new Elf64_Phdr[ehdr.e_phnum];
    if (!phdrs) {
        early_serial.write("[ELF] Failed to allocate program header table\n");
        return -ENOMEM;
    }

    bytes_read = vfs_read(inode, phdrs, phdr_table_size, ehdr.e_phoff);
    if (bytes_read != (ssize_t)phdr_table_size) {
        delete[] phdrs;
        early_serial.write("[ELF] Failed to read program headers\n");
        return -ENOEXEC;
    }

    // Process program headers
    uint64_t highest_addr = 0;
    bool has_interpreter = false;

    for (int i = 0; i < ehdr.e_phnum; i++) {
        Elf64_Phdr* phdr = &phdrs[i];

        switch (phdr->p_type) {
            case PT_LOAD: {
                // Load loadable segment
                int ret = load_segment(inode, phdr);
                if (ret < 0) {
                    delete[] phdrs;
                    std::snprintf(log_buf, sizeof(log_buf),
                                  "[ELF] Failed to load segment %d: %d\n", i, ret);
                    early_serial.write(log_buf);
                    return ret;
                }

                // Track highest address for brk
                uint64_t seg_end = phdr->p_vaddr + phdr->p_memsz;
                if (seg_end > highest_addr) {
                    highest_addr = seg_end;
                }
                break;
            }

            case PT_INTERP: {
                // Dynamic linker requested
                has_interpreter = true;

                if (phdr->p_filesz >= sizeof(load_info->interpreter)) {
                    delete[] phdrs;
                    early_serial.write("[ELF] Interpreter path too long\n");
                    return -EINVAL;
                }

                bytes_read = vfs_read(inode, load_info->interpreter,
                                     phdr->p_filesz, phdr->p_offset);
                if (bytes_read != (ssize_t)phdr->p_filesz) {
                    delete[] phdrs;
                    early_serial.write("[ELF] Failed to read interpreter path\n");
                    return -EIO;
                }

                // Null-terminate
                load_info->interpreter[phdr->p_filesz] = '\0';

                std::snprintf(log_buf, sizeof(log_buf),
                              "[ELF] Dynamic linker: %s\n", load_info->interpreter);
                early_serial.write(log_buf);
                break;
            }

            case PT_DYNAMIC:
                // Dynamic linking information (Week 11)
                early_serial.write("[ELF] Found PT_DYNAMIC (Week 11 feature)\n");
                break;

            case PT_PHDR:
                // Program header table itself
                break;

            case PT_NOTE:
                // Auxiliary information
                break;

            default:
                std::snprintf(log_buf, sizeof(log_buf),
                              "[ELF] Skipping segment type %u\n", phdr->p_type);
                early_serial.write(log_buf);
                break;
        }
    }

    delete[] phdrs;

    // Set up load info
    load_info->entry_point = ehdr.e_entry;
    load_info->stack_top = USER_STACK_TOP;
    load_info->brk_start = (highest_addr + 0xFFF) & ~0xFFF;  // Page-aligned
    load_info->has_interpreter = has_interpreter;

    std::snprintf(log_buf, sizeof(log_buf),
                  "[ELF] Load complete: entry=0x%lx brk=0x%lx interpreter=%s\n",
                  load_info->entry_point, load_info->brk_start,
                  has_interpreter ? "yes" : "no");
    early_serial.write(log_buf);

    return 0;
}

} // namespace xinim::kernel
