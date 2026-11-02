# 13 — Fact-Check Log

This page records what was verified, how, and what remains community-reported or
untested.

## Verification method

- Read the actual VitaSDK/VitaShell/taiHEN sources.
- Built `vita-mksfoex`, `vita-pack-vpk`, and `vita-make-fself` from the current
  `vita-toolchain` source on macOS.
- Generated an SFO and a VPK; inspected the ZIP layout and compression.
- Generated FSELFs with no flag, `-s`, and `-ss`; parsed the auth ID at `0x80`.
- Built three complete projects with the installed SDK.
- Inspected VELF import tables with `vita-elf-create -vvv`.
- Did **not** run the apps or kernel plugin on physical Vita hardware.

## Core claims

| Claim | Evidence | Result |
|---|---|---|
| VPK is a ZIP containing `eboot.bin` and `sce_sys/param.sfo` | Built `vita-pack-vpk`, packed a test VPK, `unzip -l` | **Confirmed** |
| `param.sfo` defaults: `CATEGORY=gd`, 19 entries in 2026.08 | Built `vita-mksfoex`, generated and parsed an SFO | **Confirmed** |
| Raw `vita-make-fself` default vs. CMake helper | Read `vita.cmake`; built `vita-make-fself`, parsed auth ID | **Confirmed**: raw tool defaults to `...0001`; `vita_create_self` adds `-s` and produces `...0002`; `-ss` produces `...0003` |
| VitaShell installs through `ScePromoterUtil` | Read `package_installer.c` | **Confirmed** |
| VitaShell extracts to `ux0:data/pkg` | Read `package_installer.h` | **Confirmed** |
| LiveArea sizes 128×128, 840×500, 280×158, 960×544 | Inspected sample PNGs with `sips` | **Confirmed** |
| Toolchain uses ARMv7-A, Cortex-A9, NEON, hard-float, Thumb | Read `buildscripts/cmake/recipes/GccCommonArgs.cmake` | **Confirmed** |
| taiHEN disables signature checks, exposes kernel peek/poke, provides hooks | Read taiHEN README and `taihen.h` | **Confirmed** |
| HENkaku is 3.60-only and non-persistent | Official henkaku.xyz | **Confirmed** |
| Enso is permanent; 3.65 builds exist | Enso site + current Vita Hacks Guide | **Confirmed with nuance** |
| The workspace console is on 3.65 Enso but its version string is spoofed to 3.74 | User-confirmed on the hardware | **Confirmed** — the version string is a display value; NIDs and offsets follow 3.65 |
| h-encore supports 3.65/3.67/3.68; h-encore² supports 3.65–3.74 | Official READMEs | **Confirmed** |
| HENlo supports 3.65/3.68/3.74 | Current Vita Hacks Guide | **Confirmed** |
| 3.74 is the final firmware | Wikipedia system software page | **Confirmed** |
| Vita has 512 MB RAM + 128 MB VRAM, 960×544, Cortex-A9, SGX543MP4+ | Wikipedia | **Confirmed** |
| No official debugger; core dumps in `ux0:data` | vitasdk.org | **Confirmed** |
| vitaGL requires `libshacccg.suprx` | vitaGL README | **Confirmed** |
| `SceIoDevInfo` is declared in the SDK, 24 bytes | `psp2common/kernel/iofilemgr.h`: two `SceOff`, one `SceSize`, one pointer; `VITASDK_BUILD_ASSERT_EQ(0x18, …)` | **Confirmed** — corrects the earlier `[C]` "16-byte community layout" note |
| Device model codes are an SDK enum | `psp2common/kernel/sysmem.h:63-64`: `SCE_KERNEL_MODEL_VITA = 0x10000`, `SCE_KERNEL_MODEL_VITATV = 0x20000` | **Confirmed** — corrects the earlier `[C]` "constants absent" note |
| `sceIoDevctl("ux0:", 0x3001, …)` is the device-info command | `psp2/io/devctl.h` shows the call verbatim in its example | **Confirmed** |
| Its results are byte counts | The unit is documented nowhere; only community code | **Unconfirmed** |
| `vita2d_draw_array` requires GPU-mapped vertex memory | `arm-vita-eabi-objdump -d libvita2d.a`: it calls `sceGxmSetVertexStream(ctx, 0, vertices)` with the caller's pointer, no copy. `vita2d_draw_rectangle`/`_line` allocate from `vita2d_pool_memalign` first | **Confirmed on hardware** — a static buffer faulted the GPU on the first frame |
| `sceAppMgrGetDevInfo` reports `ux0:` capacity under a safe eboot | Safe build showed `17.7/119.3 GB` on a 128 GB card | **Confirmed on hardware** |
| `sceAppMgrGetDevInfo` does not cover `uma0:` | `uma0:` fell through to the devctl fallback | **Confirmed on hardware** — rendered `ERR 30` |
| `sceIoDevctl("ux0:", 0x3001, …)` needs the unsafe authid | Safe build returned `0x80010030` on a mounted card; the `UNSAFE` build returned a size | **Confirmed on hardware** |
| A GPU vertex fetch from CPU stack memory faults on the stack page | `psp2-gpucrash-parse 0.8.0` on `psp2core-1793536179` and `psp2core-1793536348`: both BIF faults are PDS vertex/uniform fetches at `stack base + 0x3f000`, the last 4 KiB of the 256 KiB main thread stack; `address` reports `THREAD_REFERENCES … stack+0x3f000` | **Confirmed** — two independent COCKPIT captures with different stack bases |
| taiHEN `*ALL` loads a user plugin into every non-kernel process | `taihen-parser/src/parser.c`: `TOKEN_ALL_SECTION = "ALL"`; `taihen_config_parse()` matches it whenever the target section is not `KERNEL` | **Confirmed** — source |
| taiHEN cannot hook shared-library exports or imports | `taiHEN/USAGE.md`, "Hooking shared libraries": shared libraries live at `0xE0000000`; hook the importing module's stub instead | **Confirmed** — source |
| `vita2d_draw_array` ignores the `sceGxmSetVertexStream` return | `arm-vita-eabi-objdump -d libvita2d.a`: the call is followed unconditionally by `sceGxmDraw`; no return check | **Confirmed** — disassembly |

