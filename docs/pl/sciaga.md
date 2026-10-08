# Ściąga Fincha

Cały Finch na jednej stronie. Wyjaśnienia są w [przewodnikach](../../README.pl.md#dokumentacja).

> 🇬🇧 English version: [cheatsheet.md](../en/cheatsheet.md)

---

## Polecenie `finch`

| Polecenie | Robi |
|---|---|
| `finch run plik.fch [argumenty…]` | skompiluj i uruchom (argumenty trafiają do programu) |
| `finch build plik.fch [-o nazwa]` | zrób program (`nazwa.exe` na Windowsie) |
| `finch ir plik.fch` | pokaż LLVM IR |
| `finch lsp` | serwer języka (korzystają z niego edytory) |
| `finch version` | wersja |
| `-l lib` | dolinkuj bibliotekę C (jak `link "lib"`) |
| `--target windows` / `linux` / triple | zbuduj dla innego systemu |
| `-g` | informacje dla debuggera (gdb, lldb, Visual Studio) |
| `-O0` | bez optymalizacji |

Zmienne środowiskowe: `FINCH_PATH` (foldery z modułami), `FINCH_CC` / `CC` (kompilator C, który linkuje).

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
| `int x` | zero / `""` / `[]` / `null` / wartości domyślne struktury |
| `x = 7` | przypisanie |
| `x += 1` `-=` `*=` `/=` `%=` | zmiana |

---

## Typy

| Typ | Przykład |
|---|---|
| `int` (= `i64`) | `42`, `-7`, `0xFF`, `1_000_000` |
| `float` (= `f64`) | `3.14` |
| `bool` | `true` (prawda), `false` (fałsz) |
| `char` | `'A'`, `'\n'` |
| `str` | `"tekst\t\"w cudzysłowie\""` |
| `i8 i16 i32 i64` | ze znakiem, z rozmiarem |
| `u8 u16 u32 u64` | bez znaku, z rozmiarem |
| `f32 f64` | zmiennoprzecinkowe |
| `[]T` | tablica: `[1, 2, 3]`, `[]str imiona` |
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
while x > 0 { … }
for i in 0..10 { … }          // 0 … 9
for el in lista { … }         // każdy element (albo znak str)
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

## Tekst `str`

| Kod | Daje |
|---|---|
| `s.len` | liczba bajtów (ą, ż… to 2) |
| `s[i]`, `s[i] = 'x'` | znak |
| `a + b` | sklejony tekst |
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
| `s.ptr` | `ptr[char]` (dla C) |
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
| `str(42)`, `str(1.5)`, `str(true)`, `str('c')` | tekst |
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
| `exit(kod)` | zakończ program od razu |
| `addr(x)` | wskaźnik na x; `addr(fn)` = callback dla C |
| `new(wartosc)` | wartość na stercie → `ptr[T]` |
| `free(p)` | oddaj ją |

---

## Wskaźniki

| Finch | C |
|---|---|
| `ptr[int] p` | `int *p` |
| `addr(x)` | `&x` |
| `p.value` | `*p` |
| `p.pole` | `p->pole` |
| `p[i]` | `p[i]` |
| `null` | `NULL` |
| `ptr` | `void *` |

---

## Pamięć

- Tablice, teksty i struktury zwalniają się same na końcu swojego bloku. Bez `free`.
- `b := a` kopiuje; wynik wywołania jest przenoszony; `return x` przenosi.
- `new(...)` + `free(...)` (często z `defer`) do list wiązanych, drzew, współdzielenia.
- Pamięć z C: funkcjami z C (`malloc` / `free`, `fclose`, …).

---

## Moduły

```c
import ksztalty          // ksztalty.fch obok tego pliku (albo w FINCH_PATH)
ksztalty.pole(b)         // jego funkcje
ksztalty.Pudlo(2, 3)     // jego struktury
ksztalty.Pudlo inne      // w deklaracjach
```

---

## C

| Kod | Znaczenie |
|---|---|
| `import "x.h"` | funkcje, struktury, enumy, liczbowe `#define`, zmienne globalne |
| `link "nazwa"` | libnazwa.so / nazwa.lib (najpierw pkg-config) |
| `link "plik.c"` | skompiluj własny plik C |
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

---

## Błędy w czasie działania (program zatrzymuje się z plikiem i linijką)

| Komunikat | Przyczyna |
|---|---|
| `division by zero` | `/` albo `%` przez 0 |
| `index 5 is out of range (the length is 3)` | `a[5]` |
| `pop() on an empty array` | `pop()` na pustej tablicy |
| `used .value on a null pointer` | `.value` / `.pole` / `[i]` przez `null` |
| `can't turn "x" into int` | `int("x")` |
| `can't read the file "…"` | `read_file` |

---

## VS Code (rozszerzenie „Finch”)

| Klawisz / akcja | Robi |
|---|---|
| ▶ / `Ctrl+F5` | uruchom plik |
| `Finch: Build This File` | zbuduj; błędy trafiają do panelu Problems |
| `F12` | idź do definicji |
| najechanie myszą | typ / sygnatura / opis |
| `Ctrl+Spacja` | podpowiedzi (po `.`: pola, metody, zawartość modułu) |
| `Ctrl+Shift+O` | funkcje i struktury w pliku |
| `main`, `fn`, `fnr`, `struct`, `for`, `foreach`, `if`, `ife`, `while`, `input` + Tab | szablony |
| ustawienie `finch.path` | gdzie jest finch / finch.exe |

---

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
| `defer` | odłóż (na koniec bloku) |
