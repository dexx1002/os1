//
// Created by os on 8/3/23.
//
#include "../h/riscv.hpp" // bilo je #include "../h/riscv.h"
#include "../h/tcb.hpp"
#include "../lib/console.h"
#include "../h/MemoryAllocator.hpp"
#include "../h/_sem.hpp"
#include "../lib/mem.h"

uint64 timerCount = 0;


void Riscv::popSppSpie() // ovde se vrsi povratak iz privilegovanog rezima rada(prekidne rutine)
{
    __asm__ volatile ("csrw sepc, ra");
    __asm__ volatile ("sret");
}

void Riscv::handleSupervisorTrap() { // za svaki izuzetak(exception) ili trap
    uint64 scause = r_scause();

    //uint64 scauseVar;
    //__asm__ volatile("csrr %[ime], scause" : [ime] "=r" (scauseVar));
    if(scause == 0x0000000000000009UL){ // sistemski pozivi(ecall) - poziv iz TCB::yield(), ovde se ulazi sa pozivom "ecall" (sinhrona promena konteksta)
        // interrupt: no, cause code: environment call from S-mode (9)

        uint64 code;
        __asm__ volatile("mv %0, a0" : "=r" (code));

        if(code == 0x01){
            size_t p_size;
            __asm__ volatile("mv %0, a1" : "=r"(p_size));
            //void* rez = MemoryAllocator::mem_alloc(p_size);
            void* rez = __mem_alloc(p_size); // proba
            __asm__ volatile("mv a0, %0" : : "r"(rez));
        }
        else if(code == 0x02){
            void* ptr;
            __asm__ volatile("mv %0, a1" : "=r"(ptr));
            //int rez = MemoryAllocator::mem_free(ptr);
            int rez = __mem_free(ptr); // proba
            __asm__ volatile("mv a0, %0" : : "r"(rez));
        }
        else if(code == 0x11){ // thread_create
            void* start_routine;
            void* arg;
            TCB** handle;
            //void* stek;
            __asm__ volatile("mv %0, a1" : "=r"(handle));
            __asm__ volatile("mv %0, a2" : "=r"(start_routine));
            __asm__ volatile("mv %0, a3" : "=r"(arg));
            //__asm__ volatile("mv %0, a4" : "=r"(stek));
            (*handle) = TCB::createThread((void(*)(void*))start_routine, arg); //, stek
            if(*handle == 0)
                __asm__ volatile("mv a0, %0" : : "r"(0xffffffffffffffff));
            else
                __asm__ volatile("mv a0, %0" : : "r"(0));
        }
        else if(code == 0x12){ // thread_exit
            TCB::running->setFinished(true);
            TCB::dispatch();
            __asm__ volatile("mv a0, %0" : : "r"(0)); // vraca se 0, uspesno zavrseno
        }
        else if(code == 0x13){ // thread_dispatch
            TCB::dispatch();
        }
        else if(code == 0x14){ // thread_join
            // thread_join
        }
        else if(code == 0x21){
            _sem** ptr;
            uint32 init;
            __asm__ volatile("mv %0, a1" : "=r"(ptr));
            __asm__ volatile("mv %0, a2" : "=r"(init));
            *ptr = new _sem(init); // videti !!!
            __asm__ volatile("mv a0, %0" : : "r"(0));
        }
        else if(code == 0x22){
            _sem* ptr;
            __asm__ volatile("mv %0, a1" : "=r"(ptr));
            delete ptr; // videti !!!
            __asm__ volatile("mv a0, %0" : : "r"(0));
        }
        else if(code == 0x23){
            _sem* ptr;
            __asm__ volatile("mv %0, a1" : "=r"(ptr));
            int rez = ptr->wait();
            __asm__ volatile("mv a0, %0" : : "r"(rez));
        }
        else if(code == 0x24){
            _sem* ptr;
            __asm__ volatile("mv %0, a1" : "=r"(ptr));
            int rez = ptr->signal();
            __asm__ volatile("mv a0, %0" : : "r"(rez));
        }
        else if(code == 0x31){

        }
        else if(code == 0x41){ // getc()
            //char c = __get(); // poziv za console, uraditi kasnije
            char c = __getc();
            __asm__ volatile("mv a0, %0" : : "r"(c));
        }
        else if(code == 0x42){ // putc()
            char c;
            __asm__ volatile("mv %0, a1" : "=r"(c));
            //__put(c); poziv za console, uraditi kasnije
            __putc(c); // proba
        }
        else{
            uint64 sepc = r_sepc() + 4; // moramo da se vratimo na instrukciju iza ecall-a,
            // zato dodajemo +4(sve instrucije su duzine 4 bajta)
            uint64 sstatus = r_sstatus();
            TCB::timeSliceCounter = 0;
            TCB::dispatch(); // koristi se dispatch(), jer smo registre vec sacuvali
            w_sstatus(sstatus);
            w_sepc(sepc);
        }

    }
    else if(scause == 0x0000000000000002UL){ // ilegalna instrukcija

    }
    else if(scause == 0x0000000000000005UL){ // nedozvoljenja adresa citanja

    }
    else if(scause == 0x0000000000000007UL){ // nedozvoljenja adresa upisa

    }
    else if(scause == 0x8000000000000001UL) { // prekid od tajmera(upada u supervizorski softverski prekid)
        // interrupt: yes, cause code: supervisor software interrupt (timer)

        timerCount++;
        if(timerCount >= 50){
            __putc('a');
            __putc('\n');
            timerCount = 0;
        }
        __asm__ volatile("csrc sip, 0x02"); // mora da se clear-uje signal za

        /*
        TCB::timeSliceCounter++;
        if(TCB::timeSliceCounter >= TCB::running->getTimeSlice()){
            uint64 sepc = r_sepc();
            uint64 sstatus = r_sstatus();
            TCB::timeSliceCounter = 0;
            TCB::dispatch(); // koristi se dispatch(), jer smo registre vec sacuvali
            w_sstatus(sstatus);
            w_sepc(sepc);
        }
        mc_sip(SIP_SSIP);*/
    }
    else if(scause == 0x8000000000000009UL){ // spoljasnji prekidi
        // interrupt: yes, cause code: supervisor external interrupt (console)
        console_handler();
    }
    else{ // neki nepredvidjeni slucaj
        // unexpected trap cause
        // print(scause)
        // print(sepc)
        // print(stval)
    }

    //__asm__ volatile("csrc sip, 0x02"); // mora da se clear-uje signal za prekid u sip-u
    //console_handler();
}