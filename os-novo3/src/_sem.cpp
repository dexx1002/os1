//
// Created by os on 8/5/23.
//

#include "../h/_sem.hpp"

int _sem::signal() {
    if(!aktivan) return -1;
    if(value < 0){
        TCB* thr = blokirani.removeFirst();
        thr->setBlocked(false);
        Scheduler::put(thr);
    }
    value++;
    return 0;
}

int _sem::wait() {
    if(!aktivan) return -1;
    if(value <= 0) {
        blokirani.addLast(TCB::running);
        TCB::running->setBlocked(true);
        TCB::yield();
    }
    value--;
    return 0;
}

_sem::~_sem() {
    aktivan = false;
    TCB* thr = nullptr;
    if(blokirani.peekFirst()){
        while(blokirani.peekFirst()){
            thr = blokirani.removeFirst();
            thr->setBlocked(false);
            Scheduler::put(thr);
        }
    }
}

