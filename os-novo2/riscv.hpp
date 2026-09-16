#ifndef RISCV_HPP
#define RISCV_HPP

// Ovaj toolchain (minimalan riscv64-unknown-elf-g++, bez punog libstdc++
// header seta) nema <cstdint>/<cstddef>. Zato koristimo GCC/Clang ugrađene
// (builtin) tipove -- rade bez ijednog #include-a, a tačno odgovaraju
// tipovima koje bi <cstdint> inače definisao preko standardne biblioteke.
using uint8_t   = __UINT8_TYPE__;
using uint16_t  = __UINT16_TYPE__;
using uint32_t  = __UINT32_TYPE__;
using uint64_t  = __UINT64_TYPE__;
using uintptr_t = __UINTPTR_TYPE__;
using size_t    = decltype(sizeof(0));

// Ovaj header okuplja sve RISC-V specificne detalje (CSR registri, layout
// trap frame-a) na jedno mesto, umesto da se raspiruju kroz .cpp fajlove.

namespace RISCV {

// Adrese Control and Status registara koje trenutno koristimo. Vrednosti su
// standardne po RISC-V Privileged Architecture specifikaciji.
enum class Csr : uint16_t {
    MStatus = 0x300,
    MTvec   = 0x305,
    MEpc    = 0x341,
    MCause  = 0x342,
};

// Generičko čitanje/pisanje CSR-a preko compile-time parametra. CSR adresa
// mora biti poznata u compile-time-u (deo je same instrukcije), pa je
// non-type template parametar prirodan izbor za ovo u C++20 -- umesto
// zasebne r_/w_ funkcije za svaki registar (kao u prethodnoj verziji).
template <Csr Reg>
[[nodiscard]] inline uint64_t readCsr() noexcept {
    uint64_t value;
    asm volatile("csrr %0, %1" : "=r"(value) : "i"(static_cast<unsigned>(Reg)));
    return value;
}

template <Csr Reg>
inline void writeCsr(uint64_t value) noexcept {
    asm volatile("csrw %0, %1" : : "i"(static_cast<unsigned>(Reg)), "r"(value));
}

// Uzroci sinhronih izuzetaka (ecall) po privilegovanom režimu iz kog su
// pozvani -- vrednosti mcause registra po specifikaciji.
enum class TrapCause : uint64_t {
    EcallFromUMode = 8,
    EcallFromSMode = 9,
    EcallFromMMode = 11,
};

// Snimak svih opštih registara u trenutku trapa. Redosled polja MORA tačno
// da odgovara redosledu čuvanja/vraćanja u boot.S (SAVE_REGS/RESTORE_REGS
// makroi). static_assert ispod je "osiguranje": ako neko izmeni jedan fajl
// bez drugog, build će pući umesto da tiho unese bag.
struct TrapFrame {
    uint64_t ra, sp, gp, tp;
    uint64_t t0, t1, t2;
    uint64_t s0, s1;
    uint64_t a0, a1, a2, a3, a4, a5, a6, a7;
    uint64_t s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
    uint64_t t3, t4, t5, t6;
    // boot.S čuva tačno 31 registar (248 bajtova), ali pomera sp za 256
    // bajtova jer RISC-V ABI zahteva 16-bajtno poravnanje steka (248 nije
    // deljivo sa 16, 256 jeste). Ovo polje predstavlja taj neiskorišćeni
    // padding, da struct veličina iskreno odgovara stvarnoj alokaciji.
    uint64_t _stackAlignmentPadding;
};

static_assert(sizeof(TrapFrame) == 256,
              "TrapFrame mora biti 256B: 31 registar + 8B padding za poravnanje (vidi boot.S)");
static_assert(__builtin_offsetof(TrapFrame, a0) == 72,
              "Layout TrapFrame-a mora pratiti redosled cuvanja u boot.S");
static_assert(__builtin_offsetof(TrapFrame, t6) == 240,
              "Layout TrapFrame-a mora pratiti redosled cuvanja u boot.S");

} // namespace RISCV

#endif // RISCV_HPP
