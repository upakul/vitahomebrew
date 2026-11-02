---
title: Fix the Plugin Manager Regressions - Plan
type: fix
date: 2026-11-02
topic: plugin-manager-regressions
artifact_contract: ce-unified-plan/v1
execution: code
---

# Fix the Plugin Manager Regressions - Plan

> **Partly superseded 2026-11-02 (b36).** The HOME summary placement — R1-R5 and
> D63 — is replaced by D67: the summary is now a full-width block below STORAGE
> and SYSTEM, and the header carries only the tabs, clock and hazard band. The
> parser, write, snapshot, and row fixes in this plan remain current.
>
> **Followed by:** `2026-11-03-1000-plugins-production-hardening-plan.md`
> (D69-D71), which adds the running-set badge, confirmations and receipts, the
> session sheet with reboot and undo, and interrupted-swap recovery.
>
> The result block at the end records the b35 build this plan produced. The
> current artifact and check counts are in `PROJECTS.md`.

## Goal Capsule

- **Objective:** Make the shipped COCKPIT plugin manager honest and reliable on the 3.65 console: HOME always names how many plugins are online, PLUGINS can always enable or disable an editable row, every config entry is visible with an explicit status and control, and the backup model is one first-session original plus one per-session snapshot.
- **Means:** Fix the parser and adapter faults that produced false read-only states, replace the three-deep rotation with `config.txt.original` + `config.txt.session`, redesign the PLUGINS row around a status badge and a greyed action control, and move the HOME summary off the hazard line.
- **Supersedes:** In part the 2026-11-02 tabbed-plugin-manager plan: R4-R6 (summary placement), R9/R12 (status and three-state cycle), R33-R34 (three-deep backups). `DECISIONS.md` D60 and D57 are superseded by new entries; the old entries stay for history.
- **Execution profile:** Deep. Stop and re-plan if the real adapter cannot verify a write after replacement, or if a hardware-only fault remains after the host evidence below.
- **Finishes:** Host tests, preview renders, a clean VitaSDK build, updated docs, and a pushed branch. The console owner performs the final on-device check.

## Reported Symptoms and Root Causes

| # | Symptom | Root cause found in the shipped code | Evidence |
|---|---|---|---|
| 1 | HOME has no activated-plugin summary | The summary is drawn at `SUMMARY_Y 48`, exactly where the hazard band is drawn (`HAZARD_Y 48`). A fault paints over it. The summary is also micro-scale `C_DIM`, and a config problem renders only a tiny `NO TAI CONFIG`. | `ui/tabbar.c:23-24,94-135`; `ui/header.c:24-25,112-123` |
| 1a | HOME can show no plugins on a valid config | A UTF-8 BOM before the first `*KERNEL` makes the parser miss the first section. The next plugin path is then "before a section", which sets `config_invalid`, zeroes the HOME count, and makes the editor read-only. | `core/plugin_editor.c:221-260,382-402`; `core/plugins.c:328-331,398-401` |
| 1b | HOME can show no plugins even with a readable config | `platform_read_config` refuses to read unless `sceIoGetstat` succeeds first. A stat-only failure becomes "absent", so the scanner never opens a file that `sceIoOpen` could read. | `platform/platform_psp2.c:263-291` |
| 2 | Toggles are unreliable | `editor_scan` sets `read_only` whenever the active-config backup copy fails. The backup copy is a three-deep rotation that rewrites an existing `.bak` by rename-over-existing, which is the exact operation the adapter marks `[C]`. The second session onward can therefore be read-only before the user touches anything. | `core/plugin_editor.c:358-379`; `platform/platform_psp2.c:294-380` |
| 2a | Toggles fail with `BACKUP MISSING OR CHANGED` | `editor_toggle` refuses unless `.bak` exists and matches the session-start bytes. A failed or deleted backup disables every toggle even though the live config is intact. | `core/plugin_editor.c:681-693` |
| 2b | Toggles fail with `WRITE FAILED` | `platform_write_config` writes a verified `.new` and then calls `sceIoRename(tmp, path)` over an existing config. If the filesystem refuses rename-over-existing, no config write ever succeeds. There is no fallback and no read-back after the replace. | `platform/platform_psp2.c:294-343` |
| 2c | A toggle can lose the tail of a large config | `platform_read_config` accepts a short read as a complete file. A short read is then parsed, and the rewrite drops every unread byte. | `platform/platform_psp2.c:277-291` |
| 3 | Three rolling backups | D57's rotation is still active: `.bak`, `.1.bak`, `.2.bak`, for both configs and `boot_config.txt`. The user asked for one first-session original and one session snapshot, and `boot_config.txt` is out of scope entirely. | `core/plugin_editor.c:358-360`; `platform/platform_psp2.c:360-380`; D57 |
| 4 | Disabled/missing entries are hard to understand | The page has one status string and no action control. Selection skips non-actionable rows, so a missing or locked row cannot even be selected to read why. A missing row's status does not say whether the control is usable. | `app/app.c:67-85`; `panels/plugins.c:136-178` |
| 4a | The enable/disable switch is absent | The only action is Cross on the selected row; there is no visible control, no greyed state, and no reason shown when Cross does nothing. | `panels/plugins.c:136-302`; `app/app.c:87-121` |
| 5 | The page is full of small faults | Orphan listing treats a missing `ux0:tai` directory as an unreadable listing, so most consoles always show `EXCLUSIONS UNVERIFIED`. Orphans are capped at 12 with no overflow notice, never sorted, and `.original`/`.session` are not excluded. Boot paths keep trailing spaces. Double-commented lines are misparsed. Section suffixes are case-sensitive. | `core/plugin_editor.c:277-352,430-438`; `platform/platform_psp2.c:382-413`; `core/tai_config.h:53-70` |

