<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/logo-dark.svg">
    <img alt="Finch" src="assets/logo.svg" width="460">
  </picture>
</p>

**Mały, szybki, celny.** Język programowania oparty na LLVM, ze składnią w stylu C. Nauczysz się go w godzinę,
a programy działają tak szybko jak w C.

> 🇬🇧 [English version](README.md)

```c
import "math.h"

fn przeciwprostokatna(float a, float b) -> float {
    return sqrt(a * a + b * b)
}

fn main() {
    imie := "Finch"
    print("Cześć, tu", imie, "!")

    for i in 1..4 {
        print(i, "→", przeciwprostokatna(float(i), 1.0))
    }
}
```

## Dlaczego Finch

- **Prosty.** Nie ma średników, plików nagłówkowych ani preprocesora. Na każdą rzecz jest jeden sposób.
- **Szybki.** Kod przechodzi przez optymalizacje LLVM (`-O2`) i staje się zwykłym programem, bez garbage collectora.
  Rekurencyjne `fib(40)` liczy się ok. 0,19 s (ten sam kod w C z `clang -O2`: ok. 0,23 s), a pętle po tablicach
  ze sprawdzaniem zakresu są tak samo szybkie jak w C.
- **Pamięć bez kłopotów.** Listy, teksty i struktury są wartościami: przypisanie robi kopię, a koniec bloku sprząta.
  Nie ma `free`, o którym da się zapomnieć, ani odwołań do zwolnionej pamięci. Valgrind nie znajduje błędów w żadnym teście.
- **Bezpieczniejszy niż C tam, gdzie nic to nie kosztuje.** Liczby nie zmieniają po cichu typu, a liczb ze znakiem
  i bez znaku nie da się przypadkiem pomieszać. Indeks poza tablicą, dzielenie przez zero czy `null` kończą program
  jasnym komunikatem, a nie niezdefiniowanym zachowaniem. Kolejność działań jest taka, jakiej się spodziewasz
  (`x & 1 == 0` znaczy dokładnie to, co widać).
- **Praktyczny.** Mapy, metody, błędy jako wartości (`int(s) or 0`, `try read_file(p)`), moduły, dowolna biblioteka C
  i serwer języka dla edytora. Działa na Linuksie, macOS i Windowsie, na procesorach x86-64 i ARM64.
- **SIMD w języku.** `f32x8`, `i8x32`, `f64x4`: kilka liczb obok siebie, dodawanych i mnożonych naraz. LLVM zamienia je
  na instrukcje SSE, AVX2, AVX-512 albo NEON, więc ten sam kod jest szybki na komputerze stacjonarnym, laptopie i ARM-ie.
- **Komunikaty, które da się zrozumieć:**
  ```
  gra.fch:3:13: error: 'y' must be i32, but this is int
     |
   3 |     i32 y = x
     |             ^
     = help: use i32(...) to convert
  ```
  W terminalu są kolorowe, a `finch build` mówi, ile trwało budowanie.
- **Biblioteki C++ też.** `import "imgui.h"`, potem `ImGui.Button("OK")`: przestrzenie nazw, klasy, przeciążenia,
  argumenty domyślne i wyjątki działają, bez pisania nakładki w C.
- **Biblioteki C bez pośredników.** Wystarczy `import "raylib.h"` i `link "raylib"`, żeby z niej korzystać, łącznie
  ze strukturami przekazywanymi przez wartość i funkcjami zwrotnymi. Nagłówki czyta libclang, więc niczego nie trzeba
  deklarować ręcznie.
- **Napisany w samym sobie.** Kompilator to wyłącznie kod w Finchu: `src/*.fch`, ok. 12 tys. linii. Buduje się go
  z pliku startowego (jego własnego kodu LLVM IR, w `bootstrap/`), a potem jeszcze raz nim samym. Oba wyniki są identyczne.

## Na start

