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

// Koliko hart-ova (jezgara) naš kernel podržava -- mora se poklapati sa
// '-smp N' u Makefile QEMUFLAGS-u I sa MAX_HARTS konstantom u boot.S
// (asembler ne može da deli ovu C++ konstantu direktno, pa se dve vrednosti
// moraju ručno držati usklađenim ako se ikad promene).
constexpr int MaxHarts = 2;

// Adrese Control and Status registara koje trenutno koristimo. Vrednosti su
// standardne po RISC-V Privileged Architecture specifikaciji.
enum class Csr : uint16_t {
    MStatus = 0x300,
    MIe     = 0x304,
    MTvec   = 0x305,
    MEpc    = 0x341,
    MCause  = 0x342,
    MHartId = 0xF14, // read-only -- identifikuje FIZIČKI hart koji izvršava
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

// Koji FIZIČKI hart trenutno izvršava ovaj kod. Ključno za SMP: svaki hart
// mora da indeksira SOPSTVENO stanje (scheduler-ov 'trenutna nit' zapis,
// CLINT tajmer registre) preko sopstvenog ID-a, ne deljenog globalnog stanja.
[[nodiscard]] inline uint64_t hartId() noexcept {
    return readCsr<Csr::MHartId>();
}

// Pojedinačni bitovi unutar mstatus/mie CSR-ova koje koristimo za tajmer
// prekid. Nazivi prate RISC-V Privileged spec konvencije (MSTATUS.MIE,
// MIE.MTIE).
namespace StatusBits {
constexpr uint64_t MachineInterruptEnable = 1ull << 3; // mstatus.MIE
}

// Kratka kritična sekcija: isključi prekide, uradi nešto malo i brzo (npr.
// proveri-pa-izmeni deljeno stanje semafora), pa ih vrati u prethodno
// stanje. Vraćena vrednost pamti da li su prekidi BILI uključeni pre poziva
// -- restoreInterrupts ih ne pali "na silu" ako su već bili isključeni
// (npr. ugnježdeni pozivi), nego samo vraća ono što je zatečeno.
[[nodiscard]] inline bool disableInterrupts() noexcept {
    const auto previous = readCsr<Csr::MStatus>();
    writeCsr<Csr::MStatus>(previous & ~StatusBits::MachineInterruptEnable);
    return (previous & StatusBits::MachineInterruptEnable) != 0;
}

inline void restoreInterrupts(bool wasEnabled) noexcept {
    if (wasEnabled) {
        writeCsr<Csr::MStatus>(readCsr<Csr::MStatus>() | StatusBits::MachineInterruptEnable);
    }
}

namespace InterruptEnableBits {
constexpr uint64_t MachineTimer = 1ull << 7; // mie.MTIE
}

// CLINT (Core Local Interruptor) -- mtime/mtimecmp NISU CSR-ovi, već
// memorijski mapirani registri na fiksnoj adresi koju QEMU 'virt' mašina
// koristi. mtime je JEDAN, zajednički registar (globalni sat), ali svaki
// hart ima SOPSTVENI mtimecmp na adresi Base + 0x4000 + 8*hartId -- paljenje
// tajmera na jednom hartu ne sme da dirne tuđi.
namespace Clint {
constexpr uintptr_t Base  = 0x02000000;
constexpr uintptr_t MTime = Base + 0xBFF8;

[[nodiscard]] inline uintptr_t mtimecmpAddress(uint64_t hart) noexcept {
    return Base + 0x4000 + 8 * hart;
}

[[nodiscard]] inline uint64_t readMtime() noexcept {
    return *reinterpret_cast<volatile uint64_t*>(MTime);
}

// Pali/pomera tajmer za TRENUTNI hart (čita mhartid interno) -- postojeći
// pozivi iz koraka 1-5 (svi na hartu 0) nastavljaju da rade identično, jer
// se hartId() tamo uvek razrešava na 0.
inline void writeMtimecmp(uint64_t value) noexcept {
    *reinterpret_cast<volatile uint64_t*>(mtimecmpAddress(hartId())) = value;
}
} // namespace Clint

// ~0.2s po tiku na QEMU 'virt' CLINT-u (koji radi na 10 MHz) -- deljena
// konstanta (ne lokalna u main.cpp) jer i secondaryHartMain() (smp.cpp)
// mora da naoruža SVOJ tajmer istim intervalom pre nego što uđe u
// schedulerRun(), inače nit prikovana za taj hart nikad ne bi bila
// preuzeta (beskonačna petlja bi zauvek blokirala ceo hart).
constexpr uint64_t TimerIntervalTicks = 2'000'000;

// Naoruži periodičan tajmer prekid NA TRENUTNOM hartu: pomeri mtimecmp,
// upali mie.MTIE (izvor prekida) i mstatus.MIE (globalni prekidački
// prekidač). Svaki hart ovo mora sam da uradi za sebe -- sve tri stvari su
// CSR/memorijski-mapirano stanje PO HARTU, ne deljeno.
inline void armPeriodicTimer() noexcept {
    Clint::writeMtimecmp(Clint::readMtime() + TimerIntervalTicks);
    writeCsr<Csr::MIe>(readCsr<Csr::MIe>() | InterruptEnableBits::MachineTimer);
    writeCsr<Csr::MStatus>(readCsr<Csr::MStatus>() | StatusBits::MachineInterruptEnable);
}

// QEMU 'virt' mašina emulira SiFive test/poweroff uređaj na ovoj fiksnoj
// adresi -- upis prave vrednosti STVARNO gasi QEMU proces sa odgovarajućim
// exit kodom. Bez ovoga bi kernel morao da završi u beskonačnoj 'wfi'
// petlji, što je u redu za ručno pokretanje (Ctrl+A X da izađeš), ali
// neupotrebljivo za CI -- skript ne bi imao pouzdan način da zna da li su
// testovi prošli, sem nagađanja preko timeout-a.
namespace Poweroff {
constexpr uintptr_t Address = 0x100000;
constexpr uint32_t  Pass    = 0x5555; // QEMU izlazi sa kodom 0
constexpr uint32_t  Fail    = 0x3333; // QEMU izlazi sa kodom 1

// [[noreturn]] je ovde iskreno: na pravom hardveru (gde ovaj memorijski
// mapiran uređaj ne postoji) upis nema efekta, pa se ugrađen 'wfi' fallback
// koristi kao bezbednosna mreža.
[[noreturn]] inline void exitQemu(uint32_t code) noexcept {
    *reinterpret_cast<volatile uint32_t*>(Address) = code;
    while (true) {
        asm volatile("wfi");
    }
}
} // namespace Poweroff

// Uzroci trapa koje prepoznajemo: sinhroni izuzeci (ecall) po privilegovanom
// režimu iz kog su pozvani, i asinhroni prekidi (najviši bit mcause-a je 1).
// Vrednosti su po RISC-V Privileged Architecture specifikaciji.
enum class TrapCause : uint64_t {
    EcallFromUMode         = 8,
    EcallFromSMode         = 9,
    EcallFromMMode         = 11,
    MachineTimerInterrupt  = 0x8000000000000007ULL,
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