## Project verification

### `vita-cap-test`

| Check | Result |
|---|---|
| Build | Success |
| Auth ID | `0x2F00000000000001` (unsafe) |
| VPK | 2.6 MB, 6 files |
| Imports | `SceAudio`, `SceIofilemgr`, `SceNet`, `SceNetCtl`, `SceSysmodule`, `SceLibKernel` |
| Runtime | Not tested on hardware |

### `vita-kernel-demo`

| Check | Result |
|---|---|
| Kernel `.skprx` build | Success, 4.8 KB |
| App build | Success, 74 KB VPK |
| Kernel imports | `taihenForKernel`, `SceSysmemForDriver` |
| App imports | `KernelDemo :: kdemo_get_open_count`, `kdemo_get_last_path` |
| Auth ID | `0x2F00000000000001` for both app and plugin |
| Runtime | Not tested on hardware |

### `vita-cockpit`

| Check | Result |
|---|---|
| App build | Success, 112.1 KB VPK (b35) |
| Auth ID | `0x2F00000000000001` (UNSAFE default, D42); `0x2F00000000000002` with `--safe` |
| Imports | `ScePower`, `SceSysmem`, `SceIofilemgr`, `SceKernelModulemgr`, `SceRtc`, `SceCtrl`, `SceLibKernel`; `SceDisplay`, `SceGxm`, `SceSysmodule`, `SceAppMgr`, `SceCommonDialog` via libvita2d |
| Host tests | 316 checks, 0 failures (`tools/test/run.sh`) |
| Plugin list | `[V]` a real 19-entry config parses 19/19 and groups as KERNEL 9 / SYSTEM 8 / SYSTEM APPS 1 / ALL TITLES 1; tests cover BOM, comments, long names, section mapping, priority merge, the 64-entry ceiling and its notice, and a path before the first section as a warning |
| Plugin editor | `[V]` host tests cover ux0 precedence, empty config, full-path duplicate rows, `*!` halts, lock and warn decisions, partial enable, missing active/inactive controls, per-line pending, and exact `#` round trips with CRLF, indentation, no final newline, and double comments |
| Snapshots | `[V]` host tests cover `config.txt.original` created once and `config.txt.session` refreshed each session; a failed snapshot warns but leaves the editor writable; `boot_config.txt` is byte-identical with no backup file |
| Config write | `[C]` the adapter's verified rename-away swap is implemented and builds; host tests exercise the same transaction and its restore path in the fake, not the SDK calls, and the target's rename semantics are unverified |
| Host preview render | 13 states rendered through the shipped composition (`tools/preview/run.sh`), busiest 5,793 draw calls |
| Tabs and summary | `[V]` host preview: HOME/PLUGINS tabs and the permanent label-scale `N PLUGINS ONLINE` summary share the 64 px header; the hazard band is below it (D63); HOME keeps both panel rows at scroll 0 and has no scrollbar |
| Pending baseline | `[C]` session-scoped: relaunching COCKPIT before the reboot re-reads the edited file as the new baseline (D59) |
| Plugin descriptions | 155 rows: every AutoPlugin2 Vita catalog path plus iTLS, libshacccg, YAMT and HENkaku; 10 rows marked `[C]` for missing or corrected text |
| Runtime | `[V]` 2026-11-02 at b13: all panels render on the 3.65 console; b35 is built and host-tested, pending the fixed tabbed check |
| `SceIoDevInfo` | Uses the SDK type, 24 bytes, not the 16-byte community struct |
| Model string | Reports `PSVITA`, because `sceKernelGetModel()` cannot tell PCH-1000 from PCH-2000 |

