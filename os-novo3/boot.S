.section .entry_os, "ax"
.global _start

# Redosled čuvanja/vraćanja mora tačno da odgovara RISCV::TrapFrame iz
# riscv.hpp (proveren i preko static_assert-ova tamo). Makroi postoje da se
# izbegne ručno ponavljanje istog spiska od 30 registara dva puta -- lakše
# je proveriti ispravnost jedne definicije nego dva ručno pisana bloka.
.macro SAVE_REGS
    addi sp, sp, -256
    sd ra,  0(sp)
    # NAPOMENA: ovde se čuva TRENUTNI (već umanjeni) sp, ne sp u trenutku
    # trapa -- polje se trenutno nigde ne čita nazad pa je bezopasno, ali
    # kada budemo dodavali context switch između niti, ovo treba ispraviti
    # (npr. preko mscratch CSR-a) da polje zaista sadrži ispravnu vrednost.
    sd sp,  8(sp)
    sd gp,  16(sp)
    sd tp,  24(sp)
    sd t0,  32(sp)
    sd t1,  40(sp)
    sd t2,  48(sp)
    sd s0,  56(sp)
    sd s1,  64(sp)
    sd a0,  72(sp)
    sd a1,  80(sp)
    sd a2,  88(sp)
    sd a3,  96(sp)
    sd a4,  104(sp)
    sd a5,  112(sp)
    sd a6,  120(sp)
    sd a7,  128(sp)
    sd s2,  136(sp)
    sd s3,  144(sp)
    sd s4,  152(sp)
    sd s5,  160(sp)
    sd s6,  168(sp)
    sd s7,  176(sp)
    sd s8,  184(sp)
    sd s9,  192(sp)
    sd s10, 200(sp)
    sd s11, 208(sp)
    sd t3,  216(sp)
    sd t4,  224(sp)
    sd t5,  232(sp)
    sd t6,  240(sp)
.endm

.macro RESTORE_REGS
    ld ra,  0(sp)
    ld gp,  16(sp)
    ld tp,  24(sp)
    ld t0,  32(sp)
    ld t1,  40(sp)
    ld t2,  48(sp)
    ld s0,  56(sp)
    ld s1,  64(sp)
    ld a0,  72(sp)
    ld a1,  80(sp)
    ld a2,  88(sp)
    ld a3,  96(sp)
    ld a4,  104(sp)
    ld a5,  112(sp)
    ld a6,  120(sp)
    ld a7,  128(sp)
    ld s2,  136(sp)
    ld s3,  144(sp)
    ld s4,  152(sp)
    ld s5,  160(sp)
    ld s6,  168(sp)
    ld s7,  176(sp)
    ld s8,  184(sp)
    ld s9,  192(sp)
    ld s10, 200(sp)
    ld s11, 208(sp)
    ld t3,  216(sp)
    ld t4,  224(sp)
    ld t5,  232(sp)
    ld t6,  240(sp)
    addi sp, sp, 256
.endm

_start:
    # 1. Postavi pokazivač steka (sp)
    la sp, stack_top

    # 2. Postavi mtvec na našu trap rutinu (direktni režim -- donja 2 bita
    #    adrese moraju biti 0, što obezbeđuje .align 4 ispod)
    la t0, trap_vector
    csrw mtvec, t0

    # 3. Skoči u C++ main
    call main

1:  wfi
    j 1b

.global trap_vector
.align 4
trap_vector:
    SAVE_REGS

    mv a0, sp           # a0 = pokazivač na TrapFrame (vidi riscv.hpp)
    call handleTrap

    RESTORE_REGS
    mret

.section .bss
.align 16
stack_bottom:
    .skip 4096 * 4
stack_top:
