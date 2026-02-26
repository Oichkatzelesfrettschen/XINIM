/**
 * @file test_elf_parser.cpp
 * @brief Unit tests for ELF64 header validation and flag conversion.
 *
 * WHY: elf_loader.hpp exposes validate_elf_header() and elf_flags_to_prot()
 *      which operate on in-memory structures with no kernel I/O dependencies.
 *      These can be tested host-side by constructing valid and invalid ELF headers.
 *
 * NOTE: load_elf_binary() and load_segment() require VFS and paging; those
 *       are tested in QEMU integration tests.
 */

#include "../kernel/elf_loader.hpp"
#include <cassert>
#include <cstring>

using namespace xinim::kernel;

// Build a minimal valid ELF64 x86_64 executable header.
static Elf64_Ehdr make_valid_elf64_exec() {
    Elf64_Ehdr h{};
    h.e_ident[0] = ELFMAG0;
    h.e_ident[1] = ELFMAG1;
    h.e_ident[2] = ELFMAG2;
    h.e_ident[3] = ELFMAG3;
    h.e_ident[4] = ELFCLASS64;
    h.e_ident[5] = ELFDATA2LSB;
    h.e_ident[6] = EV_CURRENT;
    h.e_type       = ET_EXEC;
    h.e_machine    = EM_X86_64;
    h.e_version    = EV_CURRENT;
    h.e_entry      = 0x400000;
    // validate_elf_header checks e_phentsize and e_phnum bounds (1..128)
    h.e_phentsize  = static_cast<uint16_t>(sizeof(Elf64_Phdr));
    h.e_phnum      = 1;
    return h;
}

static void test_validate_valid_exec() {
    Elf64_Ehdr h = make_valid_elf64_exec();
    assert(validate_elf_header(&h));
}

static void test_validate_valid_dyn() {
    Elf64_Ehdr h = make_valid_elf64_exec();
    h.e_type = ET_DYN;
    assert(validate_elf_header(&h));
}

static void test_validate_bad_magic() {
    Elf64_Ehdr h = make_valid_elf64_exec();
    h.e_ident[0] = 0x00; // corrupt magic byte 0
    assert(!validate_elf_header(&h));
}

static void test_validate_bad_class_32bit() {
    Elf64_Ehdr h = make_valid_elf64_exec();
    h.e_ident[4] = ELFCLASS32; // 32-bit class rejected
    assert(!validate_elf_header(&h));
}

static void test_validate_bad_endian() {
    Elf64_Ehdr h = make_valid_elf64_exec();
    h.e_ident[5] = ELFDATA2MSB; // big-endian rejected on x86_64
    assert(!validate_elf_header(&h));
}

static void test_validate_bad_machine() {
    Elf64_Ehdr h = make_valid_elf64_exec();
    h.e_machine = EM_386; // wrong architecture
    assert(!validate_elf_header(&h));
}

static void test_validate_bad_type_reloc() {
    Elf64_Ehdr h = make_valid_elf64_exec();
    h.e_type = ET_REL; // relocatable, not executable or shared
    assert(!validate_elf_header(&h));
}

static void test_elf_flags_to_prot_read_only() {
    uint32_t prot = elf_flags_to_prot(PF_R);
    assert(prot & PROT_READ);
    assert(!(prot & PROT_WRITE));
    assert(!(prot & PROT_EXEC));
}

static void test_elf_flags_to_prot_rwx() {
    uint32_t prot = elf_flags_to_prot(PF_R | PF_W | PF_X);
    assert(prot & PROT_READ);
    assert(prot & PROT_WRITE);
    assert(prot & PROT_EXEC);
}

static void test_elf_flags_to_prot_rx_no_write() {
    uint32_t prot = elf_flags_to_prot(PF_R | PF_X);
    assert(prot & PROT_READ);
    assert(!(prot & PROT_WRITE));
    assert(prot & PROT_EXEC);
}

static void test_elf_flags_to_prot_none() {
    uint32_t prot = elf_flags_to_prot(0);
    assert(prot == PROT_NONE);
}

int main() {
    test_validate_valid_exec();
    test_validate_valid_dyn();
    test_validate_bad_magic();
    test_validate_bad_class_32bit();
    test_validate_bad_endian();
    test_validate_bad_machine();
    test_validate_bad_type_reloc();
    test_elf_flags_to_prot_read_only();
    test_elf_flags_to_prot_rwx();
    test_elf_flags_to_prot_rx_no_write();
    test_elf_flags_to_prot_none();
    return 0;
}
