//
// Created by os on 8/3/23.
//

#include "../lib/hw.h"
#include "../h/print.hpp"

// svuda je umesto:
// printString -> printString_l
// printInteger -> printInteger_l

static uint64 fibonacci(uint64 n){
    if (n == 0 || n == 1) { return n; }
    if (n % 10 == 0) {
        //thread_dispatch();
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

void workerBodyA(void* arg) {
    for (uint64 i = 0; i < 10; i++) {
        printString_l("A: i=");
        printInteger_l(i);
        printString_l("\n");
        for (uint64 j = 0; j < 10000; j++) {
            for (uint64 k = 0; k < 30000; k++) { /* busy wait */ }
            //thread_dispatch();
        }
    }
    printString_l("A finished!\n");
    //finishedA = true;
}

void workerBodyB(void* arg) {
    for (uint64 i = 0; i < 16; i++) {
        printString_l("B: i=");
        printInteger_l(i);
        printString_l("\n");
        for (uint64 j = 0; j < 10000; j++) {
            for (uint64 k = 0; k < 30000; k++) { /* busy wait */ }
            //thread_dispatch();
        }
    }
    printString_l("B finished!\n");
    //finishedB = true;
    //thread_dispatch();
}

void workerBodyC(void* arg) {
    uint8 i = 0;
    for (; i < 3; i++) {
        printString_l("C: i=");
        printInteger_l(i);
        printString_l("\n");
    }

    printString_l("C: dispatch\n");
    __asm__ ("li t1, 7");
    //thread_dispatch();

    uint64 t1 = 0;
    __asm__ ("mv %[t1], t1" : [t1] "=r"(t1));

    printString_l("C: t1=");
    printInteger_l(t1);
    printString_l("\n");

    uint64 result = fibonacci(12);
    printString_l("C: fibonaci=");
    printInteger_l(result);
    printString_l("\n");

    for (; i < 6; i++) {
        printString_l("C: i=");
        printInteger_l(i);
        printString_l("\n");
    }

    printString_l("C finished!\n");
    //finishedC = true;
    //thread_dispatch();
}

void workerBodyD(void* arg) {
    uint8 i = 10;
    for (; i < 13; i++) {
        printString_l("D: i=");
        printInteger_l(i);
        printString_l("\n");
    }

    printString_l("D: dispatch\n");
    __asm__ ("li t1, 5");
    //thread_dispatch();

    uint64 result = fibonacci(16);
    printString_l("D: fibonaci=");
    printInteger_l(result);
    printString_l("\n");

    for (; i < 16; i++) {
        printString_l("D: i=");
        printInteger_l(i);
        printString_l("\n");
    }

    printString_l("D finished!\n");
    //finishedD = true;
    //thread_dispatch();
}

