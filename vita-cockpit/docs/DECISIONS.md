# Decision log

Architecture and design decisions, in the order they were made. Each entry
records what forced the choice and what it costs, so a future reader can tell a
deliberate trade from an oversight.

**Status values:** Accepted · Superseded by D<n> · Proposed

---

## D1 — Panels are a registry, not a switch

**Status:** Accepted
**Context:** COCKPIT must grow into a full dashboard. A single `draw_screen()`
function would grow without bound and every panel would be able to reach into
every other.
**Decision:** A panel is data — a `Panel` struct with a title, an accent, and one
draw function — collected in `g_layout[]`.
**Consequence:** Adding a panel is one file and one registry line. Costs one
indirection per panel per frame, which is unmeasurable at five regions.
**Rejected:** a `switch` in the loop (no isolation), and a virtual filesystem of
plugins (absurd for this size).

## D2 — A hand-built 5×7 bitmap font

**Status:** Accepted
**Context:** VitaSDK 2026.08 ships no font file. Bundling a TTF means a licence,
~200 KB, and a freetype path at runtime.
**Decision:** A glyph table in `gfx/font.c`, with sizes expressed as integer
font-pixel scale.
**Consequence:** ~300 bytes of glyph data, zero dependencies, and crisp block
type that suits additive glow. Costs: no kerning, Latin only, new glyphs added by
hand.
**Rejected:** the system PGF (proportional, wrong voice), a bundled TTF (weight
and licence for no gain at HUD sizes).

## D3 — Glow by additive blending, not shaders

**Status:** Accepted (revised)
**Context:** vita2d exposes no custom shader path.
**Decision:** `vita2d_set_blend_mode_add(1)`, exposed as
`draw_set_additive()`. A shape that should glow is drawn more than once at
falling intensity while the mode is on.
**Consequence:** Real bloom from a handful of extra draw calls, with no shader,
no render target, and no post-process pass.

Revised from "expanding translucent rects", which described a helper nothing
ended up calling. The only glowing shape is the wordmark, and it builds its halo
from four offset additive passes of its own glyphs; a generic `draw_glow_rect`
was removed rather than kept unused (rule 9). The falloff is stepped rather than
smooth, which is invisible at this scale.

## D4 — Cyberpunk 2077's game HUD, not the anime

**Status:** Accepted
**Context:** Both were named as references. They are different visual systems:
the game HUD is restrained, the anime is bloom-heavy.
**Decision:** Follow the game HUD.
**Consequence:** Legibility survives a 5-inch screen. The anime's bloom would
have made small labels unreadable.

## D5 — Yellow leads; red is reserved

**Status:** Accepted
**Context:** A HUD with several saturated colours has no way to signal urgency.
**Decision:** Yellow is the only saturated colour used at rest. Red means the
machine needs attention.
**Consequence:** A red screen is unambiguous. Costs colour variety.

## D6 — No CPU load percentage

**Status:** Accepted
**Context:** No userland API reports system-wide CPU utilisation. `psp2/power.h`
exposes clock *frequency*, not load.
**Decision:** Show clock frequency and the app's own frame time. Do not
synthesise a percentage.
**Consequence:** Every number is real. A fabricated "CPU 47 %" would discredit
the rest of the panel.
**Rejected:** deriving load from idle-time sampling, which would measure this
process, not the system.

## D7 — Platform access behind a port

