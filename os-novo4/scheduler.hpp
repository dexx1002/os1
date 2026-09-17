#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include "thread.hpp"

// Dodaje nit u ready red. Pozvati pre schedulerRun().
void schedulerAddThread(TCB* thread);

// Pokrece scheduler petlju: bira spremne niti round-robin redosledom i
// prebacuje im kontrolu preko contextSwitch-a (vidi thread.hpp). Vraca se
// main()-u tek kada schedulerTick() odbroji do nule posle poziva
// schedulerRequestStopAfter() -- korisno za testiranje. Bez toga bi
// petlja radila zauvek (sto ce biti normalan slucaj kasnije, van testova).
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

#endif // SCHEDULER_HPP
