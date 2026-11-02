# 12 — Working Projects

Three projects build here; `vita-cockpit` has run on hardware. The public
repository is <https://github.com/upakul/vitahomebrew>. Run commands from the
workspace root, `/Users/ghostmeson/developer/builds/psvitahomebrew`.

| Project | Path | Purpose | Auth |
|---|---|---|---|
| `vita-cap-test` | `vita-cap-test` | I/O + audio + WiFi + library test | unsafe `...0001` |
| `vita-kernel-demo` | `vita-kernel-demo` | App + `.skprx` + custom syscall | unsafe `...0001` |
| `vita-cockpit` | `vita-cockpit` | System dashboard in Cyberpunk 2077 HUD style | unsafe `...0001` default; `--safe` `...0002` |

Source copies are in [`examples/`](examples/), including `examples/cockpit/` for
the port, the adapter, the panel contract, and the layout table.

---

## 1. `vita-cap-test`

### What it does

At launch, runs:

1. **File I/O** — creates `ux0:data/vita-cap-test`, writes `io.txt`, reads it
   back, compares.
2. **Audio** — opens a BGM port, plays a 0.5 s 440 Hz mono sine wave.
3. **WiFi** — loads `SceNet`, `sceNetInit`, `sceNetCtlInit`, reads IP.
4. **DNS** — resolves `example.com` with `gethostbyname`.
5. **HTTP** — connects to `example.com:80`, sends `HEAD /`, reads status.
6. **Libraries** — prints versions from curl, SDL2, SDL_mixer, libarchive,
   libwebp, mpg123, opus, vorbis, FLAC, mbedTLS.

### Build

```sh
cd vita-cap-test
./build.sh
```

Output:

```text
build/cap-test-v<version>-b<build>.vpk   (2.6 MB)
```

The VPK is large because it links every library being checked. A real app would
link only what it uses.

### Verified imports

The VELF import table includes:

```text
SceAudio      :: sceAudioOutOpenPort, sceAudioOutOutput, sceAudioOutReleasePort
SceIofilemgr  :: sceIoRead, sceIoWrite, sceIoClose, ...
SceNet        :: sceNetInit, sceNetConnect, sceNetSend, sceNetRecv, ...
SceNetCtl     :: sceNetCtlInit, sceNetCtlInetGetInfo, sceNetCtlTerm
SceSysmodule  :: sceSysmoduleLoadModule, sceSysmoduleUnloadModule
SceLibKernel  :: sceIoOpen, sceIoMkdir, sceKernelExitProcess, ...
```

### How to run

```sh
cd vita-cap-test
./send.sh <vita-ip>
```

On the Vita: VitaShell → `ux0:data` → install → launch **Vita Cap Test**.

Expected lines:

```text
I/O: OK (wrote and read 20 bytes)
Audio: OK (440 Hz mono, 0.5 s)
WiFi IP: 192.168.x.x
DNS: example.com -> ...
HTTP: HTTP/1.1 200 OK
curl: libcurl/8.22.0 ...
...
```

If WiFi is off, I/O/audio still pass; network lines show errors.

---

## 2. `vita-kernel-demo`

### Architecture

```text
kernel-demo-v<version>-b<build>.vpk
├── eboot.bin              user app (UNSAFE)
├── kernel_demo.skprx      taiHEN kernel plugin
└── sce_sys/...            LiveArea assets
```

Flow:

```text
App:
  taiLoadStartKernelModule("app0:kernel_demo.skprx")
        │
        ▼
Kernel plugin module_start:
  taiHookFunctionExportForKernel(
      KERNEL_PID, &open_hook,
      "SceIofilemgr", TAI_ANY_LIBRARY,
      0xCC67B6FD,             // _sceIoOpen
      hook_sceIoOpen)

  hook_sceIoOpen:
      copy user path to kernel buffer
      fd = TAI_CONTINUE(...)
      count successful opens
      remember last path

App:
  kdemo_get_open_count()   ── generated syscall stub ──► kernel plugin
  kdemo_get_last_path()    ── generated syscall stub ──► kernel plugin
```

### Build

```sh
cd vita-kernel-demo
./build.sh
```

Output:

```text
build/kernel_demo.skprx   (4.8 KB)
build/kernel-demo-v<version>-b<build>.vpk (74 KB)
```

### Verified kernel plugin imports

```text
SceSysmemForDriver :: ksceKernelStrncpyFromUser
SceSysmemForDriver :: ksceKernelStrncpyToUser
taihenForKernel    :: taiHookFunctionExportForKernel
taihenForKernel    :: taiHookReleaseForKernel
```

### Verified app imports

```text
KernelDemo :: kdemo_get_open_count
KernelDemo :: kdemo_get_last_path
```

