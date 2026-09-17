#include "../lib/hw.h"
#include "../lib/console.h"
#include "../lib/mem.h"
#include "../h/MemoryAllocator.hpp"
#include "../h/tcb.hpp"
#include "../h/workers.hpp"
#include "../h/print.hpp"
#include "../h/riscv.hpp"
#include "../h/_sem.hpp"

// trenutno se ne koristi ova varijanta
extern "C" void handleSupervisorTrap1() {
    uint64 scauseVar;
    __asm__ volatile("csrr %[ime], scause" : [ime] "=r" (scauseVar));
    if(scauseVar == 8 || scauseVar == 9){ // sistemski pozivi(ecall)

    }
    else if(scauseVar == 2){ // ilegalna instrukcija

    }
    else if(scauseVar == 5){ // nedozvoljenja adresa citanja

    }
    else if(scauseVar == 7){ // nedozvoljenja adresa upisa

    }
    else if(scauseVar == (0x01UL << 63 | 0x09)){ // spoljasnji prekidi
        //console_handler();
    }
    else if(scauseVar == (0x01UL << 63 | 0x01)){ // prekid od tajmera

    }
    //__asm__ volatile("csrc sip, 0x02"); // mora da se clear-uje signal za prekid u sip-u
    //console_handler();
}
extern "C" void supervisorTrap();



int main() {
    //__asm__ volatile("csrw stvec, %[vector]" : : [vector] "r" (&supervisorTrap)); posle ponovo ukljuciti!
    //__asm__ volatile("csrs sstatus, 0x02"); // 0b00000010, csrs - instrukcija koja setuje(operacija OR 1h) odredjene bitove
    //int* prom = new int;

    Riscv::w_stvec((uint64)&Riscv::supervisorTrap); // prekidna rutina se postavlja na supervisorTrap
    Riscv::ms_sstatus(Riscv::SSTATUS_SIE); // sie = 1

    while(1){}

    /*
    TCB* threads[5];
    threads[0] = TCB::createThread(nullptr);
    TCB::running = threads[0];

    threads[1] = TCB::createThread(workerBodyA);
    printString_l("ThreadA created\n");
    threads[2] = TCB::createThread(workerBodyB);
    printString_l("ThreadB created\n");
    threads[3] = TCB::createThread(workerBodyC);
    printString_l("ThreadC created\n");
    threads[4] = TCB::createThread(workerBodyD);
    printString_l("ThreadD created\n");

    Riscv::w_stvec((uint64)&Riscv::supervisorTrap); // prekidna rutina se postavlja na supervisorTrap
    Riscv::ms_sstatus(Riscv::SSTATUS_SIE); // sie = 1

    while(!(threads[1]->isFinished() && threads[2]->isFinished() && threads[3]->isFinished() && threads[4]->isFinished())){
        TCB::yield();
    }

    for(auto &thread : threads){
        delete thread;
    }
    printString_l("Finished\n");
    return 0;
     */
}