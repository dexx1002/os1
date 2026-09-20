#include "thread.hpp"
#include "scheduler.hpp"

extern "C" void* kalloc(size_t bytes);

namespace {

constexpr size_t StackSize = 4096;

uint64_t alignDown16(uint64_t value) {
    return value & ~static_cast<uint64_t>(0xF);
}

} // namespace

// Prva funkcija koja se izvrsi na steku nove niti (contextSwitch tamo
// skace preko sacuvanog 'ra'). 'self' saznajemo od schedulera, koji ga
// postavlja neposredno pre skoka -- ne kroz normalan argument poziva.
extern "C" void threadTrampoline() {
    TCB* self = schedulerCurrentThread();

    self->body(self->arg);
    self->state = ThreadState::Finished;

    // Zavrsena nit prepusta procesor schedulerU i nikad se vise ne bira.
    schedulerYieldFinished();

    // Nedostizno u normalnom toku -- cista bezbednosna mreza.
    while (true) {
        asm volatile("wfi");
    }
}

TCB* createThread(ThreadBody body, void* arg) {
    auto* self = static_cast<TCB*>(kalloc(sizeof(TCB)));
    auto* stackBase = static_cast<uint8_t*>(kalloc(StackSize));

    self->body     = body;
    self->arg      = arg;
    self->state    = ThreadState::Ready;
    self->next     = nullptr;
    self->homeHart = -1; // postavlja schedulerAddThread() pre prvog dispatch-a

    // RISC-V ABI zahteva 16-bajtno poravnanje steka; kalloc poravnava samo
    // na 8, pa vrh steka dodatno poravnavamo nadole.
    const auto stackTop =
        alignDown16(reinterpret_cast<uint64_t>(stackBase) + StackSize);

    self->context    = Context{};
    self->context.ra = reinterpret_cast<uint64_t>(&threadTrampoline);
    self->context.sp = stackTop;

    return self;
}
