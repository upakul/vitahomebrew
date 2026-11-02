---
title: PLUGINS Production Hardening - Plan
type: feature
date: 2026-11-03
topic: plugin-manager-hardening
artifact_contract: ce-unified-plan/v1
execution: code
---

# PLUGINS Production Hardening - Plan

## Goal Capsule

- **Objective:** Make the PLUGINS screen safe and understandable for a critical
  system-plugin editor: the row tells the truth about what is running, every
  change is confirmed and receipted, a reboot and a one-step undo are reachable,
  and a failed or interrupted write is recoverable without a PC.
- **Means:** Three independent reviews (code quality, UX, user safety) were run
  against b39. Their shared findings define the scope below. The fixes stay
  inside the existing dependency rule: policy in `core/plugin_editor`, SDK calls
  in `platform/platform_psp2`, presentation in `panels`/`ui`.
- **Out of scope:** `boot_config.txt` (never read, copied, or written), batch
  editing, presets, a config diff UI, a kernel plugin, and in-app restore from
  `.original`/`.session`.
- **Execution profile:** Deep. Stop if the target cannot be tested on hardware
  and the change would alter the write transaction without a host test.

## Review Findings That Drove the Scope

### Shared P0/P1

1. **The badge lied about the running set.** `EditorRow::status` came from the
   edited file, so a disable painted `DISABLED` while the plugin still ran.
2. **A write was silent and unbooted.** No receipt, no "applies next boot", no
   reboot action, no exit information.
3. **The screen dead-ended.** No reboot, no undo, no recovery from a bad change
   except holding L and using a PC.
4. **An interrupted swap could leave no live config.** The window between the
   two renames left only `.old`, and the next scan reported `NO CONFIG.TXT`.
5. **A failed restore was misreported.** `RENAME` was returned even when the
   restore failed and the live file was gone.
6. **An unreadable `ux0:` silently edited `ur0:`.** The edit was inert because
   taiHEN opens `ux0:` first.
7. **Locked + missing could advertise ENABLE** and then refuse it.
8. **One over-long path made the whole page read-only.**
9. **The PLUGINS header line collided with the hazard band** and duplicated the
   panel header.
10. **Interaction policy was untestable** because it lived in static functions
    in `app/app.c`.

## Requirements

- R1. `loaded` is computed from the launch file; the status badge reads `LOADED`,
  `OFF`, `MIXED`, `MISSING`, `NOT REACHED`, or `BOOT`.
- R2. A row whose file differs from launch adds `NEXT ON` or `NEXT OFF`; the
  header counts `LOADED` and `NEXT BOOT`.
- R3. Every actionable toggle opens a confirmation naming the file, next-boot
  effect, the row-specific warning, the KERNEL/SYSTEM risk, hold-L, line count,
  and session change number.
- R4. A successful write opens a receipt: written path, next boot, change saved,
  leaving COCKPIT does not cancel it.
- R5. `TRIANGLE` opens `PLUGIN SESSION` with `REBOOT NOW` when changes wait,
  `UNDO LAST CHANGE` when a snapshot differs, and `CLOSE`.
- R6. Reboot is a second confirmation, refused below 20 % battery unless
  charging, warns about the card, and handles a request that did not reset.
- R7. `config.txt.undo` is written from the verified pre-write bytes before each
  toggle. A failed undo snapshot refuses the change.
- R8. Undo restores through the same transaction, refuses if the live file
  changed on disk, and works when the live file is missing.
- R9. `.original` and `.session` are never offered as in-app restore sources.
- R10. At scan, an absent live file with a surviving `.old` is renamed back and
  reported as `RECOVERED`.
- R11. An unreadable `ux0:` config is read-only and named; neither scanner falls
  through to `ur0:`.
- R12. A failed final rename reports `RESTORE` when the original could not be
  put back.
- R13. One over-long path is skipped with `CONFIG WARNING`, not a read-only page.
- R14. A locked, missing, partial row has no action and says `MISSING`.
- R15. The PLUGINS header has no duplicate summary; the panel owns its state.
- R16. The panel shows `CROSS CHANGE   TRIANGLE SESSION` and the restore state.

## Implementation