**Gotowe kompilatory** na Linuksa (x64, ARM64), macOS (Apple Silicon, Intel) i Windowsa (x64) znajdziesz w
[wydaniach](https://github.com/Nexoniarz/Finch/releases). Rozpakuj archiwum i trzymaj katalogi `runtime/` i `std/` obok
pliku `finch`. Co jeszcze trzeba doinstalować, opisuje [przewodnik dla techników](docs/pl/dla-technikow.md#2-instalacja):
na Linuksie clang i libclang z LLVM 21, na Macu `brew install llvm@21`, na Windowsie llvm-mingw.

**Samodzielne budowanie** (na Linuksie, macOS albo Windowsie w Git Bash; potrzebne są `clang` i libclang z LLVM 21):

```sh
git clone https://github.com/Nexoniarz/Finch.git
cd Finch
nix-shell                           # albo zainstaluj clang, libclang (LLVM 21) i pkg-config po swojemu
bootstrap/build.sh                  # gotowy kompilator to build/finch

./build/finch run examples/hello.fch
./build/finch build examples/tour.fch -o tour && ./tour
tests/run.sh                        # wszystkie programy testowe, oczekiwane błędy, -g, biblioteka współdzielona, serwer języka
```

Ten sam `bootstrap/build.sh` działa na Macu (z `llvm@21` z Homebrew) i na Windowsie (z llvm-mingw); szczegóły są w
[bootstrap/README.md](bootstrap/README.md). Program na inny system zbudujesz opcją `--target windows|linux|arm64|macos`
(zob. [przewodnik](docs/pl/dla-technikow.md#budowanie-na-inny-system)).

Edytory: **Kate** (`editors/kate/install.sh`), **VS Code** (plik `.vsix` `finch-lang` z wydania) albo każdy edytor,
który obsługuje LSP (`finch lsp`).

## Dokumentacja

Wybierz przewodnik dla siebie:

| Przewodnik | Dla kogo | Polski | English |
|---|---|---|---|
| **Dla początkujących** | Dla tych, którzy nigdy nie programowali: uczniów, seniorów, ciekawych świata | [dla-poczatkujacych.md](docs/pl/dla-poczatkujacych.md) | [for-beginners.md](docs/en/for-beginners.md) |
| **Dla techników** | Znasz komputery, ale programowanie jest dla ciebie nowe: instalacja, cały język, biblioteki C, rozwiązywanie problemów | [dla-technikow.md](docs/pl/dla-technikow.md) | [for-technicians.md](docs/en/for-technicians.md) |
| **Dla inżynierów** | Wszystko: lekser, gramatyka, AST, reguły typów, generowanie IR, import przez libclang, ABI, budowa kompilatora | [dla-inzynierow.md](docs/pl/dla-inzynierow.md) | [for-engineers.md](docs/en/for-engineers.md) |
| **Ściąga** | Wszystkie funkcje, metody, operatory i polecenia na jednej stronie | [sciaga.md](docs/pl/sciaga.md) | [cheatsheet.md](docs/en/cheatsheet.md) |

## Język w pigułce

| | |
|---|---|
| Zmienne | `x := 5` (typ wynika z wartości) albo `int x = 5`; `const W = 640` |
| Typy | `int` `float` `bool` `char` `str`, o podanym rozmiarze: `i8`…`i64` `u8`…`u64` `f32` `f64`, tablice `[]T`, mapy `map[K]V`, wskaźniki `ptr[T]`, wektory `f32x8` `i8x32`… |
| Funkcje | `fn dodaj(int a, int b) -> int { return a + b }`; `fn powieksz(mut []int lista)` zmienia zmienną wywołującego: `powieksz(mut liczby)` |
| Struktury | `struct Punkt { … }`, jedno pole w linii (`int x`, `int y = 0`); tworzy się je przez `Punkt(1, 2)` albo `Punkt(x: 1)` |
| Metody | `fn Punkt.przesun(int dx) { self.x += dx }` → `p.przesun(3)` |
| Tablice | `liczby := [1, 2, 3]`, `liczby.push(4)`, `liczby[0]`, `liczby.len`, `for i, n in liczby { }`, `ludzie.sort_by(fn(a, b) { return a.wiek < b.wiek })`, `filter`, `map`, `any`, `all` |
| Funkcje jako wartości | `fn(int) -> int f = podwoj`, `f := fn(int x) -> int { return x + n }` (bierze kopie tego, czego używa), także w polach struktur i listach |
| Mapy | `wiek := ["ania": 31]`, `wiek["bob"] = 25`, `wiek.has("cyryl")`, `ile[slowo] += 1`, `for k, v in wiek { }` |
| Błędy | `fn parsuj(str s) -> int! { … return error("zła liczba") }` → `parsuj(s) or 0`, `parsuj(s) or { … }`, `try parsuj(s)` |
| Asembler | `asm("add {x}, {y}", mut x, y)`, `asm("cpuid", eax: mut a)`: składnia Intela na x86, rozmiary dobierane same na ARM64 |
| Wektory (SIMD) | `v := f32x8(lista, i)`, `v * 2.0 + w`, `v.sum()`, `select(v > 0.0, v, 0.0)`, `t.lookup(idx)`, `acc.dot_add(a, b)`, `v.store(lista, i)` |
| Procesory | `--cpu native`; `@cpu("avx2") fn …` razem z `if cpu_has("avx2")`; liczby zmiennoprzecinkowe liczone tak, jak zapisane (`fma`/`muladd` do łączenia) |
| Tekst | `"hp: \(hp)"`, `"a" + "b"`, `str(42)`, `int("42")`, `s.split(",")`, `s.upper()`, `s[0]` |
| Sterowanie | `if` / `else if` / `else` (również jako wartość), `while`, `for i in 0..10`, `break`, `continue`, `defer` |
| Pamięć | tablice, teksty i struktury zwalniają się same; `new(...)` / `free(...)` dla własnych struktur na stercie |
| Enumy i `match` | `enum Kolor { Czerwony, Zielony }`, `match k { Czerwony -> ... else -> ... }` (również jako wartość) |
| Typy generyczne | `fn max[T](T a, T b) -> T`, `struct Stos[T]`, `Stos[int]()` |
| Wartości opcjonalne | `fn znajdz(str n) -> Osoba?`, `return null`, `znajdz(n) or gosc`, `if o := znajdz(n) { }`, `x == null` |
| Interfejsy | `interface Ksztalt { fn pole() -> float }`: każda struktura z tymi metodami nim jest; `[]Ksztalt k = [Kolo(1.0), Prostokat(2.0, 3.0)]` |
| Moduły | `import ksztalty` → `ksztalty.pole(p)`, `import ui/button as btn`; biblioteka standardowa: `json`, `path`, `time`, `proc`, `fs`, `thread` |
| Wątki | `thread.spawn(addr(praca), addr(dane))`, `t.join()`, `thread.mutex()`, `atomic_add(addr(n), 1)` |
| Testy | `test "nazwa" { assert(x == 3) }` tuż obok kodu, uruchamiane przez `finch test` |
| Współpraca z C | `import "stdio.h"`, `link "glfw"`, `link "moje.c"` / `"glue.cpp"`, unie, `#define` (także `(Color){…}`), `addr(fn)` jako funkcja zwrotna |
| Wejście i wyjście | `print(...)`, `input("? ")`, `read_file`, `write_file`, `shell` |
| Narzędzia | `finch run/build/test/fmt/ir/lsp`, `finch.toml` dla projektu, pamięć podręczna kompilacji, `--target windows/arm64/macos/android`, `--cpu native`, `-g` dla gdb i lldb, `-O0`, ścieżka wywołań przy błędzie w trakcie działania |

## Przykłady

| Plik | Co pokazuje |
|---|---|
| [`examples/hello.fch`](examples/hello.fch) | najkrótszy program |
| [`examples/tour.fch`](examples/tour.fch) | zmienne, funkcje, warunki, pętle |
| [`examples/guess.fch`](examples/guess.fch) | gra w zgadywanie liczby: wczytywanie, pętle, `rand` z C |
| [`examples/todo.fch`](examples/todo.fch) | struktury, tablice, teksty i pliki |
| [`examples/wordcount.fch`](examples/wordcount.fch) | mapy, metody i błędy jako wartości: najczęstsze słowa w pliku |
| [`examples/structs.fch`](examples/structs.fch) | struktury, tablice struktur, lista jednokierunkowa z `new`/`free` |
| [`examples/types.fch`](examples/types.fch) | liczby o podanym rozmiarze i wskaźniki |
| [`examples/c_import.fch`](examples/c_import.fch) | `printf`, `math.h`, `malloc`/`free`, `stderr` |
| [`examples/simd.fch`](examples/simd.fch) | wektory: iloczyn skalarny liczony po osiem liczb naraz i zysk na czasie |
| [`examples/window.fch`](examples/window.fch) | okno GLFW z OpenGL |
| [`examples/opengl.fch`](examples/opengl.fch) | nowoczesny OpenGL 3.3: shadery, bufory, obracający się trójkąt |
| [`examples/vulkan.fch`](examples/vulkan.fch) | Vulkan: lista kart graficznych z ich pamięcią i kolejkami, utworzenie urządzenia |
| [`examples/raylib.fch`](examples/raylib.fch) | raylib i struktury C przekazywane przez wartość |
| [`examples/llvm.fch`](examples/llvm.fch) | Finch, który buduje LLVM IR przez API LLVM-C |
| [`src/`](src/) | kompilator napisany w Finchu (plik startowy do jego zbudowania leży w [`bootstrap/`](bootstrap/)) |

## Plan rozwoju

- [x] Lekser, parser, generowanie kodu LLVM, optymalizacja O2
- [x] Typy, w tym liczby o podanym rozmiarze, wskaźniki i konwersje; funkcje; `if`/`while`/`for`
- [x] Czytelne błędy kompilacji; sprawdzanie w trakcie działania (dzielenie, `null`, zakres tablicy)
- [x] Operatory bitowe z rozsądną kolejnością działań
- [x] Nagłówki C przez libclang; `link` z pkg-config; podpowiedzi przy błędach linkera
- [x] `struct`, w tym struktury C przekazywane przez wartość (ABI System V) i funkcje zwrotne
- [x] Tablice z `.len`, sprawdzaniem zakresu i metodami; `for x in lista`
- [x] Pamięć: wartości zwalniane na końcu bloku, `defer`, `new`/`free`
- [x] Teksty: `+`, `.len`, indeksowanie, metody, `str(x)`, `int(s)`; `input()`; pliki
- [x] Moduły Fincha (`import nazwa`)
- [x] Informacje dla debuggera (`-g`)
- [x] Bootstrap: kompilator Fincha napisany w Finchu, który kompiluje sam siebie
- [x] Windows: `finch.exe`, programy `.exe`, ABI Microsoft x64; budowanie na Windowsa z Linuksa (`--target windows`)
- [x] Serwer języka (`finch lsp`) i rozszerzenie do VS Code
- [x] macOS i ARM64 (Linux i Apple Silicon) z ich konwencjami wywołań C; obsługa Kate
- [x] Metody struktur, mapy, błędy jako wartości (`!`, `or`, `try`)
- [x] Typy generyczne (`fn max[T]`, `struct Stos[T]`), `match`, enumy
- [x] Biblioteka standardowa w Finchu: `json`, `path`, `time`, `proc`, `fs`
- [x] `const`, `if` jako wartość, `"\(x)"` w tekście, tekst `"""`, unie C i wyrażenia w `#define`
- [x] Bloki `test` i `finch test`, `finch fmt`, `finch.toml`, pamięć podręczna kompilacji, ścieżka wywołań przy błędach
- [x] Wątki (`import thread`) i operacje atomowe; pliki C++; `--target android`
- [x] Kompilator na każdym systemie, na który buduje: Linux, macOS (Apple Silicon i Intel), Windows; wydania robi CI
- [x] Typy wektorowe (`f32x8`, `i8x32`, …) dla SIMD i `--cpu native`
- [x] Maski, przestawianie, tabele bajtów i `dot_add` dla jąder obliczeniowych; `@cpu` i `cpu_has`; `fma`/`muladd`, bity liczb, f16 i bf16
- [x] Parametry `mut` (zmiana zmiennej wywołującego bez wskaźników); wstawki asemblera `asm(...)` na x86-64 i ARM64
- [x] Funkcje jako wartości: typy `fn(int) -> bool`, funkcje pisane na miejscu, które trzymają kopie tego, czego używają, `sort_by`, `filter`, `map`, `any`, `all`
- [x] Wartości opcjonalne `T?` (`or`, `if v := x`) i interfejsy (każdy typ z tymi metodami nim jest)
- [x] Nagłówki C++ importowane wprost: przestrzenie nazw, klasy, przeciążenia, argumenty domyślne, wyjątki jako błędy
- [ ] Biblioteki Rusta przez `Cargo.toml`

## Co gdzie leży

```
src/        kompilator w Finchu: lekser, parser, AST, generowanie kodu (LLVM IR jako tekst), import C (libclang)
            i konwencje wywołań C, serwer języka, polecenie finch
std/        biblioteka standardowa w Finchu: json, path, time, proc, fs, thread
runtime/    mały runtime w C, dołączany do każdego programu
bootstrap/  build.sh i pliki startowe: własny LLVM IR kompilatora, osobno dla każdego systemu
examples/   przykładowe programy
editors/    rozszerzenie do VS Code, podświetlanie składni i konfiguracja LSP dla Kate
tests/      run/ new/ (wzorcowe wyniki), fail/ new/fail/ (oczekiwane błędy), run.sh, cross.sh, shared.sh, lsp_test.py
docs/       przewodniki po angielsku (en/) i po polsku (pl/) dla trzech grup czytelników
```

## Licencja

[Apache License 2.0](LICENSE). Copyright 2026 Nexoniarz.
