# Changelog

## 2.5.0

- Highlighting for code inside text (`"hp \(hp)"`), `test "…" { }` blocks, `const`, `"""` text, `1e9`.
- Built-ins `min`, `max`, `abs`, `clamp`, `assert` and `atomic_…` highlighted and described on hover.
- Format Document (the language server runs `finch fmt` on the open file).
- Warnings in yellow: a big struct copied on every call or in `for x in list`.

## 2.4.0

- Highlighting for methods (`fn Point.move`), maps (`map[K]V`), `or`, `try`, `self` and `error(...)`.
- Completion for struct methods and map methods; parameter hints for method calls.
- Snippets: `method`, `fnf` (a function that can fail), `map`, `forkv`, `orb`.

## 2.3.0

- First release: highlighting, snippets, language server, Run and Build.
