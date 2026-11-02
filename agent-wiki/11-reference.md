# 11 — Reference

## NID quick table (3.60 database)

Function NIDs are often stable across firmware, but library/module NIDs and
offsets are not guaranteed. Verify against your target firmware.

### File I/O

| Module | Function | NID |
|---|---|---|
| `SceIofilemgr` | `ksceIoOpen` | `0x75192972` |
| `SceIofilemgr` | `_sceIoOpen` | `0xCC67B6FD` |
| `SceIofilemgr` | `ksceIoRead` | `0xE17EFC03` |
| `SceIofilemgr` | `ksceIoWrite` | `0x21EE91F0` |
| `SceIofilemgr` | `ksceIoClose` | `0xF99DD8A3` |
| `SceIofilemgr` | `sceIoRead` | `0xFDB32293` |
| `SceIofilemgr` | `sceIoWrite` | `0x34EFD876` |
| `SceIofilemgr` | `sceIoClose` | `0xC70B8886` |
| `SceLibKernel` | `sceIoOpen` | `0x6C60AC61` |

### Audio

| Module | Function | NID |
|---|---|---|
| `SceAudio` | `sceAudioOutOpenPort` | `0x5BC341E4` |
| `SceAudio` | `sceAudioOutOutput` | `0x02DB3F5F` |

### Networking

| Module | Function | NID |
|---|---|---|
| `SceNet` | `sceNetInit` | `0xEB03E265` |
| `SceLibNetCtl` | `sceNetCtlInetGetInfo` | `0xB26D07F3` |

### taiHEN

| API | Purpose |
|---|---|
| `taiHookFunctionExportForKernel` | Hook a kernel module export |
| `taiHookFunctionImportForKernel` | Hook a kernel module import |
| `taiHookFunctionOffsetForKernel` | Hook by module/segment/offset |
| `taiHookFunctionAbs` | Hook an absolute address |
| `taiHookReleaseForKernel` | Release a hook |
| `taiInjectDataForKernel` | Inject data into a module |
| `taiInjectReleaseForKernel` | Release an injection |
| `taiGetModuleInfoForKernel` | Get module info |
| `taiGetModuleExportFunc` | Resolve an export by NID |
| `taiLoadKernelModule` | Load a `.skprx` |
| `taiStartKernelModuleForUser` | Start a loaded module |
| `taiLoadStartKernelModule` | Load + start (user helper) |
| `taiStopUnloadKernelModule` | Stop + unload (may fail) |
| `taiMemcpyUserToKernel` | Copy user→kernel |
| `taiMemcpyKernelToUser` | Copy kernel→user |

## API map

| Area | Header | Stub | Key calls |
|---|---|---|---|
| Threads | `psp2/kernel/threadmgr.h` | `SceKernelThreadMgr_stub` | `sceKernelCreateThread`, `sceKernelStartThread`, `sceKernelDelayThread` |
| Process | `psp2/kernel/processmgr.h` | `SceLibKernel_stub` | `sceKernelExitProcess` |
| Memory | `psp2/kernel/sysmem.h` | `SceSysmem_stub` | `sceKernelAllocMemBlock`, `sceKernelGetMemBlockBase` |
| Display | `psp2/display.h` | `SceDisplay_stub` | framebuffer, vsync |
| GPU | `psp2/gxm.h` | `SceGxm_stub` | shaders, textures, render targets |
| 2D | `vita2d.h` | `vita2d` | textures, shapes, PGF/PVF text |
| Input | `psp2/ctrl.h` | `SceCtrl_stub` | `sceCtrlPeekBufferPositive` |
| Touch | `psp2/touch.h` | `SceTouch_stub` | `sceTouchPeek` |
| Audio out | `psp2/audioout.h` | `SceAudio_stub` | `sceAudioOutOpenPort`, `sceAudioOutOutput` |
| Audio in | `psp2/audioin.h` | `SceAudioIn_stub` | `sceAudioInOpenPort` |
| Camera | `psp2/camera.h` | `SceCamera_stub` | `sceCameraOpen`, `sceCameraRead` |
| Motion | `psp2/motion.h` | `SceMotion_stub` | `sceMotionGetState` |
| Net | `psp2/net/net.h` | `SceNet_stub` | `sceNetInit`, `sceNetSocket`, ... |
| NetCtl | `psp2/net/netctl.h` | `SceNetCtl_stub` | `sceNetCtlInit`, `sceNetCtlInetGetInfo` |
| HTTP | `psp2/net/http.h` | `SceHttp_stub` | `sceHttpInit`, ... |
| TLS | `psp2/libssl.h` | `SceSsl_stub` | `sceSslInit`, ... |
| Files | `psp2/io/fcntl.h` | `SceIofilemgr_stub` | `sceIoOpen`, `sceIoRead`, `sceIoWrite` |
| App util | `psp2/apputil.h` | `SceAppUtil_stub` | save data, system params |
| Dialogs | `psp2/common_dialog.h` | `SceCommonDialog_stub` | message/IME/save dialogs |
| Power | `psp2/power.h` | `ScePower_stub` | clock, battery |
| RTC | `psp2/rtc.h` | `SceRtc_stub` | date/time |
| Sysmodule | `psp2/sysmodule.h` | `SceSysmodule_stub` | load/unload system modules |
| Promoter | `psp2/promoterutil.h` | `ScePromoterUtil_stub` | package promotion |
| SQLite | `psp2/sqlite.h` | `SceSqlite_stub` | system SQLite |
| JSON | `psp2/json.h` | `SceLibJson_stub` | system JSON |

