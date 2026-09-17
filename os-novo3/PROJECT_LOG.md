# os-novo — Bare-Metal RISC-V C++20 Kernel

Prateći dokument projekta: odakle je krenulo, šta je urađeno do sada, tehničke
odluke i zašto, i šta je dalje u planu. Cilj projekta: CV/portfolio komad za
prijavu na praksu "Intern Software Engineer – Low Level SW Engineering"
(Tenstorrent) — nezavisan, samostalan bare-metal RISC-V projekat koji radi
direktno na QEMU-u, bez ikakve zavisnosti od fakultetske infrastrukture.

---

## 1. Kontekst i polazna tačka

Projekat je inspirisan predmetom **Operativni sistemi 1** (zadatak: niti,
semafori, memorijski alokator, preemptive scheduler — sve statički povezano
sa `xv6`-om kao "host" sistemom koji priprema hardver i predaje kontrolu u
supervisor režimu).

**Zašto ne samo okačiti fakultetski projekat na CV:**
- U eri AI-a, "još jedan OS1 projekat" na GitHub-u nosi sve manju težinu kao
  dokaz samostalnosti.
- Fakultetski projekat zahteva staru VM sliku da bi se pokrenuo (zbog
  zaključanih `.lib` fajlova kompajliranih za stariju verziju alata).
- Deo infrastrukture (`hw.h`, `console.h`, `mem.h` + kompajlirani `.lib`
  fajlovi) je kursno vlasništvo — direktno objavljivanje "otključane" zamene
  za njih otvara pitanja akademske politike.

**Odluka:** izgraditi **potpuno nezavisnu** verziju — isti koncepti (niti,
scheduler, semafori), ali napisani iznova, bez ijedne linije/fajla iz
kursnog materijala, bez `xv6` hosta, direktno na `qemu-system-riscv64`.

---

## 2. Istorija razvoja (hronološki)

### Faza 0 — `os-novo` (osnovni bare-metal kernel)
Prvi, minimalni bare-metal kernel: boot sekvenca, UART ispis, čitanje CSR
registara, prost bump alokator, trap handling za `ecall`. Banner je tvrdio
"C++20/23 kernel", ali kod je u suštini bio C sa `namespace`-ovima — nije
stvarno koristio C++20 feature-e.

### Faza 1 — Refaktor u pravi C++20
Cilj: da "C++20" oznaka bude zaslužena, ne marketing. Izmene:
- `<cstdint>`/`<cstddef>` → **GCC/Clang builtin tipovi** (`__UINT64_TYPE__`
  itd.), jer ovaj konkretan toolchain (minimalan `riscv64-unknown-elf-g++`)
  **nema** standardne freestanding header-e. Ovo objašnjava i zašto je
  originalni (fakultetski) kod ručno definisao tipove — verovatno isti
  problem tada.
- `enum class Csr` + **template + non-type template parametar** za
  generičko čitanje/pisanje CSR-ova (`readCsr<Csr::MCause>()`) umesto
  posebne funkcije po registru.
- `enum class TrapCause` umesto sirovih brojeva (8/9/11) za uzroke `ecall`-a.
- `TrapFrame` struct sa `static_assert`-ovima koji **garantuju** da C++
  layout odgovara redosledu čuvanja registara u `boot.S` — ako neko izmeni
  jedan fajl bez drugog, build puca umesto da tiho uvede bag.
- `[[nodiscard]]` na `kalloc`/`getc` (ignorisanje povratne vrednosti je
  verovatnije bag nego namera).
- `boot.S`: `SAVE_REGS`/`RESTORE_REGS` makroi umesto ručnog ponavljanja 30+
  linija dva puta.

