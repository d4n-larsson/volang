// The node side of tests/grammars.py (spec 037): the TextMate grammar against the tree-sitter grammar.
//
//   node check.js agree PATHS_FILE     every file listed: at every token, the two grammars give the same class
//   node check.js recover              FR-005: a mistake on one line leaves the following lines highlighted
//   node check.js time FILE            seconds to tokenize FILE with the TextMate grammar
//
// Classes compared: comment, string, escape, integer, float, boolean, keyword, builtin type, builtin
// function, type name, effect, function definition, annotation. Everything else (plain names, calls,
// operators, punctuation) is "none" on both sides.
'use strict';
const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');
const vsctm = require('vscode-textmate');
const oniguruma = require('vscode-oniguruma');

const HERE = __dirname;
const TS_DIR = path.join(HERE, 'tree-sitter-vo');
const TS = path.join(HERE, 'node_modules', '.bin', 'tree-sitter');

const TM_CLASS = [ // innermost scope prefix -> class (first match)
  ['comment.', 'comment'], ['constant.character.escape', 'escape'], ['invalid.', 'INVALID'], ['string.', 'string'],
  ['constant.numeric.float', 'float'], ['constant.numeric.integer', 'integer'], ['constant.language', 'boolean'],
  ['keyword.operator.symbol', null], ['keyword.', 'keyword'], ['storage.', 'keyword'],
  ['support.type', 'builtin type'], ['support.function', 'builtin function'], ['support.constant.effect', 'effect'],
  ['entity.name.type', 'type name'], ['entity.name.function.definition', 'function definition'],
  ['entity.name.tag.annotation', 'annotation'],
];
const TS_CLASS = { // capture -> class
  'comment': 'comment', 'string': 'string', 'string.escape': 'escape', 'number': 'integer', 'number.float': 'float',
  'boolean': 'boolean', 'type.builtin': 'builtin type', 'function.builtin': 'builtin function',
  'constant.builtin': 'effect', 'type': 'type name', 'function': 'function definition', 'attribute': 'annotation',
};
const tsClass = name => name.startsWith('keyword') ? 'keyword' : (Object.hasOwn(TS_CLASS, name) ? TS_CLASS[name] : null);
function tmClass(scopes) {
  const inner = scopes[scopes.length - 1];
  for (const [prefix, cls] of TM_CLASS) if (inner.startsWith(prefix)) return cls;
  return null;
}

async function grammar() {
  const wasm = fs.readFileSync(path.join(HERE, 'node_modules', 'vscode-oniguruma', 'release', 'onig.wasm')).buffer;
  await oniguruma.loadWASM(wasm);
  const registry = new vsctm.Registry({
    onigLib: Promise.resolve({
      createOnigScanner: sources => new oniguruma.OnigScanner(sources),
      createOnigString: s => new oniguruma.OnigString(s),
    }),
    loadGrammar: async () => vsctm.parseRawGrammar(
      fs.readFileSync(path.join(HERE, 'vscode', 'syntaxes', 'vo.tmLanguage.json'), 'utf8'), 'vo.tmLanguage.json'),
  });
  return registry.loadGrammar('source.vo');
}

// Per line: [{start, end, cls}] in bytes, for the tokens that are not only white space.
function tmTokens(g, text) {
  const lines = text.split('\n');
  let stack = vsctm.INITIAL;
  return lines.map(line => {
    const r = g.tokenizeLine(line, stack);
    stack = r.ruleStack;
    const byte = i => Buffer.byteLength(line.slice(0, i));
    return r.tokens.filter(t => line.slice(t.startIndex, t.endIndex).trim() !== '')
      .map(t => ({ start: byte(t.startIndex), end: byte(t.endIndex), cls: tmClass(t.scopes), scopes: t.scopes }));
  });
}

// file -> [{row, start, end, cls}] from `tree-sitter query highlights.scm`, one entry per row of a capture.
function tsCaptures(pathsFile) {
  const out = execFileSync(TS, ['query', 'queries/highlights.scm', '--paths', pathsFile],
    { cwd: TS_DIR, maxBuffer: 1 << 30, stdio: ['ignore', 'pipe', 'ignore'] }).toString();
  const byFile = new Map();
  let cur = null;
  // (a capture over several rows is printed without its index and text)
  const re = /^\s+capture: (?:\d+ - )?([\w.]+), start: \((\d+), (\d+)\), end: \((\d+), (\d+)\)/;
  for (const line of out.split('\n')) {
    const m = re.exec(line);
    if (m) {
      if (cur && tsClass(m[1])) { // a capture over several rows (a raw string) covers each of them
        for (let row = +m[2]; row <= +m[4]; row++) {
          cur.push({ row, start: row === +m[2] ? +m[3] : 0, end: row === +m[4] ? +m[5] : Infinity, cls: tsClass(m[1]) });
        }
      }
    } else if (line && !/^\s/.test(line) && fs.existsSync(line)) {
      byFile.set(line, cur = []);
    }
  }
  return byFile;
}

