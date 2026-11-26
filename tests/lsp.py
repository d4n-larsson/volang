#!/usr/bin/env python3
"""Spec 038: `vo lsp`, checked by a scripted editor (specs/038-vo-lsp/contracts/lsp.md).

Every answer is compared with the command it stands for, run on a copy of the project that holds the
editor's texts on disk: diagnostics with `vo check --json`, quick fixes with the toolchain's fixes, hover,
definition and prepare-rename with `vo query type-at`, references with `vo query refs`, rename with
`vo edit rename`, formatting with `vo fmt`. Then: unsaved text, several files, the newest text winning,
bad input of every kind, cancellation, the time limit, and that nothing is written or run.

  lsp.py                 the checks (part of `make test`)
  lsp.py --session N     SC-004: the server's memory over N edits and requests
  lsp.py --time          SC-003: how long diagnostics and hover take (timing-sensitive)
"""
import json
import os
import random
import select
import shutil
import signal
import statistics
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
SLOW = 20 if any(a in os.environ.get("VO_BIN", "") for a in ("aarch64", "riscv64")) else 1  # under qemu

MAIN = '''import lib.shapes;

# Prints a few areas.
fn main() uses console {
    let label = "área 𝄞";
    let a = shapes.area(2, 3);
    print(label + ": " + str(a));
    print(shapes.describe(shapes.Shape.square(2)));
}
'''
SHAPES = '''# A shape with its size.
export variant Shape {
    # A square with this side.
    square(int);
    # A circle with this radius.
    circle(int);
}

# A size in two directions.
export record Size {
    # Left to right.
    w: int;
    h: int;
}

# The area of a rectangle.
#     area(2, 3) is 6
export fn area(w: int, h: int) -> int {
    return w * h;
}

export fn describe(s: Shape) -> str {
    match s {
        case square(n) {
            return "square " + str(n);
        }
        case circle(r) {
            return "circle " + str(r);
        }
    }
}

export const limit = 10;

test "area of a rectangle" {
    expect_eq(area(2, 3), 6);
}
'''
ALONE = 'fn main() uses console, file.write {\n    print("alone");\n    let _ = write_file("ran.txt", "x");\n}\n'
FILES = {"main.vo": MAIN, "lib/shapes.vo": SHAPES, "alone.vo": ALONE}
# main.vo with: a type error after non-ASCII text (fixable), an unused local (a warning), a missing `;`
BROKEN = MAIN.replace('let a = shapes.area(2, 3);', 'let a = shapes.area(2, 3);\n    let extra = "é𝄞" + 1;\n    let unused = 4;\n    print("x")')
UGLY = MAIN.replace("    let a = shapes.area(2, 3);", "  let a=shapes.area( 2,3 );")

fails = []


def fail(case, why):
    fails.append(case)
    print(f"FAIL {case}: {why}"[:1500])


def project(work, name, files=FILES):
    d = os.path.join(work, name)
    for rel, text in files.items():
        os.makedirs(os.path.dirname(os.path.join(d, rel)), exist_ok=True)
        open(os.path.join(d, rel), "w", encoding="utf-8").write(text)
    if "main.vo" in files:  # the reviewed effects record, as a real project has
        subprocess.run([VO, "effects", "--update", "main.vo"], cwd=d, capture_output=True)
    return d


def direct(args, cwd, stdin=b""):
    r = subprocess.run([VO] + args, cwd=cwd, input=stdin, capture_output=True, timeout=120 * SLOW)
    return [json.loads(line) for line in r.stdout.decode().splitlines() + r.stderr.decode().splitlines()
            if line.startswith("{")], r.returncode


def units(text, utf8):
    return len(text.encode("utf-8")) if utf8 else len(text.encode("utf-16-le")) // 2


def to_protocol(text, pos, utf8=False):
    """A record's {"line", "col"} (1-based, code points) as the protocol's position in text."""
    lines = text.split("\n")
    line = lines[pos["line"] - 1].rstrip("\r") if pos["line"] - 1 < len(lines) else ""
    return {"line": pos["line"] - 1, "character": units(line[:pos["col"] - 1], utf8)}


def rng(text, rec, utf8=False):
    return {"start": to_protocol(text, rec["start"], utf8), "end": to_protocol(text, rec["end"], utf8)}


def offset(text, pos, utf8=False):
    lines = text.split("\n")
    if pos["line"] >= len(lines):
        return len(text)
    line, k = lines[pos["line"]], 0
    while k < len(line) and units(line[:k], utf8) < pos["character"]:
        k += 1
    return sum(len(x) + 1 for x in lines[:pos["line"]]) + k


def apply_edits(text, edits, utf8=False):
    for e in sorted(edits, key=lambda e: (e["range"]["start"]["line"], e["range"]["start"]["character"]), reverse=True):
        text = text[:offset(text, e["range"]["start"], utf8)] + e["newText"] + text[offset(text, e["range"]["end"], utf8):]
    return text


