#ifndef SMP_HPP
#define SMP_HPP

// Sekundarni hart(ovi) čekaju ovde dok ih hart 0 ne pusti da se pridruže
// scheduleru -- izbegavamo trku oko globalne inicijalizacije (heap, ready
// red...) koju hart 0 radi sam, jednonitno, pre nego što ijedan drugi hart
// sme da dirne deljeno stanje.
void smpWaitForStart();
void smpSignalStart();

// Ulazna C++ tačka za svaki hart OSIM hart-a 0 -- poziva je boot.S direktno
// posle postavljanja sopstvenog steka/mtvec-a za taj hart.
extern "C" [[noreturn]] void secondaryHartMain();

#endif // SMP_HPP
