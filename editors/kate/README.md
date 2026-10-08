# Finch in Kate

Syntax highlighting and the Finch language server for **Kate** (and KWrite / KDevelop, which share
Kate's highlighting engine).

## Install

```sh
editors/kate/install.sh
```

It copies `finch.xml` to `~/.local/share/org.kde.syntax-highlighting/syntax/` and adds a `finch` entry
to `~/.config/kate/lspclient/settings.json` (your other servers are kept). Then:

1. Restart Kate.
2. **Settings → Configure Kate → Plugins**: enable **LSP Client** (and **Build & Run** for the next part).
3. Make sure `finch` is on your PATH (`finch version` in a terminal).

Open a `.fch` file: errors appear as you type, hovering shows types and docs, `Ctrl+Space` completes
(fields and methods after `.`), **Go to Definition** and the **Symbols** outline work, and parameter hints
pop up inside `(…)`.

## Running and building

**Build & Run** plugin → **Add target set**, then add targets with these commands (the working folder
can be `%d`, the folder of the current file):

| Target | Command |
|---|---|
| Run | `finch run %f` |
| Build | `finch build %f` |

Give one of them a shortcut in **Settings → Configure Keyboard Shortcuts** (search for "Build"). Finch's
errors have the `file:line:column: error:` form, so Kate lists them and jumps to the line on click.

## By hand

- Highlighting: copy `finch.xml` into `~/.local/share/org.kde.syntax-highlighting/syntax/`
  (Windows: `%LOCALAPPDATA%\org.kde.syntax-highlighting\syntax\`).
- Language server: **Settings → Configure Kate → LSP Client → User Server Settings**, and add the
  `finch` entry from `lspclient.json` inside `"servers"`.
