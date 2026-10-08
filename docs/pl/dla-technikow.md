# Finch dla techników

**Dla osób, które dobrze znają komputery, ale programować nie umieją albo umieją niewiele.**
Ten przewodnik opisuje instalację Fincha, obsługę polecenia `finch`, cały język, pamięć,
moduły, biblioteki C, debugowanie i rozwiązywanie typowych problemów. Nie opisuje, jak
kompilator działa w środku. Do tego służy [przewodnik dla inżynierów](dla-inzynierow.md).

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
8. [Sterowanie przebiegiem](#8-sterowanie-przebiegiem)
9. [Funkcje](#9-funkcje)
10. [Tablice i mapy](#10-tablice-i-mapy)
11. [Tekst (str)](#11-tekst-str)
12. [Struktury i metody](#12-struktury-i-metody)
13. [Pamięć: kto co zwalnia](#13-pamięć-kto-co-zwalnia)
14. [Wskaźniki](#14-wskaźniki)
15. [Funkcje wbudowane](#15-funkcje-wbudowane)
16. [Moduły](#16-moduły)
17. [Biblioteki z C](#17-biblioteki-z-c)
18. [Błędy](#18-błędy)
19. [Edytory: VS Code, Kate i inne](#19-edytory-vs-code-kate-i-inne)
20. [Debugowanie](#20-debugowanie)
21. [Rozwiązywanie problemów](#21-rozwiązywanie-problemów)
22. [Struktura projektu i testy](#22-struktura-projektu-i-testy)
23. [Obecne ograniczenia](#23-obecne-ograniczenia)

---

## 1. Czym jest Finch

Finch to język **kompilowany**. Narzędzie `finch` tłumaczy plik `.fch` na prawdziwy program
(natywny plik wykonywalny, taki jak te z C). Korzysta przy tym z **LLVM**, tego samego zaplecza
kompilatorów, na którym działają clang, Rust i Swift.

Co to daje:

- **Szybkość zbliżoną do C.** Nie ma interpretera, maszyny wirtualnej ani garbage collectora.
  Kontrola zakresu tablic i inne zabezpieczenia po optymalizacji prawie nic nie kosztują.
- **Małe, samodzielne programy**, które można skopiować i uruchomić.
- **Pamięć obsługiwana za ciebie.** Listy i teksty są zwalniane automatycznie na końcu bloku,
  do którego należą, bez garbage collectora.
- **Bezpośredni dostęp do bibliotek C.** Piszesz `import "stdio.h"` i wołasz, co chcesz,
  nawet funkcje przyjmujące struktury.

Zasady projektu: jeden sposób na każdą rzecz, składnia podobna do C bez jej pułapek
i komunikaty błędów, które mówią, jak problem naprawić.

---

## 2. Instalacja

Finch działa na **Linuksie** (x86-64 i ARM64), **macOS** (Apple Silicon i Intel) i **Windowsie** (x86-64),
i jest budowany z **LLVM 21**. Każde [wydanie](https://github.com/Nexoniarz/Finch/releases) ma gotowy
`finch` dla Windowsa, Linuksa x86-64, Linuksa ARM64 i macOS ARM64.

### Windows

1. Pobierz **`finch-windows-x64.zip`** z [wydań](https://github.com/Nexoniarz/Finch/releases) i rozpakuj,
   np. do `C:\finch`. W środku są `finch.exe` i `libclang.dll`.
2. Zainstaluj **LLVM 21** (`LLVM-21.x.x-win64.exe` z [wydań LLVM](https://github.com/llvm/llvm-project/releases))
   i zaznacz *Add LLVM to the system PATH*. Finch używa jego `clang` do linkowania programów.
3. Zainstaluj **Visual Studio Build Tools 2022** z pakietem *Desktop development with C++*: dostarcza biblioteki
   Windowsa, z którymi linkowany jest każdy program.
4. Dodaj `C:\finch` do PATH, otwórz nowy terminal i wpisz `finch version`.

Programy to zwykłe pliki `.exe`. Żeby zbudować Fincha ze źródeł na Windowsie, idź za `.github/workflows/ci.yml`
(Visual Studio 2022 + pakiet `clang+llvm-21.x-x86_64-pc-windows-msvc` z wydań LLVM).

### macOS

```sh
brew install llvm@21                # LLVM dla Fincha; programy linkuje clang od Apple
```

Potem rozpakuj **`finch-macos-arm64.tar.gz`** z wydań (korzysta z `llvm@21` z Homebrew)
albo zbuduj go sam (narzędzia wiersza poleceń Xcode: `xcode-select --install`):

```sh
git clone https://github.com/Nexoniarz/Finch.git && cd Finch
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DLLVM_DIR="$(brew --prefix llvm@21)/lib/cmake/llvm"
ninja -C build
./build/finch version
```

Frameworki Apple linkuje się przez `link "Cocoa.framework"` (`-framework Cocoa`), a ich nagłówki importuje
się normalnie: `import "OpenGL/gl.h"`.

### Linux

Gotowe `finch-linux-x64.tar.gz` / `finch-linux-arm64.tar.gz` potrzebują bibliotek LLVM 21
(Debian/Ubuntu: `libllvm21 libclang1-21` z [apt.llvm.org](https://apt.llvm.org)) i kompilatora C (`cc`).
Samodzielne budowanie wygląda tak samo na x86-64 i ARM64 (Raspberry Pi 4/5, Graviton, …).
Z innymi wersjami LLVM Finch prawdopodobnie się nie skompiluje, bo API C++ LLVM zmienia się między wydaniami.

### Opcja A: Nix (zalecana, nic nie instalujesz ręcznie)

```sh
git clone https://github.com/Nexoniarz/Finch.git
cd Finch
nix-shell                        # pobiera LLVM, libclang, clang, cmake, ninja, pkg-config
cmake -S . -B build -G Ninja
ninja -C build
./build/finch version            # finch 2.4.0 (LLVM 21.x)
```

### Opcja B: pakiety z twojej dystrybucji

Potrzebujesz: kompilatora C/C++, CMake ≥ 3.20, Ninja (albo Make), plików deweloperskich LLVM 21
i libclang 21, a opcjonalnie `pkg-config`.

| Dystrybucja | Pakiety (nazwy mogą się nieco różnić zależnie od wersji) |
|---|---|
| Debian / Ubuntu | `build-essential cmake ninja-build pkg-config llvm-21-dev libclang-21-dev` |
| Fedora | `gcc-c++ cmake ninja-build pkgconf llvm-devel clang-devel` |
| Arch | `base-devel cmake ninja pkgconf llvm clang` |

Ta opcja nie była testowana. Jeśli CMake nie znajduje LLVM, wskaż mu katalog:

```sh
cmake -S . -B build -G Ninja -DLLVM_DIR=/usr/lib/llvm-21/lib/cmake/llvm
ninja -C build
```

### Budowanie na inny system

`--target` buduje program na inny system. Do linkowania Finch potrzebuje kompilatora C dla tego systemu:

| `--target` | Buduje | Kompilator C | `finch run` używa |
|---|---|---|---|
| `windows` | `.exe` dla Windowsa | `x86_64-w64-mingw32-gcc` (MinGW-w64) | Wine |
| `arm64` | program dla Linuksa ARM64 | `aarch64-linux-gnu-gcc` | `qemu-aarch64` |
| `linux` | program dla Linuksa x86-64 | `clang --target=…` | |
| `macos` | program dla macOS (na Macu: dla drugiego procesora) | `clang --target=…` | |
| dowolny triple LLVM | np. `aarch64-unknown-linux-gnu` | `clang --target=<triple>` | |

```sh
finch build gra.fch --target windows       # gra.exe
finch run gra.fch --target arm64           # buduje na ARM64 i uruchamia w QEMU
FINCH_CC=aarch64-unknown-linux-gnu-gcc finch build narzedzie.fch --target arm64   # inna nazwa kompilatora
```

Na Niksie te kompilatory to `pkgsCross.mingwW64.buildPackages.gcc` i
`pkgsCross.aarch64-multiplatform.buildPackages.gcc`.

### Dodanie `finch` do PATH (opcjonalnie)

```sh
sudo cp build/finch /usr/local/bin/
finch version
```

Podczas pracy Finch potrzebuje kompilatora C (`cc`). Używa go do linkowania programów i jednorazowo
do zbudowania swojej małej biblioteki uruchomieniowej, którą trzyma w `~/.cache/finch`.
Inny kompilator wskażesz zmienną `CC`.

---

## 3. Polecenie finch

```
finch run   <plik.fch> [argumenty...]   skompiluj i od razu uruchom
finch build <plik.fch> [-o nazwa]       skompiluj do programu (domyślna nazwa: nazwa pliku)
finch ir    <plik.fch>                  pokaż LLVM IR (dla ciekawskich)
finch lsp                              serwer języka, dla edytorów
finch version                          pokaż wersję

opcje:
  -l <lib>          dolinkuj bibliotekę C, to samo co  link "lib"  w pliku
  --target <nazwa>  zbuduj na inny system: windows, linux, arm64, macos albo triple LLVM
  -g                dodaj informacje dla debuggera (gdb / lldb / Visual Studio)
  -O0               bez optymalizacji
```

Przykłady:

```sh
finch run hello.fch                 # wypisuje wynik
finch run narzedzie.fch dane.txt -v # argumenty po nazwie pliku trafiają do programu
finch build gra.fch -o mojagra      # tworzy ./mojagra
finch build gra.fch -g -O0          # wersja dla debuggera
```

`finch run` przekazuje dalej kod wyjścia programu. Jeśli program się wysypie, powie jak
(np. `the program crashed: Segmentation fault`).

---

## 4. Jak wygląda plik Fincha

```c
// komentarz zaczyna się od dwóch ukośników
/* albo zajmuje
   kilka linijek */

import "math.h"          // nagłówki C (opcjonalne), na górze
import ksztalty          // moduły Fincha (opcjonalne): ksztalty.fch obok tego pliku
link "m"                 // biblioteki albo pliki C do dolinkowania (opcjonalne)

struct Punkt {           // własne typy
    int x
    int y
}

fn main() {              // tu zaczyna się program
    print("Cześć")
}
```

Zasady, które warto znać:

- **Jedna instrukcja na linijkę.** Średników nie ma. Długie wyrażenie możesz złamać **po** operatorze
  (`a +⏎ b`) albo w dowolnym miejscu wewnątrz nawiasów `( )` lub `[ ]`.
- **Klamry są zawsze obowiązkowe** po `if`, `else`, `for`, `while`, a otwierająca `{` stoi w tej samej linijce.
- **Bez nawiasów wokół warunków:** `if x > 5 {`, a nie `if (x > 5) {`.
- Funkcje i struktury mogą być w pliku w dowolnej kolejności.
- `main` to `fn main()`, `fn main() -> int` (zwraca kod wyjścia) albo `fn main([]str args)`
  (dostaje argumenty z linii poleceń; `args[0]` to ścieżka samego programu).

---

## 5. Zmienne

```c
x := 5               // nowa zmienna, typ zgadnięty (int)
int y = 10           // nowa zmienna, typ podany jawnie
int z                // bez wartości: startuje od zera ("" dla str, [] dla tablic, null dla wskaźników)
Punkt p              // struktura bez wartości: zera albo wartości domyślne struktury

x = 7                // przypisanie
x += 1               // także -= *= /= %=
```

- Nazwę można zadeklarować **raz** w funkcji. Finch nie pozwala użyć jej ponownie w bloku
  wewnętrznym (nie ma „przesłaniania”), bo to częste źródło pomyłek.
- Zmienna istnieje od deklaracji do końca bloku `{ }`, w którym powstała.
- Nazwy: litery ASCII, cyfry i `_`, bez cyfry na początku. Polskie litery tylko w tekstach w cudzysłowie.
  Nazwy typów (`int`, `u8`, …), funkcji wbudowanych i słowa kluczowe są zarezerwowane.

**Słowa kluczowe:** `fn return if else while for in break continue true false null import link struct defer`

---

## 6. Typy

### Typy na co dzień

| Typ     | Przechowuje | Przykład |
|---------|-------------|----------|
| `int`   | liczba całkowita, 64-bitowa (to samo co `i64`) | `42`, `-7`, `0xFF`, `1_000_000` |
| `float` | liczba z przecinkiem, 64-bitowa (to samo co `f64`) | `3.14` |
| `bool`  | `true` albo `false` | |
| `char`  | jeden znak wielkości bajtu | `'A'`, `'\n'` |
| `str`   | tekst | `"cześć\tświecie"` |
| `[]T`   | lista (tablica) elementów `T` | `[1, 2, 3]` |
| `map[K]V` | wartości typu `V` znajdowane po kluczu typu `K` | `["a": 1, "b": 2]` |
| twoje struktury | nazwane grupy wartości | `Punkt(1, 2)` |

Znaki specjalne w tekstach: `\n` nowa linia, `\t` tabulator, `\r`, `\0`, `\\`, `\"`, `\'`.

### Typy z rozmiarem (do plików, sprzętu, bibliotek C)

| Ze znakiem | Zakres | Bez znaku | Zakres |
|---|---|---|---|
| `i8`  | −128 … 127 | `u8`  | 0 … 255 |
| `i16` | −32 768 … 32 767 | `u16` | 0 … 65 535 |
| `i32` | ok. ±2,1 mld | `u32` | 0 … ok. 4,29 mld |
| `i64` | ok. ±9,2 × 10¹⁸ | `u64` | 0 … ok. 1,8 × 10¹⁹ |

`f32` to 32-bitowa liczba z przecinkiem (ok. 7 cyfr znaczących), `f64` to 64-bitowa (ok. 15–16).

### Zamiana typów

Finch zamienia typy sam **tylko wtedy, gdy nic nie może zginąć**:

| Z | Na | Automatycznie? |
|---|---|---|
| mniejsza liczba całkowita | większa o tej samej znakowości (`i8`→`i32`, `u8`→`u64`) | ✅ |
| bez znaku | ściśle większa ze znakiem (`u8`→`i16`, `u32`→`i64`) | ✅ |
| dowolna liczba całkowita | `float` / `f32` | ✅ |
| `f32` | `f64` | ✅ |
| `null` | dowolny wskaźnik | ✅ |
| dowolny wskaźnik | `ptr`, a `ptr` na dowolny wskaźnik | ✅ |
| wszystko inne | | ❌ napisz to jawnie: `typ(wartość)` |

**Liczby wpisane w kod się dopasowują.** Liczba napisana wprost w kodzie pasuje do każdego typu,
który ją pomieści: `u8 b = 200` działa, a `u8 b = 300` to błąd. To samo dotyczy stałych z nagłówków C.

**Jawna zamiana** wygląda jak wywołanie funkcji o nazwie typu:

```c
int(3.99)        // 3     (obcina część ułamkową)
float(7) / 2     // 3.5
u8(300)          // 44    (zostawia najniższe 8 bitów)
int('A')         // 65
char(66)         // 'B'
bool(0)          // false (każda liczba różna od zera to true)
str(42)          // "42"  (też float, bool, char; str([]u8) zamienia bajty na tekst)
int("42")        // 42    (zatrzymuje program, jeśli tekst nie jest liczbą)
float("2.5")     // 2.5
ptr(p)           // wskaźnik z typem jako wskaźnik bez typu
ptr(16)          // liczba jako wskaźnik (API C, które przekazują przesunięcia jako wskaźniki, np. OpenGL)
int(p)           // wskaźnik jako liczba (jego adres)
str(p)           // wskaźnik na znaki z C jako tekst
```

**Liczby o stałym rozmiarze się „zawijają”**, jak w prawdziwym sprzęcie: `u8` równe 255 plus 1 daje 0.
Działania liczą się w typie użytych wartości, więc `u8(250) + 10` daje `4`, a nie `260`.

---

## 7. Operatory

### Według priorytetu (wyższy wiąże mocniej)

| Priorytet | Operatory | Znaczenie |
|---|---|---|
| 5 | `*` `/` `%` `<<` `>>` `&` | mnożenie, dzielenie, reszta, przesunięcia bitów, bitowe AND |
| 4 | `+` `-` `\|` `^` | dodawanie (i sklejanie tekstu), odejmowanie, bitowe OR, bitowe XOR |
| 3 | `==` `!=` `<` `<=` `>` `>=` | porównania |
| 2 | `&&` | logiczne I |
| 1 | `\|\|` | logiczne LUB |

Jednoargumentowe (przed wartością): `-x` zmiana znaku, `!x` logiczne NIE, `~x` odwrócenie wszystkich bitów.

> **Celowo inaczej niż w C:** operatory bitowe wiążą mocniej niż porównania, więc
> `flagi & 4 != 0` znaczy `(flagi & 4) != 0`.

Uwagi:

- `/` na liczbach całkowitych zaokrągla w stronę zera: `7 / 2` to `3`. `%` ma znak lewej strony.
- Dzielenie przez zero **zatrzymuje program** komunikatem `runtime error: division by zero` z numerem linijki.
- `&&` i `||` liczą prawą stronę tylko wtedy, gdy to potrzebne.
- Tekst: `+` skleja, `==` `!=` porównują treść, `<` `>` porównują alfabetycznie (bajt po bajcie).
- Wskaźniki porównuje się przez `==` i `!=`, także z `null`. `str` nigdy nie jest `null`.
- Tablic i struktur nie porównasz przez `==`; porównuj ich części.

---

## 8. Sterowanie przebiegiem

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

for i in 0..10 {          // i = 0, 1, …, 9  (koniec nie jest wliczany)
    if i == 3 { continue }
    if i == 8 { break }
}

for imie in imiona {      // każdy element tablicy (albo każdy znak str)
    print(imie)
}

for i, imie in imiona {   // razem z indeksem: 0, 1, 2, ...
    print(i, imie)
}

for klucz, wartosc in wiek {  // każdy wpis mapy, w kolejności dodawania kluczy
    print(klucz, wartosc)     // (for klucz in wiek: same klucze)
}
```

- Warunek musi być typu `bool`. `if x {` z liczbą to błąd; napisz `if x != 0 {`.
- W `for i in a..b` oba końce są liczone **raz**. `i` jest typu `int` i nie można go zmieniać w pętli.
- W `for x in lista` zmiennej `x` też nie zmienisz. Żeby zmieniać elementy, przechodź po indeksach:
  `for i in 0..lista.len { lista[i] = ... }`. Dodawanie do listy w trakcie pętli jest dozwolone.
- `while true { ... }` kręci się do `break` albo `return`.
- Kod po `return`, `break` lub `continue` w tym samym bloku to błąd, bo nigdy się nie wykona.

### defer

`defer` uruchamia instrukcję, gdy bieżący blok się kończy, niezależnie od tego, jak się kończy:
normalnie, przez `return`, `break` lub `continue`. Kilka `defer` wykonuje się w odwrotnej kolejności.
Przydaje się do sprzątania:

```c
f := fopen("dane.txt", "r")
defer fclose(f)
// ... używaj f; zostanie zamknięty na końcu bloku, na każdej ścieżce
```

Instrukcja w `defer` nie może zawierać `return`, `break` ani `continue`.

---

## 9. Funkcje

```c
fn nazwa(typ1 param1, typ2 param2) -> typ_wyniku {
    ...
    return wartosc
}

fn bez_wyniku(str wiadomosc) {    // bez "->": nic nie zwraca
    print(wiadomosc)
}
```

- Argumenty zachowują się jak kopie: zmiana parametru nigdy nie zmienia zmiennej u wywołującego.
  (Finch kopiuje naprawdę tylko wtedy, gdy trzeba: funkcja, która nie zmienia tablicy, dostaje ją za darmo.)
  Żeby zmienić zmienną wywołującego, przekaż wskaźnik, zobacz [Wskaźniki](#14-wskaźniki).
- Funkcja z `-> typ` musi zwrócić wartość na każdej ścieżce. Finch to sprawdza.
- Funkcji nie można definiować wewnątrz innej funkcji.
- Funkcję, która może się nie udać, zapisuje się `-> int!` (albo `-> !` bez wyniku); zobacz
  [Błędy jako wartości](#błędy-jako-wartości).
- Długie wywołania możesz rozbić na kilka linijek wewnątrz nawiasów; przecinek po ostatnim argumencie jest dozwolony.

---

## 10. Tablice i mapy

```c
liczby := [5, 3, 8]        // tablica int
[]str imiona               // pusta tablica str
siatka := [[1, 2], [3, 4]] // tablice tablic
[]f32 xs = [1, 2.5]        // typ decyduje, czym staną się liczby
```

| Operacja | Znaczenie |
|---|---|
| `a.len` | liczba elementów |
| `a[i]` | element `i` (od 0); poza zakresem program zatrzymuje się z czytelnym komunikatem |
| `a[i] = x` | zmiana elementu |
| `a.push(x)` | dodaj na końcu |
| `a.pop()` | zabierz ostatni i go oddaj |
| `a.insert(i, x)` | wstaw `x` na pozycję `i`, przesuwając resztę |
| `a.remove(i)` | usuń pozycję `i` i ją oddaj |
| `a.clear()` | usuń wszystko |
| `a.resize(n)` | zmień długość na `n` (nowe elementy to zera) |
| `a.contains(x)`, `a.find(x)` | czy `x` jest w środku? jego indeks albo -1 (liczby, znaki, bool, str) |
| `a.sort()`, `a.reverse()` | w miejscu (liczby, znaki, str) |
| `a.slice(s, e)` | nowa tablica z elementami `s` … `e-1` |
| `a.join(sep)` | dla `[]str`: jeden tekst z `sep` między częściami |
| `a.ptr` | adres pierwszego elementu (dla C) |
| `print(a)` | wypisuje `[1, 2, 3]` |

**Przypisanie kopiuje:** po `b := a` zmiana `b` nie zmienia `a`.

### Mapy

`map[K]V` znajduje wartości po kluczu. Kluczem może być liczba całkowita, `char`, `bool` albo `str`;
wartością cokolwiek (tablice, struktury, inne mapy). Przechodzenie pętlą idzie w kolejności, w jakiej
klucze zostały dodane po raz pierwszy.

```c
wiek := ["anna": 31, "bob": 25]     // literał mapy: map[str]int
map[str][]str grupy                 // pusta mapa
map[int]str nazwy = [:]             // [:] to pusta mapa tam, gdzie typ jest znany
```

| Operacja | Znaczenie |
|---|---|
| `m[k]` | wartość dla `k`; brak klucza zatrzymuje program komunikatem `the key "k" is not in the map` |
| `m[k] = v` | dodaj albo zastąp |
| `m[k] += 1`, `m[k].push(x)`, `m[k].pole = …` | zmiana brakującego klucza najpierw go dodaje, z wartością domyślną typu (0, `""`, `[]`, …) |
| `m.len` | liczba kluczy |
| `m.has(k)` | czy `k` jest w mapie? |
| `m.get(k, domyslna)` | wartość albo `domyslna` (liczona tylko w razie potrzeby) |
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

Wyszukiwanie używa haszowania (średnio stały czas). Tak jak tablice, mapy są właścicielami kluczy
i wartości, kopiują się przy przypisaniu i zwalniają na końcu bloku. Nie dodawaj ani nie usuwaj kluczy
mapy wewnątrz `for` po tej samej mapie; zbierz je do tablicy i zmień mapę po pętli.

---

## 11. Tekst (str)

```c
s := "Cześć"
s += ", świecie"          // sklejanie
print(s.len, s[0])        // .len liczy bajty; ą, ś, ż… zajmują po 2
s[0] = 'D'                // zmiana znaku
```

| Metoda | Oddaje |
|---|---|
| `s.sub(a, b)` | fragment od `a` do `b-1` |
| `s.find(t)` | gdzie zaczyna się `t`, albo -1 |
| `s.contains(t)`, `s.starts_with(t)`, `s.ends_with(t)` | `bool` |
| `s.split(sep)` | `[]str`; `split("")` daje pojedyncze znaki |
| `s.trim()` | bez spacji i nowych linii na obu końcach |
| `s.upper()`, `s.lower()` | zmienione litery A–Z / a–z (tylko ASCII) |
| `s.replace(a, b)` | każde `a` zamienione na `b` |
| `s.repeat(n)` | `s` n razy |
| `s.bytes()` | `[]u8` z bajtami |
| `s.ptr` | `ptr[char]` dla C (ważny, dopóki żyje `s`) |

`str` pamięta swoją długość, więc `.len` działa natychmiast. Zawsze kończy się zerem, więc
przekazanie go do C jako `char*` działa.

---

## 12. Struktury i metody

```c
struct Gracz {
    str imie
    int zycia = 3            // wartość domyślna
    []int wyniki
    Punkt pozycja            // struktury w strukturach
}

g := Gracz(imie: "Ola")                      // po nazwie: brakujące pola dostają domyślne (albo zero)
q := Punkt(1, 2)                             // po kolei: wtedy trzeba podać każde pole
g.zycia -= 1
g.wyniki.push(10)
g.pozycja.x = 5
print(g)      // Gracz(imie: "Ola", zycia: 2, wyniki: [10], pozycja: Punkt(x: 5, y: 0))
```

- Przypisanie albo przekazanie struktury ją kopiuje, razem z jej tablicami i tekstami.
- Struktura nie może zawierać samej siebie wprost; dla takiego pola użyj `ptr[Wezel]` albo `[]Wezel` (drzewa, listy).

### Metody

Metoda to funkcja należąca do struktury: `fn Struktura.nazwa(...)`. W środku `self` to struktura,
na której ją wywołano, i to jest sama wartość wywołującego, nie kopia: zmiany w `self` zostają.

```c
fn Gracz.wylecz(int ile) {
    self.zycia += ile
}

fn Gracz.zyje() -> bool {
    return self.zycia > 0
}

g := Gracz(imie: "Ola")
g.wylecz(2)                // g.zycia wynosi teraz 5
if g.zyje() { ... }

druzyna[0].wylecz(1)       // na elemencie tablicy, polu, m[klucz], przez ptr[Gracz] ...
```

- Metoda jest w tym samym pliku (module) co jej struktura i jest dostępna wszędzie, gdzie ta struktura.
- Metody można też dopisywać do **struktur z nagłówków C**: `fn Vector2.length() -> f32 { ... }`.
- Metoda nie może nazywać się tak jak pole.
- Wywołanie metody zmieniającej `self` na zmiennej pętli (`for g in druzyna { g.wylecz(1) }`) to błąd,
  bo zmienna pętli jest tylko do odczytu; użyj `for i in 0..druzyna.len { druzyna[i].wylecz(1) }`.
- Tak jak funkcje, metody mogą się nie udać (`-> !`); zobacz [Błędy jako wartości](#błędy-jako-wartości).

---

## 13. Pamięć: kto co zwalnia

Tablic, tekstów i struktur nie zwalniasz nigdy. Finch robi to za ciebie, w przewidywalnym momencie:
**gdy kończy się blok, do którego wartość należy** (`}`), albo wcześniej przy `return`, `break`, `continue`.

- Zmienna **jest właścicielem** swojej wartości. Przypisanie kopiuje, więc dwie zmienne nigdy nie dzielą jednej listy.
- Wartość z wywołania funkcji albo wyrażenia jest **przenoszona** tam, gdzie ją zapisujesz, bez kopiowania.
- `return lista` przenosi zmienną poza funkcję, bez kopiowania.
- Parametry funkcji są **pożyczane**: funkcja dostaje wartość wywołującego bez kopii. Jeśli ją zmienia,
  Finch najpierw robi jej prywatną kopię. Zauważysz to najwyżej po szybkości.

Do pamięci zarządzanej samodzielnie (listy wiązane, drzewa, dzielenie między strukturami):

```c
wezel := new(Wezel(wartosc: 1))  // wstaw wartość na stertę, dostajesz ptr[Wezel]
defer free(wezel)                // free(...) ją oddaje (razem z jej tablicami i tekstami)
```

Pamięć z C (`malloc`, obiekty bibliotek) zwalniasz funkcjami z C, jak w C.

---

## 14. Wskaźniki

Wskaźnik przechowuje **adres** wartości. Finch zamiast `*` i `&` z C używa słów:

| C | Finch | Znaczenie |
|---|---|---|
| `int *p` | `ptr[int] p` | p wskazuje na int |
| `&x` | `addr(x)` | adres x (także `addr(a[i])`, `addr(s.pole)`) |
| `*p` | `p.value` | wartość, na którą wskazuje p |
| `p->pole` | `p.pole` | pole struktury, na którą wskazuje p |
| `p[i]` | `p[i]` | indeksowanie jak w C (bez kontroli zakresu) |
| `NULL` | `null` | wskazuje donikąd |
| `void *` | `ptr` | wskazuje na coś nieznanego typu |

```c
fn podwoj(ptr[int] p) {
    p.value = p.value * 2
}

x := 21
podwoj(addr(x))           // x to 42
```

Jeśli struktura ma własne pole o nazwie `value`, to `p.value` na wskaźniku do niej oznacza to pole.

Użycie `.value`, `.pole` albo `[i]` na wskaźniku `null` **zatrzymuje program** z czytelnym komunikatem.

⚠️ Wskaźnika na zmienną nie wolno używać po zakończeniu jej bloku. Finch tego nie sprawdza.

---

## 15. Funkcje wbudowane

| Funkcja | Co robi |
|---|---|
| `print(a, b, ...)` | Wypisuje wszystkie wartości oddzielone spacjami i przechodzi do nowej linii. Działa z każdym typem, także z tablicami i strukturami. |
| `input()`, `input("pytanie")` | Czyta jedną linijkę wpisaną przez użytkownika (bez znaku nowej linii). Na końcu wejścia daje pusty tekst. |
| `read_file(sciezka)` | Cały plik jako `str`. Jeśli nie da się go przeczytać, zatrzymuje program z czytelnym komunikatem, chyba że obsłużysz to: `read_file(p) or ...`. |
| `write_file(sciezka, tekst)` | Zapisuje (zastępuje) plik; oddaje `true`, jeśli się udało. Z `or` / `try` porażka niesie przyczynę. |
| `file_exists(sciezka)` | `true` / `false` |
| `delete_file(sciezka)` | Usuwa plik; `true`, jeśli się udało (albo przyczyna, z `or` / `try`). |
| `error(komunikat)` | Porażka, którą zwraca funkcja mogąca się nie udać: `return error("...")`. |
| `shell(polecenie)` | Uruchamia polecenie powłoki i oddaje jego kod wyjścia. |
| `exit(kod)` | Natychmiast kończy program. |
| `addr(x)` | Wskaźnik na `x`; `addr(funkcja)` daje C wskaźnik na funkcję (callback). |
| `new(wartosc)`, `free(p)` | Ręczna pamięć na stercie (zobacz [Pamięć](#13-pamięć-kto-co-zwalnia)). |
| `int(x)`, `str(x)`, `u8(x)`, … | Zamiany typów (zobacz [Zamiana typów](#zamiana-typów)). `str(x)` działa na każdym typie i daje ten sam tekst, co `print`. `int(tekst) or 0` obsługuje tekst, który nie jest liczbą. |

---

## 16. Moduły

Program możesz podzielić na pliki. `import ksztalty` wczytuje `ksztalty.fch` z tego samego folderu
(albo z folderu wymienionego w zmiennej środowiskowej `FINCH_PATH`, rozdzielonych `:`).

```c
// ksztalty.fch
struct Pudlo {
    float w
    float h
}

fn pole(Pudlo p) -> float {
    return p.w * p.h
}
```

```c
// main.fch
import ksztalty

fn main() {
    p := ksztalty.Pudlo(2, 3)
    print(ksztalty.pole(p))
    ksztalty.Pudlo inne      // typ z modułu w deklaracji
}
```

- Wszystkiego z modułu używasz z jego nazwą z przodu: `ksztalty.pole`, `ksztalty.Pudlo`.
- Moduł nie ma `main`. Moduły mogą importować inne moduły.

---

## 17. Biblioteki z C

### Krok 1: zaimportuj nagłówek

```c
import "stdio.h"
import "GLFW/glfw3.h"     // ścieżka w systemowych katalogach nagłówków
import "mojalib.h"        // nagłówek obok twojego pliku .fch
```

Finch czyta nagłówek przez **libclang** (parser C z clanga) i udostępnia:

- **funkcje**, także te ze zmienną liczbą argumentów, jak `printf`,
- **struktury**, przez wartość i przez wskaźnik (`Vector2`, `SDL_Rect`, `CXCursor`, …), razem z polami,
- **wartości `enum`** i **proste stałe `#define`** (`M_PI`, `GL_COLOR_BUFFER_BIT`),
- **zmienne globalne**, np. `stdout` i `stderr`.

### Krok 2: dolinkuj bibliotekę

```c
link "glfw"              // używa libglfw.so (najpierw pyta pkg-config, jeśli jest)
link "pomocnicze.c"      // skompiluj i dołącz własny plik C (obok pliku .fch)
link "gotowe.a"          // albo plik obiektowy / bibliotekę statyczną
```

Jeśli zapomnisz linijki `link`, Finch powie dokładnie, czego brakuje:

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
| `enum` | odpowiadająca liczba całkowita ze znakiem |
| `char *` jako **parametr albo wynik** | `str` |
| `char *` w strukturze albo za wskaźnikiem | `ptr[char]` (czytasz przez `str(p)`) |
| `struct X` przez wartość | `X`, struktura z tymi samymi polami |
| `struct X *`, `int *`, … | `ptr[X]`, `ptr[i32]`, … |
| `int arr[16]` w strukturze | `[16]i32`: indeksujesz i używasz `.len` |
| `void *`, wskaźniki na funkcje, niekompletne struktury | `ptr` |

### Przykład: struktury i raylib

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

Żeby dać C funkcję do wywołania zwrotnego (np. porównywarkę dla `qsort`), użyj `addr(mojaFunkcja)`.
Jej parametry muszą być typami C (liczby, wskaźniki, struktury C).

### Grafika: OpenGL i Vulkan

Oba działają bezpośrednio; `examples/opengl.fch` i `examples/vulkan.fch` to kompletne programy.

- **OpenGL 3+** na Linuksie: nowoczesne funkcje (`glCreateShader`, `glGenVertexArrays`, …) są deklarowane
  dopiero po zdefiniowaniu `GL_GLEXT_PROTOTYPES`. Finch nie definiuje makr C, więc dołączenia umieść w małym
  nagłówku obok programu i zaimportuj go:

  ```c
  // opengl.h
  #define GL_GLEXT_PROTOTYPES
  #include <GL/gl.h>
  #include <GL/glext.h>
  #include <GLFW/glfw3.h>
  ```

  Potem `import "opengl.h"`, `link "glfw"`, `link "GL"`. Parametry wyjściowe przekazujesz przez `addr(...)`
  (`glGenBuffers(1, addr(vbo))`), dane bufora przez `.ptr` (`vertices.ptr`), a przesunięcia w bajtach przez
  `ptr(...)` (`glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, ptr(8))`).
- **Vulkan**: `import "vulkan/vulkan.h"` i `link "vulkan"`. Struktury „create info” budujesz z nazwanymi polami
  (brakujące są zerami), uchwyty to `ptr`, a listy pobierasz typowym wzorcem „dwóch wywołań”: raz po liczbę,
  raz z `lista.ptr` po `lista.resize(liczba)`. Makra-funkcje, jak `VK_MAKE_API_VERSION`, nie są dostępne;
  policz liczbę sam (`1 << 22` to Vulkan 1.0).

### Co jeszcze nie działa

- **Unie** C przez wartość i struktury z **polami bitowymi** przez wartość (wskaźniki do nich działają).
- **Makra-funkcje** i makra, które nie są pojedynczą liczbą.
- `long double`, liczby 128-bitowe.

---

## 18. Błędy

### Błędy jako wartości

Niektóre porażki są normalne: użytkownik wpisze `abc`, pliku nie ma. Finch obsługuje je bez wyjątków
i bez ukrytych skoków. Funkcja, która może się nie udać, mówi to znakiem `!` po typie wyniku i kończy się
porażką przez `return error("komunikat")`:

```c
fn port_z_tekstu(str tekst) -> int! {
    n := int(tekst) or { return error("'" + tekst + "' to nie liczba") }
    if n < 1 || n > 65535 {
        return error("port " + str(n) + " jest poza zakresem")
    }
    return n
}

fn zapisz(str sciezka, str tekst) -> ! {   // może się nie udać, nic nie oddaje
    try write_file(sciezka, tekst)
}
```

Wywołujący **musi** obsłużyć porażkę, na jeden z trzech sposobów (zapomnienie to błąd kompilacji):

| Kod | W razie porażki |
|---|---|
| `port := port_z_tekstu(s) or 8080` | użyj tej wartości (liczonej tylko przy porażce; może to być kolejne wywołanie z `or`) |
| `port := port_z_tekstu(s) or { print(err); return }` | wykonaj blok; `err` to komunikat (`str`). Blok musi wyjść (`return`, `break`, `continue`, `exit`), chyba że wynik nie jest używany. |
| `port := try port_z_tekstu(s)` | zakończ bieżącą funkcję tym samym błędem (musi też móc się nie udać) |

```c
fn wczytaj(str sciezka) -> Konfig! {
    tekst := try read_file(sciezka)
    port := try port_z_tekstu(tekst.trim())
    return Konfig(port: port)
}

fn main() -> ! {                // main też może się nie udać: błąd zostanie wypisany, a kod wyjścia to 1
    konfig := try wczytaj("app.conf")
    ...
}
```

Funkcje wbudowane, które mogą się nie udać, działają tak samo: `int(tekst)`, `float(tekst)`, `read_file`,
`write_file`, `delete_file`. Bez `or` / `try` zachowują się jak wcześniej (`int("x")` zatrzymuje program,
`write_file` oddaje `bool`).

Pod spodem funkcja, która może się nie udać, zwraca wartość z flagą i komunikatem: bez alokacji przy
sukcesie, bez zwijania stosu, a porażka kosztuje tyle, co zbudowanie komunikatu.

### Błędy kompilacji

```
gra.fch:12:9: error: 'wynik' must be int, but this is str
   12 |     wynik = "wysoki"
      |             ^
```

Format: `plik:linijka:kolumna: error: wyjaśnienie`, a pod spodem linijka ze znakiem `^`.
Komunikaty są po angielsku.

### Błędy w trakcie działania

Zamiast niezdefiniowanego zachowania (jak w C) Finch zatrzymuje się z komunikatem i kodem wyjścia 1:

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
| `runtime error: called .f() on a null pointer` | metoda wywołana przez `null` w `ptr[T]` |

---

## 19. Edytory: VS Code, Kate i inne

Wszystkie funkcje edytorów pochodzą z jednego serwera języka, `finch lsp`, który używa prawdziwego
kompilatora: błędy na bieżąco, typy i opisy po najechaniu myszą, idź do definicji, podpowiedzi (pola,
metody, metody map i tablic po `.`, zawartość modułów, nazwy z nagłówków C), konspekt pliku i podpowiedzi
parametrów.

### Kate (oraz KWrite, KDevelop)

```sh
editors/kate/install.sh
```

Instaluje podświetlanie (`~/.local/share/org.kde.syntax-highlighting/syntax/finch.xml`) i dopisuje `finch`
do ustawień klienta LSP w Kate, zachowując twoje pozostałe serwery. Uruchom Kate ponownie i włącz wtyczkę
**LSP Client**. Żeby uruchamiać programy, dodaj `finch run %f` jako cel we wtyczce **Build & Run**; komunikaty
Fincha `plik:linia:kolumna: error:` są wtedy klikalne. Szczegóły: `editors/kate/README.md`.

### VS Code

Rozszerzenie **Finch** (`editors/vscode`, dołączane też do każdego wydania jako plik `.vsix`) daje:
podświetlanie składni, błędy na bieżąco, podpowiedzi (także pola i metody po `.`), typy po najechaniu
myszą, idź do definicji (F12), konspekt pliku, podpowiedzi parametrów, szablony i przycisk ▶ **Run**
(`Ctrl+F5`). Błędy z **Finch: Build This File** trafiają do panelu Problems.

Instalacja: Rozszerzenia → `…` → *Install from VSIX…* → wybierz `finch-lang-*.vsix`. Rozszerzenie uruchamia
`finch` z PATH; jeśli jest gdzie indziej, ustaw **Finch: Path** w ustawieniach.

### Inne

Neovim, Helix, Zed, Emacs, Sublime i każdy inny edytor obsługujący Language Server Protocol:
uruchamiaj `finch lsp` dla plików `.fch` (stdin/stdout, bez opcji).

## 20. Debugowanie

```sh
finch build gra.fch -g -O0 -o gra
gdb ./gra
(gdb) break gra.fch:12
(gdb) run
(gdb) bt                # gdzie jesteśmy, przez które funkcje
(gdb) info locals       # wszystkie zmienne
(gdb) print gracz.zycia
```

`-g` dodaje informacje dla debuggera (linijki, funkcje, zmienne, pola struktur). `-O0` zostawia wszystkie
zmienne widoczne; bez niego optymalizator może część z nich usunąć. Na Windowsie `-g` zapisuje format
CodeView, który czyta debugger Visual Studio i WinDbg.

---

## 21. Rozwiązywanie problemów

| Problem | Rozwiązanie |
|---|---|
| `can't find the C header 'x.h'` | Brakuje plików deweloperskich biblioteki. Zainstaluj je (Debian: `libx-dev`; Nix: dopisz do `shell.nix`). |
| `… come from "x.h", but its library isn't linked` | Dopisz `link "nazwa"` na górze pliku. |
| `the library 'x' wasn't found` | Nie jest zainstalowana albo nazwa jest zła. Na Niksie dopisz ją do `shell.nix` i pracuj w `nix-shell`. |
| `the C function 'f' can't be used from Finch yet` | Używa unii, pól bitowych albo innego nieobsługiwanego typu przez wartość. Poszukaj wariantu ze wskaźnikami. |
| `can't find the module 'x'` | Połóż `x.fch` obok importującego pliku albo ustaw `FINCH_PATH`. |
| `couldn't build the Finch runtime` | Brak działającego kompilatora C. Zainstaluj gcc lub clang albo ustaw `CC`. |
| Biblioteka nie znajduje się przy starcie programu | Zlinkowano ją z folderu, którego system nie przeszukuje. Uruchamiaj w tym samym `nix-shell` albo zainstaluj ją systemowo. |
| Windows: `finch` nie linkuje (błędy `LNK…` o `libcmt`, `kernel32`) | Zainstaluj Visual Studio Build Tools z *Desktop development with C++*. |
| Windows: `finch.exe` się nie uruchamia (brak `libclang.dll`) | Trzymaj `libclang.dll` obok `finch.exe`. |
| macOS: `finch` się nie uruchamia (`Library not loaded: …libLLVM…`) | `brew install llvm@21`. |
| macOS: `can't find the C header 'stdio.h'` | Zainstaluj narzędzia wiersza poleceń: `xcode-select --install`. |
| `--target arm64`: `no C compiler found to link with` | Zainstaluj `aarch64-linux-gnu-gcc` (Debian: `gcc-aarch64-linux-gnu`) albo ustaw `FINCH_CC`. |
| `clang` w `nix-shell` nie widzi `stdio.h` | Masz stary `shell.nix`: `llvmPackages.clang` musi być przed `llvmPackages.libclang`. |
| Program nigdy się nie kończy | Warunek pętli nigdy nie staje się fałszywy. Naciśnij Ctrl+C. |

---

## 22. Struktura projektu i testy

```
Finch/
├── src/            kompilator (C++)
├── runtime/        finch_rt.c: mała biblioteka uruchomieniowa (teksty, tablice, wejście, pliki)
├── boot/           kompilator Fincha napisany w Finchu (zobacz przewodnik dla inżynierów)
├── examples/       hello, tour, types, structs, todo, guess, c_import, window, opengl, vulkan, raylib, llvm
├── tests/
│   ├── run/        programy + dokładny wynik (.out) i wejście (.in)
│   ├── fail/       programy, które muszą się nie udać, z oczekiwanym błędem w 1. linijce
│   ├── run.sh      uruchamia testy (MEMCHECK=1 sprawdza też pamięć valgrindem)
│   └── boot.sh     buduje kompilator samohostujący nim samym i porównuje
├── editors/        rozszerzenie VS Code (vscode/), podświetlanie i konfiguracja LSP dla Kate (kate/)
├── docs/           ta dokumentacja (en, pl), łącznie ze ściągą
├── shell.nix       środowisko deweloperskie Nix
└── CMakeLists.txt
```

```sh
tests/run.sh                 # → 74 passed, 0 failed
MEMCHECK=1 tests/run.sh      # to samo pod valgrindem: bez wycieków i złych dostępów do pamięci
tests/boot.sh                # sprawdzenie samohostowania (wymaga clanga)
tests/cross.sh windows       # każdy test zbudowany na Windowsa i uruchomiony w Wine
tests/cross.sh arm64         # każdy test zbudowany na Linuksa ARM64 i uruchomiony w QEMU
python3 tests/lsp_test.py    # serwer języka
```

---

## 23. Obecne ograniczenia

Jeszcze nie ma:

- Typów generycznych (własnego `Lista[T]`), interfejsów, `match`, enumów pisanych w Finchu.
- Tekstu świadomego Unicode: `.len`, `s[i]` i `upper()` działają na bajtach / ASCII.
- Wątków.
- Windowsa na ARM64, systemów 32-bitowych, WebAssembly.

Plan rozwoju jest w [README](../../README.pl.md#plan-rozwoju).
