OUTPUT_ARCH("riscv")
ENTRY(_entry)

SECTIONS
{
  /* QEMU RISC-V virt RAM počinje na 0x80000000 */
  . = 0x80000000;

  /* 1. Kod sekcija: Prvo ide naš boot.S, pa ostatak koda */
  .text : {
    *(.entry_os)
    *(.text .text.*)
  }

  . = ALIGN(0x1000);

  /* 2. Read-Only podaci (konstante, stringovi) */
  .rodata : {
    *(.srodata .srodata.*)
    *(.rodata .rodata.*)
  }

  . = ALIGN(0x1000);

  /* 3. Inicijalizovani globalni i statički podaci */
  .data : {
    *(.sdata .sdata.*)
    *(.data .data.*)
  }

  . = ALIGN(0x1000);

  /* 4. Neinicijalizovani podaci (BSS - npr. kernel stek) */
  .bss : {
    __bss_start = .;
    *(.sbss .sbss.*)
    *(.bss .bss.*)
    *(COMMON)
    __bss_end = .;
  }

  . = ALIGN(0x1000);

  /* Simbol koji označava KRAJ koda i BSS-a u RAM-u.
     Sva memorija odavde pa do 128MB je slobodna za kalloc! */
  providers_end = .;
  end = .;
}