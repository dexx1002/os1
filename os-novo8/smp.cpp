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
    // ready reda.
    //
    // KRITIČNO (bag uhvaćen profilerom -- vidi PROJECT_LOG.md): schedulerRun()
    // se MORA pozivati u petlji, ne samo jednom! main() na hartu 0 poziva
    // schedulerRequestStopAfter() VIŠE PUTA -- jednom po test bloku -- što
    // svaki put resetuje 'stopRequested' nazad na false. Da smo ovde pozvali
    // schedulerRun() samo jednom, hart 1 bi se TRAJNO parkirao u wfi-petlji
    // čim OBILAN test blok istekne (stopRequested->true), i nikad se ne bi
    // vratio da posluži niti iz NAREDNIH test blokova -- tačno ono što se i
    // desilo (workerB nikad nije dobio priliku da se izvrši, prozor [0,0]).
    // Ranije je ovo "slučajno radilo" jer je hart 0 stizao da resetuje
    // 'stopRequested' na false PRE nego što bi hart 1 uopšte primetio
    // prolaznu true vrednost -- čista trka u tajmingu, ne stvarna ispravnost.
    // Dodavanje traceRecord poziva (dodatni spinlock/disableInterrupts
    // troškovi na svaki dispatch/preempt) pomerilo je tajming taman dovoljno
    // da se trka ovog puta razreši na pogrešnu stranu.
    while (true) {
        schedulerRun();
    }
}
