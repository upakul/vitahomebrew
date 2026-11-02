# Project

What COCKPIT is, what it is not, and where it stands.

---

## 1. Purpose

A live readout of the machine it runs on, plus one screen where the taiHEN
plugin list can be changed. It is an instrument panel first: every number it
shows is real, and the plugin manager exists because a readout you cannot act
on becomes an inventory.

The product rule is that **every number on screen is real**. A fabricated
metric would poison the credibility of the whole panel, so anything the
platform cannot report is omitted rather than approximated.

## 2. Scope

### v1 — in

| Region | Contents |
|---|---|
| HOME header | wordmark, clock, HOME/PLUGINS tabs |
| CORE | ARM, GPU, and bus clock; free user / CDRAM / physical-contiguous pools; 20 s memory trace |
| STORAGE | `ux0:`, `uma0:` used and total, with the unit chosen from each volume's size (GB or MB), segmented meters |
| POWER | battery percent, charging state, temperature, voltage, cycles, health |
| SYSTEM | firmware string, app run time, device model |
| PLUGINS summary | a full-width block below both columns: loaded count, group counts, and a config warning |
| PLUGINS tab | the full config ledger with loaded/next-boot badges, greyed/active controls, a confirmation and receipt for every change, a session sheet with reboot and one-step undo, read-only unlisted files, and the config snapshots |
| FRAME | measured FPS, frame time, 20 s trace |

The wall clock and the wordmark are header chrome, not a panel, so identity
appears exactly once on screen.

### v1 — out

- Network, audio, camera, motion.
- Installing, deleting, or downloading plugin files.
- Editing, reading, copying, or backing up `ur0:tai/boot_config.txt` or any Enso
  boot file. The app never opens it; boot modules are locked by the three known
  names only.
- A third tab, batch toggle, presets, or a config diff/restore UI.
- Any kernel plugin. This is a userland app.

### Later

App and bubble inventory, per-title storage attribution, plugin presets,
network state, screenshot browser, theme switching. The panel contract exists so
these are additive, not invasive. See §7.

## 3. Target and constraints

- PS Vita on firmware 3.65 with permanent Enso CFW.
- PSTV is out of scope; its differences are not handled.
- Unsafe eboot. Reading `uma0:` capacity needs a call the safe authid refuses,
  so the app ships `UNSAFE` (D42); `build.sh --safe` builds the restricted
  variant.
- No hardware is reachable from the development environment. Every runtime
  behaviour is unverified until someone runs it.

