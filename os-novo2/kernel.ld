OUTPUT_ARCH("riscv")
ENTRY(_start)

SECTIONS
{
    /* QEMU 'virt' mašina mapira RAM od ove adrese naviše. */
    . = 0x80000000;

    .text : {
        *(.entry_os)    /* _start i trap_vector prvi u memoriji */
        *(.text .text.*)
    }

    .rodata : {
        *(.rodata .rodata.*)
    }

    .data : {
        *(.data .data.*)
    }

    .bss : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

    . = ALIGN(4096);
    _end = .;
    end = .;    /* čitano iz kalloc.cpp kao početak slobodne memorije */
}
