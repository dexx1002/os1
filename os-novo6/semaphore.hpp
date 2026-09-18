#ifndef SEMAPHORE_HPP
#define SEMAPHORE_HPP

#include "thread.hpp"

// Prost brojački semafor. 'struct' bez konstruktora -- isti razlog kao kod
// TCB-a (nema garantovanog <new>/placement new na ovom toolchainu); polja
// se postavljaju kroz semInit() posle obične deklaracije.
//
// 'waitHead'/'waitTail' su red blokiranih niti, strukturno identičan
// ready-redu iz scheduler.cpp, samo nad drugom listom. Nit u svakom
// trenutku može biti u NAJVIŠE JEDNOM od ova dva reda (ready ILI semaforov
// wait red) -- zato je bezbedno da oba reda dele isto TCB::next polje za
// povezivanje, nikad se ne koriste istovremeno za istu nit.
struct Semaphore {
    int  value;
    TCB* waitHead;
    TCB* waitTail;
};

void semInit(Semaphore* sem, int initialValue);

// Ako je vrednost > 0, dekrementuje i odmah nastavlja. Inače BLOKIRA
// pozivajuću nit (prepušta kontrolu scheduleru) dok je neki semSignal() ne
// probudi.
void semWait(Semaphore* sem);

// Ako neko čeka na semaforu, direktno ga budi (bez diranja 'value' --
// klasična optimizacija, ekvivalentna inkrement-pa-odmah-dekrement). Inače
// samo inkrementuje 'value'.
void semSignal(Semaphore* sem);

#endif // SEMAPHORE_HPP
