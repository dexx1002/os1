#include "riscv.hpp"
#include "thread.hpp"

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

// Poslednji zabeleženi mcause za ecall test -- glavni program ga čita posle
// 'ecall'-a da stvarno proveri da je trap handler prošao kroz očekivanu
// granu, umesto da samo pretpostavi da je sve u redu.
uint64_t lastTrapCause = 0;

// Broj primljenih tajmer prekida. 'volatile' je ovde stvarno neophodan (ne
// kozmetika): menja ga trap handler asinhrono, a glavna petlja ga čita u
// wfi-petlji -- bez volatile-a bi kompajler mogao da pretpostavi da se
// vrednost nikad ne menja i da petlju optimizuje u beskonačnu.
volatile uint64_t timerTicks = 0;

// ~0.2s po tiku na QEMU 'virt' CLINT-u (koji radi na 10 MHz) -- proizvoljno
// biran interval, dovoljno čest da test ne čeka predugo, dovoljno redak da
// ne poplavi UART ispis kad kasnije budemo dodavali print po tiku.
constexpr uint64_t TimerIntervalTicks = 2'000'000;

// Deljeno stanje za test kooperativnih niti. Bez zastite (mutex/semafor) --
// namerno, jer je smenjivanje u ovoj fazi potpuno kooperativno i
// deterministicko (main eksplicitno bira ko je na redu), pa nema stvarne
// trke. Prava zastita dolazi tek sa preemptivnim schedulerom.
struct ThreadTestState {
    int  counter   = 0;
    char trace[16] = {};
    int  traceLen  = 0;
};
ThreadTestState threadTest;

void workerBody(void* arg) {
    const char id = *static_cast<char*>(arg);
    for (int round = 0; round < 3; ++round) {
        threadTest.counter = threadTest.counter + 1;
        if (threadTest.traceLen < 16) {
            threadTest.trace[threadTest.traceLen++] = id;
        }
        yieldToMain();
    }
}

} // namespace

// 'frame' trenutno nije u upotrebi -- namerno je ostavljen ovde, tipiziran
// i spreman, jer će sledeći korak (context switch između niti) direktno
// čitati/pisati po ovoj strukturi umesto sirovog uint64_t*.
extern "C" void handleTrap(RISCV::TrapFrame* frame) {
    (void)frame;

    const auto mcause = RISCV::readCsr<RISCV::Csr::MCause>();
    const auto cause   = static_cast<RISCV::TrapCause>(mcause);

    if (cause == RISCV::TrapCause::MachineTimerInterrupt) {
        // Tajmer prekid: samo prebroj i pomeri sledeći rok. Namerno bez
        // ispisa ovde -- kad budu postojale niti, baš ovo mesto postaje
        // scheduler "tick" (poziv dispatch-a), pa ne želimo UART pisanje
        // na kritičnoj putanji.
        timerTicks = timerTicks + 1; // ne ++ -- C++20 deprecira ++ na volatile
        RISCV::Clint::writeMtimecmp(RISCV::Clint::readMtime() + TimerIntervalTicks);
        return;
    }

    const auto mepc = RISCV::readCsr<RISCV::Csr::MEpc>();
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

    printString("\n[Tajmer]\n");
    {
        constexpr uint64_t ExpectedTicks = 5;

        timerTicks = 0;
        RISCV::Clint::writeMtimecmp(RISCV::Clint::readMtime() + TimerIntervalTicks);

        RISCV::writeCsr<RISCV::Csr::MIe>(RISCV::readCsr<RISCV::Csr::MIe>() |
                                          RISCV::InterruptEnableBits::MachineTimer);
        RISCV::writeCsr<RISCV::Csr::MStatus>(
            RISCV::readCsr<RISCV::Csr::MStatus>() |
            RISCV::StatusBits::MachineInterruptEnable);

        while (timerTicks < ExpectedTicks) {
            asm volatile("wfi");
        }

        // Gasimo tajmer prekid dok nemamo niti koje bi ga koristile --
        // sledeci korak (scheduler) ce ga ponovo upaliti trajno.
        RISCV::writeCsr<RISCV::Csr::MStatus>(
            RISCV::readCsr<RISCV::Csr::MStatus>() &
            ~RISCV::StatusBits::MachineInterruptEnable);

        printString("  Primljeno tik-ova: ");
        printDecimal(static_cast<unsigned>(timerTicks));
        printString("\n");
        tests.check("primljeno bar 5 tajmer prekida preko CLINT-a",
                    timerTicks >= ExpectedTicks);
    }

    printString("\n[Niti]\n");
    {
        char idA = 'A';
        char idB = 'B';
        TCB* threadA = createThread(workerBody, &idA);
        TCB* threadB = createThread(workerBody, &idB);

        // Glavni program je ovde "rucni scheduler": eksplicitno naizmenicno
        // bira A pa B, tri kruga. Pravi (automatski) scheduler dolazi u
        // sledecem koraku, pokretan tajmer prekidom koji smo upravo dodali.
        for (int round = 0; round < 3; ++round) {
            switchInto(threadA);
            switchInto(threadB);
        }

        printString("  Trag: ");
        for (int i = 0; i < threadTest.traceLen; ++i) {
            putc(threadTest.trace[i]);
        }
        printString("\n");

        tests.check("obe niti izvrsile tacno 3 kruga (brojac == 6)",
                    threadTest.counter == 6);

        bool alternates = threadTest.traceLen == 6;
        for (int i = 0; alternates && i < 6; i += 2) {
            alternates = threadTest.trace[i] == 'A' && threadTest.trace[i + 1] == 'B';
        }
        tests.check("A i B se smenjuju u ocekivanom redosledu (ABABAB)", alternates);
    }

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