### Confirmed by inspection, not hardware

The read-only and summary faults are reachable from the code alone. The BOM, rename-over-existing, and stat-only-failure paths need the console to confirm which one the user hit. The fix removes all three as failure modes rather than guessing.

## Product Contract

### Requirements

**HOME summary**

- R1. HOME always shows a summary line in the header with `N PLUGINS ONLINE` (singular `1 PLUGIN ONLINE`). The count is the launch config's activated set: a path is counted when at least one reached line is active and the file exists.
- R2. The summary names the non-empty load groups after the total, using the short labels `KERNEL`, `SYSTEM`, `APPS`, `ALL`, and `TITLES`, and appends `N PENDING` when the session has unwritten-to-boot changes. Group counts drop from `TITLES` upward before the total or pending token is dropped.
- R3. The hazard band sits at the bottom of the header, below the summary, and never covers it. A fault no longer replaces the summary.
- R4. A config problem is named in the same line and in a visible tone: `NO CONFIG.TXT`, `CONFIG TOO LARGE`, `CONFIG UNREADABLE`, or `CONFIG WARNING` with the count that could be read.
- R5. The summary is readable at a glance: the `N PLUGINS ONLINE` run is label scale and `C_GREEN` when the count is non-zero, `C_YELLOW` for a config problem, `C_WHITE` for zero; the detail run is micro and `C_DIM`.

**Editing and backups**

- R6. A toggle changes only the `#` markers on the reached lines that name the selected path. It preserves every other byte, including leading indentation, trailing spaces, CRLF, and a missing final newline.
- R7. Enabling a row removes the comment marker(s) from its reached lines; disabling a row inserts a single `#` before the path on each reached line. A line that is already in the target state is left unchanged.
- R8. A UTF-8 BOM at the start of the config is preserved and does not hide the first section or invalidate the config.
- R9. A path before the first section is a warning, not a hard failure. It is shown as `NOT REACHED`; the valid part of the config still counts and remains editable.
- R10. `*!Section` remains a hard halt for reachability. Lines after it are `NOT REACHED` and are never edited.
- R11. The first session creates `config.txt.original` from the active config and never overwrites it. Every session writes `config.txt.session` from the session-start bytes, overwriting the previous session snapshot. No `.bak`, `.1.bak`, or `.2.bak` is created.
- R11a. `ur0:tai/boot_config.txt` is never opened, read, copied, renamed, or written. Boot modules are locked by the three known names only (`taihen.skprx`, `henkaku.skprx`, `yamt.skprx`); the app does not inspect the boot file to identify any other module.
- R12. A failed backup sets a visible `BACKUP WARNING` but never makes the config read-only and never blocks a toggle. Only an unreadable, too-large, too-long, or too-many-rows config is read-only, and the reason is named.
- R13. Before replacing the config, the adapter writes a sibling, verifies it, renames the original away, renames the sibling into place, reads the live file back, and compares it. If any step fails, the original is restored and the row is unchanged.
- R14. A config changed on disk since the last known copy is refused with `CONFIG CHANGED ON DISK. RESTART COCKPIT.` The file is not written.
- R15. A missing plugin file can be disabled if it has an active line. A missing file cannot be enabled. A missing inactive row is visible and its control is greyed.
- R16. `henkaku.suprx` cannot be disabled; if any of its lines is commented it can be enabled. Boot modules are locked. Warned plugins still confirm in both directions; KERNEL enables still confirm.

