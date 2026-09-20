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
    sem->value        = initialValue;
    sem->waitHead     = nullptr;
    sem->waitTail     = nullptr;
    sem->lock.locked  = 0;
}

void semWait(Semaphore* sem) {
    // Provera-i-izmena 'value' mora biti zaštićena i od tajmer prekida (na
    // OVOM hartu) i od DRUGOG harta koji istovremeno radi isto (SMP) --
    // otud i disableInterrupts() i spinlock zajedno, ne jedno ili drugo.
    const bool wasEnabled = RISCV::disableInterrupts();
    spinLock(&sem->lock);

    if (sem->value > 0) {
        sem->value = sem->value - 1;
        spinUnlock(&sem->lock);
        RISCV::restoreInterrupts(wasEnabled);
        return;
    }

    TCB* self   = schedulerCurrentThread();
    self->state = ThreadState::Blocked;
    enqueueWaiter(sem, self);

    // KRITIČNO: brava se MORA otključati PRE bloka. schedulerBlockCurrent()
    // možda ne vrati kontrolu jako dugo (dok nas neko ne probudi) -- da smo
    // otišli u blok DRŽEĆI ovu bravu, blokirali bismo SVAKI drugi hart koji
    // pokuša da uđe u isti semafor u međuvremenu (klasična "spinlock held
    // across a blocking call" zamka -- gora čak i od spinlock+preemcije
    // problema, jer traje neodređeno dugo, ne samo jedan tajmer tik).
    spinUnlock(&sem->lock);

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
    spinLock(&sem->lock);

    TCB* waiter = dequeueWaiter(sem);
    if (waiter != nullptr) {
        // Otključaj PRE schedulerWake() -- isti razlog kao gore: ne želimo
        // da držimo semaforovu bravu dok diramo scheduler-ov (poseban)
        // ready-red spinlock. schedulerWake() je bezbedan da se pozove
        // samostalno, sam štiti svoj red.
        spinUnlock(&sem->lock);
        // Direktna "predaja" probuđenoj niti -- ne diramo 'value' jer je
        // efekat isti kao inkrement-pa-odmah-dekrement, samo bez nepotrebnog
        // koraka.
        schedulerWake(waiter);
    } else {
        sem->value = sem->value + 1;
        spinUnlock(&sem->lock);
    }

    RISCV::restoreInterrupts(wasEnabled);
}
