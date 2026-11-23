"""MkDocs hook: the examples convention's ```error CODE fence has a two-word info string, which
superfences reads as inline code (and then mis-pairs every later fence). Render it as a titled text
block of class vo-error (a label under the program: extra.css); the Markdown copies for agents keep the original form."""
import re


def on_page_markdown(markdown, **kwargs):
    return re.sub(r"^(\s*)```error (\S+)\s*$", r'\1```{ .text .vo-error title="error \2" }', markdown, flags=re.M)
