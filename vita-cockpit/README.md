# COCKPIT

A system dashboard for the PS Vita. Cyberpunk 2077 HUD as the visual reference.

It reads the machine it is running on — clocks, memory, storage, power, identity,
frame health — and the taiHEN plugins it has loaded, and draws it all as a
single live instrument panel.

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
│ ACTIVATED PLUGINS                                            │
│ KUBRIDGE.SKPRX ──────────────────────────────── ▪            │
│ kernel-user bridge module                                    │
│ ...                                                          │
├──────────────────────────────────────────────────────────────┤
│ FRAME  58.4 FPS  17.1 MS   ─┼─────┼───┼──                   │
└──────────────────────────────────────────────────────────────┘
```

- **Target:** PS Vita, firmware 3.65, permanent Enso CFW. PSTV out of scope.
- **Toolchain:** VitaSDK 2026.08, libvita2d.
- **Auth:** UNSAFE eboot by default (`0x2F00000000000001`), because `uma0:`
  capacity needs `sceIoDevctl` (D42). `./build.sh --safe` builds the
  restricted `0x2F00000000000002` variant.
- **Status:** builds and packages (89 KB VPK), runs on the 3.65 console;
  87 host checks pass.

The plugin ledger reads every activated entry in `ur0:tai/config.txt` (and the
legacy `ux0:` file), groups it by load target — KERNEL, SYSTEM, SYSTEM APPS,
ALL TITLES, TITLES — and draws a row for each file. Entries whose file is
missing are dropped, commented lines are ignored, and a plugin outside the
description catalog keeps its row and says `N/A`.

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

Produces `build/cockpit-v<version>-b<build>.vpk`, e.g. `cockpit-v01.00-b13.vpk`.
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
| D-pad up/down, left stick | scroll the panel area |
| PS button | leave the app (system) |

## Test without hardware

```sh
./tools/test/run.sh
```

Links the real `core/` against a fake port and asserts the sampling policy: the
alert threshold at its boundary, the failed-read path, the unit conversions, and
the history ring. Exits non-zero on failure.

## Preview without hardware

```sh
./tools/preview/run.sh
```

Renders the shipped composition (`app/screen.c`) through a software vita2d shim
and the same fake platform, writing
`build/preview/preview-{healthy,alert,scrolled,absent}.png`. Each scene is produced by
driving the fake port through the real sampler, so a PNG proves the policy ran.
Every number in it is synthetic, so it proves nothing about the device — only
about the screen.

## Source map

```
src/
  main.c              composition root: vita2d init/teardown
  app/                lifecycle, frame loop, and screen_draw
  core/               sampling policy, history, plugin config and catalog, the port
  gfx/                vita2d adapter: primitives, font, theme, layout
  panels/             one file per panel, plus the layout registry
  platform/           the SDK adapter, the only file that samples the device
  ui/                 screen chrome (header)
tools/
  fake/               shared mutable port double
  test/               host tests for the sampling policy
  preview/            host renderer, not linked into the app
```

The dependency rule and what may import what are in
[ARCHITECTURE.md](docs/ARCHITECTURE.md). Read that before adding a module.