**Uhvaćen i ispravljen bag:** `static_assert(sizeof(TrapFrame) == 256)` je
prvobitno pretpostavio 32 sačuvana registra — `boot.S` u stvari čuva **31**
registar (248B), a `sp` pomera za 256B zbog RISC-V ABI zahteva za 16-bajtno
poravnanje steka. Ispravljeno dodavanjem eksplicitnog "padding" polja u
`TrapFrame`, umesto da se samo promeni broj u assertu — struct sada iskreno
odražava stvarnu alokaciju.

### Faza 2 — Iskreniji test harness
Originalni testovi su samo ispisivali poruke i pretpostavljali uspeh (banner
"SVI TESTOVI PROŠLI SA 100%" bez ijedne stvarne provere). Uveden
`TestRunner` koji **stvarno proverava uslove** (null-check, poravnanje,
preklapanje blokova, rubni slučajevi) i ispisuje iskren rezime `X / Y
testova prošlo`.

### Faza 3 — `os-novo2` i pokušaj integracije `h/`/`src/` (originalni OS1 kod)
Korisnik je okačio na GitHub i originalni fakultetski kod (`h/`, `src/`
folderi: `TCB`, `Scheduler`, `_sem`, `MemoryAllocator`, `contextSwitch.S`,
`riscv.cpp`...) sa idejom da se integriše u nezavisno okruženje.

**Nalazi pregleda:**
- `hw.h`/`console.h`/`mem.h` sadrže **samo deklaracije** (typedef-ovi,
  `extern` potpisi) — nema stvarne "profesorove" logike. Prava zaključana
  stvar je samo kompajlirani `.lib` (konkretne adrese, implementacije
  `__putc`/`__mem_alloc`/`plic_claim`). Ovo se dalo rešiti pisanjem
  sopstvenog `platform.hpp` sa istim imenima, ali vrednostima izvedenim iz
  sopstvenog `kernel.ld`/QEMU dokumentacije.
- **Veći problem:** ceo trap-handling kod je pisan za **supervisor (S)
  režim** (`sepc`, `sstatus`, `sret`, `scause`) jer je `xv6` prethodno radio
  delegaciju M→S moda. Naš nezavisan kernel radi isključivo u **M-modu**.
  Trebalo bi portovati S-mode CSR pozive u M-mode ekvivalente.
- Postoje dva paralelna trap fajla (`trap.S` sa eksplicitno markiranim
  "trenutno se ne koristi" kodom) — mrtav kod iz ranije faze razvoja.
- `src/main.cpp` je **nedovršen** — stvarno kreiranje niti je zakomentarisano,
  nikad end-to-end testirano u toj formi.

**Odluka (posle razmatranja opcija A/B):** korisnik je izabrao da ide **što
dalje od originalnog projekta** — ne portovati postojeći S-mode kod, nego
napisati trap/scheduler "lepak" iznova, čisto za M-mode, zadržavajući samo
*ideje* (niti, semafori, alokator) kao koncept, ne kod. Prioritet: da
projekat bude moderan i u skladu sa trenutnim tendencijama, čak i ako to
znači pisanje većine stvari ponovo.

### Faza 4 — Sveži restart: plan i gradnja iznova
Definisana savremena arhitektura (Scheduler kao apstrakcija, `TCB` sa
`enum class ThreadState`, `Semaphore` sa API-jem spremnim za priority
inheritance kasnije, syscall dispatch preko `enum class` umesto magičnih
heksova) i redosled gradnje:

1. Timer interrupt (CLINT) — ✅ **gotovo**
2. TCB + context switch (kooperativno) — ✅ **gotovo**
3. Preemptive round-robin scheduler — ⏳ sledeće
4. Semafori — ⏳ planirano
5. CI pipeline (GitHub Actions) — ⏳ planirano

---

## 3. Trenutno stanje koda (posle koraka 1 i 2)

### Fajlovi i odgovornosti

