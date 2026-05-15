OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(EfiMain)
PHDRS
{
  text PT_LOAD FLAGS(5);
  data PT_LOAD FLAGS(6);
  reloc PT_LOAD FLAGS(4);
}
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
  } :text
  . = ALIGN(0x1000);
  .data : {
    *(.data)
    *(.data.*)
    *(.bss)
    *(.bss.*)
    *(COMMON)
  } :data
  .dynamic : { *(.dynamic) } :data
  .dynsym : { *(.dynsym) } :data
  .dynstr : { *(.dynstr) } :data
  .hash : { *(.hash) } :data
  .gnu.hash : { *(.gnu.hash) } :data
  .rela : {
    *(.rela*)
  } :data
  . = ALIGN(0x1000);
  .reloc : {
    *(.reloc)
  } :reloc
  /DISCARD/ : {
    *(.eh_frame)
    *(.note*)
    *(.comment)
  }
}
