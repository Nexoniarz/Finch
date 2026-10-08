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
10. [Tablice](#10-tablice)
11. [Tekst (str)](#11-tekst-str)
12. [Struktury](#12-struktury)
13. [Pamięć: kto co zwalnia](#13-pamięć-kto-co-zwalnia)
14. [Wskaźniki](#14-wskaźniki)
15. [Funkcje wbudowane](#15-funkcje-wbudowane)
16. [Moduły](#16-moduły)
17. [Biblioteki z C](#17-biblioteki-z-c)
18. [Błędy](#18-błędy)
19. [Debugowanie](#19-debugowanie)
20. [Rozwiązywanie problemów](#20-rozwiązywanie-problemów)
21. [Struktura projektu i testy](#21-struktura-projektu-i-testy)
22. [Obecne ograniczenia](#22-obecne-ograniczenia)

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

Finch działa obecnie na **Linuksie x86-64**. Testowany był z **LLVM 21**.
Z innymi wersjami LLVM prawdopodobnie się nie skompiluje, bo API C++ LLVM zmienia się między wydaniami.

### Opcja A: Nix (zalecana, nic nie instalujesz ręcznie)

```sh
git clone https://github.com/Nexoniarz/Finch.git
cd Finch
nix-shell                        # pobiera LLVM, libclang, clang, cmake, ninja, pkg-config
cmake -S . -B build -G Ninja
ninja -C build
./build/finch version            # finch 2.0.0 (LLVM 21.x)
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
finch version                          pokaż wersję

opcje:
  -l <lib>   dolinkuj bibliotekę C, to samo co  link "lib"  w pliku
  -g         dodaj informacje dla debuggera (gdb / lldb)
  -O0        bez optymalizacji
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

---

## 10. Tablice

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

## 12. Struktury

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
- Struktury nie mają metod. Pisz funkcje, które je przyjmują: `fn wylecz(Gracz g) -> Gracz`.

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
| `read_file(sciezka)` | Cały plik jako `str`. Jeśli nie da się go przeczytać, zatrzymuje program z czytelnym komunikatem. |
| `write_file(sciezka, tekst)` | Zapisuje (zastępuje) plik; oddaje `true`, jeśli się udało. |
| `file_exists(sciezka)` | `true` / `false` |
| `shell(polecenie)` | Uruchamia polecenie powłoki i oddaje jego kod wyjścia. |
| `exit(kod)` | Natychmiast kończy program. |
| `addr(x)` | Wskaźnik na `x`; `addr(funkcja)` daje C wskaźnik na funkcję (callback). |
| `new(wartosc)`, `free(p)` | Ręczna pamięć na stercie (zobacz [Pamięć](#13-pamięć-kto-co-zwalnia)). |
| `int(x)`, `str(x)`, `u8(x)`, … | Zamiany typów (zobacz [Zamiana typów](#zamiana-typów)). |

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

### Co jeszcze nie działa

- **Unie** C przez wartość i struktury z **polami bitowymi** przez wartość (wskaźniki do nich działają).
- **Makra-funkcje** i makra, które nie są pojedynczą liczbą.
- `long double`, liczby 128-bitowe.

---

## 18. Błędy

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
| `runtime error: can't read the file "…"` | `read_file` na brakującym albo nieczytelnym pliku |

---

## 19. Debugowanie

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
zmienne widoczne; bez niego optymalizator może część z nich usunąć.

---

## 20. Rozwiązywanie problemów

| Problem | Rozwiązanie |
|---|---|
| `can't find the C header 'x.h'` | Brakuje plików deweloperskich biblioteki. Zainstaluj je (Debian: `libx-dev`; Nix: dopisz do `shell.nix`). |
| `… come from "x.h", but its library isn't linked` | Dopisz `link "nazwa"` na górze pliku. |
| `the library 'x' wasn't found` | Nie jest zainstalowana albo nazwa jest zła. Na Niksie dopisz ją do `shell.nix` i pracuj w `nix-shell`. |
| `the C function 'f' can't be used from Finch yet` | Używa unii, pól bitowych albo innego nieobsługiwanego typu przez wartość. Poszukaj wariantu ze wskaźnikami. |
| `can't find the module 'x'` | Połóż `x.fch` obok importującego pliku albo ustaw `FINCH_PATH`. |
| `couldn't build the Finch runtime` | Brak działającego kompilatora C. Zainstaluj gcc lub clang albo ustaw `CC`. |
| Biblioteka nie znajduje się przy starcie programu | Zlinkowano ją z folderu, którego system nie przeszukuje. Uruchamiaj w tym samym `nix-shell` albo zainstaluj ją systemowo. |
| `clang` w `nix-shell` nie widzi `stdio.h` | Masz stary `shell.nix`: `llvmPackages.clang` musi być przed `llvmPackages.libclang`. |
| Program nigdy się nie kończy | Warunek pętli nigdy nie staje się fałszywy. Naciśnij Ctrl+C. |

---

## 21. Struktura projektu i testy

```
Finch/
├── src/            kompilator (C++)
├── runtime/        finch_rt.c: mała biblioteka uruchomieniowa (teksty, tablice, wejście, pliki)
├── boot/           kompilator Fincha napisany w Finchu (zobacz przewodnik dla inżynierów)
├── examples/       hello, tour, types, structs, todo, guess, c_import, window, raylib, llvm
├── tests/
│   ├── run/        programy + dokładny wynik (.out) i wejście (.in)
│   ├── fail/       programy, które muszą się nie udać, z oczekiwanym błędem w 1. linijce
│   ├── run.sh      uruchamia testy (MEMCHECK=1 sprawdza też pamięć valgrindem)
│   └── boot.sh     buduje kompilator samohostujący nim samym i porównuje
├── docs/           ta dokumentacja (en, pl)
├── shell.nix       środowisko deweloperskie Nix
└── CMakeLists.txt
```

```sh
tests/run.sh                 # → 59 passed, 0 failed
MEMCHECK=1 tests/run.sh      # to samo pod valgrindem: bez wycieków i złych dostępów do pamięci
tests/boot.sh                # sprawdzenie samohostowania (wymaga clanga)
```

---

## 22. Obecne ograniczenia

Finch 2.0 to kompletny mały język, ale nie skończony. Jeszcze nie ma:

- Metod w strukturach, typów generycznych, słowników (map), `match`. (Używaj tablic struktur i funkcji.)
- Błędów jako wartości: błąd w `int("x")` albo `read_file` zatrzymuje program. Sprawdzaj wcześniej (`file_exists`).
- Tekstu świadomego Unicode: `.len`, `s[i]` i `upper()` działają na bajtach / ASCII.
- Wątków.
- Platform innych niż Linux x86-64.

Plan rozwoju jest w [README](../../README.pl.md#plan-rozwoju).
