OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(EfiMain)
SECTIONS
{
  . = 0;
  ImageBase = .;
  . = 0x1000;
  .text : {
    *(.text)
    *(.text.*)
    *(.rodata)
    *(.rodata.*)
    *(.sdata)
    *(.sdata.*)
  }
  .data : {
    *(.data)
    *(.data.*)
    *(.bss)
    *(.bss.*)
    *(COMMON)
  }
  .dynamic : { *(.dynamic) }
  .dynsym : { *(.dynsym) }
  .dynstr : { *(.dynstr) }
  .hash : { *(.hash) }
  .gnu.hash : { *(.gnu.hash) }
  .rela : {
    *(.rela*)
  }
  .reloc : {
    *(.reloc)
  }
  /DISCARD/ : {
    *(.eh_frame)
    *(.note*)
    *(.comment)
  }
}
