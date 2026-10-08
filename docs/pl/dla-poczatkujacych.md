# Finch dla początkujących

**Ten przewodnik jest dla ciebie, jeśli jeszcze nigdy nie pisałeś programu.**
Nie potrzebujesz żadnego doświadczenia ani matematyki ponad to, czego uczy się w podstawówce.
Czytaj powoli, po jednym rozdziale, i przepisuj każdy przykład samodzielnie. Tak zostaje w głowie najlepiej.

> 🇬🇧 English version: [for-beginners.md](../en/for-beginners.md)

---

## Spis treści

1. [Co to jest program?](#1-co-to-jest-program)
2. [Przygotowanie Fincha](#2-przygotowanie-fincha)
3. [Twój pierwszy program](#3-twój-pierwszy-program)
4. [Wypisywanie na ekran](#4-wypisywanie-na-ekran)
5. [Pudełka z nazwami: zmienne](#5-pudełka-z-nazwami-zmienne)
6. [Rodzaje wartości](#6-rodzaje-wartości)
7. [Liczenie](#7-liczenie)
8. [Podejmowanie decyzji: if](#8-podejmowanie-decyzji-if)
9. [Powtarzanie: pętle](#9-powtarzanie-pętle)
10. [Własne polecenia: funkcje](#10-własne-polecenia-funkcje)
11. [Kiedy coś pójdzie nie tak](#11-kiedy-coś-pójdzie-nie-tak)
12. [Małe projekty do zrobienia](#12-małe-projekty-do-zrobienia)
13. [Ściąga](#13-ściąga)
14. [Słowniczek](#14-słowniczek)

---

## 1. Co to jest program?

Komputer jest bardzo szybki, ale nie myśli. Robi dokładnie to, co mu się każe,
krok po kroku.

**Program** to spisana lista takich kroków. Trochę jak przepis kuchenny:

> 1. Weź dwa jajka.
> 2. Wbij je do miski.
> 3. Wymieszaj.

**Język programowania** to sposób zapisania takiego przepisu, który komputer rozumie.
**Finch** (po polsku: zięba) jest jednym z takich języków. Zrobiono go tak, żeby był
mały i prosty, więc nauczysz się go szybko.

Komputer czyta program od góry do dołu, linijka po linijce.

---

## 2. Przygotowanie Fincha

Finch działa na komputerach z **Linuksem**. Jeśli nauczyciel, rodzic albo znajomy już go
dla ciebie zainstalował, przejdź od razu do następnego rozdziału.

W przeciwnym razie otwórz program **Terminal** (okienko, w którym wpisuje się polecenia)
i wpisz te linijki po kolei, naciskając **Enter** po każdej:

```sh
git clone https://github.com/Nexoniarz/Finch.git
cd Finch
nix-shell
cmake -S . -B build -G Ninja
ninja -C build
```

Jeśli ostatnia linijka skończy się bez słowa `error`, Finch jest gotowy.
(Jeśli komputer nie zna polecenia `nix-shell`, poproś kogoś o skorzystanie z
[przewodnika dla techników](dla-technikow.md#2-instalacja), gdzie opisano inny sposób.)

Żeby sprawdzić, czy działa, wpisz:

```sh
./build/finch version
```

Powinieneś zobaczyć coś w stylu `finch 1.0.0`.

---

## 3. Twój pierwszy program

1. Otwórz dowolny edytor tekstu (na przykład *Edytor tekstu*, *Kate*, *gedit* albo *VS Code*).
2. Wpisz dokładnie to:

```c
fn main() {
    print("Cześć!")
}
```

3. Zapisz plik w folderze `Finch` pod nazwą **`czesc.fn`**. Końcówka `.fn` mówi, że to program w Finchu.
4. W Terminalu wpisz:

```sh
./build/finch run czesc.fn
```

Zobaczysz:

```
Cześć!
```

🎉 **Właśnie napisałeś i uruchomiłeś program.**

### Co właściwie napisaliśmy?

```text
fn main() {            ← „tu zaczyna się główna część mojego programu”
    print("Cześć!")    ← „pokaż na ekranie tekst Cześć!”
}                      ← „tu główna część się kończy”
```

- `fn main()` to miejsce, od którego **zaczyna się** każdy program w Finchu. Każdy program ma dokładnie jedno `main`.
- Nawiasy klamrowe `{` i `}` obejmują wszystko, co należy do `main`, jak dwie okładki książki.
- `print(...)` pokazuje coś na ekranie.
- Tekst zawsze piszemy w cudzysłowie: `"Cześć!"`.

Spacje przed `print` nie są obowiązkowe, ale dzięki nim od razu widać, co jest w środku
klamer. Zawsze je dodawaj, to dobry nawyk.

---

## 4. Wypisywanie na ekran

`print` umie pokazać tekst i liczby. Między kolejne rzeczy wstaw przecinek, a Finch sam doda spację:

```c
fn main() {
    print("Mam", 10, "lat")
    print("2 + 2 =", 2 + 2)
    print()
    print("Linijka wyżej jest pusta")
}
```

Pokaże:

```
Mam 10 lat
2 + 2 = 4

Linijka wyżej jest pusta
```

Każdy `print` zaczyna nową linijkę. `print()` bez niczego w środku robi pustą linijkę.

### Notatki dla siebie: komentarze

Wszystko po `//` komputer pomija. Możesz tam zostawiać sobie notatki:

```c
// Ten program się wita
fn main() {
    print("Cześć")   // to pokazuje Cześć
}
```

---

## 5. Pudełka z nazwami: zmienne

Wyobraź sobie pudełko z naklejoną etykietą. Możesz coś do niego włożyć, a potem sprawdzić,
co jest w środku, czytając etykietę. W programowaniu takie pudełko nazywa się **zmienną**.

```c
fn main() {
    wiek := 10
    print(wiek)
}
```

`wiek := 10` znaczy: *„zrób nowe pudełko, nazwij je `wiek` i włóż do niego 10”*.

Później możesz zmienić zawartość pudełka, używając samego `=`:

```c
fn main() {
    punkty := 0
    print("Punkty:", punkty)

    punkty = 5
    print("Punkty:", punkty)

    punkty = punkty + 1
    print("Punkty:", punkty)
}
```

```
Punkty: 0
Punkty: 5
Punkty: 6
```

`punkty = punkty + 1` czytamy tak: *„weź to, co jest w `punkty`, dodaj 1 i włóż wynik z powrotem do `punkty`”*.
To samo można zapisać krócej: `punkty += 1`.

**Zapamiętaj:**

- `:=` robi **nowe** pudełko. Używasz tego tylko raz dla danej nazwy.
- `=` wkłada nową wartość do pudełka, **które już istnieje**.

### Nazywanie pudełek

Nazwy mogą zawierać angielskie litery `a`–`z` i `A`–`Z`, cyfry i znak `_`, ale nie mogą zaczynać się od cyfry:
`wiek`, `suma_punktow`, `gracz2` są w porządku. `2gracz` już nie.

⚠️ **Bez polskich liter w nazwach!** `ą`, `ę`, `ł`, `ż` i inne mogą być tylko w tekście w cudzysłowie.
Pisz `zolw`, a nie `żółw`. Natomiast `print("Żółw")` działa bez problemu.

Wybieraj nazwy, które mówią, co jest w środku: `cena` jest lepsza niż `c`.

---

## 6. Rodzaje wartości

Finch pilnuje, *jakiego rodzaju* rzecz jest w każdym pudełku. Ten rodzaj nazywa się **typem**.
Najczęściej będziesz używać tych:

| Typ     | Co przechowuje            | Przykłady                |
|---------|---------------------------|--------------------------|
| `int`   | liczby całkowite          | `0`, `42`, `-7`          |
| `float` | liczby z przecinkiem      | `3.14`, `0.5`, `-2.0`    |
| `str`   | tekst                     | `"Cześć"`, `"Ola"`       |
| `bool`  | tak albo nie              | `true` (prawda), `false` (fałsz) |
| `char`  | jeden znak                | `'A'`, `'z'`, `'?'`      |

⚠️ W liczbach zamiast przecinka piszemy **kropkę**: `3.14`, a nie `3,14`. Tak jest w prawie wszystkich językach programowania.

Kiedy używasz `:=`, Finch sam rozpoznaje typ:

```c
imie := "Ola"       // str
wiek := 10          // int
wzrost := 1.42      // float
lubi_koty := true   // bool
```

Typ możesz też napisać sam, przed nazwą:

```c
int wiek = 10
str imie = "Ola"
```

Oba sposoby robią to samo. Wybierz ten, który ci bardziej pasuje.

**Pudełko nie zmienia typu.** Jeśli w `wiek` są liczby całkowite, nie włożysz tam potem tekstu.
Finch zatrzyma się i powie o tym, co pomaga wcześnie wyłapywać pomyłki.

---

## 7. Liczenie

| Znak | Co robi                     | Przykład | Wynik |
|------|-----------------------------|----------|-------|
| `+`  | dodawanie                   | `7 + 2`  | `9`   |
| `-`  | odejmowanie                 | `7 - 2`  | `5`   |
| `*`  | mnożenie                    | `7 * 2`  | `14`  |
| `/`  | dzielenie                   | `7 / 2`  | `3`   |
| `%`  | reszta z dzielenia          | `7 % 2`  | `1`   |

**Uwaga na dzielenie liczb całkowitych:** `7 / 2` daje `3`, a nie `3.5`. Obie liczby są
całkowite, więc wynik też jest całkowity (część po kropce jest odrzucana).
Jeśli chcesz dostać `3.5`, użyj liczby z kropką: `7.0 / 2`.

Mnożenie i dzielenie wykonują się przed dodawaniem i odejmowaniem, tak jak w szkole.
Nawiasy zmieniają kolejność:

```c
print(2 + 3 * 4)     // 14
print((2 + 3) * 4)   // 20
```

`%` przydaje się do sprawdzania, czy liczba jest parzysta. Liczba parzysta dzielona przez 2
nie daje reszty, więc `n % 2` wynosi `0`.

**Nie wolno dzielić przez zero.** Jeśli program spróbuje, zatrzyma się i powie, w której linijce to się stało.

---

## 8. Podejmowanie decyzji: if

Program może wybierać, co zrobić. `if` (czyt. „if”, po polsku „jeżeli”) znaczy *„zrób to tylko wtedy, gdy coś jest prawdą”*.

```c
fn main() {
    temperatura := 25

    if temperatura > 20 {
        print("Jest ciepło, załóż koszulkę")
    }
}
```

Dodaj `else` („w przeciwnym razie”):

```c
if temperatura > 20 {
    print("Jest ciepło")
} else {
    print("Weź kurtkę")
}
```

A `else if` dodaje kolejne możliwości. Finch sprawdza je od góry i wykonuje tylko
**pierwszą** prawdziwą:

```c
if temperatura > 25 {
    print("Gorąco!")
} else if temperatura > 15 {
    print("Przyjemnie")
} else {
    print("Zimno")
}
```

### Porównywanie

| Znak | Znaczy                       |
|------|------------------------------|
| `==` | jest równe                   |
| `!=` | nie jest równe               |
| `<`  | jest mniejsze niż            |
| `>`  | jest większe niż             |
| `<=` | jest mniejsze lub równe      |
| `>=` | jest większe lub równe       |

⚠️ **Jedno `=` wkłada wartość do pudełka. Dwa `==` pytają „czy to jest równe?”.**

Tekst też można porównywać: `if imie == "Ola" { ... }`.

### Łączenie pytań

- `&&` znaczy **i**: oba warunki muszą być prawdziwe. `if wiek > 6 && wiek < 12 { ... }`
- `||` znaczy **lub**: wystarczy, że jeden jest prawdziwy. `if dzien == "sob" || dzien == "nd" { ... }`
- `!` znaczy **nie**: odwraca prawdę i fałsz. `if !pada { ... }`

---

## 9. Powtarzanie: pętle

Komputer nigdy się nie nudzi. **Pętla** powtarza te same kroki wiele razy.

### Liczenie z `for`

```c
fn main() {
    for i in 1..6 {
        print("Liczba", i)
    }
}
```

```
Liczba 1
Liczba 2
Liczba 3
Liczba 4
Liczba 5
```

`for i in 1..6` znaczy: *„licz za pomocą `i` od 1 do 6, ale **bez** 6”*.
Czyli `0..10` daje dziesięć liczb: 0, 1, 2 … 9.

### Powtarzanie, dopóki coś jest prawdą: `while`

```c
fn main() {
    licznik := 3
    while licznik > 0 {
        print(licznik)
        licznik -= 1
    }
    print("Start!")
}
```

```
3
2
1
Start!
```

⚠️ Upewnij się, że pętla może się skończyć. Gdyby `licznik` nigdy nie malał, pętla kręciłaby się
w nieskończoność. Jeśli tak się stanie, naciśnij w Terminalu **Ctrl + C**, żeby ją zatrzymać.

### Wcześniejsze wyjście i pomijanie

- `break` od razu wychodzi z pętli.
- `continue` pomija resztę bieżącego okrążenia i przechodzi do następnego.

```c
for i in 1..100 {
    if i == 5 {
        break         // stop, gdy dojdziemy do 5
    }
    print(i)          // pokaże 1 2 3 4
}
```

---

## 10. Własne polecenia: funkcje

Kiedy robisz to samo w wielu miejscach, nadaj temu nazwę. Taki nazwany kawałek kodu to **funkcja**.
Jedną już znasz: `main`.

```c
fn przywitaj(str imie) {
    print("Cześć,", imie)
}

fn main() {
    przywitaj("Ola")
    przywitaj("Tomek")
}
```

```
Cześć, Ola
Cześć, Tomek
```

`str imie` w nawiasie znaczy: *„ta funkcja potrzebuje jednego tekstu, a w środku nazywamy go `imie`”*.
To, co wpiszesz w nawias przy wywołaniu, zostanie przekazane do środka.

### Funkcje, które oddają odpowiedź

Napisz `->` i typ odpowiedzi, a odpowiedź oddaj słowem `return`:

```c
fn dodaj(int a, int b) -> int {
    return a + b
}

fn main() {
    wynik := dodaj(2, 3)
    print(wynik)          // 5
}
```

Nie ma znaczenia, gdzie w pliku umieścisz swoje funkcje, nad czy pod `main`.

---

## 11. Kiedy coś pójdzie nie tak

Każdy się myli przy programowaniu, nawet eksperci, codziennie.
Finch stara się tłumaczyć, co poszło nie tak, prostymi słowami (na razie po angielsku). Przykład:

```c
fn main() {
    wiek := 10
    print(wiekk)
}
```

```
czesc.fn:3:11: error: there is no variable named 'wiekk'
    3 |     print(wiekk)
      |           ^
```

Jak to czytać:

- `czesc.fn` to plik,
- `3` to numer **linijki**,
- `11` to który znak w linijce (**kolumna**),
- potem wyjaśnienie, a pod nim ta linijka ze strzałką `^` wskazującą problem.

Tutaj po prostu źle napisaliśmy `wiek`. Komunikat znaczy: „nie ma zmiennej o nazwie 'wiekk'”.

### Częste błędy

| Finch mówi | Co to znaczy | Jak naprawić |
|---|---|---|
| `there is no variable named 'x'` | Nie ma zmiennej `x`: literówka albo zapomniałeś ją zrobić | Sprawdź pisownię. Utwórz ją przez `x := ...` |
| `a variable named 'x' already exists here` | Użyłeś `:=` dwa razy dla tej samej nazwy | Za drugim razem użyj `=` |
| `'x' must be int, but this is str` | Zły rodzaj wartości dla tego pudełka | Włóż wartość właściwego rodzaju |
| `put each statement on its own line` | Dwa polecenia w jednej linijce | Wciśnij Enter między nimi |
| `this '{' is never closed` | Brakuje `}` | Dopisz `}` |
| `a condition must be bool` | `if` potrzebuje pytania tak/nie | Porównaj coś: `if x > 0` |
| `unexpected character 'ż'` | Polska litera w nazwie | Zmień na zwykłą literę: `zolw` |
| `runtime error: division by zero` | Program podzielił przez 0 w trakcie działania | Sprawdź liczbę przed dzieleniem |

---

## 12. Małe projekty do zrobienia

Spróbuj najpierw napisać każdy sam. Do rozwiązania zajrzyj dopiero, gdy utkniesz.

### Projekt 1: Tabliczka mnożenia

Pokaż tabliczkę mnożenia przez 7, od 7 × 1 do 7 × 10.

<details><summary>Rozwiązanie</summary>

```c
fn main() {
    for i in 1..11 {
        print("7 x", i, "=", 7 * i)
    }
}
```
</details>

### Projekt 2: Parzysta czy nieparzysta

Dla każdej liczby od 1 do 10 napisz, czy jest parzysta, czy nieparzysta.

<details><summary>Rozwiązanie</summary>

```c
fn main() {
    for n in 1..11 {
        if n % 2 == 0 {
            print(n, "jest parzysta")
        } else {
            print(n, "jest nieparzysta")
        }
    }
}
```
</details>

### Projekt 3: Stopnie Celsjusza na Fahrenheita

Pokaż tabelkę od 0 °C do 100 °C co 10 stopni. Wzór: `F = C * 9 / 5 + 32`.

<details><summary>Rozwiązanie</summary>

```c
fn na_fahrenheita(float c) -> float {
    return c * 9 / 5 + 32
}

fn main() {
    for c in 0..11 {
        celsjusz := float(c * 10)
        print(celsjusz, "C =", na_fahrenheita(celsjusz), "F")
    }
}
```

`float(...)` zamienia liczbę całkowitą na liczbę z przecinkiem.
</details>

### Projekt 4: Fizz Buzz

Policz od 1 do 20. Dla liczb podzielnych przez 3 napisz `Fizz`, przez 5 napisz `Buzz`,
przez obie naraz napisz `FizzBuzz`, a w pozostałych przypadkach samą liczbę.

<details><summary>Rozwiązanie</summary>

```c
fn main() {
    for n in 1..21 {
        if n % 15 == 0 {
            print("FizzBuzz")
        } else if n % 3 == 0 {
            print("Fizz")
        } else if n % 5 == 0 {
            print("Buzz")
        } else {
            print(n)
        }
    }
}
```
</details>

### Projekt 5: Największa z trzech

Napisz funkcję `najwieksza(int a, int b, int c) -> int`, która oddaje największą z trzech liczb.

<details><summary>Rozwiązanie</summary>

```c
fn najwieksza(int a, int b, int c) -> int {
    if a >= b && a >= c {
        return a
    }
    if b >= c {
        return b
    }
    return c
}

fn main() {
    print(najwieksza(4, 9, 2))   // 9
}
```
</details>

---

## 13. Ściąga

```c
// komentarz

fn main() {                       // tu zaczyna się program
    print("tekst", 42)            // pokaż coś

    x := 5                        // nowe pudełko (typ zgadnięty)
    int y = 10                    // nowe pudełko (typ podany)
    x = 7                         // zmień zawartość
    x += 1                        // dodaj 1 do x

    if x > 5 {                    // decyzja
        print("duże")
    } else if x > 0 {
        print("małe")
    } else {
        print("zero albo mniej")
    }

    for i in 0..10 {              // licz od 0 do 9
        print(i)
    }

    while x > 0 {                 // powtarzaj, dopóki prawda
        x -= 1
    }
}

fn dodaj(int a, int b) -> int {   // własne polecenie
    return a + b
}
```

---

## 14. Słowniczek

| Słowo | Znaczenie |
|---|---|
| **program** | Lista kroków dla komputera |
| **kod** | Tekst programu |
| **uruchomić** | Kazać komputerowi wykonać kroki |
| **kompilować** | Przetłumaczyć kod na coś, co komputer umie uruchomić. `finch run` robi to za ciebie |
| **zmienna** | Pudełko z nazwą, w którym jest wartość |
| **typ** | Rodzaj wartości: liczba, tekst, tak/nie… |
| **funkcja** | Nazwana grupa kroków, której można używać wiele razy |
| **pętla** | Kroki, które się powtarzają |
| **warunek** | Pytanie tak/nie, np. `x > 5` |
| **błąd** (error) | Komunikat, że coś jest nie tak, i gdzie |
| **bug** | Pomyłka w programie |

### Słówka po angielsku, które spotkasz w Finchu

| Słowo | Po polsku |
|---|---|
| `print` | wypisz |
| `if` / `else` | jeżeli / w przeciwnym razie |
| `for` … `in` | dla … w |
| `while` | dopóki |
| `break` / `continue` | przerwij / kontynuuj |
| `return` | zwróć (oddaj) |
| `true` / `false` | prawda / fałsz |
| `fn` (od *function*) | funkcja |
| `error` | błąd |

---

**Co dalej?** Kiedy to wszystko wyda ci się łatwe, przejdź do
[przewodnika dla techników](dla-technikow.md). Opisuje każdą część Fincha.