## File formats

| Format | Meaning |
|---|---|
| `.elf` | Normal ARM ELF (compiler output) |
| `.velf` | Sony ELF (`ET_SCE_RELEXEC`, `0xFE04`) with module/import/export tables |
| `.self` | Signed Sony container; homebrew uses unsigned FSELF |
| `eboot.bin` | The app's FSELF |
| `.suprx` | Userland PRX/library/plugin |
| `.skprx` | Kernel PRX/module/plugin |
| `.vpk` | ZIP package with `eboot.bin` + `sce_sys/param.sfo` + assets |
| `.sfo` | System metadata (`param.sfo`) |
| `.psp2dmp` | User app core dump |
| `.psp2kerndmp` | Kernel dump |
| `.spsp2dmp` | Encrypted system dump container (community) |

## SELF header

| Offset | Field |
|---|---|
| `0x00` | magic `0x454353` (`SCE\0`) |
| `0x04` | version 3 |
| `0x80` | auth ID (`uint64_t`) |

Auth IDs:

```text
0x2F00000000000001  unsafe/full
0x2F00000000000002  safe
0x2F00000000000003  secret-safe
```

## VPK layout

```text
sce_sys/param.sfo
eboot.bin
sce_sys/icon0.png
sce_sys/pic0.png
sce_sys/livearea/contents/bg.png
sce_sys/livearea/contents/startup.png
sce_sys/livearea/contents/template.xml
[optional data files]
[optional .skprx / .suprx]
```

## `param.sfo` fields

| Key | Type | Notes |
|---|---|---|
| `APP_VER` | string | `00.00` (raw tool; `vita_create_vpk` sets it to `VITA_VERSION`) |
| `ATTRIBUTE` | dword | `0x8000` default |
| `ATTRIBUTE2` | dword | `0`; `12` = enlarged memory |
| `ATTRIBUTE_MINOR` | dword | `0x10` |
| `BOOT_FILE` | string | empty |
| `CATEGORY` | string | `gd` |
| `CONTENT_ID` | string | `HB0001-<TID>_00-...` |
| `EBOOT_APP_MEMSIZE` | dword | `0` |
| `EBOOT_ATTRIBUTE` | dword | `0` |
| `EBOOT_PHY_MEMSIZE` | dword | `0` |
| `LAREA_TYPE` | dword | `0` |
| `NP_COMMUNICATION_ID` | string | empty |
| `PARENTAL_LEVEL` | dword | `0` |
| `PSP2_DISP_VER` | string | `00.000` |
| `PSP2_SYSTEM_VER` | dword | `0` |
| `STITLE` | string | title |
| `TITLE` | string | title |
| `TITLE_ID` | string | 9 chars |
| `VERSION` | string | `00.00` default |

