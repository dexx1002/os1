#ifndef THREAD_HPP
#define THREAD_HPP

#include "riscv.hpp"

// Callee-saved registri koje RISC-V ABI garantuje da ce biti ocuvani preko
// obicnog poziva funkcije. To je sve sto nam treba da "skocimo" sa jednog
// poziva-steka na drugi kroz normalan call/ret mehanizam.
struct Context {
    uint64_t ra, sp;
    uint64_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
};

static_assert(sizeof(Context) == 112, "Context mora imati 14 registara x 8B");
static_assert(__builtin_offsetof(Context, sp) == 8,
              "sp mora biti odmah posle ra (vidi contextSwitch.S)");

// Implementirano u contextSwitch.S. Sacuva trenutni ra/sp/s0-s11 u '*from',
// ucita iste iz '*to', pa 'ret' -- efektivno skok na drugi poziv-stek.
extern "C" void contextSwitch(Context* from, Context* to);

enum class ThreadState { Ready, Running, Blocked, Finished };

using ThreadBody = void (*)(void*);

// 'next' je deo TCB-a, ne posebne strukture cvora -- scheduler koristi ovo
// polje da napravi intruzivnu kruznu listu (ready red) bez ijedne dodatne
// alokacije. 'homeHart' je hart kome je ova nit "prikovana" (pinned) --
// vidi opsirno objasnjenje u scheduler.cpp zasto nit NIKAD ne sme da
// migrira na drugi hart posle preotimanja. 'id' je redni broj dodeljen pri
// kreiranju -- koristi ga profiling/tracing alat da razlikuje niti u tragu.
struct TCB {
    Context     context;
    ThreadBody  body;
    void*       arg;
    ThreadState state;
    TCB*        next;
    int         homeHart;
    uint32_t    id;
};

// Alocira stek i TCB, priprema pocetni kontekst (ra = threadTrampoline,
// sp = vrh steka) tako da prvi contextSwitch ka ovoj niti skoci pravo u
// telo niti. Nit se jos ne izvrsava -- to je posao schedulera
// (schedulerAddThread + schedulerRun, vidi scheduler.hpp).
[[nodiscard]] TCB* createThread(ThreadBody body, void* arg = nullptr);

#endif // THREAD_HPP
