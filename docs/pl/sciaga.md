# Ściąga z Fincha

Cały Finch na jednej stronie. Wyjaśnienia znajdziesz w [przewodnikach](../../README.pl.md#dokumentacja).

> 🇬🇧 English version: [cheatsheet.md](../en/cheatsheet.md)

---

## Polecenie `finch`

| Polecenie | Co robi |
|---|---|
| `finch run plik.fch [argumenty…]` | kompiluje i uruchamia (argumenty trafiają do programu) |
| `finch build plik.fch [-o nazwa]` | tworzy program (na Windowsie `nazwa.exe`) |
| `finch build plik.fch --shared [-I katalog]` | tworzy bibliotekę (`libplik.so`, `plik.dll`), którą wczyta program w C; jej funkcjami C są funkcje `export fn` (liczby, bool, char, wskaźniki) |
| `finch test plik.fch` | uruchamia każdy blok `test "…" { }` osobno (także w importowanych modułach) |
| `finch fmt [--check] [pliki/katalogi]` | wcięcia po 4 spacje na każdy poziom `{ }`, bez spacji na końcach linii, najwyżej jedna pusta linia z rzędu (`--check`: tylko wypisuje, co by zmienił) |
| `finch run` / `build` / `test` bez pliku | w katalogu z `finch.toml`: plik z pola `entry` |
| `finch ir plik.fch` | wypisuje LLVM IR |
| `finch lsp` | serwer języka (korzystają z niego edytory) |
| `finch version` | wersja |
| `-l bib` | dolinkowuje bibliotekę C (to samo co `link "bib"`) |
| `--target windows` / `linux` / `arm64` / `macos` / `android` / triple | buduje na inny system (`android`: clang z NDK wskazanego przez `ANDROID_NDK_HOME`, zwykle z `--shared`) |
| `-g` | informacje dla debuggera (gdb, lldb) |
| `-O0` | bez optymalizacji |
| `--cpu native` / `x86-64-v3` / `apple-m1` … | używa wszystkich instrukcji danego procesora (AVX2, AVX-512, NEON); program będzie wtedy wymagał podobnego procesora |
| `--trace` / `--no-trace` | błąd w trakcie działania pokazuje ścieżkę wywołań; domyślnie włączone dla `run`, `test`, `-g` i `-O0`, wyłączone dla zwykłego `build` (spowalnia program) |

Zmienne środowiskowe: `FINCH_PATH` (katalogi z modułami), `FINCH_CC` (kompilator C, który linkuje), `FINCH_CXX` (dla plików `.cpp`).
Skompilowany program i jego pliki z `link "x.c"` zostają w pamięci podręcznej (`~/.cache/finch`), dopóki nie zmienią się one same albo ich nagłówki.

`finch.toml` (obok kodu; wtedy `finch run` nie potrzebuje ani nazwy pliku, ani opcji):

```toml
name = "gra"                        # co zbuduje finch build
entry = "src/main.fch"
paths = ["src"]                     # katalogi z modułami
include = ["vendor/SDL3/include"]   # nagłówki C
links = ["SDL3", "native/glue.cpp"] # biblioteki i twoje pliki .c / .cpp / .o / .a
libdirs = ["vendor/SDL3/lib"]
cflags = ["-DFAST"]                 # dla twoich plików .c / .cpp
ldflags = ["-Wl,-rpath,."]
cpu = "native"                      # to samo co --cpu native

[target.windows]                    # tylko przy budowaniu na ten system (są też linux, macos, android, linux-arm64)
links = ["ws2_32"]
```

---

## Plik

```c
import "stdio.h"        // nagłówek C
import ksztalty         // moduł Fincha: ksztalty.fch
link "m"                // biblioteka C albo "moje.c" / ".o" / ".a"

struct Punkt {
    int x
    int y = 0           // wartość domyślna
}

fn main() {
    print("Cześć")
}
```

`main` może wyglądać tak: `fn main()`, `fn main() -> int` (zwraca kod wyjścia) albo `fn main([]str args)`.
Jedna instrukcja w linii, bez średników, `{` w tej samej linii, warunki bez nawiasów.

---

## Zmienne

| Kod | Znaczenie |
|---|---|
| `x := 5` | nowa zmienna, typ wynika z wartości |
| `int x = 5` | nowa zmienna z podanym typem |
| `int x` | zero / `""` / `[]` / `null` / wartości domyślne struktury (także struktur w jej środku) |
| `x = 7` | przypisanie |
| `x += 1` `-=` `*=` `/=` `%=` | zmiana |
| `const W = 640`, `const f32 G = 9.8` | stała (na najwyższym poziomie pliku; z modułu: `ksztalty.W`) |

---

## Typy

| Typ | Przykład |
|---|---|
| `int` (= `i64`) | `42`, `-7`, `0xFF`, `1_000_000` |
| `float` (= `f64`) | `3.14`, `1e9`, `2.5e-3` |
| `bool` | `true`, `false` |
| `char` | `'A'`, `'\n'` |
| `str` | `"tekst\t\"w cudzysłowie\""`, `"hp \(hp), prędkość \(v, 2)"` (kod w tekście; `, 2` = dwie cyfry po przecinku), `"""` kilka linii `"""` (wcięcie zamykającego cudzysłowu znika) |
| `i8 i16 i32 i64` | liczby całkowite ze znakiem, o podanym rozmiarze |
| `u8 u16 u32 u64` | liczby całkowite bez znaku |
| `f32 f64` | liczby zmiennoprzecinkowe |
| `[]T` | tablica: `[1, 2, 3]`, `[]str imiona` |
| `map[K]V` | mapa: `["a": 1]`, `map[str]int ile`, `[:]` |
| `ptr[T]`, `ptr` | wskaźnik, wskaźnik bez typu |
| `f32x8`, `i8x32`, `f64x4`, `u16x16` … | wektor: N liczb liczonych naraz (SIMD), zob. niżej |
| `Nazwa` / `modul.Nazwa` | struktura |

Znaki specjalne: `\n \t \r \0 \\ \" \'`.

---

## Operatory

| Priorytet | Operatory |
|---|---|
| 5 | `*` `/` `%` `<<` `>>` `&` |
| 4 | `+` `-` `\|` `^` (`+` łączy też teksty) |
| 3 | `==` `!=` `<` `<=` `>` `>=` |
| 2 | `&&` |
| 1 | `\|\|` |
| jednoargumentowe | `-x` `!x` `~x` |

`x & 1 == 0` znaczy `(x & 1) == 0`.

---

## Sterowanie

```c
if a > b { … } else if a == b { … } else { … }
x := if a > b { a } else { b }  // jako wartość: po jednej wartości w każdej gałęzi, else obowiązkowe
while x > 0 { … }
for i in 0..10 { … }          // 0 … 9
for el in lista { … }         // każdy element (albo znak tekstu)
for i, el in lista { … }      // razem z indeksem
for klucz, wartosc in m { … } // każdy wpis mapy, w kolejności dodania
break     continue     return wartosc
defer sprzatnij()             // wykona się, gdy blok się skończy
```

---

## Funkcje

```c
fn dodaj(int a, int b) -> int {
    return a + b
}
fn przywitaj(str imie) {      // bez wyniku
    print("Cześć", imie)
}
```

Parametry zachowują się jak kopie. Kolejność funkcji w pliku nie ma znaczenia. Po ostatnim argumencie może stać przecinek.

```c
fn powieksz(mut []int lista) {  // mut: zmienia zmienną wywołującego
    lista.push(1)
}
powieksz(mut liczby)            // mut także przy wywołaniu (zmienna, pole albo element)
```

```c
fn(int) -> int f = podwoj       // funkcja jako wartość (jej typ: fn(parametry) -> wynik)
f = fn(int x) -> int { return x + n }    // napisana na miejscu; bierze kopie n i reszty, tylko do odczytu
f(3)                            // wywołanie; także jako argument, w liście, w polu struktury
```

---

## Metody

```c
fn Punkt.przesun(int dx, int dy) {   // self to punkt, na którym ją wywołano (nie kopia)
    self.x += dx
    self.y += dy
}
fn Punkt.odleglosc() -> float { … }

p.przesun(1, 2)       lista[0].przesun(1, 1)       wskaznik_na_punkt.przesun(0, 1)
```

Działają też na strukturach z C: `fn Vector2.length() -> f32 { … }`.

---

## Błędy jako wartości

```c
fn parsuj(str s) -> int! {        // ! = może się nie udać
    if s.len == 0 {
        return error("pusty tekst")
    }
    return int(s) or { return error("to nie liczba: " + s) }
}
fn zapisz(str p, str t) -> ! {    // może się nie udać, bez wyniku
    try write_file(p, t)
}
```

| Kod | Gdy się nie uda |
|---|---|
| `x := parsuj(s) or 0` | weź 0 |
| `x := parsuj(s) or { print(err); return }` | wykonaj blok (`err` to komunikat) |
| `x := try parsuj(s)` | przerwij też bieżącą funkcję z tym samym błędem |
| `fn main() -> !` | błąd wypisuje `error: …`, kod wyjścia 1 |

Wbudowane funkcje, które mogą się nie udać: `int(s)`, `float(s)`, `read_file`, `write_file`, `delete_file`.

---

## Struktury

| Kod | Znaczenie |
|---|---|
| `struct P { … }` | definicja (jedno pole w linii, `= wartość` opcjonalnie) |
| `P(1, 2)` | utworzenie, wszystkie pola po kolei |
| `P(x: 1)` | utworzenie przez nazwy pól, reszta domyślna albo zero |
| `p.x`, `p.x = 5` | odczyt, zmiana pola |
| `b := a` | pełna kopia |

---

## Tablice `[]T`

| Kod | Wynik |
|---|---|
| `a.len` | liczba elementów |
| `a[i]`, `a[i] = x` | element (zakres jest sprawdzany) |
| `a.push(x)` | dodaje na końcu |
| `a.pop()` | zdejmuje ostatni i go zwraca |
| `a.insert(i, x)` | wstawia na pozycję i |
| `a.remove(i)` | usuwa pozycję i i ją zwraca |
| `a.clear()` | opróżnia |
| `a.resize(n)` | długość n (nowe elementy są zerami) |
| `a.find(x)` | indeks albo -1 |
| `a.contains(x)` | bool |
| `a.sort()` | sortuje (liczby, znaki, teksty) |
| `a.sort_by(fn(x, y) { return x.wiek < y.wiek })` | sortuje według twojej zasady (stabilnie) |
| `a.filter(f)`, `a.map(f)` | nowa tablica: elementy, które f zostawia / wyniki f |
| `a.any(f)`, `a.all(f)` | czy f jest prawdą dla któregoś / każdego elementu |
| `a.reverse()` | odwraca kolejność |
| `a.slice(p, k)` | nowa tablica z elementów p … k-1 |
| `a.join(sep)` | `[]str` → jeden tekst |
| `a.ptr` | adres pierwszego elementu (dla C) |

---

## Mapy `map[K]V`

| Kod | Wynik |
|---|---|
| `m := ["a": 1, "b": 2]` | mapa (klucze: liczby, char, bool, str) |
| `m[k]` | wartość (brak klucza zatrzymuje program) |
| `m[k] = v` | dodaje albo podmienia |
| `m[k] += 1`, `m[k].push(x)` | brakujący klucz zaczyna od wartości domyślnej (0, `[]`, …) |
| `m.len` | liczba kluczy |
| `m.has(k)` | bool |
| `m.get(k, domyslna)` | wartość albo `domyslna` |
| `m.remove(k)` | bool: czy klucz był |
| `m.clear()` | opróżnia |
| `m.keys()`, `m.values()` | tablice, w kolejności dodania |

---

## Wektory `f32x8` (SIMD)

Typy: `f32x4/8/16`, `f64x2/4/8`, `i8x16/32/64`, `i16x8/16/32`, `i32x4/8/16`, `i64x2/4/8` i tak samo `u…`; najwyżej 512 bitów.

| Kod | Wynik |
|---|---|
| `f32x8(x)` | `x` w każdym elemencie |
| `f32x8(a, b, …, h)` | po jednej liczbie na element |
| `f32x8(lista, i)` | `lista[i]` … `lista[i+7]` (z `[]f32` albo `ptr[f32]`) |
| `f32x8(v)` | 8-elementowy wektor przeliczony element po elemencie (`f32x8(calkowite)`, `i32x8(ulamki)`) |
| `v + w`, `v * 2.0`, `-v` | element po elemencie; zwykła liczba działa na każdy element (`/` tylko dla liczb zmiennoprzecinkowych) |
| `& \| ^ << >> ~` | element po elemencie, dla liczb całkowitych |
| `v < w`, `v == w`, … | maska `boolx8`: `m.all()`, `m.any()`, `m.count()`, `m & n`, `!m`, `m[i]` |
| `select(m, a, b)` | element po elemencie: `a` tam, gdzie `m` jest prawdą, a gdzie nie, `b` |
| `v[i]`, `v[i] = x`, `v.len` | jeden element; liczba elementów |
| `min(v, w)`, `max`, `abs(v)`, `clamp(v, lo, hi)` | element po elemencie |
| `v.sum()`, `v.min()`, `v.max()`, `v.dot(w)` | jedna liczba ze wszystkich elementów |
| `v.sqrt()`, `v.floor()`, `v.ceil()`, `v.round()` | każdy element (liczby zmiennoprzecinkowe) |
| `v.fma(b, c)`, `v.muladd(b, c)` | `v * b + c`: zawsze z jednym zaokrągleniem / najszybciej na tym procesorze |
| `v.store(lista, i)` | zapisuje elementy do `lista[i]` … |
| `f32x8(lista, i, wypelnienie)`, `v.store_part(lista, i)` | koniec listy: tylko elementy, które są w niej (pozostałe to `wypelnienie`) |
| `v.shuffle(3, 2, 1, 0)`, `v.lo()`, `v.hi()`, `concat(a, b)` | przestawione elementy (shuffle przyjmuje numery zapisane w kodzie) |
| `tabela.lookup(idx)` | `tabela[idx % tabela.len]` dla każdego elementu (`pshufb`, `tbl`) |
| `acc.dot_add(a, b)` | sumy `i32x8` dostają po 4 iloczyny bajtów (`u8x32` × `i8x32`): `vpdpbusd`, `usdot` tam, gdzie są |
| `@cpu("avx2,fma") fn …`, `cpu_has("avx2")` | funkcja, która może używać tych rozszerzeń; przed jej wywołaniem sprawdź procesor |

---

## Tekst `str`

| Kod | Wynik |
|---|---|
| `s.len` | liczba bajtów |
| `s[i]`, `s[i] = 'x'` | znak |
| `a + b` | połączony tekst |
| `"x = \(x)"` | wartość dowolnego kodu wstawiona w tekst (`\(x, 2)`: liczba z dwiema cyframi po przecinku) |
| `s.sub(a, b)` | fragment a … b-1 |
| `s.find(t)` | pozycja albo -1 |
| `s.contains(t)` | bool |
| `s.starts_with(t)`, `s.ends_with(t)` | bool |
| `s.split(sep)` | `[]str` |
| `s.trim()` | bez spacji na końcach |
| `s.upper()`, `s.lower()` | A–Z / a–z |
| `s.replace(a, b)` | każde a zamienione na b |
| `s.repeat(n)` | tekst n razy |
| `s.bytes()` | `[]u8` |
| `s.ptr` | `ptr[char]` (dla C; parametr `const char *` przyjmie też samo `str`) |
| `==` `!=` `<` `>` | porównanie |

---

## Zamiana typów

| Kod | Wynik |
|---|---|
| `int(3.9)` | `3` |
| `int("42")` | `42` (zatrzymuje program, jeśli to nie liczba) |
| `int('A')` | `65` |
| `float(7)`, `float("2.5")` | `7`, `2.5` |
| `u8(300)` | `44` (zostają najniższe bity) |
| `char(66)` | `'B'` |
| `bool(0)` | `false` |
| `str(42)`, `str(1.5)`, `str(true)`, `str('c')` | tekst (liczba zmiennoprzecinkowa: najkrótszy zapis tej samej liczby) |
| `str(3.14159, 2)` | `"3.14"` (tyle cyfr po przecinku) |
| `str([1, 2])`, `str(m)`, `str(punkt)` | ten sam tekst, który pokazuje `print` |
| `int("x") or 0`, `float(s) or 0.0` | liczba albo wartość zapasowa |
| `str(bajty)` | `[]u8` → tekst |
| `str(p)` | `char*` z C → tekst |
| `ptr(p)`, `ptr(16)` | wskaźnik bez typu, ze wskaźnika albo z liczby |
| `int(p)` | wskaźnik → adres |

Automatycznie tylko wtedy, gdy nic nie ginie: mniejsza liczba → większa, liczba całkowita → zmiennoprzecinkowa, `f32` → `f64`.

---

## Funkcje wbudowane

| Funkcja | Co robi |
|---|---|
| `print(a, b, …)` | wypisuje wartości oddzielone spacjami i kończy linię |
| `input()`, `input("Pytanie? ")` | wczytuje linię |
| `read_file(sciezka)` | plik → str |
| `write_file(sciezka, tekst)` | str → plik, `true`, jeśli się udało |
| `file_exists(sciezka)` | bool |
| `delete_file(sciezka)` | bool |
| `shell(polecenie)` | uruchamia polecenie, zwraca jego kod wyjścia |
| `exit(kod)` | od razu kończy program (kończy też blok `or { }`) |
| `min(a, b, …)`, `max(a, b, …)`, `abs(x)`, `clamp(x, lo, hi)` | dla dowolnych liczb (twoja funkcja o tej samej nazwie ma pierwszeństwo) |
| `fma(a, b, c)`, `muladd(a, b, c)` | `a * b + c` z jednym zaokrągleniem / jedną instrukcją, gdzie procesor potrafi (zapisane `a * b + c` nigdy nie jest łączone) |
| `f32_bits(x)`, `f32_from_bits(u)`, `f64_bits`, `f64_from_bits` | bity liczby zmiennoprzecinkowej jako `u32`/`u64` i z powrotem, bez zmian |
| `f16_to_f32(h)`, `f32_to_f16(x)`, `bf16_to_f32(b)`, `f32_to_bf16(x)` | liczby połówkowej precyzji i bfloat16 (w `u16`) ↔ `f32`; także na wektorach |
| `error(komunikat)` | `return error("…")` w funkcji `-> T!` |
| `addr(x)` | wskaźnik na x; `addr(fn)` = funkcja zwrotna dla C |
| `asm("add {x}, {y}", mut x, y)` | instrukcje procesora (składnia Intela na x86); `eax: mut v` = ten rejestr, `scratch: "rcx"` |
| `assert(warunek)`, `assert(warunek, "dlaczego")` | zatrzymuje program, gdy warunek jest fałszywy (w bloku `test`: ten test nie przechodzi) |
| `atomic_add(p, n)`, `atomic_sub`, `atomic_load(p)`, `atomic_store(p, n)`, `atomic_swap(p, n)`, `atomic_cas(p, stara, nowa)` | liczba całkowita, którą zmieniają też inne wątki, przez jej `ptr` (`addr(licznik)`) |
| `new(wartosc)` | wartość na stercie → `ptr[T]` |
| `free(p)` | oddaje tę pamięć |

---

## Wskaźniki

| Finch | C |
|---|---|
| `ptr[int] p` | `int *p` |
| `addr(x)` | `&x` (zmienna-struktura podana tam, gdzie oczekiwany jest `ptr[TaStruktura]`: `push(cialo)` = `push(addr(cialo))`) |
| `p.value` | `*p` |
| `p.pole` | `p->pole` |
| `p[i]` | `p[i]` |
| `null` | `NULL` |
| `ptr` | `void *` |

---

## Pamięć

- Tablice, teksty i struktury zwalniają się same na końcu swojego bloku. `free` nie jest potrzebne.
- `b := a` kopiuje; wynik funkcji jest przenoszony; `return x` przenosi.
- Przekazywanie struktury: `fn f(Cialo c)` pracuje na własnej kopii (struktura wywołującego się nie zmienia), `fn f(ptr[Cialo] c)` zmienia strukturę wywołującego (`f(cialo)` albo `f(addr(cialo))`), metoda dostaje `self` przez adres, nigdy jako kopię. Struktura z co najmniej 256 bajtami liczb, przekazywana przez wartość albo w `for x in lista`, dostaje ostrzeżenie.
- `new(...)` z `free(...)` (często razem z `defer`) przydaje się do list wiązanych, drzew i współdzielenia.
- Pamięć z C zwalniasz funkcjami C (`malloc` / `free`, `fclose`, …).

---

## Moduły

```c
import ksztalty           // ksztalty.fch obok tego pliku (albo w FINCH_PATH)
ksztalty.pole(b)          // jego funkcje
ksztalty.Pudelko(2, 3)    // jego struktury
ksztalty.Pudelko inne     // w deklaracjach
import ui/button          // ui/button.fch: button.draw()
import ui/button as btn   // pod inną nazwą: btn.draw()
```

---

## C

| Kod | Znaczenie |
|---|---|
| `import "x.h"` | funkcje, struktury, enumy, liczby z `#define`, zmienne globalne |
| `link "nazwa"` | libnazwa.so / nazwa.lib (najpierw pyta pkg-config) |
| `link "plik.c"`, `link "glue.cpp"` | kompiluje twój plik C albo C++ (w C++ funkcje w `extern "C"`) |
| `addr(mojaFn)` | przekazuje funkcję Fincha jako funkcję zwrotną |

| Typ w C | W Finchu |
|---|---|
| `int` / `long long` / `unsigned` | `i32` / `i64` / `u32` (według rozmiaru) |
| `float` / `double` | `f32` / `f64` |
| `char *` (parametr, wynik) | `str` |
| `char *` (pole, za wskaźnikiem) | `ptr[char]` |
| `struct X` | `X` |
| `T *` | `ptr[T]` |
| `void *`, uchwyty | `ptr` |
| `T x[N]` (w strukturze) | `[N]T` |
| `union U` | `U`: `U u`, `u.pole`, `U(pole: v)`, do C przez wskaźnik (nie przez wartość) |
| `#define N 4`, `"tekst"`, `(1u << 3)`, `((Uint64)2)` | stałe (bez makr z parametrami) |
| `#define RAYWHITE (Color){ 245, 245, 245, 255 }` | wartość typu `Color` |

**`char *` to `str`**: funkcja C, która przyjmuje albo zwraca `char *`, przyjmuje albo zwraca `str` Fincha (bez `.ptr` i bez zamiany); tylko pole `char *` i `char **` zostają jako `ptr[char]`.
C++ i Rust: przez nagłówek C. C++ z `extern "C"` (i `link "x.cpp"`), Rust z `#[no_mangle] pub extern "C" fn`, zbudowany jako `staticlib` (`link "target/release/libgra.a"`), z nagłówkiem (może go napisać cbindgen).

Wynik `char *` funkcji C od razu jest `str`; struktury przez wartość działają w konwencjach wywołań System V, Windows x64 i ARM64 (Linux i macOS).

---

## Błędy w trakcie działania (program kończy się z plikiem i linią)

| Komunikat | Przyczyna |
|---|---|
| `division by zero` | `/` albo `%` przez 0 |
| `index 5 is out of range (the length is 3)` | `a[5]` |
| `pop() on an empty array` | |
| `used .value on a null pointer` | `.value` / `.pole` / `[i]` przez `null` |
| `can't turn "x" into int` | `int("x")` |
| `can't read the file "…"` | `read_file` (bez `or`) |
| `the key "x" is not in the map` | `m["x"]` |
| `assert failed: dlaczego` | `assert(warunek, "dlaczego")` |

Pod komunikatem pojawiają się wywołania, które do niego doprowadziły (`finch run`, `finch test`, `-g`, `-O0`, `--trace`):

```
gra.fch:6: runtime error: index 3 is out of range (the length is 2)
  in Wrog.nastepny (gra.fch:6)
  in krok (gra.fch:10)
  in main (gra.fch:15)
```

## Testy

```c
test "wysokość skoku" {         // obok kodu; uruchamia je finch test plik.fch, finch run/build je pomijają
    p := Gracz()
    p.skocz()
    assert(p.y > 0.0, "y wynosi \(p.y)")
}
```

---

## Edytory

Kate: `editors/kate/install.sh`, potem włącz wtyczkę **LSP Client**; uruchamianie przez **Build & Run** → `finch run %f`.
Dowolny edytor z LSP: `finch lsp`.

### VS Code (rozszerzenie „Finch”)

| Klawisz / akcja | Co robi |
|---|---|
| ▶ / `Ctrl+F5` | uruchamia plik |
| `Finch: Build This File` | buduje; błędy trafiają do panelu Problems |
| `F12` | przejście do definicji |
| najechanie kursorem | typ / sygnatura / opis |
| `Ctrl+Spacja` | podpowiedzi (po `.`: pola, metody, zawartość modułu) |
| `Ctrl+Shift+O` | funkcje i struktury w pliku |
| `main`, `fn`, `fnr`, `fnf`, `method`, `struct`, `for`, `foreach`, `forkv`, `map`, `orb`, `if`, `ife`, `while`, `input` + Tab | gotowe fragmenty kodu |
| ustawienie `finch.path` | gdzie leży finch / finch.exe |

---

## Enumy i `match`

```
enum Kolor { Czerwony, Zielony, Niebieski }    enum Status { Ok = 200, NotFound = 404 }
k := Kolor.Zielony    int(k)    Kolor(2)    k == Kolor.Czerwony   // tylko == i !=; Kolor(7) to błąd
fn Kolor.nastepny() -> Kolor { ... }                               // metody działają

match k { Czerwony -> ...   Zielony, Niebieski -> { ... }   else -> ... }  // same nazwy wariantów; każdy wariant albo else
x := match n { 1, 2 -> "kilka"   else -> "dużo" }                          // wyrażenie: każda gałąź tego samego typu
```

## Typy generyczne

```
fn max[T](T a, T b) -> T { ... }     max(3, 9)     max[float](1, 2.5)    // typ wywnioskowany albo podany
struct Stos[T] { []T el }            fn Stos[T].push(T x) { ... }        // typ zawsze idzie z nazwą:
s := Stos[int]()     Stos[int] t     Para[str, int]("wiek", 31)          //   nigdy samo Stos()
```

## Biblioteka standardowa (`import nazwa`; twój plik o tej samej nazwie ma pierwszeństwo)

| | |
|---|---|
| `path` | `join(a, b)` `base(p)` `dir(p)` `ext(p)` `stem(p)` `parts(p)` `clean(p)` `is_abs(p)` |
| `time` | `now()` `millis()` `sleep(ms)` `date(t)` `local(t)` `format(t, wzor)` `days_in_month(r, m)` |
| `proc` | `run(polecenie)` `output(polecenie)` (przy błędzie zwraca jego wyjście) `env(nazwa, domyslna)` `pid()` `cwd()` |
| `thread` | `spawn(addr(praca), addr(dane))` → `t.join()`; `mutex()` → `m.lock()` `m.unlock()` `m.close()`; `cpus()` (funkcja wątku: `fn praca(ptr arg)`) |
| `fs` | `read_bytes(p)` `write_bytes(p, dane)` `exists(p)` `is_dir(p)` `mkdir(p)` `list(katalog)` `remove_dir(p)` `data_dir(aplikacja)` |
| `json` | `parse(tekst)` `stringify(v)` `pretty(v)` `new_text/number/bool/null/list/object` `v.get(k)` `v.at(i)` `v.set(k, x)` `v.push(x)` `v.has(k)` `v.str_or(d)` `v.int_or(d)` `v.float_or(d)` `v.bool_or(d)` |

## Słowa kluczowe

`fn return if else while for in break continue true false null import link struct enum match defer or try const export mut`
(`test` i `as` tylko tam, gdzie coś znaczą: `test "nazwa" { }`, `import x as y`)
(oraz `self` wewnątrz metod)