### Key CMake details

- Kernel plugin: `add_executable`, `-nostdlib`, `taihenForKernel_stub`,
  `SceSysmemForDriver_stub`, `vita_create_self(... CONFIG kernel/exports.yml UNSAFE)`.
- Stubs: `vita_create_stubs(kernel_demo_stubs kernel_demo kernel/exports.yml KERNEL)`.
- Reset `VITA_ELF_CREATE_FLAGS`/`VITA_MAKE_FSELF_FLAGS` before the app.
- Package the `.skprx`:
  `FILE ${CMAKE_CURRENT_BINARY_DIR}/kernel_demo.skprx kernel_demo.skprx`
- Depend on `kernel_demo.skprx-self`.
- `__sce_headroom=0x10000`.

### How to run

```sh
cd vita-kernel-demo
./send.sh <vita-ip>
```

On the Vita: install, launch **Kernel Demo**. Expected:

```text
Kernel plugin demo
Plugin: loaded (modid 0x...)
Kernel opens seen: N
Last path: app0:eboot.bin
```

Press **START** to exit. The plugin remains loaded; reboot to clear it (it is
not in `config.txt`).

### Safety

- Only reads/counts paths; no code patches, no kernel writes.
- Loaded on demand; reboot clears it.
- Hold **L** at boot if a plugin causes a bootloop.
- Do not add to `*KERNEL` until tested.

### Extending

- Add `kdemo_reset()`.
- Hook `ksceIoClose` and track open/close pairs.
- Add a ring buffer for the last N paths.
- Hook a game/system app import and filter by title ID.
- Add a narrow kernel-memory read export only with a whitelist.

---

## 3. `vita-cockpit`

### What it does

A two-tab system dashboard: clocks, memory, storage, power, identity, and frame
health on HOME, plus a PLUGINS ledger that can enable or disable a taiHEN plugin
from the console. Cyberpunk 2077 HUD as the visual reference.

### Status

**Hardware-verified at b13; b38 is built and host-tested, pending the fixed
tabbed check.** Runs on the 3.65 console: every panel renders, and `ux0:` reads
`17.7/119.3 GB` through `sceAppMgrGetDevInfo`. The shipped build is `UNSAFE` by
default (D42) because `uma0:` capacity needs `sceIoDevctl`, which the safe authid
refuses; `./build.sh --safe` still builds the restricted variant. `uma0:` reads
`--` when no second volume is mounted.

HOME carries a read-only `PLUGINS` summary block below STORAGE and SYSTEM: the
activated count at primary scale, `PLUGINS ONLINE`, the non-empty group counts,
and pending/config-warning tokens (D67). It reports the launch set, sits below
the fold, and scrolls with HOME; every edit is on the PLUGINS tab.

PLUGINS resolves the active config the way taiHEN does (D55), lists every path
active or commented, and groups by load target — KERNEL, SYSTEM, SYSTEM APPS,
ALL TITLES, TITLES. One row per unique full path; a duplicate basename shows the
full path. Every row has a status badge (`ACTIVATED`, `DISABLED`, `MISSING`,
`PARTIAL`, `NOT REACHED`, `BOOT`) and an action control that is greyed when the
action is impossible (D66). A missing active file can be disabled; a missing
inactive file cannot be enabled. `henkaku.suprx` and the three boot modules are
locked; the DRM, storage, and other load-bearing plugins warn before a change
(D58). The active config gets `config.txt.original` once and
`config.txt.session` every launch (D61); `boot_config.txt` is never opened,
read, or backed up. A write is a verified rename-away swap, and a failure names
the stage (`OPEN`, `DATA`, `VERIFY`, `MOVE`, `RENAME`, `READBACK`, `RESTORE`)
(D64).

The route here was three hardware faults, each fixed by evidence rather than
guesswork:

1. **GPU fault on launch.** `vita2d_draw_array` hands the caller's pointer to
   `sceGxmSetVertexStream` with no copy, so it needs GPU-mapped memory; it was
   given stack and `.bss` buffers (D29). The two pre-fix dumps fault on the last
   page of the main thread stack (`stack+0x3f000`), requestor PDS vertex/uniform
   fetch — see `gpu-crash-study/ANALYSIS.md`.
2. **Blank text, traces and hazard stripes.** Everything drawn through
   `vita2d_draw_array` rendered nothing while rectangle/line chrome worked, so the
   primitive was removed entirely; the renderer now uses only
   `vita2d_draw_rectangle` and `vita2d_draw_line` (D31).
