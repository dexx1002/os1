#include "riscv.hpp"

namespace UART {
    constexpr uintptr_t UART_BASE = 0x10000000;

    auto* const RHR = reinterpret_cast<volatile uint8_t*>(UART_BASE + 0);
    auto* const THR = reinterpret_cast<volatile uint8_t*>(UART_BASE + 0);
    auto* const LSR = reinterpret_cast<volatile uint8_t*>(UART_BASE + 5);

    constexpr uint8_t LSR_RX_READY = 1 << 0;
    constexpr uint8_t LSR_TX_IDLE  = 1 << 5;
}

extern "C" {

void putc(char c) {
    while ((*UART::LSR & UART::LSR_TX_IDLE) == 0) {}
    *UART::THR = static_cast<uint8_t>(c);
}

char getc() {
    if ((*UART::LSR & UART::LSR_RX_READY) == 0) {
        return '\0';
    }
    return static_cast<char>(*UART::RHR);
}

void printString(const char* str) {
    while (*str) {
        putc(*str++);
    }
}

} // extern "C"