**PLUGINS page**

- R17. Every path named in the active config is one row, active or commented, grouped by the most privileged section. Each row shows its display name, description or `N/A`, one status badge, and one action control.
- R18. Status badges are `ACTIVATED`, `DISABLED`, `MISSING`, `PARTIAL`, `NOT REACHED`, and `BOOT`. `PENDING` is a second badge, not a status.
- R19. The action control reads `ENABLE` or `DISABLE` when the action is possible, and `LOCKED`, `MISSING`, `BOOT`, or `--` when it is not. A greyed control never performs an action; pressing Cross on it opens a short reason instead.
- R20. Every row is selectable, including rows with a greyed control. The selection cursor follows the drawn order.
- R21. Files present in `ur0:tai` or `ux0:tai` but not named by the active config are listed under `INSTALLED - NOT IN CONFIG`, read-only, sorted, with `UNLISTED` and `SHADOWED` tags where applicable. A missing `ux0:tai` directory is an empty listing, not an error. More than the cap is stated, not hidden. The caption says "not listed in config", not "not loaded", because the app does not inspect `boot_config.txt`.
- R22. The page header names the active config, the ignored config when both exist, the row count, the activated count, the pending count, and the backup state. A read-only page names the reason.

### Non-goals

- Adding an unlisted file to the config.
- Editing `boot_config.txt` or any Enso boot file.
- Restoring a backup from inside COCKPIT.
- Persisting pending state across a relaunch.
- A third tab or a batch apply.

## Key Decisions

- **D61. One original and one session backup.** `config.txt.original` is created once and never rewritten. `config.txt.session` is rewritten from the session-start bytes on every launch. A backup failure is a warning, not a lock. The old rotation is removed. This is what the user asked for and it removes the read-only failure mode.
- **D61 (boot scope). `boot_config.txt` is outside the app.** It is never opened, read, copied, renamed, or written. The only boot knowledge in the app is the three well-known module names, locked by name. This supersedes the old boot-path parsing and boot-file backup.
- **D62. The toggle is a two-state row action.** A row is either fully on or fully off. `ENABLE` completes a partial row; `DISABLE` comments every reached line. The old three-state `RESTORE` cycle is removed because it made the control unpredictable. The session backup is the restore path.
- **D63. The summary is permanent chrome.** `N PLUGINS ONLINE` lives at label scale on its own header line, and the hazard band moves to the header's bottom 8 px. A fault cannot hide the count.
- **D64. Read first, stat second.** The adapter opens and reads the file to EOF; `sceIoGetstat` only distinguishes absent from unreadable when open fails. A short read is an error, never a complete config. `sceIoRename` is used only to move a file to an absent name; replacement is a rename-away swap, followed by a live read-back.
- **D65. A BOM and a stray line are data, not corruption.** The parser preserves the BOM, skips it for the first header, and treats a path before a section as an unreachable warning. Neither makes the file read-only.
- **D66. The row owns its control.** The panel draws a status badge and an action control from `editor_action_text()` and `editor_action_reason()`. The app acts only when `editor_next_action()` returns an action. This keeps the control and the policy in the same module.

## Implementation Units

### U1. Adapter: open-first read, verified swap, two backups

- **Files:** `src/core/platform.h`, `src/platform/platform_psp2.c`, `tools/fake/fake_platform.c`, `tools/fake/fake_platform.h`.
- **Work:**
  - `platform_read_config`: open first, read until EOF or the ceiling, reject a short read, reject a directory, use stat only to classify an open failure.
  - `platform_write_config`: write `.new` with `O_EXCL`, verify it, remove a stale `.old`, rename the original to `.old`, rename `.new` to the path, read the live file back, restore `.old` on any failure, remove `.old` on success.
  - `platform_file_exists`: return false for a directory.
  - Replace `platform_backup_rotate` with `platform_backup_config(path, text, len, &original_ok, &session_ok)`: write `.original` only when absent, always write `.session`, both through `platform_write_config`.
  - `platform_list_dir`: a missing directory returns 0; a real read error returns -1.
- **Tests:** short read refused; write failure leaves the original byte-identical and leaves no `.new`/`.old`; rename failure restores the original; original is never overwritten; session is refreshed; missing `ux0:tai` is an empty listing; `boot_config.txt` is byte-identical and has no `.bak` after a scan and a toggle.

### U2. Config model: BOM, warnings, exact `#` edit, two-state action

