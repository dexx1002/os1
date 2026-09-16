//
// Created by os on 8/3/23.
//
#include "../h/scheduler.hpp"

List<TCB> Scheduler::readyThreadQueue;

TCB *Scheduler::get() {
    return readyThreadQueue.removeFirst();
}

void Scheduler::put(TCB *pcb) {
    readyThreadQueue.addLast(pcb);
}