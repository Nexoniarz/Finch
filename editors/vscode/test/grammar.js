// Tokenizes Finch code with the same engine VS Code uses and checks the scopes.
const fs = require('fs'), path = require('path');
const vsctm = require('vscode-textmate'), oniguruma = require('vscode-oniguruma');
const grammarPath = path.join(__dirname, "..", "syntaxes", "finch.tmLanguage.json");
const wasm = fs.readFileSync(require.resolve("vscode-oniguruma/release/onig.wasm")).buffer;
const onigLib = oniguruma.loadWASM(wasm).then(() => ({
  createOnigScanner: (s) => new oniguruma.OnigScanner(s),
  createOnigString: (s) => new oniguruma.OnigString(s),
}));
const registry = new vsctm.Registry({ onigLib, loadGrammar: async () => vsctm.parseRawGrammar(fs.readFileSync(grammarPath, 'utf8'), grammarPath) });
const code = [
  'import "stdio.h"',
  'import shapes',
  'struct Point {',
  '    int x = 0x1F',
  '}',
  'fn add(int a, float b) -> int {',
  '    s := "hi\\n" + str(a)  // note',
  '    for i in 0..10 { print(i, \'c\', 3.5, true, null) }',
  '    p := Point(x: 1)',
  '    return a + p.x',
  '}',
];
const expect = [  // [line, token text, scope that must be present]
  [0, 'import', 'keyword.control.import'], [0, '"stdio.h"', 'string.quoted.double'],
  [1, 'shapes', 'entity.name.namespace'],
  [2, 'struct', 'storage.type.struct'], [2, 'Point', 'entity.name.type.struct'],
  [3, 'int', 'storage.type.primitive'], [3, '0x1F', 'constant.numeric.hex'],
  [5, 'fn', 'storage.type.function'], [5, 'add', 'entity.name.function'], [5, 'float', 'storage.type.primitive'], [5, '->', 'keyword.operator.arrow'],
  [6, ':=', 'keyword.operator.assignment'], [6, '\\n', 'constant.character.escape'], [6, '// note', 'comment.line'], [6, 'str', 'storage.type.primitive'],
  [7, 'for', 'keyword.control'], [7, 'in', 'keyword.control'], [7, '..', 'keyword.operator.range'], [7, 'print', 'support.function.builtin'],
  [7, 'c', 'string.quoted.single'], [7, '3.5', 'constant.numeric.float'], [7, 'true', 'constant.language'], [7, 'null', 'constant.language'],
  [8, 'Point', 'entity.name.type'],
  [9, 'return', 'keyword.control'],
];
registry.loadGrammar('source.finch').then((g) => {
  let state = vsctm.INITIAL, toks = [];
  code.forEach((line, i) => {
    const r = g.tokenizeLine(line, state);
    r.tokens.forEach((t) => toks.push([i, line.substring(t.startIndex, t.endIndex), t.scopes.join(' ')]));
    state = r.ruleStack;
  });
  let bad = 0;
  for (const [line, text, scope] of expect) {
    const hit = toks.find((t) => t[0] === line && t[1].trim() === text);
    const ok = hit && hit[2].includes(scope);
    if (!ok) { bad++; console.log('FAIL', line, JSON.stringify(text), 'wanted', scope, 'got', hit ? hit[2] : '(no token)'); }
  }
  console.log(bad ? `grammar: ${bad} failed` : `grammar: all ${expect.length} ok`);
  process.exit(bad ? 1 : 0);
});
