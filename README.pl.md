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
- **Szybki.** Kompiluje się przez potok `-O2` z LLVM do kodu natywnego, bez garbage collectora.
  Rekurencyjne `fib(40)` trwa ok. 0,19 s (`clang -O2` na tym samym kodzie C: ok. 0,23 s), a kod na tablicach
  z kontrolą zakresu dorównuje C.
- **Pamięć bez bólu.** Listy, teksty i struktury są wartościami: przypisanie kopiuje, a koniec bloku je zwalnia.
  Nie ma `free`, o którym można zapomnieć, nie ma użycia po zwolnieniu, a każdy test jest czysty pod valgrindem.
- **Bezpieczniejszy niż C tam, gdzie to tanie.** Nie ma cichego zwężania typów ani mieszania liczb ze znakiem
  i bez znaku. Indeks poza zakresem, dzielenie przez zero i null zatrzymują program czytelnym błędem, zamiast
  powodować niezdefiniowane zachowanie. Priorytety operatorów nie gryzą (`x & 1 == 0` znaczy to, co widać).
- **Błędy mówią po ludzku:**
  ```
  gra.fn:3:13: error: 'y' must be i32, but this is int (use i32(...) to convert)
      3 |     i32 y = x
        |             ^
  ```
- **Każda biblioteka C, bezpośrednio.** Piszesz `import "raylib.h"` i `link "raylib"`, i możesz z niej korzystać,
  także ze struktur przekazywanych przez wartość i z callbacków. Nagłówki czyta libclang, więc nie trzeba niczego deklarować ręcznie.
- **Napisany także w sobie.** `boot/` to kompilator Fincha napisany w Finchu, który kompiluje sam siebie
  do identycznego co do bajtu wyniku.

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
tests/run.sh                        # 58 passed, 0 failed
tests/boot.sh                       # kompilator napisany w Finchu buduje sam siebie
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
| Typy | `int` `float` `bool` `char` `str`, z rozmiarem `i8`…`i64` `u8`…`u64` `f32` `f64`, tablice `[]T`, wskaźniki `ptr[T]` |
| Funkcje | `fn add(int a, int b) -> int { return a + b }` |
| Struktury | `struct Punkt { … }` z jednym polem na linijkę (`int x`, `int y = 0`); tworzenie przez `Punkt(1, 2)` albo `Punkt(x: 1)` |
| Tablice | `liczby := [1, 2, 3]`, `liczby.push(4)`, `liczby[0]`, `liczby.len`, `for n in liczby { }` |
| Tekst | `"a" + "b"`, `str(42)`, `int("42")`, `s.split(",")`, `s.upper()`, `s[0]` |
| Sterowanie | `if` / `else if` / `else`, `while`, `for i in 0..10`, `break`, `continue`, `defer` |
| Pamięć | automatyczna dla tablic, tekstów i struktur; `new(...)` / `free(...)` dla własnych struktur na stercie |
| Moduły | `import ksztalty` → `ksztalty.pole(p)` |
| Współpraca z C | `import "stdio.h"`, `link "glfw"`, `link "moje.c"`, `addr(fn)` dla callbacków |
| Wejście/wyjście | `print(...)`, `input("? ")`, `read_file`, `write_file`, `shell` |
| Narzędzia | `finch run/build/ir`, `-g` dla gdb, `-O0` |

## Przykłady

| Plik | Pokazuje |
|---|---|
| [`examples/hello.fn`](examples/hello.fn) | najmniejszy program |
| [`examples/tour.fn`](examples/tour.fn) | zmienne, funkcje, warunki, pętle |
| [`examples/guess.fn`](examples/guess.fn) | gra w zgadywanie: wejście, pętle, `rand` z C |
| [`examples/todo.fn`](examples/todo.fn) | struktury, tablice, teksty i pliki |
| [`examples/structs.fn`](examples/structs.fn) | struktury, tablice struktur, lista wiązana z `new`/`free` |
| [`examples/types.fn`](examples/types.fn) | liczby z rozmiarem i wskaźniki |
| [`examples/c_import.fn`](examples/c_import.fn) | `printf`, `math.h`, `malloc`/`free`, `stderr` |
| [`examples/window.fn`](examples/window.fn) | okno GLFW + OpenGL |
| [`examples/raylib.fn`](examples/raylib.fn) | raylib ze strukturami C przez wartość |
| [`examples/llvm.fn`](examples/llvm.fn) | Finch budujący LLVM IR przez API LLVM-C |
| [`boot/`](boot/) | kompilator Fincha napisany w Finchu |

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
- [ ] Metody w strukturach, mapy, `match`
- [ ] Błędy jako wartości zamiast zatrzymywania programu
- [ ] Więcej platform (ARM64, macOS)

## Struktura

```
src/        kompilator: lekser, parser, AST, codegen, import C + ABI, sterownik (C++17, ok. 4,5 tys. linii)
runtime/    mały runtime w C dołączany do każdego programu
boot/       kompilator Fincha napisany w Finchu (ok. 3 tys. linii)
examples/   przykładowe programy
tests/      run/ (wzorcowe wyjście), fail/ (oczekiwane błędy), run.sh, boot.sh
docs/       przewodniki en/ i pl/ dla trzech grup odbiorców
```

## Licencja

[Apache License 2.0](LICENSE). Copyright 2026 Nexoniarz.
