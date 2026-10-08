# Finch dla techników

**Dla osób, które dobrze znają komputery, ale programować nie umieją albo umieją niewiele.**
Ten przewodnik opisuje instalację Fincha, obsługę polecenia `finch`, cały język,
korzystanie z bibliotek C i rozwiązywanie typowych problemów. Nie opisuje, jak kompilator
działa w środku. Do tego służy [przewodnik dla inżynierów](dla-inzynierow.md).

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
10. [Wskaźniki](#10-wskaźniki)
11. [Biblioteki z C](#11-biblioteki-z-c)
12. [Błędy](#12-błędy)
13. [Rozwiązywanie problemów](#13-rozwiązywanie-problemów)
14. [Struktura projektu i testy](#14-struktura-projektu-i-testy)
15. [Obecne ograniczenia](#15-obecne-ograniczenia)

---

## 1. Czym jest Finch

Finch to język **kompilowany**. Narzędzie `finch` tłumaczy plik `.fn` na prawdziwy program
(natywny plik wykonywalny, taki sam jak te z C). Korzysta przy tym z **LLVM**, tego samego
zaplecza kompilatorów, na którym działają clang, Rust i Swift.

Co to daje:

- **Szybkość zbliżoną do C.** Nie ma interpretera, maszyny wirtualnej ani garbage collectora.
- **Małe, samodzielne programy**, które można skopiować i uruchomić.
- **Bezpośredni dostęp do bibliotek C.** Piszesz `import "stdio.h"` i wołasz, co chcesz.

Zasady projektu: jeden sposób na każdą rzecz, składnia podobna do C bez jej pułapek
i komunikaty błędów, które mówią, jak problem naprawić.

---

## 2. Instalacja

Finch działa obecnie na **Linuksie x86-64**. Testowany był z **LLVM 21**.
Z innymi wersjami LLVM prawdopodobnie się nie skompiluje, bo API C++ LLVM zmienia się między wydaniami.

### Opcja A: Nix (zalecana, nic nie instalujesz ręcznie)

```sh
git clone https://github.com/Nexoniarz/Finch.git
cd Finch
nix-shell                        # pobiera LLVM, libclang, cmake, ninja, pkg-config
cmake -S . -B build -G Ninja
ninja -C build
./build/finch version            # finch 1.0.0 (LLVM 21.x)
```

### Opcja B: pakiety z twojej dystrybucji

Potrzebujesz: kompilatora C/C++, CMake ≥ 3.20, Ninja (albo Make), plików deweloperskich LLVM 21
i libclang 21, a opcjonalnie `pkg-config`.

| Dystrybucja | Pakiety (nazwy mogą się nieco różnić zależnie od wersji) |
|---|---|
| Debian / Ubuntu | `build-essential cmake ninja-build pkg-config llvm-21-dev libclang-21-dev` |
| Fedora | `gcc-c++ cmake ninja-build pkgconf llvm-devel clang-devel` |
| Arch | `base-devel cmake ninja pkgconf llvm clang` |

Jeśli CMake nie znajduje LLVM, wskaż mu katalog:

```sh
cmake -S . -B build -G Ninja -DLLVM_DIR=/usr/lib/llvm-21/lib/cmake/llvm
ninja -C build
```

### Dodanie `finch` do PATH (opcjonalnie)

```sh
sudo cp build/finch /usr/local/bin/
finch version
```

Podczas pracy Finch potrzebuje działającego kompilatora C (`cc`). Używa go jako **linkera**,
który skleja skompilowany kod w gotowy program. Inny kompilator wskażesz zmienną środowiskową `CC`.

---

## 3. Polecenie finch

```
finch run   <plik.fn>              skompiluj i od razu uruchom
finch build <plik.fn> [-o nazwa]   skompiluj do programu (domyślna nazwa: nazwa pliku)
finch ir    <plik.fn>              pokaż LLVM IR (dla ciekawskich)
finch version                      pokaż wersję

opcje:
  -l <lib>   dolinkuj bibliotekę C, to samo co  link "lib"  w pliku
  -O0        bez optymalizacji
```

Przykłady:

```sh
finch run hello.fn                 # wypisuje wynik
finch build gra.fn -o mojagra      # tworzy ./mojagra
./mojagra
finch run okno.fn -l glfw          # z biblioteką C
```

`finch run` przekazuje dalej kod wyjścia twojego programu. Jeśli program się wysypie,
powie jak (np. `the program crashed: Segmentation fault`).

---

## 4. Jak wygląda plik Fincha

```c
// komentarz zaczyna się od dwóch ukośników
/* albo zajmuje
   kilka linijek */

import "math.h"          // nagłówki C (opcjonalne), zawsze na górze
link "m"                 // biblioteki C do dolinkowania (opcjonalne)

fn main() {              // tu zaczyna się program
    print("Cześć")
}
```

Zasady, które warto znać:

- **Jedna instrukcja na linijkę.** Średników nie ma. Długie wyrażenie możesz złamać
  **po** operatorze (`a +⏎ b`) albo w dowolnym miejscu wewnątrz nawiasów `( )`.
- **Klamry są zawsze obowiązkowe** po `if`, `else`, `for`, `while`, a otwierająca `{` stoi w tej samej linijce.
- **Bez nawiasów wokół warunków:** `if x > 5 {`, a nie `if (x > 5) {`.
- Funkcje mogą być w pliku w dowolnej kolejności.
- `main` to `fn main()` albo `fn main() -> int` (zwraca kod wyjścia).

---

## 5. Zmienne

```c
x := 5               // nowa zmienna, typ zgadnięty (int)
int y = 10           // nowa zmienna, typ podany jawnie
int z                // bez wartości: startuje od zera ("" dla str, null dla wskaźników)

x = 7                // przypisanie
x += 1               // także -= *= /= %=
```

- Nazwę można zadeklarować **raz** w funkcji. Finch nie pozwala użyć tej samej nazwy ponownie
  w bloku wewnętrznym (nie ma „przesłaniania”), bo to częste źródło pomyłek.
- Zmienna istnieje od deklaracji do końca bloku `{ }`, w którym powstała.
- Nazwy: litery ASCII, cyfry i `_`, bez cyfry na początku. Polskie litery tylko w tekstach w cudzysłowie.
  Nazwy typów (`int`, `u8`, …) i słowa kluczowe są zarezerwowane.

**Słowa kluczowe:** `fn return if else while for in break continue true false null import link`

---

## 6. Typy

### Typy na co dzień

| Typ     | Przechowuje | Przykład |
|---------|-------------|----------|
| `int`   | liczba całkowita, 64-bitowa (to samo co `i64`) | `42`, `-7`, `0xFF`, `1_000_000` |
| `float` | liczba z przecinkiem, 64-bitowa (to samo co `f64`) | `3.14` |
| `bool`  | `true` albo `false` | |
| `char`  | jeden znak wielkości bajtu | `'A'`, `'\n'` |
| `str`   | tekst (napis w stylu C) | `"cześć\tświecie"` |

Znaki specjalne w tekstach: `\n` nowa linia, `\t` tabulator, `\r`, `\0`, `\\`, `\"`, `\'`.

### Typy z rozmiarem (do plików, sprzętu, bibliotek C)

| Ze znakiem | Zakres | Bez znaku | Zakres |
|---|---|---|---|
| `i8`  | −128 … 127 | `u8`  | 0 … 255 |
| `i16` | −32 768 … 32 767 | `u16` | 0 … 65 535 |
| `i32` | ok. ±2,1 mld | `u32` | 0 … ok. 4,29 mld |
| `i64` | ok. ±9,2 × 10¹⁸ | `u64` | 0 … ok. 1,8 × 10¹⁹ |

`f32` to 32-bitowa liczba z przecinkiem (ok. 7 cyfr znaczących), `f64` to 64-bitowa (ok. 15–16).

### Wskaźniki

`ptr[T]` wskazuje na wartość typu `T`. Samo `ptr` wskazuje na „coś” nieznanego typu,
jak `void*` w C. Szczegóły w rozdziale [Wskaźniki](#10-wskaźniki).

### Zamiana typów

Finch zamienia typy sam **tylko wtedy, gdy nic nie może zginąć**:

| Z | Na | Automatycznie? |
|---|---|---|
| mniejsza liczba całkowita | większa o tej samej „znakowości” (`i8`→`i32`, `u8`→`u64`) | ✅ |
| bez znaku | ściśle większa ze znakiem (`u8`→`i16`, `u32`→`i64`) | ✅ |
| dowolna liczba całkowita | `float` / `f32` | ✅ |
| `f32` | `f64` | ✅ |
| `null` | dowolny wskaźnik | ✅ |
| dowolny wskaźnik, `str` | `ptr` | ✅ |
| `ptr` | dowolny `ptr[T]` | ✅ |
| wszystko inne | | ❌ napisz to jawnie: `typ(wartość)` |

**Liczby wpisane w kod się dopasowują.** Liczba napisana wprost w kodzie pasuje do każdego typu,
który ją pomieści: `u8 b = 200` działa, a `u8 b = 300` to błąd (`the number 300 doesn't fit in u8`).
To samo dotyczy stałych z nagłówków C.

**Jawna zamiana** wygląda jak wywołanie funkcji o nazwie typu:

```c
int(3.99)      // 3     (obcina część ułamkową)
int(-3.99)     // -3
float(7) / 2   // 3.5
u8(300)        // 44    (zostawia najniższe 8 bitów)
i8(200)        // -56
int('A')       // 65
char(66)       // 'B'
bool(0)        // false (każda liczba różna od zera to true)
ptr(tekst)     // str jako wskaźnik bez typu
str(p)         // wskaźnik jako str
```

**Liczby o stałym rozmiarze się „zawijają”**, jak w prawdziwym sprzęcie: `u8` równe 255 plus 1 daje 0.
Działania liczą się w typie użytych wartości, więc `u8(250) + 10` daje `4`, a nie `260`.
Jeśli potrzebujesz więcej miejsca, najpierw zamień typ: `int(x) + 10`.

---

## 7. Operatory

### Według priorytetu (wyższy wiąże mocniej)

| Priorytet | Operatory | Znaczenie |
|---|---|---|
| 5 | `*` `/` `%` `<<` `>>` `&` | mnożenie, dzielenie, reszta, przesunięcie bitów w lewo/prawo, bitowe AND |
| 4 | `+` `-` `\|` `^` | dodawanie, odejmowanie, bitowe OR, bitowe XOR |
| 3 | `==` `!=` `<` `<=` `>` `>=` | porównania |
| 2 | `&&` | logiczne I |
| 1 | `\|\|` | logiczne LUB |

Jednoargumentowe (przed wartością): `-x` zmiana znaku, `!x` logiczne NIE, `~x` odwrócenie wszystkich bitów.

> **Celowo inaczej niż w C:** operatory bitowe wiążą mocniej niż porównania, więc
> `flagi & 4 != 0` znaczy `(flagi & 4) != 0`. W C to samo po cichu znaczy co innego.

Uwagi:

- `/` na liczbach całkowitych zaokrągla w stronę zera: `7 / 2` to `3`, `-7 / 2` to `-3`. `%` ma znak lewej strony.
- Dzielenie przez zero **zatrzymuje program** komunikatem `runtime error: division by zero` z numerem linijki.
  Jeśli zero jest wpisane wprost w kod, jest to błąd już przy kompilacji.
- `&&` i `||` liczą prawą stronę tylko wtedy, gdy to potrzebne.
- Tekst (`str`) można porównywać przez `==` i `!=` (porównuje zawartość). `<` i `>` na tekstach nie działają.
- Wskaźniki porównuje się przez `==` i `!=`, także z `null`.
- Operatory bitowe wymagają liczb całkowitych. Dla `bool` używaj `&&` i `||`.
- `>>` na liczbach ze znakiem zachowuje znak (`-16 >> 2` to `-4`). Na liczbach bez znaku wypełnia zerami.

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
```

- Warunek musi być typu `bool`. `if x {` z liczbą to błąd; napisz `if x != 0 {`.
- W `for i in a..b` oba końce są liczbami całkowitymi obliczanymi **raz**, przed startem pętli.
  `i` jest typu `int` i nie można go zmieniać wewnątrz pętli.
- `while true { ... }` kręci się do `break` albo `return`.
- Kod po `return`, `break` lub `continue` w tym samym bloku to błąd, bo nigdy się nie wykona.

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

- Parametry są kopiami. Zmiana parametru nie zmienia zmiennej u wywołującego.
  Do tego służą wskaźniki (niżej).
- Funkcja z `-> typ` musi zwrócić wartość na każdej ścieżce. Finch to sprawdza.
- Funkcji nie można definiować wewnątrz innej funkcji.
- Wbudowane nazwy, których nie możesz użyć: `print`, `addr` i wszystkie nazwy typów.

### Funkcje wbudowane

| Funkcja | Co robi |
|---|---|
| `print(a, b, ...)` | Wypisuje wszystkie wartości oddzielone spacjami i przechodzi do nowej linii. Działa z każdym typem. |
| `addr(x)` | Daje wskaźnik na zmienną `x`. |
| `int(x)`, `u8(x)`, `float(x)`, … | Zamienia typy (zobacz [Zamiana typów](#zamiana-typów)). |

---

## 10. Wskaźniki

Wskaźnik przechowuje **adres** wartości, czyli miejsce w pamięci, gdzie ona leży. Finch zamiast
`*` i `&` z C używa słów:

| C | Finch | Znaczenie |
|---|---|---|
| `int *p` | `ptr[int] p` | p wskazuje na int |
| `&x` | `addr(x)` | adres x |
| `*p` | `p.value` | wartość, na którą wskazuje p |
| `NULL` | `null` | wskazuje donikąd |
| `void *` | `ptr` | wskazuje na coś nieznanego typu |

```c
fn podwoj(ptr[int] p) {
    p.value = p.value * 2
}

fn main() {
    x := 21
    podwoj(addr(x))
    print(x)               // 42

    ptr[int] nic = null
    if nic == null {
        print("pusty")
    }
}
```

Wskaźnik może wskazywać na wskaźnik: `ptr[ptr[int]]`, a potem `pp.value.value`.

Użycie `.value` na wskaźniku `null` **zatrzymuje program** komunikatem
`runtime error: used .value on a null pointer`, zamiast losowo go wysypać.

⚠️ Wskaźnika na zmienną nie wolno używać po zakończeniu funkcji, do której ta zmienna należała,
bo zmienna wtedy już nie istnieje. Finch jeszcze tego nie sprawdza.

---

## 11. Biblioteki z C

Prawie każda biblioteka na Linuksie ma interfejs w C: grafika, dźwięk, sieć, bazy danych.
Finch może z nich korzystać bezpośrednio.

### Krok 1: zaimportuj nagłówek

```c
import "stdio.h"
import "math.h"
import "GLFW/glfw3.h"     // ścieżka w systemowych katalogach nagłówków
import "mojalib.h"        // nagłówek leżący obok twojego pliku .fn
```

Finch czyta nagłówek przez **libclang** (parser C z clanga) i udostępnia:

- **funkcje**, także te ze zmienną liczbą argumentów, jak `printf`,
- **wartości `enum`**,
- **proste stałe `#define`**, które są pojedynczą liczbą, np. `M_PI`, `EOF`, `GL_COLOR_BUFFER_BIT`,
- **zmienne globalne**, np. `stdout` i `stderr`.

### Krok 2: dolinkuj bibliotekę

Nagłówek tylko *opisuje* funkcje. Ich właściwy kod jest w pliku biblioteki (`libNAZWA.so`).
Powiedz Finchowi, której biblioteki użyć, na górze pliku:

```c
link "glfw"              // używa libglfw.so
link "GL"                // używa libGL.so
```

Wpisz samą nazwę: dla `libglfw.so` to `glfw`. Jeśli `pkg-config` zna tę nazwę, Finch użyje jego
ustawień (ścieżek i dodatkowych bibliotek). W przeciwnym razie przekaże linkerowi `-lNAZWA`.
Biblioteka standardowa C i matematyczna (`libm`) są dolinkowane zawsze.

Na linii poleceń `-l NAZWA` robi to samo co `link "NAZWA"`.

Jeśli zapomnisz linijki `link`, Finch powie dokładnie, czego brakuje:

```
error: glfwInit, glfwCreateWindow, glfwMakeContextCurrent and 5 more come from "GLFW/glfw3.h", but its library isn't linked
  the header only says the functions exist; their code lives in a library.
  add this at the top of your file (with the library's real name):

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
| `char *`, `const char *` | `str` |
| `int *`, `double *`, … | `ptr[i32]`, `ptr[f64]`, … |
| `void *`, `struct X *`, `FILE *`, wskaźniki na funkcje | `ptr` |

Czyli `int` z C to `i32` w Finchu. Liczby wpisane w kod dopasowują się same (`abs(-5)` działa),
ale zmienną typu `int` z Fincha trzeba zamienić: `abs(i32(x))`.

### Przykład

```c
import "stdio.h"
import "math.h"
import "stdlib.h"

fn main() {
    printf("%d + %d = %d\n", 2, 3, 5)
    print(sqrt(2), M_PI, RAND_MAX)

    ptr[i32] n = malloc(4)       // poproś C o 4 bajty pamięci
    n.value = 7
    print(n.value)
    free(n)                      // i oddaj je

    fprintf(stderr, "to idzie na wyjście błędów\n")
}
```

Pełny przykład z grafiką jest w `examples/window.fn`: okno GLFW + OpenGL, które zmienia kolor.
`examples/llvm.fn` pokazuje, że Finch potrafi nawet sterować samym LLVM przez jego API w C.

### Co jeszcze nie działa

- Funkcje C, które przyjmują lub zwracają **`struct` przez wartość**, np. `div()`. Finch mówi o tym
  wprost, jeśli spróbujesz. Funkcje przyjmujące *wskaźnik* na strukturę działają.
- **Makra-funkcje** (`#define MAX(a,b) ...`) i makra, które nie są pojedynczą liczbą.
- `long double`, liczby 128-bitowe.
- Odczytywanie **pól** struktury C.

---

## 12. Błędy

### Błędy kompilacji

Finch zatrzymuje się na pierwszym problemie i pokazuje, gdzie jest:

```
gra.fn:12:9: error: 'wynik' must be int, but this is str
   12 |     wynik = "wysoki"
      |             ^
```

Format: `plik:linijka:kolumna: error: wyjaśnienie`, a pod spodem linijka ze znakiem `^` pod miejscem błędu.
Komunikaty są po angielsku.

### Błędy w trakcie działania

Niektóre problemy widać dopiero, gdy program działa. Zamiast niezdefiniowanego zachowania
(jak w C) Finch zatrzymuje się z czytelnym komunikatem na wyjściu błędów i kodem wyjścia 1:

| Komunikat | Przyczyna |
|---|---|
| `runtime error: division by zero` | `/` albo `%` przez zmienną równą 0 |
| `runtime error: division overflows …` | najmniejsza liczba ze znakiem podzielona przez −1 |
| `runtime error: used .value on a null pointer` | odczyt lub zapis `p.value`, gdy `p` jest `null` |

---

## 13. Rozwiązywanie problemów

| Problem | Rozwiązanie |
|---|---|
| `can't find the C header 'x.h'` | Brakuje plików deweloperskich biblioteki. Zainstaluj je (Debian: `libx-dev`; Nix: dopisz do `shell.nix`). Własny nagłówek połóż obok pliku `.fn`. |
| `… come from "x.h", but its library isn't linked` | Dopisz `link "nazwa"` na górze pliku. |
| `the library 'x' wasn't found` | Biblioteka nie jest zainstalowana albo nazwa jest zła. Na Niksie dopisz ją do `shell.nix` i pracuj wewnątrz `nix-shell`. |
| `the C function 'f' can't be used from Finch yet` | Używa struktury przez wartość albo innego nieobsługiwanego typu. Poszukaj wariantu, który przyjmuje wskaźniki. |
| `no C compiler found to link with` | Zainstaluj gcc lub clang albo ustaw `CC`. |
| Program się kompiluje, ale przy starcie nie znajduje biblioteki C | Został zlinkowany ze ścieżki, której system nie przeszukuje. Uruchamiaj go w tym samym `nix-shell` albo zainstaluj bibliotekę systemowo. |
| `unexpected character 'ż'` | Polska litera w nazwie. W nazwach używaj tylko a–z. |
| CMake: `libclang not found` | Zainstaluj pakiet deweloperski libclang albo uruchom `cmake` w `nix-shell`. |
| Budowanie kończy się błędami API LLVM | Masz inną wersję LLVM niż 21. |
| Program nigdy się nie kończy | Warunek pętli nigdy nie staje się fałszywy. Naciśnij Ctrl+C. |

---

## 14. Struktura projektu i testy

```
Finch/
├── src/            kompilator (C++)
├── examples/       przykładowe programy (hello, tour, types, c_import, window, llvm)
├── tests/
│   ├── run/        programy + dokładny wynik, który muszą wypisać (.out)
│   ├── fail/       programy, które muszą się nie udać, z oczekiwanym błędem w 1. linijce
│   └── run.sh      skrypt uruchamiający testy
├── docs/           ta dokumentacja (en, pl)
├── shell.nix       środowisko deweloperskie Nix
└── CMakeLists.txt
```

Testy uruchomisz po zbudowaniu:

```sh
tests/run.sh             # → 34 passed, 0 failed
```

---

## 15. Obecne ograniczenia

Finch 1.0 to solidny rdzeń, a nie skończony język. Jeszcze nie ma:

- `struct`, tablic, czytania z klawiatury (`input()`), sklejania tekstów (`"a" + "b"`).
  Na razie użyj do tego funkcji z C (`scanf`, `snprintf`, `malloc`…).
- Automatycznego zarządzania pamięcią (w planach: zwalnianie na końcu bloku i `defer`).
- Modułów Fincha (na razie jeden program = jeden plik `.fn`).
- Platform innych niż Linux x86-64.

Plan rozwoju jest w [README](../../README.pl.md#plan-rozwoju).
