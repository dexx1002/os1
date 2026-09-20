#include "smp.hpp"
#include "scheduler.hpp"

namespace {
// Obična (ne-atomska) volatile zastavica je dovoljna ovde -- samo JEDAN
// pisac (hart 0, tačno jednom) i čitaoci samo čitaju dok ne postane 'true'.
// Nema kritične sekcije, pa nam ne treba spinlock.
volatile bool startFlag = false;
} // namespace

void smpWaitForStart() {
    // NAMERNO busy-spin, NE 'wfi': 'wfi' bi zahtevao da neko pošalje
    // prekid BAŠ ovom hartu da ga probudi (npr. IPI preko CLINT msip
    // registra), a mi tu infrastrukturu nismo gradili -- ovde samo čekamo
    // da hart 0 promeni običnu memorijsku promenljivu, što plain busy-spin
    // ispravno detektuje bez ikakve dodatne opreme.
    while (!startFlag) {
    }
}

void smpSignalStart() {
    startFlag = true;
}

extern "C" [[noreturn]] void secondaryHartMain() {
    smpWaitForStart();

    // KRITIČNO: bez ovoga, nit prikovana za ovaj hart (npr. beskonačna
    // petlja) nikad ne bi bila preuzeta -- mie.MTIE i mstatus.MIE su PO
    // HARTU CSR-ovi, hart 0 ih pali samo za SEBE (vidi main.cpp), pa svaki
    // hart mora sam za sebe ovo da uradi pre nego što počne da izvršava
    // tuđi (potencijalno beskonačan) kod.
    RISCV::armPeriodicTimer();

    // Ovaj hart se ponaša kao "radnik": učestvuje u istom scheduleru kao i
    // hart 0, preuzimajući niti iz SVOG (spinlock-zaštićenog, po-hartu)
    // ready reda. Kad schedulerRun() vrati kontrolu (stopRequested postane
    // true), ovaj hart nema više šta da radi u ovoj verziji -- program se
    // uskoro gasi u celini preko poweroff mehanizma (koji hart 0 poziva),
    // pa je prosto parkiranje dovoljno.
    schedulerRun();

    while (true) {
        asm volatile("wfi");
    }
}
