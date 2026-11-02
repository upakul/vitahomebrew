# 10 — Troubleshooting

## Build errors

| Error | Cause | Fix |
|---|---|---|
| `The C compiler identification is unknown` | `VITASDK` not set / toolchain file not found | Export `VITASDK`; pass `-DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake` |
| `arm-vita-eabi-gcc not found` | `$VITASDK/bin` not on `PATH` | `export PATH="$VITASDK/bin:$PATH"` |
| `vita2d.h: No such file or directory` | `libvita2d` not installed | `vdpm install libvita2d` |
| `Invalid version '1.0' change to format ##.##` | `VITA_VERSION` regex | Use `01.00` |
| `Cannot allocate N bytes for SCE data at end of segment 0; segment 1 overlaps` | Text/RW segments too tight | Add `-Wl,--defsym=__sce_headroom=0x10000` |
| `Invalid relocation type 25!` | PIC/PIE | `set(CMAKE_POSITION_INDEPENDENT_CODE OFF)`; no `-fPIC`/`-pie` |
| `dest = elf_begin(...) failed` / `vita-elf-create` segfault | Built against system `libelf` (not 0.8.13) | Use prebuilt VitaSDK or libelf 0.8.13 |
| `Unable to find module with NID ...` | Linked a real library instead of `*_stub` | Link `libSce*_stub.a`; don't mix SDK versions |
| `undefined reference to pthread_mutex_lock` | Old SDK/newlib regression | Update VitaSDK; link `pthread` |
| `No rule to make target 'foo.vpk'` | CMake ≥ 3.20 target is `foo.vpk-vpk` | Build `foo.vpk-vpk` or just `cmake --build build` |
| `add_dependencies ... non-existent target foo.skprx` | Target is `foo.skprx-self` | Depend on `foo.skprx-self` |
| `Could not find symbol 'module_start' for main export 'start'` | Kernel `CONFIG exports.yml` leaked into app via cache | Reset `VITA_ELF_CREATE_FLAGS`/`VITA_MAKE_FSELF_FLAGS` before app |
| `vita-pack-vpk: .sfo file missing` / `.bin file missing` | Wrong `vita_create_vpk` args | Use `vita_create_vpk(target titleid eboot ...)` |
| `Error: cannot stat '...'` | Bad `FILE src dst` path | Fix source path; `FILE` takes two args |
| `error: unknown type name 'SceUID'` | Missing include | `#include <psp2/types.h>` or `vitasdk.h` |
| Linker warning `crtn.o: missing .note.GNU-stack` | Toolchain artifact | Harmless; ignore |

## Install errors

| Code / symptom | Meaning | Fix |
|---|---|---|
| `0x8010113D` at 99% | Asset validation; usually non-indexed PNG | Re-encode `sce_sys` PNGs as indexed/palette (`pngquant`) |
| `0x80010005` | I/O error during install | Free space, card, stale config; reboot |
| `0xFFFFFFFF` | Generic install failure | Inspect ZIP: `eboot.bin` + `sce_sys/param.sfo` at root |
| `C2-12828-1` | App crashed at launch | Check unsafe setting/dependencies; pull `.psp2dmp` |
| `C1-6775-5` | HENkaku/taiHEN not active | Re-run exploit or boot Enso |
| `C1-6703-6` | App needs higher firmware / compatibility plugin | Load `0syscall6`/`reF00D` under `*KERNEL` |
| `0x8002D011` | `MODULEMGR_NO_MOD`; required module missing | Reboot, reset taiHEN config, check storage driver |
| `0x8002D002` | `MODULEMGR_IN_USE`; can't unload | Module exposes syscalls; use override or leave loaded |
| Install fails while app runs | Old process holds app dir | Close app, retry |
| VPK > 4 GiB on FAT32 | FAT32 limit | exFAT or split |
| Title ID collision | Installer replaces existing app | Delete old bubble or new title ID |

## Runtime / app errors

| Symptom | Likely cause | Fix |
|---|---|---|
| App exits immediately | `main` returns / `sceKernelExitProcess` | Check logic; add a loop |
| Black screen | No `vita2d_swap_buffers` / draw not started | Check frame loop |
| Text missing | Font not loaded / PGF failed | `vita2d_load_default_pgf()`; check null |
| Audio open fails | Port busy / wrong type | Use `SCE_AUDIO_OUT_PORT_TYPE_BGM`; release port |
| Audio silence | Wrong format/sample count | Mono/stereo matches buffer; 64–65472 samples |
| File open fails | Safe sandbox / wrong path | Use `ux0:data`; `UNSAFE` only if needed |
| `sceIoMkdir` fails | Directory exists | Ignore `EEXIST` |
| Network init fails | `SceNet` module not loaded / memory | `sceSysmoduleLoadModule(SCE_SYSMODULE_NET)`; 1 MB buffer |
| `gethostbyname` fails | No WiFi/DNS | Check Vita network; use IP fallback |
| `connect` hangs | No route / blocked port | Add timeout; test `example.com:80` |
| App can't see `ux0:` | Safe eboot | Use `UNSAFE` or restrict to allowed paths |

## Plugin errors

| Symptom | Likely cause | Fix |
|---|---|---|
| Plugin not loading | Stale `ux0:tai/config.txt` shadows `ur0:tai` | Delete `ux0:tai`; use one config |
| Plugin not loading | Wrong section/case/path | `*KERNEL` uppercase; path must exist |
| Plugin not loading | Kernel plugin needs reboot | Reboot; user plugins need Refresh taiHEN config |
| Plugin loads but hook does nothing | Wrong NID/module | Check `taiHookFunction*` return; verify NID |
| Bootloop | Bad `*KERNEL` plugin | Hold **L**; remove line; Safe Mode |
| Crash in hook | User pointer dereference / recursion / stack | Copy pointers; no recursion; small frames |
| `TAI_ERROR_NOT_FOUND` (`0x90010002`) | Module/library/function not found | Check module loaded and NID |
| `TAI_ERROR_NOT_ALLOWED` (`0x90010009`) | Unsafe homebrew disabled / permissions | Enable unsafe; check taiHEN |
| `0x8002D002` on unload | Module exposes syscalls | Use modulemgr override or leave loaded |
| App can't load `.skprx` | Not unsafe / path wrong | Enable unsafe; `app0:...` path; check VPK contents |

## FTP / transfer

| Symptom | Fix |
|---|---|
| Connection refused | VitaShell FTP not started (press Select) |
| Wrong IP | Check VitaShell screen; same network |
| Upload hangs | Firewall/AP isolation; try USB |
| `curl: (67) Login denied` | VitaShell FTP is anonymous; don't pass a user |
| File not visible | Refresh VitaShell; check `ux0:data` |

## Diagnostic commands

```sh
# Toolchain
arm-vita-eabi-gcc --version
vdpm status
vdpm list

# Build artifacts
arm-vita-eabi-readelf -h build/app.velf
unzip -l build/app.vpk
file build/app.self

# SELF authid
python3 - <<'PY'
import struct
d=open('build/app.self','rb').read(0x100)
print(hex(struct.unpack_from('<Q',d,0x80)[0]))
PY

# Kernel plugin imports
cd build
vita-elf-create -vvv myplugin myplugin_debug.velf 2>&1 | grep library_nid
```

## Recovery quick reference

1. Hold **L** at boot.
2. Safe Mode: power off, hold **R + Power + PS**.
3. Restart → Rebuild Database → Restore System.
4. Delete `ur0:tai/boot_config.txt` to deactivate Enso (use installer to fix).
5. Keep a backup of `ur0:tai/config.txt`.