## Community claims / caveats

These are widely reported but not all verified against primary sources:

| Claim | Status | Note |
|---|---|---|
| Non-indexed PNGs cause `0x8010113D` at 99% | Strong community report | Indexed/palette is the safe choice |
| `pic0.png` is mandatory | Community report | Official sample lacks it; include it anyway |
| `bg0.png` is mandatory | **False** | `template.xml` names the file; `bg.png` works |
| Title ID must be 4 letters + 5 digits | **False** | 9 chars; `VITASHELL` is valid |
| `UNSAFE` is required for an eboot to run | **False** | Safe eboots run normally |
| 3.60 plugins work on 3.65 | **Not reliable** | Module/library NIDs and offsets differ |
| `ux0:tai` is the modern path | **Outdated** | `ur0:tai`; stale `ux0:tai` shadows it |
| Rebuild Database fixes every bootloop | **Overgeneralized** | It can restore boot but wipes LiveArea layout/messages/playlists |
| Format the memory card to fix crashes | **Bad advice** | Destructive; check plugins/config first |
| Sony bans ordinary homebrew users | **Not observed** | ToS still prohibits modification; trophy/online cheating are the cited risks |

## Source-quality warning

Several 2025–2026 “PS Vita dev wiki” GitHub repos and blog posts are
LLM-generated compilations. They contain confidently wrong claims, including:

- “`UNSAFE` is required for a SELF to run.”
- “Title ID must be 4 uppercase letters + 5 digits.”
- “LiveArea background must be `bg0.png`.”
- “PNG must be exactly 8-bit.”

When a claim can be checked in `vita.cmake`, `vita-make-fself.c`, the headers, or
a real project's source, check it. Prefer primary sources and reproducible
builds.

## What was not tested

- On-device launch/install of the three VPKs.
- On-device kernel plugin load/hook behavior on 3.65.
- Audio output, WiFi/DNS/HTTP on the physical Vita.
- 3.60 vs 3.65 NID differences for every API used.
- `vitaGL`/`libshacccg.suprx` workflow.
- Camera, microphone, video, Bluetooth.

These are documented from sources and community reports, not measured here.

## Reproducible verification commands

```sh
# SFO fields
python3 - <<'PY'
import struct
d=open('build/app.vpk_param.sfo','rb').read()
magic,ver,keyofs,valofs,count=struct.unpack_from('<IIIII',d,0)
for i in range(count):
    no,al,ty,vs,ts,do=struct.unpack_from('<HBBIII',d,0x14+i*0x10)
    name=d[keyofs+no:d.index(b'\0',keyofs+no)].decode()
    val=d[valofs+do:valofs+do+vs]
    if ty==2: val=val.rstrip(b'\0').decode()
    else: val=struct.unpack('<I',val)[0]
    print(name, '=', repr(val))
PY

# Auth ID
python3 - <<'PY'
import struct
d=open('build/app.self','rb').read(0x100)
print(hex(struct.unpack_from('<Q',d,0x80)[0]))
PY

# VELF type
arm-vita-eabi-readelf -h build/app.velf | grep Type

# VPK layout
unzip -l build/app.vpk
```
