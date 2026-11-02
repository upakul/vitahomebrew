# Architecture

How COCKPIT is put together, what may import what, and how to extend it without
breaking the shape.

---

## 1. The dependency rule

**Source dependencies point inward, toward policy. Details depend on policy;
policy never depends on details.**

In practice, for this codebase:

- Sampling *policy* — what to read, how often, how to convert, when to raise an
  alert — must be testable without a Vita.
- The SDK and the graphics library are *details*. They live at the edge.
- A panel may read a metric and draw. A metric may not know that panels exist.

The rule is enforced by review and by the module table in §3, not by the
compiler. The one mechanical aid is that only three files may include `psp2/*`
headers (§5).

## 2. Layers

```
        ┌──────────────────────────────────────────────┐
        │  main.c          composition root            │  outermost
        ├──────────────────────────────────────────────┤
        │  app/            lifecycle, loop, layout     │
        ├──────────────────────────────────────────────┤
        │  panels/  ui/    presentation adapters       │
        ├──────────────────────────────────────────────┤
        │  gfx/            vita2d adapter              │
        │  platform/       PSP2 SDK adapter            │
        ├──────────────────────────────────────────────┤
        │  core/metrics    sampling policy, alerts     │  innermost
        │  core/history    ring container              │
        │  core/platform.h the port                    │
        └──────────────────────────────────────────────┘
                          dependencies point down
```

`core/` is the inner circle. It contains no SDK call and no draw call, so it can
be compiled and exercised on a host machine against a fake platform.

`platform/` and `gfx/` are adapters. They exist only to satisfy contracts
declared inward.

`panels/` and `ui/` compose the two. `app/` owns time and ordering. `main.c`
constructs and starts.

## 3. Modules

| Path | Responsibility | May include |
|---|---|---|
| `core/history` | fixed-capacity ring of samples | libc |
| `core/platform.h` | the port: raw device readings, no SDK types | stdint |
| `core/metrics` | convert raw readings, maintain history, decide alerts | `core/*` |
| `core/plugins` | parse tai config, resolve loaded plugins | `core/*` |
| `platform/platform_psp2` | implement the port against the SDK | `psp2/*`, `core/platform.h` |
| `gfx/layout` | screen geometry constants shared by `panels/` and `ui/` | nothing |
| `gfx/draw` | rect, batched rects, line, triangle, additive blend, clip | `vita2d.h`, `psp2/gxm.h` |
| `gfx/font` | 5×7 glyph table, text layout, glyph-rect enumeration | `gfx/draw` |
| `gfx/theme` | palette, panel chrome, meters, traces, formatters | `gfx/*`, `core/history` |
| `panels/*` | one panel each; draw one region | `core/metrics`, `gfx/theme`, `gfx/font`, `gfx/layout` |
| `ui/header` | wordmark, clock, hazard band | `core/metrics`, `gfx/theme`, `gfx/font`, `gfx/layout` |
| `app/screen` | one frame: header, scrolled panels, strip, sweep | `gfx/*`, `panels/*`, `ui/*` |
| `app/app` | lifecycle, fixed-rate scheduler, frame loop | everything below |
| `main.c` | `vita2d_init()`, `app_run()`, `vita2d_fini()` | `app/app`, `vita2d.h` |

`app/screen.c` exists so the host preview renders the shipped composition
instead of a copy of it (D26). It makes no SDK call and reads no clock, which is
what lets `tools/preview` link it unchanged.

Outside the app: `tools/fake/` is the shared mutable port double, `tools/test/`
links it against the real `core/` to assert the sampling policy, and
`tools/preview/` renders the real screen through a software vita2d shim. None of
them is linked into `cockpit.self`.

One rule the type system cannot express: **every vertex buffer handed to
`vita2d_draw_array` must come from `vita2d_pool_memalign`.** GXM reads the vertex
stream when the scene is submitted, so the buffer has to be GPU-mapped and alive
until the frame ends. A stack or static array faults the GPU on the first frame;
that is exactly what the first hardware build did (D29).

No cycles. `core` never includes `gfx`, `panels`, `ui`, or `app`.

## 4. The port

`core/platform.h` declares what the machine can report, in plain C types. No
`Sce*` type crosses it.

```c
typedef struct {
    uint64_t free_bytes;
    uint64_t total_bytes;
    int ok;
} PlatformStorage;

void platform_read_storage(int index, PlatformStorage *out);
void platform_read_clocks(PlatformClocks *out);
void platform_read_memory(PlatformMemory *out);
void platform_read_power(PlatformPower *out);
void platform_read_identity(PlatformIdentity *out);
void platform_read_clock(PlatformClock *out);
double platform_uptime_seconds(void);

int platform_read_text_file(const char *path, char *buf, int cap);
int platform_file_exists(const char *path);
```

