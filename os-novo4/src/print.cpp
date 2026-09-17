//
// Created my os 3.8.23.
//

#include "../h/print.hpp"
#include "../lib/console.h"
#include "../h/riscv.hpp"

void printString_l(char const *string) // test/printing
{
    uint64 sstatus = Riscv::r_sstatus();
    Riscv::mc_sstatus(Riscv::SSTATUS_SIE);

    while (*string != '\0')
    {
        __putc(*string);
        string++;
    }

    Riscv::ms_sstatus(sstatus & Riscv::SSTATUS_SIE ? Riscv::SSTATUS_SIE  : 0);
}

void printInteger_l(uint64 integer) { // test/printing
    uint64 sstatus = Riscv::r_sstatus();
    Riscv::mc_sstatus(Riscv::SSTATUS_SIE);

    static char digits[] = "0123456789ABCDEF";
    char buf[16];
    int i, neg;
    uint x;

    neg = 0;
    if(integer < 0){
        neg = 1;
        x = -integer;
    } else {
        x = integer;
    }

    i = 0;
    do{
        buf[i++] = digits[x % 10];
    }while((x /= 10) != 0);
    if(neg)
        buf[i++] = '-';

    while(--i >= 0)
        __putc(buf[i]);

    Riscv::ms_sstatus(sstatus & Riscv::SSTATUS_SIE ? Riscv::SSTATUS_SIE  : 0);
}
