/*
 * Licensed under MIT License - URIX project.
 * elf.h - ELF64 file format definitions and loader interface
 */

#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include <stddef.h>
#include <memory/vmm.h>
#include <memory/physical/pmm.h>

/* ELF Magic Number */
#define ELF_MAGIC 0x464C457F

/* Class */
#define ELFCLASS32 1
#define ELFCLASS64 2

/* Data Encoding */
#define ELFDATA2LSB 1 /* Little endian */
#define ELFDATA2MSB 2 /* Big endian */

/* Type */
#define ET_NONE 0 /* No file type */
#define ET_REL 1  /* Relocatable file */
#define ET_EXEC 2 /* Executable file */
#define ET_DYN 3  /* Shared object file */
#define ET_CORE 4 /* Core file */

/* Machine */
#define EM_X86_64 62

/* Program Header Types */
#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_SHLIB 5 
#define PT_PHDR 6
#define PT_TLS 7

#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

/* Section Header Types */
#define SHT_NULL 0     /* Unused */
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2   /* Symbol table */
#define SHT_STRTAB 3   /* String table */
#define SHT_RELA 4
#define SHT_HASH 5     /* Symbol hash table */
#define SHT_DYNAMIC 6
#define SHT_NOTE 7
#define SHT_NOBITS 8  /* BSS section */
#define SHT_REL 9 
#define SHT_DYNSYM 11

/* ELF64 Header */
typedef struct
{
    uint8_t e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf64_ehdr_t;

/* ELF64 Program Header */
typedef struct
{
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed)) elf64_phdr_t;

/* ELF64 Section Header */
typedef struct
{
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} __attribute__((packed)) elf64_shdr_t;

/* ELF Loading Result */
typedef struct
{
    uint64_t entry_point;
    uint64_t base_address;
    uint64_t brk;
    uint64_t stack_top;
} elf_info_t;

/**
 * elf_validate - Validate an ELF64 header
 *
 * hdr: Pointer to ELF header
 *
 * Returns: 0 if valid, -1 if invalid
 */
int elf_validate(const elf64_ehdr_t *hdr);

/**
 * elf_load - Load an ELF64 executable into an address space
 *
 * data: Pointer to ELF file data in memory
 * size: Size of ELF file in bytes
 * addr_space: Target address space to load into
 * info: Output - loading information
 *
 * Returns: 0 on success, -1 on failure
 */
int elf_load(const void *data, size_t size, address_space_t *addr_space, elf_info_t *info);

/**
 * elf_load_from_file - Load an ELF executable from a file
 *
 * path: Path to ELF file in filesystem
 * addr_space: Target address space to load into
 * info: Output - loading information
 *
 * Returns: 0 on success, -1 on failure
 */
int elf_load_from_file(const char *path, address_space_t *addr_space, elf_info_t *info);

#endif /* ELF_H */