# Finch dla inżynierów

**Kompletny opis wnętrza kompilatora Finch 1.0.** Opisuje, jak kod źródłowy staje się tokenami,
tokeny drzewem AST, a AST kodem LLVM IR, który potem jest optymalizowany, zapisywany do pliku
obiektowego i linkowany. Zawiera dokładne reguły typów, sposób tłumaczenia każdej konstrukcji na
IR (z prawdziwym wyjściem kompilatora), import nagłówków C przez libclang, szczegóły ABI
oraz opis każdej funkcji kompilatora wraz z tym, czym różni się od sąsiednich.

Zakładam, że znasz C/C++ i widziałeś już LLVM IR.
Język z perspektywy użytkownika opisuje [przewodnik dla techników](dla-technikow.md).

> 🇬🇧 English version: [for-engineers.md](../en/for-engineers.md)

---

## Spis treści

1. [Architektura](#1-architektura)
2. [Pliki źródłowe](#2-pliki-źródłowe)
3. [Lekser](#3-lekser)
4. [Gramatyka i parser](#4-gramatyka-i-parser)
5. [AST](#5-ast)
6. [Model semantyczny](#6-model-semantyczny)
7. [System typów](#7-system-typów)
8. [Generowanie kodu](#8-generowanie-kodu)
9. [Kontrole w czasie działania](#9-kontrole-w-czasie-działania)
10. [Współpraca z C przez libclang](#10-współpraca-z-c-przez-libclang)
11. [Optymalizacja, emisja, linkowanie](#11-optymalizacja-emisja-linkowanie)
12. [Diagnostyka](#12-diagnostyka)
13. [System budowania](#13-system-budowania)
14. [Testy](#14-testy)
15. [Semantyka a C: zachowania zdefiniowane i niezdefiniowane](#15-semantyka-a-c-zachowania-zdefiniowane-i-niezdefiniowane)
16. [Spis funkcji: kto co robi](#16-spis-funkcji-kto-co-robi)
17. [Rozbudowa kompilatora](#17-rozbudowa-kompilatora)
18. [Znane ograniczenia i plan](#18-znane-ograniczenia-i-plan)

---

## 1. Architektura

Finch to klasyczny jednoprzebiegowy front-end przed LLVM. Sprawdzanie typów i generowanie IR
odbywają się w jednym przejściu po AST. Nie ma osobnego przebiegu semantycznego ani własnej
reprezentacji pośredniej.

```
 plik.fn
    │  readFile()                                    src/main.cpp
    ▼
 lex()  ──────────► std::vector<Token>               src/lexer.cpp
    ▼
 parse() ─────────► Program (AST)                    src/parser.cpp, src/ast.h
    │                 ├─ imports ──► importHeaders() ──► CImports    src/cimport.cpp (libclang)
    │                 └─ links
    ▼
 hostMachine() ───► llvm::TargetMachine (triple, data layout)
    ▼
 generate() ──────► llvm::Module  (typy + IRBuilder, potem verifyModule)   src/codegen.cpp
    ▼
 optimize() ──────► potok PassBuilder default<O2>        (pomijany przy -O0)
    ▼
 emitObject() ────► plik.o   (legacy::PassManager + addPassesToEmitFile)
    ▼
 link() ──────────► cc plik.o -o plik -lm [-l… | pkg-config --libs …]
    ▼
 plik wykonywalny  (finch run: uruchamiany z pliku tymczasowego, potem usuwany)
```

Decyzje, które kształtują całą resztę:

- **Szybka porażka.** Pierwszy błąd wypisuje diagnostykę i wywołuje `exit(1)` (`fail()` w `src/error.h`).
  Nie ma odtwarzania po błędach, więc AST i codegen nigdy nie widzą częściowo poprawnego wejścia.
- **Jedno przejście.** `Codegen` niesie stan zasięgów, pętli i bieżącej funkcji. Każde wyrażenie
  zwraca jednocześnie wartość IR i swój typ Fincha (`Value_`).
- **Niech LLVM robi robotę.** Zmienne lokalne to `alloca`, a `mem2reg`/SROA zamieniają je na SSA.
  Finch nigdy sam nie buduje SSA ani węzłów φ, z wyjątkiem `&&`/`||`.
- **Systemowy toolchain C jest środowiskiem uruchomieniowym.** `printf`, `strcmp`, `dprintf`, `exit`
  pochodzą z libc. Finch nie ma własnej biblioteki uruchomieniowej.

---

## 2. Pliki źródłowe

| Plik | Linie | Odpowiedzialność |
|---|---|---|
| `src/error.h` | ~25 | `fail(line, col, msg)`: wypisuje `plik:linia:kol: error:` i fragment kodu z `^`, kończy proces. Trzyma `g_file`, `g_source`. |
| `src/lexer.h/.cpp` | ~240 | enum `Tok`, `Token`, `lex()`, `tokName()` |
| `src/ast.h` | ~235 | `Type`, wszystkie węzły `Expr`/`Stmt`, `FnDecl`, `Import`, `Link`, `Program` |
| `src/parser.h/.cpp` | ~430 | Parser zstępujący `Parser`, `typeFromName()` |
| `src/cimport.h/.cpp` | ~280 | Import nagłówków przez libclang: `importHeaders()` → `CImports` (funkcje, stałe, zmienne globalne) |
| `src/codegen.h/.cpp` | ~900 | `Codegen`: sprawdzanie typów + generowanie LLVM IR, `generate()` |
| `src/main.cpp` | ~260 | Sterownik: CLI, maszyna docelowa, optymalizacja, emisja, linkowanie, analiza błędów linkera, `run` |
| `tests/run.sh` | | Testy wzorcowego wyjścia i oczekiwanych błędów |

---

## 3. Lekser

`lex(src, file)` przechodzi raz, liniowo, przez źródło i zwraca `std::vector<Token>`
zakończony `Tok::End`.

```cpp
struct Token {
    Tok kind;
    std::string text;      // treść identyfikatora / literału (sekwencje \ już zdekodowane)
    int line, col;         // od 1
    bool newlineBefore;    // między poprzednim tokenem a tym był znak nowej linii
};
```

### Nowe linie jako koniec instrukcji

Finch nie ma średników. Zamiast emitować tokeny `NEWLINE`, jak robią to wewnętrznie Python i Go,
lekser **oznacza** każdy token flagą `newlineBefore`. Co ona znaczy, decyduje parser (rozdział 4).
Dzięki temu gramatyka nie zawiera tokenów nowej linii, które trzeba by wszędzie pomijać:
w listach argumentów, między `}` a `else` i tak dalej.

### Tokeny

- **Pozycje:** `line` i `col` liczone są od 1, a `col` liczy **znaki**, nie bajty.
  Bajty kontynuacji UTF-8 jej nie zwiększają, więc `^` trafia we właściwe miejsce także w linijkach z `"Błąd"`.
  Nieoczekiwany znak spoza ASCII jest pokazywany w całości (`'ż'`), z podpowiedzią, że nazwy są tylko ASCII.
- **Identyfikatory / słowa kluczowe:** `[A-Za-z_][A-Za-z0-9_]*`. Słowa kluczowe: `fn return if else while for in break continue true false null import link`.
  **Nazwy typów nie są słowami kluczowymi.** `int`, `u8`, `ptr`… są leksowane jako `Ident`, a parser rozpoznaje je przez `typeFromName()`.
  Lekser nie zależy więc od listy typów, a `u8(x)` parsuje się jak zwykłe wywołanie.
- **Liczby całkowite:** dziesiętne albo szesnastkowe `0x`, z separatorami `_` (usuwanymi z `text`).
  Zera wiodące nic nie zmieniają: Finch nie ma ósemkowych, `010` to 10. Wartość liczy parser (`strtoull`, podstawa 10 lub 16).
- **Liczby zmiennoprzecinkowe:** `cyfry.cyfry`. Kropka należy do liczby tylko wtedy, gdy **po niej jest cyfra**,
  więc `0..10` to `Int DotDot Int`, a nie `Float(0.) Dot …`.
- **Teksty / znaki:** `"…"` i `'…'`. Sekwencje `\n \t \r \0 \\ \" \'` są dekodowane do `text`.
  Nieznana sekwencja, niezamknięty literał albo nowa linia w środku literału to błąd. Literał znakowy musi dać dokładnie jeden bajt.
- **Operatory:** zasada najdłuższego dopasowania: `->` przed `-`, `<<` i `<=` przed `<`, `&&` przed `&`, `..` przed `.`, `:=` (samotny `:` to błąd z podpowiedzią `:=`).
- **Komentarze:** `//` do końca linii, `/* … */` (bez zagnieżdżania; niezamknięty to błąd).

---

## 4. Gramatyka i parser

`Parser` (`src/parser.cpp`) to ręcznie pisany parser zstępujący z jednym tokenem wyprzedzenia
(`cur()`), a w dwóch miejscach z drugim (`peekTok()`).

### Gramatyka (EBNF)

```ebnf
program     = { import | link | function } ;
import      = "import" STRING ;
link        = "link" STRING ;
function    = "fn" IDENT "(" [ param { "," param } ] ")" [ "->" type ] block ;
param       = type IDENT ;
type        = TYPENAME | "ptr" "[" type "]" ;          (* TYPENAME: int float bool char str ptr i8..u64 f32 f64 *)

block       = "{" { statement TERMINATOR } "}" ;
statement   = if | while | for | return | "break" | "continue" | block
            | type IDENT [ "=" expr ]                  (* deklaracja *)
            | IDENT ":=" expr                          (* deklaracja z wnioskowaniem *)
            | target assignop expr                     (* target: IDENT albo postfix kończący się .value *)
            | call ;
if          = "if" expr block [ "else" ( if | block ) ] ;
while       = "while" expr block ;
for         = "for" IDENT "in" expr ".." expr block ;
return      = "return" [ expr ] ;                      (* expr tylko w tej samej linii *)
assignop    = "=" | "+=" | "-=" | "*=" | "/=" | "%=" ;

expr        = or ;
or          = and { "||" and } ;
and         = cmp { "&&" cmp } ;
cmp         = add { ( "==" | "!=" | "<" | "<=" | ">" | ">=" ) add } ;
add         = mul { ( "+" | "-" | "|" | "^" ) mul } ;
mul         = unary { ( "*" | "/" | "%" | "<<" | ">>" | "&" ) unary } ;
unary       = ( "-" | "!" | "~" ) unary | postfix ;
postfix     = primary { "." IDENT } ;
primary     = INT | FLOAT | STRING | CHAR | "true" | "false" | "null"
            | IDENT [ "(" [ expr { "," expr } ] ")" ]
            | "(" expr ")" ;
```

### Reguła końca instrukcji

`TERMINATOR` nie jest tokenem. Po każdej instrukcji `endOfStatement()` wymaga, żeby następny
token był `}`, końcem pliku albo miał `newlineBefore == true`. W przeciwnym razie zgłasza błąd
`put each statement on its own line`.

Wewnątrz wyrażeń `sameLine()` decyduje, czy operator binarny, `.` albo `(` wywołania
**kontynuuje** bieżące wyrażenie:

```cpp
bool sameLine() const { return parenDepth > 0 || !cur().newlineBefore; }
```

Czyli:

```c
a := 1 +        // '+' jest w tej linii → wyrażenie przechodzi do następnej
     2
b := (1         // w nawiasach (parenDepth > 0) nowe linie są ignorowane
      + 2)
c := 1
-5              // '-' zaczyna nową linię → instrukcja skończyła się na '1'; "-5" to wtedy błąd
```

`parenDepth` rośnie wokół grup `( … )`, list argumentów i list parametrów. Przy wejściu do bloku
`{ }` jest **zapamiętywany i zerowany**, więc blok wewnątrz nawiasów (dziś to się nie zdarza)
i tak używałby reguł linii.

`(` wywołania musi być w tej samej linii co nazwa funkcji. Inaczej `f⏎(x)` po cichu stałoby się wywołaniem.

### Priorytety: jak w Go, nie jak w C

| Poziom | Operatory |
|---|---|
| 5 | `*` `/` `%` `<<` `>>` `&` |
| 4 | `+` `-` `\|` `^` |
| 3 | `==` `!=` `<` `<=` `>` `>=` |
| 2 | `&&` |
| 1 | `\|\|` |

Wszystkie operatory binarne są lewostronnie łączne. Porównania też (`a < b < c` się sparsuje),
ale sprawdzanie typów odrzuci potem porównanie `bool < int`. C stawia `&`, `^`, `|` **poniżej** `==`,
przez co `x & MASKA == 0` znaczy `x & (MASKA == 0)`. Finch przyjmuje tabelę z Go, co usuwa całą tę klasę błędów.

### Jak rozpoznawane są instrukcje

`statement()` rozgałęzia się po pierwszym tokenie:

1. Słowa kluczowe (`if`, `while`, `for`, `return`, `break`, `continue`, `{`) są obsługiwane wprost.
   `fn`, `import` i `link` wewnątrz bloku dają celowane komunikaty.
2. **Deklaracja z typem:** `atDeclaration()` zwraca prawdę, gdy bieżący token jest nazwą typu,
   a następny identyfikatorem (`int x`), albo gdy to `ptr`, a po nim `[` (`ptr[int] p`).
   To jedyne miejsce wymagające dwóch tokenów wyprzedzenia.
3. **Deklaracja z wnioskowaniem:** `IDENT`, a po nim `:=`.
4. W pozostałych przypadkach parser **najpierw parsuje całe wyrażenie**, a potem patrzy na następny
   token. Jeśli to operator przypisania w tej samej linii, wyrażenie staje się celem przypisania.
   Musi być `VarExpr` albo `MemberExpr`, inaczej: `only a variable or p.value can be assigned to`.
   Takie podejście („najpierw wyrażenie, potem decyzja”) obsługuje dowolne formy l-wartości
   (`pp.value.value = 1`) bez osobnej gramatyki l-wartości.
5. Samodzielne wyrażenie jako instrukcja musi być `CallExpr`. Wszystko inne daje
   `this value is computed but never used`.

### Poziom pliku

`program()` przechodzi przez `import`, `link` i `fn`. Element zaczynający się nazwą typu dostaje
podpowiedź `functions start with 'fn', like: fn add(int a, int b) -> int { ... }`, co łapie
definicje funkcji pisane jak w C. Argument `link` jest walidowany: `"libfoo.so"` albo ścieżka
są odrzucane z podpowiedzią samej nazwy.

---

## 5. AST

Wszystkie węzły są w `src/ast.h`. Wyrażenia i instrukcje to małe hierarchie klas z jawnym
znacznikiem `kind`. Kod je przetwarzający robi `switch` po `kind` i `static_cast`, co pozwala
obejść się bez RTTI i `dynamic_cast` i sprawia, że rozgałęzienia są oczywiste.

```cpp
struct Expr { ExprKind kind; Pos pos; virtual ~Expr(); };
using ExprPtr = std::unique_ptr<Expr>;
```

| ExprKind | Węzeł | Pola |
|---|---|---|
| `Int` | `IntExpr` | `long long value` |
| `Float` | `FloatExpr` | `double value` |
| `Bool` / `Char` / `Str` | `BoolExpr` / `CharExpr` / `StrExpr` | `value` |
| `Null` | `NullExpr` | |
| `Var` | `VarExpr` | `name` |
| `Unary` | `UnaryExpr` | `char op` (`-` `!` `~`), `operand` |
| `Binary` | `BinaryExpr` | `BinOp op`, `lhs`, `rhs` |
| `Call` | `CallExpr` | `std::string callee`, `args` |
| `Member` | `MemberExpr` | `obj`, `field` (w 1.0 ma sens tylko `value`) |

| StmtKind | Węzeł | Pola |
|---|---|---|
| `Block` | `BlockStmt` | `std::vector<StmtPtr> body` |
| `VarDecl` | `VarDeclStmt` | `hasType`, `type`, `name`, `init` (może być null, gdy `hasType`) |
| `Assign` | `AssignStmt` | `target` (Var/Member), `char op` (`= + - * / %`), `value` |
| `Expr` | `ExprStmt` | `expr` (zawsze wywołanie) |
| `If` | `IfStmt` | `cond`, `then`, `otherwise` (null, `BlockStmt` albo zagnieżdżony `IfStmt` dla `else if`) |
| `While` | `WhileStmt` | `cond`, `body` |
| `For` | `ForStmt` | `var`, `from`, `to`, `body` |
| `Return` | `ReturnStmt` | `value` (może być null) |
| `Break` / `Continue` | | |

Wywołania przechowują funkcję **po nazwie**, a nie jako wyrażenie. Finch nie ma funkcji jako
wartości, a rozwiązywanie nazw dopiero w codegenie pozwala, żeby jeden `CallExpr` oznaczał
funkcję wbudowaną (`print`, `addr`), konwersję (`u8`), funkcję Fincha albo funkcję C (rozdział 8.10).

### Przykład

```c
x += add(x, 2)
```

parsuje się do:

```
AssignStmt (op '+')
├─ target: VarExpr "x"
└─ value:  CallExpr "add"
           ├─ VarExpr "x"
           └─ IntExpr 2
```

**W AST nie ma węzła „load”.** To, czy `VarExpr` znaczy „adres x”, czy „wartość w x”, zależy
od miejsca, w którym występuje. Decyduje o tym codegen (`place()` kontra `expr()`, rozdział 8.2).

### `Type`

```cpp
struct Type {
    enum Kind { Void, Bool, Char, I8, I16, I32, I64, U8, U16, U32, U64, F32, F64, Str, Ptr, Null };
    Kind kind;
    std::shared_ptr<Type> elem;   // tylko Ptr: typ wskazywany; null = `ptr` bez typu
};
```

- `int` i `i64` to **ten sam typ** (`I64`), a `name()` wypisuje go jako `int`. Analogicznie `float` to `F64`.
- `Null` to typ wyłącznie literału `null`. Żadna zmienna nie może go mieć.
- `Void` to „typ” wywołań, które nic nie zwracają, i funkcji bez `->`.
- Równość jest strukturalna (`ptr[ptr[int]] == ptr[ptr[int]]`).
- Rodzaje są ułożone tak, że `isInt()` to `I8..U64`, `isSigned()` to `I8..I64`, a `isUnsigned()` to `U8..U64`.

---

## 6. Model semantyczny

Całe sprawdzanie semantyczne odbywa się w `Codegen` podczas emisji IR.

### Funkcje

`Codegen::run()` robi dwa przejścia po `Program::fns`:

1. `declare()` tworzy każdą `llvm::Function` (tylko sygnaturę), więc **kolejność definicji nie ma
   znaczenia** i działa rekurencja wzajemna. Sprawdza też: duplikaty nazw, nazwy wbudowane i sygnaturę
   `main` (bez parametrów; nic nie zwraca, zwraca `int` albo `i32`).
2. `define()` generuje ciała.

Bez `main` kompilacja kończy się błędem `there is no main function`.

### Zasięgi

`scopes` to `std::vector<std::unordered_map<std::string, Var>>`. Nowa mapa jest wkładana dla
parametrów funkcji, dla każdego bloku `{ }` i dla każdej pętli `for` (trzyma zmienną pętli).

```cpp
struct Var { AllocaInst *slot; FType type; bool readonly; };
```

**Bez przesłaniania:** `addVar()` odrzuca nazwę, którą `lookup()` znajdzie w *dowolnym*
zewnętrznym zasięgu, a nie tylko w bieżącym. Odrzuca też nazwy funkcji i wbudowanych.
To świadoma reguła prostoty: w całym ciele funkcji nazwa znaczy jedną rzecz.

**Tylko do odczytu:** zmienne `for` mają `readonly`. Przypisanie do nich albo `addr()` na nich
to błąd, więc licznika pętli nie da się zmienić za jej plecami.

### Rozwiązywanie identyfikatorów

Dla `VarExpr name` w pozycji wartości kolejność jest taka:

1. zmienna lokalna (`lookup`),
2. stała C (`cimports.consts`: wartości enum, liczby z `#define`),
3. zmienna globalna C (`cimports.globals`: `stdout`, …),
4. błąd `there is no variable named 'name'`.

Dla wywołań: `print` → `addr` → nazwa typu (konwersja) → funkcja Fincha → funkcja C → błąd.
Ponieważ funkcje Fincha są sprawdzane przed funkcjami C, **funkcja Fincha przesłania funkcję C
o tej samej nazwie**. Funkcja Fincha jest emitowana jako `finch.<nazwa>`, więc oba symbole
współistnieją przy linkowaniu.

---

## 7. System typów

### Reprezentacja w LLVM

| Finch | LLVM | Uwagi |
|---|---|---|
| `bool` | `i1` | |
| `char`, `i8`, `u8` | `i8` | znakowość jest w typie Fincha, nie w LLVM |
| `i16`, `u16` | `i16` | |
| `i32`, `u32` | `i32` | |
| `int`/`i64`, `u64` | `i64` | |
| `f32` | `float` | |
| `float`/`f64` | `double` | |
| `str` | `ptr` | napis C zakończony zerem |
| `ptr`, `ptr[T]` | `ptr` | LLVM 21 ma nieprzezroczyste wskaźniki; typ wskazywany istnieje tylko w typie Fincha |
| `null` | `ptr` | `ConstantPointerNull` |
| nic | `void` | |

Liczby całkowite w LLVM nie mają znaku. Każda operacja, w której znak ma znaczenie
(`sdiv`/`udiv`, `srem`/`urem`, `ashr`/`lshr`, `icmp slt`/`ult`, `sext`/`zext`,
`sitofp`/`uitofp`, `fptosi`/`fptoui`), wybiera instrukcję na podstawie typu **Fincha**.

### Wartości w codegenie

```cpp
struct Value_ {
    Value *v;          // wartość IR
    FType type;        // jej typ w Finchu
    bool literal;      // liczba wpisana w kodzie (albo zaimportowana stała C)
};
```

### Literały („stałe bez typu”)

`literal` jest ustawiane dla `IntExpr`, `FloatExpr` i zaimportowanych stałych C. Przechodzi
przez minus jednoargumentowy i przez arytmetykę **między dwoma literałami**, która zwija się do
stałej (`arith()` ustawia `literal = l.literal && r.literal && isa<Constant>(wynik)`). **Jawna
konwersja je kasuje:** `u8(250)` to stała typu `u8`, a nie literał.

Literał całkowity może stać się **dowolnym typem całkowitym, w którym się mieści** (`fits()`
sprawdza zakres, uwzględniając znakowość źródłowego literału). Dzięki temu `u8 b = 200`,
`malloc(4 * 10)` (do `u64`) i `glClear(GL_COLOR_BUFFER_BIT)` (makro `i64` do parametru `u32`)
działają bez rzutowań, a `u8 b = 300` jest odrzucane komunikatem `the number 300 doesn't fit in u8`.
Literał `f64` może stać się `f32` (`f32 h = 0.5`).

Czemu nie wystarczy sprawdzać `isa<ConstantInt>`? `IRBuilder` zwija stałe, więc `u8(250)` też jest
`ConstantInt`. Gdyby regułą było „jest stałą”, `u8(250) + 10` po cichu rozszerzyłoby się do
`int` i dało 260, a ten sam kod ze zmienną `u8` zawinąłby się do 4. Jawna flaga sprawia, że
zachowanie zależy od tego, co *napisałeś*, a nie od tego, co zdołał zobaczyć optymalizator.

### Konwersje niejawne: `coerce(value, want, pos, what)`

Używane wszędzie, gdzie wartość trafia do miejsca o znanym typie: inicjalizatory, przypisania,
argumenty, wartości zwracane, warunki (`want = bool`), granice zakresu (`want = int`).
Dozwolone, po kolei:

1. identyczne typy,
2. `null` → dowolny `ptr…` albo `str`,
3. dowolny `ptr…` albo `str` → `ptr` bez typu, a `ptr` bez typu → dowolny `ptr[T]` (reguły `void*`; tak działa `ptr[i32] n = malloc(4)`),
4. int → int, gdy wartość jest mieszczącym się literałem **albo** `widens(from, to)`:
   - ta sama znakowość i ściśle więcej bitów, **albo**
   - bez znaku → ze znakiem o ściśle większej liczbie bitów,
   - (ze znakiem → bez znaku nigdy się nie rozszerza),
5. dowolna całkowita → dowolna zmiennoprzecinkowa (`sitofp`/`uitofp`),
6. `f32` → `f64` (`fpext`),
7. literał `f64` → `f32` (`fptrunc`, zwijane do stałej).

Wszystko inne to błąd. Komunikat podpowiada jawną konwersję (`use i32(...) to convert`)
i ostrzega, gdy obetnie wartość (`it cuts off the fraction`).

### Argumenty operatorów binarnych: `unify(l, r)`

Przed arytmetyką i porównaniem obie strony są sprowadzane do jednego typu:

1. ten sam typ → gotowe,
2. obie całkowite: strona-literał dopasowuje się do drugiej, jeśli się mieści; w przeciwnym razie
   rozszerzana jest strona, która `widens()` do drugiej; w przeciwnym razie **błąd** (`i32` + `u32`:
   żadna nie rozszerza się do drugiej, a Finch nie zgaduje),
3. całkowita i zmiennoprzecinkowa → całkowita zamieniana na ten typ zmiennoprzecinkowy,
4. `f32` i `f64` → obie `f64`.

Typ wyniku arytmetyki to ten wspólny typ. **Nie ma promocji całkowitych jak w C:**
`u8 + u8` liczy się w `u8` i się zawija. To celowe: tego wymaga typ z rozmiarem, i tak samo działają Rust i Zig.

### Konwersje jawne: `convert(call, to)`

`T(x)` parsuje się jako wywołanie o nazwie typu i jest tłumaczone tutaj:

| Z → Na | Instrukcja |
|---|---|
| int/char → int/char | `CreateIntCast(signed = from.isSigned())` → `trunc` / `sext` / `zext` |
| bool → int/char | `zext` |
| float → int | `fptosi` / `fptoui` (wg znakowości celu) |
| int → float | `sitofp` / `uitofp` (wg znakowości źródła) |
| float → float | `fpext` / `fptrunc` (`CreateFPCast`) |
| int → bool | `icmp ne x, 0` |
| ptr/str/null → ptr…, ptr → str | brak instrukcji (wszystko to `ptr`) |

`fptosi` dla wartości spoza zakresu to w LLVM poison (w C: UB). Finch 1.0 tego nie sprawdza, zobacz rozdział 15.

### Porównania: `compare()`

- **Wskaźniki:** jeśli którakolwiek strona jest `ptr…` albo `null`, dozwolone są tylko `==`/`!=`,
  kompilowane do `icmp eq/ne ptr`. `str` porównywany z `null` też porównuje adresy.
- **`str == str`:** wywołanie `strcmp`, a potem `icmp eq/ne i32 %r, 0` (porównanie treści). `<`/`>` na `str` są odrzucane.
- **Liczby zmiennoprzecinkowe:** `oeq olt ole ogt oge` (uporządkowane: fałsz dla NaN) oraz `une` dla `!=`
  (nieuporządkowane: `NaN != NaN` to prawda). Tak samo jak w C.
- **Całkowite / char:** `eq ne` oraz `slt sle sgt sge` albo `ult ule ugt uge` wg znakowości. `char` porównywany jest bez znaku.
- **bool:** tylko `==`/`!=`.

---

## 8. Generowanie kodu

`Codegen` posiada `llvm::IRBuilder<> b`, `Module`, tablicę funkcji `fns`
(`nazwa → {Function*, const FnDecl*}`), `scopes`, stos `loops` i `curFn`.

### 8.1 Zmienne to miejsca na stosie

Każda zmienna, **łącznie z parametrami**, dostaje `alloca` tworzoną przez `slot()`.
`slot()` zawsze wstawia ją **na początek bloku wejściowego**, drugim `IRBuilder` ustawionym na
`entry.begin()`, niezależnie od miejsca deklaracji. Tylko takie `alloca` `mem2reg`/`SROA` umieją
zamienić na rejestry SSA, więc wyjście O2 nie ma ruchu na stosie dla wartości skalarnych.
Ponieważ każde nowe miejsce trafia na sam początek, w IR z `-O0` `alloca` są w **odwrotnej kolejności
deklaracji** (`%b2` przed `%a1` niżej).

Parametry są kopiowane do swoich miejsc na wejściu do funkcji (`store %a, %a1`), dzięki czemu
parametry są zwykłymi, modyfikowalnymi zmiennymi lokalnymi.

### 8.2 Load i store: `expr()` kontra `place()`

To jest serce generowania dostępów do pamięci:

- `place(e)` zwraca **`Place {Value *addr; FType type}`**, czyli *adres* l-wartości.
  - `VarExpr` → `alloca` zmiennej (albo `GlobalVariable` zmiennej globalnej C).
  - `MemberExpr` `p.value` → `expr(p)` (które **ładuje wskaźnik** z miejsca p) plus kontrola null
    (rozdział 9). Adresem jest sam załadowany wskaźnik, a typem `*p.type.elem`.
- `expr(e)` zwraca **`Value_`**, czyli r-wartość. Dla `Var` i `Member` woła `place()`, a potem
  emituje **jeden `load`** typu `ty(place.type)` z `place.addr`.

Instrukcja `load` dla identyfikatora powstaje więc w `Codegen::expr()`, w przypadku `ExprKind::Var`,
jako `b.CreateLoad(ty(pl.type), pl.addr, v.name)`. Nazwa wzięta od zmiennej sprawia, że IR z `-O0`
pokazuje `%x1`, `%x2` itd. (LLVM dokleja numery do powtórzonych nazw). `p.value` kosztuje **dwa**
loady: wskaźnika i wartości wskazywanej. `pp.value.value` kosztuje trzy.

Instrukcje store powstają w dokładnie trzech miejscach: `addVar()` (inicjalizacja), `assign()` i krok pętli `for`.

### 8.3 Przypisanie i kolejność obliczeń

```c
x += add(x, 2)
```

`assign()` **najpierw** liczy `place(target)`. Dla operatorów złożonych potem **ładuje bieżącą
wartość**, następnie oblicza prawą stronę, wywołuje `arith()`, robi `coerce()` z powrotem do typu
celu i zapisuje. Prawdziwe wyjście z `-O0`:

```llvm
define i32 @main() {
entry:
  %x = alloca i64, align 8
  store i64 5, ptr %x, align 8                       ; x := 5
  %0 = load i64, ptr %x, align 8                     ; bieżące x (dla +=)
  %x1 = load i64, ptr %x, align 8                    ; argument x
  %1 = call i64 @finch.add(i64 %x1, i64 2)
  %2 = add i64 %0, %1
  store i64 %2, ptr %x, align 8
  %x2 = load i64, ptr %x, align 8
  %3 = call i32 (ptr, ...) @printf(ptr @fmt, i64 %x2)
  ret i32 0
}
```

Kolejność obliczeń w Finchu jest **zawsze od lewej do prawej**: cel, potem argumenty operatora
binarnego od lewej, potem argumenty wywołania od lewej. C zostawia większość tego nieokreśloną.
Zauważ, że w `x += f()` stara wartość `x` jest czytana **przed** wywołaniem `f()`. Gdyby `f`
zmieniła `x` przez wskaźnik, ta zmiana zostałaby nadpisana. To to samo co `x = x + f()`
czytane od lewej do prawej.

Po O2 cały program powyżej zwija się do `printf(@fmt, i64 12)`.

### 8.4 Funkcje, linkowanie i `main`

```llvm
define internal i64 @finch.add(i64 %a, i64 %b) {
entry:
  %b2 = alloca i64, align 8
  %a1 = alloca i64, align 8
  store i64 %a, ptr %a1, align 8
  store i64 %b, ptr %b2, align 8
  %a3 = load i64, ptr %a1, align 8
  %b4 = load i64, ptr %b2, align 8
  %0 = add i64 %a3, %b4
  ret i64 %0
}
```

- Funkcje użytkownika nazywają się **`finch.<nazwa>`** i mają **wiązanie wewnętrzne** (internal linkage).
  Kropka nie może wystąpić w identyfikatorze C, więc funkcja Fincha nigdy nie zderzy się z symbolem
  libc ani biblioteki, a wiązanie wewnętrzne pozwala LLVM swobodnie je wstawiać (inline),
  specjalizować albo usuwać. W C funkcja bez `static` jest zewnętrzna i musi zostać zachowana
  tak, jak ją napisano. To prawdopodobnie jeden z powodów, dla których rekurencyjne `fib(40)`
  działa odrobinę szybciej niż ten sam kod C pod `clang -O2` (ok. 0,19 s wobec 0,23 s na maszynie autora).
- `main` jest emitowane jako **`i32 @main()` z wiązaniem zewnętrznym**. `fn main()` zwraca `i32 0`
  na każdej ścieżce wyjścia, także przy jawnym `return`. `fn main() -> int` przycina swój wynik
  `i64` przez `CreateIntCast(…, i32, signed)`.
- Brakujący return: jeśli po ciele funkcji bieżący blok nie ma terminatora, funkcje `void` dostają
  `ret void` (albo `ret i32 0` dla `main`). Funkcje zwracające wartość dostają błąd kompilacji:
  `function 'f' can reach its end without returning int`. Sprawdza się to na faktycznie zbudowanym
  CFG (rozdział 8.5), więc `if/else` zwracające wartość w obu gałęziach jest akceptowane bez dodatkowej analizy.
- Arytmetyka jest emitowana **bez flag `nsw`/`nuw`**, więc przepełnienie liczb całkowitych to
  zdefiniowane zawinięcie w kodzie uzupełnień do dwóch (rozdział 15).

### 8.5 Sterowanie przebiegiem i stan „zakończony”

Blok, do którego wstawia builder, jest jedynym źródłem prawdy o osiągalności:

```cpp
bool terminated() { return b.GetInsertBlock()->getTerminator() != nullptr; }
```

Po `return`, `break` albo `continue` bieżący blok ma terminator.
`blockBody()` sprawdza `terminated()` **przed każdą instrukcją**. Jeśli jest ustawiony, instrukcja
jest nieosiągalna i to błąd kompilacji (`this code can never run`). Martwe bloki dla kodu po skoku nigdy nie powstają.

`continueAt(bb, deadEnd)` jest używane po `if` i `while`. Jeśli blok łączący `bb` **nie ma
poprzedników** (wszystkie ścieżki wróciły albo przerwały), zostaje usunięty, a builder zostaje
w `deadEnd`, który jest zakończony. Stan „zakończony” propaguje się więc na zewnątrz przez
zagnieżdżone konstrukcje i widzi go sprawdzanie brakującego returna.

**`if / else if / else`:** bloki `then`, `else` (jeśli jest) i `endif`. `else if` to `IfStmt`
w `otherwise`, generowany rekurencyjnie wewnątrz bloku `else`.

**`while`:** bloki `while.cond`, `while.body`, `while.end`. Jeśli warunek zwinie się do stałej
`true`, emitowany jest bezwarunkowy `br`, więc `while.end` ma poprzedników tylko przez `break`.
`while true` bez `break` zostawia więc funkcję „zakończoną”, a funkcja, której ostatnią instrukcją
jest taka pętla, nie potrzebuje końcowego `return`.

**`for i in a..b`:** `a` i `b` są obliczane **raz**, przed pętlą, i zamieniane na `int`.
Bloki: `for.cond` (`icmp slt i, end`), `for.body`, `for.step` (`i + 1`), `for.end`. `continue`
skacze do `for.step`, a nie do `for.cond`, więc inkrementacja nigdy nie jest pomijana.
Zmienna pętli żyje we własnym zasięgu wokół pętli.

```llvm
  store i64 0, ptr %i, align 8
  br label %for.cond
for.cond:
  %3 = load i64, ptr %i, align 8
  %4 = icmp slt i64 %3, 2
  br i1 %4, label %for.body, label %for.end
for.body:
  ...
  br label %for.step
for.step:
  %6 = load i64, ptr %i, align 8
  %7 = add i64 %6, 1
  store i64 %7, ptr %i, align 8
  br label %for.cond
for.end:
```

**`break` / `continue`:** `loops` to stos par `{continueTo, breakTo}`. `while` odkłada
`{while.cond, while.end}`, a `for` odkłada `{for.step, for.end}`. Poza pętlą to błąd.

### 8.6 Skrócone `&&` / `||`: jedyne ręcznie budowane φ

```c
fn check(int a, int b) -> bool {
    return a > 0 && b > 0
}
```

```llvm
  %a3 = load i64, ptr %a1, align 8
  %0 = icmp sgt i64 %a3, 0
  br i1 %0, label %and.rhs, label %and.end
and.rhs:
  %b4 = load i64, ptr %b2, align 8
  %1 = icmp sgt i64 %b4, 0
  br label %and.end
and.end:
  %2 = phi i1 [ false, %entry ], [ %1, %and.rhs ]
```

`logic()` zapamiętuje blok, w którym lewa strona się **skończyła** (`from`), i blok, w którym
skończyła się prawa (`rhsEnd`). To niekoniecznie bloki, w których obliczanie się zaczęło, bo
zagnieżdżone `&&` tworzy własne bloki. φ używa `!isAnd` jako stałej skrótu (`false` dla `&&`, `true` dla `||`).

### 8.7 `print`

`print(a, b, …)` kompiluje się do **jednego wywołania `printf`**. Format jest budowany w czasie
kompilacji z typów argumentów, łączony spacjami i zakończony `\n`:

| Typ Fincha | Format | Konwersja argumentu |
|---|---|---|
| całkowite ze znakiem | `%lld` | `sext` → `i64` |
| całkowite bez znaku | `%llu` | `zext` → `i64` |
| `f32`, `f64` | `%g` | `fpext` → `double` |
| `char` | `%c` | `zext` → `i32` |
| `str` | `%s` | bez zmian |
| `bool` | `%s` | `select i1, "true", "false"` |
| `ptr…` | `%p` | bez zmian (glibc wypisuje `(nil)` dla null) |
| `null` | dosłowny tekst `null` | brak |

Format jest generowany, więc teksty użytkownika zawsze idą jako argumenty `%s`, nigdy jako
format. `%` w tekście użytkownika jest bezpieczne. O2 często zamienia `printf("%s\n", s)` na `puts(s)`.

### 8.8 Teksty

Literał tekstowy to prywatna stała globalna `unnamed_addr` (`CreateGlobalString`), a jego
wartością jest `ptr` na nią. Optymalizator może scalać identyczne literały. Zmienna `str`
zadeklarowana bez wartości wskazuje na wspólną stałą `""`. W 1.0 teksty są **niezmienne
i statyczne**. Nie ma sklejania ani alokacji (rozdział 18).

### 8.9 `addr()` i wskaźniki

`addrOf()` wymaga argumentu `Var` albo `Member` i zwraca `place(arg).addr` z typem `ptr[T]`.
Nie ma żadnego load: `addr(x)` *to jest* `alloca` zmiennej x. Takiej `alloca` mem2reg nie zamieni
na rejestr, co jest spodziewanym kosztem pobrania adresu. `addr(p.value)` daje wskaźnik już
zapisany w `p`, po kontroli null. `addr` zmiennej `for` jest odrzucane (jest tylko do odczytu).

### 8.10 Wywołania: `callFinch()` kontra `callC()`

Obie sprawdzają liczbę argumentów i robią `coerce()` każdego argumentu do typu parametru. Różnice:

| | `callFinch()` | `callC()` |
|---|---|---|
| Wołana funkcja | `finch.<nazwa>`, utworzona w `declare()` | `getOrInsertFunction(nazwa, …)`: deklarowana leniwie przy pierwszym użyciu, potem używana ponownie |
| Zmienna liczba argumentów | nigdy | obsługiwana; dodatkowe argumenty dostają domyślne promocje C |
| Atrybuty parametrów | niepotrzebne | `signext`/`zeroext` dla parametrów i wartości zwracanych węższych niż 32 bity, na deklaracji i w miejscu wywołania |
| Nieobsługiwane sygnatury | niemożliwe | odrzucane z powodem zapisanym przez cimport (`passes a C struct by value`, …) |
| Wywołanie `main` | odrzucane | nie dotyczy |

Domyślne promocje dla części zmiennej: `f32` → `double`; `bool`, `char` i liczby całkowite
węższe niż 32 bity → `i32` (z rozszerzeniem znaku albo zerami). Wskaźniki, `str` i liczby
32/64-bitowe przechodzą bez zmian. To odwzorowuje reguły C, których oczekuje `va_arg` w `printf`.

Po co `signext`/`zeroext`: na x86-64 System V clang przekazuje `char`/`short`/`_Bool` już
rozszerzone do 32 bitów i tak je oznacza. Niektóre funkcje na tym polegają. Bez atrybutu LLVM
zostawiłby górne bity nieokreślone, co byłoby niezgodnością ABI z kodem C skompilowanym clangiem.

---

## 9. Kontrole w czasie działania

Finch usuwa dwa najczęstsze źródła niezdefiniowanego zachowania w C, które da się tanio sprawdzić.
Wszystkie kontrole korzystają z jednej funkcji pomocniczej na moduł:

```llvm
; Function Attrs: cold noinline noreturn
define internal void @finch.panic(ptr %0, i64 %1) #0 {
entry:
  %2 = call i32 (i32, ptr, ...) @dprintf(i32 2, ptr @panic.fmt, ptr @panic.file, i64 %1, ptr %0)
  call void @exit(i32 1)
  unreachable
}
```

`panicFn()` tworzy ją przy pierwszym użyciu. `panicIf(cond, msg, pos)` emituje
`br cond, %panic, %ok`, wywołanie `finch.panic(msg, linia)` i `unreachable` w `%panic`,
a dalej kontynuuje w `%ok`. Pisze na deskryptor 2 przez `dprintf`, więc nie zależy od symbolu
`stderr` z libc. Funkcja jest `cold` i `noinline`, więc układ skoków faworyzuje ścieżkę bez
błędu, a kontrola kosztuje jedno porównanie i dobrze przewidywany skok.

### Dzielenie i reszta całkowita: `checkDivisor()`

```llvm
define internal i64 @finch.div(i64 %a, i64 %b) {
  ...
  %0 = icmp eq i64 %b4, 0
  br i1 %0, label %panic, label %ok
panic:
  call void @finch.panic(ptr @panic.msg, i64 2)
  unreachable
ok:
  %1 = icmp eq i64 %b4, -1
  %2 = icmp eq i64 %a3, -9223372036854775808
  %3 = and i1 %2, %1
  br i1 %3, label %panic5, label %ok6
panic5:
  call void @finch.panic(ptr @panic.msg.1, i64 2)
  unreachable
ok6:
  %4 = sdiv i64 %a3, %b4
```

- Dzielnik będący **stałym zerem** to błąd **kompilacji**.
- Stały dzielnik różny od zera (i od −1 dla typów ze znakiem) nie generuje żadnej kontroli.
- Przy dzieleniu ze znakiem `MIN / -1` się przepełnia (na x86 kończy się SIGFPE). To też jest sprawdzane.
- Gdy wszystko jest znane, O2 usuwa kontrole całkowicie: `div(10, 2)` powyżej staje się `5` w `main`.

### `.value` na null: w `place()`

Każde `p.value`, którego wskaźnik nie jest oczywiście niezerowy (nie jest wprost `alloca` ani
zmienną globalną), dostaje `panicIf(icmp eq p, null, "used .value on a null pointer")`.
Bez tego LLVM traktuje load z null jako UB i może go **usunąć**. Zanim dodano tę kontrolę,
`print(p.value)` przy `p` równym null wypisywało śmieciową wartość pod O2 zamiast się wysypać.

---

## 10. Współpraca z C przez libclang

`importHeaders(imports, dir)` (`src/cimport.cpp`) działa **przed** codegenem i tworzy:

```cpp
struct CImports {
    std::unordered_map<std::string, CFunc>   fns;      // nazwa, nagłówek, wynik, parametry, variadic, powód braku obsługi
    std::unordered_map<std::string, CConst>  consts;   // wartości enum + liczbowe #define
    std::unordered_map<std::string, CGlobal> globals;  // zmienne extern
};
```

### Parsowanie

Dla **każdego** `import "h"` parsowana jest osobna jednostka translacji z pliku w pamięci
`finch_import.c` zawierającego `#include "h"`. Osobne jednostki dają dokładne przypisanie błędów:
brakujący nagłówek wskazuje swoją linijkę `import`. Flagi:

```
-x c -std=gnu11 -I<katalog pliku .fn> -isystem<każdy systemowy katalog nagłówków>
CXTranslationUnit_DetailedPreprocessingRecord   (żeby widzieć makra)
CXTranslationUnit_SkipFunctionBodies            (szybkość: ciała funkcji inline są zbędne)
```

- **Systemowe katalogi nagłówków** pochodzą z uruchomienia `$CC -E -v -x c /dev/null` i odczytania
  bloku między `#include <...> search starts here:` a `End of search list.`. Dzięki temu `stdio.h`
  rozwiązuje się tak samo jak dla systemowego kompilatora. Ma to znaczenie na NixOS, gdzie wrapper
  cc dokłada ścieżki ze store, o których „goły” libclang nic nie wie.
- **`gnu11`** zamiast `c11`: glibc w trybie ścisłym ukrywa `M_PI` i inne nazwy spoza ISO.
- **Diagnostyka** o wadze ≥ error przerywa import. `file not found` zamienia się w
  `can't find the C header 'h'`.

### Odwiedzający

`visit()` przechodzi po kursorach najwyższego poziomu. Wygrywa pierwsza deklaracja danej nazwy.

| Kursor | Działanie |
|---|---|
| `FunctionDecl` | Mapuje kanoniczny **typ funkcji** (`clang_getArgType`, więc parametry tablicowe są już zamienione na wskaźniki). Funkcje `static` są zapisywane jako nieobsługiwane (`static inline` nie ma symbolu do zlinkowania). `FunctionNoProto` (K&R `f()`) jest nieobsługiwane. Zmienna liczba argumentów pochodzi z `clang_isFunctionTypeVariadic`. |
| `VarDecl` z `extern` | → `CGlobal`, jeśli typ się mapuje |
| `EnumDecl` | rekurencja do dzieci |
| `EnumConstantDecl` | → `CConst` (`i64`, literał) |
| `MacroDefinition` | pomija wbudowane i makra-funkcje; `macroValue()` próbuje odczytać liczbę |

### Mapowanie typów: `mapType()`

Działa na typie **kanonicznym**, więc typedefy (`size_t`, `uint32_t`, `GLuint`, `FILE`) są rozwijane.
Typy całkowite są mapowane **według rozmiaru** (`clang_Type_getSizeOf`), a nie nazwy, więc
`long` staje się `i64` na LP64.

| C (kanoniczny) | Finch |
|---|---|
| `void` | nic |
| `_Bool` | `bool` |
| `char` (dowolna znakowość) | `char` |
| `signed char`, `short`, `int`, `long`, `long long`, `enum` | `i8`/`i16`/`i32`/`i64` wg rozmiaru |
| warianty `unsigned` | `u8`…`u64` wg rozmiaru |
| `float` / `double` | `f32` / `f64` |
| `char *` (dowolne kwalifikatory) | `str` |
| `T *`, gdzie T mapuje się na typ niebędący void | `ptr[T]` |
| `void *`, wskaźnik na struct/union/funkcję/typ niemapowalny | `ptr` (nieprzezroczysty) |
| struct/union przez wartość | **nieobsługiwane** („passes a C struct by value”) |
| `long double`, `__int128`, wektory, … | **nieobsługiwane** |

Funkcje nieobsługiwane zostają w tablicy, więc próba ich wywołania daje precyzyjny błąd zamiast
„nie ma takiej funkcji”.

### Makra: `macroValue()`

Tokenizuje zakres makra, odrzuca token nazwy i komentarze, zdejmuje **zrównoważone nawiasy
zewnętrzne**, akceptuje jeden wiodący `-` i wymaga **dokładnie jednego** literału liczbowego.
Liczby całkowite są czytane przez `strtoull(…, 0)`, czyli według **reguł C**: `0x` szesnastkowo
i zero wiodące jako **ósemkowe**, inaczej niż literały samego Fincha. Zmiennoprzecinkowe to
wszystko, co zawiera `.`, `e` lub `E` i nie jest szesnastkowe. Akceptowane są przyrostki
`u U l L f F`. To obejmuje `M_PI`, `EOF` (`(-1)`), `RAND_MAX`, `INT_MAX` oraz stałe `GL_*` i `GLFW_*`.
Wyrażenia (`(1 << 4)`), odwołania do innych makr i teksty są pomijane.

### Wywoływanie

Zaimportowane funkcje są deklarowane w module **dopiero przy wywołaniu**, przez
`getOrInsertFunction`. To używa istniejącej deklaracji, jeśli `print` już utworzył `printf`.
Sygnatury się zgadzają (`i32 (ptr, ...)`), więc nie ma konfliktu.

### Błędy linkowania

Gdy linkowanie się nie uda, `linkFailed()` w `main.cpp` przeszukuje wyjście linkera pod kątem
`undefined reference to \`sym'` (GNU ld) i `undefined symbol: sym` (lld). Grupuje symbole według
`CFunc::header` i wypisuje, z którego nagłówka pochodzą, razem z odgadniętą linijką `link`
(`guessLib()`: nazwa pliku bez rozszerzenia i końcowych cyfr, małymi literami:
`GLFW/glfw3.h` → `glfw`). `cannot find -lfoo` zamienia się w `the library 'foo' wasn't found`.

---

## 11. Optymalizacja, emisja, linkowanie

### Cel

`hostMachine()` tworzy `TargetMachine` dla `sys::getDefaultTargetTriple()` z CPU `"generic"`,
bez dodatkowych cech i z `Reloc::PIC_`, które jest wymagane dla domyślnych plików PIE we
współczesnych dystrybucjach. Jej **data layout jest ustawiany na module, zanim powstanie
jakikolwiek IR**, w konstruktorze `Codegen`. To ważne: `IRBuilder` bierze wyrównanie loadów
i store'ów z data layoutu i bez niego dostępy do `i64` byłyby emitowane z `align 4`.

`generic` znaczy, że plik zadziała na każdym x86-64. Zachowanie w stylu `-march=native` wymagałoby
zmiany nazwy CPU i cech w tym miejscu.

### Optymalizacja

`optimize()` buduje potok nowego menedżera przebiegów:

```cpp
PassBuilder pb(tm);
pb.register{Module,CGSCC,Function,Loop}Analyses(...);
pb.crossRegisterProxies(lam, fam, cgam, mam);
pb.buildPerModuleDefaultPipeline(OptimizationLevel::O2).run(mod, mam);
```

To jest środkowa część `-O2` z clanga: SROA/mem2reg, instcombine, inlining, GVN, LICM,
rozwijanie pętli, wektoryzacja i tak dalej. `-O0` pomija to wywołanie, co przydaje się do
czytania IR wyprodukowanego przez `Codegen` (`finch ir plik.fn -O0`). Generator kodu maszynowego
i tak działa na domyślnym poziomie TargetMachine.

### Emisja

`emitObject()` używa **starego** (legacy) menedżera przebiegów, bo potok kodu maszynowego LLVM
jest wciąż dostępny tylko przez niego: `tm->addPassesToEmitFile(pm, out, nullptr, CodeGenFileType::ObjectFile)`.

### Linkowanie

```
$CC plik.o -o plik -lm  [pkg-config --libs NAZWA | -lNAZWA]...
```

Jako linker służy systemowy sterownik kompilatora C (domyślnie `cc`). Zna pliki crt platformy,
ścieżkę dynamicznego loadera, libc i domyślne ścieżki bibliotek, a odtwarzanie tego byłoby
pracochłonne. `-lm` jest dodawane zawsze. Dla każdego `link "NAZWA"` i `-l NAZWA` najpierw
próbowane jest `pkg-config --libs NAZWA`, żeby dołączyć ścieżki `-L` i biblioteki zależne.
Jeśli to się nie uda, używane jest `-lNAZWA`. Na Niksie wrapper cc zamienia ścieżki `-L`
w wpisy `RPATH`, więc wynik działa bez `LD_LIBRARY_PATH`. Plik obiektowy jest usuwany po
linkowaniu, niezależnie od wyniku.

### `finch run`

Kompiluje do pliku tymczasowego (`sys::fs::createTemporaryFile`), uruchamia go przez `system()`,
usuwa i przekazuje dalej kod wyjścia. Jeśli proces potomny zginął od sygnału, wypisuje
`the program crashed: <strsignal>` i zwraca `128 + numer_sygnału`, jak powłoka.

---

## 12. Diagnostyka

`fail(line, col, msg)` jest `[[noreturn]]`: wypisuje

```
plik:linia:kol: error: msg
 linia | tekst źródła
       |      ^
```

i kończy proces z kodem 1. Fragment jest brany z `g_source`, a tabulatory w prefiksie są
zachowywane, żeby `^` trafiło w miejsce. Konwencje w całym kodzie:

- **Pozycje:** operatory binarne raportują pozycję **operatora**, wywołania pozycję nazwy funkcji,
  a błędy typów w argumentach pozycję **argumentu**.
- **Komunikaty** mówią prostymi słowami, co jest źle, i **jak to naprawić**, gdy naprawa jest
  oczywista (`use int(...) to convert`, `did you mean ':='?`, `add import "math.h"`).
- `headerHint()` mapuje około dwóch tuzinów popularnych nazw z libc/libm na ich nagłówki, na
  potrzeby wywołań nieznanych funkcji.
- Wewnętrzna niespójność, jeśli `verifyModule` kiedykolwiek zawiedzie, wypisuje
  `internal compiler error (please report)` z wyjściem weryfikatora i kończy proces z kodem 2.

Kompilator nie ma ostrzeżeń. Wszystko, co byłoby ostrzeżeniem, jest albo w porządku, albo błędem.

---

## 13. System budowania

`CMakeLists.txt`:

- `find_package(LLVM CONFIG)`. **Jeśli istnieje cel `LLVM` (współdzielone `libLLVM.so`), Finch linkuje
  tę jedną bibliotekę.** To nie kosmetyka, tylko konieczność: `libclang.so` sam linkuje `libLLVM.so`.
  Linkowanie Fincha ze statycznymi bibliotekami komponentów (`libLLVMCore.a`, …) wprowadza do procesu
  **drugą kopię LLVM**. Obie kopie rejestrują te same obiekty globalne i program wywracał się przy
  wyjściu w globalnym destruktorze (`std::vector<TensorSpec>::~vector`, podwójne zwolnienie).
  Do tego binarka miała 82 MB zamiast 11 MB.
- libclang jest szukany przez `find_path(clang-c/Index.h)` i `find_library(clang)`, z podpowiedzią katalogów LLVM.
- `FINCH_VERSION` pochodzi z `project(VERSION …)` i jest wypisywane przez `finch version` razem z `LLVM_VERSION_STRING`.
- C++17, `-Wall -Wextra`, a budowanie przechodzi bez ostrzeżeń.

---

## 14. Testy

`tests/run.sh` (ścieżkę kompilatora nadpisuje `FINCH=…`):

- `tests/run/*.fn`: uruchamiane przez `finch run`. Połączone stdout+stderr musi być dokładnie równe `*.out`.
- `tests/fail/*.fn`: muszą zakończyć się kodem różnym od zera, a wyjście musi zawierać tekst po
  `// expect: ` z pierwszej linii. To obejmuje błędy kompilacji, błędy w czasie działania (z numerami
  linii) oraz problemy z nagłówkami i linkowaniem. `tests/fail/lib_hint.h` deklaruje funkcję, której
  nie ma w żadnej bibliotece, żeby przetestować podpowiedź `link`.

Stan obecny: **34 passed, 0 failed.** Każdy nowy komunikat błędu powinien dostać test w `fail/`,
a każda nowa funkcjonalność test w `run/`.

---

## 15. Semantyka a C: zachowania zdefiniowane i niezdefiniowane

| Sytuacja | C | Finch 1.0 |
|---|---|---|
| Przepełnienie liczby ze znakiem | UB | **Zdefiniowane zawinięcie** (bez flag `nsw`) |
| Przepełnienie liczby bez znaku | zawija się | zawija się |
| Dzielenie całkowite przez zero | UB | **Błąd kompilacji**, jeśli stałe, w przeciwnym razie **błąd w czasie działania** |
| `INT_MIN / -1` | UB | **Błąd w czasie działania** |
| Load/store przez null | UB | **Błąd w czasie działania** dla `.value` |
| `x & 1 == 0` | parsuje się jako `x & (1 == 0)` | parsuje się jako `(x & 1) == 0` |
| Niejawne zwężenie (`int` → `char`) | po cichu | **Błąd kompilacji** |
| Mieszanie ze znakiem / bez znaku | cicha konwersja | **Błąd kompilacji**, chyba że jedna strona się rozszerza |
| Użycie zmiennej przed nadaniem wartości | UB | Niemożliwe: każda deklaracja inicjalizuje (domyślnie zerem) |
| Brak `return` w funkcji z wynikiem | UB, jeśli wynik jest użyty | **Błąd kompilacji** |
| Przesunięcie o ≥ szerokość bitową | UB | **Nadal poison** (w 1.0 niesprawdzane) |
| `float` → int poza zakresem | UB | **Nadal poison** (w 1.0 niesprawdzane) |
| Wiszący wskaźnik na martwą zmienną lokalną | UB | **Nadal UB** (brak kontroli czasu życia) |
| Kolejność obliczeń | w większości nieokreślona | Od lewej do prawej |

---

## 16. Spis funkcji: kto co robi

Grupy funkcji o podobnych nazwach i czym się różnią.

### Leksowanie i parsowanie

| Funkcja | Robi | Różni się od |
|---|---|---|
| `lex()` | tekst → tokeny, ustawia `newlineBefore` | nic nie wie o gramatyce ani typach |
| `tokName()` | rodzaj tokenu → nazwa czytelna dla człowieka (do błędów) | |
| `Parser::cur()` / `peekTok()` / `next()` | bieżący token / podgląd bez konsumowania / konsumpcja | |
| `accept(k)` kontra `expect(k)` | konsumuje, jeśli pasuje, i zwraca bool / konsumuje albo **zgłasza błąd** „expected …, found …” | |
| `unexpected(ctx)` | błąd „unexpected X (ctx)” na bieżącym tokenie | `expect` mówi, czego chciał; `unexpected` tłumaczy kontekst |
| `sameLine()` | czy bieżący token może kontynuować wyrażenie? | `endOfStatement()` to odpowiednik na poziomie instrukcji (wymaga nowej linii albo `}`) |
| `atDeclaration()` | podgląd `int x` / `ptr[...] p` | `type()` faktycznie konsumuje typ |
| `typeFromName()` kontra `isTypeName()` | nazwa → `Type` / nazwa → bool | |
| `statement()` kontra `block()` | jedna instrukcja / `{ instrukcja* }` z kontrolą końców i zerowaniem `parenDepth` | |
| `callArgs()` | wspólne dla wywołań i konwersji: `( expr, … )` | |
| `primary()` / `postfix()` / `unary()` / `multiplicative()` / `additive()` / `compare()` / `andExpr()` / `orExpr()` | po jednej funkcji na poziom priorytetu | |

### Codegen: typy i wartości

| Funkcja | Robi | Różni się od |
|---|---|---|
| `ty(FType)` | typ Fincha → typ LLVM | |
| `zero(FType)` | wartość domyślna dla deklaracji bez inicjalizatora | |
| `widens(from, to)` | czy każdą wartość `from` da się przedstawić w `to`? (czysta reguła typów) | `fits()` pyta o to samo dla jednej stałej |
| `fits(c, from, to)` | czy wartość tego literału mieści się w `to`? | |
| `coerce(v, want)` | **niejawna** konwersja do miejsca o znanym typie albo błąd | `convert()` to **jawne** `T(x)` i pozwala na stratne rzutowania; `unify()` zbliża do siebie **dwa** argumenty |
| `unify(l, r)` | sprowadza argumenty binarne do wspólnego typu | używane przez `arithOp()` i `compare()` |
| `intCast()` / `intToFloat()` | emitują rzutowanie z poprawną znakowością | niskopoziomowe pomocniki trzech powyższych |

### Codegen: wyrażenia

| Funkcja | Robi | Różni się od |
|---|---|---|
| `expr(e)` | r-wartość: emituje kod, zwraca `{wartość, typ, literal}` | `place()` zwraca **adres**, zamiast ładować |
| `place(e)` | adres l-wartości: zmiennej, zmiennej globalnej C albo `p.value` | używane przez `expr()` (potem load), `assign()` (potem store) i `addrOf()` (zwraca bez zmian) |
| `binary()` | rozgałęzienie po `BinOp` | |
| `arith()` kontra `arithOp()` | `arith()` opakowuje `arithOp()` i liczy flagę `literal` wyniku | `arithOp()` robi `unify` i emituje instrukcję |
| `compare()` | wszystkie porównania: wskaźniki/str/float/int | zawsze zwraca `bool` i nigdy nie jest literałem |
| `logic()` | `&&`/`\|\|` ze skokami i φ | jedyne miejsce budujące φ ręcznie |
| `call()` | zamienia nazwę wywołania na jeden z pięciu rodzajów | |
| `callFinch()` kontra `callC()` | zobacz rozdział 8.10 | |
| `print()` | buduje format printf z typów | |
| `addrOf()` | `addr(x)` → `place(x).addr` | |
| `convert()` | `T(x)` | |
| `libc()` | `getOrInsertFunction`: zadeklaruj albo użyj istniejącej funkcji zewnętrznej | `declare()` tworzy funkcje **Fincha** przez `Function::Create` |
| `cGlobal()` | zadeklaruj albo użyj istniejącej zmiennej `extern` | |

### Codegen: instrukcje i sterowanie

| Funkcja | Robi | Różni się od |
|---|---|---|
| `declare()` kontra `define()` | przejście sygnatur / przejście ciał | |
| `block()` kontra `blockBody()` | `block()` otwiera i zamyka zasięg wokół `blockBody()`; `blockBody()` emituje instrukcje i odrzuca nieosiągalne | `define()` woła `blockBody()` bezpośrednio, bo parametry i zewnętrzne ciało dzielą jeden zasięg |
| `stmt()` | rozgałęzienie po `StmtKind`; obsługuje też `break`/`continue` | |
| `varDecl()` / `assign()` | deklaracja z opcjonalnym wnioskowaniem / zapis do istniejącego miejsca | |
| `addVar()` | tworzy miejsce, zapisuje wartość początkową, rejestruje nazwę (z kontrolą przesłaniania) | `slot()` tylko tworzy `alloca` |
| `lookup()` | szuka we wszystkich zasięgach, od najbardziej wewnętrznego | |
| `condition(e)` | `coerce(expr(e), bool)` z komunikatem „a condition must be bool” | |
| `ifStmt()` / `whileStmt()` / `forStmt()` / `returnStmt()` | rozdział 8.5 | |
| `terminated()` | czy bieżący blok kończy się już terminatorem? | |
| `continueAt(bb, deadEnd)` | przejdź do bloku łączącego albo usuń go, jeśli jest nieosiągalny | |
| `newBlock()` | tworzy blok podstawowy w bieżącej funkcji | |

### Kontrole w czasie działania

| Funkcja | Robi |
|---|---|
| `panicFn()` | pobiera albo tworzy `finch.panic` |
| `panicIf(cond, msg, pos)` | warunkowy skok do bloku z panic |
| `checkDivisor(l, r, pos)` | kontrole zera i `MIN/-1` albo błąd kompilacji |

### Sterownik (`main.cpp`)

| Funkcja | Robi |
|---|---|
| `hostMachine()` | tworzy `TargetMachine` |
| `optimize()` | potok O2 |
| `emitObject()` | zapisuje `.o` |
| `capture()` | uruchamia polecenie powłoki, zbiera wyjście i kod wyjścia |
| `libFlags()` | pkg-config albo `-l` dla jednej biblioteki |
| `link()` | uruchamia linker, usuwa `.o`, tłumaczy porażki |
| `linkFailed()` / `guessLib()` | zamieniają błędy linkera na porady w języku Fincha |

---

## 17. Rozbudowa kompilatora

### Nowa funkcja wbudowana (przykład: `len(str) -> int`)

1. Dodaj nazwę do `Codegen::isBuiltin()`, żeby nie dało się jej zdefiniować ponownie.
2. W `Codegen::call()` dodaj rozgałęzienie `if (c.callee == "len") return len(c);`.
3. Zaimplementuj: sprawdź liczbę argumentów, `coerce(expr(arg), Str, …)`, wyemituj `strlen` przez `libc()`,
   zwróć `{call, I64}`. `strlen` zwraca `u64`; jeśli chcesz `int`, przekonwertuj przez `intCast`.
4. Dodaj `tests/run/len.fn` z plikiem `.out` i przypadek w `tests/fail/` dla złego argumentu.

### Nowy operator binarny

1. Lekser: nowy `Tok`, jego zapis w `switch` operatorów i w `tokName()`.
2. AST: nowy `BinOp`.
3. Parser: umieść go w funkcji właściwego poziomu priorytetu.
4. Codegen: `opName()`, emisja w `arithOp()` albo `compare()` i wyjaśnienie złych argumentów w `badOperands()`.

### Nowa instrukcja

Dodaj `StmtKind` i węzeł w `ast.h`, rozpoznaj ją w `Parser::statement()` i obsłuż w `Codegen::stmt()`.
Jeśli może przenosić sterowanie, upewnij się, że wynik respektuje `terminated()` i używa `continueAt()`
dla swojego bloku łączącego.

### Konwencje

Trzymaj się istniejącego stylu: wczesne `fail()` z podpowiedzią naprawy, komentarze tylko tam,
gdzie *dlaczego* nie jest oczywiste, i jeden test na jedno zachowanie.

---

## 18. Znane ograniczenia i plan

- **Brak typów złożonych:** `struct`, tablice/wycinki z `.len` i struktury C przez wartość. To kolejne
  kamienie milowe, w tej kolejności: tablice opierają się na strukturach (wskaźnik + długość),
  a model pamięci na obu.
- **Model pamięci** (uzgodniony projekt): wartości posiadające pamięć na stercie są zwalniane
  automatycznie na końcu bloku, który je posiada, `defer` uruchamia kod przy wyjściu z bloku,
  a `free` zostaje do ręcznej kontroli. Szkic implementacji: lista sprzątania na blok, emitowana na
  każdej krawędzi wyjścia (normalny koniec, `return`, `break`, `continue`). `blockBody()` i stos
  `loops` już znają każdą krawędź wyjścia.
- **Teksty:** sklejanie, `len`, `str(x)`. Zależą od modelu pamięci.
- **Wejście:** `input()`.
- **Moduły:** `import nazwa` dla kodu w Finchu. Dziś jeden `.fn` = jeden program.
- **Niesprawdzane UB:** wielkość przesunięcia, zakres float→int, wiszące wskaźniki. Przesunięcia
  i float→int dałoby się sprawdzać jak dzielenie, niewielkim kosztem.
- **Informacje dla debuggera:** brak DWARF. `DIBuilder` podpiąłby się w `define()` i przy `pos` każdej instrukcji.
- **Platformy:** tylko triple hosta, testowane na Linux/x86-64. Kod jest niezależny od platformy
  z wyjątkiem wykrywania nagłówków przez `cc -E -v` i użycia `dprintf`.
- **Bootstrap:** długoterminowym celem jest przepisanie kompilatora w Finchu. Wołanie API LLVM-C już
  działa: LLVM-C używa nieprzezroczystych uchwytów (`LLVMModuleRef` itd.), które mapują się na `ptr`
  Fincha. `examples/llvm.fn` buduje i wypisuje moduł z poziomu Fincha (`import "llvm-c/Core.h"`,
  `link "LLVM"`). Do samodzielnego kompilatora brakuje jeszcze struktur, tablic, tekstów i operacji na plikach.