class Editor:
    """A scripted editor: `ws` is the workspace the server works in; `twin` is a copy whose files hold
    the editor's texts, for the commands the answers are compared with."""

    def __init__(self, work, name, files=FILES, init=True, utf8=False, env=None):
        self.ws, self.twin = project(work, name, files), project(work, name + "-twin", files)
        self.utf8, self.texts, self.versions = utf8, {}, {}
        self.p = subprocess.Popen([VO, "lsp"], cwd=self.ws, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  env=dict(os.environ, **(env or {})))
        self.buf, self.next, self.notes = b"", 100, []
        if init:
            caps = {"general": {"positionEncodings": ["utf-8", "utf-16"]}} if utf8 else {}
            self.init = self.request("initialize", {"processId": None, "rootUri": self.uri(""), "capabilities": caps})
            self.notify("initialized", {})

    def uri(self, rel):
        return "file://" + os.path.join(self.ws, rel).rstrip("/")

    def rel(self, uri):
        return os.path.relpath(uri[len("file://"):], self.ws)

    def raw(self, data):
        self.p.stdin.write(data)
        self.p.stdin.flush()

    def send(self, obj):
        body = json.dumps(obj).encode()
        self.raw(b"Content-Length: %d\r\n\r\n" % len(body) + body)

    def notify(self, method, params):
        self.send({"jsonrpc": "2.0", "method": method, "params": params})

    def read(self, timeout=20):
        """The next message, or None at a timeout or the end of output."""
        end = time.time() + timeout * SLOW
        while True:
            i = self.buf.find(b"\r\n\r\n")
            if i >= 0:
                n = int(self.buf[:i].decode().lower().split("content-length:")[1].split()[0])
                if len(self.buf) >= i + 4 + n:
                    body, self.buf = self.buf[i + 4:i + 4 + n], self.buf[i + 4 + n:]
                    msg = json.loads(body.decode("utf-8"))
                    assert msg.get("jsonrpc") == "2.0"
                    return msg
            left = end - time.time()
            if left <= 0 or not select.select([self.p.stdout], [], [], left)[0]:
                return None
            chunk = os.read(self.p.stdout.fileno(), 1 << 20)
            if not chunk:
                return None
            self.buf += chunk

    def ask(self, method, params):
        self.next += 1
        self.send({"jsonrpc": "2.0", "id": self.next, "method": method, "params": params})
        return self.next

    def answer(self, i, timeout=20):
        """The answer to request i; notifications that come first are kept."""
        while True:
            msg = self.read(timeout)
            assert msg is not None, f"no answer to request {i}"
            if "id" in msg and "method" not in msg:
                assert msg["id"] == i, f"answer {msg.get('id')} while waiting for {i}"
                return msg
            self.notes.append(msg)

    def request(self, method, params, timeout=20):
        return self.answer(self.ask(method, params), timeout)

    def result(self, method, rel, extra=None):
        msg = self.request(method, dict({"textDocument": {"uri": self.uri(rel)}}, **(extra or {})))
        return msg.get("result") if "error" not in msg else msg

    def set(self, rel, text, open_=True):
        """The file's text: in the editor (opened or changed, not saved) or only on the workspace's disk;
        always on the twin's disk."""
        path = os.path.join(self.twin, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        open(path, "w", encoding="utf-8").write(text)
        if not open_:
            open(os.path.join(self.ws, rel), "w", encoding="utf-8").write(text)
            return
        self.versions[rel] = self.versions.get(rel, 0) + 1
        if rel in self.texts:
            self.notify("textDocument/didChange", {"textDocument": {"uri": self.uri(rel), "version": self.versions[rel]},
                                                    "contentChanges": [{"text": text}]})
        else:
            self.notify("textDocument/didOpen", {"textDocument": {"uri": self.uri(rel), "languageId": "vo",
                                                                  "version": self.versions[rel], "text": text}})
        self.texts[rel] = text

    def text(self, rel):
        return self.texts[rel] if rel in self.texts else open(os.path.join(self.ws, rel), encoding="utf-8").read()

    def diagnostics(self, rel, timeout=10):
        """The diagnostics published for rel's current version (an unopened file: the next publish)."""
        want = self.versions.get(rel) if rel in self.texts else None
        end = time.time() + timeout * SLOW
        while True:
            for k, m in enumerate(self.notes):
                p = m.get("params", {})
                if m.get("method") == "textDocument/publishDiagnostics" and p.get("uri") == self.uri(rel) and p.get("version") == want:
                    del self.notes[:k + 1]
                    return p["diagnostics"]
            msg = self.read(max(0.01, end - time.time()) / SLOW)
            if msg is None:
                return None
            self.notes.append(msg)

    def expected(self, entry, rel):
        """What `vo check --json ENTRY` in the twin says about rel, as the protocol's diagnostics."""
        recs, _ = direct(["check", "--json", os.path.join(self.twin, entry)], self.twin)
        text = open(os.path.join(self.twin, rel), encoding="utf-8").read()
        return [{"range": rng(text, r, self.utf8), "severity": 2 if r["severity"] == "warning" else 1, "code": r["code"],
                 "codeDescription": {"href": f"https://d4n-larsson.github.io/volang/errors/{r['code']}/"},
                 "source": "vo", "message": r["message"]}
                for r in recs if r.get("kind") == "diagnostic" and r.get("file") == os.path.join(self.twin, rel)]

    def position(self, rel, needle, nth=0, inside=0):
        """The protocol's position of the nth `needle` in rel's text (plus `inside` characters)."""
        text, at = self.text(rel), -1
        for _ in range(nth + 1):
            at = text.index(needle, at + 1)
        at += inside
        line = text.count("\n", 0, at)
        return {"line": line, "character": units(text[text.rfind("\n", 0, at) + 1:at], self.utf8)}

    def target(self, rel, pos):
        """The same position as the toolchain's FILE:LINE:COL in the twin."""
        line = self.text(rel).split("\n")[pos["line"]]
        k = 0
        while k < len(line) and units(line[:k], self.utf8) < pos["character"]:
            k += 1
        return f"{os.path.join(self.twin, rel)}:{pos['line'] + 1}:{k + 1}"

    def here(self, rec_file):
        """A twin path in a record as the workspace's address."""
        return self.uri(os.path.relpath(rec_file, self.twin))

    def close(self, timeout=10):
        try:
            i = self.ask("shutdown", None)
            self.answer(i)
            self.notify("exit", None)
            self.p.stdin.close()
            code = self.p.wait(timeout * SLOW)
        except (BrokenPipeError, AssertionError, subprocess.TimeoutExpired):
            self.p.kill()
            code = "no exit"
        self.p.stdout.close()
        return code


def others_in(cwd, server_pid):
    found = []
    for pid in os.listdir("/proc"):
        if pid.isdigit() and int(pid) != server_pid:
            try:
                if os.readlink(f"/proc/{pid}/cwd") == cwd and open(f"/proc/{pid}/stat").read().split(") ")[-1][0] != "Z":
                    found.append(int(pid))
            except OSError:
                pass
    return found


# ---- the cases ----

def lifecycle(work):
    version = subprocess.run([VO, "version"], capture_output=True).stdout.decode().split()[1]
    for utf8 in (False, True):
        e = Editor(work, f"life{int(utf8)}", init=False, utf8=utf8)
        early = e.request("textDocument/hover", {"textDocument": {"uri": e.uri("main.vo")}, "position": {"line": 0, "character": 0}})
        if early.get("error", {}).get("code") != -32002:
            fail("a request before initialize", f"{early}")
        caps = {"general": {"positionEncodings": ["utf-8", "utf-16"]}} if utf8 else {}
        r = e.request("initialize", {"processId": None, "rootUri": e.uri(""), "capabilities": caps})["result"]
        want = {"positionEncoding": "utf-8" if utf8 else "utf-16",
                "textDocumentSync": {"openClose": True, "change": 1, "save": {"includeText": False}},
                "hoverProvider": True, "definitionProvider": True, "referencesProvider": True,
                "renameProvider": {"prepareProvider": True}, "documentFormattingProvider": True,
                "documentSymbolProvider": True, "codeActionProvider": {"codeActionKinds": ["quickfix"]}}
        if r.get("capabilities") != want or r.get("serverInfo") != {"name": "vo", "version": version}:
            fail(f"initialize (utf8 {utf8})", f"{r}")
        e.notify("initialized", {})
        e.notify("some/unknown", {})
        if e.request("textDocument/completion", {}).get("error", {}).get("code") != -32601:
            fail("a method not offered", "not -32601")
        if e.close() != 0:
            fail("shutdown then exit", "status is not 0")
    for how in ("exit", "end of input"):
        e = Editor(work, "life-" + how.replace(" ", ""))
        if how == "exit":
            e.notify("exit", None)
        e.p.stdin.close()
        if e.p.wait(10 * SLOW) != 1:
            fail(how + " without shutdown", "status is not 1")
        e.p.stdout.close()


def diagnostics(work):
    for utf8 in (False, True):
        e = Editor(work, f"diag{int(utf8)}", utf8=utf8)
        label = f"(utf8 {utf8})"

        def check(case, rel, entry):
            got, want = e.diagnostics(rel), e.expected(entry, rel)
            if got != want:
                fail(f"diagnostics: {case} {label}", f"{rel}\n  server:  {got}\n  command: {want}")
            return want

        e.set("main.vo", MAIN)
        if check("the file as saved", "main.vo", "main.vo"):
            fail("fixture", "main.vo is not clean")
        e.set("main.vo", BROKEN)
        if len(check("an unsaved change with three problems", "main.vo", "main.vo")) < 2:
            fail("fixture", "the broken text has too few diagnostics")
        if open(os.path.join(e.ws, "main.vo"), encoding="utf-8").read() != MAIN:
            fail("diagnostics", "the server wrote the unsaved text to disk")
        e.set("main.vo", BROKEN.replace('print("x")', 'print("x");'))
        got = check("the syntax error fixed: the checker's diagnostics", "main.vo", "main.vo")
        if not any(d["severity"] == 2 for d in got) or not any(d["severity"] == 1 for d in got):
            fail("fixture", f"expected an error and a warning, got {got}")
        e.set("main.vo", MAIN)
        check("the problems removed", "main.vo", "main.vo")
        e.set("alone.vo", ALONE.replace('"alone"', "alone"))
        check("a file that is its own program", "alone.vo", "alone.vo")
        e.set("fresh/new.vo", 'fn main() uses console {\n    print(1);\n}\n')
        if os.path.exists(os.path.join(e.ws, "fresh")):
            fail("a file only the editor has", "it was created on disk")
        check("a file only the editor has", "fresh/new.vo", "fresh/new.vo")
        e.set("main.vo", MAIN.replace("\n", "\r\n").replace('str(a)', 'a'))
        check("Windows line endings", "main.vo", "main.vo")
        if e.close() != 0:
            fail("diagnostics", "exit status")


def several_files(work):
    e = Editor(work, "multi")
    e.set("main.vo", MAIN)
    e.set("lib/shapes.vo", SHAPES)
    for rel in ("main.vo", "lib/shapes.vo"):
        if e.diagnostics(rel) != []:
            fail("several files", f"{rel} is not clean at the start")
    e.set("lib/shapes.vo", SHAPES.replace("export fn area(w: int, h: int) -> int", "export fn area(w: int) -> int").replace("w * h", "w"))
    got, want = e.diagnostics("main.vo"), e.expected("main.vo", "main.vo")
    if got != want or not want:
        fail("a change in an imported file", f"main.vo\n  server:  {got}\n  command: {want}")
    if e.diagnostics("lib/shapes.vo") != e.expected("main.vo", "lib/shapes.vo"):
        fail("a change in an imported file", "lib/shapes.vo differs from the command")
    e.set("lib/shapes.vo", SHAPES)
    if e.diagnostics("main.vo") != [] or e.diagnostics("lib/shapes.vo") != []:
        fail("the imported file restored", "diagnostics remain")
    e.notify("textDocument/didClose", {"textDocument": {"uri": e.uri("lib/shapes.vo")}})
    del e.texts["lib/shapes.vo"]
    if e.diagnostics("lib/shapes.vo") != []:
        fail("closing a document", "its diagnostics are not cleared")
    e.diagnostics("main.vo")
    # a mistake in the unopened file: published for it, and cleared when it is fixed on disk
    e.set("lib/shapes.vo", SHAPES.replace("return w * h;", 'return "no";'), open_=False)
    e.notify("textDocument/didSave", {"textDocument": {"uri": e.uri("main.vo")}})
    got, want = e.diagnostics("lib/shapes.vo"), e.expected("main.vo", "lib/shapes.vo")
    if got != want or not want:
        fail("a mistake in an unopened, imported file", f"server {got}\n  command {want}")
    e.set("lib/shapes.vo", SHAPES, open_=False)
    e.notify("textDocument/didSave", {"textDocument": {"uri": e.uri("main.vo")}})
    if e.diagnostics("lib/shapes.vo") != []:
        fail("the unopened file fixed on disk", "its diagnostics are not cleared")
    # the library file alone: its program is main.vo's
    e2 = Editor(work, "multi2")
    e2.set("lib/shapes.vo", SHAPES.replace("return w * h;", "return w * h + missing;"))
    got, want = e2.diagnostics("lib/shapes.vo"), e2.expected("main.vo", "lib/shapes.vo")
    if got != want or not want:
        fail("a library file opened alone", f"server {got}\n  command {want}")
    e.close(), e2.close()


def quick_fixes(work):
    for utf8 in (False, True):
        e = Editor(work, f"fix{int(utf8)}", utf8=utf8)
        text = BROKEN.replace('print("x")', 'print("x");\n    print("y")')
        e.set("main.vo", text)
        e.diagnostics("main.vo")
        recs, _ = direct(["check", "--json", os.path.join(e.twin, "main.vo")], e.twin)
        fixable = [r for r in recs if r.get("kind") == "diagnostic" and r.get("fixes")]
        if not fixable:
            fail("fixture", "no fixable diagnostic")
        for r in fixable:
            actions = e.result("textDocument/codeAction", "main.vo", {"range": rng(text, r, utf8), "context": {"diagnostics": []}})
            mine = [a for a in actions if a["diagnostics"][0]["code"] == r["code"] and a["diagnostics"][0]["range"] == rng(text, r, utf8)]
            if len(mine) != len(r["fixes"]) or any(a["kind"] != "quickfix" or not a["title"] for a in mine):
                fail(f"quick fix for {r['code']} (utf8 {utf8})", f"{len(mine)} actions for {len(r['fixes'])} fixes: {actions}")
                continue
            for a, f in zip(mine, r["fixes"]):
                got = apply_edits(text, a["edit"]["changes"][e.uri("main.vo")], utf8)
                want = apply_edits(text, [{"range": rng(text, f, utf8), "newText": f["replacement"]}], utf8)
                if got != want:
                    fail(f"quick fix for {r['code']} (utf8 {utf8})", f"applied:\n{got}\nthe toolchain's fix:\n{want}")
        empty = e.result("textDocument/codeAction", "main.vo", {"range": {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 3}}, "context": {"diagnostics": []}})
        if empty != []:
            fail("quick fixes where there is nothing to fix", f"{empty}")
        e.close()


