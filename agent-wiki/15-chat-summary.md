# 15 — Chat Summary

This page summarizes the investigation and work that produced this wiki.

## The request

The user has a jailbroken PS Vita on **3.65 permanent CFW (Enso)**, installed
with **VitaDeploy**. They want to build homebrew apps — not full 3D games — with
file I/O, audio, and WiFi, and they want the app to reach the **kernel plugin
layer** (layer 3), not boot-level CFW modification (layer 4).

## What was investigated

1. **How Vita homebrew is created**
   - VitaSDK toolchain, GCC/newlib, ARMv7-A/Cortex-A9/NEON/hard-float.
   - ELF → VELF → SELF/FSELF → VPK pipeline.
   - Sony NID-based linking and stub libraries.
   - `vita-elf-create`, `vita-make-fself`, `vita-mksfoex`, `vita-pack-vpk`.

2. **Packaging and install**
   - VPK is a ZIP with `eboot.bin` and `sce_sys/param.sfo`.
   - `param.sfo` fields and defaults.
   - LiveArea assets and `template.xml`.
   - Safe vs unsafe auth IDs.
   - VitaShell extracts to `ux0:data/pkg`, creates `head.bin`, and promotes via
     `ScePromoterUtil` to `ux0:app/<TITLEID>`.

3. **Runtime APIs**
   - File I/O: `sceIoOpen/Read/Write/Close/Mkdir`, `SceIofilemgr`.
   - Audio: `sceAudioOutOpenPort/Output/ReleasePort`, `SceAudio`.
   - WiFi: `SceNet`, `SceNetCtl`, BSD sockets, `gethostbyname`, HTTP.
   - UI: `vita2d`, `imgui-vita2d`, SDL2.
   - Libraries: curl+mbedTLS, libarchive, libwebp, mpg123, vorbis, opus, FLAC.

4. **Kernel plugins**
   - taiHEN, `.skprx` vs `.suprx`.
   - `config.txt` paths/precedence, boot vs on-demand load.
   - Hooks, injections, custom syscall exports, generated stubs.
   - Module unload limits, NID/firmware differences, safety.

5. **Firmware**
   - HENkaku 3.60, Enso, h-encore, h-encore², HENlo, modoru.
   - 3.65 Enso via VitaDeploy as the recommended development base.

## What was installed

- VitaSDK **2026.08** at `vitasdk`.
- `vdpm` packages:
  - `libvita2d`, `imgui-vita2d`
  - `sdl2`, `sdl2_image`, `sdl2_mixer`, `sdl2_ttf`, `sdl2_gfx`
  - `mpg123`, `libvorbis`, `libogg`, `opus`, `opusfile`, `flac`
  - `curl-mbedtls`, `mbedtls`, `zstd`
  - `libarchive`, `libwebp`
  - `libconfig`, `jsoncpp`, `taihen`

## What was built and verified

| Project | Result |
|---|---|
| `vita-cap-test` | 2.6 MB unsafe VPK; imports for `SceAudio`, `SceIofilemgr`, `SceNet`, `SceNetCtl` |
| `vita-kernel-demo` | 74 KB VPK + 4.8 KB `.skprx`; app imports `KernelDemo` syscalls; plugin imports taiHEN + `SceSysmemForDriver` |

The capability test exercises file I/O, PCM audio, WiFi IP, DNS, HTTP, and
library versions. The kernel demo loads a `.skprx` on demand, hooks
`SceIofilemgr::_sceIoOpen`, and reads kernel state via a generated syscall stub.

## Key findings

- **VPK is a ZIP**, not a Sony package format.
- **`vita_create_self` defaults to safe**; `UNSAFE` is opt-in.
- **The raw `vita-make-fself` defaults to unsafe**; CMake adds `-s`.
- **Safe authid** `0x2F00000000000002`; unsafe `...0001`; secret-safe `...0003`.
- **VitaShell promotes packages** via `ScePromoterUtil`; it does not just unzip.
- **Title ID** must be 9 uppercase characters; `XXXXYYYYY` recommended.
- **Version** must be `##.##`.
- **LiveArea PNGs** should be indexed/palette; non-indexed can fail `0x8010113D`.
- **`__sce_headroom=0x10000`** fixes `vita-elf-create` segment overlap.
- **Kernel plugin config** is `ur0:tai/config.txt`; a stale `ux0:tai` shadows it.
- **Kernel hooks** must check returns, call `TAI_CONTINUE`, copy user pointers,
  avoid recursion, and release in `module_stop`.
- **3.65 Enso** is the recommended development base; 3.60 is similar but some
  NIDs/offsets differ.

## Current state

- Toolchain installed and working.
- Three projects build successfully.
- Host tools were used to inspect VPK/SFO/authid/imports.
- **Not tested on physical Vita hardware** in this environment. The user can
  install the VPKs with VitaShell FTP and run them.
- The kernel demo is safe-by-design (read/count only) and loads on demand, so a
  reboot clears it.

## Next steps

1. Run `vita-cap-test.vpk` on the Vita; confirm I/O, audio, WiFi.
2. Run `vita-kernel-demo.vpk`; confirm the plugin loads and the open count grows.
3. If the kernel hook fails on 3.65, check the NID/import table and firmware
   differences.
4. Extend the kernel demo with a second hook, a reset export, or a ring buffer.
5. For a real app, start from `vita-cockpit`, add the specific APIs from
   `04-userland-apps.md`, and use `./build.sh --safe` unless a private API is
   required.

## Caveats

- Kernel plugins can brick; use hold-L/Safe Mode recovery.
- Do not edit `ur0:tai/boot_config.txt`.
- Do not add untested plugins to `*KERNEL`.
- The community field notes include [C]/[L] claims that are not all verified.
- See `13-fact-check.md` for what was actually verified.
