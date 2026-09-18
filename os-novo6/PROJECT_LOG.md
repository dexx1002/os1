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
3. Preemptive round-robin scheduler — ✅ **gotovo**
4. Semafori — ✅ **gotovo**
5. CI pipeline (GitHub Actions) — ✅ **gotovo (kod strane); workflow fajl
   još nije okačen na GitHub — odloženo, `.github/workflows/` mora ići na
   koren repoa, ne u `os-novoN` podfolder, vidi Fazu 7)**

### Faza 5 — `os-novo3`: preemptivni scheduler (korak 3)
Novi folder na GitHub-u (`os-novo3`) — root fajlovi iz koraka 2, plus
originalni `h/`/`src/` fakultetski kod zadržan **samo informativno** (nije
korišćen, nije integrisan).

`switchInto`/`yieldToMain` iz koraka 2 (kooperativno, main ručno bira ko je
na redu) zamenjeni su pravim schedulerom:
- **`scheduler.hpp`/`scheduler.cpp`** — ready red kao intruzivna kružna
  lista preko `TCB::next` (bez ijedne dodatne alokacije za čvorove).
  `schedulerRun()` dequeue-uje nit i radi `contextSwitch` u nju;
  `schedulerTick()` (pozvan iz `handleTrap` na tajmer prekid) trenutnu nit
  vraća u red i prebacuje kontrolu nazad na scheduler petlju.
  `schedulerRequestStopAfter(N)` omogućava testiranje (zaustavi posle N
  tik-ova) umesto da petlja radi zauvek.
- **Test:** dve niti sa **beskonačnim petljama** koje nikad same ne
  ustupaju procesor — jedini način da obe napreduju je da ih tajmer
  *prinudno* smenjuje. Ako oba brojača rastu, preotimanje stvarno radi.

**Uhvaćen i ispravljen kritičan bag pre testiranja na hardveru/QEMU-u:**
`contextSwitch` ne prolazi kroz `mret` (to je "sirov" register-swap preko
callee-saved registara, ne pravi trap-return), pa **ne restaurira
`mstatus.MIE`** automatski. Kad se nit prekine usred trapa, `MIE` ostaje
"zaglavljen" na 0 (hardver ga automatski čisti pri ulasku u bilo koji trap)
sve dok se *baš ta konkretna* nit kasnije prirodno ne nastavi kroz sopstveni
`mret`. Bez eksplicitne ispravke: **druga nit koju scheduler ikad pokrene**
(prva ikad da se izvršava, dispatch-ovana odmah nakon što je neka DRUGA nit
već bila prekinuta) startovala bi sa isključenim prekidima — i pošto nijedna
od test-niti nikad sama ne ustupa procesor, nikad ne bi bila preuzeta →
**trajno zamrzavanje testa, bez ijedne poruke greške** (najgora vrsta baga
za debug). Ispravka: eksplicitno paljenje `mstatus.MIE` pre SVAKOG
dispatch-a u `schedulerRun()`, ne oslanjanje na prirodno restauriranje kroz
`mret`.

**Rezultat testa:** oba brojača (A: ~92M, B: ~1.8B) su rasla — dokaz da
preotimanje radi (A ne bi nikad sama ustupila procesor). Velika razlika u
brzini (B ~20x brža od A) najverovatnije je posledica **QEMU TCG JIT
zagrevanja** — prvi prolazak kroz blok koda se interpretira/prevodi, naredni
prolasci kroz već-prevedeni blok su brži; pošto A radi prva (hladan start)
a B profitira od već zagrejanog prevodioca za skoro identičan kod, asimetrija
ima smisla. Na pravom silikonu ovoga ne bi bilo u ovoj razmeri (hardver nema
JIT) — vredna napomena za README kao svesna razlika emulacije naspram
pravog hardvera.

### Faza 6 — `os-novo4`: semafori (korak 4)
Dodati `semaphore.hpp`/`semaphore.cpp` — prost brojački semafor sa
blokirajućim redom čekanja (`waitHead`/`waitTail`, ista intruzivna-lista
tehnika kao ready red, deli `TCB::next` polje jer nit u svakom trenutku
može biti u najviše jednom od dva reda).

