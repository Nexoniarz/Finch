# Finch dla inżynierów

**Kompletny opis wnętrza Fincha 2.0.** Jak kod źródłowy staje się tokenami, tokeny drzewem AST,
a AST kodem LLVM IR, który potem jest optymalizowany, zapisywany i linkowany z małym runtime'em w C.
Dokładne reguły typów; model własności i to, jak tłumaczy się na kopie i zwolnienia; jak każda
konstrukcja staje się IR (z prawdziwym wyjściem kompilatora); obsługa nagłówków C, struktur C i ABI
System V; moduły, informacje dla debuggera i kompilator samohostujący z `boot/`. Każda funkcja
kompilatora jest opisana razem z tym, czym różni się od sąsiednich.

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
6. [Wczytywanie: pliki i moduły](#6-wczytywanie-pliki-i-moduły)
7. [Model semantyczny](#7-model-semantyczny)
8. [System typów](#8-system-typów)
9. [Własność i pamięć](#9-własność-i-pamięć)
10. [Generowanie kodu](#10-generowanie-kodu)
11. [Kontrole w czasie działania](#11-kontrole-w-czasie-działania)
12. [Runtime w C](#12-runtime-w-c)
13. [Współpraca z C: nagłówki, struktury, ABI](#13-współpraca-z-c-nagłówki-struktury-abi)
14. [Optymalizacja, emisja, linkowanie](#14-optymalizacja-emisja-linkowanie)
15. [Informacje dla debuggera](#15-informacje-dla-debuggera)
16. [Diagnostyka](#16-diagnostyka)
17. [Kompilator samohostujący (boot/)](#17-kompilator-samohostujący-boot)
18. [System budowania](#18-system-budowania)
19. [Testy](#19-testy)
20. [Semantyka a C](#20-semantyka-a-c)
21. [Spis funkcji](#21-spis-funkcji)
22. [Rozbudowa kompilatora](#22-rozbudowa-kompilatora)
23. [Platformy i Windows](#23-platformy-i-windows)
24. [Serwer języka](#24-serwer-języka)
25. [Rozszerzenie VS Code](#25-rozszerzenie-vs-code)
26. [Znane ograniczenia](#26-znane-ograniczenia)

---

## 1. Architektura

Finch to jednoprzebiegowy front-end przed LLVM: sprawdzanie typów i generowanie IR to jedno przejście
po AST, bez własnej reprezentacji pośredniej.

```
 main.fch ──► Loader::load() ── lex() ── parse() ──► Program  (+ każdy importowany moduł, rekurencyjnie)
                                                       │
                wszystkie `import "x.h"` ──► importHeaders() (libclang) ──► CImports
                                                       │
 hostMachine() ──► TargetMachine (triple + data layout)│
                                                       ▼
 generate(progs, cimports, tm, debug) ──► Codegen::run()
     declareStructs()   wszystkie struktury Fincha i C, ciała typów LLVM, kontrola cykli
     declareFns()       sygnatura każdej funkcji (+ analiza „pożyczania” parametrów)
     define()           każde ciało: typy + IRBuilder, zasięgi, zwalnianie, kontrole
     defineMainWrapper  C-owe `main(argc, argv)`
     DIBuilder::finalize (z -g) ; verifyModule
                                                       ▼
 optimize()  PassBuilder default<O2>            (pomijane przy -O0)
 emitObject() legacy PM + addPassesToEmitFile ──► prog.o
 link()      cc prog.o ~/.cache/finch/rt-<hash>.o [twoje .c/.o/.a] -lm [pkg-config/-l…]
                                                       ▼
 plik wykonywalny   (finch run: uruchamiany z pliku tymczasowego z resztą argumentów, potem usuwany)
```

Decyzje, które kształtują resztę:

- **Szybka porażka.** Pierwszy błąd wypisuje diagnostykę i kończy proces (`failAt()` w `src/error.h`).
  Bez odtwarzania po błędach, więc dalsze fazy nigdy nie widzą częściowo poprawnego wejścia.
- **Jedno przejście.** `Codegen` niesie zasięgi, pętle, bieżącą funkcję i moduł. Każde wyrażenie zwraca
  wartość IR, typ Fincha i dwie flagi (`Value_`, §8).
- **Niech LLVM robi robotę.** Zmienne lokalne to `alloca`, które `mem2reg`/SROA zamieniają na rejestry.
  Węzły φ Finch buduje ręcznie tylko dla `&&`/`||`.
- **Wartości, nie referencje.** Tablice, teksty i struktury mają semantykę wartości: przypisanie kopiuje,
  koniec bloku zwalnia. Własność rozstrzyga się w czasie kompilacji, bez liczenia referencji (§9).
- **Malutki runtime w C** (`runtime/finch_rt.c`, ok. 430 linii) robi to, co w IR byłoby żmudne: operacje na
  tekstach, wzrost tablic, wejście, pliki, panic. Jest wbudowany w kompilator i trzymany w cache.

---

## 2. Pliki źródłowe

| Plik | Linie | Odpowiedzialność |
|---|---|---|
| `src/error.h` | 40 | `g_files` (wszystkie źródła), `failAt(file, line, col, msg)`, `fail(line, col, msg)` |
| `src/lexer.h/.cpp` | 250 | `Tok`, `Token`, `lex(indeksPliku)`, `tokName()` |
| `src/ast.h` | 310 | `Type` (z Array, Fixed, Struct, Named), wszystkie węzły `Expr`/`Stmt`, `FnDecl`, `StructDecl`, `Import`, `Link`, `Program` |
| `src/parser.h/.cpp` | 540 | Parser zstępujący `Parser`, `typeFromName()` |
| `src/cimport.h/.cpp` | 420 | Import przez libclang: funkcje, struktury C (pola, przesunięcia), stałe, zmienne globalne |
| `src/codegen.h` | 15 | `generate()` |
| `src/codegen_impl.h` | 265 | Klasa `Codegen` i jej struktury pomocnicze, wspólne dla czterech plików poniżej |
| `src/codegen.cpp` | 1090 | Program, deklaracje, zasięgi i sprzątanie, instrukcje, wyrażenia, miejsca (`ref`), operatory, analiza modyfikacji |
| `src/codegen_types.cpp` | 500 | Typy LLVM, rozwiązywanie typów, układ struktur, konwersje, własność (helpery kopiowania/zwalniania), wypisywanie, typy DWARF |
| `src/codegen_builtins.cpp` | 710 | Wywołania, konstruktory, funkcje wbudowane, konwersje, metody tablic i tekstów, deklaracje runtime'u, panic |
| `src/abi.cpp` | 310 | Klasyfikacja System V x86-64, wywołania C ze strukturami przez wartość, wrappery wywoływalne z C |
| `src/main.cpp` | 390 | Sterownik: CLI, ładowanie, maszyna docelowa, O2, emisja, cache runtime'u, linkowanie, porady przy błędach linkera, `run` |
| `runtime/finch_rt.c` | 440 | Biblioteka uruchomieniowa (§12) |
| `boot/*.fch` | 3080 | Kompilator samohostujący (§17) |

---

## 3. Lekser

`lex(file)` przechodzi raz przez `g_files[file].text` i zwraca `std::vector<Token>` zakończony `Tok::End`.

```cpp
struct Token { Tok kind; std::string text; int line, col; bool newlineBefore; };
```

- **Nowe linie nie są tokenami.** Każdy token zapamiętuje `newlineBefore`, a o znaczeniu decyduje
  parser (§4). Dzięki temu obsługa nowych linii nie zaśmieca każdej reguły listowej w gramatyce.
- **Pozycje:** od 1; `col` liczy znaki (bajty kontynuacji UTF-8 jej nie zwiększają), więc `^` trafia
  pod `"Błąd"`. Zabłąkany znak spoza ASCII jest pokazywany w całości z podpowiedzią, że nazwy są tylko ASCII.
- **Identyfikatory / słowa kluczowe:** `[A-Za-z_][A-Za-z0-9_]*`. Słowa kluczowe:
  `fn return if else while for in break continue true false null import link struct defer`.
  **Nazwy typów nie są słowami kluczowymi**: `int`, `u8`, `ptr`… to `Ident`, rozpoznawane przez parser
  za pomocą `typeFromName()`, więc `u8(x)` jest zwykłym wywołaniem, a lekser nie zależy od listy typów.
- **Liczby:** dziesiętne albo `0x` szesnastkowe z separatorami `_`. Bez ósemkowych (`010` to 10).
  `.` należy do liczby tylko wtedy, gdy po niej jest cyfra, więc `0..10` to `Int DotDot Int`.
- **Teksty / znaki:** sekwencje `\n \t \r \0 \\ \" \'`. Bez nowej linii w literale.
- **Operatory:** najdłuższe dopasowanie (`->` przed `-`, `<<`/`<=` przed `<`, `&&` przed `&`, `..` przed `.`,
  `:=` przed `:`). Samotny `:` to token (argumenty nazwane).
- **Komentarze:** `//` i niezagnieżdżane `/* */`.

---

## 4. Gramatyka i parser

`Parser` (`src/parser.cpp`) to ręcznie pisany parser zstępujący z jednym tokenem wyprzedzenia, a w
`atDeclaration()` z maksymalnie trzema.

```ebnf
program     = { import | link | struct | function } ;
import      = "import" ( STRING | IDENT ) ;                 (* "x.h" = nagłówek C, nazwa = moduł Fincha *)
link        = "link" STRING ;                               (* "glfw" | "plik.c" | "plik.o" | "lib.a" *)
struct      = "struct" IDENT "{" { type IDENT [ "=" expr ] NOWA_LINIA } "}" ;
function    = "fn" IDENT "(" [ param { "," param } ] ")" [ "->" type ] block ;
param       = type IDENT ;
type        = "[" "]" type | "ptr" [ "[" type "]" ] | TYPENAME | IDENT [ "." IDENT ] ;

block       = "{" { statement TERMINATOR } "}" ;
statement   = if | while | for | return | "break" | "continue" | "defer" statement | block
            | type IDENT [ "=" expr ]                        (* deklaracja, zobacz atDeclaration *)
            | IDENT ":=" expr
            | target assignop expr                           (* cel: zmienna, pole, element, p.value *)
            | call | method ;
for         = "for" IDENT "in" expr ( ".." expr block | block ) ;   (* zakres | po elementach *)
assignop    = "=" | "+=" | "-=" | "*=" | "/=" | "%=" ;

expr        = or ;            or = and { "||" and } ;       and = cmp { "&&" cmp } ;
cmp         = add { ( "==" | "!=" | "<" | "<=" | ">" | ">=" ) add } ;
add         = mul { ( "+" | "-" | "|" | "^" ) mul } ;
mul         = unary { ( "*" | "/" | "%" | "<<" | ">>" | "&" ) unary } ;
unary       = ( "-" | "!" | "~" ) unary | postfix ;
postfix     = primary { "." IDENT [ args ] | "[" expr "]" } ;
primary     = INT | FLOAT | STRING | CHAR | "true" | "false" | "null"
            | IDENT [ args ] | "(" expr ")" | "[" [ expr { "," expr } [","] ] "]" ;
args        = "(" [ arg { "," arg } [","] ] ")" ;  arg = [ IDENT ":" ] expr ;
```

**Koniec instrukcji.** Po instrukcji `endOfStatement()` wymaga `}`, końca pliku albo tokenu
z `newlineBefore`. Wewnątrz wyrażeń `sameLine()` (`parenDepth > 0 || !newlineBefore`) decyduje, czy
operator, `.`, `[` albo `(` wywołania kontynuuje wyrażenie. `parenDepth` rośnie w `( )` i `[ ]`,
a przy wejściu do `{ }` jest zapamiętywany i zerowany.

**Priorytety są jak w Go**, nie jak w C: `&` wiąże jak `*`, a `|`/`^` jak `+`, wszystkie powyżej porównań,
więc `x & MASKA == 0` znaczy `(x & MASKA) == 0`.

**Rozpoznawanie instrukcji.** `atDeclaration()` zwraca prawdę dla `[` `]` (typ tablicowy), `TYP IDENT`,
`ptr [` i `IDENT . IDENT IDENT` (typ z modułu). W pozostałych przypadkach parser **parsuje całe
wyrażenie, a potem patrzy na następny token**: operator przypisania robi z niego cel (musi być `Var`,
`Member` albo `Index`), a inaczej musi to być wywołanie funkcji lub metody, w przeciwnym razie
`this value is computed but never used`.

**Literały struktur to wywołania**: `Punkt(1, 2)` albo `Punkt(x: 1, y: 2)`. Literały w klamrach
(`Punkt{…}`) byłyby niejednoznaczne z blokami (`if gotowe {`); wywołanie nie potrzebuje specjalnej
gramatyki, a argumenty nazwane (`IDENT ":"` w `args()`) są akceptowane tylko przez konstruktory na etapie generowania kodu.

---

## 5. AST

Wszystkie węzły są w `src/ast.h`: małe hierarchie klas z jawnym `kind`, rozgałęziane przez
`switch` + `static_cast`.

| ExprKind | Węzeł | Pola |
|---|---|---|
| `Int` `Float` `Bool` `Char` `Str` | literały | `value` |
| `Null` | `NullExpr` | |
| `Var` | `VarExpr` | `name` |
| `Unary` | `UnaryExpr` | `op` (`- ! ~`), `operand` |
| `Binary` | `BinaryExpr` | `BinOp op`, `lhs`, `rhs` |
| `Call` | `CallExpr` | `callee` (nazwa), `args`, `argNames` |
| `Member` | `MemberExpr` | `obj`, `field` (`.x`, `.len`, `.ptr`, `.value`) |
| `Index` | `IndexExpr` | `obj`, `index` |
| `ArrayLit` | `ArrayLitExpr` | `elems` |
| `Method` | `MethodExpr` | `obj`, `name`, `args`, `argNames`; także `modul.fch(...)` |

| StmtKind | Węzeł |
|---|---|
| `Block` `VarDecl` `Assign` `Expr` `If` `While` `For` `ForEach` `Return` `Break` `Continue` `Defer` | jak w nazwie; `ForEachStmt` ma `var`, `list`, `body`; `DeferStmt` trzyma jedną instrukcję |

`math.sqrt(x)` parsuje się jako `MethodExpr` na `VarExpr("math")`; to, czy `math` jest modułem, czy zmienną,
rozstrzyga się przy generowaniu kodu. Wywołania nazywają funkcję, zamiast trzymać wyrażenie: Finch nie ma
funkcji jako wartości, a późne rozwiązywanie nazwy pozwala, by jeden `CallExpr` był funkcją wbudowaną,
konwersją (`u8(x)`), konstruktorem struktury, funkcją Fincha albo funkcją C.

Nie ma węzła „load”: to, czy `x` znaczy adres, czy wartość, zależy od kontekstu (`ref()` kontra `expr()`, §10.2).

### `Type`

```cpp
struct Type {
    enum Kind { Void, Bool, Char, I8, I16, I32, I64, U8, U16, U32, U64, F32, F64,
                Str, Ptr, Null, Array, Fixed, Struct, Named };
    Kind kind;
    std::shared_ptr<Type> elem;   // Ptr (null = `ptr` bez typu), Array, Fixed
    long long count;              // Fixed
    std::string name, module;     // Named (jak napisano) / Struct (rozwiązany)
    StructInfo *info;             // Struct
};
```

Parser tworzy `Named` dla nazw struktur; `Codegen::resolve()` zamienia je na `Struct` ze wskaźnikiem
`info`. `Fixed` (`[N]T`) pochodzi tylko z pól struktur C. `int` to `I64`, a `float` to `F64`.
Równość jest strukturalna; dwa `Struct` są równe, gdy mają to samo `info`.

---

## 6. Wczytywanie: pliki i moduły

`Loader::load(path, module, from)` w `main.cpp` czyta plik, dopisuje go do `g_files` (pozycje niosą
indeks pliku w `Pos::file`), leksuje go i parsuje, a potem wczytuje każdy jeszcze niewidziany `import nazwa`:
`nazwa.fch` obok importującego pliku albo w folderze z `FINCH_PATH`. Wynik to `std::vector<Program>`,
z głównym plikiem na początku. Cykle nie przeszkadzają: moduł jest oznaczany jako wczytany, zanim
zaczniemy podążać za jego importami.

Wszystkie pliki trafiają do **jednego modułu LLVM**. Każdy moduł Fincha ma `ModuleScope` (funkcje,
struktury, zbiór modułów, które importuje). Nazwy szuka się w bieżącym module; nazwy innego modułu
są dostępne tylko z kwalifikatorem (`geometry.length`, `geometry.Vec`) i tylko tam, gdzie moduł
zaimportowano. Funkcje są emitowane jako `finch.<moduł>.<nazwa>` (`finch.<nazwa>` w głównym pliku),
typy struktur jako `finch.<moduł>.<Nazwa>`.

---

## 7. Model semantyczny

### Kolejność deklaracji

`Codegen::run()` najpierw deklaruje **wszystkie struktury** wszystkich modułów i zaimportowane struktury C,
rozwiązuje typy pól i ustawia ciała typów LLVM (`declareStructs` → `resolveStruct`), potem deklaruje
**wszystkie sygnatury funkcji** (`declare`), a dopiero potem definiuje ciała. Kolejność w pliku i między
modułami nigdy więc nie ma znaczenia, a rekurencja (także wzajemna i między modułami) działa.

`resolveStruct` wykrywa cykle przez wartość trójstanową flagą (`struct A { A inner }` to błąd z radą,
by użyć `ptr[A]` albo `[]A`). Wskaźniki i tablice struktury nie potrzebują jej układu, więc
`struct Wezel { []Wezel dzieci; ptr[Wezel] rodzic }` jest w porządku: rozwiązują nazwę „płytko”
(`resolveT(…, deep=false)`).

### Zasięgi i zmienne

```cpp
struct Var     { Value *slot; FType type; bool readonly; std::string readonlyWhy;
                 bool owned; int order; bool moved; };
struct Cleanup { bool isDefer; std::string var; const Stmt *body; int order; };
struct Scope   { unordered_map<string, Var> vars; vector<Cleanup> cleanups; };
```

Zasięg jest otwierany dla parametrów funkcji, każdego `{ }`, każdego `for` (trzyma `i`), każdego `for … in`
(ukryta tymczasowa lista) i każdej jego iteracji (trzyma `x`).

- **Bez przesłaniania:** `addVar()` odrzuca nazwę znalezioną w *dowolnym* zewnętrznym zasięgu,
  funkcję modułu, nazwę zaimportowanego modułu i funkcję wbudowaną.
- **Tylko do odczytu:** zmienne pętli zakresowej i pętli po elementach (`readonlyWhy` daje konkretny komunikat).
- `order` numeruje deklaracje; `defer` używa go, by ukryć zmienne zadeklarowane po nim (§9.5).

### Rozwiązywanie nazw

`x` w pozycji wartości: zmienna lokalna → stała C → zmienna globalna C → (nazwa modułu: błąd z podpowiedzią) → błąd.
Wywołanie `n(...)`: funkcje wbudowane (`print addr input new free exit shell read_file write_file file_exists`) →
nazwy typów (konwersja) → funkcja bieżącego modułu → struktura bieżącego modułu / struktura C
(konstruktor) → funkcja C → błąd (z podpowiedzią, jeśli funkcja istnieje w innym module, albo z
`headerHint` dla znanych nazw z libc).

---

## 8. System typów

### Reprezentacja w LLVM

| Finch | LLVM |
|---|---|
| `bool` | `i1` |
| `char` `i8` `u8` / `i16` `u16` / `i32` `u32` / `int` `u64` | `i8` / `i16` / `i32` / `i64` |
| `f32` / `float` | `float` / `double` |
| `str`, `[]T` | `{ ptr, i64, i64 }` (wskaźnik, długość, pojemność) |
| `ptr`, `ptr[T]`, `null` | `ptr` (nieprzezroczysty) |
| `[N]T` | `[N x T]` |
| struktura Fincha | nazwany typ struct, naturalny układ |
| struktura C | nazwany **upakowany** (packed) struct z jawnymi bajtami wypełnienia (§13.2) |

Znakowość istnieje tylko w typie Fincha i wybiera `sdiv/udiv`, `ashr/lshr`, `icmp s*/u*`,
`sext/zext`, `sitofp/uitofp`, `fptosi/fptoui`.

### Wartości

```cpp
struct Value_ { Value *v; FType type; bool literal; bool fresh; };
```

- **`literal`**: liczba wpisana w kod albo zaimportowana stała C. Może stać się dowolnym typem całkowitym,
  w którym się mieści (`fits()`), albo `f32`. Przechodzi przez minus jednoargumentowy i arytmetykę samych
  literałów zwijaną do stałej; jawne konwersje ją kasują (`u8(250)` to stała z typem, więc `u8(250) + 10` zawija się do 4).
- **`fresh`**: wartość posiadająca pamięć (str/tablica/struktura z takimi polami), której nikt jeszcze nie
  trzyma: wynik wywołania, literał, sklejenie, konstruktor. Trzeba ją zapisać (przenieść) albo zwolnić (§9).

### Konwersje niejawne: `coerce(v, want)`

Identyczne typy; `null` → dowolny wskaźnik; wskaźnik z typem ↔ `ptr` bez typu; całkowita → całkowita,
jeśli wartość to mieszczący się literał albo typ się rozszerza (`widens()`: ta sama znakowość i więcej bitów
albo bez znaku → ściśle większa ze znakiem); całkowita → zmiennoprzecinkowa; `f32` → `f64`; literał `f64` → `f32`.
Nic więcej: błąd podpowiada jawną konwersję. Typy posiadające pamięć konwertują się tylko na siebie.
Literały tablic biorą typ elementu z miejsca, do którego trafiają (`exprWant()`), więc `[]f32 xs = [1, 2.5]` działa.

### Argumenty operatorów: `unify(l, r)`

Strona-literał dopasowuje się do drugiej; w przeciwnym razie rośnie strona, która się rozszerza;
`i32` + `u32` to błąd; całkowita + zmiennoprzecinkowa → zmiennoprzecinkowa; f32 + f64 → f64.
Nie ma promocji całkowitych z C: `u8 + u8` zostaje `u8`.

### Konwersje jawne: `convert()`

| Z → Na | IR |
|---|---|
| int/char ↔ int/char | `trunc`/`sext`/`zext` wg znakowości źródła |
| bool → int | `zext` |
| float ↔ int | `fptosi`/`fptoui`, `sitofp`/`uitofp` |
| float ↔ float | `fpext`/`fptrunc` |
| int → bool | `icmp ne 0` |
| liczba/bool/char → str | `finch_str_from_int/uint/float/char/bool` (fresh) |
| str → int/float | `finch_str_to_int/float` (panic przy złym tekście) |
| `[]u8`/`[]char` → str | `finch_str_from_bytes` |
| ptr → str | `finch_str_from_c` (pożyczony, `cap = -1`) |
| ptr ↔ ptr | nic |
| int → ptr / ptr → int | `inttoptr` / `ptrtoint` |

### Porównania

Wskaźniki/null: tylko `icmp eq/ne ptr`. `str`: `finch_str_eq` (długość + `memcmp`) dla `==`/`!=`,
`finch_str_cmp` dla porządku. Liczby zmiennoprzecinkowe: `oeq olt ole ogt oge` i `une` dla `!=`.
Całkowite i znaki wg znakowości (char bez znaku). Bool: tylko równość. Tablice/struktury: błąd.

---

## 9. Własność i pamięć

### 9.1 Model

Każda wartość **typu posiadającego pamięć** (`str`, `[]T` albo struktura Fincha z takim polem) ma
dokładnie jednego właściciela: zmienną, element tablicy, pole struktury albo komórkę na stercie z `new()`.
Właściciel zwalnia wartość, gdy sam znika. Nie ma liczenia referencji ani garbage collectora;
każda decyzja zapada statycznie.

| Sytuacja | Co się dzieje |
|---|---|
| `x := expr` / `T x = expr` / `x = expr` / pole, element, `push` | **`own(v)`**: wartość fresh jest przenoszona; pożyczona jest kopiowana w głąb |
| `x = …` (typ posiadający) | najpierw liczona jest nowa wartość, stara zwalniana po zapisie |
| koniec bloku, `return`, `break`, `continue` | posiadane zmienne każdego opuszczanego zasięgu są zwalniane (od najnowszej), uruchamiają się defery |
| `return lokalna` | zmienna jest **przenoszona na zewnątrz** (oznaczana jako `moved` na czas sprzątania tego returna) |
| przekazanie argumentu | **pożyczka**: funkcja dostaje płytką kopię i nigdy jej nie zwalnia; argumenty fresh zwalnia wywołujący po wywołaniu |
| parametr, który funkcja zmienia | kopiowany na wejściu i posiadany przez funkcję (decyduje `mutates()`, niżej) |
| wartość tymczasowa, której nikt nie trzyma (`print(a + b)`, `f(g())`, instrukcja-wyrażenie) | **`release(v)`**: zwalniana zaraz po użyciu |
| `a.pop()`, `a.remove(i)` | element jest przenoszony na zewnątrz (fresh) |
| `for x in lista` | `x` pożycza kolejne elementy; jeśli ciało może zmieniać `lista`, elementy są kopiowane |

Literał tekstowy to wartość fresh z `cap = 0`: przeniesienie nic nie kosztuje, zwalnianie jest pomijane
(`release()` ignoruje stałe), a zapis do niego (`s[i] = c`) najpierw robi kopię na stercie (`finch_str_own`).
Teksty z C są pożyczane z `cap = -1` i zawsze kopiowane, zanim zostaną zatrzymane.

### 9.2 Pożyczanie parametrów: `mutates()`

Przy deklarowaniu funkcji każdy parametr posiadający pamięć jest sprawdzany przez `mutates(nazwa, ciało)`:
przypisanie, którego zmienna-korzeń (`rootVar()`: przez `.pole` i `[i]`) jest tym parametrem, metoda
zmieniająca (`push pop insert remove clear resize sort reverse`) albo `addr(…)` na nim. Niezmieniane
parametry są pożyczane (`Fn::borrowParam`), zmieniane kopiowane raz na wejściu. Analiza jest składniowa
i zachowawcza: wszystko, co *może* zmienić parametr, powoduje kopię, co zawsze jest poprawne.
Ta sama analiza decyduje, czy `for x in lista` musi kopiować elementy.

### 9.3 Kopiowanie i zwalnianie

`dropAt(addr, T)` / `copyAt(dst, src, T)` działają na adresach:

- `str`: `finch_str_drop` (zwalnia, gdy `cap > 0`) / `finch_str_copy` (literały współdzieli, resztę duplikuje).
- `[]T` i struktury posiadające: generowane helpery `finch.drop.<klucz>` i `finch.copy.<klucz>`, tworzone
  raz na typ (`dropFn`, `copyFn`, klucz z `typeKey()`, np. `arr_S_main_Gracz`). Tablice zwalniają każdy posiadający
  element, a potem `finch_arr_free`; kopie wołają `finch_arr_clone_raw`, a potem kopiują w głąb posiadające
  elementy na surowe bity. Struktury zwalniają/kopiują tylko pola posiadające.

Helpery są emitowane we własnych funkcjach (`HelperScope` zapamiętuje pozycję buildera i wyłącza lokacje
debugowe), mogą wołać się rekurencyjnie (`struct Wezel { []Wezel dzieci }`) i są wstawiane (inline) przez O2.

### 9.4 Jak to wygląda

```c
fn greet(str name) -> str {
    msg := "Hi, " + name
    return msg
}

fn main() {
    a := ["x"]
    b := a
    b.push(greet("Ola"))
    print(b[1])
}
```

IR funkcji `main` z `-O0` (skrócony):

```llvm
  %3 = call ptr @finch_alloc(i64 24)                         ; ["x"]: jeden element na stercie
  store { ptr, i64, i64 } { ptr @str.1, i64 1, i64 0 }, ptr %4  ; literał, cap 0
  ...
  store { ptr, i64, i64 } %7, ptr %a                          ; przeniesione do a (fresh)
  %a1 = load { ptr, i64, i64 }, ptr %a
  store { ptr, i64, i64 } %a1, ptr %1
  call void @finch.copy.arr_str(ptr %2, ptr %1)               ; b := a  kopiuje
  %8 = load { ptr, i64, i64 }, ptr %2
  store { ptr, i64, i64 } %8, ptr %b
  %9 = call { ptr, i64, i64 } @finch.greet({ ptr, i64, i64 } { ptr @str.2, i64 3, i64 0 })
  ...
  call void @finch_arr_reserve(ptr %b, i64 %12, i64 24)       ; push: powiększ, jeśli trzeba
  store { ptr, i64, i64 } %9, ptr %15                         ; wynik greet przeniesiony, bez kopii
  ...
  %20 = icmp uge i64 1, %19                                   ; b[1]: kontrola zakresu
  br i1 %20, label %out.of.range, label %in.range
  ...
  call void @finch.drop.arr_str(ptr %b)                       ; koniec main: od najnowszej
  call void @finch.drop.arr_str(ptr %a)
  ret void
```

a `greet` zwraca `msg` bez kopiowania (`ret … %5` po wczytaniu slotu: zmienna została przeniesiona).

### 9.5 defer

`defer instrukcja` dokłada do bieżącego zasięgu `Cleanup{isDefer, body, order = bieżące varOrder}`
i niczego w tym miejscu nie emituje. Za każdym razem, gdy emitowane jest sprzątanie zasięgu (dojście do
końca bloku, `return`, wyjście przez `break`/`continue`), ciało jest generowane **w tym wyjściu**, z
`deferLimit = order`, żeby `lookup()` ukrył zmienne zadeklarowane po defer: te są wtedy już zwolnione
(sprzątanie idzie w odwrotnej kolejności). `return`/`break`/`continue` w instrukcji defer to błędy (`inDefer`).
Ciała defer są więc powielane na każdą ścieżkę wyjścia, przez co na zwykłej ścieżce nic nie kosztują.

### 9.6 Pamięć ręczna

`new(v)` alokuje `sizeof(T)` przez `finch_alloc`, zapisuje `own(v)` i zwraca `ptr[T]`. `free(p)` zwalnia
wskazywaną wartość, jeśli `T` posiada pamięć (pomijając null), i woła `free` z libc. Wskaźniki nigdy
niczego nie posiadają: `addr(x)` może wisieć po końcu bloku `x`; nie jest to sprawdzane.

### 9.7 Weryfikacja

`MEMCHECK=1 tests/run.sh` buduje każdy program testowy i uruchamia go pod valgrindem z
`--leak-check=full --errors-for-leak-kinds=all`: zero wycieków i zero nieprawidłowych dostępów we wszystkich,
w tym przy returnach z wnętrza pętli, `continue`/`break` z posiadanymi zmiennymi, deferach, modyfikacji
podczas iteracji, zagnieżdżonych tablicach struktur i przekazywaniu struktur C.

---

## 10. Generowanie kodu

### 10.1 Zmienne

Każda zmienna, łącznie z parametrami, dostaje `alloca` na początku bloku wejściowego (`slot()`;
struktury C dostają wyrównanie z C). `addVar()` zapisuje wartość początkową, rejestruje zmienną,
dokłada sprzątanie, jeśli zmienna posiada pamięć, i (z `-g`) zgłasza ją debuggerowi.

### 10.2 Miejsca: `ref()`, `place()`, `expr()`

```cpp
struct LRef { bool isPlace; Place pl; Value_ val; };   // adres albo (jeśli go nie ma) wartość
```

`ref(e, forWrite)` przechodzi łańcuch `Var`/`Member`/`Index` i zwraca **adres**, jeśli istnieje:

- `Var` → jej `alloca` (albo zmienna globalna C).
- `.pole` na miejscu-strukturze → `getelementptr` do pola (`llvmIndex`; struktury C pomijają elementy wypełnienia).
- przez wskaźnik (`p.value`, `p.pole`, `p[i]`) → wczytanie wskaźnika, kontrola null, adres celu.
  `p.value` na wskaźniku do struktury z własnym polem `value` oznacza to pole.
- `a[i]` na miejscu-tablicy/tekście → wczytanie `{ptr,len,cap}`, `boundsCheck(i, len)`, `getelementptr`.
  Przy `forWrite` na `str` najpierw `finch_str_own` (kopiowanie przy zapisie dla literałów). `[N]T` sprawdza względem `N`.
- `.len` / `.ptr` → wartości.
- wartość tymczasowa (wynik wywołania…) nie ma adresu: jej pole/element jest wyciągany, kopiowany
  (jeśli posiada pamięć), a sama wartość zwalniana.

`expr()` dla tych rodzajów woła `ref()` i wczytuje spod adresu: stąd bierze się każdy `load` zmiennej,
pola albo elementu. `place()` wymaga adresu (cele przypisań, `addr()`).

### 10.3 Przypisanie

`x = v`: **najpierw** liczone jest `v` (może przenieść pamięć, w której żyje cel: `a[0] = f()`, gdzie `f`
robi `push` do `a`), potem `place(cel, forWrite)`, `coerce`, `own`, wczytanie starej wartości, zapis i
zwolnienie starej. Złożone `x += v`: liczenie `v`, wczytanie celu, `arith()`, zapis, zwolnienie starej
wartości (dla `str +=`).

### 10.4 Funkcje i `main`

Funkcje są `internal` z naturalną sygnaturą LLVM (agregaty przez wartość). Parametry posiadające
pamięć są kopiowane na wejściu tylko wtedy, gdy `borrowParam` jest fałszywe. `fn main` to wewnętrzna
`finch.main`; osobna zewnętrzna `i32 main(i32, ptr)` (`defineMainWrapper`) buduje `[]str args` przez
`finch_args`, jeśli trzeba, woła ją, zwalnia argumenty i zwraca kod wyjścia. Funkcja, która dochodzi do
swojego końca, dostaje sprzątanie zasięgów i `ret void` albo błąd `can reach its end without returning T`.

### 10.5 Sterowanie przebiegiem

Bieżący blok buildera jest źródłem prawdy o osiągalności (`terminated()`): instrukcje po terminatorze to
błąd (`this code can never run`). `continueAt()` usuwa blok łączący, do którego nikt nie skacze.
`if`/`while`/`for` mają klasyczny układ bloków; `while true` bez `break` zostawia funkcję zakończoną
(nie trzeba po nim `return`). `for i in a..b` liczy oba końce raz; `continue` idzie do `for.step`.
`for x in lista` czyta długość listy z jej adresu **w każdej rundzie** (`push` w środku pętli jest bezpieczny)
i trzyma indeks w slocie na stosie. `break`/`continue` emitują sprzątanie zasięgów otwartych wewnątrz pętli
(`Loop::scopeDepth`) przed skokiem.

### 10.6 Skrócone `&&` / `||`

Jedyne ręcznie budowane φ: lewa strona kończy się w bloku `from`, prawa w `rhsEnd`;
`phi i1 [!isAnd, from], [rhs, rhsEnd]`.

### 10.7 Wywołania

`callFinch()` konwertuje argumenty na typy parametrów (literały tablic przez `exprWant`), przekazuje
posiadające płytko, woła, a potem zwalnia argumenty fresh. Wyniki typów posiadających są fresh.
`construct()` buduje strukturę od `zeroinitializer` przez `insertvalue`: pozycyjnie (wszystkie pola)
albo po nazwie (brakujące biorą wyrażenie domyślne pola, liczone w module struktury z podmienionymi
zasięgami wywołującego, albo zero). Wywołania C opisuje §13.

### 10.8 Wypisywanie

`print` emituje `printf` dla każdego argumentu z formatem wybranym wg typu (`%lld`, `%llu`, `%g`, `%c`,
`%.*s` dla `str` z jego długością, `%p`), oddzielając spacjami, a na końcu `\n`. Tablice, tablice o stałym
rozmiarze i struktury wołają generowane helpery `finch.print.<klucz>`, które wypisują `[1, 2]` /
`Punkt(x: 1, y: 2)`, biorąc teksty i znaki w środku w cudzysłów.

### 10.9 Metody tablic i tekstów

Tablice: `push` (`finch_arr_reserve` + zapis + len++), `pop`/`remove` (przeniesienie na zewnątrz,
`remove_gap`), `insert` (`insert_gap`), `clear`/`resize` (zwolnienie znikających elementów,
`finch_arr_resize`), `find`/`contains` (pętla z `compare(Eq)`), `slice` (nowa tablica, `copyAt` na element),
`reverse` (pętla zamian), `sort` (`qsort` z libc z generowaną porównywarką `finch.cmp.<klucz>`), `join` (runtime).
Metody zmieniające tablicę wymagają adresu (zmienna/pole/element).
Teksty: `sub find contains starts_with ends_with split trim upper lower replace repeat bytes`, wszystkie w runtime.

---

## 11. Kontrole w czasie działania

Wszystkie błędy w czasie działania idą przez `finch_panic(plik, linia, msg)` /
`finch_panic_index(plik, linia, i, len)` (zadeklarowane `noreturn cold`), więc gałąź błędu jest układana
jako zimna, a kontrola kosztuje porównanie i dobrze przewidywany skok. LLVM usuwa kontrole, które potrafi
udowodnić (stałe indeksy w zakresie, kontrole wyciągnięte z pętli).

| Kontrola | Gdzie |
|---|---|
| dzielenie/reszta przez zero (stałe zero: błąd kompilacji) | `checkDivisor()` |
| `MIN / -1` dla typów ze znakiem | `checkDivisor()` |
| indeks tablicy/tekstu/tablicy stałej poza zakresem (`icmp uge`, więc także ujemne) | `boundsCheck()` |
| `.value`, `.pole`, `[i]` przez null | `member()`, `index()` |
| `pop()` na pustej, `slice()`/`sub()` poza zakresem | metody, runtime |
| zły tekst w `int(s)`/`float(s)`, nieczytelny plik w `read_file` | runtime |

Sito na 20 milionów elementów z kontrolą zakresu przy każdym dostępie działa tak szybko jak to samo w C
(ok. 0,09 s): kontrole są wyciągane z pętli albo zwijane.

---

## 12. Runtime w C

`runtime/finch_rt.c` to zwykłe C bez stanu globalnego. CMake wbudowuje go w kompilator jako surowy
napis (`rt_source.inc`); przy pierwszym użyciu `runtimeObject()` kompiluje go przez `$CC -O2 -fPIC -c`
do `~/.cache/finch/rt-<hash>.o`, gdzie hash obejmuje źródło, wersję Fincha i kompilator C.
Jest linkowany statycznie do każdego programu.

Wspólny układ:

```c
typedef struct { char *ptr; int64_t len; int64_t cap; } FStr;   // ptr zawsze zakończony zerem
typedef struct { void *ptr; int64_t len; int64_t cap; } FArr;
// cap tekstu: > 0 sterta (posiadany), 0 literał (statyczny), -1 pożyczony z C (skopiuj, zanim zatrzymasz)
```

Funkcje przyjmują i zwracają te struktury **przez wskaźnik**, więc ich ABI jest trywialne (bez klasyfikacji
struktur): budowanie tekstów (`concat`, `from_int/uint/float/char/bool`, `sub`, `trim`, `upper`, `lower`,
`replace`, `split`, `join`, `repeat`, `from_bytes`), porównania (`eq`, `cmp`, `find`, `starts`, `ends`),
własność (`copy`, `own`, `drop`, `from_c`), tablice (`reserve`, `make`, `resize`, `clone_raw`, `free`,
`insert_gap`, `remove_gap`), `args`, `input`, pliki, `shell` i panic. Brak pamięci wypisuje
`out of memory` i kończy proces.

---

## 13. Współpraca z C: nagłówki, struktury, ABI

### 13.1 Import nagłówków

`importHeaders(imports, dirs)` parsuje każdy `import "x.h"` jako osobną jednostkę translacji z pliku
w pamięci `#include "x.h"`, z `-x c -std=gnu11`, `-I` dla folderu każdego pliku `.fch` i `-isystem` dla
każdego katalogu przeszukiwanego przez systemowy kompilator C (odczytanego z `$CC -E -v -x c /dev/null`,
dzięki czemu działa to na NixOS). `gnu11`, bo glibc w trybie ścisłego ISO ukrywa `M_PI` i podobne.

`Importer` odwiedza kursory najwyższego poziomu:

| Kursor | Wynik |
|---|---|
| `FunctionDecl` | `CFunc` (sygnatura mapowana w **kontekście sygnatury**; funkcje `static` i K&R zapisywane jako nieobsługiwane) |
| `TypedefDecl` rekordu | nazywa rekord (`typedef struct {…} CXCursor` → `CXCursor`) |
| `StructDecl` (definicja) | nazywa rekord jego tagiem |
| `VarDecl` `extern` | `CGlobal` (mapowany w **kontekście danych**) |
| `EnumConstantDecl`, liczbowe `MacroDefinition` | `CConst` (literał) |

`mapType()` działa na typach kanonicznych i mapuje liczby całkowite wg rozmiaru. Kontekst decyduje o
`char *`: w sygnaturach funkcji to `str` (Finch konwertuje przy wywołaniu), ale w strukturach, zmiennych
globalnych i za wskaźnikami to `ptr[char]` (tam układ pamięci musi zostać pojedynczym wskaźnikiem).
Rekordy przez wartość stają się `Named{nazwa, "C"}` i są definiowane przez `defineRecord()`: każde pole
(`clang_Type_visitFields`) z przesunięciem w bajtach, rozmiarem i wyrównaniem, zagnieżdżone rekordy,
tablice stałe (`Fixed`). Unie i pola bitowe zostają jako ukryte bajty i oznaczają strukturę jako
nieobsługiwaną przy wywołaniach przez wartość; wskaźniki na wszystko inne (void, funkcje, niekompletne
struktury) stają się `ptr`.

### 13.2 Układ struktur C

`resolveStruct()` buduje **upakowany** struct LLVM, który odwzorowuje C bajt w bajt: wypełnienie
`[n x i8]` przed każdym polem, którego przesunięcie wyprzedza bieżącą pozycję, samo pole (tylko jeśli jego
rozmiar w LLVM zgadza się z C) i wypełnienie końcowe do `sizeof` z C. Dostęp do pól używa zapisanego
`llvmIndex`. Upakowanie czyni układ dokładnym niezależnie od reguł wyrównania LLVM; alokacje i wartości
tymczasowe dostają wyrównanie z C jawnie.

### 13.3 Konwencja wywołań System V x86-64

`classify(T)` (w `abi.cpp`) spłaszcza strukturę do skalarnych liści z przesunięciami w bajtach (przez
zagnieżdżone struktury i tablice stałe) i:

- **MEMORY**, jeśli jest większa niż 16 bajtów albo ma niewyrównany liść: przekazywana jako wskaźnik z
  `byval(T) align ≥8`, zwracana przez ukryty pierwszy wskaźnik `sret(T)`.
- w przeciwnym razie każdy eightbyte jest **INTEGER**, jeśli którykolwiek liść w nim jest liczbą
  całkowitą/wskaźnikiem, a inaczej **SSE**. Części: INTEGER → `iN` dla pozostałych bajtów (`i32` dla
  4-bajtowego `Color`), SSE → `double`, `<2 x float>` albo `float`. Jedna część idzie wprost; dwie jako
  dwa argumenty albo zwracane jako `{a, b}`.

`makePlan()` przechodzi parametry, licząc 6 rejestrów całkowitych i 8 SSE; struktura, której części już się
nie mieszczą, idzie w całości na stos (MEMORY), tak jak robi to clang. Małe parametry całkowite dostają
`signext`/`zeroext`. `emitCCall()` zapisuje każdy argument-strukturę do slotu wystarczająco dużego dla
struktury i jej postaci rejestrowej, wczytuje części z przesunięć 0 i 8, woła z zaplanowanym typem funkcji
i atrybutami, a wyniki-struktury odbudowuje w ten sam sposób.

`cThunk(fn)` to odwrotność dla `addr(fn)`: wewnętrzna funkcja z sygnaturą C, która składa parametry-struktury
z rejestrów albo pamięci `byval`, woła funkcję Fincha i obniża jej wynik. Akceptowane są tylko sygnatury zgodne z C.

`tests/run/c_structs.fch` sprawdza `{float,float}`, `{float,float,float}`, `{u8×4}`, `{double,int}`,
`{i64,i64}`, strukturę 24-bajtową, strukturę z `char[8]`, zagnieżdżone struktury, wyczerpanie rejestrów
przy 13 argumentach-strukturach oraz wywołania zwrotne przyjmujące i zwracające struktury, na bibliotece C
kompilowanej przez `link "abi.c"`. Wyniki zgadzają się z tymi samymi wywołaniami zrobionymi z C.

### 13.4 Linkowanie kodu C

`link "nazwa"` próbuje `pkg-config --libs nazwa`, a potem `-lnazwa`. `link "plik.c"` kompiluje plik
(względem pliku `.fch`) przez `$CC -O2 -fPIC -c` do tymczasowego obiektu; pliki `.o` i `.a` przechodzą
bez zmian. Gdy linkowanie się nie uda, `linkFailed()` mapuje niezdefiniowane symbole na nagłówek, który
je zadeklarował, i wypisuje linijkę `link` do dopisania.

---

## 14. Optymalizacja, emisja, linkowanie

- **Cel:** `sys::getDefaultTargetTriple()`, CPU `generic`, `Reloc::PIC_`. Data layout jest ustawiany na
  module, zanim powstanie jakikolwiek IR (od niego zależy wyrównanie loadów i store'ów).
- **Optymalizacja:** `PassBuilder::buildPerModuleDefaultPipeline(O2)`: środkowa część `-O2` z clanga.
  `-O0` ją pomija (`finch ir plik.fch -O0` pokazuje surowe wyjście `Codegen`).
- **Emisja:** stary `PassManager` + `addPassesToEmitFile(ObjectFile)`.
- **Linkowanie:** `$CC prog.o rt.o [obiekty użytkownika] -lm [biblioteki]`. Na Niksie wrapper cc zamienia
  ścieżki `-L` na `RPATH`, więc programy działają bez `LD_LIBRARY_PATH`.
- **`run`:** tymczasowy plik wykonywalny uruchamiany z resztą argumentów i potem usuwany; sygnał jest
  zgłaszany jako `the program crashed: <nazwa>` z kodem wyjścia `128 + n`.

Uwagi o wydajności: funkcje użytkownika są `internal`, więc LLVM swobodnie je wstawia i specjalizuje
(rekurencyjne `fib(40)`: ok. 0,19 s wobec ok. 0,23 s dla `clang -O2` na tym samym kodzie C na maszynie
autora); kod tablicowy z kontrolą zakresu dorównuje C na sicie.

---

## 15. Informacje dla debuggera

Z `-g` `Codegen` tworzy `DIBuilder`, jednostkę kompilacji (`DW_LANG_C`, więc składnia wyrażeń C w gdb
działa dla `print p.x`), `DIFile` na plik źródłowy, `DISubprogram` na funkcję, zmienne-parametry
(`createParameterVariable`, więc `bt` pokazuje `length (v=…)`), zmienne lokalne (`insertDeclare` na ich
`alloca`) i `DILocation` na każdą instrukcję i wywołanie (`setLoc`). Typy: podstawowe z nazwami Fincha
(`int`, `u8`, `float`), `str`/tablice jako struktury `{ptr, len, cap}`, wskaźniki, tablice stałe i struktury
z przesunięciami pól z `StructLayout` LLVM. Struktury rekurencyjne używają zastępowalnej deklaracji
wyprzedzającej. Generowane helpery nie mają lokacji. Najlepiej działa `-g -O0`; przy O2 część zmiennych jest usuwana.

---

## 16. Diagnostyka

`failAt(plik, linia, kol, msg)` wypisuje `ścieżka:linia:kol: error: msg` i linijkę źródła ze znakiem `^`
(tabulatory zachowane, kolumny liczone w znakach), a potem kończy proces z kodem 1. Konwencje: operatory
binarne raportują operator, wywołania nazwę funkcji, błędy argumentów pozycję argumentu. Komunikaty mówią,
co jest źle, a gdy naprawa jest oczywista, także jak to naprawić. Ostrzeżeń nie ma. Błąd weryfikatora LLVM
wypisuje `internal compiler error (please report)` i kończy proces z kodem 2.

---

## 17. Kompilator samohostujący (`boot/`)

`boot/` to drugi kompilator Fincha napisany w Finchu (ok. 3100 linii):

| Plik | Zawartość |
|---|---|
| `boot/lexer.fch` | lekser (tokeny to `{kind, text, line, col, nl}`; rodzaje to słowa, a słowa kluczowe i operatory są swoim własnym rodzajem) |
| `boot/ast.fch` | `Type` (`kind`, `elem` jako tablica 0/1 elementów, `name`, `module`), jeden ogólny `Node` na wszystko, `Program` |
| `boot/parser.fch` | ta sama gramatyka i reguły nowych linii co parser C++, wspinanie się po priorytetach dla operatorów binarnych |
| `boot/gen.fch` | sprawdzanie typów + generator **tekstowego LLVM IR**: zasięgi, miejsca, konwersje, kopie, helpery, wypisywanie, kontrole, metody tablic i tekstów |
| `boot/main.fch` | ładowanie modułów i sterownik: zapisuje `.ll`, uruchamia `clang -O2 plik.ll runtime/finch_rt.c` |

Obsługuje rdzeń języka: `int float bool char str`, `[]T`, struktury (wartości domyślne, konstruktory po
nazwie i po kolei), `ptr[T]`/`addr`/`new`/`free`/`null`, wszystkie instrukcje poza `defer`, moduły oraz
funkcje wbudowane i metody kompilatora C++, bez importu C, liczb z rozmiarem i `defer`. Ma tę samą
semantykę wartości i analizę pożyczania (kopiuje tam, gdzie główny kompilator), ale **nigdy nie zwalnia**:
jako kompilator bootstrapowy zostawia pamięć systemowi operacyjnemu.

Stan kompilatora to jedna struktura `Gen` przekazywana jako `ptr[Gen]`: Finch nie ma zmiennych globalnych,
a przekazanie wskaźnika to sposób, w jaki funkcja zmienia wartość wywołującego. Bez `defer` i typu map zasięgi
to tablice tablic przeszukiwane liniowo; funkcje pomocnicze (kopiowanie/wypisywanie/porównywanie na typ)
są generowane przez tymczasową podmianę buforów wyjścia (`beginHelper`/`endHelper`).

**Punkt stały.** `tests/boot.sh`:

1. `build/finch` (C++) kompiluje `boot/` → **stage 1**,
2. stage 1 kompiluje `boot/` → **stage 2** (i jego IR),
3. stage 2 kompiluje `boot/` → IR **stage 3**,

i wymaga, żeby IR ze stage 2 i stage 3 był **identyczny co do bajtu** (ok. 32 000 linii). Potem kompiluje
kompilatorem samohostującym każdy test z `tests/run`, który mieści się w podzbiorze języka, i porównuje
wynik z oczekiwanym. Kompilacja samego siebie zajmuje kompilatorowi samohostującemu ok. 2,3 s.

---

## 18. System budowania

- `find_package(LLVM CONFIG)`; **jeśli istnieje współdzielony cel `LLVM`, Finch linkuje tę jedną bibliotekę.**
  `libclang.so` sam linkuje `libLLVM.so`; linkowanie dodatkowo statycznych bibliotek komponentów wprowadzało
  do procesu dwie kopie LLVM, a zduplikowane obiekty globalne wywracały program przy wyjściu (podwójne
  zwolnienie w globalnym destruktorze). Do tego binarka miała 82 MB zamiast ok. 20 MB.
- libclang: `find_path(clang-c/Index.h)` + `find_library(clang)`.
- `runtime/finch_rt.c` jest wczytywany przy konfiguracji do `build/rt_source.inc` jako surowy literał
  napisowy; `CMAKE_CONFIGURE_DEPENDS` ponawia konfigurację, gdy plik się zmieni.
- `FINCH_VERSION` z `project(VERSION 2.3.0)`. C++17, `-Wall -Wextra` (MSVC: `/W3`), bez ostrzeżeń.
- `shell.nix` wymienia `llvmPackages.clang` przed `llvmPackages.libclang`: ten drugi dostarcza też
  „gołego” `clang`, który nie widzi nagłówków systemowych.

---

## 19. Testy

- `tests/run.sh`: każdy `tests/run/*.fch` z `fn main` musi wypisać dokładnie swój `.out` (stdin z `.in`, jeśli
  jest); pliki bez `main` to moduły albo pliki pomocnicze. Każdy `tests/fail/*.fch` musi się nie udać z tekstem
  z linijki `// expect:`. Obecnie **59 passed, 0 failed**.
- `MEMCHECK=1 tests/run.sh`: to samo plus valgrind na każdym programie (bez wycieków i złych dostępów).
- `tests/boot.sh`: punkt stały samohostowania i przebieg na podzbiorze (§17).

---

## 20. Semantyka a C

| Sytuacja | C | Finch 2.0 |
|---|---|---|
| Przepełnienie liczby ze znakiem | UB | zdefiniowane zawinięcie (bez `nsw`) |
| Dzielenie całkowite przez zero, `INT_MIN / -1` | UB | błąd kompilacji, jeśli stałe, inaczej błąd w czasie działania |
| Indeks tablicy poza zakresem | UB | błąd w czasie działania (także dla pól `[N]T` struktur C) |
| Dereferencja null | UB | błąd w czasie działania dla `.value`, `.pole`, `[i]` |
| Użycie po zwolnieniu / podwójne zwolnienie tablic, tekstów, struktur | częste błędy | niemożliwe: tych nie zwalnia się ręcznie |
| Wyciek tablic, tekstów, struktur | częsty błąd | niemożliwy bez `new` |
| `x & 1 == 0` | `x & (1 == 0)` | `(x & 1) == 0` |
| Niejawne zwężanie, mieszanie ze znakiem / bez znaku | po cichu | błędy kompilacji |
| Niezainicjalizowane zmienne | UB | niemożliwe (zero albo wartości domyślne) |
| Brak return | UB, jeśli użyty | błąd kompilacji |
| Kolejność obliczeń | w większości nieokreślona | od lewej do prawej; w przypisaniu najpierw prawa strona |
| Przesunięcie ≥ szerokość, float→int poza zakresem | UB | nadal poison (jeszcze niesprawdzane) |
| Wiszący wskaźnik z `addr()` / `new` + `free` | UB | nadal UB |

---

## 21. Spis funkcji

### Sterownik (`main.cpp`)

| Funkcja | Robi |
|---|---|
| `Loader::load` / `findModule` | czyta, leksuje i parsuje plik oraz rekurencyjnie jego moduły / szuka `nazwa.fch` |
| `hostMachine`, `optimize`, `emitObject` | maszyna docelowa, potok O2, plik obiektowy |
| `runtimeObject` | kompiluje runtime i trzyma go w cache |
| `capture`, `libFlags`, `link` | uruchamia polecenie / pkg-config albo `-l` / linkowanie, z kompilacją plików z `link "x.c"` |
| `linkFailed`, `guessLib` | zamieniają wyjście linkera na porady |

### Lekser i parser

| Funkcja | Robi | Różni się od |
|---|---|---|
| `lex` / `tokName` | tekst → tokeny / nazwy do komunikatów | |
| `accept` / `expect` / `unexpected` | konsumuje, jeśli pasuje / konsumuje albo zgłasza błąd / błąd z kontekstem | |
| `sameLine` / `endOfStatement` | czy ten token może kontynuować wyrażenie? / czy instrukcja się skończyła? | |
| `atDeclaration` / `type` | podgląd deklaracji / parsowanie typu | |
| `args` | `( [nazwa:] expr, … )` dla wywołań, metod i konstruktorów | |
| `postfix` | łańcuchy `.pole`, `.metoda(…)`, `[i]` | `primary` parsuje początek łańcucha |

### Codegen: program i instrukcje (`codegen.cpp`)

| Funkcja | Robi | Różni się od |
|---|---|---|
| `declareStructs` / `declareFns` / `declare` | wszystkie typy / wszystkie sygnatury / jedna sygnatura + analiza pożyczania | |
| `define` / `defineMainWrapper` | jedno ciało / punkt wejścia C | |
| `pushScope` / `popScope` | otwiera / zamyka zasięg (`popScope` emituje jego sprzątanie, jeśli osiągalne) | |
| `emitCleanups(downTo)` / `emitScopeCleanups(i)` | sprzątanie kilku zasięgów bez zamykania (return/break) / jednego zasięgu | |
| `slot` / `tmpOf` / `tmp` | `alloca` zmiennej / anonimowa / anonimowa z wartością | |
| `lookup` / `addVar` | szuka widocznej zmiennej (respektuje `deferLimit`) / deklaruje zmienną | |
| `stmt`, `varDecl`, `assign`, `ifStmt`, `whileStmt`, `forStmt`, `forEachStmt`, `returnStmt`, `jump` | instrukcje | |
| `rootVar` / `mutates` / `mutatesExpr` | zmienna, od której zaczyna się l-wartość / czy instrukcja albo wyrażenie ją zmienia? | |

### Codegen: wyrażenia

| Funkcja | Robi | Różni się od |
|---|---|---|
| `expr` / `exprWant` | r-wartość / to samo z oczekiwanym typem dla literałów tablic | |
| `ref` / `place` | adres, jeśli jest, inaczej wartość / adres albo błąd | `expr` wczytuje z `ref` |
| `member` / `index` | `.x`, `.len`, `.ptr`, `.value`, auto-dereferencja / `[i]` z kontrolą zakresu i kopiowaniem przy zapisie | |
| `arrayLit` | `[…]` → tablica na stercie | |
| `unary`, `binary`, `arith`/`arithOp`, `compare`, `logic` | operatory (`arith` dokłada flagę literału) | |
| `unify` / `coerce` / `convert` | dwa argumenty do siebie / jedna wartość do miejsca / jawne `T(x)` | |

### Codegen: typy i własność (`codegen_types.cpp`)

| Funkcja | Robi |
|---|---|
| `ty`, `resolve`/`resolveT`, `findStruct`, `resolveStruct` | typ LLVM; Named → Struct (głęboko albo płytko); szukanie struktury; układ + kontrola cykli |
| `owning`, `zero`, `defaultValue` | czy typ posiada pamięć; stała zerowa; zero albo wartości domyślne struktury |
| `own` / `release` | coś do zapisania (przeniesienie albo kopia) / zwolnienie niezatrzymanej wartości fresh |
| `copyValue`, `dropValue`, `copyAt`, `dropAt`, `copyFn`, `dropFn`, `typeKey` | kopiowanie i zwalnianie przez wartość, przez adres i helpery na typ |
| `forN` | wewnętrzna pętla `for i in 0..n` używana przez helpery |
| `strConst`, `cstr`, `strFromC`, `elemSize` | stała-literał str; str → `char*`; `char*` → pożyczony str; rozmiar elementu |
| `emitPrint`, `printFn`, `printf_` | wypisywanie |
| `diType`, `setLoc` | informacje dla debuggera |

### Codegen: wywołania (`codegen_builtins.cpp`) i ABI (`abi.cpp`)

| Funkcja | Robi |
|---|---|
| `call` / `method` | rozwiązuje wywołanie / metodę albo wywołanie z modułu |
| `callFinch` / `construct` / `callC` | funkcja Fincha / konstruktor struktury / funkcja C |
| `print`, `addrOf`, `convert`, `arrayMethod` | funkcje wbudowane |
| `rt` / `libc` | deklaruje funkcję runtime'u / libc |
| `fileName`, `panicIf`, `boundsCheck`, `checkDivisor` | kontrole w czasie działania |
| `classify`, `makePlan`, `declareC`, `emitCCall`, `cThunk` | klasyfikacja System V, plan rejestrów, deklaracja, wywołanie, wrapper dla callbacków |

---

## 22. Rozbudowa kompilatora

**Funkcja wbudowana:** dodaj nazwę do `isBuiltin()`, obsłuż ją w `call()`, zaimplementuj (w IR albo jako
funkcję runtime'u zadeklarowaną w tabeli `rt()`), dodaj przypadki w `tests/run/` i `tests/fail/`. Jeśli ma
istnieć też w kompilatorze samohostującym, dodaj ją do `call()` i `runtimeDecls()` w `boot/gen.fch`.

**Metoda tablicy albo tekstu:** dodaj gałąź w `arrayMethod()` albo w części `str` w `method()`; jeśli
zmienia tablicę, dopisz jej nazwę do zbiorów `changing` w `method()` i `mutatesExpr()`, żeby pożyczanie
dalej było poprawne.

**Operator:** token leksera + `tokName`, `BinOp`, właściwa funkcja priorytetu w parserze, `opName`, emisja
w `arithOp`/`compare` i wyjaśnienie w `badOperands`.

**Instrukcja:** `StmtKind` + węzeł, `Parser::statement`, `Codegen::stmt`; jeśli przenosi sterowanie,
respektuj `terminated()`, używaj `continueAt()` dla bloków łączących i emituj sprzątanie (`emitCleanups`)
przed wyskokiem z zasięgów.

**Nowy typ posiadający pamięć:** rozszerz `owning()`, `dropAt`/`copyAt` (i ich helpery), `emitPrint`, `diType`.

---

## 23. Platformy i Windows

`src/target.h` trzyma `g_target`: triple LLVM, flagi `windows` / `msvc` / `cross`, kompilator C (`cc`)
i rozszerzenie plików wykonywalnych. `setTarget()` wybiera kompilator C: `FINCH_CC`, potem `CC` (nie przy
kompilacji krzyżowej), potem `x86_64-w64-mingw32-gcc` dla `--target windows` z Linuksa, `clang` na
Windowsie, `cc` na Uniksie. Przez tę warstwę idzie wszystko, co zależy od platformy:

- **Nagłówki:** libclang parsuje z `--target=<triple>` (więc `long` ma 32 bity na Windowsie i używane są
  właściwe nagłówki), a na Windowsie z `-D_USE_MATH_DEFINES`. Katalogi nagłówków pochodzą z `<cc> -E -v`.
- **Procesy:** `capture()` / `runCommand()` opakowują `popen` / `system` (`_popen` na Windowsie) i dekodują
  kody wyjścia (POSIX `WIFEXITED`/sygnały albo kody Windowsa i wyjątki NTSTATUS, np. `0xC0000005`).
  `shellQuote()` cytuje dla `sh` (`'…'`) albo `cmd.exe` (`"…"`); linia polecenia dla `cmd /c`, która
  zaczyna się od cudzysłowu, dostaje dodatkową parę cudzysłowów.
- **Linkowanie:** bez `-lm` przy MSVC; nazwy `.exe`; klucz cache runtime'u obejmuje triple i kompilator C.
  Komunikaty linkera MSVC (`LNK2019 unresolved external symbol`, `LNK1104`/`LNK1181 cannot open file`) są
  tłumaczone tak jak komunikaty GNU ld.
- **Uruchamianie:** program dla Windowsa zbudowany na Linuksie uruchamia się przez `wine`.
- **Informacje dla debuggera:** CodeView dla celów MSVC, DWARF w pozostałych.
- **Runtime:** na Windowsie nie ma `getline` ani `sys/wait.h`; `stdin`/`stdout`/`stderr` są tam makrami,
  więc `finch_std_stream(i)` udostępnia je Finchowi, gdy nagłówek nie ma dla nich zmiennej `extern`.
- **Wypisywanie wskaźników** jest wszędzie takie samo: `null` albo `0x…`.

### Konwencja wywołań Microsoft x64

`classify()` rozgałęzia się po `g_target.windows`. Struktura o rozmiarze dokładnie 1, 2, 4 albo 8 bajtów
idzie jako **jedna liczba całkowita** tego rozmiaru, nawet jeśli zawiera floaty (`{float, float}` → `i64`).
Każda inna struktura jest **pośrednia (Indirect)**: wywołujący kopiuje ją na swój stos i przekazuje adres
kopii (bez `byval`: funkcja dostaje zwykły wskaźnik). Wyniki 1/2/4/8-bajtowe wracają w `rax` jako liczba;
wszystko inne przez ukryty wskaźnik `sret`. Każdy argument zajmuje jedno miejsce, więc nie ma liczenia
rejestrów. `cThunk()` robi to samo w drugą stronę dla callbacków.

`tests/windows.sh` buduje każdy test z `--target windows`, uruchamia go w Wine i porównuje wynik: przechodzą
wszystkie, łącznie z `c_structs` (który wtedy sprawdza reguły Microsoftu na bibliotece C zbudowanej MinGW).
CI dodatkowo buduje `finch.exe` przez MSVC na Windows Server i uruchamia tam `tests/run.sh`.

### Budowanie finch.exe

CI używa Visual Studio 2022, Ninja i oficjalnego pakietu `clang+llvm-21.x-x86_64-pc-windows-msvc`
(biblioteki statyczne, `/MT`, więc `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`). Dwa szczegóły Windowsa
w `CMakeLists.txt`: ten pakiet podaje ścieżkę do biblioteki DIA SDK z maszyny, na której zbudowano LLVM,
więc jest ona przekierowywana na zainstalowane Visual Studio; a osadzony runtime jest zapisywany jako
kilka surowych napisów, bo MSVC ogranicza jeden literał do 16 KB. LLVM jest inicjalizowany tylko z
natywnym backendem, żeby budowanie statyczne nie wciągało wszystkich architektur.

---

## 24. Serwer języka

`finch lsp` (`src/lsp.cpp`) mówi protokołem LSP przez stdin/stdout, używając `llvm::json`. Przy
`didOpen`/`didChange` uruchamia na dokumencie prawdziwy front-end: `Loader` (z tekstem otwartych
dokumentów jako nadpisaniem plików), `importHeaders` (w cache na zestaw importów, bo czytanie nagłówków
jest najwolniejsze) i `generate()` bez optymalizacji i emisji. Dwa przełączniki pozwalają użyć
kompilatora wewnątrz serwera:

- `g_throwErrors`: `failAt()` rzuca `FinchError{file, line, col, msg}` zamiast wypisywać i kończyć proces.
  Serwer publikuje to jako diagnostykę (dla właściwego pliku, także wewnątrz importowanego modułu) i czyści
  diagnostyki, które zniknęły.
- `g_index`: podczas generowania kodu `Codegen::note()` zapisuje `SymRef` dla każdej rozwiązywanej nazwy
  (zmienne, parametry, pola, funkcje, konstruktory, funkcje i stałe C, funkcje wbudowane, metody, elementy
  modułów): jej zakres, opis w markdownie i pozycję definicji. `define()` i `declareStructs()` dokładają
  `FnInfo` / `StructIndex`; `addVar()` dokłada `VarInfo` z zakresem linii otaczającej funkcji. Pozycje nazw
  pochodzą z pól `namePos`, które zapisuje parser.

Zapytania korzystają z tego indeksu:

| Zapytanie | Źródło |
|---|---|
| hover, definition | `SymRef` pod kursorem |
| completion | słowa kluczowe, typy, funkcje wbudowane, zmienne otaczającej funkcji, funkcje, struktury, importowane moduły, nazwy z C pasujące do wpisanego początku; po `nazwa.`: pola struktury (także przez `ptr[T]`), metody tablic/tekstów albo zawartość modułu |
| signatureHelp | cofa się do niezamkniętego `(`, liczy przecinki na najwyższym poziomie i szuka sygnatury Fincha / C / wbudowanej |
| documentSymbol | funkcje i struktury (z polami) z pliku |

Kod w trakcie pisania (`p.`, `add(1, `) się nie kompiluje, więc serwer trzyma **ostatnią udaną** analizę
każdego dokumentu i dołącza z niej funkcje, struktury i zmienne do podpowiedzi. `tests/lsp_test.py`
steruje serwerem tak jak edytor i sprawdza każdą funkcję.

`src/builtins_doc.h` to jedyny opis funkcji wbudowanych i metod, używany przy hover, podpowiedziach
i podpowiedziach parametrów (i odwzorowany w ściągach).

---

## 25. Rozszerzenie VS Code

`editors/vscode` to zwykłe rozszerzenie w JavaScripcie: `extension.js` uruchamia `finch lsp` przez
`vscode-languageclient` (ścieżka z ustawienia `finch.path`) i dodaje **Run** / **Build** jako zadania
`ProcessExecution` (bez cytowania dla powłoki; matcher `$finch` zamienia linie `plik:linia:kol: error:`
w wpisy w Problems). `syntaxes/finch.tmLanguage.json` to gramatyka TextMate, testowana przez `npm test`
z `vscode-textmate` + `vscode-oniguruma` (tym samym silnikiem, którego używa VS Code);
`language-configuration.json` daje komentarze, nawiasy, automatyczne domykanie i wcięcia; `snippets/`
szablony. CI pakuje je przez `vsce` do `finch-lang.vsix`.

## 26. Znane ograniczenia

- Brak metod w strukturach, typów generycznych, map, `match`, domknięć i błędów jako wartości (`int("x")`, `read_file` robią panic).
- Teksty to bajty: `.len`, `s[i]`, `upper()` nie znają Unicode.
- Wiszące wskaźniki (`addr` zmiennej, która zniknęła, użycie po `free`) nie są wykrywane.
- Wielkość przesunięć i konwersje float→int nie są sprawdzane (poison w LLVM, jak w C).
- C: unie i struktury z polami bitowymi przez wartość, makra-funkcje, `long double`.
- Dwa systemy: Linux i Windows, oba x86-64 (ABI System V i Microsoft x64). Jeszcze bez macOS i ARM.
- Kompilator samohostujący nie ma importu C, liczb z rozmiarem ani `defer` i nie zwalnia pamięci.
