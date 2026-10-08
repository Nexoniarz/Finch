# Finch for Visual Studio Code

Support for the [Finch](https://github.com/Nexoniarz/Finch) programming language (`.fch` files):

- **Syntax highlighting**, bracket matching, comments with `Ctrl+/`
- **Errors as you type**: the real compiler checks the file (`finch lsp`)
- **Completion**: variables, functions, structs, keywords, built-ins, struct fields and array/str methods after `.`, names from imported C headers
- **Hover** shows types and signatures; **Go to Definition** (F12); **Outline** of functions and structs; **parameter hints**
- **Run** button (▶ in the editor title, or `Ctrl+F5`) and **Finch: Build This File**; compile errors land in the Problems panel
- Snippets: `main`, `fn`, `fnr`, `struct`, `if`, `ife`, `for`, `foreach`, `while`, `input`, …

## Requirements

Finch 2.3 or newer. The extension runs `finch` from your PATH; if it lives somewhere else,
set **Finch: Path** (`finch.path`) to the full path of `finch` / `finch.exe`.
