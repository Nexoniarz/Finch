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
11. [Listy: wiele wartości w jednym pudełku](#11-listy-wiele-wartości-w-jednym-pudełku)
12. [Zabawa tekstem](#12-zabawa-tekstem)
13. [Pytanie użytkownika](#13-pytanie-użytkownika)
14. [Własne rodzaje wartości: struktury](#14-własne-rodzaje-wartości-struktury)
15. [Zapisywanie do pliku](#15-zapisywanie-do-pliku)
16. [Kiedy coś pójdzie nie tak](#16-kiedy-coś-pójdzie-nie-tak)
17. [Małe projekty do zrobienia](#17-małe-projekty-do-zrobienia)
18. [Ściąga](#18-ściąga)
19. [Słowniczek](#19-słowniczek)

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

Powinieneś zobaczyć coś w stylu `finch 2.4.0`.

---

## 3. Twój pierwszy program

1. Otwórz dowolny edytor tekstu (na przykład *Edytor tekstu*, *Kate*, *gedit* albo *VS Code*).
2. Wpisz dokładnie to:

```c
fn main() {
    print("Cześć!")
}
```

3. Zapisz plik w folderze `Finch` pod nazwą **`czesc.fch`**. Końcówka `.fch` mówi, że to program w Finchu.
4. W Terminalu wpisz:

```sh
./build/finch run czesc.fch
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

## 11. Listy: wiele wartości w jednym pudełku

Czasem jedno pudełko nie wystarczy. Na liście zakupów jest wiele rzeczy.
**Lista** (programiści mówią też **tablica**) trzyma wiele wartości po kolei:

```c
fn main() {
    owoce := ["jabłko", "banan", "wiśnia"]
    print(owoce)
    print("Mam", owoce.len, "owoce")
    print("Pierwszy to", owoce[0])
}
```

```
["jabłko", "banan", "wiśnia"]
Mam 3 owoce
Pierwszy to jabłko
```

- Nawiasy kwadratowe `[ ]` tworzą listę. Wartości oddzielamy przecinkami.
- `.len` mówi, ile rzeczy jest na liście.
- `owoce[0]` to **pierwsza** rzecz. Liczymy od **0**, więc `owoce[1]` to druga, a `owoce[2]` trzecia.

Jeśli zapytasz o miejsce, którego nie ma, np. `owoce[10]`, program się zatrzyma i powie:
`index 10 is out of range (the length is 3)`, czyli „indeks 10 jest poza zakresem (długość to 3)”.

### Dodawanie i zabieranie

```c
fn main() {
    []int wyniki          // pusta lista liczb całkowitych
    wyniki.push(10)       // dołóż 10 na koniec
    wyniki.push(25)
    wyniki.push(7)
    print(wyniki)         // [10, 25, 7]

    ostatni := wyniki.pop()  // zabierz ostatni
    print(ostatni, wyniki)   // 7 [10, 25]

    wyniki.sort()            // posortuj
    print(wyniki)
}
```

### Przechodzenie po liście

`for ... in` odwiedza każdą wartość po kolei:

```c
fn main() {
    suma := 0
    for n in [3, 5, 2] {
        suma += n
    }
    print("Suma:", suma)    // Suma: 10
}
```

Jeśli chcesz też wiedzieć, *w którym miejscu* listy jesteś, daj `for` dwie nazwy: pierwsza liczy
0, 1, 2…, druga to wartość:

```c
fn main() {
    for i, owoc in ["jabłko", "banan", "wiśnia"] {
        print(i + 1, owoc)
    }
}
```

```
1 jabłko
2 banan
3 wiśnia
```

Listy umieją więcej: `contains` (czy zawiera), `find` (znajdź), `insert` (wstaw), `remove` (usuń),
`sort` (sortuj), `reverse` (odwróć), `join` (połącz). Wszystkie opisuje [przewodnik dla techników](dla-technikow.md#10-tablice-i-mapy).

### Szukanie po nazwie: mapy

Lista znajduje rzeczy po miejscu: `owoce[0]`. Czasem chcesz znaleźć coś po **nazwie**, tak jak
słowo w słowniku albo numer w książce telefonicznej. Do tego służy **mapa** (`map`): każdy
**klucz** (nazwa) ma swoją **wartość**:

```c
fn main() {
    telefon := ["Ola": "555-1234", "Jan": "555-9876"]
    print(telefon["Ola"])          // 555-1234

    telefon["Ewa"] = "555-0000"    // nowa osoba
    print(telefon.len)             // 3

    if telefon.has("Tomek") {
        print("Tomek jest")
    } else {
        print("nie ma Tomka")
    }

    for imie, numer in telefon {   // każde imię z jego numerem
        print(imie, numer)
    }
}
```

- `["klucz": wartość, ...]` tworzy mapę. Klucze to zwykle tekst, ale mogą być też liczby.
- `telefon["Ola"]` daje wartość dla klucza. Pytanie o klucz, którego nie ma, zatrzymuje program,
  więc najpierw sprawdź `telefon.has("Tomek")` (czy ma) albo użyj `telefon.get("Tomek", "nie wiem")`.
- `for imie, numer in telefon` odwiedza każdy klucz z wartością, w kolejności dodawania.

Mapy świetnie nadają się do **liczenia**. Nowy klucz zaczyna od 0, więc `+= 1` po prostu działa:

```c
fn main() {
    map[str]int glosy             // pusta mapa: klucze to tekst, wartości to liczby
    for g in ["kot", "pies", "kot", "kot", "rybka"] {
        glosy[g] += 1
    }
    print(glosy)                  // {"kot": 3, "pies": 1, "rybka": 1}
}
```

---

## 12. Zabawa tekstem

Teksty sklejasz znakiem `+`:

```c
fn main() {
    imie := "Ola"
    powitanie := "Cześć, " + imie + "!"
    print(powitanie)
}
```

Żeby dokleić liczbę do tekstu, najpierw zamień ją na tekst przez `str(...)`:

```c
wiek := 10
print("Wiek: " + str(wiek))
```

Teksty mają też przydatne narzędzia:

```c
fn main() {
    slowo := "Finch"
    print(slowo.len)            // 5 liter
    print(slowo[0])             // F  (pierwsza litera)
    print(slowo.upper())        // FINCH (wielkimi literami)
    print(slowo.lower())        // finch (małymi literami)
    print(slowo.contains("in")) // true (czy zawiera "in")
    for litera in slowo {
        print(litera)           // F, i, n, c, h, każda w osobnej linijce
    }
}
```

⚠️ `.len` liczy bajty, a polskie litery (ą, ę, ż…) zajmują po 2 bajty. `"żółw".len` to 7, a nie 4.

---

## 13. Pytanie użytkownika

`input` pokazuje pytanie i czeka, aż ktoś wpisze odpowiedź i naciśnie Enter:

```c
fn main() {
    imie := input("Jak masz na imię? ")
    print("Miło cię poznać,", imie)
}
```

Odpowiedź jest zawsze **tekstem**. Jeśli potrzebujesz liczby, zamień ją przez `int(...)`:

```c
fn main() {
    odpowiedz := input("Ile masz lat? ")
    wiek := int(odpowiedz)
    print("Za rok będziesz mieć", wiek + 1)
}
```

Jeśli ktoś wpisze coś, co nie jest liczbą, program zatrzyma się z komunikatem
w stylu `can't turn "abc" into int` („nie da się zamienić "abc" na int”).

---

## 14. Własne rodzaje wartości: struktury

Wyobraź sobie kartę w bibliotece: każda karta ma tytuł, autora i rok.
**Struktura** (`struct`) pozwala zrobić własny rodzaj wartości z nazwanymi częściami:

```c
struct Ksiazka {
    str tytul
    str autor
    int rok
}

fn main() {
    k := Ksiazka(tytul: "Hobbit", autor: "Tolkien", rok: 1937)
    print(k.tytul, "napisał", k.autor)
    k.rok = 1938
    print(k)
}
```

```
Hobbit napisał Tolkien
Ksiazka(tytul: "Hobbit", autor: "Tolkien", rok: 1938)
```

- `struct Ksiazka { ... }` opisuje, co ma każda książka. Pisz to **poza** `main`.
- `Ksiazka(tytul: ..., autor: ..., rok: ...)` tworzy jedną książkę.
- `k.tytul` odczytuje jedną część, a `k.rok = 1938` ją zmienia.

Częściom możesz nadać wartość początkową. Wtedy nie trzeba ich podawać:

```c
struct Gracz {
    str imie
    int zycia = 3
}

fn main() {
    g := Gracz(imie: "Ola")
    print(g.zycia)     // 3
}
```

Możesz też trzymać wiele struktur na liście: `[]Ksiazka polka`, a potem `polka.push(k)`.

### Polecenia twojej struktury: metody

Struktura może mieć własne polecenia (**metody**). Napisz nazwę struktury, kropkę i nazwę polecenia.
W środku `self` (ang. „sam”) to struktura, na której użyto polecenia:

```c
struct Gracz {
    str imie
    int zycia = 3
}

fn Gracz.trafiony() {
    self.zycia -= 1
    print(self.imie, "ma jeszcze", self.zycia, "życia")
}

fn Gracz.zyje() -> bool {
    return self.zycia > 0
}

fn main() {
    g := Gracz(imie: "Ola")
    g.trafiony()             // Ola ma jeszcze 2 życia
    g.trafiony()             // Ola ma jeszcze 1 życia
    print(g.zyje())          // true
}
```

`g.trafiony()` zmienia samo `g`, więc po dwóch trafieniach `g.zycia` naprawdę wynosi 1.

---

## 15. Zapisywanie do pliku

Program zapomina wszystko, kiedy się kończy. Żeby coś zapamiętać, zapisz to do pliku:

```c
fn main() {
    write_file("notatka.txt", "Kupić mleko")
    print(read_file("notatka.txt"))
}
```

- `write_file(nazwa, tekst)` zapisuje tekst w pliku (i zastępuje to, co tam było).
- `read_file(nazwa)` oddaje tekst z pliku.
- `file_exists(nazwa)` mówi, czy plik istnieje (`true` albo `false`).

Plik pojawi się w folderze, w którym uruchomiłeś program.

---

## 16. Kiedy coś pójdzie nie tak

Każdy się myli przy programowaniu, nawet eksperci, codziennie.
Finch stara się tłumaczyć, co poszło nie tak, prostymi słowami (na razie po angielsku). Przykład:

```c
fn main() {
    wiek := 10
    print(wiekk)
}
```

```
czesc.fch:3:11: error: there is no variable named 'wiekk'
    3 |     print(wiekk)
      |           ^
```

Jak to czytać:

- `czesc.fch` to plik,
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
| `index 5 is out of range (the length is 3)` | Zapytałeś listę o miejsce, którego nie ma | Pamiętaj, że liczymy od 0; sprawdź `.len` |
| `can't turn "abc" into int` | `int(...)` dostał tekst, który nie jest liczbą | Sprawdź, co wpisał użytkownik |
| `can't use '+' on str and int` | Tekst plus liczba | Zamień liczbę na tekst przez `str(...)` |
| `'f' can fail, so say what happens then` | Użyłeś polecenia, które może się nie udać, i nie napisałeś, co wtedy | Dopisz `or ...` (niżej) |

### Rzeczy, które mogą się nie udać: `or` i `try`

Niektóre rzeczy mogą pójść źle *w trakcie działania programu*: ktoś wpisze `abc` zamiast liczby
albo pliku nie ma. Zamiast zatrzymywać program, możesz powiedzieć, co wtedy zrobić, słowem **`or`** („albo”):

```c
fn main() {
    wiek := int(input("Ile masz lat? ")) or 0      // to nie liczba? weź 0
    print("Za rok będziesz mieć", wiek + 1)

    notatka := read_file("notatka.txt") or "(jeszcze brak notatki)"
    print(notatka)
}
```

Po `or` może też być blok. W środku `err` to komunikat, co poszło nie tak:

```c
fn main() {
    while true {
        n := int(input("Podaj liczbę: ")) or {
            print("To nie jest liczba, spróbuj jeszcze raz")
            continue
        }
        print("Wybrałeś", n)
        break
    }
}
```

Twoje własne funkcje też mogą się nie udać. Dopisz `!` po typie wyniku i użyj `return error("...")`:

```c
fn bezpieczne_dzielenie(int a, int b) -> int! {
    if b == 0 {
        return error("nie można dzielić przez zero")
    }
    return a / b
}

fn main() {
    print(bezpieczne_dzielenie(10, 2) or -1)     // 5
    print(bezpieczne_dzielenie(10, 0) or -1)     // -1
}
```

Finch nie pozwoli ci zapomnieć: wywołanie `bezpieczne_dzielenie(10, 0)` bez `or` to błąd, który
przypomina, żeby to obsłużyć. (Można też przekazać problem dalej przez `try`; wyjaśnia to
[przewodnik dla techników](dla-technikow.md#błędy-jako-wartości).)

---

## 17. Małe projekty do zrobienia

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

### Projekt 6: Zgadnij liczbę

Komputer ma tajną liczbę. Gracz zgaduje, aż trafi, a komputer za każdym razem mówi „więcej” albo „mniej”.

<details><summary>Rozwiązanie</summary>

```c
fn main() {
    tajna := 42
    proby := 0
    while true {
        strzal := int(input("Twój strzał: "))
        proby += 1
        if strzal < tajna {
            print("Więcej!")
        } else if strzal > tajna {
            print("Mniej!")
        } else {
            print("Tak! Potrzebowałeś", proby, "prób")
            return
        }
    }
}
```

`return` w `main` kończy program. (Wersja z losową liczbą jest w `examples/guess.fch`.)
</details>

### Projekt 7: Średnia ocen

Zapytaj o 3 oceny, zapisz je na liście, a potem pokaż listę i średnią.

<details><summary>Rozwiązanie</summary>

```c
fn main() {
    []int oceny
    for i in 0..3 {
        oceny.push(int(input("Ocena: ")))
    }
    suma := 0
    for o in oceny {
        suma += o
    }
    print("Oceny:", oceny)
    print("Średnia:", float(suma) / oceny.len)
}
```
</details>

---

## 18. Ściąga

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

struct Zwierzak {                 // własny rodzaj wartości
    str imie
    int wiek = 1
}

// listy:  liczby := [1, 2, 3]   liczby.push(4)   liczby[0]   liczby.len   for n in liczby { }   for i, n in liczby { }
// mapy:   wiek := ["Ola": 9]   wiek["Jan"] = 10   wiek["Ola"]   wiek.has("Tomek")   for imie, lata in wiek { }
// tekst:  "a" + "b"   str(42)   int("42")   s.len   s.upper()   s.contains("x")
// pytanie: imie := input("Imię? ")
// pliki:  write_file("p.txt", tekst)   read_file("p.txt")
// metody: fn Zwierzak.urosnij() { self.wiek += 1 }    z.urosnij()
// porażki: n := int(tekst) or 0      x := read_file("p.txt") or { print(err) ... }
```

---

## 19. Słowniczek

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
| **lista / tablica** | Wiele wartości po kolei, np. `[1, 2, 3]` |
| **indeks** | Pozycja wartości na liście, liczona od 0 |
| **struktura** (struct) | Własny rodzaj wartości złożony z nazwanych części |
| **metoda** | Polecenie należące do struktury, np. `g.trafiony()` |
| **mapa** (map) | Wartości znajdowane po kluczu (nazwie), jak w słowniku |
| **wejście** (input) | To, co użytkownik wpisuje do programu |
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
| `struct` | struktura |
| `push` / `pop` | dołóż / zabierz |
| `input` | wejście (pytanie) |
| `len` (od *length*) | długość |
| `error` | błąd |
| `or` | albo (co zrobić, gdy się nie uda) |
| `try` | spróbuj (a jak się nie uda, przekaż błąd dalej) |
| `self` | sam (struktura, na której użyto metody) |
| `map` | mapa (słownik) |
| `has` / `get` | ma / weź |

---

**Co dalej?** Kiedy to wszystko wyda ci się łatwe, przejdź do
[przewodnika dla techników](dla-technikow.md). Opisuje każdą część Fincha.
