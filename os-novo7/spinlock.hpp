#ifndef SPINLOCK_HPP
#define SPINLOCK_HPP

#include "riscv.hpp"

// Najprostiji mogući spinlock preko test-and-set atomske RISC-V "A"
// ekstenzije instrukcije (amoswap.w). Neophodan tek sa SMP-om -- dok je sve
// radilo na jednom hartu, disableInterrupts()/restoreInterrupts() je bio
// dovoljan (sprečava da BAŠ TAJ hart bude prekinut usred kritične sekcije).
// Sa dva fizička harta, to VIŠE NIJE dovoljno -- isključivanje prekida na
// jednom hartu ne sprečava DRUGI hart da istovremeno uđe u istu kritičnu
// sekciju (to je potpuno nezavisan fizički procesor). Zato koristimo pravi
// atomski spinlock svuda gde je deljeno stanje dostižno sa više hartova
// (ready red u scheduler-u, semafori).
struct Spinlock {
    volatile uint32_t locked = 0;
};

inline void spinLock(Spinlock* lock) noexcept {
    uint32_t previous;
    do {
        // amoswap.w.aq: atomski upiši 1 na adresu, vrati STARU vrednost.
        // '.aq' (acquire) sprečava da se instrukcije POSLE ove "pomere"
        // ranije u izvršavanju (memory reordering) -- garantuje da sve što
        // radimo unutar kritične sekcije stvarno vidi efekat brave.
        asm volatile("amoswap.w.aq %0, %1, (%2)"
                     : "=r"(previous)
                     : "r"(1u), "r"(&lock->locked)
                     : "memory");
    } while (previous != 0); // neko drugi je već držao bravu -- probaj opet
}

inline void spinUnlock(Spinlock* lock) noexcept {
    // '.rl' (release) sprečava da se instrukcije PRE ove "pomere" kasnije --
    // sve izmene iz kritične sekcije moraju biti vidljive PRE otključavanja.
    asm volatile("amoswap.w.rl zero, zero, (%0)"
                 :
                 : "r"(&lock->locked)
                 : "memory");
}

#endif // SPINLOCK_HPP
