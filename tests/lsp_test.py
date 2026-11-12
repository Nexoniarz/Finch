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
first = c.diagnostics(uri)
check("no errors in a good file", first == [], first)

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

messy = "fn main() {\n  x := 1   \n\n\n\tprint(x)\n}"
c.notify("textDocument/didChange", {"textDocument": {"uri": uri, "version": 5}, "contentChanges": [{"text": messy}]})
fm = c.request("textDocument/formatting", {"textDocument": td, "options": {"tabSize": 4, "insertSpaces": True}})
check("formatting", fm and fm[0]["newText"] == "fn main() {\n    x := 1\n\n    print(x)\n}\n", fm)
c.notify("textDocument/didChange", {"textDocument": {"uri": uri, "version": 5}, "contentChanges": [{"text": src}]})

bad = src.replace("total := add(p.x, p.y)", 'total := add(p.x, "y")')
c.notify("textDocument/didChange", {"textDocument": {"uri": uri, "version": 6}, "contentChanges": [{"text": bad}]})
diags = c.diagnostics(uri)
check("error shown while typing", diags and "must be int, but this is str" in diags[0]["message"] and diags[0]["range"]["start"]["line"] == 11, diags)
c.notify("textDocument/didChange", {"textDocument": {"uri": uri, "version": 7}, "contentChanges": [{"text": src}]})
check("error cleared after the fix", c.diagnostics(uri) == [])

# methods, maps, errors as values
src2 = """struct Counter {
    int count
}

fn Counter.add(int n) {
    self.count += n
}

fn parse(str s) -> int! {
    return int(s) or { return error("bad") }
}

fn main() {
    c := Counter(0)
    c.add(2)
    map[str]int m
    n := parse("4") or 0
    print(c, m, n)
}
"""
lines2 = src2.split("\n")
path2 = os.path.join(d, "more.fch")
open(path2, "w").write(src2)
uri2 = pathlib.Path(path2).resolve().as_uri()
td2 = {"uri": uri2}
c.notify("textDocument/didOpen", {"textDocument": {"uri": uri2, "languageId": "finch", "version": 1, "text": src2}})
check("methods, maps and errors compile", c.diagnostics(uri2) == [])
h = c.request("textDocument/hover", {"textDocument": td2, "position": {"line": 14, "character": lines2[14].index("add")}})
check("hover: method signature", h and "fn Counter.add(int n)" in h["contents"]["value"], h)
h = c.request("textDocument/hover", {"textDocument": td2, "position": {"line": 16, "character": lines2[16].index("parse")}})
check("hover: fallible signature", h and "-> int!" in h["contents"]["value"], h)
defn = c.request("textDocument/definition", {"textDocument": td2, "position": {"line": 14, "character": lines2[14].index("add")}})
check("definition: method", defn and defn["range"]["start"]["line"] == 4, defn)

for text, line, col, want, what in [
        ("    c.\n", 18, 6, ["add", "count"], "completion: methods and fields after c."),
        ("    m.\n", 18, 6, ["has", "get", "keys", "remove"], "completion: map methods"),
]:
    e2 = src2.replace("    print(c, m, n)\n", "    print(c, m, n)\n" + text)
    c.notify("textDocument/didChange", {"textDocument": {"uri": uri2, "version": 2}, "contentChanges": [{"text": e2}]})
    comp = c.request("textDocument/completion", {"textDocument": td2, "position": {"line": line, "character": col}})
    labels = [i["label"] for i in comp["items"]]
    check(what, all(x in labels for x in want), labels)

e2 = src2.replace("    print(c, m, n)\n", "    print(c, m, n)\n    c.add(\n")
c.notify("textDocument/didChange", {"textDocument": {"uri": uri2, "version": 3}, "contentChanges": [{"text": e2}]})
sig = c.request("textDocument/signatureHelp", {"textDocument": td2, "position": {"line": 18, "character": 10}})
check("signature help: method", sig and sig["signatures"][0]["label"].startswith("fn Counter.add"), sig)

e2 = src2.replace('n := parse("4") or 0', 'n := parse("4")')
c.notify("textDocument/didChange", {"textDocument": {"uri": uri2, "version": 4}, "contentChanges": [{"text": e2}]})
diags = c.diagnostics(uri2)
check("error: unhandled failure", diags and "can fail" in diags[0]["message"], diags)

# a big struct copied on every call: a warning (yellow), not an error
src4 = "struct V {\n" + "".join("    float f%d\n" % i for i in range(40)) + "}\n\nfn len2(V v) -> float {\n    return v.f0\n}\n\nfn main() {\n    print(len2(V()))\n}\n"
path4 = os.path.join(d, "big.fch")
open(path4, "w").write(src4)
uri4 = pathlib.Path(path4).resolve().as_uri()
c.notify("textDocument/didOpen", {"textDocument": {"uri": uri4, "languageId": "finch", "version": 1, "text": src4}})
w4 = c.diagnostics(uri4)
check("warning for a big copy", w4 and len(w4) == 1 and w4[0]["severity"] == 2 and "copied on every call" in w4[0]["message"] and w4[0]["range"]["start"]["line"] == 43, w4)