def hover_markdown(rec):
    doc, sig, typ = rec.get("doc") or {}, None, rec.get("type")
    sig = doc.get("signature")
    md = "```vo\n" + (sig if sig else rec.get("text", "") + (": " + typ if typ else "")) + "\n```"
    decl = rec.get("declaration") or {}
    at_decl = decl.get("file") == rec.get("file") and decl.get("start") == rec.get("start")  # the signature says it
    if sig and typ and rec.get("text") is not None and typ != sig and not at_decl:
        md += f"\n\nType: `{typ}`"
    if doc.get("description"):
        md += "\n\n" + doc["description"]
    return md


def reading(work):
    for utf8 in (False, True):
        e = Editor(work, f"read{int(utf8)}", utf8=utf8)
        e.set("main.vo", MAIN)
        e.diagnostics("main.vo")
        for rnd in ("as saved", "after an unsaved edit"):
            if rnd != "as saved":
                e.set("main.vo", "# moved down\n\n" + MAIN.replace("fn main", "\nfn main"))
                e.diagnostics("main.vo")
            spots = [("main.vo", "area(2", 0, 1), ("main.vo", "describe(shapes", 0, 2), ("main.vo", "Shape.square", 0, 1),
                     ("main.vo", "shapes.area", 0, 1), ("main.vo", "import lib.shapes", 0, 13),  # spec 058: a module name
                     ("main.vo", "square(2)", 0, 1), ("main.vo", "label +", 0, 1), ("main.vo", "print(label", 0, 1),
                     ("main.vo", "str(a)", 0, 1), ("main.vo", "a)", 0, 0), ("main.vo", "# ", 0, 0), ("main.vo", "\n\n", 0, 1),
                     ("lib/shapes.vo", "w * h", 0, 0), ("lib/shapes.vo", "circle(r)", 0, 1), ("lib/shapes.vo", "h: int;", 0, 0)]
            for rel, needle, nth, inside in spots:
                pos = e.position(rel, needle, nth, inside)
                label = f"{rel} at `{needle.strip()}` {rnd} (utf8 {utf8})"
                recs, _ = direct(["query", "--json", "type-at", os.path.join(e.twin, "main.vo"), e.target(rel, pos)], e.twin)
                rec = next((r for r in recs if r.get("kind") == "type_at"), None)
                text = e.text(rel)
                hover = e.result("textDocument/hover", rel, {"position": pos})
                want = rec and {"contents": {"kind": "markdown", "value": hover_markdown(rec)}, "range": rng(text, rec, utf8)}
                if hover != want:
                    fail(f"hover: {label}", f"\n  server:  {hover}\n  command: {want}")
                decl = rec and rec.get("declaration")
                decl = decl if decl and decl["file"].startswith("/") else None
                want = decl and {"uri": e.here(decl["file"]), "range": rng(e.text(os.path.relpath(decl["file"], e.twin)), decl, utf8)}
                got = e.result("textDocument/definition", rel, {"position": pos})
                if got != want:
                    fail(f"definition: {label}", f"\n  server:  {got}\n  command: {want}")
                got = e.result("textDocument/prepareRename", rel, {"position": pos})
                want = rng(text, rec, utf8) if decl else None
                if got != want:
                    fail(f"prepareRename: {label}", f"\n  server:  {got}\n  command: {want}")
                recs, _ = direct(["query", "--json", "refs", os.path.join(e.twin, "main.vo"), e.target(rel, pos)], e.twin)
                for with_decl in (True, False):
                    want = [{"uri": e.here(r["file"]), "range": rng(e.text(os.path.relpath(r["file"], e.twin)), r, utf8)}
                            for r in recs if r.get("kind") == "ref" and r["file"].startswith("/") and (with_decl or r["role"] != "declaration")]
                    got = e.result("textDocument/references", rel, {"position": pos, "context": {"includeDeclaration": with_decl}})
                    if got != want:
                        fail(f"references: {label}", f"\n  server:  {got}\n  command: {want}")
        e.close()