Why a port rather than calling the SDK from `core/metrics`:

1. **No console is available.** The sampling policy — unit conversion, alert
   thresholds, history cadence — is the part most likely to contain a bug, and
   it is the part that can be tested off-device. A port is what makes that
   possible.
2. **Three platform claims are `[C]`.** `SceIoDevInfo`, the device model
   constants, and the storage unit are all absent from or unconfirmed by the
   installed headers. Keeping them in one adapter file means a correction is a
   one-file change with no reach into policy.

This is a *partial* boundary, deliberately: plain functions, not an interface
struct, because there is exactly one implementation and C has no interfaces. A
test double is produced by linking `platform_fake.c` instead of
`platform_psp2.c`, not by swapping a vtable at runtime.

That is not hypothetical: `tools/preview/` does exactly that. It links the real
`core`, `gfx`, `panels`, and `ui` against `tools/preview/fake_platform.c` and a
software stand-in for the four vita2d calls `gfx/draw.c` makes, then writes PNGs.
It is how the layout was checked before any hardware existed. It is a developer
tool, never linked into `cockpit.self`.

## 5. Where the SDK may appear

Exactly three files include `psp2/*` headers:

| File | Why |
|---|---|
| `platform/platform_psp2.c` | all device sampling |
| `gfx/draw.c` | one enum, `SCE_GXM_PRIMITIVE_TRIANGLES` |
| `app/app.c` | the process clock |

Any other file that reaches for the SDK is a layering bug. If a panel needs a new
value, add it to the port and to `Metrics` — do not call the SDK from the panel.

## 6. Data flow

```
  platform_psp2          core/metrics              panels
  ─────────────          ────────────              ──────
  scePower*    ─┐
  sceKernel*   ─┼─►  platform_read_*  ─►  convert  ─►  g_metrics  ─►  draw
  sceIoDevctl  ─┤        (10 Hz)          units          │
  sceRtc*      ─┘                                       │
                                                  core/history
  process time ──►  metrics_sample_frame()  ────────────┘
                       (every frame)
```

Panels never call the SDK and never mutate metrics. They read `g_metrics` and
draw. Sampling is the only writer.

### Cadence

| Rate | What | Why |
|---|---|---|
| every frame | frame time, FPS (EMA, α = 0.05) | needs frame resolution |
| 10 Hz | clocks, memory, storage, power, identity, clock | syscalls are not free; these change slowly |

10 Hz × 200 samples = a 20-second trace.

## 7. Platform claims and their status

`[V]` verified against a primary source. `[C]` community-reported, unconfirmed.

| Claim | Status | Where |
|---|---|---|
| Clock getters return MHz | `[V]` `psp2/power.h` | `platform_psp2.c` |
| Free memory returns bytes across three pools | `[V]` `psp2/kernel/sysmem.h` | `platform_psp2.c` |
| Battery percent/temp/voltage/cycles/health are userland | `[V]` `psp2/power.h` | `platform_psp2.c` |
| Firmware string via `sceKernelGetSystemSwVersion` | `[V]` `psp2common/kernel/modulemgr.h` | `platform_psp2.c` |
| `SceIoDevInfo` layout, 24 bytes | `[V]` `psp2common/kernel/iofilemgr.h` | `platform_psp2.c` |
| `sceIoDevctl("ux0:", 0x3001, …)` is the device-info command | `[V]` `psp2/io/devctl.h` demonstrates it verbatim | `platform_psp2.c` |
| Its results are byte counts | `[C]` the unit is documented nowhere | `platform_psp2.c` |
| Model codes `SCE_KERNEL_MODEL_VITA` / `SCE_KERNEL_MODEL_VITATV` | `[V]` `psp2common/kernel/sysmem.h` | `platform_psp2.c` |
| `sceKernelGetModel()` cannot separate PCH-1000 from PCH-2000 | `[V]` one code for both | `platform_psp2.c` |
| No userland CPU-load API exists | `[V]` absent from `psp2/` | — |

Two claims in this table were wrong when first written, both in the same
direction — marking as community knowledge something the SDK declares:

- `SceIoDevInfo` was recorded as a 16-byte community struct. It is declared at
  24 bytes. The hand-written version would have read garbage.
- The model codes were recorded as absent. They are `SCE_KERNEL_MODEL_VITA` and
  `SCE_KERNEL_MODEL_VITATV` in `psp2common/kernel/sysmem.h`, reached through the
  `psp2/kernel/sysmem.h` include the adapter already had.

Only the devctl byte unit remains genuinely `[C]`.

If a `[C]` claim is corrected on hardware, only `platform_psp2.c` changes.