**Test:** klasičan bounded-buffer producer-consumer. Producer ubacuje 10
vrednosti (1..10) u kružni bafer veličine 4 (`emptySlots`/`filledSlots`/
`mutex` semafori); consumer ih sabira. Deterministička provera: suma mora
biti tačno **55** — ako bi neka stavka bila izgubljena ili duplirana zbog
race condition-a, suma bi odstupala. Bitna osobina ovog testa: **ne zavisi
od tajmera da bi napredovao ispravno** — blokirajući semafori sami teraju
producer/consumer da se smenjuju čim se bafer napuni/isprazni; tajmer je tu
samo kao gornja granica za slučaj da nešto zapne.

**Dva bagova uhvaćena i ispravljena pre testiranja:**

1. **Kritična sekcija oko provere-i-izmene semafora.** `semWait`/`semSignal`
   proveravaju i menjaju `value` — bez zaštite, tajmer prekid bi mogao da
   upadne *tačno* između provere (`value > 0`) i dekrementa, i pusti dve
   niti da vide istu (zastarelu) vrednost, dozvoljavajući da obe "prođu"
   kroz semafor koji je trebalo da propusti samo jednu. Rešeno dodavanjem
   `disableInterrupts()`/`restoreInterrupts()` helpera u `riscv.hpp` — kratko
   isključe prekide oko provere-i-izmene, pa ih vrate u prethodno stanje
   (ne "na silu" pale ako su već bili isključeni — bezbedno za ugnježdene
   pozive).
2. **`ticksUntilStop` odbrojavanje se zaustavljalo kad nema aktivne niti.**
   `schedulerTick()` je prvobitno odbrojavao tik-ove SAMO unutar grane koja
   preuzima trenutnu nit (`if (currentTcb != nullptr)`). Dok su niti u
   koraku 3 bile beskonačne petlje (uvek je neka nit aktivna), ovo se nije
   primetilo. Ali kad producer/consumer *završe* (state → `Finished`) pre
   isteka traženih tik-ova, `currentTcb` postaje `nullptr`, odbrojavanje se
   zauvek zaustavlja, i `schedulerRun()` visi u `wfi`-petlji zauvek.
   Ispravljeno odvajanjem odbrojavanja od logike preuzimanja trenutne niti.

**Rezultat testa:** `Proizvedeno: 10 / Potroseno: 10 / Suma: 55` — tačno
očekivano, bez ijedne izgubljene ili duplirane stavke.

**Poznata neefikasnost (nije bag):** pošto `schedulerRun()` nema "prekini
čim nema više posla" logiku, program posle završetka producer/consumer
niti i dalje čeka da istekne svih 20 traženih tik-ova (do ~4s) pre nego što
nastavi dalje. Kandidat za doterivanje kasnije ako postane iritantno.

### Faza 7 — `os-novo5`: CI pipeline, poweroff mehanizam (korak 5)

**Problem pre ovog koraka:** kernel se nikad sam nije gasio (`while(true)
wfi;` na kraju `main()`-a) — u CI-ju bi to značilo da build "visi" zauvek
bez ijednog pouzdanog signala da li su testovi prošli, sem nagađanja preko
timeout-a i grep-ovanja ispisa.

**Rešenje — SiFive test/poweroff uređaj.** QEMU `virt` mašina emulira ovaj
uređaj na fiksnoj adresi `0x100000`; upis `0x5555` stvarno gasi QEMU proces
sa exit kodom `0`, upis `0x3333` sa exit kodom `1`. Dodato u `riscv.hpp`
(`RISCV::Poweroff::exitQemu`), pozvano na kraju `main()`-a na osnovu da li
su svi testovi prošli (`tests.passed == tests.total`).

**Dva efekta ove izmene:**
1. **Lokalno:** program se sam gasi posle testova — više nije potrebno
   ručno `Ctrl+A` pa `X` da se izađe iz QEMU-a. Potvrđeno testiranjem:
   `make run; echo "Exit kod: $?"` → **`Exit kod: 0`** kad su svi testovi
   prošli (11/11).
2. **CI:** `make run` sad vraća pravi exit kod, pa GitHub Actions može da
   markira build kao neuspešan ako bar jedan test padne, bez ijedne linije
   grep-ovanja izlaza -- samo standardni "exit code != 0 = fail" mehanizam.