def changing(work):
    for utf8 in (False, True):
        e = Editor(work, f"change{int(utf8)}", utf8=utf8)
        e.set("main.vo", MAIN.replace("# Prints", "# Prints, with an unsaved edit:"))
        e.diagnostics("main.vo")
        for rel, needle, new in (("main.vo", "area(2", "surface"), ("lib/shapes.vo", "circle(r)", "round"),
                                 ("lib/shapes.vo", "h: int;", "height"), ("main.vo", "label +", "title"), ("lib/shapes.vo", "describe(s", "tell")):
            pos = e.position(rel, needle, 0, 1 if "(" in needle else 0)
            got = e.result("textDocument/rename", rel, {"position": pos, "newName": new})
            before = {f: open(os.path.join(e.twin, f), encoding="utf-8").read() for f in list(FILES) + ["vo.effects"]}
            recs, code = direct(["edit", "--json", "rename", os.path.join(e.twin, "main.vo"), e.target(rel, pos), new], e.twin)
            after = {f: open(os.path.join(e.twin, f), encoding="utf-8").read() for f in before}
            for f, t in before.items():
                open(os.path.join(e.twin, f), "w", encoding="utf-8").write(t)  # the twin as it was
            label = f"rename `{needle.strip()}` to {new} (utf8 {utf8})"
            if not got or "changes" not in got:
                fail(label, f"{got}; the command exits {code}")
                continue
            for f in before:
                edits = got["changes"].get(e.uri(f), [])
                mine = apply_edits(before[f], edits, utf8)
                if mine != after[f]:
                    fail(label, f"{f} after the server's edits differs from `vo edit rename`:\n{mine}\n---\n{after[f]}")
        for rel, needle, new, why in (("main.vo", "area(2", "describe", "a name in use"), ("main.vo", "area(2", "Area", "a wrong spelling"),
                                      ("main.vo", "str(a)", "text", "a builtin"), ("main.vo", "main()", "start", "the program's start")):
            pos = e.position(rel, needle, 0, 1)
            got = e.request("textDocument/rename", {"textDocument": {"uri": e.uri(rel)}, "position": pos, "newName": new})
            recs, _ = direct(["edit", "--json", "--dry-run", "rename", os.path.join(e.twin, "main.vo"), e.target(rel, pos), new], e.twin)
            want = next((r["message"].replace(e.twin, e.ws) for r in recs if r.get("kind") == "toolchain_error"), None)
            err = got.get("error", {})
            if err.get("code") != -32803 or (want and err.get("message") != want):
                fail(f"a refused rename ({why}, utf8 {utf8})", f"server {got}\n  command's message: {want}")
        for case, text in (("a buffer that is not canonical", UGLY), ("a canonical buffer", MAIN),
                           ("a buffer with a syntax error", BROKEN), ("no final newline", UGLY.rstrip("\n")),
                           ("blank lines at the end", UGLY + "\n\n"), ("a change on the last line", MAIN.rstrip("\n") + "  \n")):
            e.set("main.vo", text)
            got = e.result("textDocument/formatting", "main.vo", {"options": {"tabSize": 4, "insertSpaces": True}})
            subprocess.run([VO, "fmt", "main.vo"], cwd=e.twin, capture_output=True)
            want = open(os.path.join(e.twin, "main.vo"), encoding="utf-8").read()
            if not isinstance(got, list) or apply_edits(text, got, utf8) != want or (text == want and got != []):
                fail(f"formatting: {case} (utf8 {utf8})", f"{got}\n  applied: {apply_edits(text, got, utf8) if isinstance(got, list) else None!r}\n  vo fmt:  {want!r}")
        e.close()


