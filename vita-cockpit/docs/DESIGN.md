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
│ COCKPIT      HOME  PLUGINS  L/R                  14:22:07    │
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
├──────────────────────────────────────────────────────────────┤
│ PLUGINS 924×140   (on the canvas, below both columns)        │
│ 7  PLUGINS ONLINE                                            │
│    KERNEL 3  SYSTEM 2  SYSTEM APPS 1                         │
├──────────────────────────────────────────────────────────────┤
│ FRAME  58.4 FPS  17.1 MS   ─┼─────┼───┼──                   │ 44 pinned
└──────────────────────────────────────────────────────────────┘
```

The PLUGINS tab replaces the panel area with a full-width ledger; the header and
FRAME strip stay pinned. The drawing is schematic; the table below is the exact
geometry. The clock is the only identity in the header. The device model lives
in SYSTEM, so nothing appears twice. The PLUGINS block is the last canvas panel:
at scroll 0 it sits just below the fold, and the scrollbar is the affordance
that reaches it (D67).

### Grid

| Region | x | y | w | h |
|---|---|---|---|---|
| Header | 0 | 0 | 960 | 64 |
| CORE | 18 | 0 | 552 | 210 |
| POWER | 582 | 0 | 360 | 210 |
| STORAGE | 18 | 222 | 552 | 172 |
| SYSTEM | 582 | 222 | 360 | 172 |
| PLUGINS | 18 | 406 | 924 | 140 |
| FRAME | 18 | 482 | 924 | 44 | (pinned, screen coords) |

Panel `y` is canvas coordinates; the app offsets them by the scroll and clips to
the band at y 64..472. The band carries a 12 px top inset, so the first panel
starts at screen y 76 and its outline is whole; the inset scrolls away. The home
canvas is `546` (the PLUGINS lower edge), and `layout_max_scroll()` is 150
because `546 > 408 - 12`. The PLUGINS tab uses its own canvas:
`plugins_content_height()` starts at 0 and grows with the rows, groups, and
orphan section; its scroll range is `height - (VIEW_H - VIEW_TOP_GAP)`.

Constants: `MARGIN 18`, `GAP 12`, `SP_3 12`, `VIEW_TOP_GAP 12`. Both columns
share both seams -- row one is 210 tall, row two 172 -- so the 12 px gap between
the rows runs unbroken across the screen and both columns end at canvas y 394.
The summary block starts 12 px below them and ends at 546. The strip begins at
screen y 482 and ends at 526, leaving an 18 px bottom margin.

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
| `C_YELLOW` | `#FCEE0A` | primary — wordmark, hero accents, active tab, warning, missing/partial status, pending |
| `C_CYAN` | `#2AD8E8` | machine data, group headers, boot status, disable control |
| `C_GREEN` | `#3BE07A` | status only: activated plugin, activated count, enable control |

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
| micro | 1.5 | 7.5 × 10.5 | units, charging state, trace caption, plugin description, status badge, action label, group header, modal body |
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

### Tabs

The 64 px header carries the tabs between the wordmark and the clock. `HOME` and
`PLUGINS` are label-scale, 1 px tracked; the active tab is `C_YELLOW` with a 2 px
underline at y 36, the inactive tab is `C_DIM`, and `L/R` sits beside them. The
header carries no plugin count: that is a panel on HOME (D67). The hazard band
sits at y 48, the line below the tabs, so a fault and the tabs coexist.

### PLUGINS summary block

The block is a satellite panel at x 18, w 924, below both columns at canvas y
406, 140 px tall. It is one column of information, read left to right and top to
bottom:

- a primary-scale count (`7`) at the content inset, tinted `C_GREEN` when the
  count is non-zero and `C_WHITE` at zero;
- `PLUGINS ONLINE` at label scale beside it, aligned to the count's bottom
  (singular `PLUGIN ONLINE` for one);
- one micro `C_DIM` line of non-empty group counts in load order: `KERNEL 3
  SYSTEM 2  SYSTEM APPS 1  ALL TITLES 1  TITLES 0`. A config with no activated
  plugins says `NO PLUGINS LISTED` instead of an empty line.

`N PENDING` sits right-aligned on the group line in `C_YELLOW`, and
`CONFIG WARNING` follows it when a stray path or a truncated row was seen. The
count is always the launch set: a pending edit has not changed what taiHEN
loaded, so it must not change the number (D59, D67).

A config problem replaces the count with `--` and names the state beside it:
`NO CONFIG.TXT` with `UR0:TAI OR UX0:TAI`, `CONFIG UNREADABLE` or
`CONFIG TOO LARGE` with `READ ONLY`, all in `C_YELLOW`.

The block is read-only; every action lives on the PLUGINS tab. It is fixed in
height because its content is a summary, and it sits below the fold at scroll 0,
so HOME carries a scrollbar whose thumb reaches it. The block has no ceiling to
hit: the group line is one line regardless of plugin count.

### PLUGINS tab

The tab is a full-width panel at x 18, w 924, starting at canvas y 0. Its height
is computed, not compiled in: `41 + 14 × 2 + 8 + 26 × groups + 52 × rows +
10 × (groups − 1)` plus the empty state or orphan section and a 16 px bottom
margin. The band clips it and the scrollbar follows `plugins_max_scroll()`.

The panel header has two lines. The first names the active config and, when both
exist, the ignored one: `SOURCE ur0:tai/config.txt  IGNORED ux0:tai/config.txt`.
The second is the state: `9 ROWS  7 ACTIVATED  0 PENDING  ORIGINAL OK  SESSION OK`.
A read-only file replaces it with `READ ONLY - <reason>`; a backup failure adds
`BACKUP WARNING` but leaves the rows editable.

Rows are grouped by the most privileged line that names the path: KERNEL,
SYSTEM (`*main`), SYSTEM APPS (`*NPXS*`), ALL TITLES (`*ALL`), and TITLES. Each
non-empty group opens with a micro, 1 px-tracked cyan header. A path listed under
several targets is one row (D51, D56).

A row is a two-line entry. The first line is a 6 px status square, the display
name at label scale (micro and truncated if it does not fit), and the action
control at the right. The second line is the catalog description or `N/A` at
micro scale on the left and the status badge on the right. The name is the
basename; when two rows share a basename the full path is shown.

Status badges are `ACTIVATED` (`C_GREEN`), `DISABLED` (`C_DIM`), `MISSING`
(`C_YELLOW`), `PARTIAL` (`C_YELLOW`), `NOT REACHED` (`C_DIM`), and `BOOT`
(`C_CYAN`). The badge is a 12 px tinted fill in the status colour with the label
in the same colour. A pending change adds a yellow `PENDING` badge to its left
(D66).

The action control is a 78 × 16 box at the first line's right. `ENABLE` is
`C_GREEN`, `DISABLE` is `C_CYAN`; `LOCKED`, `MISSING`, `BOOT`, and `--` are
`C_HAIR` fill with `C_DIM` text and perform nothing. Cross on a greyed control
opens `CANNOT CHANGE` with `editor_action_reason()`; it never writes. A missing
active row can be disabled; a missing inactive row cannot be enabled (D66).

The selection cursor is a 2 px `C_WHITE` left bar spanning both lines, dimmed on
a row the policy will not act on. Every row is selectable. The cursor owns the
scroll; at the last row a further Down scrolls the canvas to the orphan section,
and Cross is ignored while the selected row is off-screen.

The orphan section follows the last group under the header
`INSTALLED - NOT IN CONFIG`. Its caption is `NOT LISTED IN THE ACTIVE CONFIG`
when the listing is complete; a failed directory read replaces it with
`EXCLUSIONS UNVERIFIED`. `boot_config.txt` is not consulted, so the section never
claims a file is not loaded by Enso. Files in a shadowed directory are prefixed
`SHADOWED`. The list is sorted, capped at 32, and states `MORE FILES NOT SHOWN`
when the cap is reached. Orphans are read-only (D61).

A confirmation is a centred 600 × 220 `theme_panel` with `CHROME_SATELLITE`, the
action and file in the title (`DISABLE STORAGEMGR.SKPRX`), the message wrapped at
the panel inset in micro scale, and `CROSS CONFIRM  CIRCLE CANCEL` at the bottom.
An error uses the same panel with `NOT WRITTEN` and `CROSS CLOSE`; its body is
`WRITE FAILED` followed by the stage (`OPEN`, `DATA`, `CLOSE`, `VERIFY`, `MOVE`,
`RENAME`, `READBACK`, or `RESTORE`). An information modal uses `CANNOT CHANGE`.
The modal is drawn after the FRAME strip, so it is the topmost surface.

The active config has two snapshots: `config.txt.original` is written once and
never overwritten; `config.txt.session` is the session-start file and is
rewritten each session. No `.bak`, `.1.bak`, or `.2.bak` is created. The adapter
writes a verified `.new`, moves the original to `.old`, moves the sibling into
place, and reads the live file back; a failure restores the original. The raw
transaction files are excluded from the orphan list (D61, D64). `boot_config.txt`
is never opened, read, copied, or written.

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

- **The hazard band is an alert channel.** It sits at y 48, on the line below the
  tabs, and renders only when something is wrong — battery under 20 % while
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
- Do not use `C_GREEN` for a decorative highlight. It means "activated" or
  "possible action", never "look here".
