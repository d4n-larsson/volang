# Volang Design Brief

## Nordic Orbital / Arctic Console

### 1. Design direction

The goal is to preserve the Nord colour identity while making the site
feel brighter, more deliberate, and more cohesive.

**Creative direction:** *Arctic terminal meets spacecraft instrument
panel.*

Aim for a calm, precise, slightly futuristic developer environment---not
a neon cyberpunk dashboard. The interface should feel like a toolchain
for a programming language.

### 2. Colour system

Keep the familiar Nord palette, but create clearer separation between
the page, navigation, content surfaces, and code examples.

  Role               Colour            Hex
  ------------------ ----------------- -----------
  Main background    Deep space        `#242B36`
  Sidebar            Polar Night       `#2E3440`
  Elevated panels    Slate             `#353E4C`
  Code background    Deep slate        `#272F3B`
  Borders            Muted blue-grey   `#434C5E`
  Main text          Snow              `#ECEFF4`
  Secondary text     Frosted grey      `#AAB6C5`
  Muted text         Blue-grey         `#7F8B9D`
  Primary accent     Ice blue          `#88C0D0`
  Secondary accent   Arctic blue       `#81A1C1`
  Tertiary accent    Sea glass         `#8FBCBB`
  Success            Muted green       `#A3BE8C`
  Warning            Soft amber        `#EBCB8B`
  Error              Nord red          `#BF616A`
  Special            Nord purple       `#B48EAD`

Use ice blue as the primary accent. Keep the other colours for
meaningful secondary emphasis and status states.

### 3. Remove the Aurora bar

Remove the Aurora bar rather than replacing it with another prominent
decorative gradient. It currently adds colour without reinforcing the
interface's structure.

Use quieter details instead:

-   A thin, low-contrast accent border beneath the header.
-   A very subtle technical grid in selected background areas.
-   Small metadata labels where they provide useful context, such as
    `DOCUMENTATION`, `VERSION 0.4.0`, or `LANGUAGE REFERENCE`.

Example header rule:

``` css
border-bottom: 1px solid rgba(136, 192, 208, 0.18);
```

Optional subtle grid:

``` css
background-image:
  linear-gradient(rgba(136, 192, 208, 0.025) 1px, transparent 1px),
  linear-gradient(90deg, rgba(136, 192, 208, 0.025) 1px, transparent 1px);
background-size: 40px 40px;
```

Keep the grid extremely faint and avoid applying it behind dense text.

### 4. Typography

Use **IBM Plex Mono** as the primary typeface. A monospace identity fits
a programming-language reference site, but hierarchy should come from
size, weight, spacing, and colour---not from making every element look
identical.

Suggested roles:

  Element                     Font
  --------------------------- ---------------
  Body text                   IBM Plex Mono
  Navigation                  IBM Plex Mono
  Code                        IBM Plex Mono
  UI labels                   IBM Plex Mono
  Optional display headings   Space Grotesk

A fully monospace site is also a good option. If using Space Grotesk,
reserve it for large display headings rather than introducing it
throughout the interface.

Priorities: - Use a comfortable body size and line height. - Keep
paragraphs at a readable measure. - Use weight and spacing to
distinguish headings and labels. - Avoid tiny uppercase text for
important information.

### 5. Homepage hero

Make the homepage introduction feel like the opening screen of a
language and toolchain, rather than a generic documentation landing
page.

Suggested content hierarchy:

``` text
VO                                               0.4.0
LANGUAGE / TOOLCHAIN / REFERENCE

A language for              [code example or terminal panel]
programs that
are easy to inspect.

A small, statically typed language designed for
AI coding agents and the people who review what they write.

[ GET STARTED ]   [ LANGUAGE GUIDE ]
```

Treat this as a layout concept, not a literal boxed ASCII design. Use
generous whitespace, a strong heading, a concise description, and a code
example that demonstrates the language.

### 6. Code examples

Code is a central part of the product identity. Make substantial
examples feel like editor or terminal panels.