def outline(work):
    e = Editor(work, "outline")
    e.set("lib/shapes.vo", SHAPES)
    got = e.result("textDocument/documentSymbol", "lib/shapes.vo")
    names = [(s["name"], s["kind"], [(c["name"], c["kind"]) for c in s.get("children", [])]) for s in got]
    want = [("Shape", 10, [("square", 22), ("circle", 22)]), ("Size", 23, [("w", 8), ("h", 8)]), ("area", 12, []),
            ("describe", 12, []), ("limit", 14, []), ("area of a rectangle", 6, [])]
    if names != want:
        fail("document symbols", f"{names}")
    lines = SHAPES.split("\n")
    for s in got:
        r, sel = s["range"], s["selectionRange"]
        inside = (r["start"]["line"], r["start"]["character"]) <= (sel["start"]["line"], sel["start"]["character"]) and \
                 (sel["end"]["line"], sel["end"]["character"]) <= (r["end"]["line"], r["end"]["character"])
        named = s["kind"] == 6 or lines[sel["start"]["line"]][sel["start"]["character"]:sel["end"]["character"]] == s["name"]
        if not inside or not named or r["end"]["line"] >= len(lines):
            fail("document symbols", f"ranges of {s['name']}: {r} {sel}")
    e.set("lib/shapes.vo", SHAPES + "\nfn broken( {\n")
    got = e.result("textDocument/documentSymbol", "lib/shapes.vo")
    if [s["name"] for s in got][:5] != [w[0] for w in want[:5]]:
        fail("document symbols with a syntax error", f"{[s['name'] for s in got]}")
    e.close()


