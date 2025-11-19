/**
 * @file elf_loader.hpp
 * @brief ELF binary parsing and loading
 *
 * Supports ELF64 on x86_64.
 * Week 10 Phase 1: Static binaries only
 * Week 11: Dynamic linking support
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#pragma once
#include <cstdint>

namespace xinim::kernel {

// ============================================================================
// ELF64 Header
// ============================================================================

#define EI_NIDENT 16

/**
 * @brief ELF64 file header
 *
 * Located at offset 0 of the ELF file.
 */
struct Elf64_Ehdr {
    uint8_t  e_ident[EI_NIDENT];  ///< Magic number, class, endian, version
    uint16_t e_type;               ///< Object file type (ET_EXEC, ET_DYN)
    uint16_t e_machine;            ///< Architecture (EM_X86_64)
    uint32_t e_version;            ///< ELF version
    uint64_t e_entry;              ///< Entry point address
    uint64_t e_phoff;              ///< Program header offset
    uint64_t e_shoff;              ///< Section header offset
    uint32_t e_flags;              ///< Processor-specific flags
    uint16_t e_ehsize;             ///< ELF header size
    uint16_t e_phentsize;          ///< Program header entry size
    uint16_t e_phnum;              ///< Number of program headers
    uint16_t e_shentsize;          ///< Section header entry size
    uint16_t e_shnum;              ///< Number of section headers
    uint16_t e_shstrndx;           ///< String table index
} __attribute__((packed));

// ============================================================================
// ELF64 Program Header
// ============================================================================

/**
 * @brief ELF64 program header
 *
 * Describes a segment or other information needed to prepare the program for execution.
 */
struct Elf64_Phdr {
    uint32_t p_type;               ///< Segment type (PT_LOAD, PT_INTERP, PT_DYNAMIC)
    uint32_t p_flags;              ///< Segment flags (PF_R, PF_W, PF_X)
    uint64_t p_offset;             ///< File offset
    uint64_t p_vaddr;              ///< Virtual address
    uint64_t p_paddr;              ///< Physical address (unused)
    uint64_t p_filesz;             ///< Size in file
    uint64_t p_memsz;              ///< Size in memory (>= filesz for .bss)
    uint64_t p_align;              ///< Alignment
} __attribute__((packed));

// ============================================================================
// ELF Constants
// ============================================================================

// Magic number
#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

// Class
#define ELFCLASS32 1
#define ELFCLASS64 2

// Data encoding
#define ELFDATA2LSB 1  // Little-endian
#define ELFDATA2MSB 2  // Big-endian

// Type
#define ET_NONE 0    // No file type
#define ET_REL  1    // Relocatable file
#define ET_EXEC 2    // Executable file
#define ET_DYN  3    // Shared object file
#define ET_CORE 4    // Core file

// Machine
#define EM_NONE    0  // No machine
#define EM_386    3  // Intel 80386
#define EM_X86_64 62  // AMD x86-64

// Version
#define EV_NONE    0
#define EV_CURRENT 1

// Segment types
#define PT_NULL    0  // Unused segment
#define PT_LOAD    1  // Loadable segment
#define PT_DYNAMIC 2  // Dynamic linking information
#define PT_INTERP  3  // Interpreter pathname
#define PT_NOTE    4  // Auxiliary information
#define PT_SHLIB   5  // Reserved
#define PT_PHDR    6  // Program header table

// Segment flags
#define PF_X 0x1     // Execute
#define PF_W 0x2     // Write
#define PF_R 0x4     // Read

// Memory protection flags (for mmap compatibility)
#define PROT_NONE  0x0
#define PROT_READ  0x1
#define PROT_WRITE 0x2
#define PROT_EXEC  0x4

// User address space constants
#define USER_STACK_TOP    0x00007FFFFFFFFFFF  // Top of user stack
#define USER_STACK_SIZE   (8 * 1024 * 1024)   // 8MB default stack
#define USER_HEAP_START   0x0000000000400000  // Heap starts at 4MB

// ============================================================================
// ELF Loader Interface
// ============================================================================

/**
 * @brief ELF loading result
 *
 * Contains all information needed to execute the loaded binary.
 */
struct ElfLoadInfo {
    uint64_t entry_point;      ///< Program entry point (e_entry)
    uint64_t stack_top;        ///< Top of user stack
    uint64_t brk_start;        ///< Start of heap (for brk syscall)
    bool has_interpreter;      ///< True if dynamic linking required
    char interpreter[256];     ///< Path to dynamic linker (/lib64/ld-linux.so)
};

/**
 * @brief Load ELF binary from file
 *
 * Steps:
 * 1. Read and validate ELF header
 * 2. Parse program headers
 * 3. Load PT_LOAD segments into memory
 * 4. Zero-initialize .bss sections
 * 5. Detect PT_INTERP for dynamic linking
 * 6. Return entry point and stack information
 *
 * @param pathname Path to ELF binary (kernel-space string)
 * @param load_info Output: loading information
 * @return 0 on success, negative errno on failure
 *         -ENOENT: File not found
 *         -ENOEXEC: Not a valid ELF binary
 *         -EINVAL: Unsupported architecture or format
 *         -ENOMEM: Out of memory
 *         -EIO: I/O error reading file
 */
int load_elf_binary(const char* pathname, ElfLoadInfo* load_info);

/**
 * @brief Validate ELF header
 *
 * Checks:
 * - Magic number (0x7f 'E' 'L' 'F')
 * - Class (ELF64)
 * - Data encoding (little-endian)
 * - Machine (x86_64)
 * - Type (ET_EXEC or ET_DYN)
 * - Version (EV_CURRENT)
 *
 * @param ehdr ELF header to validate
 * @return true if valid, false otherwise
 */
bool validate_elf_header(const Elf64_Ehdr* ehdr);

/**
 * @brief Load PT_LOAD segment into memory
 *
 * Steps:
 * 1. Allocate virtual memory at p_vaddr
 * 2. Read p_filesz bytes from file at p_offset
 * 3. Zero-initialize remaining p_memsz - p_filesz bytes (for .bss)
 * 4. Set memory protection based on p_flags
 *
 * Week 10 Phase 1: All segments loaded immediately
 * Week 11: Demand paging with page fault handling
 *
 * @param inode File inode (from VFS)
 * @param phdr Program header describing segment
 * @return 0 on success, negative errno on failure
 */
int load_segment(void* inode, const Elf64_Phdr* phdr);

/**
 * @brief Convert ELF segment flags to memory protection flags
 *
 * Maps PF_R, PF_W, PF_X to PROT_READ, PROT_WRITE, PROT_EXEC.
 *
 * @param elf_flags ELF segment flags (p_flags)
 * @return Protection flags for mmap
 */
uint32_t elf_flags_to_prot(uint32_t elf_flags);

} // namespace xinim::kernel
