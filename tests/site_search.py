#!/usr/bin/env python3
"""Spec 015 (SC-005), on the built site (make site-check): fixed lookups against the search index the
browser searches (build/site/search/search_index.json). Each query must find one of its expected pages
among the first three; at least 90% of the queries must. Ranking follows the site search's fields: a term
in a section's title counts more than in its text; a page ranks by its best section.

This is an offline proxy for the in-browser search (same index, same fields, a simpler scorer).
"""
import json
import math
import os
import re
import sys
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INDEX = os.path.join(ROOT, "build", "site", "search", "search_index.json")

QUERIES = [
    ("E0503", ["errors/E0503/"]),
    ("E0310", ["errors/E0310/"]),
    ("R0010", ["errors/R0010/"]),
    ("pad_left", ["std/text/"]),
    ("parse_json", ["std/json/"]),
    ("key_at_or_above", ["std/collections/"]),
    ("ordered map", ["guide/collections/", "reference/types/"]),
    ("priority queue", ["guide/collections/", "std/collections/"]),
    ("deque", ["guide/collections/"]),
    ("effects", ["guide/effects/", "reference/effects/", "tools/effects/"]),
    ("effect scopes", ["guide/effects/", "reference/effects/"]),
    ("vo.effects", ["guide/effects/", "tools/effects/"]),
    ("property test", ["guide/properties/"]),
    ("shrinking", ["guide/properties/"]),
    ("requires ensures", ["guide/contracts/"]),
    ("todo hole", ["guide/holes/", "reference/holes/"]),
    ("generics", ["guide/generics/", "reference/generics/"]),
    ("type inference", ["guide/generics/", "guide/types/"]),
    ("records", ["guide/records-and-variants/", "reference/declarations/"]),
    ("match", ["guide/records-and-variants/"]),
    ("try result", ["guide/results-and-errors/", "reference/results-and-errors/"]),
    ("closures", ["guide/functions/", "reference/functions-as-values/"]),
    ("lambda", ["guide/functions/", "reference/functions-as-values/"]),
    ("export", ["guide/modules-and-packages/", "reference/programs-in-several-files/"]),
    ("packages", ["guide/modules-and-packages/", "tools/package/"]),
    ("vo check", ["tools/check/"]),
    ("vo test", ["tools/test/", "guide/tests/"]),
    ("vo fmt", ["tools/fmt/"]),
    ("replay", ["tools/replay/", "guide/determinism/", "reference/traces-and-replay/"]),
    ("trace", ["tools/run/", "guide/determinism/", "reference/traces-and-replay/"]),
    ("timeout", ["tools/run/", "guide/determinism/", "errors/R0009/"]),
    ("exit status", ["tools/exit-statuses/"]),
    ("json output", ["tools/json-output/", "reference/machine-readable-output/"]),
    ("install", ["start/install/"]),
    ("first program", ["start/first-program/"]),
    ("seed", ["guide/determinism/", "tools/run/"]),
]


def tokens(text):
    return [t for t in re.split(r"[\s\-]+", re.sub(r"<[^>]+>", " ", text.lower())) if t]


def main():
    docs = json.load(open(INDEX, encoding="utf-8"))["docs"]
    df = defaultdict(int)
    prepared = []
    for d in docs:
        title, text = tokens(d.get("title", "")), tokens(d.get("text", ""))
        for t in set(title + text):
            df[t] += 1
        prepared.append((d["location"].split("#")[0], title, text))
    n = len(prepared)
    misses = []
    for query, expected in QUERIES:
        terms = tokens(query)
        pages = defaultdict(float)
        for page, title, text in prepared:
            score = 0.0
            for t in terms:
                idf = math.log(1 + n / (1 + df.get(t, 0)))
                in_title = sum(1 for w in title if w.strip(".,:;()`") == t)
                in_text = sum(1 for w in text if w.strip(".,:;()`") == t)
                score += idf * (10 * in_title + in_text / math.sqrt(1 + len(text)))
            pages[page] = max(pages[page], score)
        top = [p for p, s in sorted(pages.items(), key=lambda x: -x[1]) if s > 0][:3]
        ok = any(e in top for e in expected)
        print(f"{'ok  ' if ok else 'MISS'} {query!r:24} -> {top}")
        if not ok:
            misses.append(query)
    rate = 1 - len(misses) / len(QUERIES)
    print(f"site_search: {len(QUERIES)} lookups, {len(QUERIES) - len(misses)} found in the first three ({rate:.0%}; at least 90% needed)")
    return 0 if rate >= 0.9 else 1


if __name__ == "__main__":
    sys.exit(main())
