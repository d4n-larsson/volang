# Site link checker

Check the links of small Markdown sites kept in the directory `pages/`, and find the pages nothing links to.

## Input

Each non-empty line of standard input is a site name. Handle the sites in input order; for each, print `== <name>` and then:

- if the name is not 1 to 30 characters, each a lowercase ASCII letter, a digit or `-`: `error: bad site name`;
- otherwise, if `pages/<name>` is not a directory: `error: no such site`;
- otherwise the report below.

## Pages

The site's files are all files anywhere below `pages/<name>/`, named by their path relative to that directory (`guide/setup.md`). Its **pages** are the files whose name ends in `.md`. Other files can be link targets but are not read.

Each page is read line by line (lines are numbered from 1). A line that starts with three backticks (```` ``` ````) opens a code block, and the next such line closes it; these fence lines and every line between them are ignored. A code block that is never closed lasts to the end of the page.

**Headings.** A line that starts with 1 to 3 `#` followed by a space is a heading; its text is the rest of the line with spaces removed at both ends (`#### x` and `#x` are not headings). Its **slug** is made from the text character by character: `A`–`Z` become lowercase, `a`–`z`, `0`–`9` and `-` are kept, a space becomes `-`, and every other character is dropped. A page's **anchors** are the slugs of its headings, in order; when a slug occurred `k` times before on the same page (`k >= 1`), this heading's anchor is `<slug>-<k>` instead (three headings `Intro` give `intro`, `intro-1`, `intro-2`). A heading whose slug is empty gives no anchor.

**Links.** Every `](` in a line starts a link target, which runs from after the `(` to the first `)` after it on the same line. If there is no such `)`, or the target is empty or contains a space, it is not a link. Look for the next `](` after the end of the previous link target (or after the `](`, when it was not a link). This covers both `[text](target)` and images `![alt](target)`.

## Checking a link

Each link is handled by the first rule that applies:

1. The target starts with `http://` or `https://`: an **external** link, counted but not checked.
2. The target starts with `mailto:`: ignored entirely (not counted anywhere).
3. The target starts with `#`: an **internal** link to an anchor of the same page. The anchor is the rest of the target; if it is not empty and the page has no such anchor, the problem is `missing anchor #<anchor>`.
4. Otherwise it is an **internal** link to a file. Split the target at its first `#` into a path and an anchor (no `#`: no anchor). Resolve the path: if it starts with `/`, start from the site's root directory, otherwise from the page's directory; take its `/`-separated parts one at a time, skipping empty parts and `.`; `..` removes the last part collected so far, and if there is none, the target is **outside** the site. The remaining parts joined with `/` name the file. Then the problem is:
   - `outside <target>` (the target as written) if it is outside the site. Such a target must not be checked or read in any way, even if it points to another site in `pages/`.
   - `missing file <file>` if there is no such file in the site;
   - `missing anchor <file>#<anchor>` if the file is a page, the anchor is not empty, and that page has no such anchor (anchors of files that are not pages are never checked);
   - otherwise there is no problem.

## Report

1. Every problem as `<page>:<line>: <problem>`, sorted by page (byte order), then line, then position in the line.
2. The **orphans**: the pages that cannot be reached from `index.md` (at the site's root) by following internal links to pages that exist (anchors do not matter; links that have a problem other than a missing anchor are not followed). `index.md` is never an orphan; if the site has no `index.md`, every page is one. Print `orphan <page>` for each, sorted by page.
3. `pages <p> links <i> external <e> problems <n> orphans <o>`: the number of pages, of internal links (rules 3 and 4, problems included), of external links, of problem lines and of orphans.

## Allowed effects

The program may print, read standard input, and read files and list directories under `pages/`. Nothing else: no reading outside `pages/` (not even to check whether a link target exists), no writing files, no environment variables, no clock, no randomness, no network.
