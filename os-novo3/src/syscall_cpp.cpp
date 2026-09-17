//
// Created by os on 7/29/23.
//
#include "../h/syscall_cpp.hpp"
#include "../h/scheduler.hpp"

void* operator new (size_t size){
    return mem_alloc(size);
}
void *operator new[](size_t size) {
    return mem_alloc(size);
}

void operator delete (void* p) noexcept{
    mem_free(p);
}
void operator delete[](void *p) noexcept {
    mem_free(p);
}



//--------------------------------
Thread::Thread(void (*body)(void *), void *arg) {
    this->body = body;
    this->arg = arg;
    thread_create(&myHandle, body, arg);
}

Thread::~Thread() { // Thread::~Thread() noexcept
    //delete myHandle;
}

int Thread::start(){
    Scheduler::put(myHandle);
    return 0;
}

void Thread::join(){
    thread_join(myHandle);
}

void Thread::dispatch() {
    thread_dispatch();
}

int Thread::sleep(time_t time) {
    return time_sleep(time);
}

Thread::Thread() {
    //thread_create(&myHandle, body, arg);
    myHandle = nullptr;
    body = nullptr;
    arg = nullptr;
}

//--------------------------------
Semaphore::Semaphore(unsigned int init) {
    sem_open(&myHandle, init);
}

Semaphore::~Semaphore() {
    sem_close(myHandle);
}

int Semaphore::wait() {
    return sem_wait(myHandle);
}

int Semaphore::signal() {
    return sem_signal(myHandle);
}

//--------------------------------
void PeriodicThread::terminate() {
    //???
}

PeriodicThread::PeriodicThread(time_t period) {
    //???
}

//--------------------------------
char Console::getc() {
    return ::getc();
}

void Console::putc(char c) {
    ::putc(c);
}