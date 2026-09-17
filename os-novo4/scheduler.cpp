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
    if (currentTcb == nullptr) {
        return; // Prekid je stigao dok scheduler petlja "izmedju niti"
    }

    if (ticksUntilStop > 0) {
        ticksUntilStop = ticksUntilStop - 1;
        if (ticksUntilStop == 0) {
            stopRequested = true;
        }
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
