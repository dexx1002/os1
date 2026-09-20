#include "trace.hpp"
#include "spinlock.hpp"

extern "C" void putc(char c);
extern "C" void printString(const char* str);

namespace {

TraceRecord buffer[TraceCapacity];
int         writeIndex = 0;
Spinlock    traceLock;

// Zaseban, mali decimalni ispis ovde umesto ponovne upotrebe main.cpp-ovog
// -- taj je lokalan (anonimni namespace) u drugoj prevodilackoj jedinici i
// nije vidljiv odavde. Radi sa punih 64 bita (mtime lako prevazidje 32-bitni
// opseg), za razliku od 'unsigned' verzije koju koristi ostatak projekta.
void printDecimal64(uint64_t value) {
    if (value == 0) {
        putc('0');
        return;
    }
    char digits[20];
    int  count = 0;
    while (value > 0 && count < 20) {
        digits[count++] = static_cast<char>('0' + (value % 10));
        value = value / 10;
    }
    while (count > 0) {
        putc(digits[--count]);
    }
}

const char* eventName(TraceEvent event) {
    switch (event) {
        case TraceEvent::Dispatch:  return "DISPATCH";
        case TraceEvent::Preempted: return "PREEMPTED";
        case TraceEvent::Blocked:   return "BLOCKED";
        case TraceEvent::Woken:     return "WOKEN";
        case TraceEvent::Finished:  return "FINISHED";
    }
    return "UNKNOWN";
}

} // namespace

void traceInit() {
    // Spinlock ima podrazumevanu vrednost (locked = 0) vec kroz svoju
    // definiciju -- nema posebne init funkcije za njega (vidi spinlock.hpp).
    writeIndex = 0;
}

void traceRecord(TraceEvent event, uint32_t threadId) {
    // KRITICNO: bez ovoga, ako tajmer prekid stigne TACNO dok ovaj hart vec
    // drzi traceLock (npr. tokom Dispatch zapisa u scheduler.cpp, gde su
    // prekidi upravo ponovo ukljuceni), schedulerTick() bi na ISTOM hartu
    // pokusao da ponovo udje u traceRecord (za Preempted zapis) i zauvek se
    // zaglavio cekajuci bravu koju sam vec drzi. disableInterrupts() ovde
    // sprecava tacno taj scenario -- isti razlog zasto ga readyQueueLock/
    // sem->lock uvek koriste ZAJEDNO sa spinlock-om, ne umesto njega.
    const bool wasEnabled = RISCV::disableInterrupts();
    spinLock(&traceLock);

    // Kad se bafer napuni, dalji dogadjaji se tiho odbacuju umesto da se
    // pise van granica ili uvodi slozenost kruznog bafera -- 512 zapisa je
    // dosta za sve dosadasnje testove zajedno, a ako jednog dana ne bude
    // dovoljno, ovo mesto je jasan signal (traceCount() == TraceCapacity)
    // da treba povecati TraceCapacity, ne skriveni bag.
    if (writeIndex < TraceCapacity) {
        TraceRecord& record = buffer[writeIndex];
        record.timestamp = RISCV::Clint::readMtime();
        record.threadId  = threadId;
        record.hartId    = static_cast<uint8_t>(RISCV::hartId());
        record.event     = event;
        writeIndex        = writeIndex + 1;
    }

    spinUnlock(&traceLock);
    RISCV::restoreInterrupts(wasEnabled);
}

int traceCount() {
    return writeIndex;
}

bool traceHasEventOnHart(uint8_t hartId) {
    for (int i = 0; i < writeIndex; ++i) {
        if (buffer[i].hartId == hartId) {
            return true;
        }
    }
    return false;
}

void traceDump() {
    printString("--- TRACE START (format: TRACE,timestamp,hart,threadId,event) ---\n");
    for (int i = 0; i < writeIndex; ++i) {
        const TraceRecord& record = buffer[i];
        printString("TRACE,");
        printDecimal64(record.timestamp);
        printString(",");
        printDecimal64(record.hartId);
        printString(",");
        printDecimal64(record.threadId);
        printString(",");
        printString(eventName(record.event));
        printString("\n");
    }
    printString("--- TRACE END ---\n");
}
