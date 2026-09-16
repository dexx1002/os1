//
// Created by os on 8/5/23.
//
#ifndef PROJEKAT_SEM_HPP
#define PROJEKAT_SEM_HPP

#include "../lib/hw.h"
#include "tcb.hpp"

class _sem{
public:
    _sem(int v = 1) : value(v), aktivan(true){ }
    int signal();
    int wait();
    ~_sem();

    void* operator new(size_t size){ // pogledati notepad++???
        return MemoryAllocator::mem_alloc(size);
    }
    void* operator new[](size_t size){ // pogledati notepad++???
        return MemoryAllocator::mem_alloc(size);
    }
    void operator delete(void* ptr){ MemoryAllocator::mem_free(ptr); }
    void operator delete[](void* ptr){ MemoryAllocator::mem_free(ptr); }
private:
    int value;
    List<TCB> blokirani;
    bool aktivan;
};

#endif //PROJEKAT_SEM_HPP