# Ściąga Fincha

Cały Finch na jednej stronie. Wyjaśnienia są w [przewodnikach](../../README.pl.md#dokumentacja).

> 🇬🇧 English version: [cheatsheet.md](../en/cheatsheet.md)

---

## Polecenie `finch`

| Polecenie | Robi |
|---|---|
| `finch run plik.fch [argumenty…]` | skompiluj i uruchom (argumenty trafiają do programu) |
| `finch build plik.fch [-o nazwa]` | zrób program (`nazwa.exe` na Windowsie) |
| `finch build plik.fch --shared` | zrób bibliotekę (`libplik.so`, `plik.dll`) dla programu w C; jej `export fn` to jej funkcje C |
| `finch test plik.fch` | uruchom jego bloki `test "…" { }`, każdy osobno (także z importowanych modułów) |
| `finch fmt [--check] [pliki/foldery]` | wcięcia wg `{ }` po 4 spacje, bez spacji na końcach linii, najwyżej jedna pusta linia (`--check`: tylko wypisz, co by zmienił) |
| `finch run` / `build` / `test` bez pliku | w folderze z `finch.toml`: jego `entry` |
| `finch ir plik.fch` | pokaż LLVM IR |
| `finch lsp` | serwer języka (korzystają z niego edytory) |
| `finch version` | wersja |
| `-l lib` | dolinkuj bibliotekę C (jak `link "lib"`) |
| `--target windows` / `linux` / `arm64` / `macos` / `android` / triple | zbuduj dla innego systemu (`android`: clang z NDK wskazanego przez `ANDROID_NDK_HOME`, zwykle z `--shared`) |
| `-g` | informacje dla debuggera (gdb, lldb, Visual Studio) |
| `-O0` | bez optymalizacji |
| `--trace` / `--no-trace` | błędy w czasie działania pokazują łańcuch wywołań; włączone dla `run`, `test`, `-g`, `-O0`, wyłączone dla zwykłego `build` (kosztuje szybkość) |

Zmienne środowiskowe: `FINCH_PATH` (foldery z modułami), `FINCH_CC` (kompilator C, który linkuje), `FINCH_CXX` (dla plików `.cpp`).
Skompilowany kod programu i jego pliki `link "x.c"` zostają w cache (`~/.cache/finch`), dopóki one albo ich nagłówki się nie zmienią.

`finch.toml` (obok kodu; `finch run` w tym folderze nie potrzebuje pliku ani flag):

```toml
name = "gra"                        # co robi finch build
entry = "src/main.fch"
paths = ["src"]                     # foldery z modułami
include = ["vendor/SDL3/include"]   # nagłówki C
links = ["SDL3", "native/glue.cpp"] # biblioteki i własne .c / .cpp / .o / .a
libdirs = ["vendor/SDL3/lib"]
cflags = ["-DFAST"]                 # dla własnych plików .c / .cpp
ldflags = ["-Wl,-rpath,."]

[target.windows]                    # tylko przy budowaniu dla tego systemu (też linux, macos, android, linux-arm64)
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

`main` to `fn main()`, `fn main() -> int` (kod wyjścia) albo `fn main([]str args)`.
Jedna instrukcja na linijkę, bez średników, `{` w tej samej linijce, bez nawiasów wokół warunków.

---

## Zmienne

| Kod | Znaczenie |
|---|---|
| `x := 5` | nowa zmienna, typ zgadnięty |
| `int x = 5` | nowa zmienna, typ podany |
| `int x` | zero / `""` / `[]` / `null` / wartości domyślne struktury (też struktur w niej) |
| `x = 7` | przypisanie |
| `x += 1` `-=` `*=` `/=` `%=` | zmiana |
| `const W = 640`, `const f32 G = 9.8` | stała (na najwyższym poziomie pliku; `ksztalty.W` z modułu) |

---

## Typy

| Typ | Przykład |
|---|---|
| `int` (= `i64`) | `42`, `-7`, `0xFF`, `1_000_000` |
| `float` (= `f64`) | `3.14`, `1e9`, `2.5e-3` |
| `bool` | `true` (prawda), `false` (fałsz) |
| `char` | `'A'`, `'\n'` |
| `str` | `"tekst\t\"w cudzysłowie\""`, `"hp \(hp), prędkość \(v, 2)"` (kod w tekście; `, 2` = dwie cyfry po przecinku), `"""` kilka linii `"""` (wcięcie zamykających cudzysłowów znika) |
| `i8 i16 i32 i64` | ze znakiem, z rozmiarem |
| `u8 u16 u32 u64` | bez znaku, z rozmiarem |
| `f32 f64` | zmiennoprzecinkowe |
| `[]T` | tablica: `[1, 2, 3]`, `[]str imiona` |
| `map[K]V` | mapa: `["a": 1]`, `map[str]int ile`, `[:]` |
| `ptr[T]`, `ptr` | wskaźnik, wskaźnik bez typu |
| `Nazwa` / `modul.Nazwa` | struktura |

Znaki specjalne: `\n \t \r \0 \\ \" \'`.

---

## Operatory

| Priorytet | Operatory |
|---|---|
| 5 | `*` `/` `%` `<<` `>>` `&` |
| 4 | `+` `-` `\|` `^` (`+` skleja też tekst) |
| 3 | `==` `!=` `<` `<=` `>` `>=` |
| 2 | `&&` (i) |
| 1 | `\|\|` (lub) |
| jednoargumentowe | `-x` `!x` (nie) `~x` |

`x & 1 == 0` znaczy `(x & 1) == 0`.

---

## Sterowanie

```c
if a > b { … } else if a == b { … } else { … }
x := if a > b { a } else { b }  // jako wartość: jedna wartość na gałąź, else wymagane
while x > 0 { … }
for i in 0..10 { … }          // 0 … 9
for el in lista { … }         // każdy element (albo znak str)
for i, el in lista { … }      // razem z indeksem
for klucz, wart in m { … }    // każdy wpis mapy, w kolejności dodawania
break     continue     return wartosc
defer sprzatanie()            // uruchomi się na końcu bloku
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

Parametry zachowują się jak kopie. Kolejność w pliku nie ma znaczenia. Przecinek po ostatnim argumencie jest dozwolony.

---

## Metody

```c
fn Punkt.przesun(int dx, int dy) {   // self to Punkt, na którym wywołano (nie kopia)
    self.x += dx
    self.y += dy
}
fn Punkt.odleglosc() -> float { … }

p.przesun(1, 2)       lista[0].przesun(1, 1)       wsk_na_punkt.przesun(0, 1)
```

Także na strukturach z C: `fn Vector2.length() -> f32 { … }`.

---

## Błędy jako wartości

```c
fn parsuj(str s) -> int! {           // ! = może się nie udać
    if s.len == 0 {
        return error("pusty")
    }
    return int(s) or { return error("to nie liczba: " + s) }
}
fn zapisz(str p, str t) -> ! {       // może się nie udać, bez wyniku
    try write_file(p, t)
}
```

| Kod | Przy porażce |
|---|---|
| `x := parsuj(s) or 0` | użyj 0 |
| `x := parsuj(s) or { print(err); return }` | wykonaj blok (`err`: komunikat) |
| `x := try parsuj(s)` | zakończ porażką też bieżącą funkcję |
| `fn main() -> !` | porażka wypisuje `error: …`, kod wyjścia 1 |

Funkcje wbudowane, które mogą się nie udać: `int(s)`, `float(s)`, `read_file`, `write_file`, `delete_file`.

---

## Struktury

| Kod | Znaczenie |
|---|---|
| `struct P { … }` | definicja (jedno pole na linijkę, `= domyślna` opcjonalnie) |
| `P(1, 2)` | utworzenie, wszystkie pola po kolei |
| `P(x: 1)` | utworzenie po nazwie, reszta domyślna/zero |
| `p.x`, `p.x = 5` | odczyt, zmiana pola |
| `b := a` | pełna kopia |

---

## Tablice `[]T`

| Kod | Daje |
|---|---|
| `a.len` | liczba elementów |
| `a[i]`, `a[i] = x` | element (z kontrolą zakresu) |
| `a.push(x)` | dodaj na końcu |
| `a.pop()` | zabierz ostatni i go oddaj |
| `a.insert(i, x)` | wstaw na pozycję i |
| `a.remove(i)` | usuń pozycję i i ją oddaj |
| `a.clear()` | wyczyść |
| `a.resize(n)` | długość n (nowe elementy to zera) |
| `a.find(x)` | indeks albo -1 |
| `a.contains(x)` | bool |
| `a.sort()` | posortuj (liczby, znaki, str) |
| `a.reverse()` | odwróć |
| `a.slice(s, e)` | nowa tablica z s … e-1 |
| `a.join(sep)` | `[]str` → jeden str |
| `a.ptr` | adres pierwszego elementu (dla C) |

---

## Mapy `map[K]V`

| Kod | Daje |
|---|---|
| `m := ["a": 1, "b": 2]` | mapa (klucze: liczby, char, bool, str) |
| `m[k]` | wartość (brak klucza: program się zatrzymuje) |
| `m[k] = v` | dodaj / zastąp |
| `m[k] += 1`, `m[k].push(x)` | brakujący klucz zaczyna od wartości domyślnej (0, `[]`, …) |
| `m.len` | liczba kluczy |
| `m.has(k)` | czy jest |
| `m.get(k, domyslna)` | wartość albo domyślna |
| `m.remove(k)` | bool: czy był |
| `m.clear()` | wyczyść |
| `m.keys()`, `m.values()` | tablice, w kolejności dodawania |

---

## Tekst `str`

| Kod | Daje |
|---|---|
| `s.len` | liczba bajtów (ą, ż… to 2) |
| `s[i]`, `s[i] = 'x'` | znak |
| `a + b` | sklejony tekst |
| `"x = \(x)"` | wartość dowolnego kodu w tekście (`\(x, 2)`: liczba z 2 cyframi po przecinku) |
| `s.sub(a, b)` | fragment a … b-1 |
| `s.find(t)` | pozycja albo -1 |
| `s.contains(t)` | czy zawiera |
| `s.starts_with(t)`, `s.ends_with(t)` | czy zaczyna / kończy się |
| `s.split(sep)` | `[]str` |
| `s.trim()` | bez spacji na końcach |
| `s.upper()`, `s.lower()` | A–Z / a–z |
| `s.replace(a, b)` | każde a → b |
| `s.repeat(n)` | n razy |
| `s.bytes()` | `[]u8` |
| `s.ptr` | `ptr[char]` (dla C; `str` trafia też do parametru `const char *` bez zmian) |
| `==` `!=` `<` `>` | porównanie |

---

## Zamiana typów

| Kod | Wynik |
|---|---|
| `int(3.9)` | `3` |
| `int("42")` | `42` (zatrzymuje program, jeśli to nie liczba) |
| `int('A')` | `65` |
| `float(7)`, `float("2.5")` | `7`, `2.5` |
| `u8(300)` | `44` (zostawia najniższe bity) |
| `char(66)` | `'B'` |
| `bool(0)` | `false` |
| `str(42)`, `str(1.5)`, `str(true)`, `str('c')` | tekst (float: najkrótszy tekst, który jest tą samą liczbą) |
| `str(3.14159, 2)` | `"3.14"` (tyle cyfr po kropce) |
| `str([1, 2])`, `str(m)`, `str(punkt)` | ten sam tekst, który pokazuje `print` |
| `int("x") or 0`, `float(s) or 0.0` | liczba albo wartość zapasowa |
| `str(bajty)` | `[]u8` → tekst |
| `str(p)` | `char*` z C → tekst |
| `ptr(p)`, `ptr(16)` | wskaźnik bez typu, ze wskaźnika albo liczby |
| `int(p)` | wskaźnik → adres |

Automatycznie tylko wtedy, gdy nic nie ginie: mniejsza → większa liczba, dowolna całkowita → float, `f32` → `f64`.

---

## Funkcje wbudowane

| Funkcja | Robi |
|---|---|
| `print(a, b, …)` | wypisz, spacje między, nowa linia na końcu |
| `input()`, `input("Pytanie? ")` | wczytaj linijkę |
| `read_file(sciezka)` | plik → str |
| `write_file(sciezka, tekst)` | str → plik, `true`, jeśli się udało |
| `file_exists(sciezka)` | czy plik istnieje |
| `delete_file(sciezka)` | usuń plik |
| `shell(polecenie)` | uruchom polecenie, jego kod wyjścia |
| `exit(kod)` | zakończ program od razu (kończy też blok `or { }`) |
| `min(a, b, …)`, `max(a, b, …)`, `abs(x)`, `clamp(x, od, do)` | dla dowolnych liczb (twoja funkcja o tej nazwie wygrywa) |
| `error(komunikat)` | `return error("…")` w funkcji `-> T!` |
| `addr(x)` | wskaźnik na x; `addr(fn)` = callback dla C |
| `assert(warunek)`, `assert(warunek, "dlaczego")` | zatrzymaj się z błędem, gdy warunek jest fałszywy (w bloku `test`: ten test nie przechodzi) |
| `atomic_add(p, n)`, `atomic_sub`, `atomic_load(p)`, `atomic_store(p, n)`, `atomic_swap(p, n)`, `atomic_cas(p, stara, nowa)` | liczba całkowita zmieniana też przez inne wątki, przez jej `ptr` (`addr(licznik)`) |
| `new(wartosc)` | wartość na stercie → `ptr[T]` |
| `free(p)` | oddaj ją |

---

## Wskaźniki

| Finch | C |
|---|---|
| `ptr[int] p` | `int *p` |
| `addr(x)` | `&x` (zmienna-struktura podana tam, gdzie idzie `ptr[TaStruktura]`: `pchnij(cialo)` = `pchnij(addr(cialo))`) |
| `p.value` | `*p` |
| `p.pole` | `p->pole` |
| `p[i]` | `p[i]` |
| `null` | `NULL` |
| `ptr` | `void *` |

---

## Pamięć

- Tablice, teksty i struktury zwalniają się same na końcu swojego bloku. Bez `free`.
- `b := a` kopiuje; wynik wywołania jest przenoszony; `return x` przenosi.
- Przekazywanie struktury: `fn f(Cialo c)` pracuje na własnej kopii (ta u wywołującego się nie zmienia), `fn f(ptr[Cialo] c)` zmienia tę u wywołującego (`f(cialo)` albo `f(addr(cialo))`), metoda dostaje `self` przez adres, nigdy kopię.
- `new(...)` + `free(...)` (często z `defer`) do list wiązanych, drzew, współdzielenia.
- Pamięć z C: funkcjami z C (`malloc` / `free`, `fclose`, …).

---

## Moduły

```c
import ksztalty          // ksztalty.fch obok tego pliku (albo w FINCH_PATH)
ksztalty.pole(b)         // jego funkcje
ksztalty.Pudlo(2, 3)     // jego struktury
ksztalty.Pudlo inne      // w deklaracjach
import ui/przycisk       // ui/przycisk.fch: przycisk.rysuj()
import ui/przycisk as p  // inna nazwa: p.rysuj()
```

---

## C

| Kod | Znaczenie |
|---|---|
| `import "x.h"` | funkcje, struktury, enumy, liczbowe `#define`, zmienne globalne |
| `link "nazwa"` | libnazwa.so / nazwa.lib (najpierw pkg-config) |
| `link "plik.c"`, `link "glue.cpp"` | skompiluj własny plik C albo C++ (C++: funkcje w `extern "C"`) |
| `addr(mojaFn)` | funkcja Fincha jako callback |

| Typ C | Finch |
|---|---|
| `int` / `long long` / `unsigned` | `i32` / `i64` / `u32` (wg rozmiaru) |
| `float` / `double` | `f32` / `f64` |
| `char *` (parametr, wynik) | `str` |
| `char *` (pole, za wskaźnikiem) | `ptr[char]` |
| `struct X` | `X` |
| `T *` | `ptr[T]` |
| `void *`, uchwyty | `ptr` |
| `T x[N]` (w strukturze) | `[N]T` |
| `union U` | `U`: `U u`, `u.pole`, `U(pole: v)`, przez wskaźnik do C (nie przez wartość) |
| `#define N 4`, `"tekst"`, `(1u << 3)`, `((Uint64)2)` | stałe (bez makr-funkcji) |
| `#define RAYWHITE (Color){ 245, 245, 245, 255 }` | `Color` |

**`char *` to `str`**: funkcja C, która bierze albo zwraca `char *`, bierze albo daje `str` Fincha (bez `.ptr`, bez zamiany); tylko pole `char *` i `char **` zostają `ptr[char]`.
C++ i Rust: przez nagłówek C. C++ z `extern "C"` (i `link "x.cpp"`), Rust z `#[no_mangle] pub extern "C" fn` zbudowany jako `staticlib` (`link "target/release/libgra.a"`) i nagłówek (napisze go cbindgen).

Wynik `char *` z funkcji C to od razu `str`; struktury przez wartość działają w konwencjach System V, Windows x64 i ARM64 Linux (sprawdzone w emulatorach; macOS przez CI).

---

## Błędy w czasie działania (program zatrzymuje się z plikiem i linijką)

| Komunikat | Przyczyna |
|---|---|
| `division by zero` | `/` albo `%` przez 0 |
| `index 5 is out of range (the length is 3)` | `a[5]` |
| `pop() on an empty array` | `pop()` na pustej tablicy |
| `used .value on a null pointer` | `.value` / `.pole` / `[i]` przez `null` |
| `can't turn "x" into int` | `int("x")` |
| `can't read the file "…"` | `read_file` (bez `or`) |
| `the key "x" is not in the map` | `m["x"]` |
| `assert failed: dlaczego` | `assert(warunek, "dlaczego")` |

Pod komunikatem wywołania, które do niego doprowadziły (`finch run`, `finch test`, `-g`, `-O0`, `--trace`):

```
gra.fch:6: runtime error: index 3 is out of range (the length is 2)
  in Enemy.next (gra.fch:6)
  in step (gra.fch:10)
  in main (gra.fch:15)
```

## Testy

```c
test "wysokość skoku" {         // obok kodu; finch test plik.fch je uruchamia, finch run/build je pomijają
    g := Gracz()
    g.skocz()
    assert(g.y > 0.0, "y to \(g.y)")
}
```

---

## Edytory

Kate: `editors/kate/install.sh`, potem włącz wtyczkę **LSP Client**; uruchamianie przez **Build & Run** → `finch run %f`.
Każdy edytor z LSP: `finch lsp`.

### VS Code (rozszerzenie „Finch”)

| Klawisz / akcja | Robi |
|---|---|
| ▶ / `Ctrl+F5` | uruchom plik |
| `Finch: Build This File` | zbuduj; błędy trafiają do panelu Problems |
| `F12` | idź do definicji |
| najechanie myszą | typ / sygnatura / opis |
| `Ctrl+Spacja` | podpowiedzi (po `.`: pola, metody, zawartość modułu) |
| `Ctrl+Shift+O` | funkcje i struktury w pliku |
| `main`, `fn`, `fnr`, `fnf`, `method`, `struct`, `for`, `foreach`, `forkv`, `map`, `orb`, `if`, `ife`, `while`, `input` + Tab | szablony |
| ustawienie `finch.path` | gdzie jest finch / finch.exe |

---

## Enumy i `match`

```
enum Kolor { Czerwony, Zielony }          enum Status { Ok = 200, Brak = 404 }
k := Kolor.Zielony    int(k)    Kolor(1)    k == Kolor.Czerwony   // tylko == i !=; Kolor(7) to błąd
fn Kolor.nastepny() -> Kolor { ... }                               // metody działają

match k { Czerwony -> ...   Zielony -> { ... }   else -> ... }     // same nazwy wariantów; każdy wariant albo else
x := match n { 1, 2 -> "mało"   else -> "dużo" }                   // wyrażenie: gałęzie tego samego typu
```

## Typy generyczne

```
fn max[T](T a, T b) -> T { ... }     max(3, 9)     max[float](1, 2.5)    // wynika z argumentów albo zapisane
struct Stos[T] { []T elementy }      fn Stos[T].dodaj(T x) { ... }       // typ zawsze podaje się przy tworzeniu:
s := Stos[int]()     Stos[int] t     Para[str, int]("wiek", 31)          //   nigdy samo Stos()
```

## Biblioteka standardowa (`import nazwa`; własny plik o tej nazwie ma pierwszeństwo)

| | |
|---|---|
| `path` | `join(a, b)` `base(p)` `dir(p)` `ext(p)` `stem(p)` `parts(p)` `clean(p)` `is_abs(p)` |
| `time` | `now()` `millis()` `sleep(ms)` `date(t)` `local(t)` `format(t, fmt)` `days_in_month(r, m)` |
| `proc` | `run(cmd)` `output(cmd)` (błąd z jego wyjściem) `env(nazwa, domyślna)` `pid()` `cwd()` |
| `thread` | `spawn(addr(praca), addr(dane))` → `t.join()`; `mutex()` → `m.lock()` `m.unlock()` `m.close()`; `cpus()` (funkcja wątku: `fn praca(ptr arg)`) |
| `fs` | `read_bytes(p)` `write_bytes(p, dane)` `exists(p)` `is_dir(p)` `mkdir(p)` `list(folder)` `remove_dir(p)` `data_dir(app)` |
| `json` | `parse(tekst)` `stringify(v)` `pretty(v)` `new_text/number/bool/null/list/object` `v.get(k)` `v.at(i)` `v.set(k, x)` `v.push(x)` `v.has(k)` `v.str_or(d)` `v.int_or(d)` `v.float_or(d)` `v.bool_or(d)` |

## Słowa kluczowe

| Słowo | Po polsku |
|---|---|
| `fn` | funkcja |
| `return` | zwróć |
| `if` / `else` | jeżeli / w przeciwnym razie |
| `while` | dopóki |
| `for` … `in` | dla … w |
| `break` / `continue` | przerwij / kontynuuj |
| `true` / `false` / `null` | prawda / fałsz / nic |
| `import` / `link` | zaimportuj / dolinkuj |
| `struct` | struktura |
| `enum` | wyliczenie |
| `match` | dopasuj |
| `defer` | odłóż (na koniec bloku) |
| `or` | albo (co zrobić przy porażce) |
| `try` | spróbuj (porażkę przekaż dalej) |
| `const` | stała |
| `export` | wystaw (funkcja dla programu w C) |
| `self` | sam (w metodzie: struktura, na której ją wywołano) |
| `test` | test (tylko jako `test "nazwa" { }`) |
| `as` | jako (tylko w `import x as y`) |
| `map` | mapa |
