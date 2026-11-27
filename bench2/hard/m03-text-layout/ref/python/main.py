import sys

import directives
import filling
import pagination


def main():
    st = directives.State()
    blocks, errors, words = [], [], []

    def end_paragraph():
        if words:
            lst = st.lists[-1] if st.lists else None
            lines = filling.paragraph(words, st)
            blocks.append(pagination.Block("para", lines, lst, st.brk, st.title, st.space))
            st.brk, st.space = False, None
            words.clear()

    for no, line in enumerate(sys.stdin.read().split("\n"), 1):
        if line.startswith("."):
            end_paragraph()
            err, head = directives.apply(st, [f for f in line.split(" ") if f])
            if err:
                errors.append(f"error line {no}: {err}")
            elif head is not None:
                lines = filling.heading(head, st)
                blocks.append(pagination.Block("heading", lines, None, st.brk, st.title, st.space,
                                               lines[0].lstrip(" ")))
                st.brk, st.space = False, None
        elif line.strip(" "):
            st.started = True
            words.extend(f for f in line.split(" ") if f)
        else:
            end_paragraph()
    end_paragraph()
    pages = pagination.paginate(blocks, st.height)
    out = pagination.render(pages, st.width, st.height)
    if st.contents and any(b.kind == "heading" for b in blocks):
        out += pagination.contents(blocks, st.width)
    out += errors
    sys.stdout.write("".join(s + "\n" for s in out))


main()
