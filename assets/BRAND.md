# Finch brand

> 🇵🇱 Opis po polsku jest niżej.

## Logo

| File | Use |
|---|---|
| `logo.svg` / `logo.png` | the icon + wordmark, for light backgrounds |
| `logo-dark.svg` / `logo-dark.png` | the same for dark backgrounds |
| `logo-mark.svg` / `logo-mark-dark.svg` | the mark alone, no background: for light / for dark backgrounds |
| `icon.svg`, `icon.png`, `icon-256.png`, `favicon-64.png` | app icon / avatar / favicon |
| `social-preview.svg` / `social-preview.png` | GitHub social preview (1280×640) |

**The mark** is the **f** of the wordmark with a dot in its top-right corner: the same rounded strokes, and a dot the
size of the one over the **i**, half a stroke after the f's curl with their tops lined up. It stands on its own
(Ink f and orange dot on light backgrounds, Cloud f on dark), and as the app icon on a Finch Orange hexagon (white f,
Seed dot), the shape of the classic systems languages. `make_logo.py` draws it and writes every file above:
`python3 assets/make_logo.py --png` (the PNGs are rendered with `rsvg-convert` or Inkscape).

The wordmark is drawn with strokes (no font needed), lowercase, with an orange dot over the **i**.
Keep clear space around the logo of at least the height of the letter **n**. Don't recolor the mark, rotate it,
stretch it, add shading or outlines, or put it on busy backgrounds; the dot stays in the f's top-right corner.

## Colors

| Name | Hex | Use |
|---|---|---|
| Finch Orange | `#FF7A1A` | the mark, accents, the dot over the i |
| Glow | `#FFA24D` | light orange highlights |
| Ember | `#D9570B` | dark orange, pressed states |
| Seed | `#FFC145` | the dot of the app icon, numbers in code |
| Ink | `#1B1F2A` | text, dark backgrounds |
| Slate | `#2E3445` | secondary dark |
| Cloud | `#F5F3EF` | text on dark, light backgrounds |
| Shell | `#FFF3E6` | warm light backgrounds |

## Type

Headlines and text: **Inter**. Code: **JetBrains Mono**.

## Voice

Short sentences. Plain words. Say what something does, then show it. Errors explain how to fix the problem.
Tagline: **Small, quick, sharp.**

---

## Marka Finch (po polsku)

**Logo:** pliki jak w tabeli wyżej. Znak to **f** z napisu z kropką w prawym górnym rogu: te same zaokrąglone
linie i kropka wielkości tej nad **i**, pół grubości linii za zawinięciem f, z wyrównanymi górami. Działa sam
(f w kolorze Ink i pomarańczowa kropka na jasnym tle, f w kolorze Cloud na ciemnym) i jako ikona aplikacji na
sześciokącie Finch Orange (białe f, kropka Seed), kształcie klasycznych języków systemowych. Rysuje go
`make_logo.py` (`python3 assets/make_logo.py --png` zapisuje wszystkie pliki). `logo.svg` jest na jasne tło, `logo-dark.svg` na ciemne,
`logo-mark.svg` / `logo-mark-dark.svg` to sam znak bez tła (na jasne / na ciemne tło), `icon*` to ikona i awatar, a `social-preview.png` to podgląd linku na GitHubie.
Napis jest rysowany liniami (nie potrzebuje czcionki), małymi literami, z pomarańczową kropką nad **i**.
Zostaw wokół logo wolne miejsce co najmniej wysokości litery **n**. Nie zmieniaj kolorów znaku, nie obracaj
go, nie rozciągaj, nie dodawaj cieni ani obrysów i nie kładź go na pstrokatym tle; kropka zostaje w prawym górnym rogu f.

**Kolory:** Finch Orange `#FF7A1A` (znak, akcenty), Glow `#FFA24D` (jasne akcenty), Ember `#D9570B` (ciemny pomarańcz), Seed `#FFC145` (kropka ikony, liczby w kodzie),
Ink `#1B1F2A` (tekst, ciemne tło), Slate `#2E3445`, Cloud `#F5F3EF` (tekst na ciemnym), Shell `#FFF3E6` (ciepłe jasne tło).

**Czcionki:** Inter do tekstu, JetBrains Mono do kodu.

**Styl wypowiedzi:** krótkie zdania, proste słowa. Najpierw powiedz, co coś robi, potem to pokaż.
Komunikaty błędów mówią, jak naprawić problem. Hasło: **Small, quick, sharp.** („Mały, zwinny, skuteczny”).
