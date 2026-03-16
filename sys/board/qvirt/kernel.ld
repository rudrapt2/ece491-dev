OUTPUT_ARCH("riscv")

PHDRS {
  text PT_LOAD;
  rodata PT_LOAD;
  data PT_LOAD;
}

phys_base = 0x80060000;

ENTRY(boot)

virt_offset = 0xFFFFFFC000000000;

SECTIONS {
  . = phys_base + virt_offset;

  PROVIDE(_kimg_start = .);

  .text : AT(ADDR(.text) - virt_offset) {
    PROVIDE(_kimg_text_start = .);
    *(.text.boot) /* entry */
    *(.text .text.*)
    . = ALIGN(16);
    PROVIDE(_kimg_text_end = .);
  } :text

  . = ALIGN(4096);

  .rodata : AT(ADDR(.rodata) - virt_offset) {
    PROVIDE(_kimg_rodata_start = .);
    . = ALIGN(16);
    *(.srodata .srodata.*)
    . = ALIGN(16);
    *(.rodata .rodata.*)
    PROVIDE(_kimg_rodata_end = .);
  } :rodata

  . = ALIGN(4096);

  .data : AT(ADDR(.data) - virt_offset) {
    PROVIDE(_kimg_data_start = .);
    PROVIDE(_kimg_blob_start = .);
    *(*.data.blob)
    PROVIDE(_kimg_blob_end = .);
    *(.sdata .sdata.*)
    . = ALIGN(16);
    *(.data .data.*)
    . = ALIGN(16);
  } :data

  .bss : AT(ADDR(.bss) - virt_offset) {
    *(.data.pagetable)
    *(.sbss .sbss.*)
    . = ALIGN(16);
    *(.bss .bss.*)
    . = ALIGN(16);
    PROVIDE(_kimg_data_end = .);
  } :data

  PROVIDE(_kimg_end = .);
}