The full constraint table, with `[V]`/`[C]` verification status for each
platform claim, is in [ARCHITECTURE.md §7](ARCHITECTURE.md#7-platform-claims-and-their-status).

## 4. Build and deploy

```sh
./build.sh                 # cmake configure + build
./send.sh 192.168.1.42     # upload the newest VPK over FTP
```

`build.sh` locates `../vitasdk` automatically, so no global `VITASDK` export is
required. On the Vita, open VitaShell, press Select to start the FTP server, then
run `send.sh` with the address it shows.

Scroll the panel area with the d-pad or the left stick. The header and the
FRAME strip stay pinned; a thin bar on the right shows position. Press L or R to
switch between HOME and PLUGINS. The HOME `PLUGINS` block is read-only: it counts
the launch set, and it sits below the fold, so scroll down to reach it. Pending
edits are shown only on the PLUGINS tab that made them. On PLUGINS, up/down
moves the selection and Cross opens a confirmation for the selected row; every
actional change confirms, names the next-boot effect, and opens a receipt after
the write. `TRIANGLE` opens `PLUGIN SESSION`, where `REBOOT NOW` applies the
changes and `UNDO LAST CHANGE` restores `config.txt.undo`; Circle cancels. At the
last row, another down press scrolls to the installed-but-unlisted files; Cross
on a greyed control explains why it cannot act instead of writing. A change is
written immediately and applies on the next boot. If a write fails, the message
names the stage (`OPEN`, `DATA`, `CLOSE`, `VERIFY`, `MOVE`, `RENAME`, `READBACK`,
`RESTORE`, or `UNDO`).

Install by selecting the VPK in `ux0:data` inside VitaShell and pressing Cross.
The bubble appears on LiveArea.

### Every build gets a new artifact name

**Rule: no two builds may share a filename.** A VPK that overwrites its
predecessor cannot be identified after the fact — not on the console, not in
`ux0:data`, not in a bug report. This is not optional bookkeeping; it is what
makes "which build is on the Vita?" answerable.

The artifact is named:

```
build/cockpit-v<version>-b<build>.vpk        e.g. cockpit-v01.00-b44.vpk
```

| Part | Source | Changes |
|---|---|---|
| `version` | `VITA_VERSION` in `CMakeLists.txt` | only on a release, and it must stay `##.##` because it also becomes `APP_VER` |
| `build` | `BUILD_NUMBER`, incremented by `build.sh` | **every build**, automatically |

The split is deliberate. `APP_VER` has to be `##.##`, so a per-build version
would either break the SFO or lie about the release. The build number carries
per-build identity; the version carries release identity. Both appear in the
filename, so one glance identifies the artifact.

Mechanics:

- `BUILD_NUMBER` lives in the project root, **not** in `build/`. Wiping the build
  directory must not restart numbering into a collision with VPKs already copied
  to the console.
- `build.sh` hashes the sources. Unchanged sources reuse the previous number;
  changed sources increment it. Rebuilding to check for warnings therefore does
  not burn a number.
- Only the newest VPK is kept. Superseded artifacts are deleted after every
  build, so the directory always holds exactly the one to flash.
- `send.sh` uploads the newest `build/cockpit-v*.vpk`. Old builds are left alone
  so several can sit in `ux0:data` side by side.
- A release that must keep a fixed name (a store upload, say) is a deliberate
  exception and should say so in `DECISIONS.md`; it is not the default.

Do not hand-edit `BUILD_NUMBER` downward to reuse a number. If numbering has to
restart, delete the stale VPKs first and record why.

## 5. Verification status

COCKPIT inherits the workspace rule: a claim is `[V]` only when it has been
checked against a primary source or observed on hardware, and `[C]` when it is
community knowledge.

| Area | Status |
|---|---|
| Builds and packages | `[V]` clean build, 117.0 KB VPK (b44), no warnings |
| Host tests (sampling, plugin model, snapshots, undo, recovery, write transaction, input) | `[V]` `tools/test/run.sh`, 400 checks |
| Plugin list (`ux0:` first, `ur0:` fallback) | `[V]` host harness keeps a 19-entry real config in full and groups it as KERNEL 9 / SYSTEM 8 / SYSTEM APPS 1 / ALL TITLES 1; host tests cover BOM, commented entries, long names, section mapping, priority merge, the 64-entry ceiling, full-path duplicate rows, `*!` halts, lock/warn policy, the two snapshots, and the exact `#` edit |
| Clocks, memory, power APIs declared in SDK | `[V]` header inspection |
| Clock row follows a runtime clock change | `[C]` — the getters are re-read every sample, so an overclock applied by a plugin should appear within ~100 ms without a restart; not yet observed on hardware |
| `SceIoDevInfo` layout | `[V]` `psp2common/kernel/iofilemgr.h`, 24 bytes |
| `sceIoDevctl("ux0:", 0x3001, …)` command | `[V]` `psp2/io/devctl.h` demonstrates it verbatim |
| Storage result unit (bytes) | `[C]` — the one claim still unconfirmed |
| Device model codes | `[V]` `psp2common/kernel/sysmem.h` |
| Screen layout and typography | `[V]` host preview render, 19 states; loaded/next-boot badges, confirmation, receipt, session and restart modals, and CORE/POWER outlines (D53, D54, D66, D69-D71) |
| Renders on hardware | `[V]` 2026-11-02 (b13): text, clocks, memory, power, identity and frame all draw on the console; the production-hardened tabbed build b44 is not on hardware yet |
| Config write path | `[C]` the adapter's rename-away swap and live read-back are implemented and build; host tests exercise the same transaction in the fake, not the SDK calls, and the target's rename semantics are unverified. A failure names the stage (`OPEN`, `DATA`, `VERIFY`, `MOVE`, `RENAME`, `READBACK`, `RESTORE`). |
| STORAGE reads | `[V]` both volumes report on hardware under `UNSAFE` (D42); `ux0:` also works on the safe build via `sceAppMgrGetDevInfo` (D38) |
| Behaviour not listed above | unverified |

The SYSTEM panel shows **FW 3.74** because the console's version string is
spoofed; the machine is on 3.65 Enso. That is the value the API returns, so it is
correct to display. See `AGENTS.md` and `agent-wiki/01-platform.md`.

The host tests (`tools/test/run.sh`) link the real `core/` against a fake port and
assert the alert policy at its boundary, the failed-read path, the unit
conversions, the history ring, the config model, the loaded/next-boot split, both
snapshots, the one-step undo, interrupted-swap recovery, the ux0-unreadable rule,
the exact `#` edit, the write transaction and its restore path, and the input
reducer. The host
preview (`tools/preview/run.sh`) renders the shipped composition against the same
fake. Neither proves anything about the device: every reading in them is
synthetic.

## 6. Failure policy

Panels degrade, they do not crash.

- A metric that could not be sampled renders `--`, never a stale or invented
  value.
- A failed sample raises `g_metrics.alert`, which draws the hazard band.
- A storage mount that does not exist (no `uma0:` without a second volume)
  is shown as absent, not as an error. A mount whose call *failed* shows the
  return code, so a permission problem is not mistaken for a missing card.
- A config write that fails leaves the original file untouched and shows
  `WRITE FAILED <stage>`; a snapshot failure shows `BACKUP WARNING` but never
  locks the editor. A failed restore says the config may be incomplete and tells
  the user not to reboot.
- `config.txt.old` is moved back before the config is declared missing. A
  `ux0:` config that exists but cannot be read is read-only and named; the app
  never writes the shadowed `ur0:` copy in its place.
- `config.txt.undo` is written before every toggle and is the only in-app
  restore source. `REBOOT NOW` is refused below 20 % battery unless charging.
- A config that is too large, too long, or unreadable is shown read-only with the
  reason, never rewritten from a truncated read. A path before the first section
  is a warning, not a lock.
- Only the active `config.txt` and its `config.txt.original` /
  `config.txt.session` snapshots are written. `boot_config.txt` is never opened,
  read, copied, renamed, or written.

## 7. Roadmap

| Step | Adds | Needs |
|---|---|---|
| v1 | the dashboard and the plugin manager | b44 builds and is host-tested; the hardened tabbed UI is pending the console check |
| v2 | app inventory, per-title storage | `sceAppMgrGetAppInfo`, directory walk of `ux0:app` |
| v3 | network status | `SceNetCtl` |
| v4 | theme switching, a second visual language | a theme abstraction in `gfx/theme` |

Each step adds panels and at most one adapter. None of them change the frame
loop or the panel contract.
