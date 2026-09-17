//
// Created by os on 8/3/23.
//

#ifndef OS1_CCB_HPP
#define OS1_CCB_HPP

#include "../lib/hw.h"
#include "../h/scheduler.hpp"

// Thread Control Block
class TCB{
public:
    using Body = void (*)(void*);  // kod kosovca je using Body = void (*)(void*);
    using Arg = void*;
    static TCB *createThread(Body body);
    static TCB *createThread(Body body, void* arg); //, uint64* stack

    bool isFinished() const { return finished; }
    void setFinished(bool value) { finished = value; }
    bool isBlocked() const { return blocked; }
    void setBlocked(bool value) { blocked = value; }

    uint64 getTimeSlice() const { return timeSlice; }

    static void yield();

    //static void bodyWrapper(void*); //???
    static TCB *running;

    ~TCB() { delete[] stack; }
private:
    TCB(Body body, uint64 timeSlice1, Arg arg = nullptr) : body(body),
                     stack(body != nullptr ? new uint64[STACK_SIZE] : nullptr),
                     context({
                                     (uint64)&threadWrapper, //body != nullptr ? (uint64) body : 0
                                     stack != nullptr ? (uint64) &stack[STACK_SIZE] : 0,
                             }),
                     //timeSlice(timeSlice1),
                     finished(false)
    {
        if(body != nullptr){
            Scheduler::put(this);
        }
        timeSlice = timeSlice1;
        arg = arg;
    }

    struct Context{
        uint64 ra; // gde treba da se vrati korutina
        uint64 sp; // pozicija na steku od koje je korutina je stavila podatke
    };
    Body body;
    Arg arg;
    uint64 *stack; // stek
    Context context;
    bool finished;
    bool blocked;
    uint64 timeSlice;
    static uint64 timeSliceCounter;

    static void threadWrapper();
    static void contextSwitch(Context *oldContext, Context *runningContext); // napravljen u contextSwitch.S
    static void dispatch();

    static uint64 constexpr STACK_SIZE = 1024;
    static uint64 constexpr TIME_SLICE = 2;

    friend class Riscv;
};

#endif //OS1_CCB_HPP