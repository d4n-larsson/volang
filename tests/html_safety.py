#!/usr/bin/env python3
"""Spec 051 SC-002: no value inserted by std.html adds an element, an attribute or a URL scheme.

  html_safety.py [--quick] [--seed N]   1,000 hostile strings (200 with --quick) in every place

One vo program reads the strings (hex, one per line) and renders each into every template below; each
output is parsed with Python's html.parser (and html5lib when it is installed) and must have exactly the
elements and attributes the template has with a harmless value, and every href/src must be relative
or http, https or mailto.
"""
import os
import random
import subprocess
import sys
import tempfile
from html.parser import HTMLParser

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")

TEMPLATES = [
    "<p>{}</p>",
    '<a title="{}">x</a>',
    "<a title='{}'>x</a>",
    '<a href="{}">x</a>',
    '<a href="/notes/{}?q={}">x</a>',
    '<img src="{}" alt="{}">',
    "<title>{}</title><textarea>{}</textarea>",
    '<form action="{}"><input name="q" value="{}"></form>',
    "<ul><li>{}</li><li class=\"{}\">{}</li></ul>",
]

PARTS = ["<", ">", "&", "\"", "'", "`", "/", "=", " ", "\t", "\n", "\r", "\x00", "\x01", "\x0b", " ", "é",
         "script", "SCRIPT", "<script>alert(1)</script>", "</p>", "</a>", "<!--", "-->", "<![CDATA[", "]]>",
         "onerror=", " onmouseover=alert(1) ", "javascript:", "JaVaScRiPt:", " javascript:", "\tjava\nscript:",
         "data:text/html,", "vbscript:", "&lt;", "&#60;", "&#x3c;", "&#106;avascript:", "&quot;", "&#39;",
         "&amp;", "%3Cscript%3E", "\\", "{}", "{{", "}}", "x", "http://ok", "mailto:a@b", "#", "?", ":",
         "<img src=x onerror=alert(1)>", "<style>", "</textarea>", "</title>", "<svg/onload=alert(1)>"]


def strings(n, seed):
    rng = random.Random(seed)
    out = list(PARTS)
    while len(out) < n:
        out.append("".join(rng.choice(PARTS) for _ in range(rng.randint(2, 8))))
    return out[:n]


def program():
    lines = ["import std.bytes.from_hex;", "import std.bytes.from_text;", "import std.bytes.to_hex;",
             "import std.bytes.to_text;", "import std.html.html;", "import std.html.html_text;", "",
             "fn emit(s: str) uses console {", "    print(to_hex(from_text(s)));", "}", "",
             "fn main() -> result[void] uses console, input {", "    while true {",
             "        match read_line() {", "            case ok(line) {",
             "                let v = try to_text(try from_hex(line));"]
    for t in TEMPLATES:
        n = t.count("{}")
        vo_t = t.replace("\\", "\\\\").replace('"', '\\"')
        lines.append(f'                emit(html_text(html("{vo_t}"{", v" * n})));')
    lines += ["            }", "            case err(_) {", "                return ok();", "            }", "        }",
              "    }", "    return ok();", "}", ""]
    return "\n".join(lines)


class Shape(HTMLParser):
    """The elements and attribute names of a document, and its URL attribute values."""

    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.events = []
        self.urls = []

    def handle_starttag(self, tag, attrs):
        self.events.append(("start", tag, tuple(sorted(a for a, _ in attrs))))
        for a, v in attrs:
            if a in ("href", "src", "action", "formaction"):
                self.urls.append(v or "")

    def handle_startendtag(self, tag, attrs):
        self.handle_starttag(tag, attrs)

    def handle_endtag(self, tag):
        self.events.append(("end", tag))

    def handle_comment(self, data):
        self.events.append(("comment",))


def shape(markup):
    p = Shape()
    p.feed(markup)
    p.close()
    return p.events, p.urls


def scheme_ok(url):
    u = url.lstrip("".join(chr(c) for c in range(0, 33)))
    head = u
    for stop in "/?#":
        head = head.split(stop, 1)[0]
    if ":" not in head:
        return True
    return head.split(":", 1)[0].lower() in ("http", "https", "mailto")


def html5lib_shape(markup):
    try:
        import html5lib
    except ImportError:
        return None
    doc = html5lib.parseFragment(markup, namespaceHTMLElements=False)
    out = []

    def walk(el):
        for child in el:
            if isinstance(child.tag, str):
                out.append((child.tag, tuple(sorted(child.attrib))))
                walk(child)
    walk(doc)
    return out


def main():
    quick = "--quick" in sys.argv
    seed = int(sys.argv[sys.argv.index("--seed") + 1]) if "--seed" in sys.argv else 1
    values = strings(200 if quick else 1000, seed)
    with tempfile.TemporaryDirectory() as d:
        with open(os.path.join(d, "render.vo"), "w") as f:
            f.write(program())
        stdin = "".join(v.encode().hex() + "\n" for v in ["benign"] + values)
        p = subprocess.run([VO, "run", "--timeout", "120", "render.vo"], cwd=d, input=stdin, capture_output=True,
                           text=True, timeout=600)
    if p.returncode != 0:
        print("FAIL html_safety: the renderer did not run:", p.returncode, p.stderr[:600])
        return 1
    rows = [bytes.fromhex(l).decode() for l in p.stdout.split()]
    k = len(TEMPLATES)
    if len(rows) != k * (len(values) + 1):
        print(f"FAIL html_safety: {len(rows)} outputs for {len(values) + 1} values and {k} templates")
        return 1
    want = [shape(rows[i])[0] for i in range(k)]
    want5 = [html5lib_shape(rows[i]) for i in range(k)]
    bad = 0
    for n, v in enumerate(values):
        for i in range(k):
            out = rows[(n + 1) * k + i]
            events, urls = shape(out)
            problems = []
            if events != want[i]:
                problems.append("structure changed")
            if not all(scheme_ok(u) for u in urls):
                problems.append(f"URL scheme {urls}")
            if want5[i] is not None and html5lib_shape(out) != want5[i]:
                problems.append("structure changed (html5lib)")
            if problems:
                bad += 1
                if bad <= 10:
                    print(f"FAIL html_safety: template {TEMPLATES[i]!r} value {v!r}: {', '.join(problems)}: {out!r}")
    parser = "html.parser and html5lib" if want5[0] is not None else "html.parser"
    print(f"html_safety: {len(values)} strings x {k} templates, {bad} problems ({parser})")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
