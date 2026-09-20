#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include "thread.hpp"

// Dodaje nit u ready red. Pozvati pre schedulerRun().
void schedulerAddThread(TCB* thread);

// Pokrece scheduler petlju: bira spremne niti round-robin redosledom i
// prebacuje im kontrolu preko contextSwitch-a (vidi thread.hpp). Vraca se
// pozivaocu tek kada schedulerTick() odbroji do nule posle poziva
// schedulerRequestStopAfter() -- korisno za testiranje. Bez toga bi petlja
// radila zauvek (sto ce biti normalan slucaj kasnije, van testova).
// SMP napomena: SVAKI hart poziva ovu funkciju NEZAVISNO, za sebe -- svi
// hart-ovi dele ISTI ready red (zasticen spinlock-om), ali svaki ima
// SOPSTVENU "trenutnu nit" i scheduler kontekst (indeksirano po hartId()).
void schedulerRun();

// Zaustavi schedulerRun() posle tacno 'ticks' tajmer prekida. Pozvati pre
// schedulerRun().
void schedulerRequestStopAfter(uint64_t ticks);

// Poziva se IZ timer interrupt handlera (main.cpp/handleTrap). No-op ako
// trenutno nijedna nit ne radi (scheduler petlja je "izmedju niti").
void schedulerTick();

// Trenutno aktivna nit (ili nullptr). threadTrampoline ovim saznaje 'self'
// kad prvi put pocne da se izvrsava.
[[nodiscard]] TCB* schedulerCurrentThread();

// Nit poziva kad zavrsi (state je vec postavljen na Finished od strane
// pozivaoca) -- prepusta kontrolu schedulerU. Scheduler tu nit vise nikad
// ne bira, pa se ovaj poziv u praksi nikad ne "vraca" u tu nit.
void schedulerYieldFinished();

// Nit poziva kad se blokira (npr. semWait na praznom semaforu) -- pozivalac
// MORA vec da je postavio state na Blocked i, ako je relevantno, stavio nit
// u neki DRUGI red (npr. semaforov red cekanja) pre ovog poziva. Scheduler
// je vise ne bira dok je neko ne probudi preko schedulerWake().
void schedulerBlockCurrent();

// Vraca ranije blokiranu nit nazad u ready red (state -> Ready). Poziva se
// iz semSignal() kad neko čeka na semafor koji se upravo oslobađa.
void schedulerWake(TCB* thread);

#endif // SCHEDULER_HPP