- **Files:** `src/core/tai_config.h`, `src/core/plugin_editor.h`, `src/core/plugin_editor.c`, `src/core/plugins.c`, `src/core/plugins.h`.
- **Work:**
  - Add BOM-aware content start to both parsers.
  - Parse a leading run of `#` and spaces as the comment prefix; store the first `#`, the count, and the path start.
  - Reject a path containing spaces or control characters so a prose comment ending in `.skprx` is not a row.
  - Make the plugin suffix check case-insensitive.
  - `invalid` becomes a warning; `read_only` is set only by `!source_ok`, `too_large`, `lines_truncated`, or `rows_truncated`. Add `rows_truncated` and raise `EDITOR_ROWS_MAX` to `EDITOR_LINES_MAX` so the row cap and line cap cannot disagree.
  - `editor_scan`: resolve first, read once, write the two backups from the launch bytes, then parse. A backup failure sets `backup_warning` and nothing else.
  - `editor_next_action`: `DISABLE` when every reached line is active; `ENABLE` otherwise. Remove mode 2.
  - `editor_toggle`: remove the `.bak` precondition; write the exact `#` edit; verify through the adapter; update `last_text` only after success.
  - Add `editor_action_text`, `editor_action_reason`, and `editor_readonly_reason`.
  - `EDITOR_ORPHANS_MAX` to 32, add `orphan_truncated`, sort the list, exclude `.original` and `.session`.
  - Remove every boot-config read, parse, backup, and path set. Lock only the three known boot module names.
  - Rename `EDITOR_ENABLED` to `EDITOR_ACTIVATED` and display `ACTIVATED`.
- **Tests:** BOM config counts and edits; stray path warns but edits; CRLF/indent/no-final-newline round trip; double `#` enable removes the comment run; missing active/inactive controls; locked/boot/not-reached; partial enable; write failure; changed-on-disk; >512 lines read-only; >row cap impossible because the caps match; orphan sort/exclusion/overflow.

### U3. HOME summary

- **Files:** `src/ui/header.c`, `src/ui/tabbar.c`, `src/gfx/layout.h` only if a constant is shared.
- **Work:**
  - Move `HAZARD_Y` to 56, `HAZARD_H` 8.
  - Draw the HOME summary at y 42 (label) with a micro detail run; the band is below it.
  - Build `N PLUGINS ONLINE` plus the non-empty group counts and pending. Fit by dropping group counts first, then the detail run; keep the total and pending token.
  - Name `NO CONFIG.TXT`, `CONFIG TOO LARGE`, `CONFIG UNREADABLE`, and `CONFIG WARNING` explicitly.
  - On PLUGINS, show the source, rows, activated, pending, and backup state.
- **Tests:** host preview; summary fit at one, five, and zero groups; alert scene shows both the summary and the band.

### U4. PLUGINS page: badges, controls, empty state, orphans

- **Files:** `src/panels/plugins.h`, `src/panels/plugins.c`, `src/app/app.c`, `src/ui/modal.c`, `src/ui/modal.h`, `src/ui/ui_state.h`.
- **Work:**
  - Row: status dot, name, `ENABLE`/`DISABLE` control on the first line; description and status badge on the second.
  - Badge fill is the status color at low alpha with the label in the status color; `PENDING` gets a second yellow badge.
  - Control: bright fill for a possible action, `C_HAIR` fill and `C_DIM` text for a greyed one. Pressing Cross on a greyed row opens `CANNOT CHANGE` with `editor_action_reason`.
  - Selection moves through every row; initial selection is row 0 if any.
  - Empty state, no-config state, read-only reason, backup warning, and invalid warning.
  - Orphan section renamed `INSTALLED - NOT IN CONFIG`, sorted, tagged, overflow notice.
- **Tests:** preview scenes; app selection logic extracted or covered through the editor action API.

### U5. Preview scenes and draw-call budget

- **Files:** `tools/preview/preview.c`, `tools/preview/run.sh` only if sources change.
- **Scenes (13):** healthy HOME, alert HOME, PLUGINS, PLUGINS scrolled to the unlisted section, config warning with a stray path, missing active/inactive, backup warning (still editable), five groups, duplicate names, warned confirm, write failure, too-long read-only, HOME pending.
- **Gate:** every scene under 6,500 draw calls; every PNG non-empty. The new row chrome adds a badge and a control per row, so the busiest scene (the confirmation over the ledger) is 5,793 calls.

### U6. Documentation and tracker

