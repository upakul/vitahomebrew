# PS Vita Homebrew & taiHEN: Field Notes for an AI Agent

*Research snapshot: November 2026. Written for an agent that already knows the official VitaSDK API reference and needs the undocumented, version-specific, community-level knowledge.*

---

## 0. What is verified vs. forum lore

I used three confidence levels below:

| Tag | Meaning |
|---|---|
| **[V]** | Verified against a primary source: VitaSDK/taiHEN source, official Vita Hacks Guide, or a project's own README/source. |
| **[C]** | Community-reported and widely reproduced, but not stated in primary docs. Treat as a strong hypothesis, not a guarantee. |
| **[L]** | Lore, outdated advice, or a claim I found contradicted by primary sources. See §7. |

**Primary sources that were accessible:** VitaSDK.org, `vitasdk/vita-toolchain` source (`vita.cmake`, `vita-make-fself.c`, `vita-pack-vpk.c`), `vitasdk/vita-headers` (including `db/360` NID tables), `tai.henkaku.xyz` docs, `yifanlu/taiHEN` source and `USAGE.md`, Vita Hacks Guide, `enso.henkaku.xyz` FAQ, and the READMEs/source of VitaDeploy, modoru, YAMT, PSVshell, 0syscall6, enso_ex, psp2hfw, KVDB, vdbtcp, DebugNet, Cat-A-Log, vitacompanion, and vita-parse-core.

**Sources that were not directly accessible:** `psdevwiki.com` / `playstationdev.wiki` (Cloudflare challenge), `wiki.henkaku.xyz` (Anubis challenge), Discord servers, and some Reddit threads (only search-result snippets). Where I rely on those, I say so and mark **[C]**.

**A warning about 2025–2026 "Vita dev wikis":** several GitHub "PSVita-Dev-Wiki" repos and blog posts are LLM-generated compilations. They contain confidently wrong claims that I could disprove from SDK source — for example, that `UNSAFE` is required for a SELF to run at all, that a title ID must be "4 uppercase letters + 5 digits", and that LiveArea background must be named `bg0.png`. Do not treat those as authoritative. When a claim about the SDK can be checked in `vita.cmake`, `vita-make-fself.c`, or the headers, check it.

---

## 1. Building homebrew apps with VitaSDK

### 1.1 Toolchain setup that actually works

VitaSDK is installed with `vdpm`, and packages are installed into the SDK:

```sh
export VITASDK=/usr/local/vitasdk
export PATH="$VITASDK/bin:$PATH"

vdpm install zlib sdl2 libvita2d taihen
```

The current supported series as of late 2026 is `2026.08`; there is also a `nightly` channel. The Docker image is `vitasdk/vitasdk:2026.08`. [V] ([vitasdk.org](https://vitasdk.org/))

The single most common "nothing builds" cause is not passing the toolchain file, or passing it after `project()`. The toolchain file must be set **before** `project()`:

```cmake
cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake"
        CACHE PATH "Vita toolchain")
  else()
    message(FATAL_ERROR "Set VITASDK to your VitaSDK root")
  endif()
endif()

project(hello_vita C)
include("${VITASDK}/share/vita.cmake" REQUIRED)
```

The toolchain already adds `-Wl,-q` (required to preserve relocation tables) and `-Wl,-z,nocopyreloc`. **Do not add `-Wl,-q` yourself**; duplicate flags are harmless but unnecessary, and forgetting it when building outside CMake produces `vita-elf-create` failures. [V] (`vita.toolchain.cmake`)

For large ports, the RetroArch `Makefile.vita` is a useful reference for flags:

```make
ARCHFLAGS := -march=armv7-a -mfpu=neon -mfloat-abi=hard -DVITA
CFLAGS    += $(ARCHFLAGS) -mword-relocations -fno-optimize-sibling-calls
LDFLAGS   := -Wl,-q
```

`-mword-relocations` and `-fno-optimize-sibling-calls` are not needed for hello-world, but are common for big C/C++ codebases. [C] ([RetroArch Makefile.vita](https://sources.debian.org/src/retroarch/1.20.0+dfsg-2/Makefile.vita/))

### 1.2 A complete app `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake"
        CACHE PATH "Vita toolchain")
  else()
    message(FATAL_ERROR "Set VITASDK to your VitaSDK root")
  endif()
endif()

project(hello_vita C)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "Hello Vita")
set(VITA_TITLEID  "HOMB00001")   # exactly 9 chars, unique to you
set(VITA_VERSION  "01.00")       # must match ##.## exactly

set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -s VERSION=${VITA_VERSION}")
set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -s CONTENT_ID=HB0001-${VITA_TITLEID}_00-0000000000000000")

# Optional: request the enlarged-memory profile (+109 MiB, see §1.4)
set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d ATTRIBUTE2=12")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2 -g")

add_executable(${PROJECT_NAME} src/main.c)
target_link_libraries(${PROJECT_NAME}
  SceDisplay_stub
  SceCtrl_stub
  SceLibKernel_stub
)

vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME}
  # UNSAFE        # uncomment only if you need private/system APIs; see §1.3
  STRIPPED
)

vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
  VERSION ${VITA_VERSION}
  NAME    ${VITA_APP_NAME}
  FILE    sce_sys sce_sys
)
```

Notes:

- `STRIPPED` strips the **output SELF**, not necessarily the `.elf` you keep for crash dumps. Build with `-g` and keep the unstripped ELF; see §4.4.
- `FILE sce_sys sce_sys` recursively copies the whole `sce_sys` directory. `vita-pack-vpk` supports directory recursion. [V] (`vita-pack-vpk.c`)
- Do **not** put a `param.sfo` inside your source `sce_sys/` if you copy the whole directory: `vita_create_vpk` already generates and adds `sce_sys/param.sfo`, and a second file with the same archive path can produce a duplicate entry or a confusing install. Keep `param.sfo` out of the source tree. [V] (`vita-pack-vpk.c` adds the SFO first, then `-a` entries)

### 1.3 Safe vs. unsafe eboots: the distinction people get wrong

`vita_create_self` ultimately calls `vita-make-fself`. The relevant flags are:

| Build | Flag | AuthID written | What it means |
|---|---|---|---|
| **safe** (default in CMake) | `-s` | `0x2F00000000000002` | Restricted API and filesystem access; runs without "Enable Unsafe Homebrew". |
| **unsafe** | `UNSAFE` in CMake | `0x2F00000000000001` | Full/private/system API access; user must enable unsafe homebrew. |
| **secret-safe** | `-ss` via `VITA_MAKE_FSELF_FLAGS` (manual) | `0x2F00000000000003` | Special; the installed 2026.08 CMake helper has no `SECRETSAFE` option. The tool says "Do not use this option if you don't know what it does." |
| custom authid | `-a 0x...` | caller-supplied | e.g. SceShell's `0x2800000000000001`; advanced/risky. |

These values are directly from `vita-make-fself.c`: if `safe` is 2, `authid = 0x2F00000000000000 | 2`; if `safe` is 3, `| 3`; otherwise `0x2F00000000000001`. [V]

**The critical correction:** omitting `UNSAFE` does **not** make the eboot unsigned/unrunnable. It makes a **safe** eboot, which is the normal, runnable default. A safe eboot is restricted:

- vsh functions are blocked (registry access, partition table access, etc.).
- Device access is limited to `ux0:data`, `app0:` (read-only app package), `savedata0:`, and a few other locations.
- `app0:` is read-only; persistent writable data goes to `ux0:data/<YourApp>/`.

[V] (Yifan Lu's "PSA for homebrew devs": [gist](https://gist.github.com/yifanlu/3b36b987c9099b620c5b65ce8e93801d))

So the rule is: **start safe; add `UNSAFE` only when a specific API forces it.** Shipping `UNSAFE` unnecessarily makes users enable a system-wide security relaxation and is bad etiquette.

"Enable Unsafe Homebrew" is in **Settings → HENkaku Settings**. It is required both for unsafe eboots and for `taiLoadKernelModule`/`taiLoadStartKernelModule` from user code. [V] ([Vita Hacks Guide](https://vita.hacks.guide/installing-henkaku), taiHEN docs)

### 1.4 Title IDs, version strings, and app memory

**Title ID:**

- Exactly **9 characters**; VitaSDK recommends `XXXXYYYYY` where `XXXX` is an author identifier and `YYYYY` is a per-app number. [V] (`vita.cmake`)
- It is not validated by `vita-mksfoex` or `vita_create_vpk`; the firmware/app database is what cares. [V] (source has no title-ID regex)
- Real examples include `MLCL00001` (molecularShell), `VSDK00007` (sample), and `VITASHELL` (all letters). So the common claim "must be 4 uppercase letters + 5 digits" is **false**. [V] + [L]
- Use uppercase A–Z/0–9, keep it unique, and avoid Sony's `NPXS*` system-app namespace. Changing the title ID creates a new bubble and a new `ux0:app/<TID>` directory; old app data does not follow automatically. [C]

**Version:** `VITA_VERSION` must match `^[0-9][0-9]\.[0-9][0-9]$`. `"2.0"`, `"2.00"`, and `"02.0"` all fail with:

```
CMake Error ... Invalid version '2.0' change to format ##.##
```

[V] (`vita.cmake` regex)

**App memory:**

- `MEMSIZE` is passed to `vita-make-fself -m` and is measured in **KiB**.
- `vita-make-fself` usage says: "Memory budget for the application in kilobytes. (Normal app: 0, System mode app: 0x1000 - 0x12800)". [V]
- Most homebrew should leave `MEMSIZE` unset (`0`, the normal app default).
- `ATTRIBUTE2=0x0C` (decimal 12) requests the enlarged-memory profile: **+109 MiB** according to psdevwiki's PARAM.SFO table. Community measurements put the default app budget around 256 MiB, so 0x0C lands around 365 MiB. [C] (psdevwiki PARAM.SFO, seen via search snippet; direct page blocked)
- `-pm` sets a physically contiguous budget, subtracted from the standard budget. [V]
- `NOASLR` disables ASLR for the process main module — useful for reproducible crash addresses, not for release. [V]

**Do not** set `MEMSIZE 0x10000` thinking "64 MiB"; that is outside the documented system-app range and is not how normal apps request memory. Use `ATTRIBUTE2` for the enlarged profile.

### 1.5 Build and link errors: exact messages and fixes

| Error | Cause | Fix |
|---|---|---|
| `CMake Error at CMakeLists.txt:NN (add_dependencies): Cannot add target-level dependencies to non-existent target "foo.skprx"` | On CMake ≥ 3.20, `vita_create_self` creates target `foo.skprx-self`, not `foo.skprx`. `vita_create_vpk` creates `foo.vpk-vpk`. [V] | Depend on `foo.skprx-self` / `foo.vpk-vpk`, or add the dependency to the `-vpk` target. |
| `CMake Error ... Invalid version '2.0' change to format ##.##` | `VITA_VERSION` regex. [V] | Use `02.00`. |
| `-- The C compiler identification is unknown` | `VITASDK` not set, `$VITASDK/bin` not on `PATH`, or toolchain file not found. | Export both; pass `-DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake`. |
| `vita-elf-create: Cannot allocate 4460 bytes for SCE data at end of segment 0; segment 1 overlaps` | The linker packed LOAD segments too tightly for the SCE metadata. [V] (vita-toolchain #303) | Add `target_link_options(app PRIVATE "-Wl,--defsym=__sce_headroom=0x10000")` (or a larger value). |
| `vita-elf-create: Invalid relocation type 25!` | PIC/PIE code generation. [V] (vita-toolchain #264) | `set(CMAKE_POSITION_INDEPENDENT_CODE OFF)`; don't build with `-fPIC`/`-pie`. |
| `vita-elf-create: dest = elf_begin(...) failed: Request error: invalid ELF_C__ argument` / segfault | `vita-elf-create` linked against an incompatible system `libelf` (only when building the toolchain from source). [V] (vita-toolchain #39) | Use the prebuilt VitaSDK; or build against libelf 0.8.13. |
| `Unable to find module with NID 1586302498 for function symbol sceKernelExitThread` | Linking a real library instead of a `*_stub`, or headers/stubs from mismatched SDK versions. [V] (vita-toolchain #61) | Link the `*_stub` libraries; don't mix SDK versions. |
| `cannot find -lreVita_stub` | Stub library not built yet or not in the linker search path. [V] (GBAtemp thread) | `link_directories(${CMAKE_CURRENT_BINARY_DIR})`; `add_dependencies(app reVita_stub)`; ensure `vita_create_stubs` runs first. |
| `undefined reference to 'pthread_mutex_lock'` in C++ | Known newlib/SDK regression in some 2020 builds. [V] (vita-toolchain #138) | Update VitaSDK; link `pthread`; avoid pinning an old SDK. |
| `fatal error: vita2d.h: No such file or directory` | `libvita2d` not installed. | `vdpm install libvita2d`. |
| `vita-pack-vpk: .sfo file missing` / `.bin file missing` | Wrong arguments to `vita_create_vpk` (or calling `vita-pack-vpk` directly without `-s`/`-b`). [V] | Use `vita_create_vpk(target titleid eboot ...)`; don't hand-call the packer. |
| `Error: cannot stat '...': No such file or directory` | Bad `FILE src dst` path. [V] (`vita-pack-vpk.c`) | Fix the source path; remember `FILE` takes **two** arguments. |
| `No rule to make target 'hello.vpk'` | Target is `hello.vpk-vpk` on CMake ≥ 3.20. [V] | `cmake --build build --target hello.vpk-vpk`, or just `cmake --build build`. |
| `error: unknown type name 'SceUID'` | Missing include. | `#include <psp2/types.h>` or the umbrella `<vitasdk.h>` / `<vitasdkkern.h>`. |

Two more build-system facts worth knowing:

- `vita_create_vpk`'s `FILE` can copy a directory recursively, so `FILE sce_sys sce_sys` is valid. [V]
- `vita_create_stubs` has a documented **1024-entry limit per library** in CMake mode; `vita-libs-gen-2` removes it. [V] (vita-toolchain README)

### 1.6 VPK packaging and LiveArea assets

A VPK is a ZIP with `eboot.bin` and `sce_sys/param.sfo` at the archive root. `vita-pack-vpk` adds those first, then any `-a src=dst` entries. [V] (`vita-pack-vpk.c`)

**LiveArea layout:**

```
sce_sys/
├── icon0.png                         128×128
├── pic0.png                          960×544   (loading image; commonly required)
└── livearea/
    └── contents/
        ├── bg.png or bg0.png         840×500
        ├── startup.png               280×158
        └── template.xml
```

**Image rules — the precise version:**

- The Vita's installer validates PNGs. Non-indexed PNGs (color type 2/6, grayscale type 0) are the usual cause of install failure `0x8010113D` at 99%. Convert to an **indexed/palette PNG (color type 3)**, e.g. with `pngquant`. [V] (hammerill/livearea-specs; GBAtemp/PSX-Place reports)
- **8-bit is the safe default, but it is not literally "must be 8-bit".** The official `hello_world` sample ships an 840×500 **1-bit colormap** `bg.png` and installs fine. The key property is color type 3 (indexed), not bit depth 8. [V] (I inspected `vitasdk/samples/hello_world/sce_sys/livearea/contents/bg.png`: `depth 1, colortype 3`)
- `icon0.png` should be opaque; it is displayed in a circle, and alpha is not wanted. [C]
- `startup.png` may have alpha. [C]
- `pic0.png` is widely reported as mandatory; the hammerill spec lists it but does not call it mandatory. Treat "include pic0.png" as strong advice, not a documented hard requirement. [C]
- **`bg0.png` is not mandatory.** `template.xml` names the background file; the official samples use `bg.png` and work. `bg0.png` is just a convention. The claim "must be bg0, not bg" is **[L]**. [V] (samples + `template.xml`)
- **Do not use `FILE sce_sys/icon0.png ...` with a stale `bg.png` left in the directory** if you copy the whole tree; you will ship dead assets. Clean the directory first. [C]

**Minimal `template.xml`:**

```xml
<?xml version="1.0" encoding="utf-8"?>
<livearea style="a1" format-ver="01.00" content-rev="1">
  <livearea-background>
    <image>bg.png</image>
  </livearea-background>
  <gate>
    <startup-image>startup.png</startup-image>
  </gate>
</livearea>
```

`style="a1"` centers `startup.png`; `style="psmobile"` places it to the right and supports `frame` elements with text/images. [V] (hammerill spec, samples, psdevwiki Template.xml)

**Install error `0x8010113D`:** VPK install reaches ~99%, then fails. Root cause is asset validation, almost always a non-indexed PNG. Fix with `pngquant` (or ffmpeg palettegen/paletteuse for `pic0.png`). [V] (hammerill/livearea-specs; [GBAtemp](https://gbatemp.net/threads/error-0x8010113d-during-vpk-installation.12842); [renpy-vita #12](https://github.com/SonicMastr/renpy-vita/issues/12))

### 1.7 VPK install quirks and error codes

| Symptom / code | Meaning | Fix |
|---|---|---|
| `0x8010113D` at 99% | Asset validation failed; usually a non-indexed PNG. [V] | Re-encode all `sce_sys` PNGs as indexed/palette. |
| `0x80010005` | I/O error during install (storage/card/config). [C] | Check free space, reseat SD2Vita, remove stale `ux0:tai`, reboot. |
| `0xFFFFFFFF` | Generic VitaShell/install failure; often a corrupt/renamed ZIP or wrong archive structure. [C] | Unzip and inspect: `eboot.bin` and `sce_sys/param.sfo` must be at root. Try installing from a folder instead. |
| `C2-12828-1` | The app crashed at launch; a core dump may have been written. [V] | See §4.4; common causes: missing `libshacccg.suprx`, bad eboot, unsafe homebrew disabled, missing dependency. |
| `C1-6775-5` | HENkaku/taiHEN is not active (launching homebrew without running the exploit). [V] | Re-run HENkaku/h-encore/HENlo, or reboot into Enso. |
| `C1-6703-6` | Game/app requires a higher firmware, or the compatibility plugin is not loaded. [C] | Load `0syscall6`/`reF00D` under `*KERNEL`; check plugin order. |
| `0x8002D011` | `SCE_KERNEL_ERROR_MODULEMGR_NO_MOD` — a required module is not loaded. [V] | Reboot; reset taiHEN config; reinstall the storage driver. Seen with YAMT mount failures. |
| `0x8002D002` | `SCE_KERNEL_ERROR_MODULEMGR_IN_USE` — cannot unload a module that exposes syscalls. [V] | See §2.6. |
| Install fails while app is running | The old process still holds the app directory. [C] | Close the app, then reinstall. |
| VPK > 4 GiB on FAT32 | FAT32 file-size limit. [C] | Use exFAT or split content. |
| Title ID already exists | The installer updates/replaces the existing app; stale `ux0:app/<TID>` can confuse promotion. [C] | Delete the old app/bubble, or use a new title ID. |

**Installing without a VPK:** copy an extracted app directory to `ux0:app/<TITLEID>/` and refresh the LiveArea (VitaShell: Triangle → Refresh LiveArea). `vitacompanion` can do this remotely with `promote`:

```sh
echo 'promote ux0:data/my-homebrew' | nc $VITA_IP 1338
```

[V] ([vitacompanion README](https://github.com/devnoname120/vitacompanion))

### 1.8 The fast iteration loop

Reinstalling a VPK for every change is slow. Two better loops:

**Replace the eboot over FTP:**

```sh
# VitaShell FTP (port 1337) or vitacompanion
curl -T build/eboot.bin ftp://$VITA_IP:1337/ux0:/app/HOMB00001/eboot.bin
```

Then relaunch the app. This works because the app's `eboot.bin` is just a file in `ux0:app/<TID>`. [V] (xyzz's how2homebrew gist; vitacompanion)

**Remote quit/launch with vitacompanion:**

```sh
echo 'quit HOMB00001'    | nc $VITA_IP 1338
curl -T build/eboot.bin ftp://$VITA_IP:1337/ux0:/app/HOMB00001/eboot.bin
echo 'launch HOMB00001'  | nc $VITA_IP 1338
```

`launch` takes a **title ID**, not a path; `quit` takes a title ID or `all`. [V] (vitacompanion README)

**Do not** use the old `echo destroy | nc ...` recipe found in some 2026 AI-generated wikis; the documented command is `quit <TITLEID>` / `quit all`. [L]

---

## 2. taiHEN plugins

### 2.1 `.suprx` vs `.skprx`

| | User plugin (`.suprx`) | Kernel plugin (`.skprx`) |
|---|---|---|
| Loads into | A user process (game/app/shell) | Kernel (`KERNEL_PID = 0x10005`) |
| Config section | Title ID, `*ALL`, `*main` | `*KERNEL` |
| Can hook | Imports of the target process; exports of modules loaded in that process | Exports/imports of kernel modules |
| Cleanup | taiHEN cleans user patches when the process exits; still release hooks explicitly | **Nothing is cleaned automatically**; you must release hooks/injections in `module_stop` |
| Title-scoped loading | Yes | **No** native support; kernel limitation. Workaround: `taiLoadKernelModule` + `taiStartKernelModule` from a user plugin, with unsafe homebrew enabled |
| Reload | HENkaku Settings → Refresh taiHEN config | Reboot |

[V] (taiHEN docs, `taihen.h`, `plugin.c`)

`module_start` returns `SCE_KERNEL_START_SUCCESS` for a resident module. For a fire-and-forget kernel module, return `SCE_KERNEL_START_NO_RESIDENT`; taiHEN will clean it up after it runs. [V] (taiHEN USAGE)

### 2.2 `config.txt`: paths, precedence, format

**Path precedence:** taiHEN opens `ux0:tai/config.txt` first; if that fails, it opens `ur0:tai/config.txt`. [V] (`plugin.c`)

This is the source of a huge number of "my plugin isn't loading" reports: a stale `ux0:tai/config.txt` **shadows** the `ur0:tai/config.txt` you are editing. On SD2Vita setups, `ux0` is the microSD, so the stale file can be on a different card than you think.

**Recommended modern layout:** keep one config in `ur0:tai` and delete `ux0:tai` entirely. This is what the Vita Hacks Guide does for StorageMgr/YAMT. [V] ([Vita Hacks Guide](https://vita.hacks.guide/storagemgr))

**Format rules:**

- `#` starts a comment.
- `*` starts a section.
- Any other non-whitespace character starts a plugin path.
- Section names are compared with `strcmp`, so they are **case-sensitive**. `*ALL` is uppercase; `*KERNEL` is uppercase; SceShell's special ID is lowercase `*main`. [V] (`taihen-parser/src/parser.c`)
- `*!Section` is a **halt point**: parsing stops at that section. This is real, underdocumented, and useful for blacklisting. [V] (taihen-parser README)
- Paths may contain spaces; leading/trailing whitespace is trimmed; the file is UTF-8. [V] (taihen-parser)
- A path must belong to a section; a path before the first `*` makes the config invalid. [V] (`taihen_config_validate`)
- Order within a section is load order. Dependencies must come first. [V]

**Example `ur0:tai/config.txt`:**

```text
# Comments start with '#'. Sections start with '*'.
# Kernel plugins load at taiHEN start; user plugins load per title.
# After editing: HENkaku Settings -> Refresh taiHEN config (user plugins);
# reboot for kernel plugins.

*KERNEL
ur0:tai/0syscall6.skprx
ur0:tai/storagemgr.skprx

*main
ur0:tai/henkaku.suprx
ur0:tai/shellbat.suprx

*NPXS10015
ur0:tai/henkaku.suprx

*ALL
ur0:tai/vitagrafix.suprx

*PCSE00001
ur0:tai/game_specific.suprx
```

**Why `0syscall6` before `storagemgr`:** users report that `0syscall6`/`reF00D` fail to load if listed after the storage driver; putting it first fixed `C1-6703-6` in a GBAtemp thread. [C] ([GBAtemp](https://gbatemp.net/threads/repatch-ref00d-or-0syscall6-not-working.564305))

**`boot_config.txt` is not `config.txt`.** Enso reads `ur0:tai/boot_config.txt` at bootloader stage. It is model-specific and dangerous to edit; corrupting it can prevent boot. Use the Enso installer's "fix boot config" option, not a text editor. [V] (enso FAQ; PSX-Place boot_config thread)

### 2.3 Hooks

The core API (from `taihen.h`):

```c
SceUID taiHookFunctionExportForKernel(SceUID pid, tai_hook_ref_t *p_hook,
    const char *module, uint32_t library_nid, uint32_t func_nid,
    const void *hook_func);

SceUID taiHookFunctionImportForKernel(SceUID pid, tai_hook_ref_t *p_hook,
    const char *module, uint32_t import_library_nid,
    uint32_t import_func_nid, const void *hook_func);

SceUID taiHookFunctionOffsetForKernel(SceUID pid, tai_hook_ref_t *p_hook,
    SceUID modid, int segidx, uint32_t offset, int thumb,
    const void *hook_func);

SceUID taiHookFunctionAbs(SceUID pid, tai_hook_ref_t *p_hook,
    void *dest_func, const void *hook_func);

int taiHookReleaseForKernel(SceUID tai_uid, tai_hook_ref_t hook);
```

User-space equivalents are `taiHookFunctionExport`, `taiHookFunctionImport`, `taiHookFunctionOffset`, `taiHookRelease`. [V]

**Rules that prevent most crashes:**

1. **Check every return value.** A wrong NID/module returns `TAI_ERROR_NOT_FOUND` (`0x90010002`). If you ignore it and call `TAI_CONTINUE` on an uninitialized ref, you jump through garbage.
2. **Always call `TAI_CONTINUE`**, even if you override the return value. This preserves the hook chain. The docs are explicit: not calling it prevents other plugins' hooks from running. [V]
3. **Hook function prototypes must match exactly.** taiHEN does not type-check. A wrong signature corrupts the stack.
4. **Shared-library exports cannot be hooked** (modules in the `0xE0000000` range, e.g. `SceLibKernel`, `SceGxm`). Hook the **imports** of the modules that use them instead. [V]
5. **Weak imports** (modules loaded on demand via `sceSysmoduleLoadModule` / `sceKernelLoadStartModule`) need load/unload hooks: install your hook after load, release before unload. [V]
6. **Hooking a function while it is being called can crash** — known taiHEN bug #12. Avoid hot-patching tight loops; hook at module start when possible. [V]
7. **User pointers in kernel hooks are hostile.** Never dereference a `const char *path` from user mode directly. Copy with `ksceKernelStrncpyUserToKernel` / `taiMemcpyUserToKernel` and bound the length. [V]
8. **Don't recurse.** If you hook `ksceIoOpen` and then call `ksceIoOpen` (or a logging path that does) inside the hook, you re-enter your own hook. Use an in-memory ring buffer and flush from a thread. [C]

**Kernel hook example — log file opens safely:**

```c
#include <taihen.h>
#include <vitasdkkern.h>
#include <psp2kern/kernel/debug.h>
#include <psp2kern/kernel/sysmem/data_transfers.h>

static tai_hook_ref_t g_open_ref;
static SceUID g_open_hook = -1;

static SceUID open_patch(const char *path, int flags, SceMode mode)
{
    char k_path[256];

    /* Copy before doing anything else; the pointer is a user pointer. */
    if (ksceKernelStrncpyUserToKernel(k_path, (uintptr_t)path,
                                      sizeof(k_path)) >= 0) {
        ksceKernelPrintf("[mykern] open %s flags=%x\n", k_path, flags);
    }

    /* Always continue the chain; pass the original pointer. */
    return TAI_CONTINUE(SceUID, g_open_ref, path, flags, mode);
}

int module_start(SceSize argc, const void *args)
{
    g_open_hook = taiHookFunctionExportForKernel(
        KERNEL_PID,
        &g_open_ref,
        "SceIofilemgr",
        TAI_ANY_LIBRARY,
        0x75192972,          /* ksceIoOpen */
        open_patch);

    if (g_open_hook < 0) {
        ksceKernelPrintf("[mykern] hook failed: 0x%08X\n", g_open_hook);
        return SCE_KERNEL_START_FAILED;
    }
    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args)
{
    if (g_open_hook >= 0) {
        taiHookReleaseForKernel(g_open_hook, g_open_ref);
        g_open_hook = -1;
    }
    return SCE_KERNEL_STOP_SUCCESS;
}
```

`0x75192972` is the NID used in taiHEN's own docs for `ksceIoOpen`/`sceIoOpenForDriver`; `0xCC67B6FD` is the `SceIofilemgr::_sceIoOpen` export, while the user-facing `SceLibKernel::sceIoOpen` is `0x6C60AC61`. [V] (taiHEN docs, `taihen.h`, 3.60 NID database)

**User-space import hook example — redirect one app's file open:**

```c
#include <taihen.h>
#include <psp2/kernel/threadmgr.h>
#include <string.h>

static tai_hook_ref_t g_open_ref;
static SceUID g_open_hook_uid = -1;

static SceUID open_hook(const char *path, int flags, SceMode mode)
{
    if (strcmp(path, "ux0:data/myapp/config.ini") == 0) {
        path = "ux0:data/myapp/config.default.ini";
    }
    return TAI_CONTINUE(SceUID, g_open_ref, path, flags, mode);
}

int module_start(SceSize argc, const void *args)
{
    /* SceLibKernel = 0xCAE9ACE6, sceIoOpen = 0x6C60AC61 (taiHEN docs) */
    g_open_hook_uid = taiHookFunctionImport(&g_open_ref, "MyGame",
                                            0xCAE9ACE6, 0x6C60AC61, open_hook);
    if (g_open_hook_uid < 0)
        return SCE_KERNEL_START_FAILED;

    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args)
{
    if (g_open_hook_uid >= 0) {
        taiHookRelease(g_open_hook_uid, g_open_ref);
        g_open_hook_uid = -1;
    }
    return SCE_KERNEL_STOP_SUCCESS;
}
```

The docs use exactly these NIDs for a local import hook. [V] (taiHEN USAGE)

**Offset hook example — only after validating the module:**

```c
tai_module_info_t info = {0};
info.size = sizeof(info);

/* SceIofilemgr is a kernel module, so KERNEL_PID is valid.
 * SceShell is userland; use taiGetModuleInfo() for it. */
int ret = taiGetModuleInfoForKernel(KERNEL_PID, "SceIofilemgr", &info);
if (ret < 0) {
    return ret; /* module not loaded */
}

/* Validate the exact module identity before patching. */
if (info.module_nid != EXPECTED_MODULE_NID) {
    return TAI_ERROR_INVALID_MODULE;
}

/* segidx/offset/thumb must be found for this exact firmware.
 * EXPECTED_OFFSET is a placeholder; do not ship a literal you did not verify. */
ret = taiHookFunctionOffsetForKernel(KERNEL_PID, &g_ref, info.modid,
                                     0, EXPECTED_OFFSET, 1, my_hook);
```

The module NIDs `0x0552F692` (retail 3.60) and `0x5549BF1F` (retail 3.65) and the text-segment sizes `0x541B74` / `0x5420F4` are documented in a real patch project's firmware-validation notes. [V] ([livearea_nolimits firmware-validation](https://github.com/devnoname120/livearea_nolimits/blob/main/docs/firmware-validation.md))

### 2.4 Injections

```c
SceUID taiInjectDataForKernel(SceUID pid, SceUID modid, int segidx,
                              uint32_t offset, const void *data, size_t size);
SceUID taiInjectAbsForKernel(SceUID pid, void *dest, const void *src,
                             size_t size);
int taiInjectReleaseForKernel(SceUID tai_uid);
```

Rules:

- Use an injection for **read-only data** or for a function too small to hook.
- **Only one module can patch a given module + address at a time.** The kernel saves the original data, so huge patches are not recommended. [V] (`taihen.h`)
- Release the injection when done. [V]
- Validate the target module and offset first; an injection into the wrong firmware is a brick-adjacent bug. [C]

Real example from PSVshell: it NOPs two bytes at `ScePervasiveForDriver_0xE9D95643 + 0x1D`:

```c
const uint8_t nop[] = {0x00, 0xBF};
g_injects[0] = taiInjectAbsForKernel(KERNEL_PID,
    (void *)((uintptr_t)ScePervasiveForDriver_0xE9D95643 + 0x1D),
    &nop, sizeof(nop));
```

[V] ([PSVshell source](https://github.com/Electry/PSVshell/blob/master/src/main.c))

### 2.5 Custom syscall exports and generated stub libraries

A kernel module can export a library to user mode as **syscalls**. In the module YAML:

```yaml
mykern:
  attributes: 0
  process_image: false
  imagemodule: false
  version:
    major: 1
    minor: 1
  main:
    start: module_start
    stop: module_stop
  libraries:
    MyKern:
      syscall: true
      version: 1
      functions:
        - my_kernel_function
```

The `syscall: true` key is documented in the module-config template: "The kernel module is used to expose exports to users. If true, other kernel modules cannot import this library." [V] (`yml_template/module_config.yml`)

Generate the matching user stub:

```cmake
vita_create_stubs(mykern_stubs mykern exports.yml KERNEL)

link_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(myapp src/main.c)
target_link_libraries(myapp mykern_stub SceLibKernel_stub)
add_dependencies(myapp mykern_stubs)
```

`vita_create_stubs` runs `vita-elf-export` and `vita-libs-gen`, then copies `lib<name>_stub.a` and `lib<name>_stub_weak.a` into the build directory. [V] (`vita.cmake`)

Gotchas:

- The YAML's **first line must be the module name at column 0**, because `vita_create_stubs` derives the stub name with a regex on the file content. [V] (`vita.cmake`)
- `vita-libs-gen` has a 1024-entry limit per library in CMake mode; `vita-libs-gen-2` removes it. [V] (vita-toolchain README)
- **The syscall stack is only 4096 bytes.** Deep call chains or large local buffers in a syscall will overflow. Use `ksceKernelRunWithStack` to increase it when needed. [V] (taiHEN USAGE)
- A module that exposes syscalls **cannot be unloaded by default**; see §2.6.

### 2.6 Module unloading limits

If you try to unload a kernel module that exposes syscalls, the kernel returns:

```
SCE_KERNEL_ERROR_MODULEMGR_IN_USE = 0x8002D002
```

[V] (`psp2/kernel/error.h`; taiHEN USAGE)

The taiHEN docs give the workaround: hook an override function in the module manager that always returns 1, and release the hook when the module unloads. The docs' example hooks `SceKernelModulemgr`, library `SceSblACMgrForKernel` `0x11F9B314`, function `0xBBA13D9C`. [V] (taiHEN USAGE)

Other unload facts:

- `module_stop` of your **user** plugin is **not** called automatically when you unload the kernel module it loaded. Do not rely on it for kernel cleanup. [V]
- Kernel patches are not auto-cleaned. Release every hook and injection in `module_stop`. [V]
- If the target module unloads while your hook is installed, the hook trampoline can point into freed memory. Hook the target's unload path and release first. [V] (weak-import example in taiHEN USAGE)

### 2.7 Firmware and NID differences (3.60 / 3.65 / 3.68 / 3.74)

**What is stable:** function NIDs are hashes of function names/signatures and are often stable across firmware. **What is not stable:** module NIDs, library NIDs, segment layouts, and offsets. A plugin that hardcodes a library NID from 3.60 can fail on 3.65 even though the function NID is unchanged.

Concrete example from PSVshell's source — it resolves both 3.60 and 3.65 pairs and falls back:

```c
ret = taiGetModuleExportFunc(KERNEL_PID,
    "SceKernelModulemgr", 0xC445FA63, 0xD269F915,
    (uintptr_t *)&_ksceKernelGetModuleInfo);          /* 3.60 */
if (ret < 0)
    taiGetModuleExportFunc(KERNEL_PID,
        "SceKernelModulemgr", 0x92C9FFC2, 0xDAA90093,
        (uintptr_t *)&_ksceKernelGetModuleInfo);      /* 3.65 */
```

and:

```c
/* SceSysmem */
0x63A519E5 / 0x3650963F   /* 3.60 library / function */
0x02451F0F / 0xB9B69700   /* 3.65 library / function */
```

[V] ([PSVshell `main.c`](https://github.com/Electry/PSVshell/blob/master/src/main.c))

**Module NIDs differ too.** The retail SceShell module NID is `0x0552F692` on 3.60 and `0x5549BF1F` on 3.65, and the text segment size changes from `0x541B74` to `0x5420F4`. [V] (livearea_nolimits firmware-validation)

**Practical rules:**

1. Resolve by module name + function NID with `TAI_ANY_LIBRARY` when possible; it is more portable than hardcoding a library NID.
2. If you must use a library NID, try the known 3.60 pair, then the 3.65 pair, and fail gracefully if both fail.
3. For offset hooks, validate `module_nid` **and** the segment size before patching. If either mismatches, do nothing. This is how real system-patch plugins avoid bricking unknown firmware.
4. Do not assume a 3.60 plugin works on 3.65 or vice versa. Modern taiHEN builds (the 0.12.x line shipped with current Enso) advertise support for all FW ≥ 3.60 with a single module, but that is the loader, not your plugin's NIDs. [C] (taiHEN 0.12.2 changelog via GameBrew)
5. On 3.68 and 3.74 there is no Enso; you are running h-encore/HENlo + taiHEN. Plugins may work if the NIDs match, but the supported development baseline is 3.60 or 3.65.

**Firmware map for plugin authors:**

| FW | Exploit | Persistent CFW | Development note |
|---|---|---|---|
| 3.60 | HENkaku | Enso 3.60 | Oldest; some plugins/patches target only this. |
| 3.61 / 3.63 | none public | n/a | Update to 3.74 then use HENlo, or use a hardware method; modoru needs an existing exploit. |
| 3.65 | h-encore / HENlo | Enso 3.65 | **Recommended modern base.** |
| 3.67 / 3.68 | h-encore / HENlo | no Enso | Downgrade to 3.65 for Enso. |
| 3.69–3.73 | h-encore² | no Enso | Downgrade. |
| 3.74 | HENlo / h-encore² | no Enso | Downgrade to 3.65 if factory firmware permits. |

[V] (Vita Hacks Guide, h-encore README, enso FAQ)

---

## 3. Kernel patches

### 3.1 What taiHEN can and cannot patch

taiHEN runs as a kernel module and hooks **non-secure-world kernel modules** and user processes. It does **not** patch lv0 / secure_kernel. Patches at that level use a different mechanism: enso_ex's base-kernel loader, psp2renga/HFW, or a dedicated syscall patch such as `0syscall6`. [V] (0syscall6 README: "patches lv0's secure_kernel's syscall 6"; enso_ex README: custom base-kernel module loader)

This distinction matters: a taiHEN plugin cannot "patch the firmware" in the bootloader sense. If a guide tells you to drop a `.skprx` in `*KERNEL` to change lv0 behavior, it is probably wrong or is relying on a separate exploit.

### 3.2 Common targets and what they control

| Module | What it controls | Typical hooks | Notes |
|---|---|---|---|
| `SceIofilemgr` | File I/O, mounts, directory ops | `ksceIoOpen` `0x75192972`; `_sceIoOpen` `0xCC67B6FD`; `SceLibKernel::sceIoOpen` `0x6C60AC61`; `ksceIoDread`, `ksceIoMount` | Copy user pointers; avoid recursion; don't block. |
| `SceSysmem` | Memory blocks, process local storage | `ksceKernelAllocMemBlock`, `ksceKernelFindMemBlockByAddr`, `ksceKernelGetProcessLocalStorageAddrForPid` | Library/function NIDs differ 3.60 vs 3.65. |
| `SceKernelModulemgr` | Module load/start/stop/info | `ksceKernelGetModuleInfo`, `ksceKernelLoadStartModule`, `ksceKernelGetModuleInfoMinByAddr` | Used for unload overrides and module validation. |
| `SceSblACMgr` | Auth IDs, program type, capabilities | `ksceSblACMgrIsGameProgram`, `ksceSblACMgrGetProcessProgramAuthId`, `ksceSblACMgrHasCapability` | Security-sensitive. Do not use to bypass license checks. |
| `SceLowio` / `ScePervasive` | Clock/performance registers | `ScePervasiveForDriver` `0xE9D95643` | PSVshell's overclock path. |
| `ScePower` | CPU/GPU/BUS clocks | `kscePowerSetArmClockFrequency`, etc. | Supported API; safer than raw register pokes. |

[V] (vita-headers: `psp2kern/kernel/acmgr.h`, `modulemgr.h`, `sysmem.h`; PSVshell source; taiHEN docs)

### 3.3 Patching checks: the "validate, then patch" pattern

The safe pattern for a system patch is:

1. Resolve the module and get `tai_module_info_t`.
2. Verify `module_nid` against the exact firmware(s) you support.
3. Verify the text segment size (or a hash) matches your patch table.
4. Apply patches one at a time; if any fails, roll back the ones already applied.
5. If the module identity is unknown, **do not patch**.

The `livearea_nolimits` project documents this for SceShell: it validates 22 patch regions against module NID and segment size, and rolls back earlier injections if a later one fails. It also notes that the old 3.60/3.65 patch tables were wrong — the "3.65" table actually matched retail 3.60. That is exactly the failure mode you avoid by validating. [V] ([firmware-validation.md](https://github.com/devnoname120/livearea_nolimits/blob/main/docs/firmware-validation.md))

A check-patch hook is simple:

```c
static tai_hook_ref_t g_check_ref;

static int check_patch(void)
{
    /* Always continue the chain first. */
    TAI_CONTINUE(int, g_check_ref);
    return 1;   /* force "allowed" */
}
```

The taiHEN docs use this shape for a sysroot check that enables dynarec. [V] (taiHEN USAGE)

### 3.4 Overclocking

PSVshell exposes these exact steps:

| Device | Frequencies (MHz) | Default |
|---|---|---|
| CPU | 41, 83, 111, 166, 222, 333, 444, 500 | 333 |
| GPU (ES4) | 41, 55, 83, 111, 166, 222 | 111 |
| BUS | 55, 83, 111, 166, 222 | 222 |
| XBAR | 83, 111, 166 | 111 |

[V] ([PSVshell README](https://github.com/Electry/PSVshell))

The 500 MHz CPU path is not a normal `scePower` call. PSVshell:

1. Calls `ScePervasiveForDriver_0xE9D95643(15, 16)` to apply a 15:16 multiplier.
2. Writes `500` and `15` to `ScePower_41C8` / `ScePower_41CC` so `kscePowerGetArmClockFrequency()` reports the truth.
3. NOPs two bytes at `ScePervasiveForDriver_0xE9D95643 + 0x1D`.

[V] (PSVshell `oc.c`, `main.c`)

Risks: 500 MHz is not stable on every unit; heat and battery drain rise; a crashed overclock can take the system down. Always restore defaults in `module_stop`. [C]

### 3.5 Mounting storage

**YAMT (Yet Another Mount Tool):**

- Kernel plugin for SD2Vita, PSVSD, and USB; requires Enso; supports 3.60/3.65. [V] (YAMT/PSX-Place)
- Mounts SD2Vita as `ux0:` and the Sony card as `uma0:` by default; configurable in **Settings → Devices → Storage Devices**. [V] (Vita Hacks Guide YAMT)
- Config file: `ur0:tai/yamt.cfg`. If a mount is misbehaving, delete it and reboot. [V] (YAMT issue #10)
- The full version changes `boot_config.txt`; the lite version is for normal users. [V] (PSX-Place YAMT)
- Do not run YAMT and StorageMgr at the same time. [V] (Vita Hacks Guide StorageMgr warns to uninstall YAMT first)

**StorageMgr:**

- `ur0:tai/storagemgr.skprx` plus `ur0:tai/storage_config.txt`.
- `MCD=uma0`, `INT=imc0`, `GCD=ux0` (SD2Vita) or `UMA=ux0` (USB). [V] (Vita Hacks Guide StorageMgr)
- Delete `ux0:tai` after migrating, or the stale config shadows `ur0:tai`. [V]

**Anti-brick storage advice:** keep VitaDeploy installed on the **official memory card** (or internal storage), not only on the SD2Vita. If the SD2Vita dies or its mount config is wrong, you still have a file manager and installer. [V] (Vita Hacks Guide YAMT)

### 3.6 Anti-brick and recovery

**If a plugin causes a bootloop:**

1. **Hold `L` during boot.** taiHEN skips plugin loading. This is the first-line recovery and is documented by the Enso FAQ and Vita Hacks Guide. [V]
2. If it boots, edit the active `config.txt` (usually `ur0:tai/config.txt`) and remove the new line.
3. If it still does not boot, enter **Safe Mode**: power off, then hold **R + Power + PS** until the PS logo appears. [V] (Sony support)
4. In Safe Mode:
   - **Restart** — just reboots.
   - **Rebuild Database** — rebuilds the app database; can restore a bootable state with plugins disabled. It deletes LiveArea layout/messages/playlists but not saves. [V] (ConsoleMods, update365 FAQ)
   - **Restore the PS Vita System** — resets settings and can regenerate `boot_config.txt`; this is the heavy option. [V] (enso FAQ)
5. **Do not edit `boot_config.txt`.** To deactivate Enso, delete `ur0:tai/boot_config.txt`; to reactivate, run the Enso installer and use "fix boot config". [V] (enso FAQ)
6. If you use enso_ex: hold **VOLUP** at boot if `boot_config.txt` is corrupt; hold **START** for SD-as-os0 recovery; **VOLDOWN** skips a failing module. [V] (psp2hfw README, enso_ex README)

**Permanent-brick reality:** normal plugin/config mistakes are recoverable soft bricks. Permanent bricks come from interrupted firmware flashes, corrupted bootloaders, or writing to the wrong eMMC/boot area. The Vita Hacks Guide calls bricks "basically impossible unless you ignore instructions," but that is a statement about following the guide, not a guarantee. Treat any bootloader/firmware write as high-risk. [V] + [C]

---

## 4. On-device debugging

### 4.1 Logging

| Method | Scope | How to read it |
|---|---|---|
| `sceClibPrintf` / `printf` | User app/plugin | TTY; capture with Cat-A-Log or a serial/UART setup. |
| `ksceKernelPrintf` | Kernel plugin | Same; Cat-A-Log captures it. |
| `psvDebugScreenPrintf` | User app | On-screen debug text (debugscreen sample). |
| File logging | Any | Write to `ux0:data/<app>/log.txt`; flush and close. |
| DebugNet | User app | UDP to a PC; see §4.2. |

[V] (Cat-A-Log README; VitaSDK samples)

**Cat-A-Log setup:**

1. Copy `catlog.skprx` and `catlog.suprx` to `ur0:tai`.
2. `*KERNEL`: `ur0:tai/catlog.skprx`.
3. `*NPXS10015`: `ur0:tai/catlog.suprx`.
4. Reboot.
5. On the PC: `nc -kl <port>`.
6. On the Vita: **Settings → Network → Cat Log settings**, set the PC IP/port.

[V] ([Cat-A-Log README](https://github.com/isage/catlog))

**Do not** put heavy logging inside a filesystem hook. `ksceKernelPrintf` can be slow, and any path that re-enters `SceIofilemgr` can recurse. Use a ring buffer in kernel memory and a low-priority thread to flush it. [C]

### 4.2 DebugNet

DebugNet is a user-space UDP logging library. Server side:

```sh
socat udp-recv:18194 stdout
```

Vita side:

```c
#include <debugnet.h>

debugNetInit("192.168.1.10", 18194, DEBUG);
debugNetPrintf(DEBUG, "value = %d\n", value);
debugNetFinish();
```

[V] ([psxdev/debugnet](https://github.com/psxdev/debugnet))

### 4.3 GDB stubs

**KVDB** is a kernel-mode GDB stub. By default it listens on UART; for TCP you need the **vdbtcp** companion. [V] ([KVDB README](https://github.com/DaveeFTW/kvdb))

- `kvdb.skprx` under `*KERNEL`.
- `vdbtcp.suprx` under `*main` (it provides GDB on 31337, FTP on 1337, and vitacompanion-compatible commands on 1338). [V] ([vdbtcp README](https://github.com/DaveeFTW/vdbtcp))
- GDB:

```gdb
set architecture arm
target remote 192.168.1.50:31337
```

- KVDB is alpha-quality: one debugging session per boot, memory faults are not handled cleanly, and it does not launch the target itself. vdbtcp can launch a SELF via its command service. [V]

**`vita-uvdb`** is an alternative intra-process GDB stub that needs only the `kubridge` kernel plugin, not KVDB. It is useful when you want breakpoints inside your own app without a kernel debugger. [C] (build-vitasdk docs)

### 4.4 Core dumps (`.psp2dmp`)

When a user app crashes with `C2-12828-1`, the Vita can write a core dump to `ux0:data`:

```
ux0:data/psp2core-<pid>-<addr>-<eboot-name>.psp2dmp
```

The exact filename varies; the extension is `.psp2dmp`. [V] (xyzz/vita-parse-core; VitaShell issue #534)

The file is a **gzip-compressed ELF core** with Sony's own note layout: `MODULE_INFO`, `THREAD_INFO`, `THREAD_REG_INFO`, `STACK_INFO`, `TTY_INFO` — not Linux `NT_PRSTATUS`. Generic `gdb target core` will not understand it. [V] ([vita-crashdump README](https://github.com/TibixDev/vita-crashdump))

**Parse it:**

```sh
# Classic tool (Python 2, pinned old pyelftools)
python2 main.py psp2core-....psp2dmp build/hello_vita.elf
```

`vita-parse-core` needs `arm-vita-eabi-objdump` and `arm-vita-eabi-addr2line` on `PATH`, and you must pass the **unstripped `.elf`**, not `.velf` and not `eboot.bin`. Build with `-g`. [V] ([xyzz/vita-parse-core](https://github.com/xyzz/vita-parse-core))

Modern alternatives:

```sh
vita-crashdump psp2core-....psp2dmp build/hello_vita.elf
```

`vita-crashdump` is a Rust rewrite with ARM unwinding, fault-address extraction (`DFAR`/`IFAR`), demangling, and a GUI. [V] ([TibixDev/vita-crashdump](https://github.com/TibixDev/vita-crashdump))

**Kernel coredump trigger:** VitaSDK 2026.08 does **not** declare
`ksceCoredumpCreateDump` in `psp2kern/coredump.h`. The kernel-side hook is
`ksceKernelSysrootRegisterCoredumpTrigger()` with a
`SceCoredumpTriggerParam` (`psp2kern/kernel/sysroot.h`,
`psp2kern/coredump.h`). The ABI is undocumented; this registers a callback
rather than directly forcing a dump:

```c
#include <psp2kern/kernel/sysroot.h>
#include <psp2kern/coredump.h>

static int coredump_trigger(SceUID pid,
                            SceKernelCoredumpStateUpdateCallback update,
                            SceKernelCoredumpStateFinishCallback finish,
                            SceCoredumpTriggerParam *param)
{
    /* Fill param and trigger the dump through the system service. */
    return 0;
}

ksceKernelSysrootRegisterCoredumpTrigger(coredump_trigger);
```

**Kernel dumps are a different mechanism.** `psp2kerndmp` / kernel-module dumpers produce kernel dumps, and the user-space `vita-parse-core` is not the right parser for them. I could not verify the `.spsp2dmp` extension; treat that as unconfirmed. [C]

**ASLR:** crash addresses vary between runs. Build with `NOASLR` in `vita_create_self` when you need reproducible addresses, or use the module+offset resolution that the parsers provide. [V] (`vita.cmake`)

### 4.5 Common crash causes in kernel/user code

| Cause | Symptom | Prevention |
|---|---|---|
| NULL/invalid user pointer dereferenced in a kernel hook | Kernel data abort, instant reboot | `ksceKernelStrncpyUserToKernel` / `taiMemcpyUserToKernel`; check return. |
| Wrong NID, hook failed, code still calls the hook | Jump to garbage, crash | Check `taiHookFunction*` return; only use the ref if `>= 0`. |
| Stack overflow | Data abort with SP near the stack guard; random corruption | Keep frames small; 4 KiB syscall stack; use `ksceKernelRunWithStack`. |
| Module unload race | Crash after unloading a plugin/module | Release hooks before unload; hook load/unload for weak imports. |
| Missing `TAI_CONTINUE` | Other plugins stop working; subtle breakage | Always continue the chain. |
| Recursion in a filesystem hook | Hang or stack overflow when logging | Don't call the hooked function or logging that re-enters it. |
| Unaligned access | `SIGBUS`-like data abort | Use `memcpy` for packed structs; avoid casting unaligned pointers. |
| Missing runtime dependency (`libshacccg.suprx`) | `C2-12828-1` at launch for vitaGL apps | Extract it from your own firmware; don't redistribute it. |
| Unsafe homebrew disabled | App fails to launch or restricted API fails | Enable it in HENkaku Settings; document it for users. |

[V] for the API facts; [C] for the symptom patterns.

---

## 5. Firmware and jailbreak landscape

### 5.1 What runs on what

- **3.60:** HENkaku, then Enso 3.60. Oldest but still used; some patches target only 3.60. [V]
- **3.65:** h-encore or HENlo, then Enso 3.65. This is the modern recommended base; VitaDeploy's "Quick 3.65 Install" installs 3.65 + Enso + a taiHEN preset. [V] ([Vita Hacks Guide](https://vita.hacks.guide/installing-enso))
- **3.67/3.68:** h-encore or HENlo; **no Enso**. Downgrade to 3.65 for persistent CFW. [V]
- **3.69–3.73:** h-encore²; **no Enso**. Downgrade. [V]
- **3.74:** HENlo; **no Enso**. Downgrade to 3.65 if the factory firmware permits. [V]
- **3.61/3.63:** no direct modern exploit; modoru to 3.60/3.65. [C]

**Important limit:** modoru can only go down to the console's **factory firmware**. Some devices shipped above 3.65 and cannot be downgraded to 3.60/3.65. [V] (modoru FAQ)

### 5.2 Enso 3.65 and VitaDeploy

VitaDeploy is the current all-in-one toolbox: file manager, SD2Vita mount/format, firmware updater/downgrader, 3.65 Quick Install, optional Enso, optional taiHEN setup, app downloader, IMCUnlock, storage format, battery controller reset, AutoAVLS toggle, kiosk/manufacturing mode disable. It can run entirely from `vs0`. [V] ([VitaDeploy README](https://github.com/SKGleba/VitaDeploy))

Quick 3.65 install:

1. Enable unsafe homebrew.
2. VitaDeploy → **Install a different OS** → **Quick 3.65 Install**.
3. Do not let the Vita sleep.
4. Confirm twice; wait for the reboot.
5. If asked to transfer internal memory to the removable card, choose **No** — "Yes" deletes the memory card data. [V]

Enso FAQ facts:

- Delete `ur0:tai/boot_config.txt` to **deactivate** Enso without uninstalling.
- Re-activate with the installer's Square "fix boot config".
- Triangle fully uninstalls.
- Hold **L** at boot to disable taiHEN plugin loading. [V] ([enso.henkaku.xyz](https://enso.henkaku.xyz/))

### 5.3 h-encore² and HENlo

- **h-encore:** 3.65–3.68; PC-side `finalhe`; hold **R** while launching to force the bootstrap menu. If HENkaku Settings is missing, use "Reset taiHEN config.txt" in the bootstrap menu. [V] ([h-encore README](https://github.com/TheOfficialFloW/h-encore))
- **h-encore²:** 3.65–3.74. [V] (official h-encore² README)
- **HENlo:** browser exploit at `jailbreak.psp2.dev`; works on 3.65, 3.68, and 3.74; installs HENkaku + VitaDeploy. [V] ([Vita Hacks Guide](https://vita.hacks.guide/using-henlo))

After any of these, enable **Settings → HENkaku Settings → Enable Unsafe Homebrew**. [V]

### 5.4 modoru

Requirements and gotchas:

- Device already running HENkaku/h-encore on 3.60–3.74.
- Battery ≥ 50%.
- **All plugins disabled**; rename `ux0:tai` and `ur0:tai` temporarily.
- Cannot launch from SD2Vita; use a memory card or internal storage.
- Put the **full** `PSP2UPDAT.PUP` at `ux0:app/MODORU000/PSP2UPDAT.PUP`; don't reinstall the VPK afterward or the PUP is removed.
- You can only downgrade to your factory firmware or above. [V] ([modoru README](https://github.com/TheOfficialFloW/modoru))

I will not link firmware downloads; obtain the PUP from your own region/console source.

### 5.5 SD2Vita and YAMT

- YAMT is the modern mount tool; requires Enso; 3.60/3.65. [V]
- Settings → Devices → Storage Devices → enable **Use YAMT**, set `ux0:` to SD2Vita and `uma0:` to Default/Memory Card. [V]
- Keep VitaDeploy on the official card for recovery. [V]
- If YAMT mount fails with `0x8002D011`, reboot and reset the taiHEN config; that code is `MODULEMGR_NO_MOD`, i.e. a required module is missing. [V] + [C]
- Do not run YAMT and StorageMgr together. [V]

### 5.6 Plugin config migration

When moving from `ux0:tai` to `ur0:tai`:

1. Copy all plugins and `config.txt` to `ur0:tai`.
2. Update every path inside `config.txt` from `ux0:tai/...` to `ur0:tai/...`.
3. Delete `ux0:tai` entirely. If it remains, `ux0:tai/config.txt` shadows `ur0:tai/config.txt`. [V]
4. Keep load order; `0syscall6` before storage drivers. [C]
5. User plugins: HENkaku Settings → Refresh taiHEN config. Kernel plugins: reboot. [V]

### 5.7 PSN and ban risk

- The Vita Hacks Guide FAQ says: "Sony doesn't ban consoles nor accounts unless you hack trophies or in-game." [V] ([FAQ](https://vita.hacks.guide/faq))
- Community consensus is that ordinary homebrew/plugin use has not produced a ban wave; trophy cheating, online cheating, and pre-release content are the repeatedly cited risk cases. [C] (r/vitahacks, r/VitaPiracy discussions)
- PSN's ToS prohibits modifying/hacking the system, so a ban is always theoretically possible. Use a secondary account if that risk matters to you. [C]
- PSN spoofing/version spoofing in HENkaku Settings is what lets lower-firmware consoles access PSN; it is not a guarantee of safety. [V]

---

## 6. Community etiquette and legal boundaries

**Hard lines:**

- **No Sony keys.** Do not include, request, or link to `keys.bin`, decrypted `kprx_auth_sm`, `psp2renga` key material, or any Sony signing/encryption key.
- **No copyrighted firmware.** Do not redistribute `PSP2UPDAT.PUP`, decrypted firmware images, `os0`/`vs0` dumps, or Sony system modules.
- **No game backups/ROMs.** Do not link to or distribute game dumps, `nonpdrm`-generated licenses, or piracy tools. `modoru` needs a PUP the user supplies; that is the boundary.
- **Do not redistribute `libshacccg.suprx`.** It is a Sony file extracted from the user's own firmware. Guides should tell users to extract it themselves, not ship it.
- **No leaked official SDK.** VitaSDK is a clean-room community toolchain; the leaked Sony SDK is legally grey and not needed for homebrew.

**Etiquette:**

- Credit original authors and respect licenses (MIT/GPL).
- Test kernel plugins on a spare console or at least with a known recovery path.
- Warn users about brick risk, especially for boot-time kernel plugins.
- Don't tell users to install every plugin; each `*KERNEL` entry increases boot risk.
- Don't recommend "format the memory card" as a first fix; it is destructive and usually unnecessary.
- When asking for help, include firmware, exploit (Enso/h-encore/HENlo), plugin versions, exact error code, and whether hold-L boots. [C]
- Don't bump old threads or DM developers for support; use the issue tracker with logs. [C]
- Don't use Autoplugin blindly. It is convenient but opaque; manual `config.txt` editing is easier to debug and is what most guides now recommend. [C]

---

## 7. Dead ends and outdated advice

| Advice | Why it is wrong / outdated | What to do instead |
|---|---|---|
| "You must build with `UNSAFE` or the eboot won't run." | False. Omitting `UNSAFE` produces a **safe** eboot, which runs with restricted APIs. [V] | Start safe; add `UNSAFE` only when a specific API requires it. |
| "Title ID must be 4 uppercase letters + 5 digits." | False. It must be 9 characters; `VITASHELL` is all letters. [V] | Use 9 uppercase alphanumerics, unique to you. |
| "LiveArea background must be `bg0.png`, not `bg.png`." | False. `template.xml` names the file; official samples use `bg.png`. [V] | Use `bg.png` or `bg0.png` consistently with `template.xml`. |
| "PNG must be exactly 8-bit." | Overgeneralized. Indexed color type 3 is the key; a 1-bit indexed PNG works in the official sample. [V] | Convert to palette/indexed; 8-bit is the safe default. |
| "`pic0.png` is optional." | Community reports say the installer rejects VPKs without it. Not in the formal spec, but include it. [C] | Ship a 960×544 indexed `pic0.png`. |
| "Always use `ux0:tai`." | Outdated. `ur0:tai` is the standard, and a stale `ux0:tai/config.txt` shadows it. [V] | Use `ur0:tai`; delete `ux0:tai`. |
| "A 3.60 plugin works on 3.65." | Not reliably. Library/module NIDs and offsets differ. [V] | Resolve at runtime with 3.60/3.65 fallbacks; validate module NID/size. |
| "Rebuild Database fixes every bootloop." | It can restore boot, but it wipes LiveArea layout/messages/playlists. [V] | Try hold-L and config edit first. |
| "Format the memory card to fix crashes." | Destructive; rarely the actual cause. [C] | Check plugins, config, storage driver, free space first. |
| "Install Enso on 3.68/3.74." | Enso is 3.60/3.65 only. [V] | HENlo/h-encore + downgrade to 3.65 via VitaDeploy/modoru. |
| "Trinity is the tool for 3.69–3.70." | Superseded by h-encore²/HENlo. [V] | Use the current Vita Hacks Guide flow. |
| "Use the 3.65 Enso updater from 3.60." | Superseded by VitaDeploy Quick 3.65. [V] | Use VitaDeploy. |
| "VPK is just a ZIP; rename any ZIP to `.vpk`." | Only if `eboot.bin` and `sce_sys/param.sfo` are at the root. [V] | Inspect the archive; use `vita-pack-vpk` or VitaShell's folder install. |
| "`echo destroy | nc ... 1338` quits the app." | Old/incorrect vitacompanion command. [V] | `echo 'quit <TITLEID>' | nc ... 1338`. |
| "Autoplugin is the recommended way." | Convenience tool; opaque config and duplicate-plugin risk. [C] | Edit `config.txt` manually; use Autoplugin only as a starting point. |
| "Vita hacking cannot brick the console." | Permanent firmware/bootloader writes can brick. Normal plugin mistakes are soft bricks. [V] + [C] | Treat firmware/bootloader operations as high-risk; keep recovery media. |
| "Use the leaked Sony SDK for private APIs." | Legally grey and unnecessary; VitaSDK exposes the same reverse-engineered surface. [C] | Use VitaSDK + taiHEN. |

---

## 8. Kernel plugin safety checklist

Use this before shipping any `.skprx`.

- [ ] **I know why this must be kernel-space.** A user plugin cannot do it; I am not using kernel mode for convenience.
- [ ] **I have read the taiHEN USAGE doc** and understand hooks vs. injections vs. peek/poke.
- [ ] **I have a recovery path:** hold-L works, I know the Safe Mode combo, and I have a backup of `ur0:tai/config.txt`.
- [ ] **The plugin is not in `*KERNEL` yet.** I load it manually or from a test title until it is proven.
- [ ] **Every `taiHookFunction*` return is checked.** On failure, I do not call `TAI_CONTINUE`.
- [ ] **Every hook calls `TAI_CONTINUE`** and preserves the chain.
- [ ] **Every user pointer is copied** with `ksceKernelStrncpyUserToKernel` / `taiMemcpyUserToKernel`, with a bound.
- [ ] **No recursion:** I do not call the hooked function or a logging path that re-enters it.
- [ ] **No blocking I/O, sleep, or long loops in a hook.** Work is deferred to a thread.
- [ ] **Stack frames are small.** I remember the 4 KiB syscall stack; I use `ksceKernelRunWithStack` if needed.
- [ ] **Offset hooks validate module NID and segment size** before patching.
- [ ] **Every hook and injection is released in `module_stop`.**
- [ ] **Unload races are handled:** hooks are released before the target module unloads.
- [ ] **I do not unload a module that exposes syscalls** without the modulemgr override (`0x8002D002`).
- [ ] **I use `SCE_KERNEL_START_NO_RESIDENT`** for fire-and-forget modules.
- [ ] **I do not patch `SceSbl*`/auth checks casually.** If I do, I document the risk and test every supported firmware.
- [ ] **I test on 3.60 and 3.65** if I claim both; I do not assume NIDs/offsets match.
- [ ] **I version-gate:** unknown module NID → do nothing, log, and exit cleanly.
- [ ] **I warn users** about boot risk and tell them how to recover.
- [ ] **I ship source and a license**, and I do not include Sony keys/firmware.

---

## 9. Recommended workflows

### 9.1 New homebrew app

1. **Install VitaSDK** with `vdpm`; set `VITASDK` and `PATH`. Verify with `arm-vita-eabi-gcc --version`.
2. **Build the official `hello_world` sample first.** If that fails, fix the toolchain before writing your own CMake.
3. **Create `CMakeLists.txt`** from §1.2. Set a unique 9-char title ID and a `##.##` version.
4. **Add LiveArea assets** from §1.6. Convert PNGs to indexed/palette; include `icon0.png`, `pic0.png`, `bg.png`, `startup.png`, `template.xml`.
5. **Build:** `cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake && cmake --build build`.
6. **Inspect the VPK:** `unzip -l build/hello-v<version>-b<build>.vpk` — confirm `eboot.bin` and `sce_sys/param.sfo` are at the root.
7. **Deploy:** FTP the VPK to `ux0:` and install with VitaShell; or use `vitacompanion promote` for an extracted directory.
8. **Iterate:** replace `ux0:app/<TID>/eboot.bin` over FTP and relaunch; avoid full reinstalls.
9. **Debug:** build with `-g`, keep the unstripped `.elf`, reproduce a crash, pull the `.psp2dmp`, parse it with `vita-parse-core` or `vita-crashdump`.
10. **Harden:** test with `UNSAFE` off first; add `UNSAFE` only if needed. Test on a cold boot. Check `app0:` vs `ux0:data` paths.
11. **Ship:** README with firmware requirements, install steps, controls, known issues, and a warning that homebrew is unofficial.

### 9.2 New kernel plugin

1. **Write a user-space proof of concept** if possible. If a user plugin can do it, stop.
2. **Create the plugin CMake** from §2.1: `add_executable`, `taihenForKernel_stub`, `SceSysmemForDriver_stub`, `-nostdlib`, `vita_create_self(... .skprx ... CONFIG exports.yml UNSAFE)`.
3. **Resolve the target at runtime.** Use `taiGetModuleInfoForKernel` + `taiGetModuleExportFunc`; log the module NID and function pointer.
4. **Hook one function** with no side effects: copy the path/args, log, `TAI_CONTINUE`. Verify the hook UID is `>= 0`.
5. **Load manually first.** Use `taiLoadKernelModule` + `taiStartKernelModule` from a test user plugin, not `*KERNEL`. Kernel plugins have no native per-title config section.
6. **Test cold boot with hold-L ready.** If it bootloops, hold L, remove the line, and inspect.
7. **Add cleanup.** `module_stop` releases every hook/injection; test load → unload → load again.
8. **Test on 3.60 and 3.65** if you support both. Add 3.60/3.65 NID fallbacks and module-NID validation.
9. **Add to `*KERNEL` only after it survives cold boots.** Keep a commented backup of the config on the device.
10. **Release with source, a clear risk warning, and recovery instructions.** Do not auto-update kernel plugins silently.

---

## 10. Sources

### Official / primary

- VitaSDK: <https://vitasdk.org/>
- VitaSDK docs: <https://docs.vitasdk.org/>
- `vita-toolchain`: <https://github.com/vitasdk/vita-toolchain>
- `vita.cmake`: <https://github.com/vitasdk/vita-toolchain/blob/master/cmake_toolchain/vita.cmake>
- `vita-make-fself.c`: <https://github.com/vitasdk/vita-toolchain/blob/master/src/vita-make-fself/vita-make-fself.c>
- `vita-pack-vpk.c`: <https://github.com/vitasdk/vita-toolchain/blob/master/src/vita-pack-vpk/vita-pack-vpk.c>
- `vita.toolchain.cmake`: <https://github.com/vitasdk/vita-toolchain/blob/master/cmake_toolchain/vita.toolchain.cmake>
- Samples: <https://github.com/vitasdk/samples>
- Vita headers: <https://github.com/vitasdk/vita-headers>
- 3.60 NID database: <https://github.com/vitasdk/vita-headers/tree/master/db/360>
- taiHEN docs: <https://tai.henkaku.xyz/docs/>
- taiHEN USAGE: <https://tai.henkaku.xyz/docs/md__u_s_a_g_e.html>
- taiHEN source: <https://github.com/yifanlu/taiHEN>
- taiHEN `error.h`: <https://github.com/yifanlu/taiHEN/blob/master/error.h>
- taihen-parser: <https://github.com/DaveeFTW/taihen-parser>
- Yifan Lu, "PSA for homebrew devs": <https://gist.github.com/yifanlu/3b36b987c9099b620c5b65ce8e93801d>
- xyzz, "how2homebrew": <https://gist.github.com/xyzz/cb6ad5fd3ce14e6c71cf77a9d810ce4f>

### Guides and firmware

- Vita Hacks Guide: <https://vita.hacks.guide/>
- Installing HENkaku: <https://vita.hacks.guide/installing-henkaku>
- Using HENlo: <https://vita.hacks.guide/using-henlo>
- Installing Ensō: <https://vita.hacks.guide/installing-enso>
- YAMT: <https://vita.hacks.guide/yamt>
- StorageMgr: <https://vita.hacks.guide/storagemgr>
- Troubleshooting: <https://vita.hacks.guide/troubleshooting>
- FAQ: <https://vita.hacks.guide/faq>
- Ensō FAQ: <https://enso.henkaku.xyz/>
- HENkaku usage: <https://henkaku.xyz/usage>
- h-encore: <https://github.com/TheOfficialFloW/h-encore>
- modoru: <https://github.com/TheOfficialFloW/modoru>
- VitaDeploy: <https://github.com/SKGleba/VitaDeploy>
- enso_ex: <https://github.com/SKGleba/enso_ex>
- psp2hfw: <https://github.com/SKGleba/psp2hfw>
- 0syscall6: <https://github.com/SKGleba/0syscall6>
- YAMT: <https://github.com/SKGleba/yamt-vita>

### Debugging / tooling

- vita-parse-core: <https://github.com/xyzz/vita-parse-core>
- vita-crashdump: <https://github.com/TibixDev/vita-crashdump>
- psp2_core_parse: <https://github.com/shoui520/psp2_core_parse>
- KVDB: <https://github.com/DaveeFTW/kvdb>
- vdbtcp: <https://github.com/DaveeFTW/vdbtcp>
- DebugNet: <https://github.com/psxdev/debugnet>
- Cat-A-Log: <https://github.com/isage/catlog>
- vitacompanion: <https://github.com/devnoname120/vitacompanion>
- PSVshell: <https://github.com/Electry/PSVshell>
- kubridge: <https://github.com/bythos14/kubridge>

### LiveArea / packaging

- hammerill, livearea-specs: <https://github.com/hammerill/livearea-specs>
- psdevwiki Template.xml (blocked directly; content seen via search snippets): <https://www.psdevwiki.com/vita/Template.xml>
- psdevwiki PARAM.SFO (blocked directly; content seen via search snippets): <https://www.psdevwiki.com/vita/PARAM.SFO>

### Community threads and issue reports

- `0x8010113D` on GBAtemp: <https://gbatemp.net/threads/error-0x8010113d-during-vpk-installation.12842>
- `0x8010113D` on PSX-Place: <https://www.psx-place.com/threads/error-0x8010113d-during-vpk-installation.12842>
- `0xFFFFFFFF` install failure: <https://gbatemp.net/threads/cant-install-vpk-files-error-0xffffffff.517486>
- `C2-12828-1` discussion: <https://gbatemp.net/threads/c2-12828-1-error-with-a-few-games.621812>
- 0syscall6/reF00D load order: <https://gbatemp.net/threads/repatch-ref00d-or-0syscall6-not-working.564305>
- CMake target-level dependency error: <https://gbatemp.net/threads/target-level-dependencies-to-non-existent-target.615151>
- vita-toolchain #303 (`__sce_headroom`): <https://github.com/vitasdk/vita-toolchain/issues/303>
- vita-toolchain #264 (invalid relocation / PIC): <https://github.com/vitasdk/vita-toolchain/issues/264>
- vita-toolchain #39 (system libelf): <https://github.com/vitasdk/vita-toolchain/issues/39>
- vita-toolchain #138 (pthread link): <https://github.com/vitasdk/vita-toolchain/issues/138>
- renpy-vita #12 (`0x8010113D`): <https://github.com/SonicMastr/renpy-vita/issues/12>
- VitaShell #534 (core dump path): <https://github.com/TheOfficialFloW/VitaShell/issues/534>
- livearea_nolimits firmware validation: <https://github.com/devnoname120/livearea_nolimits/blob/main/docs/firmware-validation.md>

---

## Appendix: quick error-code reference

### taiHEN return codes

| Code | Name | Meaning |
|---|---|---|
| `0x90010002` | `TAI_ERROR_NOT_FOUND` | Module/library/function NID not found, or module not loaded. |
| `0x90010003` | `TAI_ERROR_INVALID_ARGS` | Bad arguments. |
| `0x90010004` | `TAI_ERROR_INVALID_KERNEL_ADDR` | Bad kernel address. |
| `0x90010005` | `TAI_ERROR_PATCH_EXISTS` | Address already patched by another hook/injection. |
| `0x90010006` | `TAI_ERROR_HOOK_ERROR` | Hook installation failed. |
| `0x90010009` | `TAI_ERROR_NOT_ALLOWED` | No permission; often unsafe homebrew disabled. |
| `0x9001000A` | `TAI_ERROR_STUB_NOT_RESOLVED` | Import stub unresolved. |
| `0x9001000B` | `TAI_ERROR_INVALID_MODULE` | Module identity mismatch. |
| `0x9001000D` | `TAI_ERROR_BLOCKING` | Called during plugin load. |

[V] (`taiHEN/error.h`)

### Kernel/module errors

| Code | Name | Meaning |
|---|---|---|
| `0x8002D002` | `SCE_KERNEL_ERROR_MODULEMGR_IN_USE` | Module can't be unloaded (e.g. exposes syscalls). |
| `0x8002D011` | `SCE_KERNEL_ERROR_MODULEMGR_NO_MOD` | Module not found/loaded. Seen with storage mount failures. |

[V] (`psp2/kernel/error.h`)

### Installer/app errors

| Code | Meaning |
|---|---|
| `0x8010113D` | VPK asset validation failed; usually a non-indexed PNG. |
| `0x80010005` | I/O error during install (storage/card/config). |
| `0xFFFFFFFF` | Generic VitaShell/install failure. |
| `C1-6775-5` | HENkaku not active. |
| `C1-6703-6` | App/game requires higher firmware or compatibility plugin. |
| `C2-12828-1` | Application crashed; core dump may exist. |

[C] except where noted above.
