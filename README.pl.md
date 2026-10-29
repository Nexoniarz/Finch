<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/logo-dark.svg">
    <img alt="Finch" src="assets/logo.svg" width="460">
  </picture>
</p>

**Mały, zwinny, skuteczny.** Język programowania na LLVM ze składnią podobną do C,
na tyle prosty, że nauczysz się go w godzinę, i tak szybki jak C.

> 🇬🇧 [English version](README.md)

```c
import "math.h"

fn przeciwprostokatna(float a, float b) -> float {
    return sqrt(a * a + b * b)
}

fn main() {
    imie := "Finch"
    print("Cześć od", imie, "!")

    for i in 1..4 {
        print(i, "→", przeciwprostokatna(float(i), 1.0))
    }
}
```

## Dlaczego Finch

- **Prosty.** Bez średników, bez plików nagłówkowych, bez preprocesora. Każdą rzecz robi się na jeden sposób.
- **Szybki.** Kompiluje się przez potok `-O2` z LLVM do kodu natywnego, bez garbage collectora.
  Rekurencyjne `fib(40)` trwa ok. 0,19 s (`clang -O2` na tym samym kodzie C: ok. 0,23 s), a kod na tablicach
  z kontrolą zakresu dorównuje C.
- **Pamięć bez bólu.** Listy, teksty i struktury są wartościami: przypisanie kopiuje, a koniec bloku je zwalnia.
  Nie ma `free`, o którym można zapomnieć, nie ma użycia po zwolnieniu, a każdy test jest czysty pod valgrindem.
- **Bezpieczniejszy niż C tam, gdzie to tanie.** Nie ma cichego zwężania typów ani mieszania liczb ze znakiem
  i bez znaku. Indeks poza zakresem, dzielenie przez zero i null zatrzymują program czytelnym błędem, zamiast
  powodować niezdefiniowane zachowanie. Priorytety operatorów nie gryzą (`x & 1 == 0` znaczy to, co widać).
- **Praktyczny.** Mapy, metody, błędy jako wartości (`int(s) or 0`, `try read_file(p)`), moduły, każda
  biblioteka C i serwer języka dla twojego edytora. Działa na Linuksie, macOS i Windowsie, na x86-64 i ARM64.
- **Błędy mówią po ludzku:**
  ```
  gra.fch:3:13: error: 'y' must be i32, but this is int (use i32(...) to convert)
      3 |     i32 y = x
        |             ^
  ```
- **Każda biblioteka C, bezpośrednio.** Piszesz `import "raylib.h"` i `link "raylib"`, i możesz z niej korzystać,
  także ze struktur przekazywanych przez wartość i z callbacków. Nagłówki czyta libclang, więc nie trzeba niczego deklarować ręcznie.
- **Napisany w sobie.** Kompilator to kod w Finchu i nic więcej: `src/*.fch`, ok. 8,6 tys. linii. Buduje się go z ziarna
  (własny kod LLVM IR, `bootstrap/`), potem z samego siebie, a oba wyniki są identyczne. Pierwsza wersja, w C++,
  jest w gałęzi `before-bootstrap`.

## Szybki start

**Budowanie** (Linux x86-64 lub ARM64; potrzebny `clang` i libclang, LLVM 21):

```sh
git clone https://github.com/Nexoniarz/Finch.git
cd Finch
nix-shell                           # albo zainstaluj samodzielnie clang + libclang (LLVM 21) + pkg-config
bootstrap/build.sh                  # kompilator to build/finch: z ziarna, potem z samego siebie

./build/finch run examples/hello.fch
./build/finch build examples/tour.fch -o tour && ./tour
tests/run.sh                        # każdy program, każda odmowa, -g, biblioteka współdzielona, serwer języka
```

Gotowe kompilatory są w [wydaniach](https://github.com/Nexoniarz/Finch/releases) (Linux x64 i ARM64: rozpakuj, zostaw
`runtime/` i `std/` obok `finch`). **Programy dla Windowsa, ARM64 i macOS** buduje się z Linuksa przez
`--target windows|arm64|macos`; sam kompilator nie działa jeszcze na macOS ani Windowsie (potrzebuje ziarna
zrobionego z nagłówkami tego systemu: patrz [bootstrap/README.md](bootstrap/README.md)).

Edytory: **Kate** (`editors/kate/install.sh`), **VS Code** (plik `.vsix` `finch-lang` z wydania) albo dowolny
edytor z obsługą LSP (`finch lsp`).

## Dokumentacja

Wybierz przewodnik dla siebie:

| Przewodnik | Dla kogo | Polski | English |
|---|---|---|---|
| **Początkujący** | Nigdy nie programowałeś: uczniowie, seniorzy, ciekawscy | [dla-poczatkujacych.md](docs/pl/dla-poczatkujacych.md) | [for-beginners.md](docs/en/for-beginners.md) |
| **Technicy** | Znasz komputery, programowanie jest nowe: instalacja, cały język, biblioteki C, problemy | [dla-technikow.md](docs/pl/dla-technikow.md) | [for-technicians.md](docs/en/for-technicians.md) |
| **Inżynierowie** | Wszystko: lekser, gramatyka, AST, reguły typów, tłumaczenie na IR, import przez libclang, ABI, wnętrze | [dla-inzynierow.md](docs/pl/dla-inzynierow.md) | [for-engineers.md](docs/en/for-engineers.md) |
| **Ściąga** | Każda funkcja, metoda, operator i polecenie na jednej stronie | [sciaga.md](docs/pl/sciaga.md) | [cheatsheet.md](docs/en/cheatsheet.md) |

## Język w pigułce

| | |
|---|---|
| Zmienne | `x := 5` (typ zgadnięty) albo `int x = 5`; `const W = 640` |
| Typy | `int` `float` `bool` `char` `str`, z rozmiarem `i8`…`i64` `u8`…`u64` `f32` `f64`, tablice `[]T`, mapy `map[K]V`, wskaźniki `ptr[T]` |
| Funkcje | `fn add(int a, int b) -> int { return a + b }` |
| Struktury | `struct Punkt { … }` z jednym polem na linijkę (`int x`, `int y = 0`); tworzenie przez `Punkt(1, 2)` albo `Punkt(x: 1)` |
| Metody | `fn Punkt.przesun(int dx) { self.x += dx }` → `p.przesun(3)` |
| Tablice | `liczby := [1, 2, 3]`, `liczby.push(4)`, `liczby[0]`, `liczby.len`, `for i, n in liczby { }` |
| Mapy | `wiek := ["ania": 31]`, `wiek["bob"] = 25`, `wiek.has("cy")`, `ile[s] += 1`, `for k, v in wiek { }` |
| Błędy | `fn parsuj(str s) -> int! { … return error("zły") }` → `parsuj(s) or 0`, `parsuj(s) or { … }`, `try parsuj(s)` |
| Tekst | `"hp \(hp)"`, `"a" + "b"`, `str(42)`, `int("42")`, `s.split(",")`, `s.upper()`, `s[0]` |
| Sterowanie | `if` / `else if` / `else` (też jako wartość), `while`, `for i in 0..10`, `break`, `continue`, `defer` |
| Pamięć | automatyczna dla tablic, tekstów i struktur; `new(...)` / `free(...)` dla własnych struktur na stercie |
| Enumy, match | `enum Kolor { Czerwony, Zielony }`, `match k { Czerwony -> ... else -> ... }` (także jako wartość) |
| Generyki | `fn max[T](T a, T b) -> T`, `struct Stos[T]`, `Stos[int]()` |
| Moduły | `import ksztalty` → `ksztalty.pole(p)`, `import ui/button as btn`; biblioteka standardowa: `json`, `path`, `time`, `proc`, `fs`, `thread` |
| Wątki | `thread.spawn(addr(praca), addr(dane))`, `t.join()`, `thread.mutex()`, `atomic_add(addr(n), 1)` |
| Testy | `test "nazwa" { assert(x == 3) }` obok kodu, uruchamiane przez `finch test` |
| Współpraca z C | `import "stdio.h"`, `link "glfw"`, `link "moje.c"` / `"glue.cpp"`, unie, `#define` (też `(Color){…}`), `addr(fn)` dla callbacków |
| Wejście/wyjście | `print(...)`, `input("? ")`, `read_file`, `write_file`, `shell` |
| Narzędzia | `finch run/build/test/fmt/ir/lsp`, `finch.toml` dla projektu, cache budowania, `--target windows/arm64/macos/android`, `-g` dla gdb/lldb, `-O0`, łańcuch wywołań przy błędach w czasie działania |

## Przykłady