def scheduling(work):
    e = Editor(work, "burst")
    e.set("main.vo", MAIN)
    e.diagnostics("main.vo")
    del e.notes[:]
    for k in range(20):
        e.set("main.vo", MAIN.replace("str(a)", "a" if k < 19 else "str(a)") + f"# {k}\n")
        time.sleep(0.005)
    hover = e.result("textDocument/hover", "main.vo", {"position": e.position("main.vo", "area(2", 0, 1)})
    if not hover or "area" not in hover["contents"]["value"]:
        fail("a hover during the pause", f"{hover}")
    last = e.diagnostics("main.vo")
    seen = [m["params"]["version"] for m in e.notes if m.get("method") == "textDocument/publishDiagnostics"]
    if last != [] or seen != sorted(seen):
        fail("a burst of changes", f"last {last}, versions published before it: {seen}")
    time.sleep(0.4 * SLOW)
    extra = e.read(0.3)
    if extra is not None:
        fail("a burst of changes", f"something was published after the newest text's diagnostics: {extra}")
    e.close()


def robustness(work):
    e = Editor(work, "robust", env={"VO_LSP_TEST": "slow"})
    e.set("main.vo", MAIN)
    e.diagnostics("main.vo")
    spot = lambda: e.position("main.vo", "area(2", 0, 1)

    def alive(case):
        h = e.result("textDocument/hover", "main.vo", {"position": spot()})
        if not isinstance(h, dict) or "contents" not in h:
            fail(case, f"the hover after it: {h}")

    rnd = random.Random(3)
    for case, text in (("random bytes", bytes(rnd.randrange(256) for _ in range(4000)).decode("latin-1")),
                       ("5,000 nested parentheses", "fn main() {\n    let x = " + "(" * 5000 + "1" + ")" * 5000 + ";\n}\n"),
                       ("an empty document", ""), ("only a quote", '"'), ("NUL characters", "fn main() {\n\x00\x00}\n")):
        e.set("junk.vo", text)
        if e.diagnostics("junk.vo", 15) is None:
            fail(case, "no diagnostics were published for it")
        for method, extra in (("textDocument/hover", {"position": {"line": 1, "character": 3}}), ("textDocument/documentSymbol", {}),
                              ("textDocument/formatting", {"options": {}}),
                              ("textDocument/codeAction", {"range": {"start": {"line": 0, "character": 0}, "end": {"line": 9, "character": 0}}, "context": {"diagnostics": []}})):
            msg = e.request(method, dict({"textDocument": {"uri": e.uri("junk.vo")}}, **extra))
            if "result" not in msg and msg.get("error", {}).get("code") != -32803:
                fail(case, f"{method}: {msg}")
        alive(case)
    for case, rel, pos in (("a position far past the end", "main.vo", {"line": 100000, "character": 100000}),
                           ("a character past the end of its line", "main.vo", {"line": 0, "character": 5000}),
                           ("an unopened file", "lib/shapes.vo", {"line": 17, "character": 12}),
                           ("a file that does not exist", "nowhere.vo", {"line": 0, "character": 0})):
        for method in ("textDocument/hover", "textDocument/definition", "textDocument/references", "textDocument/prepareRename"):
            msg = e.request(method, {"textDocument": {"uri": e.uri(rel)}, "position": pos, "context": {"includeDeclaration": True}})
            if "result" not in msg:
                fail(case, f"{method}: {msg}")
        alive(case)
    h = e.result("textDocument/hover", "lib/shapes.vo", {"position": {"line": 17, "character": 12}})
    if not h or "area" not in h["contents"]["value"]:
        fail("hover in an unopened file", f"{h}")
    for uri in ("untitled:Untitled-1", "http://example.com/x.vo", "file://other-host/x.vo"):
        if e.request("textDocument/hover", {"textDocument": {"uri": uri}, "position": {"line": 0, "character": 0}}).get("result", 0) is not None:
            fail(f"an address that is not a file ({uri})", "the answer is not null")
        e.notify("textDocument/didOpen", {"textDocument": {"uri": uri, "languageId": "vo", "version": 1, "text": "x"}})
    for case, params, code in (("no textDocument", {}, -32602), ("a position that is a string", {"textDocument": {"uri": e.uri("main.vo")}, "position": "x"}, -32602),
                               ("params that are a list", [1], -32602)):
        if e.request("textDocument/hover", params).get("error", {}).get("code") != code:
            fail(f"wrong parameters ({case})", f"not {code}")
    if e.request("textDocument/rename", {"textDocument": {"uri": e.uri("main.vo")}, "position": spot()}).get("error", {}).get("code") != -32602:
        fail("rename without a new name", "not -32602")
    for case, body, code in (("a body that is not JSON", b"hello", -32700), ("a batch", b'[{"jsonrpc":"2.0","id":1,"method":"shutdown"}]', -32600),
                             ("a number", b"7", -32600)):
        e.raw(b"Content-Length: %d\r\n\r\n" % len(body) + body)
        msg = e.read()
        if not msg or msg.get("id", 0) is not None or msg.get("error", {}).get("code") != code:
            fail(case, f"{msg}")
        alive(case)
    e.raw(b"content-length: 2\r\nContent-Type: application/vscode-jsonrpc; charset=utf-8\r\n\r\n{}")  # other headers, any case
    e.send({"jsonrpc": "2.0", "id": 9, "result": None})  # an answer to nothing
    e.notify("$/cancelRequest", {"id": 424242})
    e.notify("$/setTrace", {"value": "off"})
    alive("odd but legal messages")
    # the time limit, and cancellation (VO_LSP_TEST=slow: a hover on line 4242 sleeps; the limit is 0.4 s)
    t = time.time()
    msg = e.request("textDocument/hover", {"textDocument": {"uri": e.uri("main.vo")}, "position": {"line": 4242, "character": 0}})
    if msg.get("error", {}).get("code") != -32803 or time.time() - t > 3 * SLOW:
        fail("a request over the time limit", f"{msg} after {time.time() - t:.1f} s")
    alive("a request over the time limit")
    i = e.ask("textDocument/hover", {"textDocument": {"uri": e.uri("main.vo")}, "position": {"line": 4242, "character": 0}})
    e.notify("$/cancelRequest", {"id": i})
    j = e.ask("textDocument/hover", {"textDocument": {"uri": e.uri("main.vo")}, "position": spot()})
    first, second = e.answer(i), e.answer(j)
    if first.get("error", {}).get("code") != -32800 or "result" not in second:
        fail("a cancelled request", f"{first}; the next: {str(second)[:100]}")
    if others_in(e.ws, e.p.pid):
        time.sleep(0.5 * SLOW)
        if others_in(e.ws, e.p.pid):
            fail("after the time limit and a cancellation", "a copy of the server is still running")
    if e.close() != 0:
        fail("robustness", "exit status")
    # a frame that cannot be one ends the session; the editor going away ends the server and its copy
    e = Editor(work, "robust2", env={"VO_LSP_TEST": "slow"})
    e.raw(b"Hello: there\r\n\r\n{}")
    if e.p.wait(10 * SLOW) != 1:
        fail("a frame without Content-Length", "the server did not end with status 1")
    e.p.stdin.close(), e.p.stdout.close()
    e = Editor(work, "robust3", env={"VO_LSP_TEST": "slow"})
    e.set("main.vo", MAIN)
    e.diagnostics("main.vo")
    e.ask("textDocument/hover", {"textDocument": {"uri": e.uri("main.vo")}, "position": {"line": 4242, "character": 0}})
    time.sleep(0.1 * SLOW)
    e.p.stdout.close()
    e.p.stdin.close()
    try:
        e.p.wait(5 * SLOW)
    except subprocess.TimeoutExpired:
        fail("the editor going away", "the server did not end")
        e.p.kill()
    time.sleep(0.2 * SLOW)
    if others_in(e.ws, e.p.pid):
        fail("the editor going away", "a copy of the server is still running")
        for pid in others_in(e.ws, e.p.pid):
            os.kill(pid, signal.SIGKILL)