| Unit | Change |
|---|---|
| `core/plugin_editor` | `loaded`/`next_change` per row; status text; missing-before-locked; mixed disable-first; re-stat on enable; `editor_undo`; `.undo` refresh; `.old` recovery; `ux0:` unreadable rule; over-long path skip; `editor_row_label`; action enum |
| `core/platform.h` | `platform_recover_swap`, `platform_snapshot_undo`, `platform_read_undo`, `platform_remove_undo`, `platform_request_reboot`; accurate stage list |
| `platform/platform_psp2` | implement the five calls; `sceIoSyncByFd` best-effort; report `RESTORE` honestly; no silent mount fallback |
| `core/plugins` | stop on unreadable `ux0:`; keep the HOME set consistent |
| `ui/ui_state` | receipt, session, restart modals; choice list; modal body 512 bytes |
| `ui/modal` | 600 × 240, six lines, receipts, session choice rows, restart keys |
| `core/input` | `INPUT_OPTIONS`, `modal_nav` for list modals |
| `app/app` | always-confirm path, receipt/error copy, session sheet, battery gate, reboot, undo confirm, title fitting |
| `panels/plugins` | three-line header, `LOADED`/`OFF`/`MIXED`, `NEXT ON/OFF`, white cursor, white-space fix, action enum colour |
| `tools/fake` | per-path read/write/rename failures, reboot result, recover/undo |
| `tools/test` | 396 checks: truth, locked+missing, recovery, undo, ux0 rule, RESTORE, long path |
| `tools/preview` | 19 scenes: next-boot row, receipt, session, restart |

## Test Matrix

| Area | Cases |
|---|---|
| Truth | loaded stays 1 after disable; next -1; relaunch makes the edited file the baseline |
| Policy agreement | locked+missing partial has no action and says MISSING; mixed disables first |
| Recovery | `.old` restored; `RECOVERED`; live present is not overwritten |
| Undo | snapshot before each toggle; restore after relaunch; restore with live missing; source-changed refusal; snapshot failure refuses the change |
| Mount rule | `ux0:` unreadable → read-only, no `ur0:` row, HOME unreadable |
| Write stages | DATA, RENAME, RESTORE, UNDO |
| Long path | skipped, warning, not read-only |
| Input | options edge; session up/down; text modal swallows nav |
| Preview | next-boot row, confirm, receipt, session, restart, failure, read-only |

## Failure Policy

- A failed snapshot aborts the change before the live file is touched.
- A failed live write restores the original and reports the stage.
- A failed restore says the config may be incomplete and tells the user not to
  reboot.
- A reboot below 20 % battery while discharging is refused.
- If `scePowerRequestColdReset` returns without resetting, the app says so and
  tells the user to hold power for 5 seconds.
- No code path touches `boot_config.txt`.

## Hardware Checks

1. Disable a non-warned SYSTEM row: confirm names the file and next boot; receipt
   appears; row stays `LOADED` + `NEXT OFF`; header says `1 NEXT BOOT`.
2. `TRIANGLE` → `PLUGIN SESSION` → `REBOOT NOW` → `RESTART THE VITA`; battery
   gate; console cold-resets; after boot the row is `OFF`, HOME count drops.
3. Toggle back and reboot: plugin returns.
4. Disable, then `UNDO LAST CHANGE` before reboot: config returns; row is
   `LOADED`, `0 NEXT BOOT`.
5. Disable a storage driver: the confirm names the card risk. Do not confirm on
   a daily console.
6. Force a write failure (full card) and confirm `NOT WRITTEN`, the file is
   unchanged, and the app remains responsive.
7. Power-loss drill between the renames: reboot, launch COCKPIT, confirm
   `RECOVERED`, and that the config is back.
8. Confirm `config.txt.original`, `.session`, and `.undo` exist; no `.bak` and no
   new `boot_config.txt*` file.

## Result

- 396 host checks, 0 failures.
- 19 preview scenes, busiest 6,905 draw calls, under the 8,000 budget.
- Build: `cockpit-v01.00-b50.vpk`, 115.0 KB, 0 warnings.
- `boot_config.txt` remains byte-identical in the fake; no backup beside it.
- Hardware verification is pending: the rename transaction and the cold-reset
  permission are still `[C]`.
