# Finch brand

> 🇵🇱 Opis po polsku jest niżej.

## Logo

| File | Use |
|---|---|
| `logo.svg` / `logo.png` | the mark + wordmark, for light backgrounds |
| `logo-dark.svg` / `logo-dark.png` | the same for dark backgrounds |
| `logo-mark.svg`, `logo-mark-dark.svg` | the mark alone (it works on light and dark) |
| `icon.svg`, `icon.png`, `icon-256.png`, `favicon-64.png` | app icon / avatar / favicon |
| `social-preview.svg` / `social-preview.png` | GitHub social preview (1280×640) |

**The mark** is a hexagon in three shades of orange, lit from the upper left, with a white finch flying through it
towards the upper right: wings swept back, a short seed-cracker beak, a forked tail. Small, quick, sharp. It is made by
`make_logo.py`, which also puts it into the logos and the social preview; the PNGs are rendered from the SVGs
(`rsvg-convert -w 512 -h 512 icon.svg -o icon.png`, and 256, 64, the logos at 1000×300, the preview at 1280×640, the
VS Code icon at 128).

The wordmark is drawn with strokes (no font needed), lowercase, with an orange dot over the **i**.
Keep clear space around the logo of at least the height of the letter **n**. Don't recolor the mark, rotate it,
stretch it, or put it on busy backgrounds; the bird stays white.

## Colors

| Name | Hex | Use |
|---|---|---|
| Finch Orange | `#FF7A1A` | the mark's right face, accents, the dot over the i |
| Glow | `#FFA24D` | the mark's lit face, light orange highlights |
| Ember | `#D9570B` | the mark's lower face, the bird's eye |
| Seed | `#FFC145` | the beak, numbers in code |
| Ink | `#1B1F2A` | text, the wing, dark backgrounds |
| Slate | `#2E3445` | secondary dark |
| Cloud | `#F5F3EF` | text on dark, light backgrounds |
| Shell | `#FFF3E6` | the icon background |

## Type

Headlines and text: **Inter**. Code: **JetBrains Mono**.

## Voice

Short sentences. Plain words. Say what something does, then show it. Errors explain how to fix the problem.
Tagline: **Small, quick, sharp.**

---

## Marka Finch (po polsku)

**Logo:** pliki jak w tabeli wyżej. Znak to sześciokąt w trzech odcieniach pomarańczu, oświetlony z lewej góry,
z białym gilem (finch) lecącym przez niego w prawo w górę: skrzydła odgięte do tyłu, krótki dziób do łuskania ziaren,
rozwidlony ogon. Rysuje go `make_logo.py`. `logo.svg` jest na jasne tło, `logo-dark.svg` na ciemne,
`logo-mark*.svg` to sam znak, `icon*` to ikona i awatar, a `social-preview.png` to podgląd linku na GitHubie.
Napis jest rysowany liniami (nie potrzebuje czcionki), małymi literami, z pomarańczową kropką nad **i**.
Zostaw wokół logo wolne miejsce co najmniej wysokości litery **n**. Nie zmieniaj kolorów znaku, nie obracaj
go, nie rozciągaj i nie kładź na pstrokatym tle; ptak zostaje biały.

**Kolory:** Finch Orange `#FF7A1A` (prawa ściana znaku, akcenty), Glow `#FFA24D` (oświetlona ściana), Ember `#D9570B` (dolna ściana, oko ptaka), Seed `#FFC145` (dziób),
Ink `#1B1F2A` (tekst, skrzydło, ciemne tło), Slate `#2E3445`, Cloud `#F5F3EF` (tekst na ciemnym), Shell `#FFF3E6` (tło ikony).

**Czcionki:** Inter do tekstu, JetBrains Mono do kodu.

**Styl wypowiedzi:** krótkie zdania, proste słowa. Najpierw powiedz, co coś robi, potem to pokaż.
Komunikaty błędów mówią, jak naprawić problem. Hasło: **Small, quick, sharp.** („Mały, zwinny, skuteczny”).
