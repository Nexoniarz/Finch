# Finch dla techników

**Dla osób, które dobrze znają komputer, ale niewiele albo wcale nie programowały.**
Ten przewodnik opisuje instalację Fincha, polecenie `finch`, cały język, pamięć, moduły, biblioteki C, debugowanie
i rozwiązywanie typowych problemów. Nie opisuje, jak kompilator działa od środka; o tym jest
[przewodnik dla inżynierów](dla-inzynierow.md).

> 🇬🇧 English version: [for-technicians.md](../en/for-technicians.md)

---

## Spis treści

1. [Czym jest Finch](#1-czym-jest-finch)
2. [Instalacja](#2-instalacja)
3. [Polecenie finch](#3-polecenie-finch)
4. [Jak wygląda plik Fincha](#4-jak-wygląda-plik-fincha)
5. [Zmienne](#5-zmienne)
6. [Typy](#6-typy)
7. [Operatory](#7-operatory)
8. [Sterowanie](#8-sterowanie)
9. [Funkcje](#9-funkcje)
10. [Tablice i mapy](#10-tablice-i-mapy)
11. [Tekst (str)](#11-tekst-str)
12. [Struktury i metody](#12-struktury-i-metody)
13. [Pamięć: kto co zwalnia](#13-pamięć-kto-co-zwalnia)
14. [Wskaźniki](#14-wskaźniki)
15. [Funkcje wbudowane](#15-funkcje-wbudowane)
16. [Moduły](#16-moduły)
17. [Biblioteki C](#17-biblioteki-c)
18. [Błędy](#18-błędy)
19. [Edytory: VS Code, Kate i inne](#19-edytory-vs-code-kate-i-inne)
20. [Debugowanie](#20-debugowanie)
21. [Rozwiązywanie problemów](#21-rozwiązywanie-problemów)
22. [Układ repozytorium i testy](#22-układ-repozytorium-i-testy)
23. [Enumy i match](#23-enumy-i-match)
24. [Typy generyczne](#24-typy-generyczne)
25. [Biblioteka standardowa](#25-biblioteka-standardowa)
26. [Obecne ograniczenia](#26-obecne-ograniczenia)
27. [Testy: bloki test i finch test](#27-testy-bloki-test-i-finch-test)
28. [Projekt: finch.toml](#28-projekt-finchtoml)
29. [Wątki](#29-wątki)
30. [C++, Rust i Android](#30-c-rust-i-android)
31. [Wektory (SIMD)](#31-wektory-simd)

---

## 1. Czym jest Finch

Finch to **kompilowany** język programowania. Polecenie `finch` tłumaczy plik `.fch` na prawdziwy program (natywny plik
wykonywalny, taki jak programy napisane w C) za pomocą **LLVM**, tego samego zaplecza, z którego korzystają clang,
Rust i Swift.

Co z tego wynika:

- **Szybkość bliska C.** Nie ma interpretera, maszyny wirtualnej ani garbage collectora. Sprawdzanie zakresów tablic
  i inne zabezpieczenia po optymalizacji prawie nic nie kosztują.
- **Małe, samodzielne programy**, które można skopiować i uruchomić.
- **Pamięć zarządza się sama.** Listy i teksty są zwalniane automatycznie na końcu bloku, do którego należą,
  bez udziału garbage collectora.
- **Bezpośredni dostęp do bibliotek C.** `import "stdio.h"` i możesz wołać wszystko, co tam jest, nawet funkcje
  przyjmujące struktury.

Zasady projektu: na każdą rzecz jeden sposób, składnia jak w C, ale bez jego pułapek, i komunikaty błędów, które mówią,
jak naprawić problem.

---

## 2. Instalacja

Kompilator Fincha jest napisany w Finchu (zob. [przewodnik dla inżynierów](dla-inzynierow.md)). Działa na **Linuksie**
(x86-64 i ARM64), **macOS** (Apple Silicon i Intel) i **Windowsie** (x86-64) i na każdy z tych systemów buduje programy.
Wszędzie potrzebuje **clanga** (kompiluje nim każdy program) i **libclang** z LLVM 21 (czyta nim nagłówki C).

### Sposób A: gotowa wersja

Pobierz archiwum dla swojego systemu z [wydań](https://github.com/Nexoniarz/Finch/releases) i rozpakuj je.
Katalogi `runtime/` i `std/` muszą leżeć obok pliku `finch` (albo zainstaluj go, zob. niżej).

| System | Archiwum | Doinstaluj |
|---|---|---|
| Linux x86-64 | `finch-linux-x64.tar.gz` | clang 21 i libclang (tabela niżej) |
| Linux ARM64 (Raspberry Pi 4/5, Graviton…) | `finch-linux-arm64.tar.gz` | to samo |
| macOS, Apple Silicon | `finch-macos-arm64.tar.gz` | `xcode-select --install` (clang i nagłówki systemu), `brew install llvm@21` (libclang) |
| macOS, Intel | `finch-macos-x64.tar.gz` | to samo |
| Windows x64 | `finch-windows-x64.zip` (`finch.exe` z `libclang.dll`) | [llvm-mingw](https://github.com/mstorsjo/llvm-mingw/releases) (archiwum `ucrt-x86_64`): rozpakuj i dodaj jego katalog `bin` do PATH |

Na Linuksie:

| Dystrybucja | Pakiety |
|---|---|
| Debian / Ubuntu | `clang-21 libclang-21-dev` z [apt.llvm.org](https://apt.llvm.org), `pkg-config` |
| Fedora | `clang clang-devel pkgconf` |
| Arch | `clang pkgconf` |
| Nix | `nix-shell` w repozytorium daje wszystko naraz |

Na Windowsie llvm-mingw daje Finchowi w jednym katalogu `clang`, nagłówki C i linker; zbudowane nim programy nie
potrzebują do działania niczego więcej. Plik `libclang.dll` trzymaj obok `finch.exe`.

### Sposób B: zbuduj sam

```sh
git clone https://github.com/Nexoniarz/Finch.git
cd Finch
nix-shell                        # Linux: clang, libclang, pkg-config (albo zainstaluj je z dystrybucji)
bootstrap/build.sh               # build/finch
./build/finch version
```

`bootstrap/build.sh` kompiluje plik startowy (`bootstrap/<system>.ll`, własny LLVM IR kompilatora) w pierwszy kompilator,
buduje nim `src/`, buduje `src/` jeszcze raz otrzymanym kompilatorem i sprawdza, czy oba wyniki są identyczne. Jeśli
linker nie znajduje libclang, wskaż ją: `FINCH_LIBCLANG="-L/usr/lib/llvm-21/lib -lclang-21" bootstrap/build.sh`.

Na Macu, z LLVM z Homebrew:

```sh
brew install llvm@21
export FINCH_CLANG="$(brew --prefix llvm@21)/bin/clang"
export FINCH_LIBCLANG="-L$(brew --prefix llvm@21)/lib -lclang"
export FINCH_LLVM_INCLUDE="$(brew --prefix llvm@21)/include"
bootstrap/build.sh
```

Na Windowsie, w Git Bash, z llvm-mingw w PATH i oficjalnym LLVM dla Windowsa (dla `libclang.dll` i jej nagłówków)
rozpakowanym do `C:/llvm`:

```sh
export FINCH_LIBCLANG="C:/llvm/bin/libclang.dll"
export FINCH_LLVM_INCLUDE="C:/llvm/include"
export FINCH_LDFLAGS="-LC:/llvm/bin"
bootstrap/build.sh               # build/finch.exe, z libclang.dll skopiowaną obok
```

### Budowanie na inny system

`--target` buduje program na inny system. Do linkowania Finch potrzebuje kompilatora C tamtego systemu:

| `--target` | Buduje | Używany kompilator C | `finch run` uruchamia przez |
|---|---|---|---|
| `windows` | plik `.exe` dla Windowsa | `x86_64-w64-mingw32-gcc` (MinGW-w64) | Wine |
| `arm64` | program dla Linuksa ARM64 | `aarch64-linux-gnu-gcc` | `qemu-aarch64` |
| `linux` | program dla Linuksa x86-64 | `clang --target=…` | |
| `macos` | program dla macOS (na Macu: dla drugiego rodzaju procesora) | `clang --target=…` | |
| dowolny triple LLVM | np. `aarch64-unknown-linux-gnu` | `clang --target=<triple>` | |

```sh
finch build gra.fch --target windows       # gra.exe
finch run gra.fch --target arm64           # buduje dla ARM64 i uruchamia przez QEMU
FINCH_CC=aarch64-unknown-linux-gnu-gcc finch build narzedzie.fch --target arm64   # inna nazwa kompilatora
```

Na Niksie te kompilatory to `pkgsCross.mingwW64.buildPackages.gcc` i
`pkgsCross.aarch64-multiplatform.buildPackages.gcc`.

### Dodanie `finch` do PATH (opcjonalnie)

```sh
sudo bootstrap/install.sh          # /usr/local/bin/finch oraz /usr/local/share/finch/{runtime,std}
bootstrap/install.sh ~/.local      # albo do własnego katalogu
finch version
```

Przy pracy Finch potrzebuje `clanga` (kompiluje nim IR każdego programu i linkuje go). Mała biblioteka runtime
jest budowana raz i trzymana w `~/.cache/finch`. `FINCH_CC` wskazuje inny kompilator do linkowania; `FINCH_PATH`
dodaje katalogi, w których Finch szuka modułów.

---

## 3. Polecenie finch

```
finch run   <plik.fch> [argumenty...]   kompiluje i od razu uruchamia
finch build <plik.fch> [-o nazwa]       kompiluje do programu (domyślna nazwa: nazwa pliku)
finch test  <plik.fch>                  uruchamia jego bloki test "..." { } (rozdział 27)
finch fmt   [--check] [pliki/katalogi]  porządkuje wcięcia plików .fch (domyślnie wszystkich w bieżącym katalogu)
finch ir    <plik.fch>                  wypisuje LLVM IR (dla ciekawskich)
finch lsp                               serwer języka, dla edytorów
finch version                           pokazuje wersję

opcje:
  -l <bib>          dolinkuj bibliotekę C, to samo co  link "bib"  w pliku
  -I <katalog>      szukaj nagłówków C także tam
  --target <nazwa>  zbuduj na inny system: windows, linux, arm64, macos, android albo triple LLVM
  --cpu <nazwa>     użyj wszystkich instrukcji procesora: native (tego), x86-64-v3 (AVX2),
                    x86-64-v4 (AVX-512), apple-m1, ... (rozdział 31)
  -g                dodaj informacje dla debuggera (gdb i lldb)
  -O0               bez optymalizacji
  --trace           błędy w trakcie działania pokazują ścieżkę wywołań (i tak włączone dla run, test, -g i -O0)
  --no-trace        nie pokazują (zwykłe finch build ich nie pokazuje: pamiętanie ścieżki spowalnia wywołania)
```

W katalogu z plikiem `finch.toml` (rozdział 28) `finch run`, `finch build` i `finch test` nie potrzebują nazwy pliku.

Skompilowany program i zbudowane przez niego pliki C (`link "x.c"`) trafiają do pamięci podręcznej (`~/.cache/finch`,
na Windowsie `%LOCALAPPDATA%`). Druga kompilacja tego samego kodu, albo kodu, którego pliki C się nie zmieniły, pomija tę pracę.

`finch fmt` zmienia tylko układ: cztery spacje na każdy poziom `{ }`, żadnych spacji na końcach linii, najwyżej jedna
pusta linia z rzędu. Linie kontynuujące wyrażenie (w nawiasach `( )` albo po `+`) i tekst `"""` zachowują swój kształt.
`finch fmt --check` niczego nie zmienia i kończy się błędem, jeśli jakiś plik by się zmienił (przydatne w CI).
Edytory z serwerem języka formatują na żądanie.

Przykłady:

```sh
finch run hello.fch                 # wypisuje wynik
finch run narzedzie.fch dane.txt -v # argumenty po nazwie pliku trafiają do programu
finch build gra.fch -o mojagra      # tworzy ./mojagra
finch build gra.fch -g -O0          # wersja do debuggera
```

`finch run` przekazuje kod wyjścia twojego programu. Jeśli program się wysypie, mówi jak
(np. `the program crashed: Segmentation fault`).

---

## 4. Jak wygląda plik Fincha

```c
// komentarz zaczyna się od dwóch ukośników
/* albo zajmuje
   kilka linii */

import "math.h"          // nagłówki C (opcjonalnie), na początku pliku
import ksztalty          // moduły Fincha (opcjonalnie): ksztalty.fch obok tego pliku
link "m"                 // biblioteki albo pliki C do dolinkowania (opcjonalnie)

struct Punkt {           // twoje własne typy
    int x
    int y
}

fn main() {              // tu zaczyna się program
    print("Cześć")
}
```

Zasady, które warto znać:

- **Jedna instrukcja w linii.** Nie ma średników. Długie wyrażenie można złamać **po** operatorze (`a +⏎ b`)
  albo w dowolnym miejscu w nawiasach `( )` lub `[ ]`.
- **Klamry są zawsze obowiązkowe** po `if`, `else`, `for`, `while`, a otwierająca `{` stoi w tej samej linii.
- **Warunki bez nawiasów:** `if x > 5 {`, a nie `if (x > 5) {`.
- Funkcje i struktury mogą stać w pliku w dowolnej kolejności.
- `main` to `fn main()`, `fn main() -> int` (zwraca kod wyjścia) albo `fn main([]str args)`
  (dostaje argumenty z wiersza poleceń; `args[0]` to ścieżka samego programu).

---

## 5. Zmienne

```c
x := 5               // nowa zmienna, typ wynika z wartości (int)
int y = 10           // nowa zmienna z podanym typem
int z                // bez wartości: zero ("" dla str, [] dla tablic, null dla wskaźników)
Punkt p              // struktura bez wartości: zera albo wartości domyślne struktury

x = 7                // przypisanie
x += 1               // także -= *= /= %=
```

- Nazwę można zadeklarować w funkcji **tylko raz**. Finch nie pozwala użyć tej samej nazwy w bloku wewnętrznym
  (nie ma „przesłaniania”), bo to częste źródło pomyłek.
- Zmienna istnieje od deklaracji do końca bloku `{ }`, w którym stoi.
- Nazwy: litery ASCII, cyfry i `_`, nie mogą zaczynać się od cyfry. Polskie litery wolno pisać tylko w tekstach.
  Nazwy typów (`int`, `u8`, …), funkcji wbudowanych i słowa kluczowe są zarezerwowane.

**Słowa kluczowe:** `fn return if else while for in break continue true false null import link struct enum match defer or try const export`

### Stałe

```c
const SZEROKOSC = 640
const WYSOKOSC = SZEROKOSC / 2    // z innych stałych
const f32 GRAWITACJA = 9.8        // z typem
const TYTUL = "Moja gra"
```

Stałą zapisuje się na najwyższym poziomie pliku, obok funkcji, i nie da się jej zmienić. Składa się z liczb, tekstów,
`true`/`false`, znaków, innych stałych i operatorów; stała z modułu to np. `ksztalty.BOKI`.

---

## 6. Typy

### Typy na co dzień

| Typ     | Przechowuje | Przykład |
|---------|-------------|----------|
| `int`   | liczbę całkowitą, 64 bity (to samo co `i64`) | `42`, `-7`, `0xFF`, `1_000_000` |
| `float` | liczbę zmiennoprzecinkową, 64 bity (to samo co `f64`) | `3.14` |
| `bool`  | `true` albo `false` | |
| `char`  | jeden znak wielkości bajtu | `'A'`, `'\n'` |
| `str`   | tekst | `"cześć\tświecie"` |
| `[]T`   | listę (tablicę) wartości typu `T` | `[1, 2, 3]` |
| `map[K]V` | wartości typu `V` wyszukiwane po kluczu typu `K` | `["a": 1, "b": 2]` |
| twoje struktury | nazwane grupy wartości | `Punkt(1, 2)` |

Znaki specjalne w tekstach i znakach: `\n` nowa linia, `\t` tabulator, `\r`, `\0`, `\\`, `\"`, `\'`.

### Typy o podanym rozmiarze (do plików, sprzętu, bibliotek C)

| Ze znakiem | Zakres | Bez znaku | Zakres |
|---|---|---|---|
| `i8`  | −128 … 127 | `u8`  | 0 … 255 |
| `i16` | −32 768 … 32 767 | `u16` | 0 … 65 535 |
| `i32` | ok. ±2,1 mld | `u32` | 0 … ok. 4,29 mld |
| `i64` | ok. ±9,2 × 10¹⁸ | `u64` | 0 … ok. 1,8 × 10¹⁹ |

`f32` to 32-bitowa liczba zmiennoprzecinkowa (ok. 7 cyfr znaczących), `f64` to 64-bitowa (ok. 15–16).

Są też **wektory**, czyli kilka liczb obok siebie liczonych naraz: `f32x8`, `i8x32` i podobne (rozdział 31).

### Zamiana typów

Finch zamienia typy sam **tylko wtedy, gdy nic nie może zginąć**:

| Z | Na | Automatycznie? |
|---|---|---|
| mniejsza liczba całkowita | większa o tym samym znaku (`i8`→`i32`, `u8`→`u64`) | ✅ |
| bez znaku | większa ze znakiem (`u8`→`i16`, `u32`→`i64`) | ✅ |
| dowolna liczba całkowita | `float` / `f32` | ✅ |
| `f32` | `f64` | ✅ |
| `null` | dowolny wskaźnik | ✅ |
| dowolny wskaźnik | `ptr` i z `ptr` na dowolny wskaźnik | ✅ |
| wszystko inne | | ❌ trzeba napisać: `typ(wartosc)` |

**Liczby wpisane w kod dopasowują się.** Liczba zapisana w kodzie pasuje do każdego typu, który ją pomieści:
`u8 b = 200` działa, `u8 b = 300` to błąd. Tak samo jest ze stałymi z nagłówków C.

**Jawne zamiany** zapisuje się nazwą typu, jak wywołanie funkcji:

```c
int(3.99)        // 3     (ucina część ułamkową)
float(7) / 2     // 3.5
u8(300)          // 44    (zostaje 8 najniższych bitów)
int('A')         // 65
char(66)         // 'B'
bool(0)          // false (każda liczba różna od zera to true)
str(42)          // "42"  (także liczby zmiennoprzecinkowe, bool i znaki; str([]u8) zamienia bajty na tekst)
int("42")        // 42    (zatrzymuje program, jeśli tekst nie jest liczbą)
float("2.5")     // 2.5
ptr(p)           // wskaźnik z typem jako wskaźnik bez typu
ptr(16)          // liczba jako wskaźnik (API C, które przekazują przesunięcia jako wskaźniki, np. OpenGL)
int(p)           // wskaźnik jako liczba (jego adres)
str(p)           // wskaźnik na znaki z C jako tekst
```

**Liczby o stałym rozmiarze się zawijają**, jak w prawdziwym sprzęcie: `u8` równe 255 plus 1 daje 0.
Obliczenia odbywają się w typie użytych wartości, więc `u8(250) + 10` to `4`, a nie `260`.

### Liczby zmiennoprzecinkowe: co jest gwarantowane

`f32` i `f64` to liczby w standardzie IEEE 754, a Finch liczy je **dokładnie tak, jak są zapisane**, tak samo na każdym
procesorze i z każdym `--cpu`:

- Każde działanie zaokrągla raz, w zapisanej kolejności. `a * b - c * d` to dwa mnożenia i odejmowanie, nigdy nie
  złożone w jedno „mnożenie z dodawaniem” (FMA); kompilatory C często robią to domyślnie, co zmienia ostatnie cyfry.
- Nic nie jest przestawiane ani upraszczane: `(a + b) + c` nie zamienia się w `a + (b + c)`, a `x * 0.0` nie jest
  z góry uznawane za 0 (gdy `x` to NaN, wynik też jest NaN). Jedyny wyjątek to `v.sum()` na wektorze, które dodaje
  elementy w najszybszej kolejności.

Gdy szybkość jest ważniejsza niż ostatni bit, poproś o nią wprost:

| | |
|---|---|
| `fma(a, b, c)` | `a * b + c` z jednym zaokrągleniem, zawsze. Jedna instrukcja tam, gdzie procesor ma FMA (z `--cpu`, na ARM64); gdzie indziej wolniejsza funkcja biblioteczna. |
| `muladd(a, b, c)` | `a * b + c` najszybciej, jak się da na tym procesorze: jedną instrukcją, gdzie można, a gdzie nie, mnożeniem i dodawaniem. |

Obie działają na liczbach i na wektorach (`v.fma(b, c)`, `v.muladd(b, c)`).

**Liczby jako bity.** Formaty plików i wagi modeli zapisują liczby jako surowe bity. Te funkcje patrzą na bity bez
przeliczania wartości (wszystkie działają też na wektorach, element po elemencie):

| | |
|---|---|
| `f32_bits(x)` / `f32_from_bits(u)` | `f32` ↔ `u32` z tymi samymi bitami (`f32_bits(1.0)` to `0x3f800000`) |
| `f64_bits(x)` / `f64_from_bits(u)` | `f64` ↔ `u64` |
| `f16_to_f32(h)` / `f32_to_f16(x)` | liczba połówkowej precyzji, trzymana w `u16`, ↔ `f32` (zaokrąglenie do najbliższej) |
| `bf16_to_f32(b)` / `f32_to_bf16(x)` | bfloat16, trzymany w `u16`, ↔ `f32` (zaokrąglenie do najbliższej, remisy do parzystej) |

```c
[]u16 wagi = wczytaj_wagi()                  // liczby f16 z pliku
v := f16_to_f32(u16x8(wagi, 0))              // osiem z nich jako f32, naraz
```

---

## 7. Operatory

### Według priorytetu (wyższy wiąże mocniej)

| Priorytet | Operatory | Znaczenie |
|---|---|---|
| 5 | `*` `/` `%` `<<` `>>` `&` | mnożenie, dzielenie, reszta, przesunięcie w lewo/prawo, bitowe AND |
| 4 | `+` `-` `\|` `^` | dodawanie (i łączenie tekstów), odejmowanie, bitowe OR, bitowe XOR |
| 3 | `==` `!=` `<` `<=` `>` `>=` | porównania |
| 2 | `&&` | logiczne AND |
| 1 | `\|\|` | logiczne OR |

Jednoargumentowe (przed wartością): `-x` zmiana znaku, `!x` logiczne NOT, `~x` odwrócenie wszystkich bitów.

> **Celowo inaczej niż w C:** operatory bitowe wiążą mocniej niż porównania, więc
> `flagi & 4 != 0` znaczy `(flagi & 4) != 0`.

Uwagi:

- `/` na liczbach całkowitych zaokrągla w stronę zera: `7 / 2` to `3`. `%` ma znak lewej strony.
- Dzielenie przez zero **zatrzymuje program** komunikatem `runtime error: division by zero` z numerem linii.
- `&&` i `||` obliczają prawą stronę tylko wtedy, gdy trzeba.
- Teksty: `+` łączy, `==` `!=` porównują zawartość, `<` `>` porównują alfabetycznie (bajt po bajcie).
- Wskaźniki można porównywać przez `==` i `!=`, także z `null`. `str` nigdy nie jest `null`.
- Tablic i struktur nie da się porównać przez `==`; porównaj ich części.

---

## 8. Sterowanie

`if` może też dawać wartość; każda gałąź zawiera jedną wartość, a `else` jest obowiązkowe:

```c
ocena := if wynik > 100 { "wysoki" } else if wynik > 10 { "średni" } else { "niski" }
f32 predkosc = if biegnie { 2.5 } else { 1.0 }
```

```c
if x > 10 {
    ...
} else if x > 0 {
    ...
} else {
    ...
}

while licznik > 0 {
    licznik -= 1
}

for i in 0..10 {          // i = 0, 1, …, 9  (koniec nie wchodzi)
    if i == 3 { continue }
    if i == 8 { break }
}

for imie in imiona {      // każdy element tablicy (albo każdy znak tekstu)
    print(imie)
}

for i, imie in imiona {   // razem z indeksem: 0, 1, 2, ...
    print(i, imie)
}

for klucz, wartosc in wiek {   // każdy wpis mapy, w kolejności dodawania kluczy
    print(klucz, wartosc)      // (for klucz in wiek: same klucze)
}
```

- Warunek musi być typu `bool`. `if x {` z liczbą to błąd; napisz `if x != 0 {`.
- W `for i in a..b` oba końce są obliczane **raz**. `i` jest typu `int` i nie można go zmieniać w pętli.
- W `for x in lista` też nie można zmieniać `x`. Żeby zmieniać elementy, idź po indeksach:
  `for i in 0..lista.len { lista[i] = ... }`. Dodawanie do listy w trakcie pętli jest w porządku.
- `while true { ... }` kręci się do `break` albo `return`.
- Kod po `return`, `break` czy `continue` w tym samym bloku to błąd, bo nigdy się nie wykona.

### defer

`defer` wykonuje instrukcję, gdy kończy się bieżący blok, niezależnie od tego, jak się kończy: normalnie, przez
`return`, `break` albo `continue`. Kilka `defer` wykonuje się w odwrotnej kolejności. Przydaje się do sprzątania:

```c
f := fopen("dane.txt", "r")
defer fclose(f)
// ... używaj f; zostanie zamknięty na końcu bloku, którąkolwiek drogą program z niego wyjdzie
```

Odroczona instrukcja nie może zawierać `return`, `break` ani `continue`.

---

## 9. Funkcje

```c
fn nazwa(typ1 param1, typ2 param2) -> typ_wyniku {
    ...
    return wartosc
}

fn bez_wyniku(str komunikat) {    // bez "->": niczego nie zwraca
    print(komunikat)
}
```

- Argumenty zachowują się jak kopie: zmiana parametru nigdy nie zmienia zmiennej wywołującego.
  (Finch kopiuje naprawdę tylko wtedy, gdy trzeba: funkcja, która nie zmienia tablicy, dostaje ją za darmo.)
  Żeby zmienić zmienną wywołującego, przekaż wskaźnik: zob. [Wskaźniki](#14-wskaźniki).
- Funkcja z `-> typ` musi zwracać wartość na każdej ścieżce. Finch to sprawdza.
- Nie można zdefiniować funkcji wewnątrz innej funkcji.
- Funkcję, która może się nie udać, zapisuje się `-> int!` (albo `-> !` bez wyniku); zob.
  [Błędy jako wartości](#błędy-jako-wartości).
- Długie wywołania można rozbić na kilka linii w nawiasach; przecinek po ostatnim argumencie jest dozwolony.

---

## 10. Tablice i mapy

```c
liczby := [5, 3, 8]        // tablica int
[]str imiona               // pusta tablica str
siatka := [[1, 2], [3, 4]] // tablice tablic
[]f32 xs = [1, 2.5]        // typ decyduje, czym staną się liczby
```

| Działanie | Znaczenie |
|---|---|
| `a.len` | liczba elementów |
| `a[i]` | element `i` (licząc od 0); indeks poza zakresem zatrzymuje program jasnym komunikatem |
| `a[i] = x` | zmiana elementu |
| `a.push(x)` | dodanie na końcu |
| `a.pop()` | zdjęcie ostatniego elementu i zwrócenie go |
| `a.insert(i, x)` | wstawienie `x` na pozycję `i` z przesunięciem reszty |
| `a.remove(i)` | usunięcie pozycji `i` i zwrócenie jej |
| `a.clear()` | usunięcie wszystkiego |
| `a.resize(n)` | zmiana długości na `n` (nowe elementy to zera) |
| `a.contains(x)`, `a.find(x)` | czy `x` tam jest? jego indeks albo -1 (liczby, znaki, bool, str) |
| `a.sort()`, `a.reverse()` | w miejscu (liczby, znaki, str) |
| `a.slice(p, k)` | nowa tablica z elementami `p` … `k-1` |
| `a.join(sep)` | dla `[]str`: jeden tekst z `sep` między częściami |
| `a.ptr` | adres pierwszego elementu (dla C) |
| `print(a)` | wypisuje `[1, 2, 3]` |

**Przypisanie kopiuje:** po `b := a` zmiana `b` nie zmienia `a`.

### Mapy

`map[K]V` znajduje wartości po kluczu. Kluczami mogą być liczby całkowite, `char`, `bool` albo `str`; wartościami
cokolwiek (tablice, struktury, inne mapy). Przechodzenie po mapie idzie w kolejności, w jakiej klucze dodano po raz pierwszy.

```c
wiek := ["anna": 31, "bob": 25]     // mapa zapisana wprost: map[str]int
map[str][]str grupy                 // pusta mapa
map[int]str nazwy = [:]             // [:] to pusta mapa tam, gdzie typ jest znany
```

| Działanie | Znaczenie |
|---|---|
| `m[k]` | wartość dla `k`; brak klucza zatrzymuje program komunikatem `the key "k" is not in the map` |
| `m[k] = v` | dodanie albo podmiana |
| `m[k] += 1`, `m[k].push(x)`, `m[k].pole = …` | zmiana nieistniejącego klucza najpierw go dodaje, z wartością domyślną typu (0, `""`, `[]`, …) |
| `m.len` | liczba kluczy |
| `m.has(k)` | czy `k` tam jest? |
| `m.get(k, domyslna)` | wartość albo `domyslna` (obliczana tylko wtedy, gdy trzeba) |
| `m.remove(k)` | usuwa `k`; `true`, jeśli był |
| `m.clear()` | usuwa wszystko |
| `m.keys()`, `m.values()` | nowe tablice, w kolejności dodawania |
| `print(m)`, `str(m)` | `{"anna": 31, "bob": 25}` |

```c
map[str]int ile
for slowo in tekst.split(" ") {
    ile[slowo] += 1                   // liczenie: nowe słowo zaczyna od 0
}
for slowo, n in ile {
    print(slowo, n)
}
```

Wyszukiwanie korzysta z haszowania (średnio stały czas). Tak jak tablice, mapy są właścicielami swoich kluczy
i wartości, kopiują się przy przypisaniu i zwalniają na końcu bloku. Nie dodawaj ani nie usuwaj kluczy mapy wewnątrz
`for` po tej samej mapie; zbierz je w tablicy i zmień mapę po pętli.

---

## 11. Tekst (str)

```c
s := "Cześć"
s += ", świecie"          // łączenie
print(s.len, s[0])        // 17 C   (.len liczy bajty; ą, ś, ż, … zajmują po 2)
s[0] = 'K'                // zmiana znaku
```

| Metoda | Zwraca |
|---|---|
| `s.sub(a, b)` | fragment od `a` do `b-1` |
| `s.find(t)` | gdzie zaczyna się `t`, albo -1 |
| `s.contains(t)`, `s.starts_with(t)`, `s.ends_with(t)` | `bool` |
| `s.split(sep)` | `[]str`; `split("")` daje pojedyncze znaki |
| `s.trim()` | bez spacji i znaków nowej linii na obu końcach |
| `s.upper()`, `s.lower()` | zamienione A–Z / a–z (litery ASCII) |
| `s.replace(a, b)` | każde `a` zamienione na `b` |
| `s.repeat(n)` | `s` powtórzone n razy |
| `s.bytes()` | `[]u8` z bajtami |
| `s.ptr` | `ptr[char]` dla C (ważny, dopóki `s` istnieje) |

Wartości wstawia się w tekst przez `\(...)`: dowolny kod, a po liczbie `, 2` dla dwóch cyfr po przecinku.
Jeśli `(` ma zostać zwykłym znakiem, poprzedź `\` drugim ukośnikiem: `"\\(to nie kod)"`.

```c
print("hp \(hp) z \(max_hp), prędkość \(v, 2), imię \(p.imie.upper())")
```

`str` pamięta swoją długość, więc `.len` jest natychmiastowe, a na końcu zawsze ma bajt zerowy, więc można go przekazać
do C jako `char*`. W drugą stronę: funkcja C, która zwraca `char *`, daje ci `str` (kopia powstaje, gdy go zatrzymujesz),
a `str(p)` czyta `ptr[char]` zdobyty w inny sposób.

Tekst na kilka linii zapisuje się między `"""`. Znak nowej linii po otwierającym cudzysłowie znika, a z każdej linii
znika też wcięcie zamykającego cudzysłowu, więc tekst może mieć wcięcie razem z kodem:

```c
shader := """
    #version 330
    void main() { gl_Position = vec4(0.0); }
    """
```

Liczby jako tekst: `str(x)` to najkrótszy zapis, który da z powrotem tę samą liczbę (`0.1`, `0.30000000000000004`,
`1e+20`); `str(x, 2)` ma zawsze dwie cyfry po przecinku (`3.14`).

---

## 12. Struktury i metody

```c
struct Gracz {
    str imie
    int zycia = 3            // wartość domyślna
    []int wyniki
    Punkt poz                // struktury w strukturach
}

p := Gracz(imie: "Ola")                      // przez nazwy: brakujące pola dostają wartość domyślną (albo zero)
q := Punkt(1, 2)                             // po kolei: wtedy trzeba podać każde pole
p.zycia -= 1
p.wyniki.push(10)
p.poz.x = 5
print(p)       // Gracz(imie: "Ola", zycia: 2, wyniki: [10], poz: Punkt(x: 5, y: 0))
```

- Przypisanie albo przekazanie struktury kopiuje ją, razem z jej tablicami i tekstami.
- Struktura nie może bezpośrednio zawierać samej siebie; do tego służy pole `ptr[Wezel]` albo `[]Wezel` (drzewa, listy).

### Metody

Metoda to funkcja należąca do struktury: `fn Struktura.nazwa(...)`. W środku `self` to struktura, na której ją
wywołano, i to jest sama wartość wywołującego, a nie kopia: zmiany w `self` zostają.

```c
fn Gracz.ulecz(int ile) {
    self.zycia += ile
}

fn Gracz.zyje() -> bool {
    return self.zycia > 0
}

p := Gracz(imie: "Ola")
p.ulecz(2)                 // p.zycia wynosi teraz 5
if p.zyje() { ... }

druzyna[0].ulecz(1)        // na elemencie tablicy, polu, m[klucz], przez ptr[Gracz] ...
```

- Metoda stoi w tym samym pliku (module) co jej struktura i jest dostępna wszędzie tam, gdzie ta struktura.
- Metody można dodawać też do **struktur z nagłówków C**: `fn Vector2.length() -> f32 { ... }`.
- Metoda nie może nazywać się tak jak pole.
- Wywołanie metody zmieniającej `self` na zmiennej pętli (`for p in druzyna { p.ulecz(1) }`) to błąd, bo zmienna pętli
  jest tylko do odczytu; użyj `for i in 0..druzyna.len { druzyna[i].ulecz(1) }`.
- Metody, tak jak funkcje, mogą się nie udać (`-> !`); zob. [Błędy jako wartości](#błędy-jako-wartości).

---

## 13. Pamięć: kto co zwalnia

Tablic, tekstów i struktur nigdy nie zwalniasz. Finch robi to za ciebie, w przewidywalnym momencie:
**gdy kończy się blok, do którego wartość należy** (`}`), albo wcześniej, przy `return`, `break`, `continue`.

- Zmienna jest **właścicielem** swojej wartości. Przypisanie ją kopiuje, więc dwie zmienne nigdy nie dzielą jednej listy.
- Wartość z wywołania funkcji albo z wyrażenia jest **przenoszona** tam, gdzie trafia: bez kopii.
- `return lista` przenosi zmienną na zewnątrz funkcji: bez kopii.
- Parametry funkcji są **pożyczane**: funkcja dostaje wartość wywołującego bez kopii. Jeśli funkcja zmienia parametr,
  Finch najpierw robi jej prywatną kopię. Zauważysz to najwyżej w szybkości.

Dla pamięci, którą zarządzasz sam (listy wiązane, drzewa, współdzielenie między strukturami):

```c
wezel := new(Wezel(wartosc: 1))   // wartość na stercie, dostajesz ptr[Wezel]
defer free(wezel)                 // free(...) oddaje pamięć (i zwalnia tablice i teksty w środku)
```

Pamięć z C (`malloc`, obiekty bibliotek) zwalnia się funkcjami C, tak jak w C.

---

## 14. Wskaźniki

Wskaźnik przechowuje **adres** wartości. Finch zamiast `*` i `&` z C używa słów:

| C | Finch | Znaczenie |
|---|---|---|
| `int *p` | `ptr[int] p` | p wskazuje na int |
| `&x` | `addr(x)` | adres x (także `addr(a[i])`, `addr(s.pole)`) |
| `*p` | `p.value` | wartość, na którą wskazuje p |
| `p->pole` | `p.pole` | pole struktury, na którą wskazuje p |
| `p[i]` | `p[i]` | indeksowanie jak w C (bez sprawdzania zakresu) |
| `NULL` | `null` | nigdzie nie wskazuje |
| `void *` | `ptr` | wskazuje na coś nieznanego typu |

```c
fn podwoj(ptr[int] p) {
    p.value = p.value * 2
}

x := 21
podwoj(addr(x))           // x wynosi 42
```

Jeśli struktura ma własne pole o nazwie `value`, to `p.value` na wskaźniku do niej oznacza to pole.

Użycie `.value`, `.pole` albo `[i]` na wskaźniku `null` **zatrzymuje program** jasnym komunikatem.

⚠️ Wskaźnika na zmienną nie wolno używać po zakończeniu bloku tej zmiennej. Finch tego nie sprawdza.

---

## 15. Funkcje wbudowane

| Funkcja | Co robi |
|---|---|
| `print(a, b, ...)` | Wypisuje wszystkie wartości oddzielone spacjami, a na końcu przechodzi do nowej linii. Działa z każdym typem, także z tablicami i strukturami. |
| `input()`, `input("pytanie")` | Wczytuje jedną linię wpisaną przez użytkownika (bez znaku nowej linii). Na końcu wejścia zwraca pusty tekst. |
| `read_file(sciezka)` | Cały plik jako `str`. Jeśli pliku nie da się przeczytać, zatrzymuje program jasnym komunikatem, chyba że to obsłużysz: `read_file(p) or ...`. |
| `write_file(sciezka, tekst)` | Zapisuje (zastępuje) plik; zwraca `true`, jeśli się udało. Z `or` / `try` niepowodzenie niesie przyczynę. |
| `file_exists(sciezka)` | `true` / `false` |
| `delete_file(sciezka)` | Usuwa plik; `true`, jeśli się udało (albo przyczyna, z `or` / `try`). |
| `error(komunikat)` | Niepowodzenie, które zwraca funkcja mogąca się nie udać: `return error("...")`. |
| `shell(polecenie)` | Uruchamia polecenie powłoki i zwraca jego kod wyjścia. |
| `exit(kod)` | Natychmiast kończy program. |
| `addr(x)` | Wskaźnik na `x`; `addr(funkcja)` daje C wskaźnik na funkcję (funkcję zwrotną). |
| `new(wartosc)`, `free(p)` | Ręczna pamięć na stercie (zob. [Pamięć](#13-pamięć-kto-co-zwalnia)). |
| `fma(a, b, c)`, `muladd(a, b, c)` | `a * b + c` z jednym zaokrągleniem / najszybciej (zob. [Liczby zmiennoprzecinkowe](#liczby-zmiennoprzecinkowe-co-jest-gwarantowane)). |
| `f32_bits(x)`, `f32_from_bits(u)`, `f16_to_f32(h)`, … | Bity liczby i liczby połówkowej precyzji (zob. [Liczby zmiennoprzecinkowe](#liczby-zmiennoprzecinkowe-co-jest-gwarantowane)). |
| `int(x)`, `str(x)`, `u8(x)`, … | Zamiany typów (zob. [Zamiana typów](#zamiana-typów)). `str(x)` działa na każdym typie i daje ten sam tekst, który pokazuje `print`. `int(tekst) or 0` obsługuje tekst, który nie jest liczbą. |

---

## 16. Moduły

Program można podzielić na pliki. `import ksztalty` wczytuje `ksztalty.fch` z tego samego katalogu (albo z katalogu
wymienionego w zmiennej środowiskowej `FINCH_PATH`; kilka katalogów oddziela się `:`).

```c
// ksztalty.fch
struct Pudelko {
    float w
    float h
}

fn pole(Pudelko p) -> float {
    return p.w * p.h
}
```

```c
// main.fch
import ksztalty

fn main() {
    p := ksztalty.Pudelko(2, 3)
    print(ksztalty.pole(p))
    ksztalty.Pudelko inne    // typ z modułu w deklaracji
}
```

- Wszystkiego z modułu używa się z nazwą modułu na początku: `ksztalty.pole`, `ksztalty.Pudelko`.
- Moduł nie ma `main`. Moduły mogą importować inne moduły.
- Moduły w katalogach: `import ui/button` wczytuje `ui/button.fch`, a używa się go jako `button.draw()`.
- Pod inną nazwą: `import ui/button as btn` → `btn.draw()`. Dwa importy nie mogą mieć tej samej nazwy.

---

## 17. Biblioteki C

### Krok 1: zaimportuj nagłówek

```c
import "stdio.h"
import "GLFW/glfw3.h"     // ścieżka w systemowych katalogach nagłówków
import "mojabib.h"        // nagłówek obok twojego pliku .fch
```

Finch czyta nagłówek przez **libclang** (parser C z clanga) i udostępnia:

- **funkcje**, także te ze zmienną liczbą argumentów, jak `printf`,
- **struktury**, przez wartość i przez wskaźnik (`Vector2`, `SDL_Rect`, `CXCursor`, …), razem z ich polami,
- **wartości `enum`** i **proste stałe z `#define`** (`M_PI`, `GL_COLOR_BUFFER_BIT`),
- **zmienne globalne**, jak `stdout` i `stderr`.

### Krok 2: dolinkuj bibliotekę

```c
link "glfw"              // używa libglfw.so (najpierw pyta pkg-config, jeśli jest zainstalowany)
link "pomocnicze.c"      // kompiluje i linkuje twój plik C (obok pliku .fch)
link "gotowe.a"          // albo plik obiektowy / bibliotekę statyczną
```

Jeśli zapomnisz o linii `link`, Finch powie dokładnie, czego brakuje:

```
error: glfwInit, glfwCreateWindow, glfwMakeContextCurrent and 5 more come from "GLFW/glfw3.h", but its library isn't linked
  ...
      link "glfw"
```

### Jak typy C wyglądają w Finchu

| C | Finch |
|---|---|
| `char`, `signed char`, `unsigned char` | `char`, `i8`, `u8` |
| `short`, `int`, `long`, `long long` | `i16`, `i32`, `i64`, `i64` (według faktycznego rozmiaru) |
| `unsigned …` | `u16`, `u32`, `u64` |
| `float`, `double` | `f32`, `f64` |
| `_Bool` / `bool` | `bool` |
| `enum` | odpowiednia liczba całkowita ze znakiem |
| `char *` jako **parametr albo wynik** | `str` |
| `char *` w strukturze albo za wskaźnikiem | `ptr[char]` (czyta się go przez `str(p)`) |
| `struct X` przez wartość | `X`, struktura z tymi samymi polami |
| `struct X *`, `int *`, … | `ptr[X]`, `ptr[i32]`, … |
| `int tab[16]` w strukturze | `[16]i32`: można go indeksować i użyć `.len` |
| `void *`, wskaźniki na funkcje, niekompletne struktury | `ptr` |

### Przykład: struktury i funkcje zwrotne

```c
import "raylib.h"
link "raylib"

fn main() {
    InitWindow(800, 450, "Finch")
    while !WindowShouldClose() {
        BeginDrawing()
        ClearBackground(Color(r: 30, g: 30, b: 40, a: 255))
        DrawCircleV(Vector2(400, 225), 50, Color(255, 136, 0, 255))
        EndDrawing()
    }
    CloseWindow()
}
```

Żeby dać C funkcję do wywołania (jak funkcję porównującą dla `qsort`), użyj `addr(mojaFunkcja)`.
Jej parametry muszą być typami C (liczby, wskaźniki, struktury C).

### Grafika: OpenGL i Vulkan

Oba działają bezpośrednio; `examples/opengl.fch` i `examples/vulkan.fch` to kompletne programy.

- **OpenGL 3+** na Linuksie: nowoczesne funkcje (`glCreateShader`, `glGenVertexArrays`, …) są zadeklarowane tylko
  wtedy, gdy zdefiniowane jest `GL_GLEXT_PROTOTYPES`. Finch nie definiuje makr C, więc umieść te `#include` w malutkim
  nagłówku obok programu i zaimportuj go:

  ```c
  // opengl.h
  #define GL_GLEXT_PROTOTYPES
  #include <GL/gl.h>
  #include <GL/glext.h>
  #include <GLFW/glfw3.h>
  ```

  Potem `import "opengl.h"`, `link "glfw"`, `link "GL"`. Parametry wyjściowe przekazuje się przez `addr(...)`
  (`glGenBuffers(1, addr(vbo))`), dane bufora przez `.ptr` (`wierzcholki.ptr`), a przesunięcia w bajtach przez
  `ptr(...)` (`glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, krok, ptr(8))`).
- **Vulkan**: `import "vulkan/vulkan.h"` i `link "vulkan"`. Struktury „create info” buduje się przez nazwy pól
  (brakujące są zerami), uchwyty to `ptr`, a listy pobiera się zwykłym sposobem z dwoma wywołaniami: raz po liczbę,
  drugi raz z `lista.ptr` po `lista.resize(liczba)`. Makra z parametrami, takie jak `VK_MAKE_API_VERSION`, nie są
  dostępne; zamiast nich policz liczbę (`1 << 22` to Vulkan 1.0).

### Czego jeszcze nie ma

- **Unii** C i struktur z **polami bitowymi** przekazywanych do funkcji C albo z niej zwracanych **przez wartość**
  (wskaźniki na nie działają, unie jako zmienne też: `SDL_Event e`, `e.type`, `e.key.key`, `SDL_PollEvent(addr(e))`).
- **Makr z parametrami** (`#define MAX(a, b) ...`). Działają makra będące liczbą, tekstem, wyrażeniem z nich
  (`(1u << 3)`, `((Uint64)1)`, `(A | B)`) albo strukturą liczb (`RAYWHITE` z raylib, `(Color){ 245, 245, 245, 255 }`).
- `long double` i liczb 128-bitowych.

---

## 18. Błędy

### Błędy jako wartości

Niektóre niepowodzenia są normalne: użytkownik wpisze `abc`, pliku brakuje. Finch obsługuje je bez wyjątków i bez
ukrytych skoków. Funkcja, która może się nie udać, mówi o tym znakiem `!` po typie wyniku, a niepowodzenie zgłasza
przez `return error("komunikat")`:

```c
fn parsuj_port(str tekst) -> int! {
    n := int(tekst) or { return error("'" + tekst + "' to nie liczba") }
    if n < 1 || n > 65535 {
        return error("port " + str(n) + " jest poza zakresem")
    }
    return n
}

fn zapisz(str sciezka, str tekst) -> ! {    // może się nie udać, niczego nie zwraca
    try write_file(sciezka, tekst)
}
```

Wywołujący **musi** obsłużyć niepowodzenie, na jeden z trzech sposobów (zapomnienie to błąd kompilacji):

| Kod | Gdy się nie uda |
|---|---|
| `port := parsuj_port(s) or 8080` | użyj tej wartości (jest obliczana tylko przy niepowodzeniu; może to być kolejne wywołanie z `or`) |
| `port := parsuj_port(s) or { print(err); return }` | wykonaj blok; `err` to komunikat (`str`). Blok musi wyjść (`return`, `break`, `continue`, `exit`), chyba że wynik nie jest używany. |
| `port := try parsuj_port(s)` | przerwij bieżącą funkcję z tym samym błędem (ona też musi móc się nie udać) |

```c
fn wczytaj(str sciezka) -> Konfig! {
    tekst := try read_file(sciezka)
    port := try parsuj_port(tekst.trim())
    return Konfig(port: port)
}

fn main() -> ! {                // main też może się nie udać: błąd zostanie wypisany, a kod wyjścia to 1
    konfig := try wczytaj("app.conf")
    ...
}
```

Tak samo działają wbudowane funkcje, które mogą się nie udać: `int(tekst)`, `float(tekst)`, `read_file`, `write_file`,
`delete_file`. Bez `or` / `try` zachowują się jak dawniej (`int("x")` zatrzymuje program, `write_file` zwraca `bool`).

Pod spodem taka funkcja zwraca wartość razem z flagą i komunikatem: przy powodzeniu nic nie jest przydzielane,
nie ma odwijania stosu, a niepowodzenie kosztuje tyle, co zbudowanie komunikatu.

### Błędy kompilacji

```
gra.fch:12:9: error: 'wynik' must be int, but this is str
   12 |     wynik = "wysoki"
      |             ^
```

Format: `plik:linia:kolumna: error: wyjaśnienie`, a pod spodem linia z `^` pod miejscem błędu.

### Błędy w trakcie działania

Zamiast niezdefiniowanego zachowania (jak w C) Finch zatrzymuje program z komunikatem i kodem wyjścia 1:

| Komunikat | Przyczyna |
|---|---|
| `runtime error: division by zero` | `/` albo `%` przez zmienną równą 0 |
| `runtime error: division overflows …` | najmniejsza liczba ze znakiem podzielona przez −1 |
| `runtime error: index 5 is out of range (the length is 3)` | `a[5]` na krótszej tablicy albo tekście |
| `runtime error: pop() on an empty array` | |
| `runtime error: used .value on a null pointer` | także `.pole` i `[i]` przez `null` |
| `runtime error: can't turn "x" into int` | `int(...)` / `float(...)` na tekście, który nie jest liczbą |
| `runtime error: can't read the file "…"` | `read_file` na brakującym albo nieczytelnym pliku (obsłuż to przez `or`) |
| `runtime error: the key "x" is not in the map …` | `m["x"]`, gdy mapa nie ma takiego klucza (użyj `.has` albo `.get`) |
| `runtime error: called .f() on a null pointer` | metoda wywołana przez `ptr[T]` równy `null` |
| `runtime error: assert failed: …` | `assert(warunek, "…")` z fałszywym warunkiem |

Pod komunikatem jest ścieżka wywołań, które do niego doprowadziły, od najgłębszego:

```
gra.fch:6: runtime error: index 3 is out of range (the length is 2)
  in Wrog.nastepny (gra.fch:6)
  in krok (gra.fch:10)
  in main (gra.fch:15)
```

`finch run`, `finch test` oraz kompilacje z `-g` i `-O0` ją pokazują; zwykłe `finch build` jej nie dodaje, bo
pamiętanie jej kosztuje czas przy każdym wywołaniu (ok. 3 razy wolniej w programie złożonym wyłącznie z drobnych
wywołań, w prawdziwym programie dużo mniej). `--trace` ją włącza.

---

## 19. Edytory: VS Code, Kate i inne

Wszystkie funkcje edytorów pochodzą z jednego serwera języka, `finch lsp`, który korzysta z prawdziwego kompilatora:
błędy w trakcie pisania, typy i opisy po najechaniu kursorem, przejście do definicji, podpowiedzi (pola, metody,
metody map i tablic po `.`, zawartość modułów, nazwy z nagłówków C), spis zawartości pliku i podpowiedzi parametrów.

### Kate (oraz KWrite, KDevelop)

```sh
editors/kate/install.sh
```

Skrypt instaluje podświetlanie składni (`~/.local/share/org.kde.syntax-highlighting/syntax/finch.xml`) i dopisuje
`finch` do ustawień klienta LSP w Kate, nie ruszając twoich innych serwerów. Uruchom Kate ponownie i włącz wtyczkę
**LSP Client**. Żeby uruchamiać programy, dodaj `finch run %f` jako cel we wtyczce **Build & Run**; komunikaty
`plik:linia:kolumna: error:` będą wtedy klikalne. Szczegóły: `editors/kate/README.md`.

### VS Code

Rozszerzenie **Finch** (`editors/vscode`, dołączane też do każdego wydania jako plik `.vsix`) daje:
podświetlanie, błędy w trakcie pisania, podpowiedzi (także pól i metod po `.`), typy po najechaniu kursorem,
przejście do definicji (F12), spis zawartości pliku, podpowiedzi parametrów, gotowe fragmenty kodu i przycisk
▶ **Run** (`Ctrl+F5`). Błędy z **Finch: Build This File** trafiają do panelu Problems.

Instalacja: Extensions → `…` → *Install from VSIX…* → wybierz `finch-lang-*.vsix`. Rozszerzenie uruchamia `finch`
z PATH; jeśli leży gdzie indziej, ustaw **Finch: Path** w ustawieniach.

### Inne

Neovim, Helix, Zed, Emacs, Sublime i każdy inny edytor obsługujący Language Server Protocol:
uruchamiaj `finch lsp` dla plików `.fch` (stdin/stdout, bez opcji).

## 20. Debugowanie

```sh
finch build gra.fch -g -O0 -o gra
gdb ./gra
(gdb) break gra.fch:12
(gdb) run
(gdb) bt                # gdzie jesteśmy i przez które funkcje
(gdb) info locals       # wszystkie zmienne
(gdb) print gracz.zycia
```

`-g` dodaje informacje dla debuggera (linie, funkcje, zmienne, pola struktur, wektory). `-O0` zostawia widoczną
każdą zmienną; bez tego optymalizator może niektóre usunąć. Na Windowsie to też jest DWARF: użyj `lldb` z llvm-mingw
albo gdb.

---

## 21. Rozwiązywanie problemów

| Problem | Rozwiązanie |
|---|---|
| `can't find the C header 'x.h'` | Brakuje plików deweloperskich biblioteki. Zainstaluj je (Debian: `libx-dev`; Nix: dopisz do `shell.nix`). |
| `… come from "x.h", but its library isn't linked` | Dodaj `link "nazwa"` na początku pliku. |
| `the library 'x' wasn't found` | Biblioteka nie jest zainstalowana albo nazwa jest błędna. Na Niksie dopisz ją do `shell.nix` i pracuj w `nix-shell`. |
| `the C function 'f' can't be used from Finch yet` | Używa przez wartość unii, pól bitowych albo innego nieobsługiwanego typu. Poszukaj wariantu, który przyjmuje wskaźniki. |
| `--cpu native`: na innym komputerze program kończy się `Illegal instruction` | Został zbudowany pod procesor, na którym go budowano. Zbuduj bez `--cpu` albo ze starszym (`x86-64-v2`). |
| `can't find the module 'x'` | Połóż `x.fch` obok importującego pliku albo ustaw `FINCH_PATH`. |
| `couldn't build the Finch runtime` | Brak działającego kompilatora C. Zainstaluj gcc albo clang, albo ustaw `CC`. |
| Program po uruchomieniu nie znajduje biblioteki | Została dolinkowana z katalogu, którego system nie przeszukuje. Uruchamiaj w tym samym `nix-shell` albo zainstaluj ją w systemie. |
| Windows: `no C compiler found to link with ('clang')` | Dodaj katalog `bin` z llvm-mingw do PATH (rozdział 2). |
| Windows: `finch.exe` się nie uruchamia (brak `libclang.dll`) | Trzymaj `libclang.dll` obok `finch.exe`. |
| macOS: `finch` się nie uruchamia (`Library not loaded: …libLLVM…`) | `brew install llvm@21`. |
| macOS: `can't find the C header 'stdio.h'` | Zainstaluj narzędzia wiersza poleceń: `xcode-select --install`. |
| `--target arm64`: `no C compiler found to link with` | Zainstaluj `aarch64-linux-gnu-gcc` (Debian: `gcc-aarch64-linux-gnu`) albo ustaw `FINCH_CC`. |
| `clang` w `nix-shell` nie znajduje `stdio.h` | Masz stary `shell.nix`: `llvmPackages.clang` musi stać przed `llvmPackages.libclang`. |
| Program nigdy się nie kończy | Warunek pętli nigdy nie staje się fałszywy. Naciśnij Ctrl+C. |

---

## 22. Układ repozytorium i testy

```
Finch/
├── src/            kompilator w Finchu (lexer, parser, gen: LLVM IR jako tekst, cimport: libclang, lsp, main ...)
├── std/            biblioteka standardowa w Finchu: json, path, time, proc, fs, thread
├── runtime/        finch_rt.c: mały runtime (teksty, tablice, mapy, wczytywanie, pliki)
├── bootstrap/      build.sh, install.sh i pliki startowe (własny IR kompilatora, osobno dla każdego systemu)
├── examples/       hello, tour, types, structs, todo, guess, wordcount, c_import, simd, window, opengl, vulkan, raylib, llvm
├── tests/
│   ├── run/ new/   programy + dokładny wynik, który muszą wypisać (.out), wejście (.in)
│   ├── fail/ new/fail/   programy, które muszą się nie skompilować albo zatrzymać, z oczekiwanym błędem w 1. linii
│   ├── tool/       finch test, finch fmt, ścieżka wywołań, projekt z finch.toml
│   ├── run.sh      uruchamia testy (MEMCHECK=1 sprawdza też pamięć valgrindem)
│   ├── cross.sh    każdy test zbudowany na Windowsa / ARM64 i uruchomiony przez Wine / QEMU
│   └── lsp_test.py serwer języka, z którym test rozmawia tak jak edytor
├── editors/        rozszerzenie VS Code (vscode/), podświetlanie i konfiguracja LSP dla Kate (kate/)
├── docs/           ta dokumentacja (en, pl), razem ze ściągą
└── shell.nix       środowisko programistyczne dla Niksa
```

```sh
tests/run.sh                 # programy, oczekiwane błędy, kompilacje z -g, biblioteka współdzielona, serwer języka
MEMCHECK=1 tests/run.sh      # to samo pod valgrindem: bez wycieków i błędnych dostępów do pamięci
tests/cross.sh windows       # każdy test zbudowany na Windowsa i uruchomiony przez Wine
tests/cross.sh arm64         # każdy test zbudowany na Linuksa ARM64 i uruchomiony przez QEMU
```

---

## 23. Enumy i match

**Enum** to typ z krótką listą nazwanych wartości.

```
enum Kolor { Czerwony, Zielony, Niebieski }
enum Status { Ok = 200, NotFound = 404 }      // wariant może dostać liczbę, jeśli jej potrzebuje

k := Kolor.Zielony
print(k)                    // Zielony
print(int(Status.NotFound)) // 404
print(Kolor(2))             // Niebieski: liczba zamieniona na enum (sprawdzane: Kolor(7) to błąd)
```

Warianty zapisuje się `Kolor.Czerwony`. Można je porównywać przez `==` i `!=` (nie przez `<`), używać jako kluczy map,
pól struktur i elementów tablic, a enum może mieć metody tak jak struktura: `fn Kolor.nastepny() -> Kolor { ... }`.

**match** wybiera, co zrobić, na podstawie wartości, zamiast drabinki `if`:

```
match k {
    Czerwony -> print("ciepły")           // w match na enumie wystarczy sama nazwa
    Zielony, Niebieski -> print("zimny")  // kilka wartości w jednej gałęzi
}

rozmiar := match n {                      // jako wyrażenie: daje wartość
    1, 2, 3 -> "mały"
    4, 5 -> "średni"
    else -> "duży"
}

match slowo {
    "koniec" -> {
        print("pa")
        return
    }
    else -> print("słucham?")
}
```

`match` działa na enumach, liczbach całkowitych, znakach, tekstach i wartościach bool. Gałęzie są sprawdzane po kolei.
`match` na enumie musi obsłużyć **każdy wariant** albo kończyć się `else` (kompilator mówi, których brakuje); każdy inny
`match` potrzebuje `else`. W wyrażeniu każda gałąź daje wartość tego samego typu.

### Warianty z wartościami

Wariant może przenosić wartości. Tworzy się go przez `Ksztalt.Kolo(2.0)`, a wartości wyjmuje przez `match`, który je nazywa:

```c
enum Ksztalt {
    Kolo(float)
    Prostokat(float, float)
    Punkt
}

fn Ksztalt.pole() -> float {
    return match self {
        Kolo(r) -> 3.14 * r * r
        Prostokat(w, h) -> w * h
        Punkt -> 0.0
    }
}
```

`_` pomija wartość. `match` musi obsłużyć każdy wariant (albo kończyć się `else`). Wypisanie pokazuje `Prostokat(2, 3.5)`.
Taki enum kopiuje się i zwalnia jak struktura; nie da się go porównać przez `==` (użyj `match`). Warianty takich enumów
nie mogą mieć liczb.

## 24. Typy generyczne

Funkcję albo strukturę można napisać raz dla dowolnego typu, podając typ jako nazwę w nawiasach kwadratowych:

```
fn max[T](T a, T b) -> T {
    if a > b {
        return a
    }
    return b
}

struct Stos[T] {
    []T elementy
}

fn Stos[T].push(T x) {
    self.elementy.push(x)
}

print(max(3, 9), max("jabłko", "gruszka"))  // T wynika z argumentów
print(max[float](1, 2.5))                   // albo jest podany, gdy nie da się go ustalić
s := Stos[int]()                            // struktura generyczna zawsze ma podany typ
s.push(7)
```

Działa kilka parametrów (`struct Para[A, B]`) i typy zagnieżdżone (`Stos[Para[int, str]]`). Każde użycie tworzy własną
kopię kodu, więc typ generyczny jest tak szybki jak kod napisany ręcznie. Nie ma ograniczeń na typ: jeśli `T` nie potrafi
tego, czego wymaga funkcja (`a > b` na strukturze), błąd o tym mówi i podaje, którą wersję budował, np.
`(while building bigger[Punkt])`.

## 25. Biblioteka standardowa

Moduły dostarczane z Finchem, napisane w Finchu. `import json` najpierw szuka `json.fch` obok twojego pliku, a dopiero
potem w bibliotece, więc twój plik o tej samej nazwie ma pierwszeństwo.

| Moduł | Co robi |
|---|---|
| `path` | `join`, `base`, `dir`, `ext`, `stem`, `parts`, `clean`, `is_abs`: ścieżki jako tekst, rozdzielane `/` |
| `time` | `now()`, `millis()` (zegar do mierzenia czasu), `sleep(ms)`, `date(t)`, `local(t)`, `format(t, "%Y-%m-%d")`, `days_in_month` |
| `proc` | `run(polecenie)` (kod wyjścia), `output(polecenie)` (to, co wypisało; błąd, jeśli się nie udało), `env(nazwa, domyslna)`, `pid()`, `cwd()` |
| `json` | `parse(tekst)`, `stringify(v)`, `pretty(v)` i typ wartości `Json` z `get`, `at`, `set`, `push`, `has`, `str_or`, `int_or` ... |
| `fs` | `read_bytes(p)` / `write_bytes(p, dane)` (`[]u8`, dowolne bajty), `exists`, `is_dir`, `mkdir` (razem z katalogami nadrzędnymi), `list` (posortowane nazwy), `remove_dir`, `data_dir(aplikacja)` (katalog na zapisy użytkownika) |
| `thread` | `spawn(addr(praca), addr(dane))`, `t.join()`, `mutex()` z `lock`, `unlock`, `close`, `cpus()` (rozdział 29) |

`min(a, b, ...)`, `max(a, b, ...)`, `abs(x)` i `clamp(x, lo, hi)` działają na dowolnych liczbach (i wektorach)
bez importu (twoja funkcja o tej samej nazwie ma pierwszeństwo).

```
import json
import path

dok := try json.parse(read_file("config.json"))
nazwa := dok.get("name").str_or("bez nazwy")      // brakujący klucz to null, więc łańcuch wywołań się nie wysypie
dok.set("name", json.new_text(nazwa + "!"))
write_file(path.join("out", "config.json"), json.pretty(dok))
```

## 26. Obecne ograniczenia

Czego jeszcze nie ma:

- Interfejsów.
- Tekstów świadomych Unicode: `.len`, `s[i]` i `upper()` działają na bajtach / ASCII.
- Zabezpieczenia przed zmienianiem tej samej tablicy przez dwa wątki naraz (użyj muteksu).
- Windowsa na ARM64, systemów 32-bitowych, WebAssembly.

### Ostrzeżenia o dużych kopiach

Struktura z co najmniej 256 bajtami zwykłych danych (liczb, struktur z liczbami, `[N]T` z C) jest kopiowana bajt po
bajcie, gdy jest parametrem (`fn f(Cialo c)`) i w `for x in ciala`. Finch o tym ostrzega, ale i tak buduje program:

```
gra.fch:27:11: warning: 'b' is a Body (264 bytes), copied on every call to weight; take ptr[Body] to share it instead (callers can still pass the variable as it is)
gra.fch:45:5: warning: each Body (264 bytes) is copied into 'x'; for i in 0..bodies.len with bodies[i] works on it in place
```

Teksty, tablice i mapy w strukturze się nie liczą: przekazanie struktury kopiuje tylko ich uchwyty, a nie zawartość.
`b := a` nie daje ostrzeżenia, bo w kodzie widać, że to kopia. Metoda nigdy nie kopiuje `self`. Edytory pokazują
ostrzeżenia na żółto.

Zob. [plan rozwoju w README](../../README.pl.md#plan-rozwoju).

## 27. Testy: bloki test i finch test

Testy stoją obok kodu, który sprawdzają:

```c
fn obrazenia(int hp, int cios) -> int {
    return max(hp - cios, 0)
}

test "obrażenia nigdy nie schodzą poniżej zera" {
    assert(obrazenia(5, 10) == 0)
    assert(obrazenia(5, 2) == 3, "wyszło \(obrazenia(5, 2))")
}
```

`finch test gra.fch` uruchamia każdy blok `test` z tego pliku i z importowanych modułów (ale nie z biblioteki
standardowej), każdy osobno: nieudany `assert` albo dowolny błąd w trakcie działania kończy tylko ten jeden test,
a następny rusza dalej.

```
ok   obrażenia nigdy nie schodzą poniżej zera
FAIL wczytywanie poziomu
     poziom.fch:31: runtime error: can't read the file "poziomy/1.txt"

1 passed, 1 failed
```

Gdy któryś test się nie uda, kod wyjścia to 1, więc CI może z tego korzystać. `finch run` i `finch build` pomijają
bloki test. `assert(warunek)` działa też poza testami: zatrzymuje program komunikatem `assert failed`, gdy warunek jest fałszywy.

## 28. Projekt: finch.toml

Plik `finch.toml` obok kodu zapisuje raz to, co inaczej trzeba by podawać przy każdym poleceniu:

```toml
name = "gra"                         # program, który zbuduje finch build (z pliku entry)
entry = "src/main.fch"               # używany, gdy finch run / build / test nie dostaną pliku
paths = ["src", "lib"]               # katalogi z modułami Fincha
include = ["vendor/SDL3/include"]    # katalogi z nagłówkami C (także dla twoich plików .c / .cpp)
links = ["SDL3", "native/glue.cpp"]  # biblioteki i twoje pliki .c / .cpp / .o / .a
libdirs = ["vendor/SDL3/lib"]        # katalogi z bibliotekami
cflags = ["-DFAST"]                  # dla twoich plików .c / .cpp
ldflags = ["-Wl,-rpath,$ORIGIN"]     # dla linkera
cpu = "native"                       # to samo co --cpu native (rozdział 31)

[target.windows]                     # dodawane tylko przy budowaniu na Windowsa
links = ["ws2_32"]
libdirs = ["vendor/SDL3/lib/x64"]

[target.linux-arm64]                 # system i procesor: windows, linux, macos, android oraz -x86_64 / -arm64
libdirs = ["vendor/SDL3/lib/arm64"]
```

Ścieżki są liczone względem `finch.toml`. Finch szuka go w katalogu pliku i w katalogach nad nim, więc
`finch run src/narzedzia/edytor.fch` w środku projektu też z niego skorzysta. Z formatu TOML czytane jest tylko tyle:
`[sekcje]`, `klucz = "tekst"`, `klucz = ["a", "b"]` (także w kilku liniach), liczby, `true`/`false` i komentarze `#`.

## 29. Wątki

```c
import thread

struct Kawalek {
    []float probki
    float szczyt
}

fn analizuj(ptr arg) {              // funkcja wątku przyjmuje jeden ptr
    ptr[Kawalek] k = arg
    for p in k.probki {
        k.szczyt = max(k.szczyt, abs(p))
    }
}

fn main() {
    []Kawalek kawalki = wczytaj_kawalki()
    []thread.Thread dzialajace
    for i in 0..kawalki.len {
        dzialajace.push(thread.spawn(addr(analizuj), addr(kawalki[i])) or {
            print(err)
            exit(1)
        })
    }
    for i in 0..dzialajace.len {
        dzialajace[i].join()         // czekamy na wszystkie
    }
}
```

To, co dostaje wątek, jest współdzielone, a nie kopiowane, więc:

- musi istnieć aż do `join()` (tutaj `kawalki` żyje do końca `main`);
- nie zmieniaj tablicy, do której wskazują wątki (żadnego `push`), dopóki nie zostaną połączone;
- to, co zmienia więcej niż jeden wątek, trzeba chronić blokadą albo musi to być liczba zmieniana przez `atomic_...`.

```c
m := thread.mutex()
defer m.close()
m.lock()
dziennik.push("gotowe")             // między lock a unlock pracuje tylko jeden wątek naraz
m.unlock()

atomic_add(addr(gotowe), 1)         // gotowe to int wspólny dla wątków
n := atomic_load(addr(gotowe))
```

`atomic_add`, `atomic_sub` i `atomic_swap` zwracają poprzednią wartość; `atomic_cas(p, stara, nowa)` wpisuje `nowa`
tylko wtedy, gdy jest tam `stara`, i mówi, czy to zrobiło. `thread.cpus()` mówi, ile wątków komputer wykonuje naraz.

## 30. C++, Rust i Android

Finch rozmawia z innymi językami przez C, tak jak każdy język.

**C++**: to, co ma wołać Finch, wystaw przez nagłówek C, z `extern "C"` po stronie C++:

```cpp
// glue.h
#ifdef __cplusplus
extern "C" {
#endif
int krok_fizyki(float dt);
#ifdef __cplusplus
}
#endif
```

i `import "glue.h"` z `link "glue.cpp"` (albo `links = ["glue.cpp"]` w `finch.toml`). Plik `.cpp` kompiluje kompilator
C++ (`clang++` albo `g++`, albo `FINCH_CXX`), który potem też linkuje, więc biblioteka C++ dołącza się sama.
Bibliotekę C++ z API w C (Box2D 3, JoltC dla Jolta, nakładka C na PhysX) importuje się jak każdą bibliotekę C.

**Rust**: zbuduj `staticlib` i wystaw funkcje C:

```rust
// Cargo.toml: [lib] crate-type = ["staticlib"]
#[no_mangle]
pub extern "C" fn dlugosc(x: f32, y: f32) -> f32 { (x * x + y * y).sqrt() }
```

Napisz nagłówek (`float dlugosc(float x, float y);` albo wygeneruj go przez `cbindgen`), a potem `import "trasa.h"`
i `link "target/release/libtrasa.a"`. Na Linuksie biblioteka statyczna z Rusta potrzebuje też `links = ["dl", "pthread"]`.

**Android**: `finch build gra.fch --target android --shared` tworzy `libgra.so` dla 64-bitowego Androida na ARM
(API 24 i nowsze) clangiem z NDK, którego Finch szuka przez `ANDROID_NDK_HOME` (albo `FINCH_CC`). Wczytaj ją z aplikacji,
np. przez projekt Androida z SDL3 (`SDL_main` biblioteki to `export fn`). Dane lokalne dla wątków idą przez emulowane
TLS, którego wymagają starsze wersje Androida.

---

## 31. Wektory (SIMD)

Procesor potrafi dodać albo pomnożyć kilka liczb jedną instrukcją: 4 liczby `float` naraz przez SSE, 8 przez AVX2,
16 przez AVX-512, 4 przez NEON na ARM. To SIMD (jedna instrukcja, wiele danych) i to dzięki temu filtry obrazu,
dźwięk, fizyka i sieci neuronowe są szybkie. W Finchu to po prostu typ: **`f32x8` to 8 liczb `f32` obok siebie**.

```c
a := f32x4(1.0, 2.0, 3.0, 4.0)
b := f32x4(0.5)                  // 0.5 w każdym elemencie
print(a + b)                     // f32x4(1.5, 2.5, 3.5, 4.5)
print(a * 2.0)                   // zwykła liczba działa na każdy element: f32x4(2, 4, 6, 8)
print(a.sum(), a[2], a.len)      // 10 3 4
```

**Typy** zapisuje się jako typ liczby, `x` i liczbę elementów: `f32x4` `f32x8` `f32x16`, `f64x2` `f64x4` `f64x8`,
`i8x16` `i8x32` `i8x64`, `i16x8` `i16x16`, `i32x4` `i32x8` `i32x16`, `i64x2` `i64x4` i to samo z `u` (bez znaku).
Od 2 do 64 elementów, razem najwyżej 512 bitów. Wektor to zwykła wartość, tak jak `int`: może być zmienną, polem
struktury, elementem tablicy, wartością w mapie i parametrem funkcji, a `print` i `str` go pokazują.

**Tworzenie:**

| | |
|---|---|
| `f32x8(x)` | `x` w każdym elemencie |
| `f32x8(a, b, c, d, e, f, g, h)` | po jednej liczbie na element |
| `f32x8(lista, i)` | `lista[i]` … `lista[i+7]` z `[]f32` (albo przez `ptr[f32]`); zatrzymuje program, jeśli nie wszystkie są w liście |
| `f32x8(v)` | inny 8-elementowy wektor przeliczony element po elemencie: `f32x8(calkowite)`, `i32x8(ulamki)` (ucina części ułamkowe), `i16x8(i32ki)` |

**Działania:**

| | |
|---|---|
| `+ - * /` | element po elemencie (`/` tylko na liczbach zmiennoprzecinkowych: x86 i ARM nie dzielą liczb całkowitych element po elemencie) |
| `& \| ^ << >> ~` | element po elemencie, na liczbach całkowitych |
| `-v` | każdy element ze zmienionym znakiem |
| `v == w`, `v != w` | `true`, jeśli wszystkie elementy są równe / jeśli któryś się różni |
| `v[i]`, `v[i] = x`, `v.len` | jeden element; liczba elementów |
| `min(v, w)`, `max(v, w)`, `abs(v)`, `clamp(v, lo, hi)` | element po elemencie; `lo`, `hi` i `w` mogą być zwykłymi liczbami |
| `v.sum()`, `v.min()`, `v.max()` | wszystkie elementy złożone w jedną liczbę |
| `v.dot(w)` | `(v * w).sum()` |
| `v.sqrt()`, `v.floor()`, `v.ceil()`, `v.round()` | każdy element (liczby zmiennoprzecinkowe) |
| `v.fma(b, c)` | `v * b + c` z jednym zaokrągleniem, zawsze |
| `v.muladd(b, c)` | `v * b + c` najszybciej, jak się da: jedną instrukcją, gdzie procesor to potrafi |
| `v.store(lista, i)` | zapisuje elementy do `lista[i]` … (albo przez `ptr`) |

`<` i inne porównania dałyby osobną odpowiedź dla każdego elementu, więc ich nie ma; użyj `min`, `max` albo `v[i] < w[i]`.

**Prawdziwa pętla.** Zwykły schemat: tyle całych wektorów, ile się zmieści, a resztę po jednej liczbie.

```c
fn iloczyn([]f32 a, []f32 b) -> f32 {
    suma := f32x8(0.0)
    i := 0
    while i + 8 <= a.len {
        suma = f32x8(a, i).muladd(f32x8(b, i), suma)
        i += 8
    }
    wynik := suma.sum()
    while i < a.len {
        wynik += a[i] * b[i]
        i += 1
    }
    return wynik
}
```

`examples/simd.fch` mierzy czas tej pętli i porównuje go ze zwykłą.

**Jakie instrukcje.** Domyślnie program działa na każdym procesorze swojego systemu (na x86-64 oznacza to SSE2, więc
`f32x8` liczy się w dwóch połówkach). `--cpu native` (albo `cpu = "native"` w `finch.toml`) pozwala LLVM użyć wszystkiego,
co ma procesor, na którym budujesz: AVX2 i FMA, AVX-512 albo nowszych części NEON na ARM. Taki program potrzebuje potem
podobnego procesora. Można też wybrać poziom: `--cpu x86-64-v3` (AVX2, większość komputerów od 2015 roku),
`--cpu x86-64-v4` (AVX-512), `--cpu apple-m1`.

Wektory szersze niż rejestry procesora LLVM dzieli na części, a ten sam kod buduje się tak samo na x86-64 i na ARM64.
`v.sum()` na liczbach zmiennoprzecinkowych dodaje elementy w kolejności, która jest najszybsza, więc ostatnie cyfry mogą
się różnić od dodawania ich po kolei.