def nothing_written(work):
    e = Editor(work, "quiet")

    def snapshot():
        found = {}
        for d, _, names in os.walk(e.ws):
            for n in names:
                p = os.path.join(d, n)
                found[os.path.relpath(p, e.ws)] = (open(p, "rb").read(), os.stat(p).st_mtime_ns)
        return found

    before = snapshot()
    e.set("main.vo", UGLY)
    e.set("alone.vo", ALONE)
    e.set("lib/shapes.vo", SHAPES.replace("w * h", "w * h * 1"))
    for rel in ("main.vo", "alone.vo", "lib/shapes.vo"):
        e.diagnostics(rel)
    e.result("textDocument/formatting", "main.vo", {"options": {}})
    e.result("textDocument/rename", "main.vo", {"position": e.position("main.vo", "area( 2", 0, 1), "newName": "surface"})
    e.result("textDocument/rename", "lib/shapes.vo", {"position": e.position("lib/shapes.vo", "describe(s", 0, 1), "newName": "tell"})
    e.result("textDocument/codeAction", "main.vo", {"range": {"start": {"line": 0, "character": 0}, "end": {"line": 20, "character": 0}}, "context": {"diagnostics": []}})
    e.result("textDocument/hover", "alone.vo", {"position": {"line": 1, "character": 6}})
    e.notify("textDocument/didSave", {"textDocument": {"uri": e.uri("main.vo")}})
    e.diagnostics("main.vo")
    e.close()
    after = snapshot()
    if after != before:
        changed = sorted(set(before) ^ set(after)) + [f for f in before if f in after and before[f] != after[f]]
        fail("the server writes nothing and runs nothing", f"changed or new files: {changed}")


