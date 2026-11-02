# 08 — Firmware and Jailbreak

The build pipeline is firmware-independent, but **running** homebrew depends on
the exploit and CFW.

**Scope:** this chapter is background for an already-jailbroken **3.65 Enso**
console. The build path in this wiki starts there. Do not perform a firmware
downgrade or storage migration unless you intend to; a failed flash is the
wiki's own documented route to a permanent brick.

## Firmware map

| Firmware | Exploit | Persistent CFW | Development note |
|---|---|---|---|
| 3.60 | HENkaku (browser) | Enso 3.60 | Oldest; some patches target only 3.60 |
| 3.61 / 3.63 | none public | n/a | Update to 3.74 then HENlo, or use a hardware method; modoru needs an existing exploit |
| 3.65 | h-encore / HENlo | **Enso 3.65** | **Recommended modern base** |
| 3.67 / 3.68 | h-encore / HENlo | no Enso | Downgrade to 3.65 for Enso |
| 3.69–3.73 | h-encore² | no Enso | Downgrade |
| 3.74 | HENlo / h-encore² | no Enso | Downgrade to 3.65 if factory firmware permits |

3.74 is the final Vita firmware (May 10, 2022).

## The user's setup: 3.65 Enso via VitaDeploy

This is the best-case development setup:

- Permanent CFW; no exploit to reapply after reboot.
- taiHEN loads at boot.
- VitaShell and VitaDeploy available.
- Normal userland apps and kernel plugins can run.
- Kernel plugins can be tested on demand and cleared by reboot.

### Check

1. Settings → System Information → `3.65`.
2. Reboot; if homebrew still launches, Enso is active.
3. Settings → HENkaku Settings → **Enable Unsafe Homebrew**.
4. VitaShell installed; FTP works.
5. `ur0:tai/config.txt` exists (modern path); `ux0:tai/config.txt` should not
   shadow it.

## Enso FAQ

- **Deactivate Enso** without uninstalling: delete `ur0:tai/boot_config.txt`.
- **Reactivate**: run the Enso installer, use Square "fix boot config".
- **Fully uninstall**: Triangle in the installer.
- **Skip plugins at boot**: hold **L**.
- **Bad plugin bootloop**: hold L, edit/remove the plugin line.
- **Safe mode**: hold **R + Power + PS**.
- **Restore system**: resets settings; can regenerate boot_config.

Do not edit `boot_config.txt` by hand. It is model-specific.

## HENkaku

- 3.60 only.
- Installed from the Vita browser at `https://henkaku.xyz`.
- Must be reapplied after every reboot unless Enso is installed.
- Loads taiHEN and enables homebrew.
- HENkaku Settings includes "Enable Unsafe Homebrew" and version spoofing.

## h-encore / h-encore²

- **h-encore**: 3.65, 3.67, 3.68.
- **h-encore²**: 3.65–3.74.
- PC-side setup with `finalhe`/qcma/psvimgtools; the exact steps are in the
  official README.
- Hold **R** while launching to force the bootstrap menu.
- If HENkaku Settings is missing, use "Reset taiHEN config.txt" in the bootstrap
  menu.

## HENlo

- Browser exploit at `http://jailbreak.psp2.dev`.
- Works on 3.65, 3.68, 3.74.
- Installs HENkaku + VitaDeploy.
- Not persistent; Enso is installed afterward.

## modoru (downgrade)

Requirements and gotchas:

- Device already running HENkaku/h-encore on 3.60–3.74.
- Battery ≥ 50%.
- **Disable all plugins**; rename `ux0:tai` and `ur0:tai` temporarily.
- Cannot launch from SD2Vita; use a memory card or internal storage.
- Put the full `PSP2UPDAT.PUP` at
  `ux0:app/MODORU000/PSP2UPDAT.PUP`.
- Don't reinstall the VPK afterward or the PUP is removed.
- You can only downgrade to your **factory firmware** or above. Some devices
  shipped above 3.65 and cannot go down to 3.60/3.65.

Firmware downloads are not linked here; obtain the PUP from your own
region/console source.

## SD2Vita and YAMT

- YAMT is the modern mount tool; requires Enso; supports 3.60/3.65.
- Settings → Devices → Storage Devices → enable **Use YAMT**.
- Default: SD2Vita as `ux0:`, Sony card as `uma0:`.
- Config: `ur0:tai/yamt.cfg`; delete it and reboot if a mount misbehaves.
- Do not run YAMT and StorageMgr together.
- Keep VitaDeploy on the official memory card for recovery. If the SD2Vita dies
  or its mount config is wrong, you still have a file manager/installer.

### StorageMgr

- `ur0:tai/storagemgr.skprx` + `ur0:tai/storage_config.txt`.
- `MCD=uma0`, `INT=imc0`, `GCD=ux0` (SD2Vita) or `UMA=ux0` (USB).
- Delete `ux0:tai` after migrating, or the stale config shadows `ur0:tai`.

## Plugin config migration

1. Copy plugins and `config.txt` to `ur0:tai`.
2. Update paths from `ux0:tai/...` to `ur0:tai/...`.
3. Delete `ux0:tai` entirely.
4. Keep load order; `0syscall6` before storage drivers (community report).
5. User plugins: Refresh taiHEN config. Kernel plugins: reboot.

## PSN and ban risk

- The Vita Hacks Guide FAQ says Sony does not ban consoles/accounts unless you
  hack trophies or in-game.
- Community consensus: ordinary homebrew/plugin use has not produced a ban wave;
  trophy cheating, online cheating, and pre-release content are the cited risks.
- PSN ToS prohibits modifying the system, so a ban is always theoretically
  possible. Use a secondary account if that matters.
- Version spoofing in HENkaku Settings lets lower-firmware consoles access PSN;
  it is not a guarantee of safety.

## Legal boundaries

- No Sony keys.
- No copyrighted firmware (`PSP2UPDAT.PUP`, `os0`/`vs0` dumps, system modules).
- No game backups/ROMs or piracy tools.
- Do not redistribute `libshacccg.suprx`; users extract it from their own
  firmware.
- Do not use the leaked official Sony SDK; VitaSDK is the clean-room community
  toolchain.
