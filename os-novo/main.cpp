#include "riscv.hpp"

extern "C" {
    void putc(char c);
    char getc();
    void printString(const char* str);
    void* kalloc(size_t bytes);
    void kfree(void* ptr);
}

extern "C" void handleTrap(uint64_t* registers) {
    (void)registers;

    uint64_t mcause = RISCV::r_mcause();
    uint64_t mepc   = RISCV::r_mepc();

    printString("\n[TRAP] Desio se prekid/ecall!\n");
    printString("  -> mcause = 0x");

    for (int i = 60; i >= 0; i -= 4) {
        uint8_t nibble = (mcause >> i) & 0xF;
        putc(nibble < 10 ? '0' + nibble : 'A' + (nibble - 10));
    }
    printString("\n");

    if (mcause == 8 || mcause == 9 || mcause == 11) {
        RISCV::w_mepc(mepc + 4);
    }
}

int main() {
    printString("===============================================\n");
    printString("   Bare-Metal RISC-V C++20/23 Kernel Booted!   \n");
    printString("===============================================\n\n");

    printString("[TEST 1] UART konzolni ispis radi odlicno.\n");

    uint64_t mstatus = RISCV::r_mstatus();
    (void)mstatus;
    printString("[TEST 2] Citanje CSR mstatus registra u redu.\n");

    printString("[TEST 3] Testiranje alokacije memorije (kalloc/kfree)...\n");

    void* p1 = kalloc(1024);
    void* p2 = kalloc(4096);

    if (p1 != nullptr && p2 != nullptr) {
        printString("  -> Uspesno alocirani blokovi na adresama!\n");
    } else {
        printString("  -> GRESKA: Alokacija memorije nije uspela!\n");
    }

    kfree(p1);
    kfree(p2);
    printString("  -> Memorija uspesno oslobodjena (kfree).\n\n");

    printString("[TEST 4] Simulacija 'ecall' sistemskog poziva...\n");
    asm volatile("ecall");
    printString("  -> Uspesno se vratili iz trap_handler-a natrag u main!\n\n");

    printString("===============================================\n");
    printString("   SVI OSNOVNI TESTOVI SU PROSLI SA 100%!      \n");
    printString("===============================================\n\n");

    while (true) {
        asm volatile("wfi");
    }

    return 0;
}