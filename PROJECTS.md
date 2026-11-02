# Project tracker

The live state of every project in this workspace. **Update your project's row
after every job** — a job is not finished until this file reflects it.

`AGENTS.md` defines the field rules. The short version:

| Status | Means |
|---|---|
| `planning` | design or scope in progress; no code yet |
| `building` | code exists but does not build or is incomplete |
| `built` | compiles and packages; has not run on hardware |
| `verified` | has run on the target console, with evidence |
| `blocked` | cannot progress; the reason is in Notes |

`built` → `verified` requires hardware evidence. Never promote without it.

---

## Board

| Project | Status | Last job | Next | Notes |
|---|---|---|---|---|
| `vita-cap-test` | `built` | 2026-11-02 — artifact renamed to `cap-test-v<version>-b<build>.vpk` with an auto-incrementing build number | Run on hardware; confirm I/O, PCM audio, WiFi/DNS/HTTP | Unsafe `...0001`. 2.6 MB VPK. |
| `vita-kernel-demo` | `built` | 2026-11-02 — artifact renamed to `kernel-demo-v<version>-b<build>.vpk`; the VPK CMake target is now interpolated from the artifact name | On-demand load test; confirm hook UID and the open counter | Not in `*KERNEL`. 3.65 NID unverified. |
| `vita-cockpit` | `built` | 2026-11-02 — fixed the hardware regressions: permanent `N PLUGINS ONLINE` summary, exact `#` toggles, BOM and stray-path warnings, two snapshots (`config.txt.original`/`.session`), rename-away verified writes with stage-named failures, status badges and greyed controls, `boot_config.txt` out of scope | Flash `cockpit-v01.00-b35.vpk` and verify the summary, every badge/control state, a toggle surviving reboot, the two snapshots, and that no `.bak`/`boot_config` file was created | `UNSAFE` default. 316 host checks. 112.1 KB VPK. 13 previews, busiest 5,793 draw calls. The SDK rename transaction is `[C]`. |

---

## Log

Newest first. One line per job: date, project, what changed. Keep the board
above as the summary; this is the history.

Build numbers before 2026-11-02 predate the counter reset that pruned the stale
VPKs; an old `b7`, or the `b10` in the 2026-11-02 storage-error entry, predates
the counter reset. The date and the entry around it distinguish it from the
current `b8`+ builds.

