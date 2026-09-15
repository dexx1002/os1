#include "riscv.hpp"

namespace Memory {
    constexpr size_t PAGE_SIZE = 4096;

    // Simbol iz kernel.ld
    extern "C" char end[];

    static uintptr_t heap_head = 0;

    void init() {
        if (heap_head == 0) {
            uintptr_t current = reinterpret_cast<uintptr_t>(end);
            heap_head = (current + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        }
    }
}

extern "C" {

void* kalloc(size_t bytes) {
    Memory::init();
    if (bytes == 0) return nullptr;

    size_t aligned_bytes = (bytes + 7) & ~static_cast<size_t>(7);
    void* ptr = reinterpret_cast<void*>(Memory::heap_head);
    Memory::heap_head += aligned_bytes;

    return ptr;
}

void kfree(void* ptr) {
    (void)ptr;
}

} // extern "C"