# a module imported under another name: completion after its name
src3 = "import path as pt\n\nfn main() {\n    print(pt.base(\"a/b\"))\n}\n"
path3 = os.path.join(d, "alias.fch")
open(path3, "w").write(src3)
uri3 = pathlib.Path(path3).resolve().as_uri()
c.notify("textDocument/didOpen", {"textDocument": {"uri": uri3, "languageId": "finch", "version": 1, "text": src3}})
check("import as: compiles", c.diagnostics(uri3) == [])
e3 = src3.replace("    print(", "    pt.\n    print(")
c.notify("textDocument/didChange", {"textDocument": {"uri": uri3, "version": 2}, "contentChanges": [{"text": e3}]})
comp = c.request("textDocument/completion", {"textDocument": {"uri": uri3}, "position": {"line": 3, "character": 7}})
labels = [i["label"] for i in comp["items"]]
ok3 = "base" in labels and "join" in labels
if not ok3:  # show what the compiler's index says about the module, to see where the names differ
    import subprocess
    idx = subprocess.run([FINCH, "index", path3], capture_output=True, text=True).stdout
    labels = [l for l in idx.splitlines() if l.startswith("@I") or (l.startswith("@F") and "base" in l)][:6]
check("completion: a module under another name", ok3, labels)

# mut parameters and functions as values
src5 = """fn grow(mut []int list, int n) {
    list.push(n)
}

fn main() {
    nums := [3, 1, 2]
    grow(mut nums, 4)
    double := fn(int x) -> int { return x * 2 }
    nums.sort_by(fn(a, b) { return a < b })
    print(nums.map(double), double(5))
}
"""
lines5 = src5.split("\n")
path5 = os.path.join(d, "fnvalues.fch")
open(path5, "w").write(src5)
uri5 = pathlib.Path(path5).resolve().as_uri()
td5 = {"uri": uri5}
c.notify("textDocument/didOpen", {"textDocument": {"uri": uri5, "languageId": "finch", "version": 1, "text": src5}})
check("mut and function values compile", c.diagnostics(uri5) == [])
h = c.request("textDocument/hover", {"textDocument": td5, "position": {"line": 9, "character": lines5[9].index("double(5)")}})
check("hover: a variable holding a function", h and "fn(int) -> int double" in h["contents"]["value"], h)
h = c.request("textDocument/hover", {"textDocument": td5, "position": {"line": 6, "character": lines5[6].index("grow")}})
check("hover: mut in a signature", h and "fn grow(mut []int list, int n)" in h["contents"]["value"], h)
syms = c.request("textDocument/documentSymbol", {"textDocument": td5})
names5 = [x["name"] for x in syms]
check("document symbols leave out function values", names5 == ["grow", "main"], names5)
e5 = src5.replace("    print(nums.map(double), double(5))\n", "    print(nums.map(double), double(5))\n    nums.\n")
c.notify("textDocument/didChange", {"textDocument": {"uri": uri5, "version": 2}, "contentChanges": [{"text": e5}]})
comp = c.request("textDocument/completion", {"textDocument": td5, "position": {"line": 10, "character": 9}})
labels = [i["label"] for i in comp["items"]]
check("completion: sort_by, filter, map, any, all", all(x in labels for x in ["sort_by", "filter", "map", "any", "all"]), labels)
e5 = src5.replace("{ return x * 2 }", "{ nums.push(x)\n return x * 2 }")
c.notify("textDocument/didChange", {"textDocument": {"uri": uri5, "version": 3}, "contentChanges": [{"text": e5}]})
diags = c.diagnostics(uri5)
check("error: a captured copy can't be changed", diags and "is a copy this function value took" in diags[0]["message"], diags)

# interfaces and optional values
src6 = """interface Shape {
    fn area() -> float
}

struct Sq {
    float s
}

fn Sq.area() -> float {
    return self.s * self.s
}

fn main() {
    Shape sh = Sq(2.0)
    int? maybe = 3
    print(sh.area(), maybe or 0)
}
"""
lines6 = src6.split("\n")
path6 = os.path.join(d, "shapes.fch")
open(path6, "w").write(src6)
uri6 = pathlib.Path(path6).resolve().as_uri()
td6 = {"uri": uri6}
c.notify("textDocument/didOpen", {"textDocument": {"uri": uri6, "languageId": "finch", "version": 1, "text": src6}})
check("interfaces and optionals compile", c.diagnostics(uri6) == [])
h = c.request("textDocument/hover", {"textDocument": td6, "position": {"line": 15, "character": lines6[15].index("maybe")}})
check("hover: an optional variable", h and "int? maybe" in h["contents"]["value"], h)
syms = c.request("textDocument/documentSymbol", {"textDocument": td6})
names6 = [x["name"] for x in syms]
check("document symbols: the interface", "Shape" in names6 and "Sq" in names6, names6)
e6 = src6.replace("    print(sh.area(), maybe or 0)\n", "    print(sh.area(), maybe or 0)\n    sh.\n")
c.notify("textDocument/didChange", {"textDocument": {"uri": uri6, "version": 2}, "contentChanges": [{"text": e6}]})
comp = c.request("textDocument/completion", {"textDocument": td6, "position": {"line": 16, "character": 7}})
labels = [i["label"] for i in comp["items"]]
check("completion: an interface's methods", "area" in labels, labels)

c.request("shutdown", None)
c.notify("exit", None)
c.p.wait(timeout=5)
check("clean exit", c.p.returncode == 0, c.p.returncode)
print("lsp:", "all ok" if failures == 0 else f"{failures} failed")
sys.exit(1 if failures else 0)
