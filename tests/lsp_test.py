#!/usr/bin/env python3
"""Talks to `finch lsp` like an editor would and checks the answers."""
import json, os, pathlib, subprocess, sys, tempfile

FINCH = os.environ.get("FINCH", os.path.join(os.path.dirname(__file__), "..", "build", "finch"))
failures = 0

def check(name, cond, detail=""):
    global failures
    if cond:
        print("ok  ", name)
    else:
        failures += 1
        print("FAIL", name, detail)

class Client:
    def __init__(self):
        self.p = subprocess.Popen([FINCH, "lsp"], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        self.next_id = 0
        self.notes = []

    def send(self, msg):
        body = json.dumps(msg).encode()
        self.p.stdin.write(b"Content-Length: %d\r\n\r\n" % len(body) + body)
        self.p.stdin.flush()

    def read(self):
        length = 0
        while True:
            line = self.p.stdout.readline().decode().strip()
            if not line:
                break
            if line.startswith("Content-Length:"):
                length = int(line.split(":")[1])
        return json.loads(self.p.stdout.read(length))

    def request(self, method, params):
        self.next_id += 1
        self.send({"jsonrpc": "2.0", "id": self.next_id, "method": method, "params": params})
        while True:
            m = self.read()
            if m.get("id") == self.next_id:
                return m.get("result")
            self.notes.append(m)

    def notify(self, method, params):
        self.send({"jsonrpc": "2.0", "method": method, "params": params})

    def diagnostics(self, uri):
        # the server publishes right after didOpen/didChange; a request flushes the order
        self.request("textDocument/hover", {"textDocument": {"uri": uri}, "position": {"line": 0, "character": 0}})
        found = [n for n in self.notes if n.get("method") == "textDocument/publishDiagnostics" and n["params"]["uri"] == uri]
        self.notes = []
        return found[-1]["params"]["diagnostics"] if found else None

src = """struct Point {
    int x
    int y
}

fn add(int a, int b) -> int {
    return a + b
}

fn main() {
    p := Point(1, 2)
    total := add(p.x, p.y)
    names := ["a", "b"]
    print(total, names.len)
}
"""
lines = src.split("\n")
def pos(line_no, text):  # 0-based position of `text` in a 1-based line
    return {"line": line_no - 1, "character": lines[line_no - 1].index(text)}

d = tempfile.mkdtemp()
path = os.path.join(d, "demo.fch")
open(path, "w").write(src)
uri = pathlib.Path(path).resolve().as_uri()
td = {"uri": uri}

c = Client()
init = c.request("initialize", {"processId": None, "rootUri": None, "capabilities": {}})
check("initialize", init["capabilities"]["hoverProvider"] is True)
c.notify("initialized", {})
c.notify("textDocument/didOpen", {"textDocument": {"uri": uri, "languageId": "finch", "version": 1, "text": src}})
check("no errors in a good file", c.diagnostics(uri) == [])

h = c.request("textDocument/hover", {"textDocument": td, "position": pos(12, "total")})
check("hover: variable type", h and "int total" in h["contents"]["value"], h)
h = c.request("textDocument/hover", {"textDocument": td, "position": pos(12, "add")})
check("hover: function signature", h and "fn add(int a, int b) -> int" in h["contents"]["value"], h)
h = c.request("textDocument/hover", {"textDocument": td, "position": pos(14, "print")})
check("hover: built-in docs", h and "Prints the values" in h["contents"]["value"], h)

defn = c.request("textDocument/definition", {"textDocument": td, "position": pos(12, "add")})
check("definition: function", defn and defn["range"]["start"]["line"] == 5, defn)
defn = c.request("textDocument/definition", {"textDocument": td, "position": pos(12, "p.x")})
check("definition: variable", defn and defn["range"]["start"]["line"] == 10, defn)

# completion after "p." on a new line inside main
edited = src.replace("    print(total, names.len)\n", "    print(total, names.len)\n    p.\n")
c.notify("textDocument/didChange", {"textDocument": {"uri": uri, "version": 2}, "contentChanges": [{"text": edited}]})
comp = c.request("textDocument/completion", {"textDocument": td, "position": {"line": 14, "character": 6}})
labels = [i["label"] for i in comp["items"]]
check("completion: struct fields after p.", "x" in labels and "y" in labels, labels)
comp = c.request("textDocument/completion", {"textDocument": td, "position": {"line": 14, "character": 4}})
labels = [i["label"] for i in comp["items"]]
check("completion: variables, functions, keywords", all(x in labels for x in ["total", "names", "add", "Point", "while", "print"]), labels[:30])

edited2 = src.replace("    print(total, names.len)\n", "    print(total, names.len)\n    names.\n")
c.notify("textDocument/didChange", {"textDocument": {"uri": uri, "version": 3}, "contentChanges": [{"text": edited2}]})
comp = c.request("textDocument/completion", {"textDocument": td, "position": {"line": 14, "character": 10}})
labels = [i["label"] for i in comp["items"]]
check("completion: array methods", "push" in labels and "sort" in labels, labels)

sig_src = src.replace("    print(total, names.len)\n", "    print(total, names.len)\n    add(1, \n")
c.notify("textDocument/didChange", {"textDocument": {"uri": uri, "version": 4}, "contentChanges": [{"text": sig_src}]})
sig = c.request("textDocument/signatureHelp", {"textDocument": td, "position": {"line": 14, "character": 11}})
check("signature help", sig and sig["signatures"][0]["label"].startswith("fn add") and sig["activeParameter"] == 1, sig)

c.notify("textDocument/didChange", {"textDocument": {"uri": uri, "version": 5}, "contentChanges": [{"text": src}]})
syms = c.request("textDocument/documentSymbol", {"textDocument": td})
names = sorted(s["name"] for s in syms)
check("document symbols", names == ["Point", "add", "main"], names)

bad = src.replace("total := add(p.x, p.y)", 'total := add(p.x, "y")')
c.notify("textDocument/didChange", {"textDocument": {"uri": uri, "version": 6}, "contentChanges": [{"text": bad}]})
diags = c.diagnostics(uri)
check("error shown while typing", diags and "must be int, but this is str" in diags[0]["message"] and diags[0]["range"]["start"]["line"] == 11, diags)
c.notify("textDocument/didChange", {"textDocument": {"uri": uri, "version": 7}, "contentChanges": [{"text": src}]})
check("error cleared after the fix", c.diagnostics(uri) == [])

c.request("shutdown", None)
c.notify("exit", None)
c.p.wait(timeout=5)
check("clean exit", c.p.returncode == 0, c.p.returncode)
print("lsp:", "all ok" if failures == 0 else f"{failures} failed")
sys.exit(1 if failures else 0)
