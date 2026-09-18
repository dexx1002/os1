#include "riscv.hpp"
#include "scheduler.hpp"
#include "semaphore.hpp"

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

// Test preemptivnog schedulera: obe niti beskonacno inkrementiraju
// SOPSTVENI brojac, bez ijednog eksplicitnog ustupanja procesora. Ako obe
// napreduju do kraja testa, to je dokaz da tajmer prekid stvarno,
// prinudno, preuzima kontrolu -- da nema preemcije, druga nit bi
// izgladnela (prva se nikad sama ne bi zaustavila).
struct PreemptCounters {
    volatile uint64_t a = 0;
    volatile uint64_t b = 0;
};
PreemptCounters preempt;

void spinningWorkerA(void*) {
    while (true) {
        preempt.a = preempt.a + 1;
    }
}

void spinningWorkerB(void*) {
    while (true) {
        preempt.b = preempt.b + 1;
    }
}

// Klasičan bounded-buffer producer-consumer test preko semafora:
// 'emptySlots' broji slobodna mesta, 'filledSlots' broji popunjena, 'mutex'
// (binarni semafor) štiti pristup deljenom kružnom baferu. Deterministički
// proveriva ispravnost: suma svih proizvedenih vrednosti (1+2+...+N) mora
// tačno odgovarati sumi koju je potrošač sabrao -- ako bi neka stavka bila
// izgubljena ili duplirana (race condition u pristupu baferu), suma bi
// odstupala.
constexpr int BufferSize     = 4;
constexpr int ItemsToProduce = 10;

int buffer[BufferSize];
int writeIndex = 0;
int readIndex  = 0;

Semaphore emptySlots;  // koliko je mesta trenutno slobodno u baferu
Semaphore filledSlots; // koliko je stavki trenutno spremno za citanje
Semaphore mutex;       // binarni semafor -- medjusobno iskljucenje pristupa baferu

int producedCount = 0;
int consumedCount = 0;
int consumedSum   = 0;

void producerBody(void*) {
    for (int i = 1; i <= ItemsToProduce; ++i) {
        semWait(&emptySlots);
        semWait(&mutex);
        buffer[writeIndex] = i;
        writeIndex = (writeIndex + 1) % BufferSize;
        producedCount = producedCount + 1;
        semSignal(&mutex);
        semSignal(&filledSlots);
    }
}

void consumerBody(void*) {
    for (int i = 0; i < ItemsToProduce; ++i) {
        semWait(&filledSlots);
        semWait(&mutex);
        const int item = buffer[readIndex];
        readIndex = (readIndex + 1) % BufferSize;
        consumedSum   = consumedSum + item;
        consumedCount = consumedCount + 1;
        semSignal(&mutex);
        semSignal(&emptySlots);
    }
}

} // namespace

// 'frame' trenutno nije u upotrebi -- ostaje ovde tipiziran i spreman za
// eventualnu buduću upotrebu (npr. inspekcija registara pri debug-u).
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
        schedulerTick();
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

    printString("\n[Preemptivni scheduler]\n");
    {
        preempt.a = 0;
        preempt.b = 0;

        TCB* threadA = createThread(spinningWorkerA);
        TCB* threadB = createThread(spinningWorkerB);
        schedulerAddThread(threadA);
        schedulerAddThread(threadB);

        // Ponovo palimo tajmer (ugašen posle prošlog testa) -- ovog puta
        // trajno, jer scheduler njime upravlja preotimanjem.
        RISCV::Clint::writeMtimecmp(RISCV::Clint::readMtime() + TimerIntervalTicks);
        RISCV::writeCsr<RISCV::Csr::MIe>(RISCV::readCsr<RISCV::Csr::MIe>() |
                                          RISCV::InterruptEnableBits::MachineTimer);
        RISCV::writeCsr<RISCV::Csr::MStatus>(
            RISCV::readCsr<RISCV::Csr::MStatus>() |
            RISCV::StatusBits::MachineInterruptEnable);

        // Nijedna od niti sama ne ustupa procesor (beskonačne petlje) --
        // jedini način da obe napreduju je da ih tajmer prekid PRINUDNO
        // smenjuje. 20 tik-ova je dovoljno da obe sigurno dobiju bar
        // nekoliko krugova, a test ne traje predugo.
        schedulerRequestStopAfter(20);
        schedulerRun();

        RISCV::writeCsr<RISCV::Csr::MStatus>(
            RISCV::readCsr<RISCV::Csr::MStatus>() &
            ~RISCV::StatusBits::MachineInterruptEnable);

        printString("  A brojac: ");
        printDecimal(static_cast<unsigned>(preempt.a));
        printString("\n  B brojac: ");
        printDecimal(static_cast<unsigned>(preempt.b));
        printString("\n");

        tests.check(
            "obe niti su dobile CPU vreme bez ijednog dobrovoljnog ustupanja "
            "(prinudno preotimanje radi)",
            preempt.a > 0 && preempt.b > 0);
    }

    printString("\n[Semafori]\n");
    {
        semInit(&emptySlots, BufferSize);
        semInit(&filledSlots, 0);
        semInit(&mutex, 1);
        producedCount = 0;
        consumedCount = 0;
        consumedSum   = 0;
        writeIndex    = 0;
        readIndex     = 0;

        TCB* producer = createThread(producerBody);
        TCB* consumer = createThread(consumerBody);
        schedulerAddThread(producer);
        schedulerAddThread(consumer);

        RISCV::Clint::writeMtimecmp(RISCV::Clint::readMtime() + TimerIntervalTicks);
        RISCV::writeCsr<RISCV::Csr::MIe>(RISCV::readCsr<RISCV::Csr::MIe>() |
                                          RISCV::InterruptEnableBits::MachineTimer);
        RISCV::writeCsr<RISCV::Csr::MStatus>(
            RISCV::readCsr<RISCV::Csr::MStatus>() |
            RISCV::StatusBits::MachineInterruptEnable);

        // Obe niti se same zaustave posle tacno ItemsToProduce stavki (za
        // razliku od prethodnog testa) -- 20 tik-ova je gornja granica za
        // slucaj da nesto zapne, ne ocekivano trajanje.
        schedulerRequestStopAfter(20);
        schedulerRun();

        RISCV::writeCsr<RISCV::Csr::MStatus>(
            RISCV::readCsr<RISCV::Csr::MStatus>() &
            ~RISCV::StatusBits::MachineInterruptEnable);

        constexpr int ExpectedSum = ItemsToProduce * (ItemsToProduce + 1) / 2;

        printString("  Proizvedeno: ");
        printDecimal(static_cast<unsigned>(producedCount));
        printString(" / Potroseno: ");
        printDecimal(static_cast<unsigned>(consumedCount));
        printString(" / Suma: ");
        printDecimal(static_cast<unsigned>(consumedSum));
        printString("\n");

        tests.check("obe niti su odradile svih 10 stavki",
                    producedCount == ItemsToProduce && consumedCount == ItemsToProduce);
        tests.check("suma potrosenih vrednosti je tacno 1+2+...+10 = 55 "
                    "(dokaz da nema izgubljenih/dupliranih stavki)",
                    consumedSum == ExpectedSum);
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