**Potvrđen paket za CI runner:** `gcc-riscv64-unknown-elf` na Ubuntu-u je
eksplicitno build-ovan sa `--disable-libstdc++-v3 --without-newlib
--with-headers=no` -- ovo direktno potvrđuje i objašnjava zašto ovaj
toolchain od početka nema `<cstdint>`/`<cstddef>` (Faza 1), i da je to
tačno paket koji odgovara lokalnom setup-u. `qemu-system-misc` pokriva
RISC-V mete na Ubuntu 24.04 (vidi raniju analizu paketa).

**Workflow fajl je napisan** (`.github/workflows/build-and-test.yml`) ali
**još nije okačen na GitHub** -- odloženo po dogovoru. Važna napomena za
kad se okači: GitHub Actions prepoznaje workflow fajlove SAMO u
`.github/workflows/` na **korenu** repoa, ne unutar `os-novoN` podfoldera.
Workflow već ima `working-directory: os-novo5` podešeno da uđe u pravi
folder za build korake -- **tu liniju treba ažurirati** kad se pređe na
sledeći folder ili na konačan/samostalan repo.

---

## 3. Trenutno stanje koda (posle koraka 1 i 2)

### Fajlovi i odgovornosti

| Fajl | Odgovornost |
|---|---|
| `boot.S` | Boot sekvenca, postavljanje `mtvec`, `trap_vector` (SAVE_REGS/RESTORE_REGS makroi, poziva `handleTrap`) |
| `kernel.ld` | Linker script — memorijski layout, `end` simbol (početak heap-a) |
| `riscv.hpp` | RISC-V specifični detalji: builtin tipovi (bez `<cstdint>`), `enum class Csr`, generička `readCsr`/`writeCsr` template funkcija, CLINT (tajmer) registri, `enum class TrapCause`, `TrapFrame` struct sa layout proverama, `RISCV::Poweroff::exitQemu` (SiFive test uređaj) |
| `uart.cpp` | NS16550A UART drajver (`putc`/`getc`/`printString`) na QEMU `virt` adresi `0x10000000` |
| `kalloc.cpp` | Prost bump alokator (`kalloc`/`kfree`) — `kfree` je namerno no-op, pravi free-list dolazi kasnije |
| `thread.hpp` / `thread.cpp` | `Context` (14 callee-saved registara), `TCB` (sa `next` poljem za intruzivnu ready-listu), `createThread`, `threadTrampoline` |
| `scheduler.hpp` / `scheduler.cpp` | Ready red (intruzivna kružna lista), `schedulerRun`, `schedulerTick`, `schedulerAddThread`, `schedulerRequestStopAfter`, `schedulerYieldFinished`, `schedulerBlockCurrent`, `schedulerWake` — preemptivni round-robin + podrška za blokiranje |
| `semaphore.hpp` / `semaphore.cpp` | Brojački semafor (`semInit`/`semWait`/`semSignal`), blokirajući red čekanja preko `TCB::next` |
| `contextSwitch.S` | Asemblerska rutina koja menja `ra`/`sp`/`s0`-`s11` između dva `Context`-a |
| `main.cpp` | Test harness (`TestRunner`) + redom: konzola, CSR, alokator, tajmer, scheduler, semafori, trap/ecall testovi; na kraju `RISCV::Poweroff::exitQemu` umesto beskonačne petlje |
| `Makefile` | Build (riscv64 toolchain) + `make run` (QEMU) |
| `.github/workflows/build-and-test.yml` | CI: instalira toolchain, builduje, `make run` -- exit kod iz poweroff mehanizma direktno određuje prolaz/pad builda. **Napisan, još nije okačen na GitHub.** |

### Test status
Trenutno **11/11 testova prolazi** (`make clean && make run`):
- UART ispis
- CSR čitanje (`mstatus`)
- Alokator: non-null, ne-preklapanje, poravnanje, rubni slučaj (`kalloc(0)`)
- Tajmer: bar 5 CLINT prekida primljeno
- Preemptivni scheduler: dve niti sa beskonačnim petljama, oba brojača
  rastu bez ijednog dobrovoljnog ustupanja procesora
