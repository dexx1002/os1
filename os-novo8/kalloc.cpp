#include "riscv.hpp"
#include "spinlock.hpp"

// Najjednostavniji mogući alokator: "bump" alokator koji samo pomera
// pokazivač na sledeći slobodan blok. Nikad ne vraća memoriju (kfree je
// namerno no-op) -- to je iskreno ograničenje ove verzije, ne skriven bag.
// Pravi free-list dolazi u sledećem koraku, zajedno sa nitima/schedulerom.
//
// SMP napomena: trenutno SAMO hart 0 poziva kalloc (sve createThread()
// pozive radi hart 0 pre nego što se hart 1 pusti preko smpSignalStart()),
// pa spinlock ovde tehnički nije neophodan JOŠ -- ali je toliko jeftin i
// jednostavan da ga dodajemo odmah, umesto da ostavimo "minu" za kasnije
// ako se ta pretpostavka ikad promeni.
namespace Memory {

constexpr size_t PageSize = 4096;

// Simbol definisan u kernel.ld -- označava kraj .bss sekcije, odnosno
// početak slobodne memorije.
extern "C" char end[];

uintptr_t heapHead = 0;
Spinlock  heapLock;

void init() {
    if (heapHead == 0) {
        const auto current = reinterpret_cast<uintptr_t>(end);
        heapHead = (current + PageSize - 1) & ~(PageSize - 1);
    }
}

} // namespace Memory

extern "C" {

[[nodiscard]] void* kalloc(size_t bytes) {
    if (bytes == 0) {
        return nullptr;
    }

    spinLock(&Memory::heapLock);

    Memory::init();

    constexpr size_t alignment = 8;
    const size_t alignedBytes = (bytes + alignment - 1) & ~(alignment - 1);

    void* ptr = reinterpret_cast<void*>(Memory::heapHead);
    Memory::heapHead += alignedBytes;

    spinUnlock(&Memory::heapLock);

    // TODO (sledeći korak): provera da heapHead nije prešao HEAP_END, i
    // pravo oslobađanje kroz free-list umesto bump pokazivača.
    return ptr;
}

void kfree(void* ptr) {
    // Namerno no-op u ovoj verziji -- bump alokator ne vraća memoriju.
    (void)ptr;
}

} // extern "C"
