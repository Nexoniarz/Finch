# Finch 🐦

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
- **Szybki.** Kompiluje się przez potok `-O2` z LLVM do kodu natywnego, bez środowiska uruchomieniowego i garbage collectora.
  Rekurencyjne `fib(40)` trwa ok. 0,19 s (`clang -O2` na tym samym kodzie C: ok. 0,23 s).
- **Bezpieczniejszy niż C tam, gdzie to tanie.** Nie ma cichego zwężania typów ani mieszania liczb ze znakiem
  i bez znaku. Dzielenie przez zero i `.value` na null zatrzymują program czytelnym błędem, zamiast
  powodować niezdefiniowane zachowanie. Priorytety operatorów nie gryzą (`x & 1 == 0` znaczy to, co widać).
- **Błędy mówią po ludzku:**
  ```
  gra.fn:3:13: error: 'y' must be i32, but this is int (use i32(...) to convert)
      3 |     i32 y = x
        |             ^
  ```
- **Każda biblioteka C, bezpośrednio.** Piszesz `import "GLFW/glfw3.h"` i `link "glfw"`, i możesz z niej korzystać.
  Nagłówki czyta libclang, więc nie trzeba niczego deklarować ręcznie.

## Szybki start

Linux x86-64, LLVM 21.

```sh
git clone https://github.com/Nexoniarz/Finch.git
cd Finch
nix-shell                           # albo zainstaluj samodzielnie LLVM 21 + libclang + cmake + ninja
cmake -S . -B build -G Ninja
ninja -C build

./build/finch run examples/hello.fn
./build/finch build examples/tour.fn -o tour && ./tour
tests/run.sh                        # 34 passed, 0 failed
```

## Dokumentacja

Wybierz przewodnik dla siebie:

| Przewodnik | Dla kogo | Polski | English |
|---|---|---|---|
| **Początkujący** | Nigdy nie programowałeś: uczniowie, seniorzy, ciekawscy | [dla-poczatkujacych.md](docs/pl/dla-poczatkujacych.md) | [for-beginners.md](docs/en/for-beginners.md) |
| **Technicy** | Znasz komputery, programowanie jest nowe: instalacja, cały język, biblioteki C, problemy | [dla-technikow.md](docs/pl/dla-technikow.md) | [for-technicians.md](docs/en/for-technicians.md) |
| **Inżynierowie** | Wszystko: lekser, gramatyka, AST, reguły typów, tłumaczenie na IR, import przez libclang, ABI, wnętrze | [dla-inzynierow.md](docs/pl/dla-inzynierow.md) | [for-engineers.md](docs/en/for-engineers.md) |

## Język w pigułce

| | |
|---|---|
| Zmienne | `x := 5` (typ zgadnięty) albo `int x = 5` |
| Typy | `int` `float` `bool` `char` `str`, z rozmiarem `i8`…`i64` `u8`…`u64` `f32` `f64`, wskaźniki `ptr[T]` / `ptr` |
| Funkcje | `fn add(int a, int b) -> int { return a + b }` |
| Sterowanie | `if` / `else if` / `else`, `while`, `for i in 0..10`, `break`, `continue` |
| Wskaźniki | `p := addr(x)`, `p.value = 5`, `null` |
| Konwersje | `int(3.9)`, `u8(x)`, `float(n)` (niejawne tylko wtedy, gdy nic nie ginie) |
| Operatory | `+ - * / %`, `& \| ^ << >> ~`, `== != < <= > >=`, `&& \|\| !` |
| Współpraca z C | `import "stdio.h"`, `link "glfw"` |
| Wypisywanie | `print(a, b, c)` wypisuje wszystko, oddzielając spacjami |

## Przykłady

| Plik | Pokazuje |
|---|---|
| [`examples/hello.fn`](examples/hello.fn) | najmniejszy program |
| [`examples/tour.fn`](examples/tour.fn) | zmienne, funkcje, warunki, pętle |
| [`examples/types.fn`](examples/types.fn) | liczby z rozmiarem i wskaźniki |
| [`examples/c_import.fn`](examples/c_import.fn) | `printf`, `math.h`, `malloc`/`free`, `stderr` |
| [`examples/window.fn`](examples/window.fn) | okno GLFW + OpenGL |
| [`examples/llvm.fn`](examples/llvm.fn) | Finch budujący LLVM IR przez API LLVM-C |

## Plan rozwoju

- [x] Lekser, parser, generowanie kodu LLVM, optymalizacja O2
- [x] Typy, w tym liczby z rozmiarem, wskaźniki, konwersje; funkcje; `if`/`while`/`for`
- [x] Czytelne błędy kompilacji; kontrole w czasie działania (dzielenie, null)
- [x] Operatory bitowe z rozsądnymi priorytetami
- [x] Nagłówki C przez libclang; `link` z pkg-config; porady przy błędach linkowania
- [ ] `struct` (i struktury C przez wartość)
- [ ] Tablice/wycinki z `.len` i kontrolą zakresu
- [ ] Pamięć: automatyczne zwalnianie na końcu bloku, `defer`, ręczne `free` dla chętnych
- [ ] Teksty: `+`, `len`, `str(x)`; `input()`
- [ ] Moduły Fincha (`import math`)
- [ ] Informacje dla debuggera (DWARF)
- [ ] Bootstrap: kompilator Fincha napisany w Finchu

## Struktura

```
src/        kompilator: lekser, parser, AST, codegen, import C, sterownik (C++17, ok. 2,4 tys. linii)
examples/   przykładowe programy
tests/      run/ (wzorcowe wyjście), fail/ (oczekiwane błędy), run.sh
docs/       przewodniki en/ i pl/ dla trzech grup odbiorców
```

## Licencja

[Apache License 2.0](LICENSE). Copyright 2026 Nexoniarz.