| Date | Project | Change |
|---|---|---|
| 2026-11-02 | `vita-cockpit` | Fixed the plugin-manager hardware regressions: the HOME summary is permanent label-scale `N PLUGINS ONLINE` above the hazard band (D63); toggles are an exact two-state `#` edit with BOM and stray-path warnings (D62, D65); the write is a verified rename-away swap with stage-named failures (D64); the active config gets `config.txt.original` + `config.txt.session` only (D61); every row has a status badge and a greyed/active control (D66); `boot_config.txt` is no longer read or backed up. 316 checks, 13 previews, b35 at 112.1 KB. Hardware check pending. |
| 2026-11-02 | `vita-cockpit` | Implemented the tabbed plugin manager: HOME summary and PLUGINS ledger (D54, D60), taiHEN config precedence (D55), byte-preserving full-path rows (D56), transactional writes with three-deep session backups (D57), lock/warn policy (D58), launch-set counts and session-scoped pending (D59). 191 host checks, 9 preview states, b28 at 108 KB. Hardware check pending. |
| 2026-11-02 | `vita-cockpit` | Planned the tabbed plugin manager: HOME summary plus a selectable PLUGINS screen that toggles `config.txt` lines with next-boot effect; research corrected the active-config precedence, locked `henkaku.suprx`, and found storage drivers need a special warning. Plan doc-reviewed with 5 reviewers; no code changed. |
| 2026-11-02 | `vita-cockpit` | Removed the header hairline and inset the scrolling band by 12 px (band 64..472, content starts at 76), so CORE/POWER outlines are no longer clipped and both panel rows fit at scroll 0; scroll range unchanged (D53). |
| 2026-11-02 | `vita-cockpit` | Grouped the plugin ledger by load target — KERNEL, SYSTEM, SYSTEM APPS, ALL TITLES, TITLES — with one row per file under its most privileged section (D51), and made an unknown description render `N/A` instead of a blank line (D52). Real 19-entry config: 9/8/1/1. 87 checks. |
| 2026-11-02 | `vita-cockpit` | Fixed the plugin list dropping entries: `PLUGINS_MAX` 12 → 64, name field 32 → 48 and path 64 → 96, duplicates checked before the ceiling, and a `MORE PLUGINS NOT SHOWN` notice if the array ever fills (D50). A real 19-entry config now parses 19/19; 74 checks. |
| 2026-11-02 | workspace | Published the workspace to `github.com/upakul/vitahomebrew`: root `README.md`, MIT `LICENSE`, and a `.gitignore` that excludes the SDK, build output, crash dumps, and the third-party parser clone. |
| 2026-11-02 | `vita-cockpit` | Applied the simplification review: deleted the preview's dead pool shim, derived canvas height and max scroll (D49), culled off-screen panels and plugin rows, unified PANEL_INSET, replaced history_max/history_min with one history_range, moved the storage errno byte into the port, and rewrote three false-confidence tests. 57 checks. |
| 2026-11-02 | workspace | Doc sweep after the spacing and catalog work: wiki cockpit section now says UNSAFE default, 54 checks, four preview states and b7; ARCHITECTURE documents the mutable PLUGINS slot; DESIGN documents the plugin ledger; PROJECT roadmap marks v1 done; wiki README lists cockpit. |
| 2026-11-02 | `vita-cockpit` | Added the full AutoPlugin2 Vita catalog: 155 plugin descriptions keyed by file name, including YAMT, HENkaku, VitaBtFix, ioPlus and kuio. Corrected four catalog rows that carried another plugin's text or unusable text. Host tests now 54 checks. |
| 2026-11-02 | `vita-cockpit` | Doc repair while checking the spacing pass: README still claimed a safe eboot and "nothing has run on hardware"; `13-fact-check.md` said 33 checks / three preview states / untested; PROJECT.md said 76 KB. All corrected to UNSAFE default, 46 checks, four states, hardware-verified, 81 KB. |
| 2026-11-02 | `vita-cockpit` | Spacing and type pass (D48): `SP_1`-`SP_6` scale, `PANEL_BODY_Y`, `READOUT_STRIDE`; CORE/POWER both 210, STORAGE/SYSTEM both 172 at the same y; CORE trace 29 -> 40 px; plugin description at micro scale; absent meter track made visible; FRAME units baseline-aligned. |
| 2026-11-02 | `vita-cockpit` | PLUGINS panel redesigned (D47): two lines per plugin with a leader rule and a green status dot, panel height computed from the list so nothing is clipped. Descriptions taken from AutoPlugin2's catalog and English strings. |
| 2026-11-02 | workspace | Removed `vita-hello` and `vita-gpu-guard` at the user's request. References swept from `AGENTS.md`, the tracker, and the wiki; `vita-cockpit` is now the build example in the quickstart. |
| 2026-11-02 | `vita-cockpit` | Added the PLUGINS panel (D43): reads both tai configs through the port (D44), keeps only entries whose file exists, and shows name + role. Layout rebalanced to make room above FRAME; `CHROME_LIST` added for a multi-row inline-title panel. |
| 2026-11-02 | `vita-cockpit` | **First fully working hardware run.** Every panel renders; `ux0:` reads 17.7/119.3 GB through `sceAppMgrGetDevInfo` on the safe eboot (D38), so no unsafe build is needed. `uma0:` reads `--` because nothing is mounted there. START no longer exits (D39). Promoted to `verified`. |
| 2026-11-02 | `vita-cockpit` | Decoded b10 storage errors: `uma0:` = `ENODEV` (correct for an absent volume), `ux0:` = `0x80010030` on a mounted card. Added a marked `--unsafe` build (D36) to test whether the safe authid refuses `sceIoDevctl`. Sweep moved to 5 s (D35) after a report that the screen looked frozen; sampling was verified present in the binary at 10 Hz. |
| 2026-11-02 | `vita-cockpit` | Hardware b7 renders correctly except STORAGE, which is `--` on every mount. Trimmed STORAGE to `ux0:`+`uma0:` (D33, per user) and made a failed mount print its devctl return code so a denied call and a misread reply stop looking identical (D34). Bounds relaxed for one diagnostic build. |
| 2026-11-02 | workspace | Artifact naming made a binding workspace rule in `AGENTS.md` (5 rules + a definition-of-done item + a "what not to do"). All four projects now increment `BUILD_NUMBER` on every build and emit `<project>-v<version>-b<build>.vpk`. |
| 2026-11-02 | `vita-cockpit` | Removed `vita2d_draw_array` from the renderer (D31): on hardware everything drawn through it — panel text, traces, hazard stripes — rendered nothing, while `vita2d_draw_rectangle`/`draw_line` chrome rendered fine. `draw_rects` is now a loop, `draw_tri` a scanline fill. Added build numbers to the artifact name (D32): `cockpit-v01.00-b2.vpk`. |
| 2026-11-02 | `gpu-crash-study` | Parsed all 8 dumps with `psp2-gpucrash-parse` 0.8.0: 4 BIF access faults, 2 fragment-phase 3D lockups. COCKPIT's two dumps fault on the last page of the main thread stack (`stack+0x3f000`), confirming D29's stack-vertex-buffer diagnosis. The two `.tmp` files are truncated non-GPU dumps. See `gpu-crash-study/ANALYSIS.md`. |
| 2026-11-01 | `vita-cockpit` | **Hardware finding:** first install launched and GPU-faulted. `vita2d_draw_array` passes the caller's pointer to `sceGxmSetVertexStream` with no copy, so it requires GPU-mapped memory; `draw_tri` (stack) and `draw_rects` (BSS) were both invalid. Both now allocate from `vita2d_pool_memalign`; pool sized explicitly at 2 MiB. Not yet re-tested on device. |
| 2026-11-01 | `vita-cockpit` | Applied the code-review fixes: battery temp divided by 100 (would have shown `TEMP 3100`), failed reads no longer enter the trace, alert fails closed on a power-read failure, catch-up bounded, model codes switched to the SDK enum, host tests added (33 checks), preview now renders the shipped composition through the real sampler |
| 2026-11-01 | `vita-cockpit` | Full code review: 7 reviewers, 35 candidates, 24 merged findings, 8 validated (7 confirmed, 1 rejected). Verdict `Ready with fixes` |
| 2026-11-01 | `vita-cockpit` | Full v1 written and building: `core/platform.h` port + `platform_psp2.c`, `gfx/theme`, five panels, `ui/header`, `app`, CMake/build/send. 76 KB VPK. |
| 2026-11-01 | `vita-cockpit` | Hygiene pass: removed two unused `draw_glow_*` helpers (rule 9), documented libvita2d's transitive stub requirements, added storage sanity bounds so a bad devctl layout shows `--` |
| 2026-11-01 | `vita-cockpit` | Host preview harness added (`tools/preview`); caught a solid-block trace, 2:1 label contrast, and an invented `PEAK 1` |
| 2026-11-01 | `vita-cockpit` | Corrected `SceIoDevInfo`: it *is* in the SDK at 24 bytes, not a 16-byte community struct |
| 2026-11-01 | workspace | `AGENTS.md` gained code-architecture rules, project-doc requirements, and this tracker |
| 2026-11-01 | `vita-cockpit` | Design review pass: uniform 3×2 grid replaced with hero/satellites/status strip; D17, D18 added; D11 revised |
| 2026-11-01 | `vita-cockpit` | Wrote `README.md` and `docs/{PROJECT,ARCHITECTURE,DESIGN,DECISIONS}.md` |
| 2026-11-01 | `vita-cockpit` | Drafted `gfx/draw`, `gfx/font`, `core/history`, `core/metrics` |
| 2026-11-01 | `vita-cap-test`, `vita-kernel-demo` | Doc-review corrections: FTP path, authid, NID, `SECRETSAFE`, coredump API, audio constants |
