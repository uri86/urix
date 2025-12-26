OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

/* Move Base to -2GB Standard High Kernel Location */
KERNEL_VIRT_BASE = 0xFFFFFFFF80000000;

SECTIONS {
  /* Virtual start at Base + 1MB */
  . = KERNEL_VIRT_BASE + 1M;

  _kernel_start = .;

  /* Physical load address (AT) is still 1M */
  .text : AT(ADDR(.text) - KERNEL_VIRT_BASE) ALIGN(4K) {
      *(.multiboot_header)
      *(.text*)
  }

  .rodata : AT(ADDR(.rodata) - KERNEL_VIRT_BASE) ALIGN(4K) {
      *(.rodata*)
  }

  .data : AT(ADDR(.data) - KERNEL_VIRT_BASE) ALIGN(4K) {
      *(.data*)
  }

  .bss : AT(ADDR(.bss) - KERNEL_VIRT_BASE) ALIGN(4K) {
      *(.bss*)
      *(COMMON)
  }

  _kernel_end = .;
}