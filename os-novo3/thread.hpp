#ifndef THREAD_HPP
#define THREAD_HPP

#include "riscv.hpp"

// Callee-saved registri koje RISC-V ABI garantuje da ce biti ocuvani preko
// obicnog poziva funkcije. To je sve sto nam treba da "skocimo" sa jednog
// poziva-steka na drugi kroz normalan call/ret mehanizam -- bez ikakvog
// oslanjanja na trap/ecall infrastrukturu (ta ostaje rezervisana za
// asinhrone prekide i syscall-ove, ne za kooperativno smenjivanje niti).
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

enum class ThreadState { Ready, Running, Finished };

using ThreadBody = void (*)(void*);

// Namerno prost 'struct' bez konstruktora -- kalloc vraca sirovu memoriju,
// a bez <new> headera (ovaj toolchain ga nema) nemamo placement new na
// raspolaganju, pa polja popunjavamo rucno u createThread() umesto kroz
// konstruktor.
struct TCB {
    Context     context;
    ThreadBody  body;
    void*       arg;
    ThreadState state;
};

// Alocira stek i TCB, priprema pocetni kontekst (ra = threadTrampoline,
// sp = vrh steka) tako da prvi contextSwitch ka ovoj niti skoci pravo u
// telo niti. Nit se jos ne izvrsava -- to radi switchInto().
[[nodiscard]] TCB* createThread(ThreadBody body, void* arg = nullptr);

// Prebacuje izvrsavanje sa "glavnog" poziv-steka (onoga ko poziva ovu
// funkciju -- u ovoj fazi uvek main()) na datu nit. Vraca se kontrola
// pozivaocu tek kad nit sama pozove yieldToMain().
void switchInto(TCB* thread);

// Poziva se IZ niti da vrati kontrolu nazad onome ko ju je pokrenuo preko
// switchInto(). U ovoj (kooperativnoj) fazi nema pravog schedulera -- nit
// eksplicitno odlucuje kad ustupa procesor.
void yieldToMain();

#endif // THREAD_HPP
