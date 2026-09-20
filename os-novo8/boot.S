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

# Koliko hartova ocekujemo (mora odgovarati '-smp N' u Makefile-u i broju
# skip-ovanih blokova za stek ispod). MAX_HARTS=2 za sada -- Faza 1 SMP-a.
.equ MAX_HARTS, 2
.equ PER_HART_STACK_SIZE, 4096 * 4

_start:
    # Svi hartovi kreću OVDE ISTOVREMENO (bez OpenSBI-ja, -bios none).
    # mhartid razlikuje ko je ko -- ne oslanjamo se na to da a0 već sadrži
    # hartid pri ulasku (iako po konvenciji obično i sadrži), nego ga sami
    # pročitamo, da kod ne zavisi od te pretpostavke.
    csrr t0, mhartid

    # Svaki hart dobija svoju "krišku" od deljenog stek regiona:
    # sp(hart N) = stack_top - N * PER_HART_STACK_SIZE. Za hart 0 ovo se
    # svodi tačno na staro ponašanje (sp = stack_top), pa hart 0 ne menja
    # ponašanje iz prethodnih koraka.
    la sp, stack_top
    li t1, PER_HART_STACK_SIZE
    mul t1, t1, t0
    sub sp, sp, t1

    # mtvec postavljamo za SVAKI hart (svaki hart ima sopstvene CSR-ove --
    # ovo NIJE deljeno stanje).
    la t2, trap_vector
    csrw mtvec, t2

    bnez t0, secondary_entry

    # Samo hart 0 nastavlja normalan boot.
    call main
1:  wfi
    j 1b

secondary_entry:
    # Nema IPI/mip čekanja ovde -- secondaryHartMain() (C++) sam radi
    # busy-spin na običnoj memorijskoj zastavici dok ga hart 0 ne pusti
    # (vidi smp.cpp). Jednostavnije i manje rizično od CLINT/mip pristupa:
    # nema šanse da "propustimo" IPI zbog pogrešnog tajminga.
    call secondaryHartMain
2:  wfi
    j 2b

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
    # MAX_HARTS * PER_HART_STACK_SIZE ukupno -- svaki hart dobija svoju
    # 16KB krišku (vidi računicu u _start). Ako se MAX_HARTS ovde promeni,
    # .equ MAX_HARTS na vrhu fajla mora da prati.
    .skip (4096 * 4) * 2
stack_top:
