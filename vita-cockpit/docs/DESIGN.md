# Design

The visual system. Read this before changing anything that renders.

Reference: **Cyberpunk 2077's in-game HUD.** Not the Edgerunners anime. The game
HUD is precise and restrained — thin rules, one chamfered corner, small tracked
labels over large values. The anime is louder and bloomier, and its bloom does
not survive a 5-inch screen.

---

## 1. Screen

Fixed 960 × 544 output, no scaling (D13). The *panel area* is a scrolling
viewport onto a taller canvas (D45), so regions no longer have to fit on one
screen. The header and the FRAME strip are pinned; only the band between them
moves.

```
┌──────────────────────────────────────────────────────────────┐
│ COCKPIT                                          14:22:07    │
├──────────────────────────────────────────────────────────────┤
│ CORE 552×210                 │ POWER 360×210                  │
│ ARM  333  GPU  166  BUS  166 │                                │
│ USER 179  CDRAM 41  PHY   64 │             87%                │
│ FREE USER MEM (MB)  171-193  │           CHARGING             │
│      ╱‾╲__╱‾╲__              │  TEMP  31   VOLT  3820         │
│                              │  CYC  214   SOH     98         │
├──────────────────────────────┼────────────────────────────────┤
│ STORAGE 552×172              │ SYSTEM 360×172                 │
│ UX0             12.4/29.5 GB │ FW                  3.65       │
│ ▓▓▓▓░░░░░░░░░░░░░░░░░░░░░░░  │ RUN           00:14:22         │
│ UMA0                   N/A   │ MODEL            PSVITA        │
│ ░░░░░░░░░░░░░░░░░░░░░░░░░░░  │                                │
├──────────────────────────────┴────────────────────────────────┤
│ ACTIVATED PLUGINS            grouped by load target          │
│ KUBRIDGE.SKPRX ──────────────────────────────── ▪            │
│ kernel-user bridge module                                    │
│ ...                                                          │
├──────────────────────────────────────────────────────────────┤
│ FRAME  58.4 FPS  17.1 MS   ─┼─────┼───┼──                   │ 44 pinned
└──────────────────────────────────────────────────────────────┘
```

The drawing is schematic; the table below is the exact geometry. The clock is
the only identity in the header. The device model lives in SYSTEM, so nothing
appears twice.

### Grid

| Region | x | y | w | h |
|---|---|---|---|---|
| Header | 0 | 0 | 960 | 64 |
| CORE | 18 | 0 | 552 | 210 |
| POWER | 582 | 0 | 360 | 210 |
| STORAGE | 18 | 222 | 552 | 172 |
| SYSTEM | 582 | 222 | 360 | 172 |
| PLUGINS | 18 | 406 | 924 | self-sizing (D47) |
| FRAME | 18 | 482 | 924 | 44 | (pinned, screen coords) |

Panel `y` is canvas coordinates; the app offsets them by the scroll and clips to
the band at y 64..472. The band carries a 12 px top inset, so the first panel
starts at screen y 76 and its outline is whole; the inset scrolls away. The
canvas is `406 + plugins_panel_height() + 12`:
729 for the four-plugin preview (two groups), 1,581 for the nineteen-plugin
console config (four groups).

Constants: `MARGIN 18`, `GAP 12`, `SP_3 12`, `VIEW_TOP_GAP 12`. Both columns
share both seams --
row one is 210 tall, row two 172 -- so the 12 px gap between the rows runs
unbroken across the screen and both columns end at canvas y 394. The strip
begins at screen y 482 and ends at 526, leaving an 18 px bottom margin.

### Vertical rhythm

Every vertical gap is one of six steps, declared in `src/gfx/theme.h`:

| Token | px | Role |
|---|---|---|
| `SP_1` | 4 | base unit; divides the 12 px column gap and the 18 px margin |
| `SP_2` | 8 | a readout to its meter or trace; a name to its description |
| `SP_3` | 12 | a hero number to its state; a panel to its neighbour |
| `SP_4` | 16 | one readout row to the next; the title rule to the first row |
| `SP_5` | 24 | last content to a panel's bottom edge |
| `SP_6` | 32 | reserved for a section break |

The first version used 2-7 px gaps against 21-35 px type, so a value, the row
below it and the caption under that read as one dense lump. Two rules now hold:
the gap between two blocks is always larger than the space inside a block, and
every gap is on the scale. A gap off the scale is a spacing bug the same way a
colour off the palette is a colour bug (D48).

Readout rows use `READOUT_STRIDE`, defined as `LINE_H(PX_VALUE) + SP_4` = 37.
Deriving the stride from the value height means changing the type ladder cannot
silently close the gap.

### Why it is not a grid of equal cards

Six identical panels in a 3 × 2 grid is the default dashboard, and it says
nothing about the machine. This layout has three tiers:

- **Hero** — CORE, 552 × 210, half again the area of any satellite. It carries
  the vital signs together because clocks and memory are read as one thing.
- **Satellites** — STORAGE, POWER, SYSTEM. Sized by how much they hold.
- **Status strip** — FRAME, full width but only 44 px tall, because it measures
  the app rather than the machine and should not compete with the machine.

Every region is sized by information importance. If a region grows, something
else must shrink.

## 2. Palette

| Token | Hex | Role |
|---|---|---|
| `C_FIELD` | `#000000` | screen field |
| `C_PANEL` | `#0D0E12` | panel fill |
| `C_HAIR` | `#1C1D24` | scrollbar track, empty meter cells |
| `C_DIM` | `#6E707E` | labels, absent values |
| `C_WHITE` | `#EDEDF0` | numeric values |
| `C_YELLOW` | `#FCEE0A` | primary — wordmark, hero accents |
| `C_CYAN` | `#2AD8E8` | machine data |
| `C_GREEN` | `#3BE07A` | status only: a loaded plugin marker (D47) |

`C_DIM` was `#43444F` until the host preview measured it at roughly 2:1 contrast
against `C_PANEL` — unreadable at 10 px on a 5-inch screen, and the clock
disappeared entirely when it fell back to `--`. The current value is about 4:1
(D20).

The field is true black, not a tinted near-black. On the OLED PCH-1000 this makes
panels float and saves power; on the LCD PCH-2000 it simply reads as dark.

Accent assignment:

| Region | Accent |
|---|---|
| CORE | yellow |
| STORAGE | cyan |
| POWER | yellow |
| SYSTEM | cyan |
| FRAME | cyan at 60 % |

An accent appears in exactly four places: the panel title, the frame outline at
45 %, the hero's corner brackets, and that panel's own meters and traces. It
never appears in a numeric readout — a number is always white, so emphasis can
never be mistaken for meaning (D19).

The earlier rule said "never in the data". That contradicted the accent table
above: if an accent only ever appeared in a title and an outline, the region
colour coding would be invisible. Meters and traces are the region's own
signal, so they carry it; numbers do not.

## 3. Typography

A hand-built 5 × 7 bitmap face (`src/gfx/font.c`). No font ships with VitaSDK
2026.08, and a TTF would add a licence and ~200 KB for no gain at these sizes.
Block glyphs also render as crisp rectangles under additive glow.

Sizes are *font-pixel scale*: `px=2` means one font pixel is a 2 × 2 screen rect.
A glyph is `5·px` wide, `7·px` tall, advancing `6·px`.

| Role | px | Box | Used for |
|---|---|---|---|
| micro | 1.5 | 7.5 × 10.5 | units, charging state, trace caption, plugin description, group header |
| label | 2 | 10 × 14 | metric names, panel titles |
| value | 3 | 15 × 21 | numbers in a row |
| hero | 4 | 20 × 28 | wordmark |
| primary | 5 | 25 × 35 | the one big number in a panel |

Rules:

- Uppercase everywhere; lowercase is folded by the renderer.
- Labels are capped at 10 characters so the label column never reflows.
- Panel titles get one extra font-pixel of tracking (`font_text_tracked`).
- Two sizes share a baseline only in a readout, where the label is
  bottom-aligned to its value. Never top-align them.