- `docs/plans/` new plan (this file), old plan annotated as partly superseded.
- `DECISIONS.md` D61-D66; mark D57 and D60 superseded; update D55.
- `DESIGN.md`, `ARCHITECTURE.md`, `PROJECT.md`, `vita-cockpit/README.md`, root `README.md`.
- `PROJECTS.md`, `agent-wiki/12-projects.md`, `agent-wiki/13-fact-check.md`.
- Update test counts, preview count, build number, and the backup instructions.

## Test Matrix

| Area | Cases |
|---|---|
| Read | absent, empty, normal, exactly at the ceiling, over the ceiling, short read, directory, stat failure with a successful open |
| Write | success, write failure, verify failure, rename-away failure, restore failure, stale `.new`, stale `.old`, zero-byte file |
| Backups | first session creates original, second session keeps original, session refreshes, backup failure warns but edits, `.original`/`.session` excluded from orphans, `boot_config.txt` unchanged with no backup |
| Parse | LF, CRLF, no final newline, BOM, indentation, tabs, double `#`, path before section, `*!` halt, repeated path in two sections, case-insensitive suffix, prose comment ending in `.skprx` |
| Rows | activated, disabled, missing active, missing inactive, partial, boot, not reached, locked, pending, duplicate basenames |
| Actions | enable, disable, partial enable, missing disable, missing enable refused, locked refused, boot refused, not-reached refused, read-only refused, changed-on-disk refused |
| Orphans | unlisted file, shadowed file, sorted, overflow, missing directory |
| Summary | 0, 1, many, groups, pending, no config, too large, unreadable, warning |
| Input | edge, repeat, modal swallow, opening press cannot accept |

## Risks and Mitigations

| Risk | Mitigation |
|---|---|
| The console refuses `sceIoRename` even when the target is absent | The write fails without touching the original; the session backup remains; the error is explicit. The console check will confirm. |
| The rename-away swap loses power between the two renames | `config.txt.original` and `config.txt.session` exist, and `.old` holds the previous file; document the manual restore. |
| A BOM is not the user's cause | The fix removes a whole class of false read-only states and is harmless otherwise. The page will name whatever remains. |
| A backup write fills `ur0:` | Two small snapshots replace three; a failure is a warning, not a lock. |
| Badge/control draw calls exceed the budget | The row uses one badge fill, one control fill, and text; the preview enforces 6,500. The busiest scene is 5,793. |
| The summary still does not fit beside the clock | It is on its own header line at y 42, below the clock and tabs, and the fit loop drops group counts first. |
| A double-commented line is normalized on enable | The toggle removes the whole comment run; this is the only unambiguous uncomment. Documented. |

## Definition of Done

- `tools/test/run.sh` passes with the new cases.
- `tools/preview/run.sh` renders every scene under the draw-call budget.
- `./build.sh` produces one uniquely named VPK with no warnings.
- The summary is visible in the alert preview.
- PLUGINS shows badges and controls for activated, disabled, missing, partial, boot, and not-reached rows.
- The backup model is `config.txt.original` + `config.txt.session` only.
- Docs, `PROJECTS.md`, and the wiki reflect the change.
- The branch is committed and pushed.

## Verification Commands

```sh
cd vita-cockpit
./tools/test/run.sh
./tools/preview/run.sh
cd ..
export VITASDK="$PWD/vitasdk"
export PATH="$VITASDK/bin:$PATH"
(cd vita-cockpit && ./build.sh)
```

The console check after the build:

1. HOME shows `N PLUGINS ONLINE` and the group counts; force an alert (low battery) and confirm the summary is still visible above the band.
2. PLUGINS shows `ACTIVATED`, `DISABLED`, `MISSING`, `PARTIAL`, `BOOT`, and `NOT REACHED` rows with the control in the expected state.
3. Toggle one enabled plugin off and one disabled plugin on. Confirm exactly one `#` moved, the status badge changes to `PENDING`, and no `READ ONLY` appears.
4. Confirm `config.txt.original` exists and is unchanged, `config.txt.session` is this session's file, and no `.bak`/`.1.bak`/`.2.bak` were created.
5. Reboot and confirm the toggled plugins follow the file.

## Result

- Host tests: 110 metrics/plugin checks + 206 editor/backup/input checks = 316,
  0 failures. Address/undefined sanitizers clean.
- Host preview: 13 scenes, busiest 5,793 draw calls (confirmation over the
  ledger), all under the 6,500 budget.
- Build: `cockpit-v01.00-b35.vpk`, 112.1 KB, 0 warnings.
- `boot_config.txt` is byte-identical in the host tests and has no `.bak`,
  `.original`, or `.session` beside it.
- Hardware check remains pending; the probe list is in the Definition of Done.
