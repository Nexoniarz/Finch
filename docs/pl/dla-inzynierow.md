# Finch dla inżynierów

**Jak od środka działa Finch 2.6, którego kompilator jest napisany w Finchu.** Jak kod źródłowy zamienia się w tokeny,
drzewo AST i LLVM IR *w postaci tekstu*, który `clang` kompiluje i linkuje z małym runtime'em w C; jak nagłówki C są
czytane przez libclang z poziomu samego Fincha i jak ręcznie odtworzono konwencje wywołań C; reguły typów i własności
wartości; jak kompilator buduje sam siebie z pliku startowego; i jak działa serwer języka.

Zakładam, że umiesz czytać kod w Finchu (zob. [przewodnik dla techników](dla-technikow.md)) i że LLVM IR nie jest ci obcy.

> 🇬🇧 English version: [for-engineers.md](../en/for-engineers.md)

---

## Spis treści

1. [Architektura](#1-architektura)
2. [Pliki źródłowe](#2-pliki-źródłowe)
3. [Część frontowa: lekser, parser, moduły](#3-część-frontowa-lekser-parser-moduły)
4. [Generowanie kodu](#4-generowanie-kodu)
5. [Import z C i ABI języka C](#5-import-z-c-i-abi-języka-c)
6. [Sterownik, linkowanie i systemy docelowe](#6-sterownik-linkowanie-i-systemy-docelowe)
7. [System typów](#7-system-typów)
8. [Własność i pamięć](#8-własność-i-pamięć)
9. [Typy generyczne, enumy i match](#9-typy-generyczne-enumy-i-match)
10. [Bootstrap](#10-bootstrap)
11. [Serwer języka](#11-serwer-języka)
12. [Semantyka w porównaniu z C](#12-semantyka-w-porównaniu-z-c)
13. [Znane ograniczenia](#13-znane-ograniczenia)

---

## 1. Architektura

W kompilatorze nie ma biblioteki LLVM. Zapisuje on LLVM IR jako **tekst**, a optymalizację, generowanie kodu maszynowego
i linkowanie zostawia `clangowi`. Dzięki temu jest mały (ok. 12 tys. linii) i dało się go napisać w samym Finchu.

```
 main.fch ─► loader ── lex ── parse ──► Program   (+ każdy importowany moduł, rekurencyjnie)
                                          │
        `import "x.h"` ─► cimport.fch (libclang wołany z Fincha) ─► funkcje, struktury i stałe C
                                          │
                                          ▼
                       gen.fch ─► generate(): deklaracje struktur i funkcji, potem ciała funkcji;
                                  sprawdzanie typów i emisja IR w jednym przejściu; klasyfikacja ABI dla wywołań C
                                          │   (tekst IR)
                                          ▼
        linker.fch ─► clang -c prog.ll  ─►  clang prog.o rt-<hash>.o [twoje .c/.o/.a] -lm [pkg-config / -l]
                                          │
                                          ▼
                        plik wykonywalny  (finch run: uruchamiany z pliku tymczasowego, potem usuwany)
```

Decyzje, od których zależy cała reszta:

- **Pierwszy błąd kończy pracę.** Kompilator wypisuje `plik:linia:kolumna: error: …` z linią źródła i strzałką,
  po czym się zatrzymuje. Nie próbuje kontynuować po błędzie, więc dalsze etapy nigdy nie widzą częściowo poprawnego wejścia.
- **Jedno przejście.** `gen.fch` sprawdza typy i emituje IR w jednym przejściu po AST. Każde wyrażenie daje operand IR,
  swój typ Fincha (rodzaj zapisany tekstem: `int`, `f32`, `str`, `array`, `map`, `struct`, `enum`, `vec`, `named`…) i flagi.
- **Resztę robi clang.** Zmienne lokalne to `alloca`, które `mem2reg` zamienia na rejestry. Optymalizuje `-O2` clanga
  (albo `-O0` przy `-g`).
- **Wartości, nie referencje.** Tablice, teksty i struktury zachowują się jak wartości; o własności decyduje kompilator (§8).
- **Mały runtime w C** (`runtime/finch_rt.c`, ok. 1150 linii) robi to, co w IR byłoby żmudne: teksty, powiększanie tablic,
  mapy, wczytywanie, pliki, błędy w trakcie działania. Kompiluje się go raz i trzyma w `~/.cache/finch`.

---

## 2. Pliki źródłowe

| Plik | Linie | Za co odpowiada |
|---|---|---|
| `src/cimport.fch` (część C++) | | Nagłówek, który nie jest C, jest czytany jako C++ (`looksLikeC`, `translateCpp`): nakładki `extern "C"` na jego funkcje, metody, konstruktory i destruktory (po jednej na liczbę argumentów, gdy są domyślne) i opisujący je nagłówek C trafiają do katalogu podręcznego; nagłówek C jest importowany jak zwykle, nakładki są linkowane (`-ffunction-sections` + `--gc-sections` / `-dead_strip` usuwają nieużywane). `CImports.cppNames` mapuje `ns.f`, `Klasa#metoda`, `Klasa.new` na nakładki; `cppCall()` w gen.fch wybiera jedną po liczbie argumentów i typach literałów i woła ją przez `callC` |
| `src/diag.fch` | 260 | Jak wyglądają błędy i ostrzeżenia: linia kodu, zakres `^` (z UTF-8 i tabulatorami), `help:`/`note:`, kolory tylko w terminalu, linia stanu po budowaniu. `finch index` zostawia jedną zwykłą linię na błąd dla serwera języka |
| `src/lexer.fch` | 470 | Tokeny, pozycje, tekst `"""`, zamiana `"\(x)"` na tokeny `("…" + str(x) + "…")` |
| `src/ast.fch` | 170 | Jedna struktura `Node` z rodzajem (`kind`) jako tekstem, dziećmi, nazwami typów i pozycją (`line`, `endLine`); rozpoznawanie typów wektorowych |
| `src/parser.fch` | 910 | Zejście rekurencyjne: deklaracje, instrukcje, wyrażenia, typy generyczne (`[T]`), `enum`, `match`, `export`, `link`, `const`, `test "…" { }` |
| `src/gen.fch` | 7030 | Sprawdzanie typów i IR: struktury, funkcje, metody, zasięgi i zwalnianie, instrukcje, wyrażenia, funkcje wbudowane, mapy, błędy jako wartości, typy generyczne (monomorfizacja), enumy i `match`, wektory (SIMD), metadane debuggera (`-g`), tryb `index` |
| `src/cimport.fch` | 790 | libclang: funkcje, struktury i unie (pola, przesunięcia, wyrównanie), enumy, stałe, zmienne globalne, makra (także o wartości struktury) |
| `src/platform.fch` | 120 | `Target`: triple, układ danych, kompilator C, znak typu `char` na danym systemie |
| `src/linker.fch` | 480 | Pamięć podręczna (runtime, obiekt programu, pliki C/C++), biblioteki (`link`, pkg-config), podpowiedzi przy błędach linkera |
| `src/main.fch` | 530 | Sterownik: wiersz poleceń, wczytywanie modułów i nakładek, `build/run/test/fmt/ir/index/lsp` |
| `src/manifest.fch` | 240 | `finch.toml`: mały czytnik TOML, listy dla poszczególnych systemów |
| `src/fmt.fch` | 225 | `finch fmt` i formatowanie w serwerze języka |
| `src/lsp.fch` | 950 | Serwer języka (§11) |
| `src/docs.fch` | 140 | Jeden opis funkcji wbudowanych, wspólny dla podpowiedzi i dymków |
| `std/*.fch` | 1040 | `json`, `path`, `time` (+`_windows`), `proc` (+`_windows`), `fs`, `thread` |
| `runtime/finch_rt.c` | 1150 | Runtime w C: teksty, tablice, mapy, pliki, wypisywanie liczb, fs, wątki, uruchamianie testów, ścieżka wywołań |
| `bootstrap/` | | `build.sh`, `install.sh`, pliki startowe (§10) |

---

## 3. Część frontowa: lekser, parser, moduły

**Lekser.** Koniec linii kończy instrukcję; wcięcia nie mają znaczenia. Każdy token zna plik, linię i kolumnę, z których
korzystają wszystkie komunikaty o błędach.

**Parser.** Zejście rekurencyjne, jedna funkcja na każdą konstrukcję. Warto wiedzieć, że: typy są parsowane do nazwy
(`[]str`, `map[str]int`, `ptr[char]`, `Stack[int]`, `f32x8`); `fn Typ.metoda` deklaruje metodę; `fn nazwa[T]`
i `struct Nazwa[T]` deklarują szablony; `enum` i `match` mają własne węzły; `export fn` oznacza funkcję widoczną z C;
blok zapamiętuje swoją `endLine` (korzysta z niej serwer języka). Stara kolejność `fn nazwa(...) typ` daje błąd z podpowiedzią.

**Moduły.** `import nazwa` szuka po kolei: `nazwa_<system>.fch` obok importującego pliku (`windows`, `linux`, `macos`),
`nazwa.fch` w tym samym miejscu, katalogów z `FINCH_PATH`, a na końcu biblioteki standardowej `std/`. Każdy moduł jest
parsowany raz; jego nazwy są dostępne jako `modul.nazwa`. `import "x.h"` trafia zamiast tego do importu z C.

Moduł rozpoznaje się po **pliku**: kluczem jest jego ścieżka względem katalogu głównego pliku (albo katalogu z
`FINCH_PATH`, albo `std/`), bez `.fch` i bez końcówki `_<system>`, np. `ui/button`. `Program.imports` przechowuje nazwę,
której używa dany plik (`button` albo `btn` przy `import ui/button as btn`), a `Program.modules` klucz; `modFor()`
przechodzi od jednego do drugiego. Symbole używają klucza, w którym `/` zamieniono na `$` (`@finch.ui$button.make`).

**Stałe.** `const NAZWA = wyrazenie` to węzeł najwyższego poziomu; `constValue()` oblicza wyrażenie przy każdym użyciu,
w module, w którym je zdefiniowano, bez zmiennych lokalnych, więc wynik zawsze składa się do literału (operatory na
literałach całkowitych są składane w `arith`). `checkConst()` dopuszcza tylko literały, nazwy, operatory, pola
i konwersje liczbowe; `constBusy` wykrywa cykle.

**Drobiazgi leksera.** `1e9` i `2.5e-3` to tokeny zmiennoprzecinkowe (tekst jest uzupełniany o `.`, którego wymaga LLVM);
z tekstu `"""` funkcja `longString()` usuwa wcięcie linii zamykającej.

---

## 4. Generowanie kodu

`gen.fch` to jedna duża struktura `Gen` i funkcje, które na niej pracują. `generate()` wykonuje kolejno:

1. `declare()`: każda struktura Fincha i C dostaje typ LLVM (struktury C z prawdziwym wyrównaniem; struktury, które
   przez wartość zawierają same siebie, są odrzucane), a potem każda funkcja dostaje sygnaturę z *analizą pożyczania*
   parametrów (§8.2).
2. `define()` dla każdej funkcji: ciało, ze sprawdzeniem typów i emisją IR. Zasięgi pamiętają, co trzeba zwolnić przy
   wyjściu. Tworzenie instancji typów generycznych dokłada w tym czasie nowe funkcje, więc pętla trwa, dopóki pojawiają się nowe.
3. Punkt wejścia C, `main(argc, argv)` (a przy `--shared`: `finch_module_on_fatal` i opakowania funkcji `export fn`).
4. `assemble()`: deklaracje, typy struktur, stałe tekstowe, funkcje pomocnicze i ciała funkcji jako jeden tekst `.ll`.

Przy `-g` metadane DWARF są dopisywane na bieżąco, w trakcie zapisywania IR (najlepiej razem z `-O0`).

Typy generyczne są **monomorfizowane**: wywołanie `max(1, 2)` wyznacza `T`, a potem AST szablonu jest generowane jeszcze
raz z podstawionym `T`, pod nazwą `max[int]` (symbol `max.int`). `match` na enumie musi być wyczerpujący (brak wariantu
to błąd, `else` obejmuje resztę); na zwykłej wartości to ciąg porównań. Wartości enumów to `i32`; `Kolor(n)` sprawdza
zakres; do wypisywania służy wygenerowana tabela nazw.

Błędy jako wartości: `-> T!` zwraca parę (wartość, tekst błędu); `try` przy błędzie wychodzi z funkcji, `or` go obsługuje,
`error("…")` go tworzy.

---

## 5. Import z C i ABI języka C

`cimport.fch` woła **libclang** bezpośrednio z Fincha (sam jest napisany z `import "clang-c/Index.h"`), parsuje nagłówek
z flagami systemu docelowego i przechodzi po kursorach. Typy tłumaczy tak: parametr `const char *` → `str`, inne
`char *` → `ptr[char]`, wynik `char *` → `str`; liczby całkowite według rozmiaru i znaku (`char` jest bez znaku na
Linuksie ARM64); struktury i enumy według ich układu w pamięci.

LLVM IR nie realizuje sam konwencji wywołań C, więc `gen.fch` odtwarza ją ręcznie:

- `classify`: System V x86-64; struktura do 16 bajtów idzie w rejestrach według klasy każdych 8 bajtów (INTEGER/SSE), większa przez pamięć;
- `classifyArm64`: AAPCS64, w tym jednorodne agregaty liczb zmiennoprzecinkowych (HFA) i wariant Apple;
- reguły Microsoft x64 (struktury 1, 2, 4 i 8-bajtowe przez wartość w rejestrze, pozostałe przez wskaźnik);
- `makePlan` zamienia klasyfikację w plan wywołania; `cThunk` sprawia, że `addr(fn)` da się wywołać z C (funkcje zwrotne).

Ten sam kod pozwala kompilatorowi wołać libclang, którego API przekazuje `CXCursor`, `CXType` i `CXString` przez wartość.

**Makra.** `#define` z jedną liczbą albo jednym literałem tekstowym jest odczytywany z tokenów. Taki, którego tokeny
mogą tworzyć wyrażenie stałe (bez klamer, średników i słów kluczowych instrukcji), oblicza clang: drugie parsowanie
nagłówka dopisuje dla każdego `static const __auto_type __finch_m_NAZWA = (NAZWA);`, a `clang_Cursor_Evaluate` zwraca
liczbę całkowitą, liczbę typu double albo tekst. Makra, które nie są stałymi, dają w tym drugim parsowaniu tylko błędy
i są pomijane.

**Unie** to `CStruct` z `isUnion`: typ LLVM to `{ [n x iA] }` (rozmiar i wyrównanie podaje clang), każde pole ma
przesunięcie 0, a pole czyta się przez pamięć. Nie da się ich przekazać do C przez wartość (pole `unsupported` mówi dlaczego).

---

## 6. Sterownik, linkowanie i systemy docelowe

`finch build|run|ir|index plik.fch [-o wynik] [-O0] [-g] [-l bib] [-I katalog] [--target t] [--cpu c] [--shared] [--overlay sciezka=plik]`.
`ir` wypisuje IR; `index` wypisuje rekordy dla serwera języka (§11); `--shared` tworzy bibliotekę współdzieloną z funkcji
`export fn`.

`--cpu <nazwa>` (albo `cpu` w `finch.toml`) dodaje `-march=<nazwa>` na x86-64 albo `-mcpu=<nazwa>` na ARM64 do wywołania
clanga, które kompiluje IR (nie runtime); wartość wchodzi do klucza pamięci podręcznej.

`--target windows|linux|arm64|macos|android|<triple>` zmienia triple i układ danych, znak typu `char` w C, reguły ABI
opisane wyżej, wariant modułów dla systemu (`nazwa_windows.fch`) i kompilator C: `FINCH_CC` albo `clang --target=…`.
Przy `link "bib"` linkowanie pyta `pkg-config` o flagi; gdy linker zawiedzie, jego wyjście jest analizowane
i kompilator podpowiada brakujące `link "…"`. Obiekt runtime'u ma klucz z hasza swojego źródła i systemu docelowego.

Zmienne środowiskowe: `FINCH_PATH` (katalogi modułów), `FINCH_RT` (źródło runtime'u), `FINCH_CC` (kompilator C i linker;
IR zawsze kompiluje `clang`, nigdy `CC`), `FINCH_LIBCLANG` (dla `bootstrap/build.sh`).

Kompilator szuka `runtime/` i `std/` obok siebie (`../`, `./`) albo w `../share/finch/` (`bootstrap/install.sh`).

**Pamięć podręczna kompilacji** (`host.cacheDir()`: `$XDG_CACHE_HOME/finch`, `~/.cache/finch`, `%LOCALAPPDATA%\finch`):

- `ir/<hash głównego pliku + systemu + flag>.o` z plikiem `.key`, w którym jest hash clanga, flag i całego IR. Jedno
  miejsce na program, więc pamięć nie rośnie przy każdej zmianie kodu; to samo IR daje ten sam obiekt bez uruchamiania clanga.
- `c/<hash źródła + kompilatora + flag + systemu>.o` dla `link "x.c"` / `.cpp`, budowany z `-MMD`; `.key` to hash
  wszystkich plików wymienionych w `.d` (źródła i jego nagłówków). Zmiana któregokolwiek z nich wymusza ponowną kompilację.
- Obiekty są zapisywane do pliku `.part<pid>` i dopiero potem przemianowywane, więc dwie równoległe kompilacje nie
  zobaczą połowy pliku. Hasze to dwa niezależne 32-bitowe (FNV-1a i djb2), razem 64 bity.

**`finch.toml`** (`src/manifest.fch`) jest szukany od katalogu głównego pliku (albo katalogu roboczego) w górę.
Trafia do `values["sekcja.klucz"]` i `lists["sekcja.klucz"]`; `list(klucz, systemy)` łączy listę z najwyższego poziomu
z listami z `[target.<system>]` i `[target.<system>-<procesor>]`. Ścieżki są liczone względem katalogu manifestu.
Wpisy trafiają tam, gdzie opcje z wiersza poleceń: `paths` → wczytywanie modułów, `include` → `-I` (libclang i pliki C),
`links` → `link`, `libdirs` i `ldflags` → polecenie linkowania, `cflags` → pliki C/C++.

**Pliki C++**: plik `.cpp`/`.cc`/`.cxx` z `links` kompiluje `cxxFor(cc)` (`FINCH_CXX` albo `clang` → `clang++`,
`…gcc` → `…g++`, `cc` → `c++`), a potem ten sam sterownik linkuje, co dołącza bibliotekę standardową C++.

**Android**: `--target android` to `aarch64-linux-android24`; kompilator C to clang z NDK,
`toolchains/llvm/prebuilt/<host>/bin/aarch64-linux-android24-clang` (z `ANDROID_NDK_HOME`/`ANDROID_NDK_ROOT`);
linkowanie dodaje `-llog`, a nie dodaje `-lpthread` (bionic ma wątki w libc). Wariantem modułów jest `linux`.

**`finch test`** kompiluje tak jak `run`, z `Options.test`: wszystkie funkcje są definiowane jak zwykle, ale `@main` woła
`finch_test_run(nazwa, fn)` dla każdego bloku `test` z modułów użytkownika (`__test_<n>`, funkcja `void` utworzona przez
parser), a potem `finch_test_summary()`. Runtime uruchamia każdy test pod `setjmp`; `fatal()` (każdy błąd w trakcie
działania i nieudany `assert`) wraca przez `longjmp` z pomocą `fatal_hook`, więc jeden nieudany test nie przerywa reszty.
Pamięć, którą test miał w chwili błędu, nie jest zwalniana.

**`finch fmt`** działa na liniach: śledzi `{ }`, `( )`/`[ ]`, teksty (razem z `\( )`), `'…'`, `"""` i `/* */`.
Linia, która zaczyna się na głębokości nawiasów 0 i nie kontynuuje linii zakończonej operatorem albo `,`, dostaje
wcięcie 4 × głębokość klamer; pozostałe (kontynuacje, wnętrza `"""`) przesuwają się o tyle samo co linia, od której
zaczęła się instrukcja, więc zachowują kształt (i usuwanie wcięcia z `"""`).

---

## 7. System typów

### Reprezentacja w LLVM

| Finch | LLVM |
|---|---|
| `bool` | `i1` |
| `char` `i8` `u8` / `i16` `u16` / `i32` `u32` / `int` `u64` | `i8` / `i16` / `i32` / `i64` |
| `f32` / `float` | `float` / `double` |
| `str`, `[]T` | `{ ptr, i64, i64 }` (wskaźnik, długość, pojemność) |
| `map[K]V` | `{ ptr, i64, i64, i64, ptr, i64, i64 }` (wpisy, len, used, cap, indeks, icap, adds; zob. src/gen.fch) |
| `ptr`, `ptr[T]`, `null` | `ptr` (nieprzezroczysty) |
| `[N]T` | `[N x T]` |
| `f32x8`, `i8x32`, … | `<8 x float>`, `<32 x i8>` (§7, Wektory) |
| struktura Fincha | nazwany typ struktury, naturalny układ |
| `self` w metodzie | `ptr` (pierwszy parametr) |
| wynik `-> T!` | `{ i1 ok, T wartosc, str blad }` (`i8` zamiast `T` dla `-> !`) |
| struktura C | nazwana struktura **packed** z jawnymi bajtami wyrównania (§5) |

Znak liczby istnieje tylko w typie Fincha i wybiera `sdiv/udiv`, `ashr/lshr`, `icmp s*/u*`, `sext/zext`,
`sitofp/uitofp`, `fptosi/fptoui`.

### Wartości

```c
struct Val {
    str v           // operand IR: %t12, 42, @.str.3, zeroinitializer
    ast.Type ty
    bool fresh
    bool lit
}
```

- **`lit`**: liczba zapisana w kodzie albo stała zaimportowana z C. Może stać się dowolnym typem całkowitym,
  w którym się mieści (`fits()`), albo `f32`. Przetrwa jednoargumentowy minus i działania na samych literałach,
  które składają się do stałej; jawne konwersje ją usuwają (`u8(250)` to stała z typem, więc `u8(250) + 10` daje 4).
- **`fresh`**: wartość posiadająca pamięć (str, tablica, struktura z takimi polami), której jeszcze nikt nie trzyma:
  wynik wywołania, literał, połączenie tekstów, konstruktor. Trzeba ją zapisać (przenieść) albo zwolnić (§8).

### Konwersje niejawne: `coerce(v, want)`

Identyczne typy; `null` → dowolny wskaźnik; wskaźnik z typem ↔ `ptr` bez typu; liczba całkowita → całkowita, jeśli
to pasujący literał albo typ się „poszerza” (`widens()`: ten sam znak i więcej bitów, albo bez znaku → większy ze znakiem);
liczba całkowita → zmiennoprzecinkowa; `f32` → `f64`; literał `f64` → `f32`. Nic poza tym: komunikat błędu podpowiada
jawną konwersję. Typy posiadające pamięć przechodzą tylko w siebie. Literał tablicy bierze typ elementów od miejsca,
do którego trafia (`exprWant()`), więc `[]f32 xs = [1, 2.5]` działa.

### Argumenty operatorów: `unify(l, r)`

Strona, która jest literałem, dopasowuje się do drugiej (także literał zmiennoprzecinkowy obok `f32`: `x * 2.0`
zostaje `f32`); w przeciwnym razie rośnie ta strona, którą da się poszerzyć; `i32` + `u32` to błąd; całkowita +
zmiennoprzecinkowa → zmiennoprzecinkowa; f32 + f64 → f64. Nie ma promocji liczb całkowitych jak w C: `u8 + u8`
zostaje `u8`. Dwa literały całkowite składają się w literał, więc `FLAG_A | FLAG_B` nadal mieści się w `u64` albo `u32`.

### Konwersje jawne: `convert()`

| Z → Na | IR |
|---|---|
| int/char ↔ int/char | `trunc`/`sext`/`zext` według znaku źródła |
| bool → int | `zext` |
| zmiennoprzecinkowa ↔ całkowita | `fptosi`/`fptoui`, `sitofp`/`uitofp` |
| zmiennoprzecinkowa ↔ zmiennoprzecinkowa | `fpext`/`fptrunc` |
| int → bool | `icmp ne 0` |
| liczba/bool/char → str | `finch_str_from_int/uint/float/char/bool` (fresh) |
| str → int/float | `finch_str_to_int/float` (przy złym tekście błąd w trakcie działania) |
| `[]u8`/`[]char` → str | `finch_str_from_bytes` |
| ptr → str | `finch_str_from_c` (pożyczony, `cap = -1`) |
| ptr ↔ ptr | nic |
| int → ptr / ptr → int | `inttoptr` / `ptrtoint` |

### Porównania

Wskaźniki i `null`: tylko `icmp eq/ne ptr`. `str`: `finch_str_eq` (długość + `memcmp`) dla `==`/`!=`,
`finch_str_cmp` dla kolejności. Liczby zmiennoprzecinkowe: `oeq olt ole ogt oge` oraz `une` dla `!=`. Liczby całkowite
i `char` według znaku (`char` bez znaku). Bool: tylko równość. Tablice i struktury: błąd.

### Wektory

`ast.vecType(nazwa)` rozpoznaje `<element>x<N>` (elementy `i8`…`u64`, `f32`, `f64`; N = 2…64; najwyżej 512 bitów)
i zwraca `Type(kind: "vec", elem: [element], count: N)`, czyli wektor LLVM `<N x T>`. Parser przyjmuje taką nazwę
wszędzie tam, gdzie może stać typ; wywołanie `f32x8(…)` trafia do `vecMake()`.

| Finch | IR |
|---|---|
| `f32x8(x)` | `insertelement` do elementu 0 + `shufflevector … zeroinitializer` (rozpowielenie) |
| `f32x8(a, …, h)` | ciąg `insertelement` |
| `f32x8(lista, i)` | sprawdzenie zakresu `i … i+7`, potem `load <8 x float>, ptr …, align 4` (wyrównanie elementu) |
| `f32x8(i32x8 v)` | `sitofp`, `fptosi`, `sext`, `trunc`… na całym wektorze |
| `v + w`, `v * 2.0` | `fadd`/`add`… na wektorach; argument skalarny jest dopasowywany do typu elementu i rozpowielany |
| `v[i]` | miejsce w pamięci: `getelementptr <element>, ptr %v, i64 i` po sprawdzeniu zakresu; albo `extractelement` na wartości tymczasowej |
| `v < w`, `v == w` … | `fcmp`/`icmp` → `<N x i1>`, maska (`boolxN`: `vec` z elementami `bool`) |
| `select(m, a, b)` | `select <N x i1>` |
| `m.any()`, `.all()`, `.count()` | `llvm.vector.reduce.or`/`.and`; `zext` + `llvm.vector.reduce.add` |
| `f32x8(lista, i, wypelnienie)`, `v.store_part(lista, i)` | `llvm.masked.load`/`llvm.masked.store` z maską `<0, 1, …> < len - i` |
| `v.shuffle(…)`, `.lo()`, `.hi()`, `concat(a, b)` | `shufflevector` ze stałą maską |
| `tabela.lookup(idx)` | `and` z `len - 1`, potem `extractelement`/`insertelement` dla każdego elementu: wzorzec, który zaplecza x86 i ARM zamieniają na `pshufb`/`tbl` |
| `acc.dot_add(a, b)` | `llvm.x86.avx512.vpdpbusd.{128,256,512}` albo `llvm.aarch64.neon.{usdot,sdot,udot}`, gdy funkcja ma te rozszerzenia; w przeciwnym razie `zext`/`sext`, `mul`, cztery `shufflevector` co czwarty element i `add` |
| `min`, `max`, `abs` | `llvm.minnum`/`maxnum`, `llvm.smin`/`umin`…, `llvm.fabs`/`llvm.abs` |
| `v.sum()`, `.min()`, `.max()` | `llvm.vector.reduce.fadd` (z `reassoc`), `.add`, `.fmin`, `.smin`… |
| `v.sqrt()`, `.floor()`, `.ceil()`, `.round()` | funkcja wewnętrzna `llvm.*` o tej nazwie |
| `v.fma(b, c)`, `fma(a, b, c)` | `llvm.fma` (jedno zaokrąglenie; bez sprzętowego FMA wywołanie libm dla każdego elementu) |
| `v.muladd(b, c)`, `muladd(a, b, c)` | `llvm.fmuladd` (jedna instrukcja tam, gdzie procesor ma FMA, a gdzie nie, `fmul` + `fadd`) |
| `v.store(lista, i)` | sprawdzenie zakresu, `store <8 x float> …, align 4` |

Dzielenie całkowite `/` i `%` jest odrzucane: ani x86, ani ARM nie mają instrukcji dzielącej liczby całkowite element
po elemencie, więc LLVM i tak rozbiłby je na pojedyncze dzielenia. Porównania kolejności są odrzucane, bo ich wynikiem
byłaby maska, dla której Finch nie ma typu.

**Wyrównanie.** LLVM daje `<8 x float>` wyrównanie ABI równe 32 (jego rozmiarowi), a `malloc` i finchowe `alloca … align 16`
gwarantują tylko 16. `llty()` zapisuje każdy wektor szerszy niż 16 bajtów w `g.wide`, a `resolveStructBody` zapisuje tam
strukturę, która taki wektor zawiera; `load()` i `store()` dodają dla tych typów `, align 16`. Bez tego LLVM mógłby
użyć `vmovaps` na adresie wyrównanym do 16 i program by się wysypał.

**Jakie instrukcje** wybiera clang: `--cpu <nazwa>` dodaje `-march=<nazwa>` (x86-64) albo `-mcpu=<nazwa>` (ARM64)
do kompilacji IR, więc to samo IR staje się kodem SSE2, AVX2 + FMA, AVX-512 albo NEON. Wektory szersze niż rejestry
procesora są dzielone na części.

Niektóre działania muszą znać rozszerzenia procesora już przy generowaniu kodu (`dot_add` wybiera `vpdpbusd` tylko tam,
gdzie ono istnieje). `linker.cpuFeatures()` pyta clanga o makra, które sam definiuje dla tego samego systemu i `--cpu`
(`clang -march=X -dM -E pusty.c`: `__AVXVNNI__`, `__AVX512VNNI__`, `__ARM_FEATURE_MATMUL_INT8` …), zamienia je na nazwy
rozszerzeń Fincha i zapamiętuje odpowiedź dla danego clanga i flag; do `gen` trafiają jako `Options.features`. `@cpu("…")`
przy funkcji (`Node.cpu`) dodaje własne: `define … "target-features"="+avx2,+fma"` (z `+evex512` przy każdym rozszerzeniu
AVX-512, którego LLVM 18+ wymaga dla rejestrów 512-bitowych), a `g.curFeatures` to suma obu na czas generowania tej
funkcji. Rozszerzenia drugiej architektury są pomijane (`cpuOf`). `checkCpuCall` nie pozwala przekazać wektora szerszego
niż 128 bitów jako parametru albo wyniku między funkcjami o różnym `@cpu`, bo wywołujący i wywoływany umieściliby go
w innych rejestrach. `cpu_has(nazwa)` woła `finch_cpu_has` z runtime'u: na x86 CPUID (liście 1 i 7) z XGETBV dla stanu
AVX/AVX-512, na Linuksie ARM64 `getauxval(AT_HWCAP/AT_HWCAP2)`, na macOS `sysctlbyname("hw.optional.arm.FEAT_…")`.

Na Uniksach clang jest zawsze uruchamiany jako `NIX_ENFORCE_NO_NATIVE=0 clang …`: inaczej nakładka cc-wrapper z Niksa
usuwa `-march=native` w `nix-shell`. Jeśli `--cpu native` mimo to nie daje na x86 makra SSE4.2, sterownik ostrzega.

---

## 8. Własność i pamięć

### 8.1 Model

Każda wartość **typu posiadającego pamięć** (`str`, `[]T` albo struktura Fincha z takim polem) ma dokładnie jednego
właściciela: zmienną, element tablicy, pole struktury albo komórkę na stercie z `new()`. Właściciel zwalnia wartość,
gdy sam znika. Nie ma liczenia referencji ani garbage collectora; wszystkie decyzje zapadają przy kompilacji.

| Sytuacja | Co się dzieje |
|---|---|
| `x := wyr` / `T x = wyr` / `x = wyr` / pole, element, `push` | **`own(v)`**: wartość świeża zostaje przeniesiona, pożyczona jest głęboko kopiowana |
| `x = …` (typ posiadający) | najpierw liczona jest nowa wartość, a stara jest zwalniana po zapisie |
| koniec bloku, `return`, `break`, `continue` | zmienne we wszystkich opuszczanych zasięgach są zwalniane (od najnowszej), wykonują się `defer` |
| `return zmienna_lokalna` | zmienna jest **przenoszona** (oznaczona jako `moved` na czas sprzątania przy tym `return`) |
| przekazanie argumentu | **pożyczka**: funkcja dostaje płytką kopię i nigdy jej nie zwalnia; świeże argumenty zwalnia wywołujący po wywołaniu |
| parametr, który funkcja zmienia | kopiowany na wejściu i należący do funkcji (decyduje `mutates()`, niżej) |
| wartość tymczasowa, której nikt nie zatrzymuje (`print(a + b)`, `f(g())`, wyrażenie jako instrukcja) | **`release(v)`**: zwalniana zaraz po użyciu |
| `a.pop()`, `a.remove(i)` | element jest przenoszony na zewnątrz (świeży) |
| `for x in lista` | `x` pożycza kolejne elementy; jeśli ciało pętli może zmienić `lista`, elementy są kopiowane |
| wywołanie metody `v.m()` | `self` **to jest** `v` (przekazywany jest adres): bez kopii, zmiany widzi wywołujący |
| `fn(A, B) -> R` | `{ ptr, ptr }`: funkcja i jej otoczenie. Każdą wartość-funkcję wywołuje się `f(ptr env, A, B)`; nazwana funkcja idzie przez opakowanie `@finch.val.*`, które pomija `env`. Lambda staje się własnym `FnSig` (`lambda`, `cnames`, `ctypes`, `envTy`), definiowanym później przez zwykłą pętlę; jej otoczenie to `{ ptr drop, ptr copy, przechwycone... }` na stercie, więc `finch_fn_copy` / `finch_fn_drop` (runtime) kopiują i zwalniają każdą wartość-funkcję bez wiedzy o jej zawartości. Przechwycone zmienne to kopie zrobione w chwili obliczenia lambdy (`lambdaExpr`, `envHelpers`), w niej tylko do odczytu (`Var.captured`) |
| `T?` | `{ i1 jest, T wartość }`; posiadające, gdy `T` jest. Kopiowanie i zwalnianie sprawdzają flagę (`copyFn` / `dropFn`, rodzaj `"opt"`). `T` i `null` wchodzą same (`coerce`); wychodzi tylko przez `orSource()` (`x or ...`), `ifSome()` (`if v := x`) i `== null` |
| interfejs | `{ ptr pudełko, ptr tablica }`: własna kopia wartości na stercie i stała tablica `[drop, copy, format, metody...]` dla każdej pary (typ, interfejs) (`ifaceTable`, `boxHelpers`). Wywołanie metody bierze wpis k+3 i podaje pudełko jako `self` (`ifaceCall`). Spełnianie jest strukturalne i sprawdzane przy zamianie wartości (`toIface` → `ifaceMethod`); `finch_iface_copy` / `finch_iface_drop` w runtime |
| parametr `mut T x` | miejsce wywołującego (przekazywany jest adres, `mutArg()` / `mutPlace()`): bez kopii i bez zwalniania; `checkMutArgs()` odrzuca tę samą zmienną w innym argumencie posiadającym albo `mut` i jako odbiorcę |
| mapy | są właścicielami kluczy i wartości tak jak tablice elementów; `m[k] = v` przenosi albo kopiuje `v` i zwalnia starą wartość |

Literał tekstowy to świeża wartość z `cap = 0`: przeniesienie go nic nie kosztuje, zwalnianie jest pomijane
(`release()` ignoruje stałe), a zapis do niego (`s[i] = c`) najpierw robi kopię na stercie (`finch_str_own`).
Teksty z C są pożyczone z `cap = -1` i zawsze kopiowane, zanim zostaną zatrzymane.

### 8.2 Pożyczanie parametrów: `mutates()`

Przy deklarowaniu funkcji każdy parametr typu posiadającego jest sprawdzany przez `mutates(nazwa, cialo)`: przypisanie,
którego zmienną bazową (`rootVar()`: przez `.pole` i `[i]`) jest ten parametr, zmieniająca metoda na nim
(`push pop insert remove clear resize sort reverse`) albo `addr(…)` na nim. Niezmieniane parametry są pożyczane
(`FnSig.borrow`); zmieniane są kopiowane raz, na wejściu. Analiza jest składniowa i ostrożna: wszystko, co *mogłoby*
zmienić parametr, powoduje kopię, a to zawsze jest poprawne. Ta sama analiza decyduje, czy `for x in lista` musi kopiować elementy.

Metody też w tym uczestniczą: `declare()` najpierw ustala, które metody zmieniają `self` (`mutates("self", cialo)`),
powtarzając to, dopóki coś się zmienia, bo wywołanie zmieniającej metody na `self` też zmienia `self`. Ich nazwy trafiają
do `g.mm`, które `mutates()` traktuje jak `push`: `p.przesun(1)` zmienia `p`. Dopiero potem dla każdej funkcji i metody
liczone jest `borrow`.

### 8.3 Kopiowanie i zwalnianie

`dropAt(adres, T)` / `copyAt(cel, zrodlo, T)` działają na adresach:

- `str`: `finch_str_drop` (zwalnia, gdy `cap > 0`) / `finch_str_copy` (literały współdzieli, resztę duplikuje).
- `[]T` i struktury posiadające pamięć: wygenerowane funkcje pomocnicze `finch.drop.<klucz>` i `finch.copy.<klucz>`,
  tworzone raz na typ (`dropFn`, `copyFn`, klucz z `typeKey()`, np. `arr_S_main_Player`). Tablica zwalnia każdy
  element posiadający pamięć, a potem woła `finch_arr_free`; kopia woła `finch_arr_clone_raw`, a potem głęboko kopiuje
  takie elementy w miejsce surowych bitów. Struktury zwalniają i kopiują tylko pola posiadające pamięć.

Funkcje pomocnicze powstają jako osobne funkcje (`beginHelper()` / `endHelper()` odkładają na bok funkcję, która jest
w trakcie pisania, i wyłączają lokalizacje debuggera), mogą wołać się nawzajem rekurencyjnie (`struct Node { []Node kids }`),
a O2 je wkleja w miejsce wywołań.

### 8.4 Jak to wygląda

```c
fn przywitaj(str imie) -> str {
    wiad := "Cześć, " + imie
    return wiad
}

fn main() {
    a := ["x"]
    b := a
    b.push(przywitaj("Ola"))
    print(b[1])
}
```

IR funkcji `main` przy `-O0` (w skrócie):

```llvm
  %3 = call ptr @finch_alloc(i64 24)                         ; ["x"]: jeden element na stercie
  store { ptr, i64, i64 } { ptr @str.1, i64 1, i64 0 }, ptr %4  ; literał, cap 0
  ...
  store { ptr, i64, i64 } %7, ptr %a                          ; przeniesione do a (świeże)
  %a1 = load { ptr, i64, i64 }, ptr %a
  store { ptr, i64, i64 } %a1, ptr %1
  call void @finch.copy.arr_str(ptr %2, ptr %1)               ; b := a  kopiuje
  %8 = load { ptr, i64, i64 }, ptr %2
  store { ptr, i64, i64 } %8, ptr %b
  %9 = call { ptr, i64, i64 } @finch.przywitaj({ ptr, i64, i64 } { ptr @str.2, i64 3, i64 0 })
  ...
  call void @finch_arr_reserve(ptr %b, i64 %12, i64 24)       ; push: powiększenie, jeśli trzeba
  store { ptr, i64, i64 } %9, ptr %15                         ; wynik przywitaj przeniesiony, bez kopii
  ...
  %20 = icmp uge i64 1, %19                                   ; b[1]: sprawdzenie zakresu
  br i1 %20, label %out.of.range, label %in.range
  ...
  call void @finch.drop.arr_str(ptr %b)                       ; koniec main: od najnowszej
  call void @finch.drop.arr_str(ptr %a)
  ret void
```

a `przywitaj` zwraca `wiad` bez kopiowania (`ret … %5` po wczytaniu zmiennej: została przeniesiona).

### 8.5 defer

`defer instrukcja` zapisuje instrukcję w bieżącym zasięgu (`Scope.defers`, razem z `orders`: ile zmiennych wtedy
istniało) oraz `Cleanup{isDefer}`; w tym miejscu nic nie emituje. Ilekroć emitowane jest sprzątanie zasięgu (koniec
bloku, `return`, wyjście przez `break`/`continue`), ciało jest generowane **przy tym wyjściu**, z `deferLimit`
ustawionym tak, by `lookup()` ukrywało zmienne zadeklarowane po `defer`: w tym momencie są już zwolnione (sprzątanie
idzie od końca). `return`/`break`/`continue` wewnątrz odroczonej instrukcji to błąd (`inDefer`). Ciała `defer` są więc
powielane na każdą drogę wyjścia i dlatego na zwykłej ścieżce nic nie kosztują.

### 8.6 Pamięć zarządzana ręcznie

`new(v)` przydziela `sizeof(T)` przez `finch_alloc`, zapisuje `own(v)` i zwraca `ptr[T]`. `free(p)` zwalnia to, na co
wskazuje wskaźnik, jeśli `T` posiada pamięć (pomija `null`), i woła `free` z libc. Wskaźniki nigdy niczego nie posiadają:
`addr(x)` może wskazywać w próżnię po końcu bloku `x`; tego się nie sprawdza.

### 8.7 Sprawdzanie

`MEMCHECK=1 tests/run.sh` buduje każdy program testowy i uruchamia go pod valgrindem z
`--leak-check=full --errors-for-leak-kinds=all`: zero wycieków i zero błędnych dostępów we wszystkich, w tym przy
`return` z wnętrza pętli, `continue`/`break` ze zmiennymi posiadającymi pamięć, `defer`, zmianach w trakcie iteracji,
zagnieżdżonych tablicach struktur i przekazywaniu struktur C.

---

## 9. Typy generyczne, enumy i match

Jak są generowane, opisuje §4. Reguły widoczne dla użytkownika są w rozdziałach 23 i 24
[przewodnika dla techników](dla-technikow.md). Testy to `tests/new/generics.fch`, `tests/new/enums.fch` i oczekiwane
błędy w `tests/new/fail/`.

---

## 10. Bootstrap

Kompilator powstaje z **pliku startowego** (ang. *seed*): jego własnego IR, zapisanego w repozytorium jako
`bootstrap/<system>.ll` (zob. [bootstrap/README.md](../../bootstrap/README.md)). `bootstrap/build.sh` robi:

```
plik .ll ──clang──► finch0 ──buduje src/──► finch1 ──buduje src/──► build/finch
```

i przerywa, jeśli `finch1` i `finch` nie dają identycznego bajt w bajt IR (punkt stały: kompilator kompiluje sam siebie
tak samo, niezależnie od tego, co go zbudowało). `bootstrap/build.sh seed` zapisuje plik startowy na nowo z bieżącego
kompilatora; trzeba to zrobić, gdy kod kompilatora zaczyna używać czegoś, czego zapisany plik startowy nie zna.

Plik startowy jest osobny dla każdego systemu, bo zawiera konwencję wywołań C dla funkcji libclang (§5). Są pliki
dla Linuksa x86-64 i ARM64, macOS ARM64 i x86-64 (zrobione z nagłówkami SDK Apple) oraz Windowsa x86-64 (z nagłówkami
llvm-mingw). CI na każdym z tych systemów buduje z nich kompilator i uruchamia tam testy.

---

## 11. Serwer języka

`finch lsp` rozmawia przez JSON-RPC na stdin/stdout (ramki `Content-Length`, `std/json`). Sam nie sprawdza typów:
przy każdej zmianie uruchamia `finch index plik.fch --overlay sciezka=kopia …`, gdzie nakładki to niezapisany tekst
z edytora. W trybie `index` kompilator zamiast IR wypisuje rekordy rozdzielone tabulatorami:

| Rekord | Znaczenie |
|---|---|
| `@R plik linia kol dlug plikDef liniaDef kolDef dymek` | odwołanie: co pokazać w dymku i gdzie jest definicja |
| `@V nazwa typ plik linia kol plikFn liniaFn koniecFn` | zmienna i funkcja, w której żyje |
| `@F modul nazwa odbiorca plik linia kol koniec sygnatura parametry` | funkcja albo metoda |
| `@S` / `@SF`, `@E`, `@I` | struktury (i pola), enumy, importy |
| `@W` | ostrzeżenie |

Pierwszy błąd jest wypisywany jako `sciezka:linia:kol: error: komunikat` i staje się diagnostyką. Gdy kod się nie
kompiluje, serwer nadal podpowiada uzupełnienia i parametry na podstawie ostatniej wersji, która się kompilowała.
Testem akceptacyjnym jest `tests/lsp_test.py`.

---

## 12. Semantyka w porównaniu z C

| Sytuacja | C | Finch |
|---|---|---|
| Przepełnienie liczby całkowitej ze znakiem | UB | zdefiniowane zawinięcie (bez `nsw`) |
| Dzielenie całkowite przez zero, `INT_MIN / -1` | UB | błąd kompilacji, jeśli to stała; w przeciwnym razie błąd w trakcie działania |
| Indeks tablicy poza zakresem | UB | błąd w trakcie działania (także dla pól `[N]T` struktur C) |
| Dereferencja `null` | UB | błąd w trakcie działania dla `.value`, `.pole`, `[i]` |
| Użycie po zwolnieniu / podwójne zwolnienie tablic, tekstów, struktur | częste błędy | niemożliwe: tych nie zwalnia się ręcznie |
| Wyciek pamięci tablic, tekstów, struktur | częsty błąd | niemożliwy bez `new` |
| `x & 1 == 0` | `x & (1 == 0)` | `(x & 1) == 0` |
| Niejawne zwężanie, mieszanie liczb ze znakiem i bez | po cichu | błędy kompilacji |
| Niezainicjowane zmienne | UB | niemożliwe (zero albo wartości domyślne) |
| Brak `return` | UB, jeśli wynik jest użyty | błąd kompilacji |
| Kolejność obliczania | w dużej mierze nieokreślona | od lewej do prawej; w przypisaniu najpierw prawa strona |
| Przesunięcie o ≥ szerokość, liczba zmiennoprzecinkowa poza zakresem przy zamianie na całkowitą | UB | poison, jak w LLVM (niesprawdzane) |
| `a * b + c` na liczbach zmiennoprzecinkowych | może zostać złożone w FMA (`-ffp-contract`) | nigdy nie jest łączone: dwa zaokrąglenia |

### Liczby zmiennoprzecinkowe

Działania na liczbach zmiennoprzecinkowych to zwykłe `fadd`/`fsub`/`fmul`/`fdiv`/`fneg` **bez flag fast-math**, więc LLVM
nie może zmieniać kolejności działań, łączyć ich ani zakładać braku NaN; `linker.fch` kompiluje też IR z
`-ffp-contract=on`, które łączy tylko wywołania `llvm.fmuladd`, o które prosi Finch. Jedyna instrukcja z flagą to
`llvm.vector.reduce.fadd` w `v.sum()` (`reassoc`). `fma` → `llvm.fma`, `muladd` → `llvm.fmuladd`, na liczbach i wektorach
(`fmaCall`, `fusedOp`).

`f32_bits`/`f32_from_bits`/`f64_…` to `bitcast` (także między wektorami). `f16_to_f32` to `bitcast i16 → half` + `fpext`,
`f32_to_f16` to `fptrunc` + `bitcast`: jedna instrukcja z F16C albo na ARM64, a gdzie indziej `__extendhfsf2`/`__truncsfhf2`
z biblioteki uruchomieniowej kompilatora C. `bf16_to_f32` to `zext` + `shl 16` + `bitcast`; `f32_to_bf16` zaokrągla do
najbliższej parzystej na liczbach całkowitych (`u + 0x7fff + ((u >> 16) & 1)`, potem `>> 16`) i zostawia NaN cichym
(`| 0x40`), więc nie potrzebuje żadnej biblioteki.
| Wiszący wskaźnik z `addr()` / `new` + `free` | UB | nadal UB |

### Ścieżka wywołań przy błędach w trakcie działania

Z `Options.trace` (domyślnie dla `run`, `test`, `-g`, `-O0`; `--trace`/`--no-trace`; nigdy z `--shared`) każda funkcja
Fincha ma na stosie ramkę:

```llvm
%frame = alloca { ptr, ptr, ptr, i64 }          ; ramka wywołującego, nazwa, plik, linia
store ptr (load @finch_trace), ...pole 0
store ptr @"nazwa", ...pole 1 ; ptr @"plik", pole 2 ; i64 <linia>, pole 3
store ptr %frame, ptr @finch_trace
```

`@finch_trace` to globalna zmienna `thread_local` w module programu (model initial-exec, chyba że to biblioteka
albo Windows); każda instrukcja zapisuje swoją linię do pola 3. Po każdym wywołaniu funkcji Fincha wywołujący zapisuje
z powrotem swoją ramkę do `@finch_trace`, co przy okazji usuwa ramki pozostawione przez funkcję, która wyszła wcześniej,
więc na licznych ścieżkach powrotu nie trzeba nic sprzątać. Opakowania wywoływane z C (`addr(fn)`, `export fn`)
zapamiętują i odtwarzają `@finch_trace` wokół wywołania. `main` woła `finch_trace_init(top, set)` z dwiema małymi
funkcjami, które ją czytają i zapisują; tak runtime (`print_trace()` w `fatal()` i uruchamianie testów) dostaje się do
zmiennej `thread_local`, której nie potrafi nazwać. Koszt: ok. 3 razy wolniej na `fib(40)`, które składa się wyłącznie
z wywołań; dlatego zwykłe `finch build` tego nie dodaje.

### Wątki i operacje atomowe

`std/thread.fch` to cienka warstwa nad runtime'em (`finch_thread_spawn/join`, `finch_mutex_*`: pthreads albo, na
Windowsie, `CreateThread` i `CRITICAL_SECTION`). Funkcja wątku to dowolna `fn f(ptr)`, przekazywana przez `addr(f)`,
co tworzy opakowanie wywoływalne z C. `atomic_add/sub/swap` stają się `atomicrmw add/sub/xchg … seq_cst`,
`atomic_load/store` to `load/store atomic … seq_cst, align <rozmiar>`, a `atomic_cas` to `cmpxchg … seq_cst seq_cst`
zwracające `i1`. Przyjmują `ptr[T]` do dowolnego typu całkowitego. Tablice, teksty i mapy Fincha nie są synchronizowane;
dzielenie ich między wątkami wymaga muteksu.

### Ostrzeżenia

`warn()` wypisuje `plik:linia:kol: warning: …` na stderr (żeby `finch ir` pozostało czystym IR) albo, w trybie `index`,
rekord `@W`, który serwer języka publikuje z ważnością 2. Jedno ostrzeżenie na miejsce w kodzie. Są dwa rodzaje:
parametr będący strukturą oraz element w `for x in lista`, jeśli ich zwykłe dane (`byteSize()`: liczby, zagnieżdżone
struktury, `[N]T` z C; `str`, tablice i mapy liczą się jako 0, bo kopiuje się tylko ich uchwyt) mają co najmniej 256 bajtów.

### Struktury przekazywane przez adres

`argWant()` (tylko argumenty wywołań, nie przypisania): gdy parametr ma typ `ptr[S]`, a argument jest zwykłą zmienną
typu `S`, argumentem staje się adres tej zmiennej, tak jakby napisano `addr(x)`. Dotyczy to tylko zmiennych, więc nic
nie jest obliczane dwa razy, a wskaźnik nigdy nie wskazuje na wartość tymczasową.

---

## 13. Znane ograniczenia

- Jeden błąd na uruchomienie, bez kontynuowania po nim.
- Informacje dla debuggera (`-g`) są najlepsze z `-O0`.
- Typy generyczne są monomorfizowane (kod rośnie z każdą instancją); nie ma ograniczeń na parametry typu, więc złe
  użycie daje błąd wewnątrz ciała instancji.
- Pamięć, którą miał nieudany test, nie jest zwalniana (uruchamianie testów wychodzi z niego przez `longjmp`).
- Wektory nie mają dzielenia całkowitego, a `lookup` na podstawowym x86-64 (bez SSSE3) działa element po elemencie.
