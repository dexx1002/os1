//
// Created by os on 7/29/23.
//
#include "../h/MemoryAllocator.hpp"

MemoryAllocator::MemBlock* MemoryAllocator::headFree = nullptr;
MemoryAllocator::MemBlock* MemoryAllocator::headUsed = nullptr;
uint32 MemoryAllocator::initDone = 0;

//extern const void* HEAP_START_ADDR;
//extern const void* HEAP_END_ADDR;
//static const size_t MEM_BLOCK_SIZE = 64;

void *MemoryAllocator::mem_alloc(size_t size) { // pretostavka da (size_t size) je vec ukljucio sizeof(MemBlock)
    if(initDone == 0)
        return nullptr;
    if(size <= 0)
        return nullptr;
    if(headFree == nullptr)
        return nullptr; // nema vise praznih blokova

    // Ideja:
    // da se prate zauzeti i slobodni blokovi
    // na pocetku slobodnih blokova se nalazi struktura za pracenje
    // na pocetku zauzetih blokova se nalazi struktura za pracenje

    MemBlock* tmp = headFree;
    uchar provera = 0;
    while(tmp){ // treba proveriti da se ne vrti beskonacno u krug
        if(tmp->size < size * MEM_BLOCK_SIZE){
            tmp = tmp->next;
            continue;
        }
        if(tmp->size - size * MEM_BLOCK_SIZE < sizeof(MemBlock)){
            if(tmp->prev)
                tmp->prev->next = tmp->next;
            else
                headFree = tmp->next;
            if(tmp->next)
                tmp->next->prev = tmp->prev;
            provera = 1;
            break;
        }
        if(tmp->size - size * MEM_BLOCK_SIZE >= sizeof(MemBlock)){
            MemBlock* noviBlok = (MemBlock*)((char*)tmp + size * MEM_BLOCK_SIZE);
            noviBlok->next = tmp->next;
            noviBlok->prev = tmp->prev;
            noviBlok->size = tmp->size - size * MEM_BLOCK_SIZE;
            if(tmp->prev)
                tmp->prev->next = noviBlok;
            else
                headFree = noviBlok;
            if(tmp->next)
                tmp->next->prev = noviBlok;
            provera = 1;
            break;
        }
        tmp = tmp->next;
    }
    if(provera == 0)
        return nullptr; // ne postoji slobodni blok dovoljne velicine
    // provera == 1, znaci nasli i ubacili u slobodan blok

    MemBlock* prev_b = nullptr; // zauzeti blok iza
    if(headUsed == nullptr || (char*)tmp < (char*)headUsed)
        prev_b = nullptr;
    else {
        prev_b = headUsed;
        while (prev_b->next != nullptr && (char *) (prev_b->next) < (char *) tmp) {
            tmp = tmp->next;
        }
    }
    MemBlock* noviZBlok = tmp;
    MemBlock* next_b = nullptr; // zauzeti blok ispred
    if(prev_b != nullptr)
        next_b = prev_b->next;
    noviZBlok->next = next_b;
    noviZBlok->prev = prev_b;
    noviZBlok->size = size * MEM_BLOCK_SIZE;
    if(prev_b)
        prev_b->next = noviZBlok;
    else
        headUsed = noviZBlok;
    if(next_b)
        next_b->prev = noviZBlok;
    else
        noviZBlok->next = headUsed; //videti kasnije??? mozda treba nullptr da bi poslednji u listi imao nullptr
    return (void*)((char*)noviZBlok + sizeof(MemBlock));
}

int MemoryAllocator::mem_free(void *adr) {
    if(initDone == 0)
        return -1;
    if(adr == nullptr)
        return -2;
    MemBlock* trazeniBlok = (MemBlock*)((char*)adr - sizeof(MemBlock));
    MemBlock* tmp = headUsed;
    uchar pronadjen = 0;
    while(1){
        if(tmp > trazeniBlok)
            break;
        if(tmp == trazeniBlok){
            pronadjen = 1;
            break;
        }
        tmp = tmp->next;
    }

    if(pronadjen == 0){
        return -3; // greska, adresa je na pola zauzetog segmenta
    }

    MemBlock* pret1 = tmp->prev;
    MemBlock* next1 = tmp->next;
    if(pret1 != nullptr)
        pret1->next = next1;
    else
        headUsed = next1;
    if(next1)
        next1->prev = pret1;

    if(headFree == nullptr){
        headFree = trazeniBlok;
        trazeniBlok->next = nullptr;
        trazeniBlok->prev = nullptr;
        // zadrava isti size
    }
    else{
        tmp = nullptr;
        if((char*)trazeniBlok > (char*)(headFree))
            for(tmp = headFree; tmp->next != nullptr && (char*)(tmp->next) < (char*)trazeniBlok; tmp = tmp->next);
        if(tmp == nullptr){ // varijanta ako treba dodati na pocetak liste slobodnih
            trazeniBlok->next = headFree;
            trazeniBlok->prev = nullptr;
            headFree->prev = trazeniBlok;
            headFree = trazeniBlok;
        }
        else{ // varijanta ako postoji bar jedan blok ispred(tmp)
            next1 = tmp->next;
            tmp->next = trazeniBlok;
            trazeniBlok->prev = tmp;
            trazeniBlok->next = next1;
            if(next1 != nullptr)
                next1->prev = trazeniBlok;
        }

        // proces povezivanja ako su susedni
        pret1 = trazeniBlok->prev;
        try_join(trazeniBlok);
        if(pret1 != nullptr)
            try_join(pret1);
    }

    return 1;
}

void MemoryAllocator::try_join(MemBlock* tmp){
    if(tmp->next && (char*)(tmp + tmp->size) == (char*)(tmp->next)){
        tmp->size += tmp->next->size;
        tmp->next = tmp->next->next;
        if(tmp->next)
            tmp->next->prev = tmp;
    }
}

void MemoryAllocator::init() {
    if(initDone == 0){
        headFree = (MemBlock*)HEAP_START_ADDR;
        headFree->next = nullptr;
        headFree->prev = nullptr;
        headFree->size = (size_t)HEAP_END_ADDR - (size_t)HEAP_START_ADDR; //((size_t)HEAP_END_ADDR - (size_t)HEAP_START_ADDR) / (MEM_BLOCK_SIZE - 1) * MEM_BLOCK_SIZE;
        headUsed = nullptr;
        initDone = 1;
    }
}