| Fajl | Odgovornost |
|---|---|
| `boot.S` | Boot sekvenca, postavljanje `mtvec`, `trap_vector` (SAVE_REGS/RESTORE_REGS makroi, poziva `handleTrap`) |
| `kernel.ld` | Linker script — memorijski layout, `end` simbol (početak heap-a) |
| `riscv.hpp` | RISC-V specifični detalji: builtin tipovi (bez `<cstdint>`), `enum class Csr`, generička `readCsr`/`writeCsr` template funkcija, CLINT (tajmer) registri, `enum class TrapCause`, `TrapFrame` struct sa layout proverama |
| `uart.cpp` | NS16550A UART drajver (`putc`/`getc`/`printString`) na QEMU `virt` adresi `0x10000000` |
| `kalloc.cpp` | Prost bump alokator (`kalloc`/`kfree`) — `kfree` je namerno no-op, pravi free-list dolazi kasnije |
| `thread.hpp` / `thread.cpp` | `Context` (14 callee-saved registara), `TCB`, `createThread`, `switchInto`, `yieldToMain`, `threadTrampoline` — kooperativni context switch |
| `contextSwitch.S` | Asemblerska rutina koja menja `ra`/`sp`/`s0`-`s11` između dva `Context`-a |
| `main.cpp` | Test harness (`TestRunner`) + redom: konzola, CSR, alokator, tajmer, niti, trap/ecall testovi |
| `Makefile` | Build (riscv64 toolchain) + `make run` (QEMU) |

### Test status
Trenutno **10/10 testova prolazi** (`make clean && make run`):
- UART ispis
- CSR čitanje (`mstatus`)
- Alokator: non-null, ne-preklapanje, poravnanje, rubni slučaj (`kalloc(0)`)
- Tajmer: bar 5 CLINT prekida primljeno
- Niti: dve niti odrade tačno 3 kruga svaka (`counter == 6`), naizmenično
  (`ABABAB` trag) — kooperativni context switch potvrđen
- Trap/ecall: `mcause` tačno odgovara `EcallFromMMode`

### Tehničke odluke vredne pomena (za README / intervju)

1. **Builtin tipovi umesto `<cstdint>`** — ovaj toolchain nema freestanding
   header-e; `__UINT64_TYPE__` i slični GCC/Clang builtin-i rade bez ijednog
   `#include`-a i tačno odgovaraju standardnim tipovima.
2. **`__builtin_offsetof` umesto `offsetof`** — iz istog razloga (nema
   `<cstddef>`).
3. **`TrapFrame` padding bag** — uhvaćen kroz `static_assert` koji je pukao
   na pogrešnoj pretpostavci (32 vs. stvarnih 31 sačuvanih registara +
   padding za 16B poravnanje). Dokaz da su assert-ovi korisni čak i kad ih
   *ti* pišeš.
4. **`++` na `volatile` je deprecated u C++20** — kompajler je upozorio;
   ispravljeno na eksplicitno čitaj-pa-piši (`timerTicks = timerTicks + 1`),
   jer standard eksplicitno obeležava inkrement/dekrement/compound-assign na
   `volatile` kao zastarelo (nisu atomski, mogu zavarati).
5. **`contextSwitch` čuva punih 14 registara** (`ra`, `sp`, `s0`-`s11`), za
   razliku od originalne (fakultetske) verzije koja je čuvala samo `ra`+`sp`.
   Originalna verzija se implicitno oslanjala na to da će pozivalac sam
   sačuvati `s0`-`s11` na svom steku pre poziva — ali RISC-V ABI to ne
   garantuje eksplicitno ("callee-saved" znači da *pozvana* funkcija ih ne
   sme izmeniti, ne da će ih *pozivalac* nužno sačuvati). Potencijalno
   latentan bag, izbegnut eksplicitnim čuvanjem svih 14.
6. **`TCB` je prost `struct` bez konstruktora** — bez `<new>` header-a nema
   garantovanog placement new-a, pa se polja popunjavaju ručno posle
   `kalloc`-a umesto kroz konstruktor.
