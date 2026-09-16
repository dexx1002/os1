//
// Created by os on 7/29/23.
//
#include "../h/syscall_c.hpp"
#include "../h/MemoryAllocator.hpp"
// razmisliti o tipu podataka koje se vracaju (int,...)

void* mem_alloc(size_t size){
    size_t prom = (size + sizeof(MemoryAllocator::MemBlock) + MEM_BLOCK_SIZE - 1) / MEM_BLOCK_SIZE; // treba ovde mozda dodati sizeof(MemoryAllocator::MemBlock)
    __asm__ volatile("mv a1, %0" : : "r"(prom));
    __asm__ volatile("li a0, 0x1");
    __asm__ volatile("ecall"); // sistemski poziv
    void* rez;
    __asm__ volatile("mv %0, a0" : "=r" (rez));
    return rez;
}

int mem_free(void* ptr){
    __asm__ volatile("mv a1, %0" : : "r"(ptr));
    __asm__ volatile("li a0, 0x2");
    __asm__ volatile("ecall"); // sistemski poziv
    int rez;
    __asm__ volatile("mv %0, a0" : "=r" (rez));
    return rez;
}

int thread_create(thread_t* handle, void(*start_routine)(void*), void* arg){ // treba drugacije !!!!!!!!!!!
    // treba drugacije !!!!!!!!!!!
    //void* stek = mem_alloc(DEFAULT_STACK_SIZE);
    //__asm__ volatile("mv a4, %0" : : "r"(stek));
    __asm__ volatile("mv a3, %0" : : "r"(arg));
    __asm__ volatile("mv a2, %0" : : "r"(start_routine));
    __asm__ volatile("mv a1, %0" : : "r"(handle));
    __asm__ volatile("li a0, 0x11");
    __asm__ volatile("ecall");
    int rez;
    __asm__ volatile("mv %0, a0" : "=r" (rez));
    return rez;
}

int thread_exit(){
    __asm__ volatile("li a0, 0x12");
    __asm__ volatile("ecall");
    int rez;
    __asm__ volatile("mv %0, a0" : "=r" (rez));
    return rez;
}

void thread_dispatch(){
    __asm__ volatile("li a0, 0x13");
    __asm__ volatile("ecall");
}

void thread_join(thread_t handle){ // proveriti
    __asm__ volatile("mv a1, %0" : : "r" (handle));
    __asm__ volatile("li a0, 0x14");
    __asm__ volatile("ecall");
}

int sem_open(sem_t* handle, uint32 init){
    __asm__ volatile("mv a2, %0" : : "r"(init));
    __asm__ volatile("mv a1, %0" : : "r"(handle));
    __asm__ volatile("li a0, 0x21");
    __asm__ volatile("ecall");
    int rez;
    __asm__ volatile("mv %0, a0" : "=r" (rez));
    return rez;
}

int sem_close(sem_t handle){
    __asm__ volatile("mv a1, %0" : : "r"(handle));
    __asm__ volatile("li a0, 0x22");
    __asm__ volatile("ecall");
    int rez;
    __asm__ volatile("mv %0, a0" : "=r" (rez));
    return rez;
}

int sem_wait(sem_t id){
    __asm__ volatile("mv a1, %0" : : "r"(id));
    __asm__ volatile("li a0, 0x23");
    __asm__ volatile("ecall");
    int rez;
    __asm__ volatile("mv %0, a0" : "=r" (rez));
    return rez;
}

int sem_signal(sem_t id){
    __asm__ volatile("mv a1, %0" : : "r"(id));
    __asm__ volatile("li a0, 0x24");
    __asm__ volatile("ecall");
    int rez;
    __asm__ volatile("mv %0, a0" : "=r" (rez));
    return rez;
}

int time_sleep(time_t t){
    __asm__ volatile("mv a1, %0" :  : "r"(t));
    __asm__ volatile("li a0, 0x31");
    __asm__ volatile("ecall");

    int result;
    __asm__ volatile("mv %0, a0" : "=r"(result));
    return result;
}

char getc(){
    __asm__ volatile("li a0, 0x41");
    __asm__ volatile("ecall");
    char rez;
    __asm__ volatile("mv %0, a0" : "=r" (rez));
    return rez;
}

void putc(char c){
    __asm__ volatile("mv a1, %0" : : "r"(c));
    __asm__ volatile("li a0, 0x42");
    __asm__ volatile("ecall");
}
