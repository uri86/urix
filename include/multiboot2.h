/*
 * Licensed under MIT License - URIX project.
 * multiboot2.h - a header file that contains all the data structure of the multiboot 2 headers.
 * Responsibilities:
 *  - define all the types of the multiboot2 tag types
 *  - define all the structres of the multiboot2 boot information
 * Notes:
 *  - based on the multiboot2 header, documentation found in: https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html
 */


#ifndef MULTIBOOT2_H
#define MULTIBOOT2_H

#include <stdint.h>

/* Multiboot2 tag types (using your acronyms) */
#define MULTIBOOT_TAG_TYPE_END      0
#define MULTI_BOOT_TAG_TYPE_BCL     1  // Boot Command Line
#define MULTI_BOOT_TAG_TYPE_BLN     2  // Boot Loader Name
#define MULTI_BOOT_TAG_TYPE_MODULES 3
#define MULTIBOOT_TAG_TYPE_BMI      4  // Basic Memory Information
#define MULTIBOOT_TAG_TYPE_BBD      5  // BIOS Boot Device
#define MULTIBOOT_TAG_TYPE_MMAP     6  // Memory Map
#define MULTI_BOOT_TAG_TYPE_VI      7  // VBE Info
#define MULTI_BOOT_TAG_TYPE_FBI     8  // Framebuffer Info
#define MULTI_BOOT_TAG_TYPE_ES      9  // ELF Symbols
#define MULTI_BOOT_TAG_TYPE_AT      10 // APM Table
#define MULTI_BOOT_TAG_TYPE_E32STP  11 // EFI 32-bit System Table Pointer
#define MULTI_BOOT_TAG_TYPE_E64STP  12 // EFI 64-bit System Table Pointer
#define MULTI_BOOT_TAG_TYPE_ST      13 // SMBIOS Tables
#define MULTI_BOOT_TAG_TYPE_AOR     14 // ACPI old RSDP
#define MULTI_BOOT_TAG_TYPE_ANR     15 // ACPI new RSDP
#define MULTI_BOOT_TAG_TYPE_NI      16 // Networking Information
#define MULTI_BOOT_TAG_TYPE_EMMAP   17 // EFI memory map
#define MULTI_BOOT_TAG_TYPE_EBSNT   18 // EFI Boot Services Not Terminated
#define MULTI_BOOT_TAG_TYPE_E32IHP  19 // EFI 32-bit Image Handle Pointer
#define MULTI_BOOT_TAG_TYPE_E64IHP  20 // EFI 64-bit Image Handle Pointer
#define MULTI_BOOT_TAG_TYPE_ILBPA   21 // Image Load Base Physical Address

/* Memory map entry types */
#define MULTIBOOT_MMAP_RESERVED 0
#define MULTIBOOT_MMAP_AVAILABLE 1
#define MULTIBOOT_MEMORY_RESERVED 2
#define MULTIBOOT_MMAP_UMAI 3 // Useable Memory holding ACPI Information
#define MULTIBOOT_MMAP_RMH 4  // Reserved Memory which needs to be preserved on Hibernation
#define MULTIBOOT_MMAP_DRM 5  // Defective RAM modules

/* -------------------------------------------------------------------------- */
/* Multiboot2 Structures                                                      */
/* -------------------------------------------------------------------------- */

/* First tag */
typedef struct multiboot_size_tag {
    uint32_t total_size;
    uint32_t reserved;
} multiboot_size_tag;

/* Generic tag */
typedef struct multiboot_tag {
    uint32_t type;
    uint32_t size;
} multiboot_tag;

/* type 1: BCL */
typedef struct multiboot_tag_bcl {
    uint32_t type;
    uint32_t size;
    uint8_t string[]; // zero-terminated UTF-8 string
} multiboot_tag_bcl;

/* type 2: BLN */
typedef struct multiboot_tag_bln {
    uint32_t type;
    uint32_t size;
    uint8_t string[];
} multiboot_tag_bln;

/* type 3: MODULES */
typedef struct multiboot_tag_modules {
    uint32_t type;
    uint32_t size;
    uint32_t mod_start;
    uint32_t mod_end;
    uint8_t string[];
} multiboot_tag_modules;