**Status:** Accepted
**Context:** The sampling policy — unit conversion, alert thresholds, history
cadence — is the part most likely to contain a bug, and no Vita is available to
test it on. Calling the SDK from `core/metrics.c` would make that policy
untestable.
**Decision:** `core/platform.h` declares raw readings in plain C types; the SDK
call lives in `platform/platform_psp2.c`.
**Consequence:** The policy compiles and runs against a fake platform on the
host. Costs one indirection and one extra file.
**Rejected:** full four-circle clean architecture with interactors and presenters
— ceremony for a 1,500-line single-screen app. This is a partial boundary by
design (see [ARCHITECTURE.md §4](ARCHITECTURE.md#4-the-port)).

## D8 — `[C]` platform quirks live in one file

**Status:** Accepted
**Context:** `SceIoDevInfo`, the device model constants, and the storage unit are
all absent from or unconfirmed by the installed headers. They are community
knowledge.
**Decision:** All three are declared in `platform/platform_psp2.c` with `[C]`
comments, and nowhere else.
**Consequence:** Correcting any of them after hardware testing is a one-file
change that cannot reach into policy or presentation.

## D9 — Safe eboot

**Status:** Accepted
**Context:** Nothing in COCKPIT needs private or system APIs.
**Decision:** Default `vita_create_self`, authid `0x2F00000000000002`.
**Consequence:** No "Enable Unsafe Homebrew" requirement and no install warning.
**Rejected:** `UNSAFE` for convenience, which would relax the whole system for no
gain.

## D10 — Two sampling cadences

**Status:** Accepted
**Context:** Frame time needs frame resolution. Storage and battery syscalls do
not, and are not free.
**Decision:** Frame metrics every frame; everything else at 10 Hz.
**Consequence:** Slow syscalls stay out of the frame path. Costs up to 100 ms of
lag on the wall clock, which is invisible.

## D11 — Layout is a compile-time rect table

**Status:** Accepted (revised; the tab composer is D54)
**Context:** One screen, one resolution, five regions of unequal size.
**Decision:** `SCREEN_W`, `SCREEN_H`, `MARGIN`, `GAP` in `gfx/layout.h`, and an
explicit `PanelSlot` table in `panels/registry.c` giving each panel its rect.
**Consequence:** The layout is one readable table and can be asymmetric. Adding a
region means deciding which other region gives up space; there is no reflow.
**Superseded:** an earlier version computed rects from a column/row index, which
could only ever produce equal cells.

## D12 — No persistence in v1

**Status:** Superseded by D54-D66 (the plugin manager writes the active config
and its two snapshots; the metrics history is still session-only)
**Context:** The app measures; it does not record.
**Decision:** COCKPIT writes nothing to disk.
**Consequence:** It cannot corrupt a memory card, needs no save-data setup, and
history resets on launch. Costs the ability to show long-term trends.

## D13 — Fixed 960×544, no scaling

**Status:** Accepted
**Context:** The Vita has exactly one screen size.
**Decision:** Draw at native resolution with no virtual canvas.
**Consequence:** Pixel-exact HUD. A future resolution change is a layout edit,
not a config value.

## D14 — History capacity 200 samples

**Status:** Accepted
**Context:** The trace should show enough history to be useful without holding a
meaningful amount of memory.
**Decision:** `HISTORY_CAP = 200`, pushed at 10 Hz.
**Consequence:** A 20-second window in 800 bytes per trace. Costs nothing.

## D15 — The hazard band is an alert channel

**Status:** Accepted
**Context:** Yellow/black hazard stripes are a Cyberpunk signature and the
obvious thing to put under the header as ornament.
**Decision:** It renders only when a real condition holds — battery under 20 %
while discharging, a failed memory sample, or `ux0:` not answering.
**Consequence:** A band on screen means something. On a healthy console the
screen is quiet. This is the difference between a signal and wallpaper.

## D16 — `g_metrics` is a global with one writer

**Status:** Accepted
**Context:** One screen, one snapshot, one sampler.
**Decision:** A single `Metrics g_metrics` is written only by `core/metrics.c`
and read by panels.
**Consequence:** No plumbing of a 500-byte struct through every call. Costs
global mutable state, which is acceptable while there is exactly one writer and
one consumer set. If COCKPIT ever gains a second screen or a background sampler,
this becomes a bug and must be revisited.

## D17 — Asymmetric layout: hero, satellites, status strip

**Status:** Accepted
**Context:** The first layout was six identical panels in a 3 × 2 grid. That is
the default shape of every monitoring dashboard, and it encodes nothing about the
machine — clocks, storage, and a frame counter all received the same 300 × 220.
**Decision:** Three tiers. CORE is the hero at 552 × 240 (210 since D48) and carries clocks and
memory together. STORAGE, POWER, and SYSTEM are satellites sized by how much they
hold. FRAME is a 44 px full-width status strip, because it measures the app
rather than the machine and should not compete with it.
**Consequence:** Area tracks importance and the screen reads as a HUD over a
machine rather than a grid of cards. Costs: the layout cannot be derived from a
grid index (D11), and adding a region is a manual negotiation for space.
**Rejected:** a uniform grid (generic, no hierarchy) and a fully free-form layout
(no predictability; the header and strip would drift between revisions).

## D18 — Chrome marks hierarchy; it is not decoration

**Status:** Accepted
**Context:** Corner brackets were initially drawn on all six panels. A mark that
appears on everything marks nothing, and identical framing made the hero
indistinguishable from a battery readout.
**Decision:** The hero gets a 28 px chamfer, a second cut at the bottom-left, and
16 px corner brackets on its two square corners. Satellites get a 14 px chamfer
and no brackets. `theme_panel()` takes a `PanelChrome` argument so the
distinction is drawn by the theme, not decided by each panel.
**Consequence:** Hierarchy is visible in the frame itself, before any number is
read. Costs one enum and a branch in the frame routine.
**Rejected:** signalling the hero with colour. Colour is already spoken for —
yellow is primary and red is alert (D5).

## D19 — Accents reach meters and traces, never numbers

**Status:** Accepted (amends D5's "never in the data")
**Context:** The palette assigns an accent per region, but an earlier rule said an
accent may appear only in the title, the frame outline, and the hero brackets.
Both cannot hold: if the accent never touched a panel's own content, the region
colour coding would be invisible, and STORAGE and CORE would differ only by a
word. The first render made the contradiction obvious — the hero trace is the
panel's whole point and it looked wrong in plain white.
**Decision:** An accent may colour that panel's meters and traces. It may never
colour a numeric readout; numbers are always `C_WHITE`, and emphasis in a
readout is size (`px=5`) rather than hue.
**Consequence:** Region identity is visible at a glance, and a coloured number
still means nothing in particular, so red keeps its monopoly on "wrong". Costs a
slightly longer rule.

## D20 — `C_DIM` was raised to a measured contrast

**Status:** Accepted
**Context:** Labels and absent values were `#43444F` on `#0D0E12`. The host
preview made the failure visible: at 10 px the labels were barely present, and
the clock vanished entirely when it fell back to `--:--:--`.
**Decision:** `C_DIM` is `#6E707E`, about 4:1 against the panel fill instead of
about 2:1.
**Consequence:** Labels and `--` are legible at arm's length on a 5-inch screen
while staying clearly secondary to `#EDEDF0`. Costs some of the "almost
invisible" texture the darker value gave.

## D21 — A trace is a line with a declared axis

**Status:** Accepted
**Context:** The first implementation filled the area under each sample, as the
design sketch's block characters suggested. At 200 samples across ~500 px it
rendered as a solid rectangle that hid the shape it existed to show. Scaling the
hero trace from zero was worse: free memory sits near capacity, so the line
pinned itself to the top edge with 80 px of empty box beneath it.
**Decision:** Traces draw a 2 px polyline, joined vertically between samples so a
one-sample spike is a spike. The hero trace is scaled to the window's own
range, and that range is printed beside it. The FRAME trace keeps a fixed
0–33.3 ms axis, because there a spike means a dropped frame and that meaning
depends on the scale not moving.
**Consequence:** The trace shows shape, and because the axis is on screen the
autoscale is declared rather than implied. Costs a caption and a min/max pass
over 200 samples at 10 Hz.

## D22 — The host preview is a verification tool, not a build target

**Status:** Accepted
**Context:** No Vita is reachable from this environment, so nothing about the
layout could be checked. The port in D7 was justified partly by host testability,
and until this point that had not been used.
**Decision:** `tools/preview/` links the real `core`, `gfx`, `panels`, and `ui`
against a fake platform and a software stand-in for the four vita2d calls
`gfx/draw.c` makes, then writes PNGs.
**Consequence:** Layout, typography, palette, and the absent-value path can all be
inspected without hardware, and three real defects were caught this way. Costs a
second implementation of the four draw calls, which must be kept in step with
`gfx/draw.c`. It is never linked into `cockpit.self`, and it proves nothing about
the device: every reading it shows is synthetic.

## D23 — A failed power read is an alert condition

**Status:** Accepted (supersedes the condition list in D15)
**Context:** Code review found that `update_alert()` rebuilt the alert from zero
on every sample, and the only sensor-driven arm was gated on `power_valid`. The
moment `scePowerGetBatteryLifePercent()` started returning a negative value the
hazard band cleared, even though the battery was still low: a reader failure
silently cancelled an alert that was already true.
**Decision:** `!power_valid` raises the band on its own, alongside `!mem_valid`,
low battery while discharging, and `ux0:` not answering.
**Consequence:** The alert channel fails closed for the one reading that matters
most. Costs a fourth condition, and a console whose power API is broken shows a
band permanently — which is the correct signal for a broken power API.

## D24 — Screen geometry lives in `gfx/layout.h`

**Status:** Accepted (supersedes the location named in D11)
**Context:** D11 put `SCREEN_W`, `SCREEN_H`, `MARGIN` and `GAP` in `app/app.h`.
`panels/registry.c` and `ui/header.c` both need them and both sit inward of
`app/`, so neither could include them without inverting the dependency rule. The
result was that `MARGIN` and `GAP` had no readers at all: `registry.c` hardcoded
`18.0f` and `header.c` defined a private `HEADER_MARGIN`. Editing the constant
changed nothing and raised no error.
**Decision:** The four constants live in `gfx/layout.h`, which both consumers may
include. `registry.c` expresses its rects through `MARGIN` and `GAP`.
**Consequence:** A constant its consumers cannot reach is a constant that gets
hardcoded instead. Costs one header, and a second place to look for geometry.

## D25 — Catch-up is bounded; a stall's backlog is dropped

**Status:** Accepted (reverses "catch up rather than drop" from the original loop)
**Context:** The loop repaid every 100 ms of accumulated slow-sample debt inside
a single frame, with no cap. A long stall — suspend/resume is the everyday case —
became thousands of blocking devctl/power/RTC calls in one frame, and each
iteration pushed into the 200-sample ring, so the stall overwrote the whole
20-second trace with duplicates of the current value.
**Decision:** `dt` is clamped to `MAX_FRAME_DT` (0.5 s), at most
`MAX_CATCH_UP_SAMPLES` (3) slow samples run per frame, and any remaining debt is
dropped rather than replayed.
**Consequence:** A stall costs at most three samples and a visible gap in the
trace, which is honest. Costs the guarantee that every 100 ms of wall time is
represented in the history.

## D26 — The preview renders the shipped composition and drives the real sampler

**Status:** Accepted (supersedes the preview's independence in D22)
**Context:** The preview kept its own copy of `app.c`'s draw loop and set
`g_metrics` fields directly. Both were load-bearing failures: changing the draw
order left the preview rendering the old composition and still passing, and the
alert policy was never executed by anything — deleting all three arms of
`update_alert()` left the PNGs byte-identical.
**Decision:** `screen_draw()` moves to `app/screen.c`, which makes no SDK call, so
both `app.c` and the preview call it. Scenario control moves into a shared
mutable fake (`tools/fake/`), and the preview produces each scene by driving the
fake port through `metrics_sample_slow()`.
**Consequence:** A preview PNG now proves the policy ran. Costs the preview its
independence from the app's composition, which is the point.

## D27 — Readout scale is a parameter; the trace axis is computed at 10 Hz

**Status:** Accepted
**Context:** Three related defects. `theme_readout` fixed the px=2/px=3 pair, so
`storage.c` and `system.c` re-derived the baseline rule by hand; the SYSTEM
firmware row hand-rolled a px=5 value that overprints its own caption at ten
characters; and `draw_trace()` rescanned 200 samples twice per frame to build an
axis that only changes at the sample rate.
**Decision:** `theme_readout_px` takes both scales and `theme_readout` wraps it at
px=2/px=3. SYSTEM's firmware value is px=3, like every other readout in that
panel. The trace axis is computed once per slow sample and cached in `Metrics`.
**Consequence:** One baseline rule, one place to change a scale, and a draw loop
that does no history scanning. Costs two fields in `Metrics` and a SYSTEM panel
without a primary number.

## D28 — Static text is baked; glyphs and traces are batched

**Status:** Accepted (implements the deferred performance finding)
**Context:** One frame issued ~2,818 draw calls, of which 462 were the wordmark
alone: seven passes over a seven-letter constant string, each glyph run its own
`vita2d_draw_rectangle`. Two traces added 400 more. On a Cortex-A9 each call is
seven GXM entry points plus a pool allocation plus one `sceGxmDraw`.
**Decision:** `draw_rects()` batches same-coloured rects into one
`vita2d_draw_array`. `font_rects()` enumerates a string's glyph rects without
drawing, so the wordmark's 66 rects are baked once and replayed as four batched
passes (halo, chroma left, chroma right, solid). Every glyph draws its runs as
one batch, and each trace is one batch. `tools/preview` counts draw calls and
fails over a 600-call budget.
**Consequence:** A full frame with the hazard band up went from ~2,818 calls to
381, with identical pixels. Costs a baked rect list in `header.c` that must be
rebuilt if the wordmark's position or scale changes — it is keyed on
`s_wordmark_count <= 0`, so moving the wordmark means resetting that.
**Rejected:** baking the wordmark into a render-target texture. It would cut the
same calls but needs a GXM render target and hardware to confirm the result, and
the batching gets most of the win with no device-specific risk.

## D29 — Vertex data comes from the vita2d pool, never from a static buffer

**Status:** Accepted (fixes the first hardware crash)
**Context:** The first build installed and launched, then faulted the GPU on the
first frame. Disassembling `libvita2d.a` showed why: `vita2d_draw_array` calls
`sceGxmSetVertexStream(ctx, 0, vertices)` with the caller's pointer and performs
no copy. GXM reads that stream when the scene is submitted, so the buffer must
live in GPU-mapped memory until the frame ends. `draw_tri` passed a stack array
and `draw_rects` a BSS array; neither is mapped. `vita2d_draw_rectangle` and
`vita2d_draw_line` are safe because they allocate from `vita2d_pool_memalign`
internally — which is the pattern the two direct callers should have followed.
The two pre-fix GPUCRASH dumps in `gpu-crash-study/` confirm the stack half of
this: both fault on the last 4 KiB page of the main thread stack
(`stack+0x3f000`), with the PDS vertex-data/uniform-fetch requestor
(`gpu-crash-study/ANALYSIS.md`).
**Decision:** `draw_tri` and `draw_rects` allocate their vertices from
`vita2d_pool_memalign`. The pool is sized explicitly at 2 MiB in `main()`, since
vita2d does not check the allocation and a null block would fault.
**Consequence:** Both draw paths are valid on device. Costs one pool allocation
per batch, which is the same thing vita2d does per primitive.

**Why the host preview did not catch this.** The software shim reads vertices
immediately and renders into a plain array, so it never models GXM's deferred
read or its address-space rules. A stack pointer and a pool pointer look
identical to it. The preview now models *pool exhaustion* — it uses a bump
allocator the size of the device pool and fails the run if an allocation misses —
but address-space validity remains outside what any host shim can prove. That
gap is the reason a hardware session is still the only verification for this
class of defect.

## D30 — The render pool is sized explicitly, not defaulted

**Status:** Accepted
**Context:** Every vertex a frame submits is allocated from the vita2d pool and
must stay valid until scene submit. The host preview measures the worst-case
frame at 260 KiB (the alert scene, with both traces full and the hazard band up).
**Decision:** `main()` calls `vita2d_init_advanced(RENDER_POOL_BYTES)` with 2 MiB
rather than relying on `vita2d_init()`'s default, which the headers do not state.
**Consequence:** The pool is ~8x the measured worst case, leaving room for more
panels. Costs 2 MiB of GPU-mapped memory, which is trivial against the app
budget.

## D31 — The renderer uses only `vita2d_draw_rectangle` and `vita2d_draw_line`

**Status:** Accepted (supersedes the batching in D28; keeps D29's rule)
**Context:** The first hardware build faulted the GPU because `vita2d_draw_array`
was given non-mapped vertex memory (D29). After that was fixed with pool
allocations, the app launched but drew no text, no trace and no hazard stripes —
everything routed through `vita2d_draw_array` — while the panel chrome, drawn
with `vita2d_draw_rectangle` and `vita2d_draw_line`, rendered correctly. The call
graph, arguments, pool size (2 MiB, confirmed allocated) and primitive enum were
all verified correct in the shipped binary, so the defect is inside the
`vita2d_draw_array` path rather than in this code, and it cannot be reproduced in
the host shim.
**Decision:** `draw_rects` is a loop of `vita2d_draw_rectangle`. `draw_tri` is a
scanline fill that emits one horizontal rect per row. Neither `vita2d_draw_array`
nor `sceGxmSetVertexStream` is called anywhere in the renderer.

The GPU crash dumps in `gpu-crash-study/` confirm the first half of this: both
COCKPIT faults are `GPU_BIF_ACCESS_FAULT` on the last 4 KiB page of the main
thread stack, which is the GPU fetching `draw_tri`'s stack vertex array. The
second half — no output from pool-backed `draw_rects` — is unexplained by the
dumps, because that build did not fault. Removing the primitive removes both.
**Consequence:** The renderer is back to ~2,900 draw calls per frame (3,641 with
the hazard band up) from 381, and every primitive it uses is one the device has
already proven. The chamfer cuts and hazard stripes are now stair-stepped
diagonals rather than true triangles, which is indistinguishable at this scale.
Costs the performance work in D28, which stays recorded: it can return once a
hardware session proves `vita2d_draw_array` works.

**Lesson for the host harness:** the preview proved geometry and typography, and
caught real defects, but it cannot model GXM's address-space rules or its
deferred vertex reads. Two consecutive hardware failures came from exactly that
gap. A preview that renders correctly is not evidence that the draw path is
valid on the device.

## D32 — Artifacts carry the build number

**Status:** Accepted
**Context:** Every rebuild produced `cockpit.vpk`, so successive builds overwrote
each other and there was no way to tell which one was on the console.
**Decision:** `build.sh` increments `BUILD_NUMBER` in the project root and passes
it as `COCKPIT_BUILD`; CMake names the package
`cockpit-v<version>-b<build>.vpk`. `send.sh` uploads the newest one.
**Consequence:** Builds coexist in `ux0:data` and the filename identifies them.
The counter lives outside `build/` so wiping the build directory does not restart
numbering into a collision. Costs one tracked counter file and a rebuild when it
needs resetting. `APP_VER` stays `##.##`; the build number is an artifact name,
not package metadata.

## D33 — STORAGE shows `ux0:` and `uma0:` only

**Status:** Accepted
**Context:** The panel carried three mounts. On the PCH-1000 this targets there
is no `imc0:` at all, so a third of the region said `--` permanently, and the
SD2Vita card that is actually interesting shares the panel with a row that can
never have data.
**Decision:** `PLATFORM_STORAGE_COUNT` is 2: `ux0:` then `uma0:`.
**Consequence:** Both rows can hold a real reading. Costs the internal memory
card on a PCH-2000 — if COCKPIT ever targets one, it comes back as a third slot
with a compile-time or runtime check, not as a permanent empty row.

## D34 — A failed storage read reports its return code

**Status:** Accepted
**Context:** The first hardware build showed `--` for every mount. A denied
`sceIoDevctl` (this is a safe eboot), a reply this code misreads, and a genuinely
absent card all rendered identically, so the screenshot could not distinguish
them and neither could the code.
**Decision:** `PlatformStorage` carries the platform call's return code, and the
STORAGE panel prints `E<rc>` when the call failed. The plausibility bounds are
temporarily reduced to `rc >= 0 && max_size > 0` so a valid reply cannot be
silently rejected for being in an unexpected unit; `free` is clamped to `total`
so the subtraction cannot underflow.
**Consequence:** The next hardware build is diagnostic: `E-<n>` means the call
itself is refused, a number means it succeeded and the value shows the unit.
Costs one temporary UI state and weaker validation, both to be removed once a
real reading is confirmed. This is the one place the "never show a number you
cannot vouch for" rule is deliberately relaxed, and it is time-boxed to that
confirmation.

**Result (build b10):** `ux0:` returned `0x80010030` and `uma0:` returned
`0x80010013`. Decoding the low byte against `sys/errno.h`: `0x13` is `ENODEV`
(19), which is the correct answer for an absent second volume. `ux0:` is a
different failure — the card exists, so the call is being refused rather than
finding nothing. That points at the safe eboot's permissions, which is what D36
tests.

**Result (build b14-unsafe):** `ux0:` returned a real size. Confirmed on
hardware: **`sceIoDevctl` on `ux0:` is refused under the safe authid and
permitted under `UNSAFE`.** D38 is the response — an ordinary app-manager call
reports the same numbers without the permission cost.

## D35 — The sweep is the liveness cadence, at 5 seconds

**Status:** Accepted
**Context:** A hardware report described the screen as "fixed". Sampling was not
at fault: `metrics_sample_slow` runs at 10 Hz (verified in the shipped binary,
three call sites in the loop). The readings themselves are simply stable — arm,
gpu and bus clocks do not move, nor do temperature, cycle count, state of health,
firmware or model. Only the wall clock, the uptime and the two traces change, and
they change slowly enough to read as a still image.
**Decision:** The interference sweep runs every 5 s instead of 7 s, so the screen
visibly refreshes at roughly the cadence a reader expects.
**Consequence:** The one device DESIGN already designates as "proves the render
loop is running" now does that job at a perceptible rate. Costs nothing; the
sweep is two rects.
**Rejected:** animating the values themselves. The design forbids animating a
value that did not change, and inventing motion would make a static reading look
like a live one — the exact failure the project is built to avoid.

## D36 — UNSAFE is available as a marked, test-only build

**Status:** Accepted (does not supersede D9; the shipped default is still safe)
**Context:** `ux0:` returns `0x80010030` from `sceIoDevctl` while the card is
plainly mounted. The remaining explanations are a privilege refusal under the
safe eboot's authid and a malformed call, and no amount of host testing
distinguishes them.
**Decision:** `CMakeLists.txt` gains `COCKPIT_UNSAFE` (default `OFF`) and
`build.sh` gains `--unsafe`. An unsafe build is named
`cockpit-v<version>-b<build>-unsafe.vpk` so it can never be mistaken for a
shipped artifact.
**Consequence:** One flash answers the question. If storage works under
`UNSAFE`, the permission is the cause and the product decision — ship unsafe, or
drop the STORAGE panel — becomes an explicit one. If it fails identically, the
call or the struct is wrong and the authid is irrelevant.
**Constraint:** `AGENTS.md` rule 3 still holds. This is the "specific API requires
it" case being *tested*, not assumed; nothing ships unsafe until the test says so
and a decision records it.

## D37 — Storage shows its own unit; an absent volume is not an error

**Status:** Accepted
**Context:** Two problems with the diagnostic from D34. `uma0:` reported
`ENODEV`, which is the correct answer for a volume that is not mounted — but it
printed as an error code, making a normal console look broken. And the value was
fixed to tenths of a GB, so a sub-gigabyte volume would render as "0.4/0.5 GB",
losing the precision that matters at that size.
**Decision:** `StorageSample` holds whole megabytes. The panel picks the unit
from the volume's own size — GB with one decimal at 1 GB and above, whole MB
below — and prints it in the value. `ENODEV` is classified as "absent" and
renders `--`; every other negative code still prints, because those are real
refusals.
**Consequence:** A card reads "12.4/29.5 GB", a small volume reads
"384/512 MB", and a missing one reads `--`. Costs a size-dependent format and a
`[C]` assumption that SCE errors carry the errno in the low byte — which is what
`ENODEV` matching exactly on hardware indicates. (The absence marker became
`N/A` in D41.)

## D38 — Storage uses `sceAppMgrGetDevInfo`, so the app stays safe

**Status:** Accepted (supersedes the devctl-only path; D9 stands)
**Context:** D36's unsafe build proved that `sceIoDevctl` on `ux0:` is refused
under the safe authid (`0x80010030`) and works under `UNSAFE`. That left a bad
choice: relax the whole system's permissions for one panel, or ship a dashboard
whose storage region is permanently blank.
**Decision:** `platform_read_storage` calls
`sceAppMgrGetDevInfo(dev, &max_size, &free_size)` first. It is an ordinary
app-manager call, reports capacity and free space in bytes, and needs no
permission relaxation. The devctl remains as a fallback for a device AppMgr does
not cover.
**Consequence:** Storage works under the shipped safe eboot and D9 is intact. The
unsafe path stays available for testing but is no longer required by anything
COCKPIT does.
**Rejected:** shipping `UNSAFE`. It would have worked, but it costs every install
a system-wide permission relaxation and an "Enable Unsafe Homebrew" requirement,
to read one number that a safe API already provides.

**Result (build b25, hardware):** `ux0:` reports `17.7/119.3 GB` under the safe
authid. The path works, D9 stands, and the unsafe build is no longer needed by
anything COCKPIT does.

## D39 — No in-app exit button

**Status:** Accepted
**Context:** START was bound to exit. A dashboard is meant to be left running, so
a stray press dismissed the screen for no reason — reported on hardware as the
app "minimising".
**Decision:** The binding is gone. `app_run` loops until the system closes it;
the PS button is the way out, as it is for any Vita app.
**Consequence:** The app can no longer be dismissed by accident, and the
controller dependency disappears entirely — `psp2/ctrl.h`, `SceCtrl_stub` and
`sceCtrlSetSamplingMode` are all removed, leaving `app.c` with one SDK call (the
process clock). Costs an exit path that depends on the system, which is how the
platform expects apps to behave anyway.

## D40 — No red, and no bar after the wordmark

**Status:** Accepted (supersedes the red reservation in D5)
**Context:** Two pieces of chrome were doing nothing useful. A 1 px red rule sat
on top of the hazard band; with the band already reading as a warning, the rule
added a colour the rest of the screen never used. And a short vertical bar
followed the wordmark purely as punctuation — it encoded nothing, which is
exactly what the design forbids.
**Decision:** Both are removed. `C_RED` is deleted from the palette; the hazard
band is yellow-and-black and is identified by its stripe pattern. The wordmark is
the word.
**Consequence:** The palette has one accent pair and no alert colour, so nothing
can be mistaken for a warning that is not the band — and nothing can be mistaken
for decoration that is a warning. Costs the ability to signal urgency with hue,
which the stripe pattern already does better at this size.
**Rejected:** keeping red as a reserved token for a future alert treatment.
Nothing used it, and an unused palette entry is a rule waiting to be broken by
whoever finds it first.

## D41 — STORAGE never renders an empty row

**Status:** Accepted
**Context:** With a second volume mounted, `uma0:` reported the same
`0x80010030` refusal that `ux0:` first hit: `sceAppMgrGetDevInfo` documents
itself for `ux0:` only, so it does not cover `uma0:`, and the devctl fallback is
refused under the safe authid. That surfaced as a ten-digit number on the HUD.
Separately, an unmounted slot rendered nothing at all, which reads as a broken
panel rather than an empty slot.
**Decision:** A storage row always states its condition. A mounted volume shows
its size; an unmounted one shows `N/A`; a refused read shows `ERR` plus the errno
byte (`ERR 30` is the permission refusal). No row is ever blank, and no raw SCE
code reaches the screen.
**Consequence:** Every state of the panel is legible at a glance, and the code
stays diagnostic without being unreadable. Costs the `--` convention for this
panel: `N/A` is deliberate, because "no volume in the slot" is a different
statement from "this metric could not be read", which is what `--` means
everywhere else.
**Open:** `uma0:` capacity needs a call the safe authid refuses. Under `UNSAFE`
the devctl path reports it. Whether that is worth the permission cost is a
product decision, not a code one.

**Resolved on hardware:** with `uma0:` mounted, the unsafe build reports its
capacity and the safe build reports `ERR 30`. The limitation is permissions, as
suspected — `sceAppMgrGetDevInfo` covers `ux0:` only, and `sceIoDevctl` is
refused. Which build ships is the product decision left open above.

## D42 — UNSAFE is the default eboot

**Status:** Accepted (supersedes D9)
**Context:** D9 chose a safe eboot because nothing in COCKPIT needed private
APIs. That held until storage. `uma0:` capacity is reachable only through
`sceIoDevctl`, which the safe authid refuses with `0x80010030`, and
`sceAppMgrGetDevInfo` covers `ux0:` only. Confirmed on hardware: the same binary
reads both volumes under `UNSAFE` and only `ux0:` under safe.
**Decision:** `UNSAFE` is the default. `build.sh --safe` produces the restricted
variant, marked `-safe` in the artifact name.
**Consequence:** Both storage rows read, which is what the panel exists to show.
Costs: the console must have "Enable Unsafe Homebrew" on, the install warns, and
the app runs with system-wide permissions it does not otherwise need. That cost
is real and accepted, because the alternative is a dashboard that cannot read
half its own storage.
**Rejected:** shipping safe with `uma0:` reading `ERR 30`. A system dashboard
that refuses to report a mounted volume is not doing its job, and the failure
would look like a bug in COCKPIT rather than a deliberate restriction.

## D43 — PLUGINS panel: a list strip, and where its space came from

**Status:** Accepted (amends the layout in D17)
**Context:** The dashboard had no way to answer "what is taiHEN actually
running", which is the first thing to check when a console misbehaves. Enso
loads plugins from `ur0:tai/config.txt` and the legacy `ux0:tai/config.txt`, and
skips any path whose file is missing — so the panel has to read the configs *and*
stat each entry, or it would report plugins that are not loaded.
**Decision:** A new PLUGINS region, 552 x 68, in the left column directly above
FRAME. It uses `CHROME_LIST`: the satellite chamfer, the title top-left and
inline, no rule. Space came from CORE (240 -> 190, the trace absorbing the loss)
and STORAGE (144 -> 114); the right column is untouched.
**Consequence:** The dashboard can name what is running without disturbing POWER
or SYSTEM, which are already tight. Costs the hero trace 36 px — it is still the
panel's escalated element, just shorter — and costs STORAGE some breathing room.
**Rejected:** a full-width strip. It read better for name-plus-description, but
clearing space above it meant squeezing POWER to 161 px, where its four detail
rows collided. A narrower panel at the micro type scale keeps every other region
readable, which matters more than the list's comfort.

## D44 — File access crosses the port; config parsing does not

**Status:** Accepted
**Context:** Reading `config.txt` is a device detail, but deciding which lines are
plugins, resolving descriptions and dropping entries whose file is missing is
policy — and that is the part most likely to be wrong.
**Decision:** `core/platform.h` gains `platform_read_text_file` and
`platform_file_exists`. `core/plugins.c` parses the text it is given and never
touches a file, so the parser runs in the host tests against sample configs.
**Consequence:** The config format is exercised off-device, including the awkward
cases — CRLF endings, indented paths, comments, section headers, a plugin listed
in both configs, and an unknown plugin that must keep an empty description rather
than an invented one. Costs two more port functions.
**Note:** descriptions are `[C]` community knowledge about common plugins, held in
one table. An unknown plugin still appears with no description, which is honest;
a guessed description would not be.

## D45 — The panel area scrolls; the header and status strip are pinned

**Status:** Accepted (supersedes the fixed single-screen assumption in D13/D17)
**Context:** Every new region had to be paid for out of another one. By the time
PLUGINS arrived, CORE's trace had been cut to 29 px and POWER's four detail rows
were colliding, purely to keep everything on one 960x544 screen. A dashboard that
grows will never fit a fixed screen, and squeezing to fit makes the regions that
are already there worse.
**Decision:** The layout table is laid out on a canvas taller than the screen
(`g_canvas_h`, 620 when written; now computed from the plugin count, D47).
`screen_draw` offsets every panel rect by the
scroll position and clips to a 396 px viewport. The header and the FRAME strip are
pinned: the clock and frame health stay visible at every scroll position. The
d-pad and the left stick scroll, at 900 px/s at full deflection, clamped to the
canvas. A thin scrollbar on the right edge shows position, because a scrollable
screen with no affordance looks like a screen that simply ends.
**Consequence:** Regions get their natural size again — CORE back to 240, POWER to
216 (both 210 since D48) — and the next region is a table edit rather than a
negotiation. Costs a
viewport clip (a GPU state change per frame), a scroll position the app owns, and
the controller dependency that D39 removed, now justified by scrolling rather than
by an exit button.
**Rejected:** scrolling the whole screen including the header. It would be simpler,
but the clock and frame time are the two readings that should never scroll away.

## D46 — A build number counts builds, not invocations; only one artifact is kept

**Status:** Accepted (amends the artifact rule in D32)
**Context:** The counter reached 46 for roughly twenty real builds, and the build
directory held 44 near-identical VPKs. Two causes. Rebuilding unchanged sources —
which happens constantly while checking for warnings — incremented the number, so
the counter tracked keystrokes rather than builds. And nothing ever deleted a
superseded artifact, so every number left a file behind.
**Decision:** `build.sh` hashes the source tree and reuses the previous number
when the hash is unchanged; a rebuild with no edits produces the same artifact
name instead of a new one. After every build it deletes all but the newest VPK.
**Consequence:** The number matches the number of distinct builds, and the build
directory holds exactly one artifact — the one you would flash. Costs a `shasum`
over the sources per build (milliseconds) and loses local history: an older build
can only be rebuilt, not recovered from disk.
**Rejected:** keeping the last N artifacts. "Which one do I flash" then becomes a
question, and the answer is always the newest.

## D47 — The plugin list is a self-sizing two-line ledger

**Status:** Accepted
**Context:** The first PLUGINS panel was a fixed-height strip showing name and
description side by side, clipped to three rows with a "+N more" count. That
hid plugins, and the region's height was a guess that had to be renegotiated
every time the screen changed.
**Decision:** One row per plugin, two lines each: the name, a leader rule to a
status dot on the right, then the description underneath. The panel computes its
own height from the row count (`plugins_panel_height()`), and `layout_update()`
writes that height into the layout table and recomputes the canvas height, so the
region grows or shrinks with the list and nothing is clipped.
**Consequence:** Every loaded plugin is visible, the canvas is exactly as tall as
its content, and adding a plugin needs no layout edit at all. Costs a mutable
layout table (`g_layout` and `g_canvas_h` are no longer `const`) and one
`layout_update()` call after the plugin scan.
**Descriptions:** the full AutoPlugin2 Vita catalog
(`AUTOPLUG2/plugins/plugins.lua`) and its English strings
(`AUTOPLUG2/lang/ENGLISH_US.lua`), keyed by plugin file name and shortened to one
line. 155 rows. Rows marked `[C]` are not straight catalog copies: libshacccg and
iTLS are community knowledge, YAMT's text is from its README (the `.suprx` is the
settings GUI), HENkaku's is from its plugin source, and ioPlus/kuio are catalog
dependencies with no standalone entry. Four catalog rows carried another
plugin's text or unusable text (adrenaline_kernel, ds3, lowmemmode, scorehax) and
were rewritten. An unknown plugin still appears with no description rather than a
guessed one.
**The green dot** is `C_GREEN`, the only status hue in the palette. It marks a
row as loaded; every row here is loaded, because the scan drops entries whose
file is missing. It is not an accent and must not be used anywhere else.

## D48 — The vertical rhythm is a 4 px scale, and the grid has one seam

**Status:** Accepted
**Context:** The first screen passed the design review but read as unpolished on
hardware. Measured, the gaps between consecutive text blocks were 2, 3, 4, 5, 5,
6, 7 and 13 px against type 21-35 px tall. A 35 px battery figure sat 2 px above
its state caption; a 21 px value sat 5 px above the next row. Separately, CORE
was 240 tall while POWER was 216, and STORAGE started 24 px below SYSTEM, so the
horizontal seam between the two rows stepped across the screen.
**Decision:** One spacing scale, `SP_1`-`SP_6` (4/8/12/16/24/32), declared in
`theme.h`. Panel content starts at `PANEL_BODY_Y` (the title rule plus `SP_4`);
readout rows use `READOUT_STRIDE` (`LINE_H(PX_VALUE) + SP_4` = 37); a block is
separated from the next by at least `SP_4`, and the space inside a block is at
most `SP_2`. Both columns share both seams: row one is 210 tall, row two 172.
**Consequence:** Every gap is on the scale and derived from the type ladder, so
changing a font size cannot silently close a gap. The two columns read as one
grid. CORE's trace grew from 29 to 40 px because the panel it sits in shrank by
30 px, and the 93 px of dead space under it is gone.
**STORAGE:** matching SYSTEM's 172 px exposed a problem: two inline rows left
55 px of empty panel. The meter moved below its readout and spans the content
width, so each volume is a two-line block and the rows fill the panel. This is a
local change to STORAGE, not a new pattern; the readout and the meter are the
same widgets.
**Also fixed:** the absent meter track was `C_HAIR` at 45 %, which rendered at
(12,13,16) against a (13,14,18) fill -- invisible. It is now 70 % (19,20,25).
The FRAME strip's FPS/MS units were 3.5 px above the value baseline; they now
share it.
**Rejected:** keeping the old gaps and only increasing the panel heights. The
panels were not the problem; the rhythm inside them was.

## D49 — Derived layout state, and draw only what the viewport can show

**Status:** Accepted (supersedes the `g_canvas_h` sentence of D47)
**Context:** A whole-project simplification review found two kinds of drift. The
canvas height was a mutable global written twice from the same formula, and the
scroll limit was recomputed in three files; a reordered or resized panel could
leave either stale. Separately, every panel was drawn at every scroll position,
including panels entirely outside the 396 px viewport: at scroll 0 the plugin
panel starts 10 px below the band and was still emitting its chrome, title and
every row.
**Decision:** `layout_canvas_height()` and `layout_max_scroll()` derive both
values from the layout table; `g_canvas_h` is gone. `layout_update()` resolves
the PLUGINS slot by panel identity rather than by index 4, and only writes that
rect. `screen_draw()` skips a panel whose rect does not intersect the view band,
and the plugin ledger skips rows outside it. On the same pass, the storage errno
byte is decoded in `platform_psp2.c` and carried across the port, so that `[C]`
claim no longer reaches a panel; the four panel insets share `PANEL_INSET`; and
the readout formatters share `THEME_ABSENT` and `theme_readout_int()`.
**Consequence:** A missing `layout_update()` fails closed to the compiled-in
heights, and a reordered table cannot resize the wrong panel. The preview's
draw-call count fell from 4,153 to 2,867 at scroll 0 and from 4,875 to 3,578 at
the bottom. Culling is pixel-exact: the band is already clipped, and every panel
draws inside its own rect.
**Rejected:** caching panel geometry or the trace columns. Culling alone paid for
the frame, and a cache adds state that a scroll offset makes easy to get wrong.

## D50 — Keep the whole plugin list, and make a ceiling visible

**Status:** Accepted (extends D47)
**Context:** `PLUGINS_MAX` was 12, chosen while the panel was still a readout.
A real 3.65 config listed 19 plugins: `add_plugin()` stored the first 12 and
dropped the rest without a trace, so the panel presented a short list as if it
were complete. The name field was 32 bytes while the catalog already contained a
41-character file name, which cut the displayed name and made its description
lookup miss. A path longer than its buffer was truncated, then failed the
existence check, dropping the row with no explanation.
**Decision:** The array holds 64 plugins (`PLUGINS_MAX`), the name field 48
bytes (`PLUGIN_NAME_MAX`), and the path 96 (`PLUGIN_PATH_MAX`). Duplicate
detection runs before the ceiling, so a repeated line such as `henkaku.suprx`
under several sections is not counted as a lost plugin. A path too long for the
buffer is skipped rather than truncated. If the ceiling is reached,
`PluginList.truncated` is set and the panel prints `MORE PLUGINS NOT SHOWN` in
the slot a further row would use; `plugins_panel_height()` accounts for the
line, so it cannot be clipped.
**Consequence:** A 19-plugin config renders all 19 rows, and the canvas grows to
match because D49 derives it from the layout table. A 19-row scratch render
measured 4,740 draw calls against the 5,500 budget. Comments are deactivated
entries and never reach the list. Host tests are 74.
**Rejected:** a dynamically allocated list. `plugins_scan()` runs once at
startup, so a heap list would work, but it adds an allocation-failure path and a
lifetime to a module that currently has neither. 64 is more than a console can
usefully boot; the flag covers the impossible case instead of hiding it.

## D51 — Group the plugin ledger by load target

**Status:** Accepted (extends D47 and D50)
**Context:** The ledger was a flat list, but `config.txt` is organised by load
target: `*KERNEL`, `*main`, `*NPXSxxxxx` system apps, `*ALL`, and individual
title IDs. A flat nineteen-row list hid that structure and made a user compare
the panel against the file line by line to see what ran where. The user asked
for the config's grouping with friendly names.
**Decision:** The parser records a `PluginSection` for every entry and the panel
draws a tracked cyan header once per non-empty group: KERNEL, SYSTEM (`*main`),
SYSTEM APPS (`*NPXS*`), ALL TITLES (`*ALL`), TITLES (any other target). A file
listed in several sections is shown once, under the most privileged one, which
is the enum order: KERNEL, SYSTEM, SYSTEM APPS, ALL TITLES, TITLES. A path
before the first section header is ignored; the format gives it no load target
(`[C]`, no such config has been observed).
**Consequence:** The 19-entry console config renders four groups: KERNEL 9,
SYSTEM 8, SYSTEM APPS 1, ALL TITLES 1. `henkaku.suprx`, listed under a system
app and under `*main`, appears once in SYSTEM. Panel height becomes
`41 + 26 × groups + 52 × rows + 10 × gaps`, so the canvas is 1,581 px for that
config and scrolls. A 19-row scratch render measured 4,423 draw calls against
the 5,500 budget.
**Rejected:** one row per section occurrence. `henkaku.suprx` would appear three
times and the panel would answer "how many lines are in config.txt" instead of
"what is running".

## D52 — A plugin with no catalog entry says N/A

**Status:** Accepted
**Context:** A row outside the 155-entry catalog kept an empty description line.
That reads as a drawing failure or a clipped row, not as "the catalog has
nothing for this file", and the user asked for the absence to be explicit.
**Decision:** `draw_row()` always draws the second line. An empty description
renders `N/A` at micro scale in the same dim tone as a real description. The
data layer still stores the empty string: only the panel supplies the token.
**Consequence:** Every plugin row is exactly two lines, so the 52 px stride is
uniform and the panel height formula needs no special case. The preview's fake
config includes one unknown file (`mystery.suprx`) so the N/A path is visible in
every render. Host tests keep asserting that an unknown entry's description is
empty, which pins the boundary between data and presentation.
**Rejected:** writing `N/A` into `PluginInfo.desc` during the scan. It would
make an unknown plugin indistinguishable from one whose catalog text is
literally "N/A", and it puts UI copy in the sampling layer.

## D53 — Inset the scrolling band, and drop the header hairline

**Status:** Accepted
**Context:** CORE and POWER started at the viewport top (y 74), exactly where
the 2 px panel outline is drawn. The scissor cut the upper half of that outline,
so the two panels looked like they continued under the header. A 1 px `C_HAIR`
rule at y 47 also ran under the clock and read as an unearned divider; the
header's only rule should be the hazard strip, and only on a fault.
**Decision:** The scroll band starts at y 64, eight pixels below the hazard
strip, and ends at 472. Content is inset by `VIEW_TOP_GAP` (12), so the first
panel begins at screen y 76 with a whole outline. `layout_max_scroll()` adds the
inset, which keeps the scroll range identical to the old 396 px band. The
scrollbar track starts at the content top (76), not the band top. The hairline
is deleted; `header_draw()` paints the wordmark, the clock, and the hazard strip
only.
**Consequence:** At scroll 0 the two-row canvas sits at 76..470, with 12 px of
air before the FRAME strip at 482. While scrolling, content clips at 64, eight
pixels below the hazard, and the inset scrolls away. `C_HAIR` remains the
scrollbar track and the empty meter cell, nothing else.
**Rejected:** raising `VIEW_Y` while keeping the band 396 px tall. The outline
would be clipped at the new edge, and shrinking `VIEW_H` to compensate would cut
the bottom of STORAGE and SYSTEM at scroll 0. Moving FRAME down would buy the
room from the bottom margin the layout already spends.

## D54 — Tabbed screens with one explicit UI state

**Status:** Accepted (supersedes D16's single-screen assumption)
**Context:** COCKPIT was one screen, and D16's metrics global was written by one
sampler and read by the panels. A second screen adds a second consumer set but
does not add a second writer.
**Decision:** `UiState` in `ui/ui_state.h` owns the active tab, both scroll
offsets, the selected row, and the modal. `screen_draw(t, ui)` receives a
read-only pointer. HOME and PLUGINS share the header and FRAME strip; only the
region between them changes. The metrics global keeps its single writer.
**Consequence:** Tab state is not hidden in a panel or a second global. The old
`layout_update()` and the PLUGINS slot in the home layout table were removed and
HOME had a fixed canvas and no scrollbar. **Amended by D67:** the read-only
summary block returns a slot below the columns, so HOME scrolls again; the state
still lives in one `UiState`.
**Rejected:** a global `scroll` per panel and an `if (tab)` branch inside every
panel. Both make a third tab a rewrite instead of an addition.

## D55 — The active plugin config follows taiHEN, not the old app order

**Status:** Accepted (supersedes the ur0-first scan; the invalid-file half is
superseded by D65)
**Context:** taiHEN opens `ux0:tai/config.txt` first and falls back to
`ur0:tai/config.txt`. COCKPIT read ur0 first, so on a console with both files
the dashboard showed the shadowed copy.
**Decision:** Resolve ux0 first; use ur0 only when ux0 cannot be read. A
readable empty ux0 file is active. A too-large or invalid file is read-only.
The active path is frozen at launch, and PLUGINS names the active path and any
ignored one.
**Consequence:** The dashboard now describes the file taiHEN actually loads. A
stale ux0 no longer hides a working ur0 silently.
**Updated by D65:** a path before the first section is a warning, not a
read-only state, and a BOM does not hide the first section.
**Rejected:** merging both files, and editing ur0 while ux0 shadows it.

## D56 — A byte-preserving line model, keyed by full path

**Status:** Accepted
**Context:** The old parser dropped comments and missing files, merged entries
by basename, and kept no offsets, so it could not support a toggle.
**Decision:** The editor keeps every line's raw span, line ending, section,
active flag, and reachability. Rows are keyed by a normalized, case-insensitive
full path. Section names are case-sensitive and `*!Section` halts parsing. A
row shows its basename unless two rows share it, in which case the full path is
shown.
**Consequence:** A toggle changes only the `#` markers on the lines it names.
Two files with the same basename in ur0 and ux0 stay distinct.
**Rejected:** re-emitting a normalized config, and basename identity.

## D57 — The PLUGINS screen is an editable ledger with transactional writes

**Status:** Superseded by D61 and D64 (the `.bak` rotation, the `.bak`
precondition, and rename-over are gone; the row is still an editable ledger)
**Context:** The ledger was read-only; a config write can leave a partial file.
**Decision:** PLUGINS is a full-screen ledger. Cross toggles a row. A write
removes a stale `.new`, writes a sibling, verifies it by read-back, checks the
active `.bak`, and renames the sibling over the original. At session start the
editor rotates `.bak` to `.1.bak` and `.1.bak` to `.2.bak`, then copies the
current file to `.bak`. A failed active-config backup makes the manager
read-only; a failure on another file only warns. `boot_config.txt` is read,
never written.
**Consequence:** A write never truncates the live file. The previous three
sessions are recoverable from the tai directory, and `.new`/`.bak` files are
excluded from the orphan list.
**Rejected:** in-place truncate-and-write; a single overwritten `.bak`; editing
`boot_config.txt`.

## D58 — Lock the recovery surface, warn the load-bearing plugins

**Status:** Accepted
**Context:** Research found nothing in `config.txt` is boot-critical, but
`henkaku.suprx` is the HENkaku Settings and reload surface, and a storage
driver in `config.txt` can be load-bearing for `ux0:`.
**Decision:** Lock `henkaku.suprx` (it can be restored but not disabled). Lock
`taihen.skprx`, `henkaku.skprx`, and `yamt.skprx` by name if they appear in
`config.txt`. Warn on `0syscall6`, `nonpdrm`, `repatch`, `nopsmdrm`,
`libshacccg`, `yamt.suprx`, `yamt_helper`, `reVitaMotion`, `storagemgr`,
`usbmc`, and `usbmcm`. KERNEL enables always confirm; the warned files confirm
in both directions.
**Consequence:** The manager cannot remove its own recovery UI or silently
disable a mount driver. A KERNEL disable that is not warned stays low-friction.
**Rejected:** locking every warned plugin, and warning every KERNEL disable;
neither has evidence that the entry can stop a boot.

## D59 — HOME counts the launch activated set; pending is session-scoped

**Status:** Accepted
**Context:** The user wants a count of activated plugins, not a count of edits.
A pending enable is not loaded until the reboot.
**Decision:** HOME counts the launch config's activated set: enabled, reached,
and present on disk. A pending enable does not count until the reboot; a
pending disable keeps counting. Pending is a per-line diff against the launch
snapshot. The baseline is session-scoped and is not persisted.
**Consequence:** A restart before the reboot re-reads the edited file as the
new baseline, so the pending marker is a session convenience. This is
documented; persisting it would need a boot identity the platform does not
expose reliably.
**Rejected:** reflecting pending edits in the total; a persisted baseline with
a guessed boot marker.

## D60 — Tabs and the HOME summary live in the header chrome

**Status:** Superseded by D63 (the summary no longer shares the hazard line)
**Context:** The old ledger slot starts at screen y 482, below the 472 band, so
a summary there is invisible at launch.
**Decision:** Tabs sit between the wordmark and clock at y 20, with a 2 px
yellow underline on the active tab. The HOME summary shares the hazard line at
y 48, right-aligned in micro scale; a fault replaces it with the hazard band.
When the run would cross the tab block, drop the word ACTIVE and then group
counts from TITLES upward; the total and pending token always survive.
**Consequence:** HOME keeps both panel rows at scroll 0 and has no scrollbar;
the summary costs no viewport height.
**Rejected:** a new pinned band (reduces the viewport), and a summary in the
old ledger slot.

## D61 — Two config snapshots, and `boot_config.txt` is out of scope

**Status:** Accepted (supersedes D57's backup model)
**Context:** The three-deep rotation rewrote an existing `.bak` by
rename-over-existing, which the adapter marks `[C]`; a failed copy therefore
made the manager read-only before the user touched anything. The user asked for
one first-session original and one per-session snapshot. Separately, the app was
reading `boot_config.txt` and creating `boot_config.txt.bak` copies, which is
outside a config manager's remit.
**Decision:** The active config gets exactly two snapshots. `config.txt.original`
is written only when it does not exist and is never overwritten. `config.txt.session`
is written from the session-start bytes on every launch and is overwritten each
session. No `.bak`, `.1.bak`, or `.2.bak` is created. `boot_config.txt` is never
opened, read, copied, renamed, or written; the three well-known boot module names
are locked by name only. A failed snapshot sets `BACKUP WARNING` but never makes
the config read-only.
**Consequence:** A bad edit has two restore points, and a backup failure cannot
lock the editor. The old `.bak` files from b28 are ignored and may be deleted by
the user; COCKPIT does not touch them.
**Rejected:** keeping the rotation; deleting `boot_config.txt.bak` from the app;
backing up both mounts when only one is active.

## D62 — The toggle is a two-state `#` edit

**Status:** Accepted (supersedes D57's three-state cycle and `.bak` precondition)
**Context:** The old action had a third `RESTORE` state and refused to write
unless `.bak` matched the launch bytes. Both made the switch unpredictable.
**Decision:** A row is either fully on or fully off. `ENABLE` removes the
comment run from every reached line that names the path; `DISABLE` inserts one
`#` before the path on every reached line. A line already in the target state is
unchanged. Every other byte, including indentation, trailing spaces, CRLF, and a
missing final newline, is preserved. The session snapshot is the restore path.
**Consequence:** The control always reads `ENABLE` or `DISABLE` for an editable
row. A partial launch row normalizes to all-on or all-off; pending reflects the
difference from the launch pattern until the file matches it again.
**Rejected:** a third restore action; re-emitting the whole file; rewriting a
line's whitespace.

## D63 — The HOME summary is permanent chrome

**Status:** Superseded by D67 (the summary is a HOME block, not header chrome)
**Context:** The summary was drawn at `SUMMARY_Y 48`, exactly where the hazard
band was drawn, and in micro `C_DIM`. A fault covered it, and on hardware it was
easy to miss.
**Decision:** HOME always draws `N PLUGINS ONLINE` (singular for one) at label
scale on its own header line, with a green dot and a micro detail run for the
non-empty groups and pending count. The hazard band moves to y 56, the bottom
8 px of the header, and never covers the summary. A config problem is named:
`NO CONFIG.TXT`, `CONFIG UNREADABLE`, `CONFIG TOO LARGE`, or `CONFIG WARNING`.
**Consequence:** The count survives an alert. The total and pending token
survive the fit loop; group counts drop first.
**Rejected:** a new pinned band; putting the summary back in the panel area;
letting a fault replace the count.

## D64 — Read first, then stat; replace by rename-away and verify

**Status:** Accepted (supersedes the write half of D57)
**Context:** The read required `sceIoGetstat` before `sceIoOpen`, so a stat-only
failure looked like a missing config. It accepted a short read as complete, and
the write depended on `sceIoRename` replacing an existing file.
**Decision:** `platform_read_config` opens first and reads to EOF or the ceiling.
The stat is used only to classify an open failure and to reject a directory or a
short read. `platform_write_config` writes and verifies `<path>.new`, moves the
original to `<path>.old`, moves the verified sibling into place, reads the live
file back, and restores `<path>.old` if any step fails. The normal path never
renames over an existing file.
**Consequence:** A short read cannot become a truncated rewrite. A write reports
success only after the live bytes were observed. If the final replace is refused,
the original is restored and the row is unchanged. `platform_write_error()`
reports `OPEN`, `DATA`, `CLOSE`, `VERIFY`, `MOVE`, `RENAME`, `READBACK`, or
`RESTORE`, so the error line names the failing stage. The recovery path still
uses a rename over the live file when verification fails; if that is refused,
both files are left for manual recovery.
**Rejected:** truncate-and-write; trusting the rename's return value; a
read-back only of the sibling.

## D65 — A BOM and a stray path are data, not corruption

**Status:** Accepted (supersedes the invalid-file half of D55)
**Context:** A UTF-8 BOM before the first `*KERNEL` made the parser miss the
section; the next path was "before a section", which zeroed the HOME count and
made the editor read-only. A single stray path therefore hid a valid config.
**Decision:** Both parsers skip a BOM at offset 0 and preserve it in the file.
A path before the first section is a warning: the row is `NOT REACHED`, the
valid entries still count, and the config stays editable. Only an unreadable,
too-large, too-long, or too-many-rows file is read-only, with the reason named.
**Consequence:** A Windows-edited config or one stray line no longer disables the
manager. The owner sees `CONFIG WARNING` and can repair the file.
**Rejected:** treating the whole file as invalid; stripping the BOM on write.

## D66 — The row owns its badge and control

**Status:** Accepted
**Context:** The page had one status string and no visible action. Selection
skipped non-actionable rows, so a missing or locked row could not be selected to
read why it would not act.
**Decision:** Every row draws a status badge (`ACTIVATED`, `DISABLED`, `MISSING`,
`PARTIAL`, `NOT REACHED`, `BOOT`), an optional `PENDING` badge, and an action
control (`ENABLE`, `DISABLE`, `LOCKED`, `MISSING`, `BOOT`, or `--`). The control
is greyed when `editor_row_actionable()` is false; Cross on a greyed row opens a
short reason instead of acting. Every row is selectable. A missing active row
may be disabled; a missing inactive row may not be enabled.
**Consequence:** The control and the policy come from the same module, so the
button cannot promise an action the editor will refuse. Scrolling past the last
row reaches the installed-but-unlisted section, and Cross is ignored when the
selected row is off-screen.
**Rejected:** a hidden Cross-only action; skipping locked rows in the cursor;
a switch drawn from panel-local policy.

## D67 — The plugin summary is a HOME block, not header chrome

**Status:** Accepted (supersedes D63)
**Context:** The header line added in b28 was easy to miss and read as chrome
rather than as one of the dashboard's readings. The owner asked for the
activated-plugin summary to return to the slot the old ledger occupied, below
STORAGE and SYSTEM, as a block rather than a status line.
**Decision:** HOME draws a full-width PLUGINS satellite block at canvas y 406,
924 × 140, below both columns. It carries the activated count at primary scale,
`PLUGINS ONLINE` at label scale, and one micro line of non-empty group counts.
`N PENDING` and `CONFIG WARNING` share the right end of that line. The block
reports the launch set — what taiHEN actually loaded — and is read-only. The
HOME header summary is removed; the header carries only the wordmark, tabs,
clock and hazard band, and the band returns to y 48.
**Consequence:** The count is no longer visible at scroll 0; the block sits just
below the fold and HOME scrolls 150 px to reach it, with the scrollbar as the
affordance. The block never grows with the plugin count, so it needs no height
cap. The PLUGINS tab keeps its own header line naming the active config and the
editor state.
**Rejected:** keeping the count in the header; listing every activated plugin in
the block (that is the ledger's job); two half-width blocks under each column.
**Amended by D68:** the block no longer shows `N PENDING`; pending stays on the
PLUGINS tab that owns the edit.

## D68 — Pending is a PLUGINS-tab state, not a HOME reading

**Status:** Accepted (amends D67)
**Context:** The HOME summary block showed `N PENDING` beside the group counts.
The block exists to answer "what is running now"; a pending edit is bookkeeping
for a change that has not happened, and mixing it into a running count made the
block read as two states at once.
**Decision:** HOME never renders a pending token. The block reports only the
launch set, its group counts, and `CONFIG WARNING`. Pending remains visible on
the PLUGINS tab, where the edit is made: the header line, the per-row `PENDING`
badge, and the row's post-toggle state.
**Consequence:** A user who toggles on PLUGINS and returns to HOME sees no
reminder there; the PLUGINS tab is the single owner of pending. The right side
of the HOME group line is empty unless a config warning is present.
**Rejected:** keeping a pending badge on HOME; hiding pending entirely.