## 8. Panel contract

```c
typedef struct Panel {
    const char *title;
    uint32_t accent;
    void (*draw)(const struct Panel *self, Rect r);
} Panel;

typedef struct {
    const Panel *panel;
    Rect rect;
    PanelChrome chrome;
} PanelSlot;

extern PanelSlot g_layout[];
extern const int g_layout_count;

/* Height of the scrolling canvas, derived from the lowest panel edge. */
int layout_canvas_height(void);

/* Largest valid scroll offset: the canvas height minus one viewport. */
int layout_max_scroll(void);

/* Resize the self-sizing PLUGINS region. */
void layout_update(void);
```

`app_run` walks `g_layout`, draws the chrome through `theme_panel()`, then calls
the panel's `draw` with its rect. Panels own no state; if one ever needs state it
goes in the struct, which is why `self` is passed.

There is no elapsed-time argument. Nothing inside a panel animates: the traces
are driven by the history they read, and the only time-based motion is the
screen-wide sweep, which `app_run` owns. An unused parameter would be dead code
(rule 9).

Geometry lives in the layout table, not in the panel. That is what allows an
asymmetric screen (D17): a panel that computed its own rect from a grid index
could not be 552 wide while its neighbour is 360.

The table is mutable only for the plugin list: `layout_update()` finds the
PLUGINS slot by panel identity and writes `plugins_panel_height()` into it, so
the region grows with the number of loaded plugins (D47). The list holds up to
`PLUGINS_MAX` entries and sets `PluginList.truncated` when a config exceeds
that, which the panel states rather than hiding (D50). Every other rect is
fixed at compile time. The canvas height and the scroll limit are derived from
the table (`layout_canvas_height()`, `layout_max_scroll()`), so neither can
drift from it (D49).

`screen_draw()` skips any panel whose translated rect falls entirely outside the
view band. The scissor would discard every pixel of it anyway, and the plugin
list is taller than the viewport, so at most scroll positions one group of
panels is off-screen. The plugin ledger applies the same test per row.

Panels do not know whether they are the hero or a satellite. The frame drawing
that distinguishes them is `theme_panel()`'s job, selected by `PanelSlot.chrome`
— so a panel can be promoted by editing the layout table alone.

### Adding a panel

1. Create `src/panels/<name>.c`.
2. Define `static void draw_<name>(const Panel *self, Rect r)`.
3. Define `const Panel panel_<name> = { "TITLE", C_ACCENT, draw_<name> };`.
4. Add `extern` for it, plus one `PanelSlot` entry with its rect, to `g_layout[]`
   in `panels/registry.c`. A panel whose height depends on its content also needs
   a case in `layout_update()`; the plugin list is the only one today (D47).
5. Add the source file to `CMakeLists.txt` and to `tools/preview/run.sh`. Both
   list sources explicitly; the preview list is the one that gets forgotten, and
   forgetting it means the preview silently stops rendering the panel.
6. If it needs a value that `Metrics` does not carry, add it to the port and to
   `Metrics` — see below.

Adding a panel means the layout must absorb it. Because the screen is
asymmetric, that is a deliberate edit to the rect table, not an automatic
reflow — decide which region gives up space.

Nothing else changes. The loop and the header are untouched; the two build
lists from step 5 are the only mechanical edit.

### Adding a metric

1. Add the field to `Metrics` in `core/metrics.h`, **and a matching
   `<field>_valid` flag**. Every metric carries one: it is how a panel knows to
   render `--` instead of the zero it happens to hold.
2. Add a `platform_read_*` function to `core/platform.h`, with an `ok` field.
3. Implement it in `platform/platform_psp2.c`, with a `[V]`/`[C]` comment.
4. Call it from `metrics_sample_slow()` and set the valid flag from `ok`.
5. Record any new platform claim in §7 and in `docs/DECISIONS.md`.
6. Add a case to `tools/test/test_metrics.c` — at minimum one that fails the
   read and asserts the panel would render absence.

## 9. Deliberate deviations

Recorded so a future reader does not "fix" them by accident.

- **No dependency-injection container or runtime polymorphism** beyond one
  function pointer in `Panel`. The system has one platform and one theme; a
  container would be ceremony.
- **The port is functions, not an interface struct.** One implementation, C, no
  interfaces. Test doubles come from the link line.
- **`gfx/draw.c` includes `psp2/gxm.h`.** `gfx` is the rendering adapter, so a
  GXM enum is inside its remit; pulling it up into `core` would be worse.
- **`g_metrics` is a global.** One screen, one snapshot, one writer. Passing a
  500-byte struct through every panel would add noise without adding safety.
- **Layout is compile-time.** No responsive system. The Vita has exactly one
  screen size (D13).
