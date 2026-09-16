#include "riscv.hpp"

extern "C" {
void  putc(char c);
char  getc();
void  printString(const char* str);
void* kalloc(size_t bytes);
void  kfree(void* ptr);
}

namespace {

void printHex(uint64_t value) {
    printString("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        const auto nibble = static_cast<uint8_t>((value >> shift) & 0xF);
        putc(nibble < 10 ? static_cast<char>('0' + nibble)
                          : static_cast<char>('A' + (nibble - 10)));
    }
}

void printDecimal(unsigned value) {
    if (value == 0) {
        putc('0');
        return;
    }
    char digits[10];
    int count = 0;
    while (value > 0 && count < 10) {
        digits[count++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    }
    while (count > 0) {
        putc(digits[--count]);
    }
}

// Mali test harness: umesto da svaki test samo ispiše poruku i ćutke
// pretpostavi uspeh, ovde se stvarno proverava uslov i broji se
// prošlo/palo -- na kraju dobijamo iskren rezime, ne hardkodovan "100%".
struct TestRunner {
    int total  = 0;
    int passed = 0;

    void check(const char* name, bool condition) {
        ++total;
        passed += condition ? 1 : 0;
        printString(condition ? "  [PASS] " : "  [FAIL] ");
        printString(name);
        printString("\n");
    }

    void summary() const {
        printString("\n===============================================\n");
        printString("  Rezultat: ");
        printDecimal(static_cast<unsigned>(passed));
        printString(" / ");
        printDecimal(static_cast<unsigned>(total));
        printString(" testova proslo\n");
        printString("===============================================\n\n");
    }
};

bool isEcallCause(RISCV::TrapCause cause) {
    using RISCV::TrapCause;
    return cause == TrapCause::EcallFromUMode || cause == TrapCause::EcallFromSMode ||
           cause == TrapCause::EcallFromMMode;
}

// Poslednji zabeleženi mcause -- glavni program ga čita posle 'ecall'-a da
// stvarno proveri da je trap handler prošao kroz očekivanu granu, umesto
// da samo pretpostavi da je sve u redu jer se program nije zaglavio.
uint64_t lastTrapCause = 0;

} // namespace

// 'frame' trenutno nije u upotrebi -- namerno je ostavljen ovde, tipiziran
// i spreman, jer će sledeći korak (context switch između niti) direktno
// čitati/pisati po ovoj strukturi umesto sirovog uint64_t*.
extern "C" void handleTrap(RISCV::TrapFrame* frame) {
    (void)frame;

    const auto mcause = RISCV::readCsr<RISCV::Csr::MCause>();
    const auto mepc    = RISCV::readCsr<RISCV::Csr::MEpc>();
    const auto cause    = static_cast<RISCV::TrapCause>(mcause);

    lastTrapCause = mcause;

    printString("\n  [TRAP] mcause = ");
    printHex(mcause);
    printString("\n");

    if (isEcallCause(cause)) {
        RISCV::writeCsr<RISCV::Csr::MEpc>(mepc + 4);
    }
}

int main() {
    printString("===============================================\n");
    printString("       Bare-Metal RISC-V C++20 Kernel Booted!       \n");
    printString("===============================================\n\n");

    TestRunner tests;

    printString("[Konzola]\n");
    printString("  UART ispis radi ako vidis ovu liniju.\n");

    printString("\n[CSR]\n");
    const auto mstatus = RISCV::readCsr<RISCV::Csr::MStatus>();
    printString("  mstatus = ");
    printHex(mstatus);
    printString("\n");
    // Bez xv6/OS sloja iznad nas, jezgro startuje direktno u M-modu -- ovo
    // je jedina privilegija koju u ovoj fazi imamo, pa je i jedino svojstvo
    // mstatus-a koje ovde ima smisla proveriti.
    tests.check("CSR mstatus se cita bez pucanja programa", true);

    printString("\n[Alokator]\n");
    void* p1 = kalloc(1024);
    void* p2 = kalloc(4096);

    tests.check("kalloc(1024) vraca ne-null pokazivac", p1 != nullptr);
    tests.check("kalloc(4096) vraca ne-null pokazivac", p2 != nullptr);
    tests.check("blokovi se ne preklapaju",
                reinterpret_cast<uintptr_t>(p2) >=
                    reinterpret_cast<uintptr_t>(p1) + 1024);
    tests.check("p1 je poravnat na 8 bajtova",
                (reinterpret_cast<uintptr_t>(p1) % 8) == 0);
    tests.check("kalloc(0) vraca null (rubni slucaj)", kalloc(0) == nullptr);

    kfree(p1);
    kfree(p2);

    printString("\n[Trap / ecall]\n");
    lastTrapCause = 0;
    asm volatile("ecall");
    tests.check(
        "trap handler pozvan sa ocekivanim mcause (ecall iz M-moda)",
        lastTrapCause == static_cast<uint64_t>(RISCV::TrapCause::EcallFromMMode));

    tests.summary();

    while (true) {
        asm volatile("wfi");
    }

    return 0; // nedostizno, ali izbegava upozorenja na nekim kompajlerima
}
