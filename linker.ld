OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS {
  . = 1M;                 /* load kernel at 1 MiB */

  _kernel_start = .;

  .text : ALIGN(4K) {
    *(.multiboot_header)
    *(.text*)
  }

  .rodata : ALIGN(4K) {
    *(.rodata*)
  }

  .data : ALIGN(4K) {
    *(.data*)
  }

  .bss : ALIGN(4K) {
    *(.bss*)
    *(COMMON)
  }

  _kernel_end = .;
}