7. **Stack poravnanje** — `kalloc` poravnava samo na 8B, a RISC-V ABI
   zahteva 16B poravnat stek; vrh novo-alociranog thread steka se dodatno
   poravnava nadole (`alignDown16`) pre upisa u `Context::sp`.

---

## 4. Plan dalje (preostali koraci)

### Korak 3 — Preemptive round-robin scheduler
Spaja tajmer prekid (korak 1) i context switch (korak 2): umesto da `main()`
ručno bira ko je na redu, tajmer prekid sam poziva scheduler koji bira
sledeću spremnu nit i radi `contextSwitch`, bez eksplicitnog zahteva niti.
Potreban ready queue (kružna lista TCB-ova).

### Korak 4 — Semafori
`wait()`/`signal()` sa blokirajućim redom po semaforu. Klasičan
producer-consumer test sa determinističkom mernom vrednošću (ne samo "izgleda
da radi").

### Korak 5 — CI pipeline (GitHub Actions)
Build + `qemu-system-riscv64` run na svaki push, sa proverom izlaza (grep na
"testova prošlo") i "build passing" bedžom u README-u.

### "Escape" ideje — dalje od standardnog zadatka (za razmatranje posle koraka 3-5)

Rangirano po tematskoj povezanosti sa ciljanim oglasom (Tenstorrent, Low
Level SW Engineering — RISC-V, dev/profiling alati):

1. **SMP (multi-hart) scheduling** — QEMU `virt` podržava `-smp N` za
   RISC-V. Per-hart ready queue, spinlock-ovi, IPI preko CLINT `msip`
   registra. Retko rađeno na predmetu; direktno relevantno za Tenstorrent
   čipove (desetine RISC-V jezgara po čipu).
2. **Profiling/tracing alat** — instrumentacija context switch/syscall/
   semafor operacija sa timestamp-ovima (`rdtime`/`rdcycle`), izvoz kao
   Gantt-dijagram. Direktno pogađa "developer tools for debugging,
   performance monitoring" iz opisa pozicije.
3. **Priority inheritance na semaforima** — rešavanje priority inversion
   problema; test scenario koji ga namerno izazove pa pokaže rešenje.
   Napredan OS koncept, redak na nivou studentskog projekta.
4. **Stack overflow detekcija** — canary vrednosti na dnu thread steka,
   provera pri context switch-u.

Preporuka (iz ranije diskusije): **SMP** kao glavni "escape" potez (najređi,
najrelevantniji za Tenstorrent), **profiling alat** kao prirodan pratilac
(treba ti *nešto* da vizuelno/merno potvrdiš da SMP scheduler radi ispravno).

---

## 5. Build instrukcije (podsetnik)

```bash
make clean && make run
```

Toolchain: `riscv64-unknown-elf-g++`/`gcc`/`ld`, `qemu-system-riscv64`
(dovoljna je Ubuntu 24.04 repo verzija 8.2.2 — RVV/CLINT funkcionalnost
korišćena ovde ne zahteva noviju).

---

## 6. Veza sa ciljanom pozicijom

Projekat cilja **"Intern Software Engineer – Low Level SW Engineering"**
(Tenstorrent) — procenjeno kao najbliži trenutnom profilu i najreleventniji
oglas od tri praćena. Direktne veze:

- *"Design and tune low-level kernels and runtime firmware that directly
  drive AI-specialized engines and RISC-V cores"* → trap handling, context
  switch, scheduler — sve na RISC-V, sve na registarskom nivou.
- *"Build and improve developer tools for debugging, performance
  monitoring"* → test harness sa stvarnim proverama; planirani
  profiling/tracing alat (escape ideja #2).
- *"Work in Linux-based, system-level environments using C++, Python, shell
  scripting, and hardware debug tools"* → GDB+QEMU debug tok, bash/Makefile
  radni ciklus.

---

*Poslednje ažurirano: posle koraka 2 (TCB + context switch), pre koraka 3
(preemptive scheduler).*
