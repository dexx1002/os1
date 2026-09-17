//
// Created by os on 7/29/23.
//
#ifndef projekat_syscall_c_hpp
#define projekat_syscall_c_hpp

#include "../lib/hw.h"
//#include "../lib/console.h"   videti kasnije

//static const uint64 MALLOC_CODE = 0x01;  // zanimljiva ideja kosovca, za svaku funckiju napravi staticki kod

const int EOF = -1;

class _sem; // napravljeno samo zbog imena
typedef _sem* sem_t;

class TCB; // napravljeno samo zbog imena
typedef TCB* thread_t;

void* mem_alloc(size_t size);
int mem_free(void*);
int thread_create(thread_t* handle, void(*start_routine)(void*), void* arg);
int thread_exit();
void thread_dispatch();
void thread_join(thread_t handle);
int sem_open(sem_t* handle, uint32 init);
int sem_close(sem_t handle);
int sem_wait(sem_t id);
int sem_signal(sem_t id);
int time_sleep(time_t);
char getc();
void putc(char c);

#endif