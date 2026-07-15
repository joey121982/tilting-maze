# Generatorul + rezolvatorul de labirint (v3) și portarea pe STM32

*Scris 2026-07-07 (formatul v2, doar PC). Rescris 2026-07-11: nucleul a fost
portat pe placa STM32F103C8 din proiect, ceea ce a schimbat modelul (matrice
10×10 fără perimetru — cerință hardware), algoritmul de generare (zidire
aleatoare, nu recursive backtracker), RNG-ul (xorshift32, nu mt19937) și
formatul JSON (v3). Deciziile au fost luate cu utilizatorul la data respectivă
și sunt marcate în text. Aceleași reguli ca la restul documentației
proiectului: fără „blackbox" — fiecare valoare cu sursa ei, presupunerile
marcate explicit.*

---

## Cuprins

1. [Imaginea de ansamblu](#1-imaginea-de-ansamblu)
2. [Modelul: matricea 10×10 și pereții](#2-modelul-matricea-10×10-și-pereții)
3. [Codurile de direcție (vectorul-soluție)](#3-codurile-de-direcție-vectorul-soluție)
4. [Algoritmul de generare și de ce e altul decât în v2](#4-algoritmul-de-generare)
5. [RNG-ul: xorshift32](#5-rng-ul-xorshift32)
6. [Algoritmul de rezolvare](#6-algoritmul-de-rezolvare)
7. [Memoria: unde stă fiecare byte](#7-memoria-unde-stă-fiecare-byte)
8. [Formatul JSON v3 (și ce s-a întâmplat cu v2)](#8-formatul-json-v3)
9. [Integrarea în firmware + exportul pe UART](#9-integrarea-în-firmware)
10. [Uneltele de PC: CLI-ul și vizualizatorul](#10-uneltele-de-pc)
11. [Cum s-a verificat (și ce NU s-a putut verifica)](#11-cum-s-a-verificat)
12. [Calculul de hardware (câte cipuri TLE94112)](#12-calculul-de-hardware)
13. [Presupuneri de confirmat](#13-presupuneri-de-confirmat)

---

## 1. Imaginea de ansamblu

Sistemul fizic e un *tilting maze*: o cutie cu 10×10 = **100 de căsuțe
motorizate** — fiecare căsuță are un perete care urcă/coboară, împins de un
micro-actuator comandat prin cipuri TLE94112ES (vezi `../../EXPLICATIE_COD.md`).
În jurul celor 100 de căsuțe e **carcasa**: un cadru fix, mereu „ridicat",
nemotorizat, care ține bila înăuntru. O bilă pornește din start și trebuie să
ajungă la final; cutia se înclină, bila se rostogolește.

Noutatea din 2026-07-11: generarea + rezolvarea nu mai sunt doar o unealtă de
PC — trăiesc într-un **nucleu comun** compilat identic în ambele lumi:

```
tilting-maze/
├── common/
│   ├── maze_core.hpp        ← nucleul: generare + rezolvare + JSON
│   └── maze_core.cpp           (fără STL, fără heap, fără excepții)
├── tools/
│   ├── maze.cpp             ← ambalaj PC: argv, ceas ca seed, fișier
│   ├── build.bat
│   ├── maze_visualizer.py   ← UI (desen, animație); citește v3 + v2
│   └── maze_reference.py    ← portul de referință Python (verificare)
└── firmware/testboard/
    ├── src/drivers/uart1.cpp  ← export serial (doar TX)
    └── CMakeLists.txt         ← compilează ../../common/maze_core.cpp
```

Un singur algoritm ⇒ **același seed produce exact același labirint pe PC și pe
placă** (aceeași aritmetică pe `uint32_t`, aceeași ordine de parcurgere,
niciun element dependent de platformă în nucleu).

De ce fără STL/heap (cerință explicită): ținta e un STM32F103C8 — Cortex-M3,
**20KB RAM, 64KB flash** (sursa: `STM32F103XX_FLASH.ld`), stivă de 1KB
(`_Min_Stack_Size = 0x400` în același linker script), iar firmware-ul e
compilat cu `-fno-rtti -fno-exceptions` (sursa: `cmake/gcc-arm-none-eabi.cmake`).
Nucleul folosește doar `<stdint.h>`.

---

## 2. Modelul: matricea 10×10 și pereții

- Matricea e **fix 10×10** (`MazeCore::N = 10`), stabilită de hardware:
  100 de căsuțe motorizate (decizie utilizator, 2026-07-11). Nu există
  parametru de dimensiune la rulare; schimbarea se face din constantă +
  recompilare.
- Fiecare celulă: **`0`** = perete jos (bila poate sta acolo), **`1`** =
  perete ridicat de motor.
- **Perimetrul NU e în matrice.** În v2 era rândul/coloana 0 și N-1 din
  matrice; acum e doar fizic (carcasa). Consumatorii care desenează (de ex.
  vizualizatorul) adaugă rama singuri. Câmpul JSON `"perimeter": "fixed"`
  spune explicit asta.
- **Start = (0,0)**, **final = (9,9)** — colțuri opuse, în coordonate
  interioare (rând, coloană), convenție păstrată din v2 și confirmată de
  utilizator la 2026-07-11.

Exemplu real — ieșirea pentru **seed 42** (rulată cu portul de referință
Python, identic algoritmic cu C++-ul; `#` = zid, spațiu = liber, `S`/`F` =
start/final, `.` = soluția; rama din jur e carcasa, nu matricea):

```
# # # # # # # # # # # #
# S # # # # # # #   # #
# . #               # #
# .     # # # # #     #
# . #     # # # # # # #
# . . #         # # # #
# # . . # # #       # #
# # # . # # # # # # # #
#     . . . # # # # # #
# # # # # . # # # #   #
# # # # # . . . . . F #
# # # # # # # # # # # #
```

57 de ziduri, soluție de 18 pași, 5 fundături.

---

## 3. Codurile de direcție (vectorul-soluție)

Neschimbate față de v2 (sunt cerute de proiect în forma asta):

| Cod | Direcție | Δrând | Δcol |
|---|---|---|---|
| `0` | jos     | +1 | 0 |
| `1` | sus     | −1 | 0 |
| `2` | dreapta | 0  | +1 |
| `3` | stânga  | 0  | −1 |

Un cod per pas de **o celulă** (model per-celulă, nu „rostogolire până la
perete" — presupunere rămasă de confirmat, vezi §13). Vectorul merge la
sistemul de înclinare.

---

## 4. Algoritmul de generare

**„Zidire aleatoare cu păstrarea conectivității"** (`generate()` în
`maze_core.cpp`):

1. Toate cele 100 de celule pornesc **libere**.
2. Se amestecă ordinea celor 100 de celule (Fisher-Yates, vezi §5).
3. În ordinea amestecată, fiecare celulă (fără start și final) e **încercată
   ca zid**: se ridică, apoi un flood-fill din start verifică dacă **tot**
   spațiul liber rămas e conectat. Dacă da, zidul rămâne; dacă nu, se coboară.
4. O **singură trecere** peste cele 100 de celule, apoi rezolvare (§6).

Consecințe garantate prin construcție: start și final mereu libere; spațiul
liber conectat ⇒ **întotdeauna rezolvabil**; fără regiuni libere izolate.

### De ce nu recursive backtracker (algoritmul din v2)

Backtracker-ul clasic funcționează pe reprezentarea „culoare pe indici impari,
pereți pe indici pari + perimetru", care **cere dimensiune totală impară**. Un
labirint clasic cu `c` culoare pe latură ocupă `2c+1` celule; interiorul cerut
de hardware e 10×10 ⇒ `2c+1 = 12` nu are soluție întreagă. Pe scurt: **nu
există așezare de labirint „perfect" clasic pe 10×10**. Zidirea aleatoare nu
are astfel de constrângeri de paritate.

### De ce o singură trecere (măsurat, nu ghicit)

Dacă am repeta trecerile până nu mai poate fi zidit nimic (punct fix), orice
fundătură ar deveni zidibilă (o celulă-frunză nu e pe drumul nimănui) și s-ar
elimina; repetat, spațiul liber degenerează într-un **singur coridor**
start→final, fără ramuri false — adică nu mai e labirint. Măsurat pe 200 de
seed-uri cu portul de referință (2026-07-11):

| Treceri | Ziduri (medie) | Pași soluție (medie) | Fundături (medie / min) |
|---|---|---|---|
| 1 (ales)   | 58,2 | 20,4 | **5,1 / 2** |
| 2          | 67,0 | 20,4 | 2,7 / 0 |
| 3          | 71,6 | 20,4 | 1,6 / 0 |
| punct fix  | 78,6 | 20,4 | **0,0 / 0** ← coridor unic |

Lungimea soluției nu crește cu trecerile, dar ramurile false dispar — deci o
singură trecere e strict mai bună pentru aspectul de labirint.

Proprietate cunoscută a algoritmului: soluțiile tind să fie destul de directe
(medie ~20 de pași la un minim teoretic de 18 = distanța Manhattan 9+9;
maximul văzut pe 500 de seed-uri: 34). Dificultatea vine din ramurile false
(fundături: 2–8 pe labirint), nu din șerpuirea drumului corect.

### Costul (trade-ul cerut: RAM minim, timp acceptabil)

O generare face ≤100 de flood-fill-uri a câte O(100) celule. Numărat exact în
portul de referință: max ~30.600 de operații de bază (extrageri din coadă +
verificări de vecini) pe 100 de seed-uri. **Estimare** de ordin de mărime pe
țintă: la ~30 de cicluri/operație și 8 MHz (ceasul real din `setup.c`: HSI
fără PLL) ⇒ **~100–150 ms per generare**. Pentru un labirint generat o dată
pe joc, irelevant; în schimb nu ținem în RAM nicio structură persistentă de
conectivitate. (Cifra de cicluri/operație e presupunere marcată; de măsurat
pe placă dacă devine relevantă.)

---

## 5. RNG-ul: xorshift32

Ales de utilizator (2026-07-11) pentru RAM minim: **4 bytes de stare**, față
de ~2,5KB cât ar fi cerut `std::mt19937` din v2. Consecință asumată:
seed-urile **nu** mai produc aceleași labirinturi ca binarul v2.

- Algoritm: George Marsaglia, *„Xorshift RNGs"*, Journal of Statistical
  Software 8(14), 2003, p. 4 — tripletul `(13, 17, 5)`:
  `x ^= x<<13; x ^= x>>17; x ^= x<<5`. Perioadă 2³²−1.
- **Starea 0 e punct fix** (ar produce doar zerouri), deci `seed == 0` e
  înlocuit cu `1` — documentat și în antetul `generate()`; seed 0 și seed 1
  dau același labirint (verificat în `maze_reference.py --check`).
- Amestecarea: **Fisher-Yates** (Knuth, TAOCP vol. 2, alg. P), de la coadă
  spre început, cu indexul aleator `xorshift32() % (i+1)`. Biasul de modulo
  există dar e neglijabil și documentat: 2³² % 100 = 96 de resturi „în plus"
  la 4,3 miliarde de valori ⇒ ordinul 10⁻⁸.
- Pe placă, seed-ul poate veni de la orice sursă (`HAL_GetTick()` la o apăsare
  de buton, zgomot ADC etc.) — nucleul nu impune nimic.

---

## 6. Algoritmul de rezolvare

**BFS** din start peste celulele libere — pe un graf neponderat dă drumul cel
mai scurt. Două detalii fixate deliberat:

1. **Vecinii se parcurg mereu în ordinea codurilor** (0 jos, 1 sus, 2 dreapta,
   3 stânga), și la generare (flood-fill) și la rezolvare. Asta face BFS-ul
   determinist: la egalitate de distanță câștigă mereu aceeași direcție, deci
   C++-ul și portul Python produc **exact același drum**, nu doar aceeași
   lungime.
2. Reconstrucția drumului se face în **două treceri înapoi** de la final
   (prima numără pașii, a doua scrie codul fiecărui pas direct la poziția lui
   finală în vectorul împachetat) — ca să nu existe un buffer intermediar de
   inversare. Timp dublu pe reconstrucție, zero RAM în plus: trade-ul cerut.

Defensiv, `solvable` e verificat și raportat, deși prin construcție spațiul
liber e mereu conectat (nu s-a văzut `false` pe 500 de seed-uri).

---

## 7. Memoria: unde stă fiecare byte

Rezultatul (`MazeCore::Maze`) — **48 de bytes**, alocați de apelant (static
sau pe stivă, nucleul nu alocă nimic):

| Câmp | Reprezentare | Bytes |
|---|---|---|
| `seed` | `uint32_t` | 4 |
| `walls` | 100 celule × 1 bit, în 4×`uint32_t` | 16 |
| `path` | ≤99 pași × 2 biți (codurile 0–3 încap exact) | 25 |
| `path_len`, `solvable` | `uint8_t` fiecare | 2 |
| aliniere | — | 1 |

Citirea se face prin `cellWall(m, idx)` și `pathStep(m, i)` — nu direct pe
biți.

Memoria tranzitorie (stivă, măsurată de compilator cu `-fstack-usage`, build
Release 2026-07-11): **`generate()` = 408B în vârf** (cozile de flood-fill/BFS
și permutarea, cu funcțiile interne inline), `writeJson()` ≈ 90B cu tot cu
conversia numerelor. Stiva firmware-ului e 1KB ⇒ apelul din `main` încape
lejer; de evitat apelul din contexte adânci sau din întreruperi.

Măsurători pe binarul real (arm-none-eabi-gcc 14.2, `-Os`, 2026-07-11):

| Ce | Flash | RAM statică |
|---|---|---|
| `maze_core.cpp.obj` (tot nucleul) | 1567B | 0 |
| `uart1.cpp.obj` | 114B | 0 |
| firmware întreg, fără apel de maze (linkerul elimină nucleul: `--gc-sections`) | 4292B (6,55%) | 1584B (7,73%) |
| firmware întreg + exemplul din §9 activ | **4672B (7,13%)** | **1632B (7,97%)** |

---

## 8. Formatul JSON v3

```json
{
  "format": "tilting-maze-v3",
  "size": 10,                       // latura interiorului (fix 10)
  "seed": 42,                       // seedul EFECTIV (0 devine 1, vezi §5)
  "start": [0, 0],                  // [rând, coloană], coordonate interioare
  "goal": [9, 9],
  "solvable": true,
  "perimeter": "fixed",             // carcasa: fizică, mereu ridicată, NU e în matrix
  "matrix": [[0,1,...], ...],       // 10×10, DOAR interiorul; 1 = perete motorizat
  "solution": [0,0,2,...],          // cod/pas: 0 jos, 1 sus, 2 dreapta, 3 stânga
  "stats": {
    "walls": 57,                    // câți 1 în matrice (toți sunt motorizați)
    "free": 43,
    "path_length": 18,
    "tle94112_chips_needed": 19     // ceil(walls/3), vezi §12
  }
}
```

Diferențe față de v2: matricea nu mai conține perimetrul; `size` e interiorul
(10), nu totalul; a dispărut `total_walls`/`interior_walls` (acum toate
zidurile sunt interioare — câmpul unic `walls`); a apărut `perimeter`;
start/goal sunt în coordonate interioare. **Fișierele v2 rămân valabile
istoric**: vizualizatorul le deschide în continuare (le recunoaște după câmpul
`format`).

Serializarea în C++ (`writeJson`) scrie caracter cu caracter printr-un
callback `void (*)(char, void*)` — același cod produce fișierul pe PC și
fluxul UART pe placă, fără `sprintf` (varianta newlib ar fi tras câțiva KB de
flash) și fără buffer.

---

## 9. Integrarea în firmware

Decizie utilizator (2026-07-11): **modulul + documentație + export UART, fără
demo în `main.cpp`** — `main.cpp` al testboard-ului de motoare a rămas neatins.
Ce s-a adăugat:

- `CMakeLists.txt`: `../../common/maze_core.cpp` la surse, `../../common` la
  include-uri, `Src/drivers/uart1.cpp`.
- Driverul `Uart1` (`inc/drivers/uart1.hpp` + `src/drivers/uart1.cpp`), în
  stilul `Spi1`: la nivel de registru, fără HAL în calea de date.

### UART-ul, valoare cu valoare

- **USART1, PA9 = TX, doar transmisie** (decizie utilizator 2026-07-11; pe
  schema testboard-ului PA9/PA10 nu sunt folosiți de altă funcție — sursa:
  etichetele din `pcb/testboard/testboard.kicad_sch`). PA10 (RX) nu e atins.
- **115200 baud, 8N1**: PCLK2 = 8 MHz (HSI fără PLL, `setup.c`) ⇒
  `USARTDIV = 8·10⁶/(16·115200) = 4,34` ⇒ mantisă 4, fracție `0,34·16 ≈ 5` ⇒
  **BRR = 0x45**; baud real 8·10⁶/69 = 115.942 ⇒ **eroare +0,64%** (formula:
  RM0008 §27.3.4). Plus toleranța HSI (~±1% la temperatura camerei, DS5319) —
  sub pragul uzual de ~2% al UART-ului la temperatură normală. Dacă placa va
  lucra la temperaturi extreme, de coborât la 9600 (BRR = 0x341, eroare
  ~0,02%).
- PA9 e configurat de driver (CRH nibble 0xA: ieșire 2 MHz, AF push-pull —
  RM0008 §9.2.2), deci `setup.c`/`.ioc` nu au fost modificate.

### Exemplu de utilizare (verificat: compilat și linkuit pe ARM la 2026-07-11)

```cpp
#include "maze_core.hpp"
#include "drivers/uart1.hpp"

/* ... in main(), dupa HAL_Init/SystemClock_Config/MX_GPIO_Init: */
Uart1::init();

static MazeCore::Maze labirint;              /* 48B, in .bss */
MazeCore::generate(labirint, HAL_GetTick()); /* sau un seed fix, reproductibil */

MazeCore::writeJson(labirint, Uart1::put, nullptr);  /* ~1,5KB de JSON */
Uart1::flush();

/* consumul direct, fara JSON: */
if (labirint.solvable) {
    for (int i = 0; i < labirint.path_len; ++i) {
        int cod = MazeCore::pathStep(labirint, i);   /* 0..3 -> inclinare */
        (void)cod;
    }
}
for (int idx = 0; idx < MazeCore::CELLS; ++idx) {
    bool ridicat = MazeCore::cellWall(labirint, idx); /* -> motorul casutei idx */
    (void)ridicat;
}
```

La 115200, dump-ul de ~1,5KB durează ~0,13s. Pe PC se citește cu orice
terminal serial (115200 8N1) și se salvează într-un `.json` pe care
vizualizatorul îl deschide direct.

### Cum se compilează firmware-ul (toolchain instalat 2026-07-11)

Pe mașina asta s-au instalat prin winget: **arm-none-eabi-gcc 14.2**
(`Arm.GnuArmEmbeddedToolchain`), **CMake** și **Ninja** (nu sunt în PATH-ul
implicit al shell-urilor deja deschise):

```bat
set PATH=C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin;C:\Program Files\CMake\bin;%LOCALAPPDATA%\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe;%PATH%
cd tilting-maze\firmware\testboard
cmake -G Ninja -B build/arm-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake
ninja -C build/arm-release
```

Rezultatul e `build/arm-release/testboard.elf` (+ raport de memorie la link,
`-Wl,--print-memory-usage`).

---

## 10. Uneltele de PC

- **`tools/maze.cpp`** — doar ambalaj: argumente (`--seed S`, `--out f.json`,
  `--help`), seed implicit din ceas, scriere în fișier prin același
  `writeJson`. `--size` a dispărut (dimensiunea e a hardware-ului); dacă e
  cerut, explică și iese cu eroare.
- **`tools/build.bat`** — compilează `maze.cpp + ..\common\maze_core.cpp`
  (g++ → clang++ → cl, C++14). Pe mașina asta încă nu există compilator C++
  de *host* (doar cel de ARM), deci binarul de PC nu a fost construit local —
  vezi §11.
- **`tools/maze_visualizer.py`** — desenează v3 (matricea interioară + rama
  de carcasă în nuanță diferită, fiindcă nu e motorizată) și v2 (istoric,
  matricea conține perimetrul). Spinbox-ul de dimensiune a dispărut (10×10
  fix). `Generate` rulează binarul C++ fără `--size`; `Open JSON…` merge și
  fără binar.
- **`tools/maze_reference.py`** — portul de referință (vezi §11):
  `--check [N]` (proprietăți pe N seed-uri), `--json SEED [fișier]`,
  `--show SEED` (ASCII).

---

## 11. Cum s-a verificat (și ce NU s-a putut verifica)

Metodologia moștenită de la v2: pe mașina de dezvoltare nu există compilator
C++ de host, deci logica se validează printr-un **port de referință Python**
(`maze_reference.py`), scris linie-cu-linie după `maze_core.cpp` (aceeași
aritmetică pe 32 de biți, aceeași ordine a vecinilor, același Fisher-Yates).

Verificat la 2026-07-11:

1. **`maze_reference.py --check 500`** — pe seed-urile 1..500: spațiul liber
   conectat; start/final libere; soluția există, e validă pas cu pas (nu iese
   din grilă, nu intră în zid, celule distincte) și atinge finalul;
   reproducibil (două generări identice); `seed 0 ≡ seed 1`. Statistici:
   ziduri 50–67 (medie 58,3), soluție 18–34 pași (medie 20,2), fundături 2–8.
2. **Comparația 1/2/3/punct-fix treceri** (tabelul din §4) — a confirmat
   empiric alegerea unei singure treceri.
3. **Vizualizatorul, headless** — încărcarea unui v3 generat de referință
   (rama adăugată corect, offseturile start/final/soluție +1) și a unui v2
   sintetic (desenat ca atare), fără regresii.
4. **Build ARM real**: firmware-ul întreg compilează fără avertismente noi și
   linkuiește; în build-ul normal linkerul elimină nucleul (nimic nu-l cheamă
   încă — corect); un link de verificare cu exemplul din §9 în loc de `main`
   a dovedit că totul se leagă și a dat cifrele de memorie din §7.

**Limitări asumate:** (a) echivalența C++ ↔ Python e stabilită prin
construcție și citire cap-la-cap, nu prin rulare comparată — C++-ul nu poate
fi *rulat* pe mașina asta până nu apare un compilator de host (sau placa +
programator, pentru rulare pe țintă); primul lucru de făcut atunci:
`maze.exe --seed 42` vs `maze_reference.py --json 42` — trebuie să iasă
byte-identic pe câmpurile de conținut. (b) UART-ul e scris după RM0008 și
verificat doar prin compilare — nu a fost văzut pe osciloscop/terminal.

---

## 12. Calculul de hardware

Un TLE94112ES comandă **3 motoare** (12 semi-punți ÷ 4/motor — vezi
`../../EXPLICATIE_COD.md` §3). Fiecare perete ridicabil = un motor:

```
tle94112_chips_needed = ceil(walls / 3)
```

Acum că perimetrul nu mai e în matrice, ambiguitatea din v2
(`interior_walls` vs `total_walls`) a dispărut: **toate** zidurile din matrice
sunt motorizate, iar presupunerea „carcasa e cadru fix, nemotorizat" a fost
**confirmată de utilizator la 2026-07-11** („fără pereții exteriori, care sunt
mereu ridicați"). Pe 500 de seed-uri: 17–23 de cipuri (medie ~20). Numărul e
informativ (dimensionare), nu intră în firmware.

---

## 13. Presupuneri de confirmat

- **Model de înclinare per-celulă** (un cod = o înclinare scurtă care mută
  bila exact o celulă) — moștenit din v2, încă neconfirmat pe mecanica reală.
  Dacă fizic bila se rostogolește până lovește un perete, vectorul-soluție
  trebuie recalculat pe alt model (comprimat pe segmente).
- **~30 de cicluri per operație de flood-fill** în estimarea de ~100–150 ms
  per generare la 8 MHz (§4) — ordin de mărime; de măsurat pe placă dacă
  timpul contează.
- **Toleranța HSI la temperatura de lucru reală** — 115200 e în regulă la
  temperatura camerei; la extreme, de trecut pe 9600 (§9).
- Unde se leagă fizic adaptorul serial (PA9 + GND) și dacă pe placa finală
  (daughterboard) PA9 rămâne liber — de verificat pe schema aceleia când se
  integrează.
