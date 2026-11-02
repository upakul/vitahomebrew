# COCKPIT

A system dashboard for the PS Vita. Cyberpunk 2077 HUD as the visual reference.

It reads the machine it is running on — clocks, memory, storage, power, identity,
frame health — and shows the taiHEN plugins the boot activated. A second tab
turns the plugin ledger into a manager: enable or disable a plugin, with the
change written to the active config and applied on the next boot.

```
┌──────────────────────────────────────────────────────────────┐
│ COCKPIT      HOME  PLUGINS  L/R                  14:22:07    │
│                    7 ACTIVE  KERNEL 3  SYSTEM 2  APPS 1      │
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
│ FRAME  58.4 FPS  17.1 MS   ─┼─────┼───┼──                   │
└──────────────────────────────────────────────────────────────┘
```

- **Target:** PS Vita, firmware 3.65, permanent Enso CFW. PSTV out of scope.
- **Toolchain:** VitaSDK 2026.08, libvita2d.
- **Auth:** UNSAFE eboot by default (`0x2F00000000000001`), because `uma0:`
  capacity needs `sceIoDevctl` (D42). `./build.sh --safe` builds the
  restricted `0x2F00000000000002` variant.
- **Status:** builds and packages (108 KB VPK, b28), runs on the 3.65 console at
  b13; 191 host checks pass. The tabbed build is pending the on-console check.

The plugin screen resolves the active config the way taiHEN does — `ux0:` first,
`ur0:` fallback — and names any ignored file. It lists every path in that config,
active or commented, grouped by load target, and one row per unique path.
`henkaku.suprx` and the three boot modules cannot be disabled; the DRM, storage,
and other load-bearing plugins warn before a change. Every session rotates three
backups (`.bak`, `.1.bak`, `.2.bak`) before reading the config, so a bad change
has a way back.

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

Produces `build/cockpit-v<version>-b<build>.vpk`, e.g. `cockpit-v01.00-b28.vpk`.
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
| D-pad up/down, left stick | scroll HOME, or move the PLUGINS selection |
| Cross | toggle the selected plugin, or confirm a modal |
| Circle | cancel a modal |
| PS button | leave the app (system) |

## Test without hardware

```sh
./tools/test/run.sh
```

Links the real `core/` against a fake port and asserts the sampling policy, the
plugin config model, the toggle and backup path, and the input reducer. Exits
non-zero on failure. 191 checks.

## Preview without hardware

```sh
./tools/preview/run.sh
```

Renders the shipped composition (`app/screen.c`) through a software vita2d shim
and the same fake platform, writing nine scenes under `build/preview/`:
`preview-healthy.png`, `preview-alert.png`, `preview-plugins.png`,
`preview-groups.png`, `preview-duplicate.png`, `preview-confirm.png`,
`preview-write-failure.png`, `preview-readonly.png`, and
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
  panels/             one file per panel, the plugin ledger, and the layout registry
  platform/           the SDK adapter, the only file that samples the device
  ui/                 screen chrome: header, tabs and summary, modal
tools/
  fake/               shared mutable port double
  test/               host tests for the sampling policy
  preview/            host renderer, not linked into the app
```

The dependency rule and what may import what are in
[ARCHITECTURE.md](docs/ARCHITECTURE.md). Read that before adding a module.
