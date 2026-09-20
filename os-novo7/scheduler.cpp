#include "scheduler.hpp"
#include "spinlock.hpp"

namespace {

// SVAKI hart ima SOPSTVENI ready red -- NE jedan deljeni. Razlog (vidi i
// homeHart komentar u thread.hpp): nit preuzeta USRED trapa ima "u letu"
// mepc/mstatus CSR stanje HARTA koji ju je preuzeo -- te vrednosti NISU
// eksplicitno sačuvane u softveru, oslanjamo se na to da će se prekinuti
// trap PRIRODNO odmotati kroz mret kad se nit nastavi. mepc/mstatus su
// PO-HARTU CSR-ovi -- ako bi DRUGI hart pokušao da nastavi tu nit, izvršio
// bi mret koristeći SVOJE (pogrešne) vrednosti -> katastrofalna greška.
// Zato je svaka nit "prikovana" (homeHart) za hart na kom je prvi put
// pokrenuta, i tu ostaje ceo život (nema migracije niti u ovoj verziji --
// poznato, namerno pojednostavljenje; pravo rešenje bi eksplicitno čuvalo
// mepc/mstatus u samom TCB-u, ali to je veća izmena trap mehanizma).
TCB*     readyHead[RISCV::MaxHarts] = {};
TCB*     readyTail[RISCV::MaxHarts] = {};
Spinlock readyQueueLock; // jedna brava za sve redove -- prosto, dovoljno za MaxHarts=2

// Koji hart dobija SLEDEĆU tek kreiranu nit (round-robin raspodela) -- ovo
// jednom postavljeno postaje ta nit-ina 'homeHart' zauvek.
uint64_t nextHartForNewThread = 0;

// SVAKI hart ima SOPSTVENI scheduler kontekst i "trenutnu nit" -- ovo NIJE
// deljeno stanje. Da su ovo obične skalarne globalne promenljive (kao u
// single-hart verziji iz koraka 3), dva harta bi se međusobno gazila preko
// istog konteksta.
Context schedulerContext[RISCV::MaxHarts]{};
TCB*    currentTcb[RISCV::MaxHarts] = {};

// Stop uslov je deljen, ali SAMO hart 0 dekrementuje 'ticksUntilStop' --
// ostali hartovi ga samo indirektno vide preko 'stopRequested' (plain
// volatile bool -- bezopasna trka, najgore što se desi je da neki hart
// primeti signal jedan tik kasnije).
volatile uint64_t ticksUntilStop = 0;
volatile bool     stopRequested  = false;

void enqueueTo(uint64_t hart, TCB* thread) {
    const bool wasEnabled = RISCV::disableInterrupts();
    spinLock(&readyQueueLock);

    thread->next = nullptr;
    if (readyTail[hart] == nullptr) {
        readyHead[hart] = readyTail[hart] = thread;
    } else {
        readyTail[hart]->next = thread;
        readyTail[hart] = thread;
    }

    spinUnlock(&readyQueueLock);
    RISCV::restoreInterrupts(wasEnabled);
}

TCB* dequeueFrom(uint64_t hart) {
    const bool wasEnabled = RISCV::disableInterrupts();
    spinLock(&readyQueueLock);

    TCB* thread = readyHead[hart];
    if (thread != nullptr) {
        readyHead[hart] = readyHead[hart]->next;
        if (readyHead[hart] == nullptr) {
            readyTail[hart] = nullptr;
        }
        thread->next = nullptr;
    }

    spinUnlock(&readyQueueLock);
    RISCV::restoreInterrupts(wasEnabled);
    return thread;
}

} // namespace

void schedulerAddThread(TCB* thread) {
    // Round-robin raspodela NOVIH niti po hartovima -- ovaj izbor postaje
    // trajan ('homeHart') za ostatak života niti.
    const bool wasEnabled = RISCV::disableInterrupts();
    spinLock(&readyQueueLock);
    thread->homeHart     = static_cast<int>(nextHartForNewThread);
    nextHartForNewThread = (nextHartForNewThread + 1) % RISCV::MaxHarts;
    spinUnlock(&readyQueueLock);
    RISCV::restoreInterrupts(wasEnabled);

    enqueueTo(static_cast<uint64_t>(thread->homeHart), thread);
}

TCB* schedulerCurrentThread() {
    return currentTcb[RISCV::hartId()];
}

