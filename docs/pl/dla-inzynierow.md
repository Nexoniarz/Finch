# Finch dla inżynierów

**Wnętrze Fincha 2.5, którego kompilator jest napisany w Finchu.** Jak kod źródłowy staje się tokenami, drzewem AST i
LLVM IR *jako tekstem*, który `clang` kompiluje i linkuje z małym runtime'em w C; jak nagłówki C czyta libclang wywoływany
z samego Fincha i jak ręcznie odtwarzane są konwencje wywołań C; reguły typów i własności; jak kompilator buduje sam siebie
z ziarna; oraz serwer języka.

Zakładam, że czytasz Fincha (zobacz [przewodnik dla techników](dla-technikow.md)) i widziałeś już LLVM IR.
Pierwsza wersja kompilatora, w C++ na API C++ LLVM, jest w gałęzi `before-bootstrap`; ten przewodnik opisuje wersję w Finchu.

> 🇬🇧 English version: [for-engineers.md](../en/for-engineers.md)

---

## Spis treści

1. [Architektura](#1-architektura)
2. [Drzewo źródeł](#2-drzewo-źródeł)
3. [Front end: lekser, parser, moduły](#3-front-end-lekser-parser-moduły)
4. [Generowanie kodu](#4-generowanie-kodu)
5. [Import C i ABI C](#5-import-c-i-abi-c)
6. [Sterownik, linkowanie i cele](#6-sterownik-linkowanie-i-cele)
7. [System typów](#7-system-typów)
8. [Własność i pamięć](#8-własność-i-pamięć)
9. [Generyki, enumy i match](#9-generyki-enumy-i-match)
10. [Bootstrap](#10-bootstrap)
11. [Serwer języka](#11-serwer-języka)
12. [Semantyka a C](#12-semantyka-a-c)
13. [Znane ograniczenia](#13-znane-ograniczenia)

---

## 1. Architektura

W kompilatorze nie ma biblioteki LLVM. Pisze LLVM IR jako **tekst**, a optymalizację, generowanie kodu i linkowanie zostawia
`clangowi`. Dzięki temu jest mały (ok. 8 tys. linii) i może być napisany w samym Finchu.

```
 main.fch ─► loader ── lex ── parse ──► Program   (+ każdy importowany moduł, rekurencyjnie)
                                          │
        `import "x.h"` ─► cimport.fch (libclang wołany z Fincha) ─► funkcje, struktury, stałe C
                                          │
                                          ▼
                       gen.fch ─► generate(): struktury, sygnatury, ciała funkcji
                                  kontrola typów i IR w jednym przejściu; klasyfikacja ABI wywołań C
                                          │   (tekst IR)
                                          ▼
        linker.fch ─► clang -c prog.ll  ─►  clang prog.o rt-<hash>.o [twoje .c/.o/.a] -lm [pkg-config / -l]
                                          │
                                          ▼
                        plik wykonywalny   (finch run: uruchamiany z pliku tymczasowego, potem usuwany)
```

Decyzje projektowe:

- **Szybko się przerywa.** Pierwszy błąd wypisuje `plik:linia:kol: error: …` z linią źródła i strzałką, i kompilator kończy.
  Bez odzyskiwania po błędach, więc kolejne fazy nie widzą półpoprawnych danych.
- **Jedno przejście.** `gen.fch` sprawdza typy i emituje IR jednym przejściem po AST. Wyrażenie daje operand IR, typ Fincha
  (napis: `int`, `f32`, `str`, `array`, `map`, `struct`, `enum`, `named`…) i flagi.
- **Niech clang pracuje.** Zmienne lokalne to `alloca`, zamieniane przez `mem2reg`. Optymalizacja to `-O2` clanga (albo `-O0` z `-g`).
- **Wartości, nie referencje.** Tablice, teksty i struktury mają semantykę wartości; własność ustala się w czasie kompilacji (§8).
- **Malutki runtime w C** (`runtime/finch_rt.c`, ok. 760 linii) robi to, co w IR jest uciążliwe: teksty, rośnięcie tablic, mapy,
  wejście, pliki, paniki. Kompilowany raz i trzymany w `~/.cache/finch`.

---

## 2. Drzewo źródeł

| Plik | Linie | Zadanie |
|---|---|---|
| `src/lexer.fch` | 470 | Tokeny i pozycje, tekst `"""`, `"\(x)"` zamienione na tokeny `("…" + str(x) + "…")` |
| `src/ast.fch` | 120 | Jedna struktura `Node` z napisem `kind`, dziećmi, nazwami typów, pozycjami (`line`, `endLine`) |
| `src/parser.fch` | 910 | Zejście rekurencyjne: deklaracje, instrukcje, wyrażenia, generyki (`[T]`), `enum`, `match`, `export`, `link`, `const`, `test "…" { }` |
| `src/gen.fch` | 6450 | Kontrola typów + IR: struktury, funkcje, metody, zakresy i zwolnienia, instrukcje, wyrażenia, wbudowane, mapy, błędy jako wartości, generyki (monomorfizacja), enumy i `match`, metadane `-g`, tryb `index` |
| `src/cimport.fch` | 790 | libclang: funkcje, struktury i unie (pola, offsety, wypełnienie), enumy, stałe, globalne, makra (także będące strukturą) |
| `src/platform.fch` | 90 | `Target`: triple, układ danych, kompilator C, znak `char` zależnie od systemu |
| `src/linker.fch` | 430 | Pamięć podręczna (runtime, obiekt programu, pliki C/C++), biblioteki (`link`, pkg-config), podpowiedzi przy błędach linkera |
| `src/main.fch` | 515 | Sterownik: CLI, loader z wyszukiwaniem modułów i nakładkami, `build/run/test/fmt/ir/index/lsp` |
| `src/manifest.fch` | 240 | `finch.toml`: mały czytnik TOML, listy zależne od celu |
| `src/fmt.fch` | 225 | `finch fmt` i formatowanie w LSP |
| `src/lsp.fch` | 910 | Serwer języka (§11) |
| `src/docs.fch` | 110 | Jeden opis wbudowanych funkcji dla podpowiedzi i uzupełniania |
| `std/*.fch` | 1040 | `json`, `path`, `time` (+`_windows`), `proc` (+`_windows`), `fs`, `thread` |
| `runtime/finch_rt.c` | 1150 | Runtime w C: tekst, tablice, mapy, pliki, wypisywanie liczb, fs, wątki, uruchamianie testów, łańcuch wywołań |
| `bootstrap/` | | `build.sh`, `install.sh`, ziarna (§10) |

---

## 3. Front end: lekser, parser, moduły

**Lekser.** Koniec linii kończy instrukcję; wcięcia nic nie znaczą. Tokeny niosą plik, linię i kolumnę, z których korzysta każdy błąd.

**Parser.** Zejście rekurencyjne, jedna funkcja na konstrukcję. Typy są parsowane do napisu (`[]str`, `map[str]int`,
`ptr[char]`, `Stack[int]`); `fn Typ.metoda` deklaruje metodę; `fn nazwa[T]` i `struct Nazwa[T]` to szablony; `enum` i `match`
mają własne węzły; `export fn` oznacza funkcję widoczną dla C; blok zapamiętuje `endLine` (dla serwera języka). Stara
kolejność `fn nazwa(...) typ` to błąd składni z podpowiedzią.

**Moduły.** `import nazwa` szuka kolejno `nazwa_<os>.fch` obok importującego pliku (`windows`, `linux`, `macos`), `nazwa.fch`,
katalogów z `FINCH_PATH`, potem biblioteki standardowej `std/`. Każdy moduł jest parsowany raz; jego nazwy są używane jako
`modul.nazwa`. `import "x.h"` trafia do importera C.

Moduł rozpoznaje się po **pliku**: kluczem jest jego ścieżka od folderu głównego pliku (albo od folderu z `FINCH_PATH`
lub `std/`) bez `.fch` i bez końcówki `_<system>`, np. `ui/przycisk`. `Program.imports` trzyma nazwę używaną w danym
pliku (`przycisk`, albo `p` przy `import ui/przycisk as p`), a `Program.modules` klucz; `modFor()` zamienia jedno na drugie.
Symbole używają klucza z `/` zamienionym na `$` (`@finch.ui$przycisk.rysuj`).

**Stałe.** `const NAZWA = wyrażenie` to węzeł najwyższego poziomu; `constValue()` liczy wyrażenie przy każdym użyciu, w
module, w którym je zdefiniowano, bez zmiennych lokalnych, więc wychodzi literał (operatory na literałach całkowitych
składa `arith`). `checkConst()` dopuszcza tylko literały, nazwy, operatory, pola i konwersje liczb; `constBusy` łapie pętle.

**Dodatki leksera.** `1e9` / `2.5e-3` to tokeny float (tekst dostaje `.` dla LLVM); teksty `"""` tracą wcięcie
linii zamykającej w `longString()`.

---

## 4. Generowanie kodu

`gen.fch` to jedna duża struktura `Gen` i funkcje na niej. Przejścia:

1. `declareStructs`: każda struktura Fincha i C dostaje typ LLVM (struktury C z prawdziwym wypełnieniem); rekurencja przez wartość jest odrzucana.
2. `declareFns`: sygnatury, z *analizą pożyczania* parametrów (parametr, który nigdzie nie jest zapisywany, jest przekazywany bez kopii).
3. `define`: każde ciało, sprawdzone i wyemitowane. Zakresy pamiętają, co zwolnić na końcu (`defer` działa pierwszy).
4. `defineMainWrapper`: C-owe `main(argc, argv)` i `finch_on_fatal` dla osadzających.
5. Z `-g` metadane DWARF dokładane są w trakcie pisania IR (używaj `-O0`).

Generyki są **monomorfizowane**: wywołanie `max(1, 2)` wnioskuje `T`, potem AST szablonu generowane jest ponownie z
przypisanym `T`, pod zmangowaną nazwą (`max$int`). `match` na enumie jest wyczerpujący (brak wariantu to błąd, `else` łapie
resztę); na wartości to ciąg porównań. Wartości enuma to `i32`; `Color(n)` jest sprawdzane; wypisywanie używa wygenerowanej tabeli nazw.

Błędy jako wartości: `-> T!` zwraca parę (wartość, tekst błędu); `try` wraca wcześniej przy błędzie, `or` go obsługuje, `error("…")` go tworzy.

---

## 5. Import C i ABI C

`cimport.fch` woła **libclang** wprost z Fincha (sam jest napisany z `import "clang-c/Index.h"`), parsuje nagłówek z flagami
celu i przechodzi po kursorach. Typy: parametr `const char *` → `str`, inny `char *` → `ptr[char]`, wynik `char *` → `str`;
liczby całkowite według rozmiaru i znaku (`char` jest bez znaku na ARM64 Linux); struktury i enumy według układu.

LLVM IR nie realizuje za ciebie konwencji wywołań C, więc `gen.fch` ją odtwarza:

- `classify`: System V x86-64: struktura do 16 bajtów idzie w rejestrach według klas ośmiobajtów (INTEGER/SSE), większa w pamięci;
- `classifyArm64`: AAPCS64, z jednorodnymi agregatami zmiennoprzecinkowymi (HFA);
- reguły Microsoft x64 (struktury 1, 2, 4, 8 bajtów w rejestrze, reszta przez wskaźnik);
- `makePlan` zamienia klasyfikację w plan wywołania; `cThunk` robi z `addr(fn)` coś, co C może wywołać (callbacki).

**Makra.** `#define` będący jedną liczbą albo tekstem czyta się z tokenów. Taki, którego tokeny mogą być wyrażeniem
stałym (bez klamer, średników i słów instrukcji), liczy clang: drugie parsowanie nagłówka dodaje
`static const __auto_type __finch_m_NAZWA = (NAZWA);` dla każdego, a `clang_Cursor_Evaluate` daje liczbę całkowitą,
double albo tekst. Te, które nie są stałymi, dają tylko błędy w tym drugim parsowaniu i są pomijane.

**Unie** to `CStruct` z `isUnion`: typ LLVM to `{ [n x iA] }` (rozmiar i wyrównanie od clanga), każde pole ma
przesunięcie 0, a pole czyta się przez pamięć. Nie da się ich przekazać do C przez wartość (`unsupported` mówi dlaczego).

Ten sam kod pozwala kompilatorowi wołać libclang, którego API przekazuje `CXCursor`, `CXType` i `CXString` przez wartość.

---

## 6. Sterownik, linkowanie i cele

`finch build|run|ir|index plik.fch [-o wyjście] [-O0] [-g] [-l biblioteka] [-I katalog] [--target cel] [--shared] [--overlay ścieżka=plik]`.
`ir` wypisuje IR; `index` wypisuje rekordy dla serwera języka (§11); `--shared` robi bibliotekę współdzieloną z `export fn`.

`--target windows|linux|arm64|macos|android|<triple>` zmienia triple i układ danych, znak `char`, reguły ABI powyżej, wariant OS
modułów (`nazwa_windows.fch`) i kompilator C: `FINCH_CC` albo `clang --target=…`. Przy linkowaniu `pkg-config` podaje flagi
dla `link "biblioteka"`; gdy linker zawiedzie, wynik jest analizowany i podpowiadane jest brakujące `link "…"`. Obiekt
runtime'u jest kluczowany skrótem jego źródła i celu.

Zmienne środowiskowe: `FINCH_PATH` (katalogi modułów), `FINCH_RT` (źródło runtime'u), `FINCH_CC` (kompilator/linker C; IR
zawsze kompiluje `clang`, nigdy `CC`), `FINCH_LIBCLANG` (dla `bootstrap/build.sh`).

Kompilator znajduje `runtime/` i `std/` obok siebie (`../`, `./`) albo w `../share/finch/` (`bootstrap/install.sh`).

**Pamięć podręczna budowania** (`host.cacheDir()`: `$XDG_CACHE_HOME/finch`, `~/.cache/finch`, `%LOCALAPPDATA%\finch`):

- `ir/<skrót pliku głównego + celu + flag>.o` z plikiem `.key`, w którym jest skrót clanga, flag i całego IR. Jedno miejsce
  na program, więc cache nie rośnie z każdą zmianą; to samo IR daje ten sam obiekt bez uruchamiania clanga.
- `c/<skrót źródła + kompilatora + flag + celu>.o` dla `link "x.c"` / `.cpp`, budowane z `-MMD`; `.key` to skrót każdego
  pliku z listy `.d` (źródła i jego nagłówków). Zmiana któregokolwiek przebudowuje obiekt.
- Obiekty są zapisywane do pliku `.part<pid>` i przemianowywane, więc dwa budowania naraz nie widzą pół pliku.
  Skróty to dwa niezależne 32-bitowe (FNV-1a i djb2), razem 64 bity.

**`finch.toml`** (`src/manifest.fch`) jest szukany od folderu pliku głównego (albo bieżącego) w górę. Trafia do
`values["sekcja.klucz"]` i `lists["sekcja.klucz"]`; `list(klucz, cele)` skleja listę z najwyższego poziomu i te z
`[target.<os>]` oraz `[target.<os>-<cpu>]`. Ścieżki liczą się od folderu manifestu. Wpisy trafiają tam, gdzie opcje
z linii poleceń: `paths` → loader, `include` → `-I` (libclang i pliki C), `links` → `link`, `libdirs` i `ldflags` →
polecenie linkowania, `cflags` → pliki C/C++.

**Pliki C++**: `.cpp`/`.cc`/`.cxx` w `links` kompiluje `cxxFor(cc)` (`FINCH_CXX` albo `clang` → `clang++`,
`…gcc` → `…g++`, `cc` → `c++`), a potem ten sam sterownik linkuje, co dokłada standardową bibliotekę C++.

**Android**: `--target android` to `aarch64-linux-android24`; kompilator C to clang z NDK,
`toolchains/llvm/prebuilt/<host>/bin/aarch64-linux-android24-clang` (z `ANDROID_NDK_HOME`/`ANDROID_NDK_ROOT`), linkowanie
dodaje `-llog` i nie dodaje `-lpthread` (bionic ma go w libc). Wariant OS modułów to `linux`.

**`finch test`** kompiluje jak `run`, z `Options.test`: wszystkie funkcje są dalej definiowane, ale `@main` wywołuje
`finch_test_run(nazwa, fn)` dla każdego bloku `test` z modułów użytkownika (`__test_<n>`, funkcja `void` zrobiona przez
parser), a na końcu `finch_test_summary()`. Runtime uruchamia każdy test pod `setjmp`; `fatal()` (każdy błąd w czasie
działania i nieudany `assert`) wraca przez `longjmp` z `fatal_hook`, więc jeden nieudany test nie kończy całości.
Pamięć, którą test miał w tym momencie, nie jest zwalniana.

**`finch fmt`** działa na liniach: śledzi `{ }`, `( )`/`[ ]`, teksty (z `\( )`), `'…'`, `"""` i `/* */`. Linia, która
zaczyna się na głębokości nawiasów 0 i nie ciągnie linii zakończonej operatorem albo `,`, dostaje wcięcie 4 × głębokość
klamer; pozostałe (ciągi dalsze, wnętrza `"""`) przesuwa się o tyle samo co linię, która zaczęła instrukcję, więc ich
kształt (i ucinanie wcięcia `"""`) zostaje.
---

## 7. System typów

### Reprezentacja w LLVM

| Finch | LLVM |
|---|---|
| `bool` | `i1` |
| `char` `i8` `u8` / `i16` `u16` / `i32` `u32` / `int` `u64` | `i8` / `i16` / `i32` / `i64` |
| `f32` / `float` | `float` / `double` |
| `str`, `[]T` | `{ ptr, i64, i64 }` (wskaźnik, długość, pojemność) |
| `map[K]V` | `{ ptr, i64, i64, i64, ptr, i64 }` (entries, len, used, cap, index, icap; zob. src/gen.fch) |
| `ptr`, `ptr[T]`, `null` | `ptr` (nieprzezroczysty) |
| `[N]T` | `[N x T]` |
| struktura Fincha | nazwany typ struct, naturalny układ |
| `self` metody | `ptr` (pierwszy parametr) |
| wynik `-> T!` | `{ i1 ok, T value, str error }` (`i8` w miejscu `T` dla `-> !`) |
| struktura C | nazwany **upakowany** (packed) struct z jawnymi bajtami wypełnienia (§5) |

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
  trzyma: wynik wywołania, literał, sklejenie, konstruktor. Trzeba ją zapisać (przenieść) albo zwolnić (§8).

### Konwersje niejawne: `coerce(v, want)`

Identyczne typy; `null` → dowolny wskaźnik; wskaźnik z typem ↔ `ptr` bez typu; całkowita → całkowita,
jeśli wartość to mieszczący się literał albo typ się rozszerza (`widens()`: ta sama znakowość i więcej bitów
albo bez znaku → ściśle większa ze znakiem); całkowita → zmiennoprzecinkowa; `f32` → `f64`; literał `f64` → `f32`.
Nic więcej: błąd podpowiada jawną konwersję. Typy posiadające pamięć konwertują się tylko na siebie.
Literały tablic biorą typ elementu z miejsca, do którego trafiają (`exprWant()`), więc `[]f32 xs = [1, 2.5]` działa.

### Argumenty operatorów: `unify(l, r)`

Strona-literał dopasowuje się do drugiej (także literał float obok `f32`: `x * 2.0` zostaje `f32`); w przeciwnym razie
rośnie strona, która się rozszerza; `i32` + `u32` to błąd; całkowita + zmiennoprzecinkowa → zmiennoprzecinkowa;
f32 + f64 → f64. Nie ma promocji całkowitych z C: `u8 + u8` zostaje `u8`. Dwa literały całkowite składają się w literał,
więc `FLAG_A | FLAG_B` dalej mieści się w `u64` albo `u32`.

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

---

## 8. Własność i pamięć

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
| wywołanie metody `v.m()` | `self` **to jest** `v` (przekazywany jest jego adres): bez kopii; zmiany widzi wywołujący |
| mapy | są właścicielami kluczy i wartości tak jak tablice elementów; `m[k] = v` przenosi/kopiuje `v` i zwalnia starą wartość |

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

Metody biorą w tym udział: `declareFns()` najpierw ustala, które metody zmieniają `self`
(`mutates("self", ciało)`), powtarzając, aż nic się nie zmieni, bo wywołanie zmieniającej metody na `self`
też zmienia `self`. Ich nazwy trafiają do `mutatingMethods`, które `mutatesExpr()` traktuje jak `push`:
`p.przesun(1)` zmienia `p`. Dopiero potem liczone jest `borrowParam` dla każdej funkcji i metody.

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

---

## 9. Generyki, enumy i match

Jak są generowane, opisuje §4. Reguły widoczne dla użytkownika: sekcje 23 i 24 [przewodnika dla techników](dla-technikow.md).
Testy: `tests/new/generics.fch`, `tests/new/enums.fch` i odmowy w `tests/new/fail/`.

---

## 10. Bootstrap

Kompilator budowany jest z **ziarna**: własnego IR zapisanego jako `bootstrap/<system>.ll` (zobacz
[bootstrap/README.md](../../bootstrap/README.md)). `bootstrap/build.sh`:

```
ziarno .ll ──clang──► finch0 ──buduje src/──► finch1 ──buduje src/──► build/finch
```

i przerywa, jeśli `finch1` i `finch` nie dają identycznego IR (punkt stały: kompilator kompiluje siebie tak samo, niezależnie
od tego, kto go zbudował). `bootstrap/build.sh seed` przepisuje ziarno z bieżącego kompilatora; rób to, gdy źródło
kompilatora zacznie używać czegoś, czego zatwierdzone ziarno nie ma.

Ziarno jest na system, bo zapieczono w nim konwencję wywołań C dla wywołań libclang (§5). Linux x86-64 i Linux ARM64 mają
ziarna. Dla macOS i Windowsa trzeba by ziaren zrobionych z nagłówkami Apple i Microsoftu.

---

## 11. Serwer języka

`finch lsp` mówi JSON-RPC przez stdin/stdout (ramki `Content-Length`, `std/json`). Sam nie sprawdza typów: przy każdej zmianie
uruchamia `finch index plik.fch --overlay ścieżka=kopia …`, gdzie nakładki to niezapisany tekst edytora. W trybie `index`
kompilator wypisuje rekordy rozdzielone tabulatorami:

| Rekord | Znaczenie |
|---|---|
| `@R plik linia kol dł plikDef liniaDef kolDef podpowiedź` | odwołanie: co pokazuje hover i gdzie jest definicja |
| `@V nazwa typ plik linia kol plikFn liniaFn koniecFn` | zmienna i funkcja, w której żyje |
| `@F moduł nazwa odb plik linia kol koniec sygnatura parametry` | funkcja lub metoda |
| `@S` / `@SF`, `@E`, `@I` | struktury (i pola), enumy, importy |

Pierwszy błąd wypisywany jest jako `ścieżka:linia:kol: error: komunikat` i staje się diagnostyką. Gdy kod się nie kompiluje,
serwer podpowiada dalej z ostatniej wersji, która się skompilowała. Test akceptacyjny: `tests/lsp_test.py` (22 sprawdzenia).

---

## 12. Semantyka a C

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

---

### Łańcuch wywołań przy błędach w czasie działania

Z `Options.trace` (domyślnie dla `run`, `test`, `-g`, `-O0`; `--trace`/`--no-trace`; nigdy z `--shared`) każda funkcja
Fincha ma ramkę na swoim stosie:

```llvm
%frame = alloca { ptr, ptr, ptr, i64 }          ; ramka wywołującego, nazwa, plik, linia
store ptr (load @finch_trace), ...pole 0
store ptr @"nazwa", ...pole 1 ; ptr @"plik", pole 2 ; i64 <linia>, pole 3
store ptr %frame, ptr @finch_trace
```

`@finch_trace` to globalna `thread_local` w module programu (model initial-exec, chyba że biblioteka albo Windows);
każda instrukcja zapisuje swoją linię do pola 3. Po każdym wywołaniu funkcji Fincha wywołujący wpisuje z powrotem swoją
ramkę do `@finch_trace`, co sprząta też ramki funkcji, która wróciła wcześniej, więc wiele ścieżek `return` nie
potrzebuje epilogu. Opakowania wołane z C (thunki `addr(fn)`, `export fn`) zapamiętują i przywracają `@finch_trace`.
`main` woła `finch_trace_init(top, set)` z dwiema małymi funkcjami do czytania i zapisu, i tak runtime (`print_trace()`
w `fatal()` oraz uruchamianie testów) dostaje się do zmiennej wątku, której nie zna z nazwy. Koszt: ok. 3 razy na
`fib(40)`, który jest samymi wywołaniami; dlatego zwykły `finch build` go pomija.

### Wątki i operacje atomowe

`std/thread.fch` to cienka warstwa nad runtime'em (`finch_thread_spawn/join`, `finch_mutex_*`: pthreads albo
`CreateThread` i `CRITICAL_SECTION` na Windowsie). Funkcja wątku to dowolna `fn f(ptr)` podana przez `addr(f)`, co robi
thunk wołany z C. `atomic_add/sub/swap` to `atomicrmw add/sub/xchg … seq_cst`, `atomic_load/store` to
`load/store atomic … seq_cst, align <rozmiar>`, a `atomic_cas` to `cmpxchg … seq_cst seq_cst` zwracające `i1`. Biorą
`ptr[T]` do dowolnej liczby całkowitej. Tablice, teksty i mapy Fincha nie są synchronizowane; dzielenie ich między
wątkami wymaga mutexu.

### Struktura jako argument przez adres

`argWant()` (tylko argumenty wywołań, nie przypisania): gdy parametr to `ptr[S]`, a argument to zwykła zmienna typu `S`,
argumentem jest adres zmiennej, jakby napisano `addr(x)`. Tylko zmienne, więc nic nie jest liczone dwa razy, a na
wartości tymczasowe nic nie wskazuje.

## 13. Znane ograniczenia

- Kompilator działa tylko na Linuksie x86-64 i ARM64 (brak ziarna dla macOS i Windowsa). Programy można budować na wszystkie trzy.
- Wynik dla macOS jest sprawdzany tylko jako IR; Windows i ARM64 uruchamiane są w CI pod Wine i qemu.
- Jeden błąd na uruchomienie, bez odzyskiwania.
- Informacje `-g` najlepiej z `-O0`.
- Sterownik używa narzędzi `sh` (`rm`, `mkdir`), więc wymaga powłoki POSIX.
- Generyki są monomorfizowane (kod rośnie z liczbą instancji); nie ma ograniczeń na typy, błędne użycie to błąd wewnątrz
  wygenerowanego ciała.
- Pamięć nieudanego testu nie jest zwalniana (uruchamianie testów wychodzi z niego przez `longjmp`).
- `--target android` jest sprawdzany do pliku obiektowego; linkowanie wymaga NDK i nie było uruchamiane na urządzeniu.

