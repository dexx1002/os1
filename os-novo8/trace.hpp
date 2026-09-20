#ifndef TRACE_HPP
#define TRACE_HPP

#include "riscv.hpp"

// Mali, fiksni skup dogadjaja koje pratimo -- dovoljno da se vidi KO
// (koja nit, koji hart) i KADA (mtime) je nesto radio. Namerno usko
// skrojeno za scheduler/semafor dogadjaje, ne pokusaj univerzalnog
// profilera.
enum class TraceEvent : uint8_t {
    Dispatch  = 0, // scheduler je dispatch-ovao nit na hart
    Preempted = 1, // nit preuzeta tajmer prekidom
    Blocked   = 2, // nit blokirana na semaforu
    Woken     = 3, // nit probudjena (spremna, ceka dispatch)
    Finished  = 4, // nit zavrsila
};

// Jedan zapis u tragu. Namerno mali (16B) da trag stane u razuman broj
// zapisa u fiksnom baferu bez dinamicke alokacije tokom rada.
struct TraceRecord {
    uint64_t   timestamp; // mtime u trenutku dogadjaja
    uint32_t   threadId;  // TCB::id niti na koju se dogadjaj odnosi
    uint8_t    hartId;    // koji hart je zabelezio dogadjaj
    TraceEvent event;
};

constexpr int TraceCapacity = 512;

void traceInit();

// Zove se iz scheduler.cpp/semaphore.cpp na svaku bitnu promenu stanja
// niti. Zasticeno sopstvenim spinlock-om -- vise hartova moze da zapisuje
// istovremeno.
void traceRecord(TraceEvent event, uint32_t threadId);

[[nodiscard]] int  traceCount();
[[nodiscard]] bool traceHasEventOnHart(uint8_t hartId);

// Ispisuje ceo trag preko UART-a u prostom, parsable CSV formatu
// ("TRACE,timestamp,hart,threadId,event") -- namenjeno da se izlaz
// kopira i ucita u prateci HTML vizualizator (vidi tools/trace-viewer.html).
void traceDump();

#endif // TRACE_HPP
