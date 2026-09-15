#ifndef RISCV_HPP
#define RISCV_HPP

typedef unsigned long long uint64_t;
typedef unsigned int       uint32_t;
typedef unsigned short     uint16_t;
typedef unsigned char      uint8_t;
typedef unsigned long      uintptr_t;
typedef uintptr_t          size_t;

namespace RISCV {
    inline uint64_t r_mcause() {
        uint64_t x;
        asm volatile("csrr %0, mcause" : "=r" (x));
        return x;
    }

    inline uint64_t r_mepc() {
        uint64_t x;
        asm volatile("csrr %0, mepc" : "=r" (x));
        return x;
    }

    inline void w_mepc(uint64_t x) {
        asm volatile("csrw mepc, %0" : : "r" (x));
    }

    inline uint64_t r_mstatus() {
        uint64_t x;
        asm volatile("csrr %0, mstatus" : "=r" (x));
        return x;
    }
}

#endif