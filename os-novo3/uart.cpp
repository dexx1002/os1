#include "riscv.hpp"

// NS16550A-kompatibilan UART na adresi koju QEMU 'virt' mašina koristi za
// svoju konzolu.
namespace UART {

constexpr uintptr_t Base = 0x10000000;

auto* const Rhr = reinterpret_cast<volatile uint8_t*>(Base + 0);
auto* const Thr = reinterpret_cast<volatile uint8_t*>(Base + 0);
auto* const Lsr = reinterpret_cast<volatile uint8_t*>(Base + 5);

constexpr uint8_t LsrRxReady = 1 << 0;
constexpr uint8_t LsrTxIdle  = 1 << 5;

} // namespace UART

extern "C" {

void putc(char c) {
    while ((*UART::Lsr & UART::LsrTxIdle) == 0) {
    }
    *UART::Thr = static_cast<uint8_t>(c);
}

// [[nodiscard]]: ignorisanje povratne vrednosti obično znači da je znak
// tiho izgubljen -- verovatnije bag nego namera.
[[nodiscard]] char getc() {
    if ((*UART::Lsr & UART::LsrRxReady) == 0) {
        return '\0';
    }
    return static_cast<char>(*UART::Rhr);
}

void printString(const char* str) {
    while (*str) {
        putc(*str++);
    }
}

} // extern "C"