// The class of the innermost capture or token covering byte column `at`.
function covering(list, at) {
  let best = null;
  for (const t of list) if (t.start <= at && at < t.end && (!best || t.end - t.start < best.end - best.start)) best = t;
  return best ? best.cls : null;
}

async function agree(pathsFile) {
  const g = await grammar();
  const files = fs.readFileSync(pathsFile, 'utf8').split('\n').filter(Boolean);
  const captures = tsCaptures(pathsFile);
  let problems = 0, tokens = 0;
  const report = (file, row, col, text, tm, ts) => {
    if (problems++ < 20) {
      console.log(`${path.relative(path.join(HERE, '..'), file)}:${row + 1}:${col + 1}: \`${text}\`: ` +
        `TextMate says ${tm || 'none'}, tree-sitter says ${ts || 'none'}`);
    }
  };
  for (const file of files) {
    const text = fs.readFileSync(file, 'utf8');
    const lines = text.split('\n');
    const tm = tmTokens(g, text);
    const ts = [];
    for (const c of captures.get(file) || []) (ts[c.row] = ts[c.row] || []).push(c);
    for (let row = 0; row < lines.length; row++) {
      const a = tm[row], b = ts[row] || [];
      const starts = new Set([...a.map(t => t.start), ...b.map(t => t.start)]);
      const bytes = Buffer.from(lines[row]);
      for (const at of [...starts].sort((x, y) => x - y)) {
        const x = covering(a, at), y = covering(b, at);
        tokens++;
        if (x !== y) report(file, row, at, bytes.subarray(at, at + 24).toString().split(/\s/)[0], x, y);
      }
    }
  }
  if (problems > 20) console.log(`... and ${problems - 20} more`);
  console.log(`agree: ${files.length} files, ${tokens} positions, ${problems} differences`);
  process.exit(problems ? 1 : 0);
}

async function recover() {
  // an unclosed string, a bad escape and a half-typed function; then ordinary code
  const text = 'fn broken( {\n    let s = "not closed;\n    let t = "bad \\q escape";\n}\n' +
    'fn fine(x: int) -> str uses console {\n    return str(x);\n}\n';
  const g = await grammar();
  const tm = tmTokens(g, text);
  const want = [['fn', 'keyword'], ['fine', 'function definition'], ['int', 'builtin type'], ['str', 'builtin type'],
    ['uses', 'keyword'], ['console', 'effect']];
  const line = text.split('\n')[4];
  let bad = 0;
  for (const [word, cls] of want) {
    const got = covering(tm[4], Buffer.byteLength(line.slice(0, line.indexOf(word))));
    if (got !== cls) { bad++; console.log(`TextMate after a mistake: \`${word}\` is ${got}, expected ${cls}`); }
  }
  if (!tm[2].some(t => t.cls === 'INVALID')) { bad++; console.log('TextMate: the bad escape is not marked invalid'); }
  const tmp = path.join(fs.mkdtempSync(path.join(require('os').tmpdir(), 'vo-grammar-')), 'broken.vo');
  fs.writeFileSync(tmp, text);
  let tree = '';
  try {
    tree = execFileSync(TS, ['parse', tmp], { cwd: TS_DIR, stdio: ['ignore', 'pipe', 'ignore'] }).toString();
  } catch (e) { tree = e.stdout.toString(); } // exit status 1: the tree has errors, as it should
  fs.rmSync(path.dirname(tmp), { recursive: true });
  if (!/function_definition \[4, 0\]/.test(tree)) { bad++; console.log('tree-sitter: the function after the mistake is not a function_definition'); }
  if (!/ERROR|MISSING/.test(tree)) { bad++; console.log('tree-sitter: the broken function has no error node'); }
  console.log(`recover: ${bad} problems`);
  process.exit(bad ? 1 : 0);
}

async function time(file) {
  const g = await grammar();
  const text = fs.readFileSync(file, 'utf8');
  const t = process.hrtime.bigint();
  tmTokens(g, text);
  console.log((Number(process.hrtime.bigint() - t) / 1e9).toFixed(3));
}

const [cmd, arg] = process.argv.slice(2);
({ agree, recover, time })[cmd](arg).catch(e => { console.error(e); process.exit(2); });
