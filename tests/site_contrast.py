#!/usr/bin/env python3
"""Spec 015 (FR-014, FR-015, SC-007), on the built site (make site-check):
- the pinned colors of site/docs/stylesheets/extra.css (text, links, header, footer, code and its syntax
  colors against their backgrounds) meet
  WCAG 2.1 AA, 4.5:1, in the light and the dark scheme;
- every page's content is in its HTML (its title is there without running scripts);
- no page or stylesheet loads anything from another site (scripts, stylesheets, fonts, @import)."""
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SITE = os.path.join(ROOT, "build", "site")
problems = []


def lum(hexcolor):
    h = hexcolor.lstrip("#")
    rgb = [int(h[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    lin = [c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4 for c in rgb]
    return 0.2126 * lin[0] + 0.7152 * lin[1] + 0.0722 * lin[2]


def ratio(a, b):
    la, lb = sorted([lum(a), lum(b)], reverse=True)
    return (la + 0.05) / (lb + 0.05)


css = open(os.path.join(ROOT, "site", "docs", "stylesheets", "extra.css")).read()
for scheme, body in re.findall(r'\[data-md-color-scheme="(\w+)"\]\s*\{(.*?)\}', css, re.S):
    v = dict(re.findall(r"--([\w-]+):\s*(#[0-9a-fA-F]{6})", body))
    pairs = [("md-typeset-color", "md-default-bg-color"), ("md-typeset-a-color", "md-default-bg-color"),
             ("md-default-fg-color--light", "md-default-bg-color"), ("md-accent-fg-color", "md-default-bg-color"),
             ("md-primary-bg-color", "md-primary-fg-color"), ("md-footer-fg-color", "md-footer-bg-color"),
             ("md-code-fg-color", "md-code-bg-color"), ("md-typeset-a-color", "md-code-bg-color")]
    pairs += [(k, "md-code-bg-color") for k in v if k.startswith("md-code-hl-")]  # syntax colors
    for fg, bg in pairs:
        r = ratio(v[fg], v[bg])
        print(f"{scheme:8} {fg:30} on {bg:20} {r:5.2f}:1")
        if r < 4.5:
            problems.append(f"{scheme}: {fg} on {bg} is {r:.2f}:1 (WCAG AA needs 4.5:1)")

external = re.compile(r'<(?:script|link)[^>]+(?:src|href)="(?:https?:)?//(?!d4n-larsson\.github\.io)[^"]*"', re.I)
pages = glob.glob(os.path.join(SITE, "**", "index.html"), recursive=True)
for p in pages:
    html = open(p, encoding="utf-8").read()
    if external.search(html):
        problems.append(f"{os.path.relpath(p, SITE)} loads from another site: {external.search(html).group(0)[:120]}")
    m = re.search(r"<h1[^>]*>(.*?)</h1>", html, re.S)
    if not m or not re.sub(r"<[^>]+>", "", m.group(1)).strip():
        problems.append(f"{os.path.relpath(p, SITE)} has no title in its HTML")
for p in glob.glob(os.path.join(SITE, "**", "*.css"), recursive=True):
    if re.search(r"@import\s+(url\()?['\"]?https?:", open(p, encoding="utf-8").read()):
        problems.append(f"{os.path.relpath(p, SITE)} imports from another site")
for pr in problems:
    print("FAIL", pr)
print(f"site_contrast: {len(pages)} pages, {len(problems)} problems")
sys.exit(1 if problems else 0)