void schedulerRequestStopAfter(uint64_t ticks) {
    // Poziva se SAMO sa harta 0, pre nego što ijedan drugi hart krene --
    // bezbedno bez zaštite u tom trenutku.
    ticksUntilStop = ticks;
    stopRequested  = false;
}

void schedulerRun() {
    const auto hart = RISCV::hartId();

    while (!stopRequested) {
        TCB* next = dequeueFrom(hart);
        if (next == nullptr) {
            // Nema spremnih niti ZA OVAJ hart -- saceka se sledeci prekid
            // umesto busy-loop-a.
            asm volatile("wfi");
            continue;
        }

        currentTcb[hart]        = next;
        currentTcb[hart]->state = ThreadState::Running;

        // KRITIČNO (iz koraka 3, ovde jednako bitno po hartu): contextSwitch
        // NE prolazi kroz mret, pa mstatus.MIE ne biva automatski
        // restauriran. Ovo je CSR operacija -- deluje samo na TRENUTNI
        // hart, pa svaki hart mora sam da izvrši ovu liniju za sebe.
        RISCV::writeCsr<RISCV::Csr::MStatus>(
            RISCV::readCsr<RISCV::Csr::MStatus>() |
            RISCV::StatusBits::MachineInterruptEnable);

        contextSwitch(&schedulerContext[hart], &currentTcb[hart]->context);
        // Vracamo se ovde tek kad schedulerTick() ili
        // schedulerYieldFinished() (POZVAN NA OVOM ISTOM HARTU) prebaci
        // kontrolu nazad na scheduler.
    }
}

void schedulerTick() {
    const auto hart = RISCV::hartId();

    // Odbrojavanje MORA da radi nezavisno od toga da li trenutno neka nit
    // radi NA OVOM hartu. SAMO hart 0 dekrementuje (vidi napomenu uz
    // deklaraciju), da izbegnemo trku na deljenom brojaču.
    if (hart == 0 && ticksUntilStop > 0) {
        ticksUntilStop = ticksUntilStop - 1;
        if (ticksUntilStop == 0) {
            stopRequested = true;
        }
    }

    if (currentTcb[hart] == nullptr) {
        return; // Prekid je stigao dok scheduler petlja na OVOM hartu "izmedju niti".
    }

    TCB* preempted   = currentTcb[hart];
    currentTcb[hart] = nullptr;

    if (preempted->state != ThreadState::Finished) {
        preempted->state = ThreadState::Ready;
        // KLJUČNO: ide nazad u red OVOG (home) harta, ne bilo kog -- vidi
        // objašnjenje uz deklaraciju redova na vrhu fajla.
        enqueueTo(static_cast<uint64_t>(preempted->homeHart), preempted);
    }

    contextSwitch(&preempted->context, &schedulerContext[hart]);
    // Vracamo se ovde tek kad scheduler NA OVOM ISTOM HARTU ponovo izabere
    // BAS ovu nit -- homeHart pinning gore garantuje da se to i desi.
}

void schedulerYieldFinished() {
    const auto hart  = RISCV::hartId();
    TCB* self        = currentTcb[hart];
    currentTcb[hart] = nullptr;
    contextSwitch(&self->context, &schedulerContext[hart]);
    // U praksi se ne vracamo ovde -- self->state je Finished, scheduler ga
    // vise nikad ne stavlja u ready red.
}

void schedulerBlockCurrent() {
    const auto hart  = RISCV::hartId();
    TCB* self        = currentTcb[hart];
    currentTcb[hart] = nullptr;
    // Namerno NE diramo mstatus.MIE ovde -- schedulerRun() ga eksplicitno
    // pali pre SVAKOG dispatch-a (na SVAKOM hartu posebno).
    contextSwitch(&self->context, &schedulerContext[hart]);
    // Vracamo se ovde tek kad nas neko probudi (schedulerWake) i scheduler
    // NA NASEM homeHart-u nas ponovo izabere.
}

void schedulerWake(TCB* thread) {
    thread->state = ThreadState::Ready;
    // KLJUČNO: budimo je u redu NJENOG home harta -- signal može stići sa
    // BILO KOG harta (npr. semSignal pozvan na hartu 0 za nit prikovanu za
    // hart 1), ali nit i dalje sme da nastavi SAMO na svom home hartu.
    enqueueTo(static_cast<uint64_t>(thread->homeHart), thread);
}