## Error codes

### taiHEN

| Code | Name | Meaning |
|---|---|---|
| `0x90010002` | `TAI_ERROR_NOT_FOUND` | Module/library/function not found |
| `0x90010003` | `TAI_ERROR_INVALID_ARGS` | Bad args |
| `0x90010004` | `TAI_ERROR_INVALID_KERNEL_ADDR` | Bad kernel address |
| `0x90010005` | `TAI_ERROR_PATCH_EXISTS` | Already patched |
| `0x90010006` | `TAI_ERROR_HOOK_ERROR` | Hook failed |
| `0x90010009` | `TAI_ERROR_NOT_ALLOWED` | Permission; often unsafe disabled |
| `0x9001000A` | `TAI_ERROR_STUB_NOT_RESOLVED` | Import stub unresolved |
| `0x9001000B` | `TAI_ERROR_INVALID_MODULE` | Module mismatch |
| `0x9001000D` | `TAI_ERROR_BLOCKING` | Called during plugin load |

### Kernel/module

| Code | Name | Meaning |
|---|---|---|
| `0x8002D002` | `SCE_KERNEL_ERROR_MODULEMGR_IN_USE` | Can't unload (e.g. syscalls) |
| `0x8002D011` | `SCE_KERNEL_ERROR_MODULEMGR_NO_MOD` | Module not found/loaded |

### Installer/app

| Code | Meaning |
|---|---|
| `0x8010113D` | VPK asset validation; usually non-indexed PNG |
| `0x80010005` | I/O error during install |
| `0xFFFFFFFF` | Generic VitaShell/install failure |
| `C1-6775-5` | HENkaku not active |
| `C1-6703-6` | Higher firmware/compat plugin needed |
| `C2-12828-1` | App crashed; core dump may exist |

## Filesystem mount points

| Path | Meaning |
|---|---|
| `ux0:` | Memory card |
| `ur0:` | Internal user partition |
| `imc0:` | Internal memory card (PCH-2000) |
| `app0:` | Installed app directory (read-only) |
| `savedata0:` | App save data |
| `gro0:` | Game card |
| `uma0:` | USB storage |
| `vs0:` / `os0:` | System partitions (protected) |

These are default mounts. YAMT/StorageMgr can remap `ux0:` and `uma0:` — for
example, SD2Vita as `ux0:` and the Sony card as `uma0:`.

## Environment

| Variable / path | Meaning |
|---|---|
| `VITASDK` | SDK root (`vitasdk`) |
| `$VITASDK/bin` | Host tools + cross-compiler |
| `$VITASDK/arm-vita-eabi` | Target sysroot |
| `$VITASDK/share/vita.cmake` | CMake macros |
| `$VITASDK/share/vita.toolchain.cmake` | CMake toolchain |
| `$VITASDK/share/vita-headers/db` | NID databases |

## Glossary

| Term | Meaning |
|---|---|
| **SELF / FSELF** | Sony executable container; FSELF is unsigned homebrew |
| **VELF** | Sony relocatable ELF with SCE module/import/export tables |
| **VPK** | Community ZIP package |
| **NID** | 32-bit function/library identifier |
| **Stub** | Import library generated from NID DB |
| **taiHEN** | CFW framework: signature bypass, hooks, kernel module loading |
| **Enso** | Permanent CFW boot loader |
| **HENkaku** | 3.60 homebrew enabler |
| **HENlo** | Browser exploit for 3.65/3.68/3.74 |
| **h-encore²** | Exploit for 3.65–3.74 |
| **VitaDeploy** | All-in-one CFW/installer toolbox |
| **YAMT** | SD2Vita/USB mount tool |
| **SceShell** | LiveArea shell |
| **SceAppMgr** | App lifecycle manager |
| **ScePromoterUtil** | Package promotion/install service |
| **PGF/PVF** | Vita font formats |
| **GXM** | Vita GPU API |
| **vita2d** | GPU 2D library |
| **vitaGL** | OpenGL subset over GXM |
| **libshacccg.suprx** | Sony shader compiler module used by vitaGL |
