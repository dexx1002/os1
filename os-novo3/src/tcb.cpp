//
// Created by os on 8/3/23.
//
#include "../h/tcb.hpp"
#include "../h/riscv.hpp"

TCB* TCB::running = nullptr;
uint64 TCB::timeSliceCounter = 0;

TCB *TCB::createThread(TCB::Body body) {
    return new TCB(body, TIME_SLICE);
}
TCB *TCB::createThread(TCB::Body body, void *arg) { //, uint64 *stack
    return new TCB(body, TIME_SLICE, arg);
}


void TCB::yield() {
    __asm__ volatile("ecall");
    /*
    Riscv::pushRegisters();
    TCB::dispatch();
    Riscv::popRegisters();
    */
}

void TCB::dispatch() {
    TCB *old = running;
    if (!(old->isFinished())) { // bilo je if (!(old->isFinished()) && !(old->isBlocked())) {
        Scheduler::put(old);
    }
    running = Scheduler::get();
    TCB::contextSwitch(&old->context, &running->context);
}

void TCB::threadWrapper() {
    Riscv::popSppSpie();
    running->body(running->arg);
    running->setFinished(true);
    TCB::yield();
}