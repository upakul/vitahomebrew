# COCKPIT

A system dashboard for the PS Vita. Cyberpunk 2077 HUD as the visual reference.

It reads the machine it is running on — clocks, memory, storage, power, identity,
frame health — and shows the taiHEN plugins the boot activated. A second tab
turns the plugin ledger into a manager: enable or disable a plugin, with the
change written to the active config and applied on the next boot.

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
│ PLUGINS 924×140   (below both columns, on the canvas)        │
│ 7  PLUGINS ONLINE                                            │
│    KERNEL 3  SYSTEM 2  SYSTEM APPS 1                         │
├──────────────────────────────────────────────────────────────┤
│ FRAME  58.4 FPS  17.1 MS   ─┼─────┼───┼──                   │
└──────────────────────────────────────────────────────────────┘
```

- **Target:** PS Vita, firmware 3.65, permanent Enso CFW. PSTV out of scope.
- **Toolchain:** VitaSDK 2026.08, libvita2d.
- **Auth:** UNSAFE eboot by default (`0x2F00000000000001`), because `uma0:`
  capacity needs `sceIoDevctl` (D42). `./build.sh --safe` builds the
  restricted `0x2F00000000000002` variant.
- **Status:** builds and packages (111.1 KB VPK, b38), runs on the 3.65 console at
  b13; 316 host checks and 15 previews pass. The fixed tabbed build is pending
  the on-console check.

The HOME `PLUGINS` block sits below STORAGE and SYSTEM, spanning both columns. It
shows the activated count at primary scale, `PLUGINS ONLINE`, the non-empty group
counts, and any pending or config warning. It reports the launch set and is
read-only; scroll HOME down to it. Every edit happens on the PLUGINS tab.

The plugin screen resolves the active config the way taiHEN does — `ux0:` first,
`ur0:` fallback — and names any ignored file. It lists every path in that config,
active or commented, grouped by load target, and one row per unique path. Each row
has a status badge (`ACTIVATED`, `DISABLED`, `MISSING`, `PARTIAL`, `NOT REACHED`,
`BOOT`) and an action control. The control is greyed when the action is
impossible: a missing inactive file cannot be enabled, a boot module cannot be
toggled, and a missing active file can only be disabled. `henkaku.suprx` and the
three boot modules are locked; the DRM, storage, and other load-bearing plugins
warn before a change.

The first session writes `config.txt.original` once and never overwrites it;
every session writes `config.txt.session` from the launch bytes. There are no
`.bak`/`.1.bak`/`.2.bak` files. `boot_config.txt` is never opened, read, copied,
or written.

A failed write leaves the original in place and reports `WRITE FAILED` plus the
stage that was refused: `OPEN`, `DATA`, `CLOSE`, `VERIFY`, `MOVE`, `RENAME`,
`READBACK`, or `RESTORE`.

## Read in this order

| Doc | Contents |
|---|---|
| [docs/PROJECT.md](docs/PROJECT.md) | Scope, status, build and deploy, roadmap |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Layers, the dependency rule, module contracts, how to extend |
| [docs/DESIGN.md](docs/DESIGN.md) | Visual language, palette, typography, layout |
| [docs/DECISIONS.md](docs/DECISIONS.md) | Numbered decision log with context and consequences |

## Build

```sh
./build.sh            # UNSAFE eboot (the default; needs Unsafe Homebrew on)
./build.sh --safe     # restricted eboot, marked -safe in the filename
```

Produces `build/cockpit-v<version>-b<build>.vpk`, e.g. `cockpit-v01.00-b38.vpk`.
The build number counts distinct builds and is kept in `BUILD_NUMBER`: unchanged
sources reuse the number, and only the newest VPK is kept (D46). Upload it with:

```sh
./send.sh <vita-ip>
```

VitaShell must be running its FTP server on port 1337. The URL form is
`ftp://<ip>:1337/ux0:/data/` — colon followed by slash.

## Controls

| Input | Action |
|---|---|
| L / R | switch HOME / PLUGINS |
| D-pad up/down, left stick | scroll HOME, or move the PLUGINS selection; at the last row it scrolls to the unlisted files |
| Cross | toggle the selected plugin, or confirm a modal; on a greyed control it shows why it cannot act |
| Circle | cancel a modal |
| PS button | leave the app (system) |

## Test without hardware

```sh
./tools/test/run.sh
```

Links the real `core/` against a fake port and asserts the sampling policy, the
plugin config model, the exact `#` edit, BOM and invalid-path handling, the two
snapshots, the transactional write and its failure/restore path, and the input
reducer. Exits non-zero on failure. 316 checks.

## Preview without hardware

```sh
./tools/preview/run.sh
```

Renders the shipped composition (`app/screen.c`) through a software vita2d shim
and the same fake platform, writing 15 scenes under `build/preview/`:
`preview-healthy.png`, `preview-alert.png`, `preview-home-plugins.png`,
`preview-home-plugins-empty.png`, `preview-plugins.png`,
`preview-plugins-bottom.png`, `preview-warning.png`, `preview-missing.png`,
`preview-backup-warning.png`, `preview-groups.png`, `preview-duplicate.png`,
`preview-confirm.png`, `preview-write-failure.png`, `preview-readonly.png`, and
`preview-home-pending.png`. Each scene is produced by driving the fake port
through the real sampler and policy, so a PNG proves the policy ran. Every
number in it is synthetic, so it proves nothing about the device — only about
the screen.

## Source map

```
src/
  main.c              composition root: vita2d init/teardown
  app/                lifecycle, frame loop, and screen_draw
  core/               sampling policy, history, plugin config and catalog, the port
  gfx/                vita2d adapter: primitives, font, theme, layout
  panels/             one file per panel, the plugin ledger, the HOME summary, and the layout registry
  platform/           the SDK adapter, the only file that samples the device
  ui/                 screen chrome: header, tabs and the PLUGINS header line, modal
tools/
  fake/               shared mutable port double
  test/               host tests for the sampling policy
  preview/            host renderer, not linked into the app
```

The dependency rule and what may import what are in
[ARCHITECTURE.md](docs/ARCHITECTURE.md). Read that before adding a module.
