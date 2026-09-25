#pragma once

#include <stddef.h>
#include <stdint.h>

// ELF-64 data types using standard *_t naming convention.
typedef uint64_t elf64_addr_t;
typedef uint64_t elf64_off_t;
typedef uint16_t elf64_half_t;
typedef uint32_t elf64_word_t;
typedef int32_t elf64_sword_t;
typedef uint64_t elf64_xword_t;
typedef int64_t elf64_sxword_t;

// ELF Identification indices (e_ident).
#define EI_MAG0 0
#define EI_MAG1 1
#define EI_MAG2 2
#define EI_MAG3 3
#define EI_CLASS 4
#define EI_DATA 5
#define EI_VERSION 6
#define EI_OSABI 7
#define EI_ABIVERSION 8
#define EI_PAD 9
#define EI_NIDENT 16

// ELF Magic numbers.
#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

// ELF File Class (e_ident[EI_CLASS]).
#define ELFCLASSNONE 0
#define ELFCLASS32 1
#define ELFCLASS64 2

// ELF Data Encoding (e_ident[EI_DATA]).
#define ELFDATANONE 0
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

// ELF Version (e_ident[EI_VERSION] and e_version).
#define EV_NONE 0
#define EV_CURRENT 1

// ELF OS ABI (e_ident[EI_OSABI]).
#define ELFOSABI_NONE 0
#define ELFOSABI_SYSV 0

// ELF File Types (e_type).
#define ET_NONE 0
#define ET_REL 1
#define ET_EXEC 2
#define ET_DYN 3
#define ET_CORE 4

// ELF Machine Architectures (e_machine).
#define EM_NONE 0
#define EM_386 3
#define EM_X86_64 62
#define EM_AARCH64 183
#define EM_RISCV 243

// ELF File Header.
struct elf64_ehdr {
  unsigned char e_ident[EI_NIDENT];
  elf64_half_t e_type;
  elf64_half_t e_machine;
  elf64_word_t e_version;
  elf64_addr_t e_entry;
  elf64_off_t e_phoff;
  elf64_off_t e_shoff;
  elf64_word_t e_flags;
  elf64_half_t e_ehsize;
  elf64_half_t e_phentsize;
  elf64_half_t e_phnum;
  elf64_half_t e_shentsize;
  elf64_half_t e_shnum;
  elf64_half_t e_shstrndx;
};
typedef struct elf64_ehdr elf64_ehdr_t;

// Program Header Types (p_type).
#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_SHLIB 5
#define PT_PHDR 6
#define PT_TLS 7

// Program Header Flags (p_flags).
#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4
#define PF_MASKOS 0x0ff00000
#define PF_MASKPROC 0xf0000000

// Program Header.
struct elf64_phdr {
  elf64_word_t p_type;
  elf64_word_t p_flags;
  elf64_off_t p_offset;
  elf64_addr_t p_vaddr;
  elf64_addr_t p_paddr;
  elf64_xword_t p_filesz;
  elf64_xword_t p_memsz;
  elf64_xword_t p_align;
};
typedef struct elf64_phdr elf64_phdr_t;

// Section Header Types (sh_type).
#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_RELA 4
#define SHT_HASH 5
#define SHT_DYNAMIC 6
#define SHT_NOTE 7
#define SHT_NOBITS 8
#define SHT_REL 9
#define SHT_SHLIB 10
#define SHT_DYNSYM 11

// Section Header Flags (sh_flags).
#define SHF_WRITE 0x1
#define SHF_ALLOC 0x2
#define SHF_EXECINSTR 0x4

// Section Header.
struct elf64_shdr {
  elf64_word_t sh_name;
  elf64_word_t sh_type;
  elf64_xword_t sh_flags;
  elf64_addr_t sh_addr;
  elf64_off_t sh_offset;
  elf64_xword_t sh_size;
  elf64_word_t sh_link;
  elf64_word_t sh_info;
  elf64_xword_t sh_addralign;
  elf64_xword_t sh_entsize;
};
typedef struct elf64_shdr elf64_shdr_t;
