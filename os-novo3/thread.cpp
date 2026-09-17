#include "thread.hpp"

extern "C" void* kalloc(size_t bytes);

namespace {

constexpr size_t StackSize = 4096;

// "Poziv-stek" od main()-a, kao sidro za povratak kad nit ustupi procesor.
// Namerno je globalna promenljiva, ne lokalna u main()-u -- yieldToMain()
// mora da joj pristupi iz koda koji se izvrsava na SASVIM DRUGOM steku
// (steku niti), gde main()-ove lokalne promenljive nisu vidljive/dostizne.
Context mainContext{};

// Koja je nit trenutno "unutra". nullptr znaci da je na redu main().
TCB* currentTcb = nullptr;

uint64_t alignDown16(uint64_t value) {
    return value & ~static_cast<uint64_t>(0xF);
}

} // namespace

// Prva funkcija koja se izvrsi na steku nove niti (contextSwitch tamo
// skace preko sacuvanog 'ra'). Ne prima argumente kroz normalan poziv --
// 'self' citamo iz currentTcb, koji switchInto() postavlja neposredno pre
// samog skoka.
extern "C" void threadTrampoline() {
    TCB* self = currentTcb;

    self->body(self->arg);
    self->state = ThreadState::Finished;

    // Zavrsena nit se vise nikad ne bira za izvrsavanje (to postaje pravilo
    // schedulera u sledecem koraku) -- ovaj yieldToMain je "poslednje zbogom".
    yieldToMain();

    // Nedostizno u normalnom toku -- cista bezbednosna mreza ako bi neko
    // greskom ponovo skociomo ovamo.
    while (true) {
        asm volatile("wfi");
    }
}

TCB* createThread(ThreadBody body, void* arg) {
    auto* self = static_cast<TCB*>(kalloc(sizeof(TCB)));
    auto* stackBase = static_cast<uint8_t*>(kalloc(StackSize));

    self->body  = body;
    self->arg   = arg;
    self->state = ThreadState::Ready;

    // RISC-V ABI zahteva 16-bajtno poravnanje steka; kalloc poravnava samo
    // na 8, pa vrh steka dodatno poravnavamo nadole.
    const auto stackTop =
        alignDown16(reinterpret_cast<uint64_t>(stackBase) + StackSize);

    self->context     = Context{};
    self->context.ra  = reinterpret_cast<uint64_t>(&threadTrampoline);
    self->context.sp  = stackTop;

    return self;
}

void switchInto(TCB* thread) {
    currentTcb     = thread;
    thread->state  = ThreadState::Running;
    contextSwitch(&mainContext, &thread->context);
    // Kontrola se vraca ovde tek kad 'thread' pozove yieldToMain().
}

void yieldToMain() {
    TCB* self = currentTcb;
    if (self->state == ThreadState::Running) {
        self->state = ThreadState::Ready;
    }
    currentTcb = nullptr;
    contextSwitch(&self->context, &mainContext);
    // Kontrola se vraca ovde tek kad main() ponovo pozove switchInto(self).
}
