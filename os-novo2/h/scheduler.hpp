//
// Created by os on 8/3/23.
//
#ifndef PROJEKAT_SCHEDULER_H
#define PROJEKAT_SCHEDULER_H

#include "list.hpp"

class TCB; // bilo je PCB umesto CCB

class Scheduler
{
private:
    static List<TCB> readyThreadQueue;
public:
    static TCB *get();
    static void put(TCB *pcb);
};

#endif
