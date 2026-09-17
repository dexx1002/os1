//
// Created by os on 7/29/23.
//
#ifndef PROJEKAT_MEMORY_ALLOCATOR_HPP
#define PROJEKAT_MEMORY_ALLOCATOR_HPP

#include "../lib/hw.h"

class MemoryAllocator {
public:
    struct MemBlock {  // koristi se i za slobodne i za zauzete blokove
        MemBlock *next;
        MemBlock *prev;
        size_t size;
    };

    static void *mem_alloc(size_t size);
    static int mem_free(void *adr);

    //static MemoryAllocator* getInstance();
    static void init();

private:
    //static MemoryAllocator* memAloc;
    static MemBlock *headFree;
    static MemBlock *headUsed;
    static uint32 initDone;
    static void try_join(MemBlock*);

    MemoryAllocator() {};
};

#endif