- A secondary line (a plugin's description) is one step down the ladder from
  the line it explains, never the same size.

## 4. Panel anatomy

**Satellite** — one chamfer, no brackets:

```
 ┌──────────────────────────────┐╲     chamfer 14 px, top-right
 │▏STORAGE                      │ ╲
 │──────────────────────────────│     hairline at 25 % accent
 │                              │
 │  UX0            12.4/29.5 GB │     label px=2 dim, value px=3 white
 │  ▓▓▓▓░░░░░░░░░░░░░░░░░░░░░░  │     meter below, full content width
 │  UMA0                  N/A   │
 │  ░░░░░░░░░░░░░░░░░░░░░░░░░░  │
 └──────────────────────────────┘
```

**Hero** — larger chamfer, a second cut at the bottom-left, and brackets:

```
 ╱──────────────────────────────────────────────┐╲   28 px chamfers
 │▏CORE                                         │ ╲
 │──────────────────────────────────────────────│
 │  ARM    333     GPU    166     BUS    166    │
 │  USER   182     CDRAM   41     PHY     64    │
 │  ▁▂▃▄▅▆▇█▇▆▅▄▃▂▁▂▃▄▅▆▇▆▅▄▃▂▁                │
 ╲──────────────────────────────────────────────┘
  ↑ brackets, 16 px, full accent, on the two square corners
```

The extra cut and the brackets are the only things that distinguish the hero from
a satellite. Hierarchy is carried by size and corner treatment, not by colour or
decoration.

## 5. Readouts

The core pattern:

```
LABEL                     VALUE
px=2 dim                  px=3 white, right-aligned
```

Inside a multi-column block (CORE), a column is `theme_readout(label, value,
valid, x, y, width)`: label at `x`, value right-aligned to `x + width`. One
function serves both a full-width row and a column, because they differ only in
the width.

A panel's primary number may escalate to `px=5`. Only one per panel, and it
stays `C_WHITE`: scale carries the emphasis, not colour (D19). In CORE the
escalated element is the trace, not a number.

SYSTEM is the exception that proves the rule: its firmware row stays at `px=3`
because the version string is unbounded, and at `px=5` a ten-character version
overprints its own caption (D27). A primary number is only safe where the value
has a known width.

Meters are segmented, ten cells, filled at full accent and empty at `C_HAIR`.
Segments communicate discrete capacity; a continuous bar would imply precision
the data does not have. A meter with no reading draws its empty track at 70 % of
`C_HAIR`: at 45 % it rendered darker than the panel fill and vanished, which is
the opposite of what an absent track is for.

In STORAGE the meter sits *below* its readout and spans the panel's content
width, so a volume reads as one block: name, size, capacity. The first version
put the meter inline at x=320, which stranded the value mid-panel and forced the
two rows into a 34 px stride -- too short for the row-two grid (D48).

STORAGE is the one place `--` is not used. A mount that is not present shows
`N/A`, and a refused read shows `ERR` plus its errno byte. "No volume in this
slot" and "this metric could not be read" are different statements, and an empty
row is never acceptable (D41).

### Plugin ledger

ACTIVATED PLUGINS is a two-line ledger, grouped by the load target the plugin
was listed under: KERNEL, SYSTEM (`*main`), SYSTEM APPS (`*NPXS*`), ALL TITLES
(`*ALL`), and TITLES for any other target. Each non-empty group opens with a
micro, 1 px-tracked cyan header. A file listed under several targets appears
once, under the most privileged one, so the panel answers what is running rather
than how many lines the file contains (D51).

A row is the file name at label scale, a leader rule to a green status dot, then
the description one step down at micro scale. An entry outside the description
catalog says `N/A` instead of leaving the second line empty (D52). The dot is
`C_GREEN`, the only status hue in the palette; every row in the panel is loaded,
because the scan drops entries whose file is missing. Height is
`41 + 26 × groups + 52 × rows + 10 × (groups − 1)`, so the panel grows with the
list instead of clipping it (D47, D48). A config that exceeds the 64-entry
ceiling adds one line for the `MORE PLUGINS NOT SHOWN` notice (D50).

Traces are a 2 px line, not a filled area. At 200 samples across ~500 px an
area chart collapses into a solid block that hides the shape it exists to show
(D21). A trace is always scaled to an axis that is printed beside it, so an
autoscaled trace declares its range instead of implying one.

## 6. Motion

| Motion | Trigger | Purpose |
|---|---|---|
| Trace scroll | sample rate | carries 20 s of history |
| Interference sweep | every ~5 s | proves the render loop is running |
| Wordmark chroma | static | the one loud element |

Nothing else animates. No value pulses, no hover states, no easing curves.

## 7. Restraint

The screen has exactly one loud element: the wordmark. Everything else is a thin
rule, a small label, or a number.

Two devices that would otherwise be decoration are load-bearing instead:

- **The hazard band is an alert channel.** It occupies the header's bottom 8 px
  and renders only when something is wrong — battery under 20 % while
  discharging, a failed memory sample, or `ux0:` not answering. It never reflows
  the layout. On a healthy console it is absent and the screen is quiet.
- **The hazard band is the only alert, and it is yellow.** There is no second
  alert colour. The band is distinguished by its pattern — hard diagonal stripes
  — not by a hue that appears nowhere else, so it cannot be confused with
  decoration and nothing else can be mistaken for it.

## 8. What not to do

- Do not return to a uniform grid. The asymmetry is the design.
- Do not introduce a second alert colour. The band is the alert.
- Do not put brackets on a satellite. They mark the hero.
- Do not add a third chamfer size.
- Do not glow anything except the wordmark and the hazard band.
- Do not introduce a size between the five above.
- Do not animate a value that did not change.
- Do not draw a metric the platform cannot report. `--` is correct; a guess is
  not.
- Do not add an icon, glyph, or shape that encodes nothing.
- Do not fill a trace as an area. A 2 px line, scaled to a printed axis.
- Do not colour a number. Emphasis in a readout is size, never hue.
- Do not use `C_GREEN` anywhere but the plugin dot. It means "loaded", not
  "look here".
