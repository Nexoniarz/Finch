#!/usr/bin/env bash
# Installs Finch support for Kate (also KWrite and KDevelop):
#   - syntax highlighting:  ~/.local/share/org.kde.syntax-highlighting/syntax/finch.xml
#   - the language server:  adds "finch" to ~/.config/kate/lspclient/settings.json
# Then restart Kate and enable Settings → Configure Kate → Plugins → "LSP Client".
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
data="${XDG_DATA_HOME:-$HOME/.local/share}/org.kde.syntax-highlighting/syntax"
conf="${XDG_CONFIG_HOME:-$HOME/.config}/kate/lspclient"
mkdir -p "$data" "$conf"
cp "$here/finch.xml" "$data/finch.xml"
echo "highlighting: $data/finch.xml"

# merge into the user's LSP settings instead of replacing them
python3 - "$conf/settings.json" "$here/lspclient.json" <<'PY'
import json, os, sys
path, ours = sys.argv[1], json.load(open(sys.argv[2]))
cur = {}
if os.path.exists(path) and os.path.getsize(path) > 0:
    cur = json.load(open(path))
cur.setdefault("servers", {}).update(ours["servers"])
with open(path, "w") as f:
    json.dump(cur, f, indent=4)
    f.write("\n")
print("language server: " + path)
PY
command -v finch > /dev/null || echo "note: 'finch' isn't on your PATH yet; Kate starts the language server with 'finch lsp'"