| Plik | Pokazuje |
|---|---|
| [`examples/hello.fch`](examples/hello.fch) | najmniejszy program |
| [`examples/tour.fch`](examples/tour.fch) | zmienne, funkcje, warunki, pętle |
| [`examples/guess.fch`](examples/guess.fch) | gra w zgadywanie: wejście, pętle, `rand` z C |
| [`examples/todo.fch`](examples/todo.fch) | struktury, tablice, teksty i pliki |
| [`examples/wordcount.fch`](examples/wordcount.fch) | mapy, metody i błędy jako wartości: najczęstsze słowa w pliku |
| [`examples/structs.fch`](examples/structs.fch) | struktury, tablice struktur, lista wiązana z `new`/`free` |
| [`examples/types.fch`](examples/types.fch) | liczby z rozmiarem i wskaźniki |
| [`examples/c_import.fch`](examples/c_import.fch) | `printf`, `math.h`, `malloc`/`free`, `stderr` |
| [`examples/window.fch`](examples/window.fch) | okno GLFW + OpenGL |
| [`examples/opengl.fch`](examples/opengl.fch) | nowoczesny OpenGL 3.3: shadery, bufory, obracający się trójkąt |
| [`examples/vulkan.fch`](examples/vulkan.fch) | Vulkan: lista kart graficznych, ich pamięć i kolejki, tworzenie urządzenia |
| [`examples/raylib.fch`](examples/raylib.fch) | raylib ze strukturami C przez wartość |
| [`examples/llvm.fch`](examples/llvm.fch) | Finch budujący LLVM IR przez API LLVM-C |
| [`src/`](src/) | kompilator napisany w Finchu (ziarno do jego zbudowania jest w [`bootstrap/`](bootstrap/)) |

## Plan rozwoju

- [x] Lekser, parser, generowanie kodu LLVM, optymalizacja O2
- [x] Typy, w tym liczby z rozmiarem, wskaźniki, konwersje; funkcje; `if`/`while`/`for`
- [x] Czytelne błędy kompilacji; kontrole w czasie działania (dzielenie, null, zakres)
- [x] Operatory bitowe z rozsądnymi priorytetami
- [x] Nagłówki C przez libclang; `link` z pkg-config; porady przy błędach linkowania
- [x] `struct`, także struktury C przez wartość (ABI System V) i callbacki
- [x] Tablice z `.len`, kontrolą zakresu i metodami; `for x in lista`
- [x] Pamięć: zwalnianie na końcu bloku, `defer`, `new`/`free`
- [x] Teksty: `+`, `.len`, indeksowanie, metody, `str(x)`, `int(s)`; `input()`; pliki
- [x] Moduły Fincha (`import nazwa`)
- [x] Informacje dla debuggera (`-g`)
- [x] Bootstrap: kompilator Fincha napisany w Finchu, który buduje sam siebie
- [x] Windows: `finch.exe`, programy `.exe`, ABI Microsoft x64; budowanie na Windowsa z Linuksa (`--target windows`)
- [x] Serwer języka (`finch lsp`) i rozszerzenie VS Code
- [x] macOS i ARM64 (Linux i Apple Silicon) z ich konwencjami wywołań C; obsługa Kate
- [x] Metody w strukturach, mapy, błędy jako wartości (`!`, `or`, `try`)
- [x] Typy generyczne (`fn max[T]`, `struct Stos[T]`), `match`, enumy
- [x] Biblioteka standardowa w Finchu: `json`, `path`, `time`, `proc`, `fs`
- [x] `const`, `if` jako wartość, `"\(x)"` w tekście, tekst `"""`, unie C i wyrażenia `#define`
- [x] Bloki `test` i `finch test`, `finch fmt`, `finch.toml`, cache budowania, łańcuch wywołań przy błędach
- [x] Wątki (`import thread`) i operacje atomowe; pliki C++; `--target android` (budowane, jeszcze nie sprawdzone na urządzeniu)

## Struktura

```
src/        kompilator, w Finchu: lekser, parser, AST, generowanie kodu (do LLVM IR jako tekst), import C (libclang)
            i konwencje wywołań C, serwer języka, sterownik
std/        biblioteka standardowa, w Finchu: json, path, time, proc, fs
runtime/    mały runtime w C dołączany do każdego programu
bootstrap/  build.sh i ziarna: własny LLVM IR kompilatora, po jednym na system
examples/   przykładowe programy
editors/    rozszerzenie VS Code, podświetlanie i konfiguracja LSP dla Kate
tests/      run/ new/ (wzorcowe wyjście), fail/ new/fail/ (oczekiwane błędy), run.sh, cross.sh, shared.sh, lsp_test.py
docs/       przewodniki en/ i pl/ dla trzech grup odbiorców
```

## Licencja

[Apache License 2.0](LICENSE). Copyright 2026 Nexoniarz.