def session(work, n):
    """SC-004: the server's resident size after n edits and requests against after the first 50."""
    e = Editor(work, "session")
    rss = lambda: next(int(line.split()[1]) for line in open(f"/proc/{e.p.pid}/status") if line.startswith("VmRSS"))
    rnd, early, mid, ok = random.Random(5), 0, 0, True
    texts = [MAIN, BROKEN, UGLY, MAIN + "# x\n" * 400, bytes(rnd.randrange(256) for _ in range(3000)).decode("latin-1")]
    for i in range(n):
        if i % 4 == 0:
            e.set("main.vo", texts[(i // 4) % len(texts)])
            if i % 40 == 0:
                ok &= e.diagnostics("main.vo") is not None
        elif i % 4 == 1:
            ok &= "error" not in e.request("textDocument/hover", {"textDocument": {"uri": e.uri("main.vo")}, "position": {"line": 7, "character": 13}}) or True
        elif i % 4 == 2:
            e.request("textDocument/documentSymbol", {"textDocument": {"uri": e.uri("lib/shapes.vo")}})
        else:
            e.request("textDocument/references", {"textDocument": {"uri": e.uri("lib/shapes.vo")}, "position": {"line": 17, "character": 12}, "context": {"includeDeclaration": True}})
        if i == 49:
            early = rss()
        if i == n // 2:
            mid = rss()
        if i % 100 == 99:
            del e.notes[:]
    late = rss()
    # no growth with the number of requests: the second half adds nothing, and the whole run at most a few pages
    ok = ok and e.close() == 0 and late <= mid and late <= early + 64
    print(f"SC-004: {n} edits and requests; resident size {early} KiB after 50, {mid} KiB after {n // 2}, "
          f"{late} KiB after {n}: {'ok' if ok else 'FAIL'}")
    return ok


def timing(work):
    """SC-003: a 2,000-line file in a 20-file program: a change to diagnostics, and a hover."""
    files = {"main.vo": "".join(f"import m{k}.f{k};\n" for k in range(19)) + "\nfn main() uses console {\n" +
             "".join(f"    print(str(f{k}({k})));\n" for k in range(19)) + "}\n"}
    for k in range(19):
        body = "".join(f"fn g{k}_{j}(x: int) -> int {{\n    let y = x + {j};\n    return y * 2;\n}}\n\n" for j in range(21 if k else 400))
        files[f"m{k}.vo"] = body + f"export fn f{k}(x: int) -> int {{\n    return g{k}_0(x);\n}}\n"
    e = Editor(work, "time", files)
    big = files["m0.vo"]
    print(f"SC-003: {len(files)} files, m0.vo has {big.count(chr(10))} lines")
    e.set("m0.vo", big)
    first = e.diagnostics("m0.vo")
    if first is None or any(d["severity"] == 1 for d in first):  # (a warning about the file's size is expected)
        print(f"the generated program does not check: {str(first)[:400]}")
        return False
    d, h = [], []
    for k in range(15):
        t = time.perf_counter()
        e.set("m0.vo", big + f"# {k}\n")
        e.diagnostics("m0.vo")
        d.append(time.perf_counter() - t)
        t = time.perf_counter()
        e.result("textDocument/hover", "m0.vo", {"position": {"line": 1, "character": 9}})
        h.append(time.perf_counter() - t)
    e.close()
    dm, hm = statistics.median(d) * 1000, statistics.median(h) * 1000
    print(f"SC-003: a change to its diagnostics {dm:.0f} ms (of which the pause before checking is 150 ms; limit 200 ms); hover {hm:.0f} ms (limit 100 ms)")
    return dm <= 200 and hm <= 100


def main():
    work = tempfile.mkdtemp(prefix="vo-lsp-")
    try:
        if "--session" in sys.argv:
            sys.exit(0 if session(work, int(sys.argv[sys.argv.index("--session") + 1])) else 1)
        if "--time" in sys.argv:
            sys.exit(0 if timing(work) else 1)
        groups = (lifecycle, diagnostics, several_files, quick_fixes, reading, changing, outline, scheduling, robustness, nothing_written)
        for group in groups:
            try:
                group(work)
            except Exception as e:  # a hang or a message that is not JSON ends the group; the rest still run
                fail(group.__name__, f"{type(e).__name__}: {e}")
        print(f"lsp: {len(groups)} groups, {len(fails)} failures")
        sys.exit(1 if fails else 0)
    finally:
        shutil.rmtree(work, ignore_errors=True)


main()
