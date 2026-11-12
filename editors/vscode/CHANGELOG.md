# Changelog

## 2.7.0

- Functions as values: `fn(int) -> bool` types and `fn(a, b) { ... }` highlighted; hover shows a function value's type.
- Completion and hover for the new list methods `sort_by`, `filter`, `map`, `any`, `all`.
- `mut` (parameters that change the caller's variable) highlighted and completed; `asm(...)` highlighted and described.
- Snippets: `fnv` (a function value), `sortby`, `fnm` (a function with a mut parameter), `asm`, `interface`, `ifv`, `fno`.
- Interfaces: `interface Shape { ... }` highlighted, in the outline, and their methods completed after `shape.`.
- Optional values: `T?` types, `if v := x { }`.
- `deref(p)` highlighted and described on hover.
- Errors in the terminal come with the line of code, a `^` under the spot and a `help:` line (the Problems panel is unchanged).

## 2.6.0

- Vector types (`f32x8`, `i8x32`, `f64x4`, …) and masks (`boolx8`) highlighted as types; completion of their methods (`sum`, `dot_add`, `shuffle`, `lookup`, `store`, …).
- `@cpu(...)` and the new built-ins (`select`, `concat`, `fma`, `muladd`, `cpu_has`, `f32_bits`, `f16_to_f32`, …) highlighted.

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