/* type 4: BMI */
typedef struct multiboot_tag_bmi {
    uint32_t type;
    uint32_t size;
    uint32_t mem_lower;
    uint32_t mem_upper;
} multiboot_tag_bmi;

/* type 5: BBD */
typedef struct multiboot_tag_bbd {
    uint32_t type;
    uint32_t size;
    uint32_t biosdev;
    uint32_t partition;
    uint32_t sub_partition;
} multiboot_tag_bbd;

/* type 6: MMAP */

typedef struct multiboot_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
} multiboot_mmap_entry;

typedef struct multiboot_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    multiboot_mmap_entry entries[];
} multiboot_tag_mmap;

/* type 7: VI */
typedef struct multiboot_tag_vi {
    uint32_t type;
    uint32_t size;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint8_t vbe_control_info[512];
    uint8_t vbe_mode_info[256];
} multiboot_tag_vi;

/* type 8: FBI */
typedef struct multiboot_tag_fbi {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint16_t reserved;
    union {
        struct {
            uint32_t framebuffer_palette_num_colors;
            struct {
                uint8_t red;
                uint8_t green;
                uint8_t blue;
            } framebuffer_palette[];
        };
        struct {
            uint8_t framebuffer_red_field_position;
            uint8_t framebuffer_red_mask_size;
            uint8_t framebuffer_green_field_position;
            uint8_t framebuffer_green_mask_size;
            uint8_t framebuffer_blue_field_position;
            uint8_t framebuffer_blue_mask_size;
        };
    };
} multiboot_tag_fbi;

/* type 9: ES */
typedef struct multiboot_tag_es {
    uint32_t type;
    uint32_t size;
    uint32_t num;
    uint32_t entsize;
    uint32_t shndx;
    char sections[];
} multiboot_tag_es;

/* type 10: AT */
typedef struct multiboot_tag_at {
    uint32_t type;
    uint32_t size;
    uint16_t version;
    uint16_t cseg;
    uint32_t offset;
    uint16_t cseg_16;
    uint16_t dseg;
    uint16_t flags;
    uint16_t cseg_len;
    uint16_t cseg_16_len;
    uint16_t dseg_len;
} multiboot_tag_at;

/* type 11: E32STP */
typedef struct multiboot_tag_e32stp {
    uint32_t type;
    uint32_t size;
    uint32_t pointer;
} multiboot_tag_e32stp;

/* type 12: E64STP */
typedef struct multiboot_tag_e64stp {
    uint32_t type;
    uint32_t size;
    uint64_t pointer;
} multiboot_tag_e64stp;

/* type 13: ST */
typedef struct multiboot_tag_st {
    uint32_t type;
    uint32_t size;
    uint8_t tables[];
} multiboot_tag_st;

/* type 14: AOR */
typedef struct multiboot_tag_aor {
    uint32_t type;
    uint32_t size;
    uint8_t rsdp[];
} multiboot_tag_aor;

/* type 15: ANR */
typedef struct multiboot_tag_anr {
    uint32_t type;
    uint32_t size;
    uint8_t rsdp[];
} multiboot_tag_anr;

/* type 16: NI */
typedef struct multiboot_tag_ni {
    uint32_t type;
    uint32_t size;
    uint8_t dhcpack[];
} multiboot_tag_ni;

/* type 17: EMMAP */
typedef struct multiboot_tag_emmap {
    uint32_t type;
    uint32_t size;
    uint32_t descr_size;
    uint32_t descr_vers;
    uint8_t efi_mmap[];
} multiboot_tag_emmap;

/* type 18: EBSNT */
typedef struct multiboot_tag_ebsnt {
    uint32_t type;
    uint32_t size;
} multiboot_tag_ebsnt;

/* type 19: E32IHP */
typedef struct multiboot_tag_e32ihp {
    uint32_t type;
    uint32_t size;
    uint32_t pointer;
} multiboot_tag_e32ihp;

/* type 20: E64IHP */
typedef struct multiboot_tag_e64ihp {
    uint32_t type;
    uint32_t size;
    uint64_t pointer;
} multiboot_tag_e64ihp;

/* type 21: ILBPA */
typedef struct multiboot_tag_ilbpa {
    uint32_t type;
    uint32_t size;
    uint32_t load_base_addr;
} multiboot_tag_ilbpa;

#endif /* MULTIBOOT2_H */
