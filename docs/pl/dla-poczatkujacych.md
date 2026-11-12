# Finch dla początkujących

**Programowanie to dla ciebie coś zupełnie nowego? Ten przewodnik jest dla ciebie.**
Nie musisz nic umieć: wystarczy matematyka ze szkoły podstawowej.
Czytaj spokojnie, rozdział po rozdziale, i każdy przykład przepisuj sam: tak najlepiej zostaje w głowie.

> 🇬🇧 English version: [for-beginners.md](../en/for-beginners.md)

---

## Spis treści

1. [Co to jest program?](#1-co-to-jest-program)
2. [Przygotowanie Fincha](#2-przygotowanie-fincha)
3. [Twój pierwszy program](#3-twój-pierwszy-program)
4. [Wypisywanie na ekranie](#4-wypisywanie-na-ekranie)
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
19. [Słowa, które będziesz słyszeć](#19-słowa-które-będziesz-słyszeć)

---

## 1. Co to jest program?

Komputer jest bardzo szybki, ale nie myśli. Robi dokładnie to, co mu każesz, krok po kroku.

**Program** to spisana lista takich kroków. Trochę jak przepis:

> 1. Weź dwa jajka.
> 2. Wbij je do miski.
> 3. Wymieszaj.

**Język programowania** to sposób zapisania takiego przepisu tak, żeby komputer go zrozumiał.
**Finch** to jeden z takich języków. Powstał po to, żeby był mały i prosty, więc szybko się go nauczysz.

Komputer czyta program od góry do dołu, linia po linii.

---

## 2. Przygotowanie Fincha

Finch działa na komputerach z **Linuksem**, **macOS** i **Windowsem**. Jeśli nauczyciel, rodzic albo znajomy
już go dla ciebie przygotował, przejdź do następnego rozdziału. (Na Macu i na Windowsie poproś kogoś o skorzystanie
z [przewodnika dla techników](dla-technikow.md#2-instalacja): są tam gotowe wersje Fincha na oba systemy.)

W przeciwnym razie otwórz program **Terminal** (okno, w którym wpisuje się polecenia) i wpisz po kolei te linie,
naciskając po każdej **Enter**:

```sh
git clone https://github.com/Nexoniarz/Finch.git
cd Finch
nix-shell
bootstrap/build.sh
```

Jeśli ostatnie polecenie skończy się bez słowa `error`, Finch jest gotowy.
(Jeśli na twoim komputerze nie ma `nix-shell`, poproś kogoś o skorzystanie z
[przewodnika dla techników](dla-technikow.md#2-instalacja). Jest tam opisany inny sposób.)

Żeby sprawdzić, czy wszystko działa, wpisz:

```sh
./build/finch version
```

Powinien pojawić się napis w rodzaju `Finch 2.6.0`.

---

## 3. Twój pierwszy program

1. Otwórz dowolny edytor tekstu (np. *Edytor tekstu*, *Kate*, *gedit* albo *VS Code*).
2. Przepisz dokładnie to:

```c
fn main() {
    print("Cześć!")
}
```

3. Zapisz plik w katalogu `Finch` pod nazwą **`hello.fch`**. Końcówka `.fch` mówi, że to program w Finchu.
4. W Terminalu wpisz:

```sh
./build/finch run hello.fch
```

Zobaczysz:

```
Cześć!
```

🎉 **Właśnie udało ci się napisać i uruchomić program.**

### Co właściwie napisaliśmy?

```text
fn main() {            ← „tu zaczyna się główna część mojego programu”
    print("Cześć!")    ← „pokaż na ekranie tekst Cześć!”
}                      ← „tu główna część się kończy”
```

- `fn main()` to miejsce, w którym **zaczyna się** każdy program w Finchu. Każdy program ma dokładnie jedno `main`.
- Nawiasy klamrowe `{` i `}` obejmują wszystko, co należy do `main`, jak dwie okładki książki.
- `print(...)` pokazuje coś na ekranie.
- Tekst zawsze stoi w cudzysłowie: `"Cześć!"`.

Spacje przed `print` nie są obowiązkowe, ale dzięki nim od razu widać, co jest w środku nawiasów.
Zawsze je dodawaj. To dobry nawyk.

---

## 4. Wypisywanie na ekranie

`print` potrafi pokazać tekst i liczby. Oddziel je przecinkami, a Finch sam wstawi między nimi spacje:

```c
fn main() {
    print("Mam", 10, "lat")
    print("2 + 2 =", 2 + 2)
    print()
    print("Linia wyżej jest pusta")
}
```

Pokaże:

```
Mam 10 lat
2 + 2 = 4

Linia wyżej jest pusta
```

Każde `print` zaczyna nową linię. `print()` bez niczego w środku robi pustą linię.

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

Wyobraź sobie pudełko z naklejoną etykietą. Możesz coś do niego włożyć, a później sprawdzić, co jest w środku,
czytając etykietę. W programowaniu takie pudełko nazywa się **zmienną**.

```c
fn main() {
    wiek := 10
    print(wiek)
}
```

`wiek := 10` znaczy: *„zrób nowe pudełko, nazwij je `wiek` i włóż do niego 10”.*

Później możesz zmienić zawartość pudełka, pisząc samo `=`:

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

`punkty = punkty + 1` czyta się tak: *„weź to, co jest w `punkty`, dodaj 1 i włóż wynik z powrotem do `punkty`”.*
To samo można napisać krócej: `punkty += 1`.

**Zapamiętaj:**

- `:=` robi **nowe** pudełko. Dla każdej nazwy używasz tego tylko raz.
- `=` wkłada nową wartość do pudełka, **które już istnieje**.

### Nazywanie pudełek

W nazwach można używać liter od `a` do `z` i od `A` do `Z`, cyfr i znaku `_`, ale nazwa nie może zaczynać się od cyfry:
`wiek`, `suma_punktow`, `gracz2` są w porządku, a `2gracz` nie.
Polskich liter (`ą`, `ę`, `ż`…) nie wolno używać w nazwach, tylko w tekstach w cudzysłowie: `print("Łódź")` jest w porządku.
Wybieraj nazwy, które mówią, co jest w środku: `cena` jest lepsza niż `c`.

---

## 6. Rodzaje wartości

Finch pilnuje, jaki *rodzaj* rzeczy leży w każdym pudełku. Taki rodzaj to **typ**.
Najczęściej będziesz używać tych:

| Typ     | Co przechowuje              | Przykłady                |
|---------|-----------------------------|--------------------------|
| `int`   | liczby całkowite            | `0`, `42`, `-7`          |
| `float` | liczby z przecinkiem        | `3.14`, `0.5`, `-2.0`    |
| `str`   | tekst                       | `"Cześć"`, `"Ola"`       |
| `bool`  | tak albo nie                | `true`, `false`          |
| `char`  | jedną literę albo znak      | `'A'`, `'z'`, `'?'`      |

Uwaga: w programach część ułamkową oddziela się **kropką**, nie przecinkiem: `3.14`, a nie `3,14`.

Przy `:=` Finch sam rozpoznaje typ:

```c
imie := "Ola"       // str
wiek := 10          // int
wzrost := 1.42      // float
lubi_koty := true   // bool
```

Typ można też napisać samemu, przed nazwą:

```c
int wiek = 10
str imie = "Ola"
```

Oba sposoby robią to samo. Wybierz ten, który ci bardziej pasuje.

**Pudełko trzyma się swojego typu.** Jeśli w `wiek` są liczby całkowite, nie włożysz tam potem tekstu.
Finch się zatrzyma i ci o tym powie, a to pomaga wcześnie wyłapywać pomyłki.

---

## 7. Liczenie

| Znak | Znaczenie                      | Przykład | Wynik  |
|------|--------------------------------|----------|--------|
| `+`  | dodawanie                      | `7 + 2`  | `9`    |
| `-`  | odejmowanie                    | `7 - 2`  | `5`    |
| `*`  | mnożenie                       | `7 * 2`  | `14`   |
| `/`  | dzielenie                      | `7 / 2`  | `3`    |
| `%`  | reszta z dzielenia             | `7 % 2`  | `1`    |

**Uważaj przy dzieleniu liczb całkowitych:** `7 / 2` daje `3`, a nie `3.5`, bo obie liczby są całkowite, więc wynik
też jest całkowity (część po kropce znika). Jeśli chcesz `3.5`, użyj liczby z kropką: `7.0 / 2`.

Mnożenie i dzielenie wykonują się przed dodawaniem i odejmowaniem, tak jak w szkole.
Nawiasy zmieniają kolejność:

```c
print(2 + 3 * 4)     // 14
print((2 + 3) * 4)   // 20
```

`%` przydaje się do sprawdzania, czy liczba jest parzysta: liczba parzysta dzielona przez 2 nie daje reszty,
więc `n % 2` wynosi `0`.

**Nie wolno dzielić przez zero.** Jeśli program spróbuje, zatrzyma się i powie, w której linii to się stało.

---

## 8. Podejmowanie decyzji: if

Program może wybierać, co zrobić. `if` znaczy *„zrób to tylko wtedy, gdy coś jest prawdą”*.

```c
fn main() {
    temperatura := 25

    if temperatura > 20 {
        print("Jest ciepło, włóż koszulkę")
    }
}
```

Dopisz `else`, żeby powiedzieć *„a jeśli nie, zrób to”*:

```c
if temperatura > 20 {
    print("Jest ciepło")
} else {
    print("Weź kurtkę")
}
```

A `else if` daje więcej możliwości. Finch sprawdza je od góry do dołu i wykonuje tylko **pierwszą**, która jest prawdą:

```c
if temperatura > 25 {
    print("Upał!")
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
| `<=` | jest mniejsze albo równe     |
| `>=` | jest większe albo równe      |

⚠️ **Jedno `=` wkłada wartość do pudełka. Dwa `==` pytają „czy to jest równe?”.**

Porównywać można też teksty: `if imie == "Ola" { ... }`.

### Łączenie pytań

- `&&` znaczy **i**: oba warunki muszą być prawdziwe. `if wiek > 6 && wiek < 12 { ... }`
- `||` znaczy **lub**: przynajmniej jeden musi być prawdziwy. `if dzien == "sob" || dzien == "niedz" { ... }`
- `!` znaczy **nie**: zamienia prawdę w fałsz i odwrotnie. `if !pada { ... }`

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

`for i in 1..6` znaczy: *„licz w `i` od 1 do 6, ale **bez** 6”*.
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

⚠️ Upewnij się, że pętla może się skończyć. Gdyby `licznik` nigdy nie malał, pętla kręciłaby się bez końca.
Jeśli tak się stanie, naciśnij w Terminalu **Ctrl + C**, żeby ją zatrzymać.

### Wcześniejsze wyjście i pomijanie

- `break` od razu wychodzi z pętli.
- `continue` pomija resztę bieżącego okrążenia i przechodzi do następnego.

```c
for i in 1..100 {
    if i == 5 {
        break         // zatrzymaj się, gdy dojdziemy do 5
    }
    print(i)          // wypisze 1 2 3 4
}
```

---

## 10. Własne polecenia: funkcje

Kiedy robisz to samo w wielu miejscach, nadaj temu nazwę. Taki nazwany blok to **funkcja**.
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

`str imie` w nawiasach znaczy: *„ta funkcja potrzebuje jednego tekstu i w środku będziemy go nazywać `imie`”.*
To, co wpiszesz w nawiasach przy jej użyciu, trafia do środka.

### Funkcje, które dają odpowiedź

Napisz `->` i typ odpowiedzi, a odpowiedź oddaj przez `return`:

```c
fn dodaj(int a, int b) -> int {
    return a + b
}

fn main() {
    wynik := dodaj(2, 3)
    print(wynik)          // 5
}
```

Nie ma znaczenia, gdzie w pliku umieścisz swoje funkcje, nad `main` czy pod nim.

---

## 11. Listy: wiele wartości w jednym pudełku

Czasem jedno pudełko nie wystarcza. Na liście zakupów jest wiele rzeczy.
**Lista** (programiści mówią też **tablica**) przechowuje wiele wartości po kolei:

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

- Nawiasy kwadratowe `[ ]` tworzą listę. Wartości oddziela się przecinkami.
- `.len` mówi, ile rzeczy jest na liście.
- `owoce[0]` to **pierwsza** rzecz. Liczy się od **0**, więc `owoce[1]` to druga, a `owoce[2]` trzecia.

Jeśli zapytasz o miejsce, którego nie ma, np. `owoce[10]`, program się zatrzyma i powie:
`index 10 is out of range (the length is 3)` (indeks 10 jest poza listą, która ma długość 3).

### Dodawanie i zabieranie

```c
fn main() {
    []int wyniki          // pusta lista liczb całkowitych
    wyniki.push(10)       // dodaj 10 na końcu
    wyniki.push(25)
    wyniki.push(7)
    print(wyniki)         // [10, 25, 7]

    ostatni := wyniki.pop()  // zdejmij ostatni
    print(ostatni, wyniki)   // 7 [10, 25]

    wyniki.sort()
    print(wyniki)
}
```

### Własna kolejność

`sort()` ustawia liczby od małych do dużych. Żeby samemu wybrać kolejność, daj `sort_by` małą funkcję, która
odpowiada na jedno pytanie: *czy `a` idzie przed `b`?*

```c
fn main() {
    wyniki := [10, 25, 7]
    wyniki.sort_by(fn(a, b) { return a > b })      // najpierw duże
    print(wyniki)                                  // [25, 10, 7]

    duze := wyniki.filter(fn(w) { return w >= 10 }) // zostaw tylko niektóre
    print(duze)                                    // [25, 10]
}
```

`fn(a, b) { ... }` to funkcja bez nazwy, napisana tam, gdzie jest potrzebna. Spotkasz je jeszcze później; na razie
myśl o niej jak o małej zasadzie, którą przekazujesz.

### Przechodzenie po liście

`for ... in` odwiedza po kolei każdą wartość:

```c
fn main() {
    suma := 0
    for n in [3, 5, 2] {
        suma += n
    }
    print("Suma:", suma)    // Suma: 10
}
```

Jeśli chcesz też wiedzieć, *w którym miejscu* listy jesteś, podaj w `for` dwie nazwy: pierwsza liczy 0, 1, 2…,
a druga to wartość:

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

Listy potrafią więcej: `contains`, `find`, `insert`, `remove`, `sort`, `reverse`, `join`.
Wszystkie są opisane w [przewodniku dla techników](dla-technikow.md#10-tablice-i-mapy).

### Szukanie po nazwie: mapy

Lista znajduje rzeczy po miejscu: `owoce[0]`. Czasem jednak chcesz szukać po **nazwie**, tak jak szuka się słowa
w słowniku albo numeru w książce telefonicznej. Do tego służy **mapa**: każdy **klucz** (nazwa) ma swoją **wartość**:

```c
fn main() {
    telefon := ["Ola": "555-1234", "Jan": "555-9876"]
    print(telefon["Ola"])          // 555-1234

    telefon["Ewa"] = "555-0000"    // dodaj nowe imię
    print(telefon.len)             // 3

    if telefon.has("Tomek") {
        print("Tomek jest")
    } else {
        print("nie ma Tomka")
    }

    for imie, numer in telefon {   // każde imię razem z numerem
        print(imie, numer)
    }
}
```

- `["klucz": wartość, ...]` tworzy mapę. Kluczami są często teksty, ale mogą być też liczby.
- `telefon["Ola"]` daje wartość dla klucza. Pytanie o klucz, którego nie ma, zatrzymuje program,
  więc najpierw sprawdź `telefon.has("Tomek")` albo użyj `telefon.get("Tomek", "nieznany")`.
- `for imie, numer in telefon` odwiedza każdy klucz z jego wartością, w kolejności, w jakiej je dodano.

Mapy świetnie nadają się do **liczenia**. Nowy klucz zaczyna od 0, więc `+= 1` po prostu działa:

```c
fn main() {
    map[str]int glosy            // pusta mapa: klucze to teksty, wartości to liczby całkowite
    for g in ["kot", "pies", "kot", "kot", "rybka"] {
        glosy[g] += 1
    }
    print(glosy)                 // {"kot": 3, "pies": 1, "rybka": 1}
}
```

---

## 12. Zabawa tekstem

Teksty można sklejać przez `+`:

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
    print(slowo.upper())        // FINCH
    print(slowo.lower())        // finch
    print(slowo.contains("in")) // true
    for litera in slowo {
        print(litera)           // F, i, n, c, h, każda w osobnej linii
    }
}
```

(`.len` liczy bajty, a polskie litery zajmują po dwa, więc `"żółw".len` to 7, a nie 4.)

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

Jeśli ktoś wpisze coś, co nie jest liczbą, program się zatrzyma z komunikatem
w rodzaju `can't turn "abc" into int` (nie da się zamienić "abc" na liczbę).

---

## 14. Własne rodzaje wartości: struktury

Pomyśl o karcie w bibliotece: każda ma tytuł, autora i rok wydania.
**Struktura** pozwala stworzyć własny rodzaj wartości z nazwanymi częściami:

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

- `struct Ksiazka { ... }` opisuje, co ma każda książka. Umieść to **poza** `main`.
- `Ksiazka(tytul: ..., autor: ..., rok: ...)` tworzy jedną książkę.
- `k.tytul` odczytuje jedną część. `k.rok = 1938` ją zmienia.

Częściom można nadać wartość początkową. Wtedy nie trzeba ich wypełniać:

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

Wiele takich wartości można trzymać na liście: `[]Ksiazka polka`, a potem `polka.push(k)`.

### Polecenia dla struktury: metody

Struktura może mieć własne polecenia. Napisz nazwę struktury, kropkę i nazwę polecenia.
W środku `self` to struktura, na której polecenie użyto:

```c
struct Gracz {
    str imie
    int zycia = 3
}

fn Gracz.trafiony() {
    self.zycia -= 1
    print(self.imie, "- pozostałe życia:", self.zycia)
}

fn Gracz.zyje() -> bool {
    return self.zycia > 0
}

fn main() {
    g := Gracz(imie: "Ola")
    g.trafiony()             // Ola - pozostałe życia: 2
    g.trafiony()             // Ola - pozostałe życia: 1
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

Plik pojawi się w katalogu, w którym uruchomiono program.

---

## 16. Kiedy coś pójdzie nie tak

Każdy, kto programuje, robi błędy, nawet eksperci, codziennie.
Finch bardzo się stara wyjaśnić prostymi słowami, co poszło nie tak. Przykład:

```c
fn main() {
    wiek := 10
    print(wiekk)
}
```

```
hello.fch:3:11: error: there is no variable named 'wiekk'
   |
 3 |     print(wiekk)
   |           ^^^^^
   = help: did you mean 'wiek'?
```

Jak to czytać:

- `hello.fch` to plik,
- `3` to numer **linii**,
- `11` mówi, jak daleko w linii (to **kolumna**),
- dalej jest wyjaśnienie, pod nim ta linia z `^^^^^` pod problemem i linia `help:` z podpowiedzią, co zrobić.

W terminalu komunikat jest kolorowy, więc ważne części się wyróżniają.

Komunikaty są po angielsku; tutaj znaczy to „nie ma zmiennej o nazwie 'wiekk'”. Po prostu literówka w `wiek`.

### Częste pomyłki

| Finch mówi | Co to znaczy | Jak naprawić |
|---|---|---|
| `there is no variable named 'x'` | Literówka w nazwie albo takie pudełko nigdy nie powstało | Sprawdź pisownię. Utwórz je przez `x := ...` |
| `a variable named 'x' already exists here` | Użyto `:=` dwa razy dla tej samej nazwy | Za drugim razem napisz `=` |
| `'x' must be int, but this is str` | Zły rodzaj wartości dla tego pudełka | Włóż wartość właściwego rodzaju |
| `put each statement on its own line` | Dwa polecenia w jednej linii | Rozdziel je Enterem |
| `this '{' is never closed` | Brakuje `}` | Dopisz `}` |
| `a condition must be bool` | `if` potrzebuje pytania typu tak/nie | Porównaj coś: `if x > 0` |
| `runtime error: division by zero` | W trakcie działania programu nastąpiło dzielenie przez 0 | Sprawdź liczbę przed dzieleniem |
| `index 5 is out of range (the length is 3)` | Pytanie listy o miejsce, którego nie ma | Pamiętaj, że liczy się od 0; sprawdź `.len` |
| `can't turn "abc" into int` | `int(...)` dostało tekst, który nie jest liczbą | Sprawdź, co wpisał użytkownik |
| `can't use '+' on str and int` | Tekst plus liczba | Użyj `str(...)` na liczbie |
| `'f' can fail, so say what happens then` | Użyto polecenia, które może się nie udać, bez powiedzenia, co wtedy zrobić | Dopisz `or ...` (zob. niżej) |

### Rzeczy, które mogą się nie udać: `or` i `try`

Niektóre rzeczy mogą się nie udać *w trakcie działania programu*: ktoś wpisze `abc` tam, gdzie miała być liczba,
albo nie ma pliku. Zamiast zatrzymywać program, możesz przez **`or`** powiedzieć, co wtedy zrobić:

```c
fn main() {
    wiek := int(input("Ile masz lat? ")) or 0     // to nie liczba? weź 0
    print("Za rok będziesz mieć", wiek + 1)

    notatka := read_file("notatka.txt") or "(jeszcze nie ma notatki)"
    print(notatka)
}
```

Po `or` może też stać blok. W jego środku `err` to komunikat, który mówi, co poszło nie tak:

```c
fn main() {
    while true {
        n := int(input("Wybierz liczbę: ")) or {
            print("To nie jest liczba, spróbuj jeszcze raz")
            continue
        }
        print("Wybrana liczba:", n)
        break
    }
}
```

Twoje własne funkcje też mogą się nie udać. Postaw `!` po typie wyniku i użyj `return error("...")`:

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

Finch nie pozwoli o tym zapomnieć: wywołanie `bezpieczne_dzielenie(10, 0)` bez `or` to błąd, który każe ci to obsłużyć.
(Problem można też przekazać dalej przez `try`; wyjaśnia to
[przewodnik dla techników](dla-technikow.md#błędy-jako-wartości).)

---

## 17. Małe projekty do zrobienia

Spróbuj najpierw napisać każdy sam. Do rozwiązania zajrzyj dopiero wtedy, gdy utkniesz.

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

Dla każdej liczby od 1 do 10 pokaż, czy jest parzysta, czy nieparzysta.

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

Pokaż tabelę od 0 °C do 100 °C co 10 stopni. Wzór: `F = C * 9 / 5 + 32`.

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

Policz od 1 do 20. Dla liczb podzielnych przez 3 powiedz `Fizz`, przez 5 `Buzz`,
przez obie `FizzBuzz`, a w pozostałych przypadkach samą liczbę.

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

Napisz funkcję `najwieksza(int a, int b, int c) -> int`, która zwraca największą liczbę.

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

Komputer wybiera tajną liczbę. Gracz zgaduje, aż trafi, a komputer za każdym razem
mówi „więcej” albo „mniej”.

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
            print("Tak! Liczba prób:", proby)
            return
        }
    }
}
```

`return` w `main` kończy program. (Wersja z losową liczbą jest w `examples/guess.fch`.)
</details>

### Projekt 7: Średnia ocen

Zapytaj o 3 oceny, zapisz je na liście i pokaż listę oraz średnią.

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

    x := 5                        // nowe pudełko (typ rozpoznany sam)
    int y = 10                    // nowe pudełko (typ podany)
    x = 7                         // zmiana zawartości pudełka
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

fn dodaj(int a, int b) -> int {   // twoje własne polecenie
    return a + b
}

struct Zwierzak {                 // twój własny rodzaj wartości
    str imie
    int wiek = 1
}

// listy:   liczby := [1, 2, 3]   liczby.push(4)   liczby[0]   liczby.len   for n in liczby { }   for i, n in liczby { }
// mapy:    wiek := ["Ola": 9]   wiek["Jan"] = 10   wiek["Ola"]   wiek.has("Tomek")   for imie, w in wiek { }
// tekst:   "a" + "b"   str(42)   int("42")   s.len   s.upper()   s.contains("x")
// pytanie: imie := input("Imię? ")
// pliki:   write_file("p.txt", tekst)   read_file("p.txt")
// metody:  fn Zwierzak.urosnij() { self.wiek += 1 }    z.urosnij()
// porażki: n := int(tekst) or 0      x := read_file("p.txt") or { print(err) ... }
```

---

## 19. Słowa, które będziesz słyszeć

| Słowo | Znaczenie |
|---|---|
| **program** | Lista kroków dla komputera |
| **kod** | Tekst programu |
| **uruchomić** | Kazać komputerowi wykonać kroki |
| **skompilować** | Przetłumaczyć kod na coś, co komputer potrafi uruchomić. `finch run` robi to za ciebie |
| **zmienna** | Pudełko z nazwą, w którym leży wartość |
| **typ** | Rodzaj wartości: liczba, tekst, tak/nie… |
| **funkcja** | Nazwana grupa kroków, której można używać wiele razy |
| **pętla** | Kroki, które się powtarzają |
| **warunek** | Pytanie typu tak/nie, np. `x > 5` |
| **lista / tablica** | Wiele wartości po kolei, np. `[1, 2, 3]` |
| **indeks** | Miejsce wartości na liście, liczone od 0 |
| **struktura** | Twój własny rodzaj wartości z nazwanych części |
| **metoda** | Polecenie należące do struktury, np. `g.trafiony()` |
| **mapa** | Wartości wyszukiwane po kluczu (nazwie), jak w słowniku |
| **wejście** (ang. *input*) | To, co ktoś wpisuje do programu |
| **błąd** (ang. *error*) | Komunikat, że coś jest nie tak, i gdzie |
| **bug** | Pomyłka w programie |

---

**Co dalej?** Kiedy to wszystko będzie już łatwe, przejdź do
[przewodnika dla techników](dla-technikow.md). Opisuje każdą część Fincha.
