#include "scheduler.hpp"

namespace {

// Ready red kao intruzivna lista preko TCB::next -- bez ikakve dodatne
// alokacije za cvorove.
TCB* readyHead = nullptr;
TCB* readyTail = nullptr;

// "Sidro" scheduler petlje -- analogno ranijem mainContext-u iz koraka 2,
// samo sto sada scheduler petlja (ne main() direktno) postaje stalna tacka
// na koju se niti vracaju kad budu preuzete ili kad zavrse.
Context schedulerContext{};

// Koja nit trenutno radi. nullptr znaci da je scheduler petlja "izmedju
// niti" (npr. ready red je prazan, ceka se na wfi).
TCB* currentTcb = nullptr;

uint64_t ticksUntilStop = 0; // 0 = nema automatskog zaustavljanja
bool     stopRequested   = false;

void enqueue(TCB* thread) {
    thread->next = nullptr;
    if (readyTail == nullptr) {
        readyHead = readyTail = thread;
    } else {
        readyTail->next = thread;
        readyTail = thread;
    }
}

TCB* dequeue() {
    if (readyHead == nullptr) {
        return nullptr;
    }
    TCB* thread = readyHead;
    readyHead = readyHead->next;
    if (readyHead == nullptr) {
        readyTail = nullptr;
    }
    thread->next = nullptr;
    return thread;
}

} // namespace

void schedulerAddThread(TCB* thread) {
    enqueue(thread);
}

TCB* schedulerCurrentThread() {
    return currentTcb;
}

void schedulerRequestStopAfter(uint64_t ticks) {
    ticksUntilStop = ticks;
    stopRequested  = false;
}

void schedulerRun() {
    while (!stopRequested) {
        TCB* next = dequeue();
        if (next == nullptr) {
            // Nema spremnih niti -- saceka se sledeci prekid umesto
            // busy-loop-a. Ako je to bas taj prekid koji postavi
            // stopRequested, spoljna while provera ce to uhvatiti.
            asm volatile("wfi");
            continue;
        }

        currentTcb        = next;
        currentTcb->state = ThreadState::Running;

        // KRITIČNO: contextSwitch NE prolazi kroz mret, pa mstatus.MIE ne
        // biva automatski restauriran. Kad se nit preuzme (preempted usred
        // trapa), mstatus.MIE ostaje "zaglavljen" na 0 (hardver ga je tako
        // postavio pri ulasku u trap) sve dok se TA KONKRETNA nit prirodno
        // ne nastavi kroz sopstveni mret. Bez ove linije, SVAKA novo-
        // pokrenuta nit (koja jos nikad nije bila prekinuta) bi krenula sa
        // iskljucenim prekidima -- i, buduci da se nikad sama ne ustupa
        // procesor (spinningWorker petlje su beskonacne), nikad ne bi bila
        // preuzeta -- livelock. Otkriveno pažljivom analizom pre testiranja:
        // druga nit u testu bi se zaglavila zauvek bez ovog fixa.
        RISCV::writeCsr<RISCV::Csr::MStatus>(
            RISCV::readCsr<RISCV::Csr::MStatus>() |
            RISCV::StatusBits::MachineInterruptEnable);

        contextSwitch(&schedulerContext, &currentTcb->context);
        // Vracamo se ovde tek kad schedulerTick() ili
        // schedulerYieldFinished() prebaci kontrolu nazad na scheduler.
    }
}

void schedulerTick() {
    // Odbrojavanje MORA da radi nezavisno od toga da li trenutno neka nit
    // radi -- ako sve niti završe (npr. producer/consumer u semafor testu)
    // PRE nego što isteknu traženi tik-ovi, a ovo bi ostalo unutar "if
    // (currentTcb != nullptr)" bloka, odbrojavanje bi se zauvek zaustavilo
    // (currentTcb je tada nullptr) i schedulerRun() bi visio u wfi-petlji
    // zauvek. Zato je odvojeno OD preuzimanja trenutne niti.
    if (ticksUntilStop > 0) {
        ticksUntilStop = ticksUntilStop - 1;
        if (ticksUntilStop == 0) {
            stopRequested = true;
        }
    }

    if (currentTcb == nullptr) {
        return; // Prekid je stigao dok scheduler petlja "izmedju niti".
    }

    TCB* preempted = currentTcb;
    currentTcb     = nullptr;

    if (preempted->state != ThreadState::Finished) {
        preempted->state = ThreadState::Ready;
        enqueue(preempted);
    }

    contextSwitch(&preempted->context, &schedulerContext);
    // Vracamo se ovde tek kad scheduler ponovo izabere BAS ovu nit.
}

void schedulerYieldFinished() {
    TCB* self  = currentTcb;
    currentTcb = nullptr;
    contextSwitch(&self->context, &schedulerContext);
    // U praksi se ne vracamo ovde -- self->state je Finished, scheduler ga
    // vise nikad ne stavlja u ready red.
}

void schedulerBlockCurrent() {
    TCB* self  = currentTcb;
    currentTcb = nullptr;
    // Namerno NE diramo mstatus.MIE ovde -- schedulerRun() ga eksplicitno
    // pali pre SVAKOG dispatch-a (vidi fix iz koraka 3), pa se stanje samo
    // ispravi kad scheduler sledeći put nekog pokrene, bez obzira da li je
    // to ova (kasnije probuđena) ili neka druga nit.
    contextSwitch(&self->context, &schedulerContext);
    // Vracamo se ovde tek kad nas neko probudi (schedulerWake) i scheduler
    // nas ponovo izabere.
}

void schedulerWake(TCB* thread) {
    thread->state = ThreadState::Ready;
    enqueue(thread);
}