3. **Blank storage.** `sceIoDevctl` on `ux0:` is refused under the safe authid
   (`0x80010030`) and works only under `UNSAFE`. Replaced with
   `sceAppMgrGetDevInfo`, a safe app-manager call that reports the same numbers
   (D38).

| Path | What |
|---|---|
| `src/core/` | `platform.h` (the port), `metrics` (policy), `history` (ring), `plugins` (launch set + catalog), `tai_config.h` (shared grammar), `plugin_editor` (line model, two snapshots, toggle), `input` (edges and repeats) |
| `src/platform/` | `platform_psp2.c`, the only SDK sampling and file adapter |
| `src/gfx/` | `draw` (vita2d adapter), `font` (5×7 face), `theme` (palette, chrome, meters, traces) |
| `src/panels/` | `core`, `storage`, `power`, `system`, `plugins` (the ledger), `plugins_summary` (the HOME block), `frame`, plus `registry.c` with the home layout table |
| `src/ui/` | `header`, `tabbar` (tabs and the PLUGINS header line), `modal` |
| `src/app/` | lifecycle, frame loop, `UiState`, and `screen_draw` |
| `tools/fake/` | shared mutable port double |
| `tools/test/` | host tests for the sampling policy, config model, snapshots, write transaction, and input |
| `tools/preview/` | host renderer: draws the real screen to PNG without a Vita |

### Build

```sh
cd vita-cockpit
./build.sh
```

Output: `build/cockpit-v<version>-b<build>.vpk` (e.g. `cockpit-v01.00-b38.vpk`).
`build.sh` counts distinct builds in `BUILD_NUMBER`: unchanged sources reuse the
number, and only the newest VPK is kept (D46). Upload it with `./send.sh <vita-ip>`.

### Test and preview without hardware

```sh
./tools/test/run.sh      # 316 assertions over the sampling policy, plugin editor, snapshots, write transaction, and input
./tools/preview/run.sh   # 15 PNG states, with a 6,500 draw-call budget
```

### Preview without hardware

```sh
cd vita-cockpit
./tools/preview/run.sh
```

Writes 15 `build/preview/preview-*.png` states (the authoritative list is in `vita-cockpit/README.md`). It links the shipped
composition (`app/screen.c`), the real `theme`, `font`, `panels`, `header`, and
`core`, a software vita2d shim, and the shared fake platform in `tools/fake/`.
Each scene is produced by driving the fake port through `metrics_sample_slow()`,
so a PNG proves the sampling policy ran, not just that the panels can draw. It
is not part of the app build.

### Design

The layout is asymmetric by decision, not accident: a hero region at 552 × 210, three
satellites, and a 44 px status strip. Do not return it to a uniform grid.

Read [`vita-cockpit/docs/DESIGN.md`](../vita-cockpit/docs/DESIGN.md) before
changing anything that renders, and
[`vita-cockpit/docs/DECISIONS.md`](../vita-cockpit/docs/DECISIONS.md) before
reversing a choice.

### Platform claims

| Claim | Status |
|---|---|
| `SceIoDevInfo` layout | `[V]` `psp2common/kernel/iofilemgr.h`, 24 bytes |
| `sceIoDevctl("ux0:", 0x3001, …)` command | `[V]` `psp2/io/devctl.h` demonstrates it verbatim |
| Its results are byte counts | `[C]` the unit is documented nowhere |
| Model codes | `[V]` `SCE_KERNEL_MODEL_VITA` / `VITATV` in `psp2common/kernel/sysmem.h` |
| `sceKernelGetModel()` cannot separate PCH-1000 from PCH-2000 | `[V]` one return code for both |

Two claims here were wrong in the same direction — marked as community knowledge
when the SDK declares them. `SceIoDevInfo` is a 24-byte SDK type, not a 16-byte
community struct, and the model codes are an SDK enum, not private constants.
Only the devctl byte unit remains genuinely unconfirmed, and it is isolated in
`platform/platform_psp2.c` behind sanity bounds that render `--` rather than a
fabricated capacity.

---

## Build environment

| Item | Value |
|---|---|
| VitaSDK | 2026.08 |
| GCC | 15.2.0 |
| Host | macOS Apple Silicon |
| Target | 3.65 Enso |
| CMake | 4.4.4 |

## Rebuild all

```sh
export VITASDK="$PWD/vitasdk"
export PATH="$VITASDK/bin:$PATH"

for p in vita-cap-test vita-kernel-demo vita-cockpit; do
  (cd "$p" && ./build.sh)
done
```

## Upload all

```sh
VITA_IP=192.168.1.42
(cd vita-cap-test && ./send.sh "$VITA_IP")
(cd vita-kernel-demo && ./send.sh "$VITA_IP")
(cd vita-cockpit && ./send.sh "$VITA_IP")
```
