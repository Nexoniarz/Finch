# Finch exercises

Learn Finch by doing: each file is a small task with tests that check it.

```sh
exercises/next.sh          # the first exercise that isn't solved yet, and what its tests say
exercises/next.sh all      # how far you are
finch test exercises/03_if_else.fch     # one exercise by itself
```

Open the exercise, read the few lines at the top, change the code marked `TODO` until its tests pass, then run
`exercises/next.sh` again. Don't change the `test` blocks: they are the checker. If you are stuck,
`exercises/solutions/` has one answer for each (there are often others).

| | Topic |
|---|---|
| 01–04 | values, functions, `if`, loops |
| 05–07 | lists, text, structs and methods |
| 08–11 | `mut`, maps, errors (`!`, `or`, `try`), values that may be missing (`T?`) |
| 12–15 | `enum` and `match`, functions as values, interfaces, generics |
| 16–18 | assembly (x86-64): registers, several values, a loop |

The guides in `docs/` explain everything in more detail when you want it; the cheat sheet (`docs/en/cheatsheet.md`)
is handy to keep open while you work.
