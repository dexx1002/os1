#include "semaphore.hpp"
#include "scheduler.hpp"
#include "riscv.hpp"

namespace {

void enqueueWaiter(Semaphore* sem, TCB* thread) {
    thread->next = nullptr;
    if (sem->waitTail == nullptr) {
        sem->waitHead = sem->waitTail = thread;
    } else {
        sem->waitTail->next = thread;
        sem->waitTail = thread;
    }
}

TCB* dequeueWaiter(Semaphore* sem) {
    if (sem->waitHead == nullptr) {
        return nullptr;
    }
    TCB* thread = sem->waitHead;
    sem->waitHead = sem->waitHead->next;
    if (sem->waitHead == nullptr) {
        sem->waitTail = nullptr;
    }
    thread->next = nullptr;
    return thread;
}

} // namespace

void semInit(Semaphore* sem, int initialValue) {
    sem->value    = initialValue;
    sem->waitHead = nullptr;
    sem->waitTail = nullptr;
}

void semWait(Semaphore* sem) {
    // Provera-i-izmena 'value' mora biti atomska u odnosu na tajmer prekid
    // -- bez ovoga, prekid bi mogao da upadne TAČNO između provere i
    // dekrementa i pusti drugu nit da vidi istu (zastarelu) vrednost, što
    // bi dozvolilo da dve niti "prođu" kroz semafor koji je trebalo da
    // propusti samo jednu.
    const bool wasEnabled = RISCV::disableInterrupts();

    if (sem->value > 0) {
        sem->value = sem->value - 1;
        RISCV::restoreInterrupts(wasEnabled);
        return;
    }

    TCB* self  = schedulerCurrentThread();
    self->state = ThreadState::Blocked;
    enqueueWaiter(sem, self);

    // NAMERNO ne zovemo restoreInterrupts() pre bloka: schedulerRun() sam
    // eksplicitno pali prekide pre svakog dispatch-a (isti fix kao za
    // preemciju u koraku 3), pa se stanje samo ispravi bez obzira šta se
    // dešava između ovog poziva i trenutka kad nas neko probudi.
    schedulerBlockCurrent();

    // Vraćamo se ovde tek kad nas signal() probudi (schedulerWake) i
    // scheduler nas ponovo izabere -- semafor je već "predat" nama u tom
    // trenutku (vidi semSignal), nema potrebe za ponovnom mstatus popravkom.
}

void semSignal(Semaphore* sem) {
    const bool wasEnabled = RISCV::disableInterrupts();

    TCB* waiter = dequeueWaiter(sem);
    if (waiter != nullptr) {
        // Direktna "predaja" probuđenoj niti -- ne diramo 'value' jer je
        // efekat isti kao inkrement-pa-odmah-dekrement, samo bez nepotrebnog
        // koraka.
        schedulerWake(waiter);
    } else {
        sem->value = sem->value + 1;
    }

    RISCV::restoreInterrupts(wasEnabled);
}