Recommended treatment: - Dark code surface, distinct from the main page
background. - Thin Nord border. - Small, restrained corner radius
(around 5--7px). - Optional header for larger examples, with filename
and language label. - Nord syntax highlighting. - Muted line numbers, if
useful. - Copy control with clear hover and keyboard-focus states. -
Output panels visually distinct from source code.

Example header:

``` text
main.vo                                      SOURCE
```

Use these headers selectively. Small snippets and inline code should
remain lightweight.

### 7. Sidebar and navigation

Make navigation feel like a precise documentation index.

Possible hierarchy:

``` text
VO                                      0.4.0

DOCUMENTATION

OVERVIEW
  Home

LEARN
  Getting started
  Language guide

TOOLCHAIN
  Commands
  Editors

REFERENCE
  Types
  Functions
  Effects
  ...

LIBRARY
  Standard library

EXAMPLES
```

Use small, muted category labels and consistent vertical spacing. Avoid
excessive uppercase or decorative icons.

For the active item: - Slightly brighter background. - Ice-blue text. -
A thin left-side indicator. - No heavy glow.

``` css
.nav-item.active {
  background: rgba(136, 192, 208, 0.08);
  color: #88C0D0;
  border-left: 2px solid #88C0D0;
}
```

Ensure hover, focus, and active states are visually distinct and
accessible.

### 8. Colour with meaning

Assign colours consistently rather than using them as decoration.

  Colour        Purpose
  ------------- ------------------------------------------------
  Ice blue      Primary actions, links, selected navigation
  Arctic blue   Secondary actions and references
  Sea glass     Types and secondary emphasis
  Green         Successful tests and success states
  Amber         Warnings and attention
  Red           Errors and destructive actions
  Purple        Special syntax or uncommon language constructs

The same meaning should use the same colour throughout the site.

### 9. Initial CSS design tokens

Use shared variables so components draw from one coherent design system.
Adapt names to the site's existing framework and styles.

``` css
:root {
  /* Backgrounds */
  --bg: #242b36;
  --bg-sidebar: #2e3440;
  --bg-surface: #353e4c;
  --bg-code: #272f3b;

  /* Borders */
  --border: #434c5e;
  --border-subtle: rgba(136, 192, 208, 0.12);

  /* Text */
  --text: #eceff4;
  --text-secondary: #aab6c5;
  --text-muted: #7f8b9d;

  /* Accent */
  --accent: #88c0d0;
  --accent-secondary: #81a1c1;
  --accent-tertiary: #8fbcbb;

  /* Status */
  --success: #a3be8c;
  --warning: #ebcb8b;
  --danger: #bf616a;
  --special: #b48ead;

  /* Typography */
  --font-mono: "IBM Plex Mono", monospace;
  --font-display: "Space Grotesk", sans-serif;

  /* Shape and motion */
  --radius: 6px;
  --radius-large: 10px;
  --transition: 160ms ease;
}
```

### 10. What to avoid

Avoid visual effects that make the site feel like a generic sci-fi
dashboard:

-   Rainbow or prominent multicolour gradients.
-   Large Aurora gradients.
-   Neon cyan everywhere.
-   Excessive glow effects.
-   Glassmorphism on every panel.
-   Animated starfields or distracting backgrounds.
-   Decorative sci-fi icons beside every heading.
-   Excessively rounded cards.
-   Borders around every section.

The futuristic feeling should come from precision, hierarchy,
typography, spacing, and subtle contrast.

### 11. Implementation priorities

Make changes in this order:

1.  Remove the Aurora bar.
2.  Establish the colour tokens and improve contrast between page,
    sidebar, content, and code surfaces.
3.  Introduce IBM Plex Mono consistently and refine typographic
    hierarchy.
4.  Redesign the homepage hero around the language and a representative
    code example.
5.  Refine code panels and syntax highlighting.
6.  Make navigation states consistent.
7.  Add restrained technical details only where they support the
    content.

### 12. Design principle

**A quiet, arctic, futuristic programming environment.**

The result should feel cohesive and purposeful: recognisably Nord, more
legible than the current dim presentation, and distinctive without
relying on loud decoration.