- **Semafori:** bounded-buffer producer-consumer, 10 stavki proizvedeno i
  potrošeno, suma tačno 55 (dokaz da nema izgubljenih/dupliranih stavki
  usled race condition-a)
- Trap/ecall: `mcause` tačno odgovara `EcallFromMMode`

Program se posle poslednjeg testa sam gasi (poweroff mehanizam) i vraća
exit kod `0` (potvrđeno: `make run; echo "Exit kod: $?"` → `Exit kod: 0`).

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
8. **`mstatus.MIE` se ne restaurira automatski kroz `contextSwitch`** — pošto
   `contextSwitch` nije `mret`, nego "sirov" register-swap, prekidi ostaju
   isključeni za novo-dispatch-ovanu nit ako je neka DRUGA nit ranije
   prekinuta usred trapa. Rešeno eksplicitnim paljenjem `MIE` pre svakog
   dispatch-a u `schedulerRun()`. Bez ovoga: tiho, potpuno zamrzavanje bez
   ijedne poruke greške — klasičan primer zašto asinhrone/prekidne putanje
   traže eksplicitnu, paranoičnu proveru pretpostavki o CPU stanju.
9. **QEMU TCG JIT zagrevanje kao izvor asimetrije u benchmark brojkama** —
   dve identične beskonačne petlje (niti A i B) završile su sa vrlo
   različitim brojem iteracija (B ~20x brža) iako dobijaju uporediv broj
   tajmer-slice-ova. Objašnjenje: QEMU prvi prolazak kroz blok koda
   interpretira/prevodi (sporo), naredni prolasci kroz već-prevedeni blok
   su brži — A "greje" prevodilac za obe niti (skoro identičan kod), B
   profitira od toga. Bitna napomena kad se izvode zaključci o performansama
   iz QEMU emulacije naspram pravog hardvera (koji nema JIT).
10. **Kritična sekcija oko semafora** — `semWait`/`semSignal` proveravaju
    i menjaju deljeno stanje (`value`, redovi čekanja); bez eksplicitnog
    isključivanja prekida oko te provere-i-izmene, tajmer prekid bi mogao
    da upadne tačno u sredinu i izazove race condition (dve niti "prođu"
    kroz semafor koji je trebalo da propusti samo jednu). Rešeno malim
    `disableInterrupts()`/`restoreInterrupts()` parom koji vraća prethodno
    stanje umesto da ga bezuslovno pali — bezbedno i za ugnježdene pozive.
11. **Odbrojavanje `ticksUntilStop` odvojeno od preuzimanja niti** — bag
    koji se ne bi ispoljio dok su test-niti beskonačne petlje (uvek postoji
    aktivna nit), ali postaje kritičan čim niti mogu *završiti* same od
    sebe: ako `currentTcb` postane `nullptr` pre isteka traženih tik-ova,
    odbrojavanje se zaustavlja i `schedulerRun()` visi zauvek. Dobar
    primer zašto testovi treba da pokriju i "srećan put do kraja", ne samo
    beskonačne/dugotrajne scenarije.
12. **SiFive poweroff uređaj umesto beskonačne `wfi` petlje na kraju** —
    QEMU `virt` mašina emulira ovaj uređaj na `0x100000`; upis prave
    vrednosti stvarno gasi QEMU proces sa odgovarajućim exit kodom (0/1).
    Rešava i lokalnu neugodnost (ručno gašenje preko `Ctrl+A X`) i
    omogućava CI da razlikuje prolaz/pad preko standardnog exit-kod
    mehanizma, bez grep-ovanja teksta iz ispisa.

---

## 4. Plan dalje

Osnovni plan (koraci 1-5) je **završen** otkako strana koda za korak 5
postoji (samo `.yml` okačivanje na GitHub je odloženo). Preostaje da se
odluči da li i koje "escape" ideje dalje razrađivati.

### "Escape" ideje — dalje od standardnog zadatka

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

*Poslednje ažurirano: posle koraka 5 (CI pipeline / poweroff mehanizam,
kod gotov, `.yml` okačivanje na GitHub odloženo). Osnovni plan (koraci 1-5)
završen -- sledeća odluka je koje "escape" ideje dalje razrađivati.*
