# 06 — Kernel Plugins (taiHEN)

This is the deep layer: a `.skprx` that runs in kernel space, hooks or patches
kernel functions, and can expose a syscall library to a userland app.

**Warning:** kernel code can crash or brick the Vita. Read the safety checklist
at the end before running anything.

## What taiHEN provides

From the taiHEN README:

1. Disables code signature checks to allow unsigned executables.
2. Exposes kernel peek/poke syscalls and kernel module loading to user apps.
3. Provides a function hooking/replacing API based on `substitute`.

It is loaded by HENkaku/h-encore/HENlo; Enso makes it load at boot.

## `.suprx` vs `.skprx`

| | User plugin `.suprx` | Kernel plugin `.skprx` |
|---|---|---|
| Runs in | A user process (game/app/shell) | Kernel (`KERNEL_PID = 0x10005`) |
| Config section | Title ID, `*ALL`, `*main` | `*KERNEL` |
| Can hook | Imports of the target process; exports in that process | Kernel module exports/imports |
| Cleanup | taiHEN cleans user patches on process exit; still release hooks | Nothing auto-cleans; release in `module_stop` |
| Per-title load | Yes | No native per-title support; load on demand via `taiLoadKernelModule` |
| Reload | HENkaku Settings → Refresh taiHEN config | Reboot |

## Config file: `config.txt`

### Path precedence

taiHEN opens `ux0:tai/config.txt` first; if it fails, it opens
`ur0:tai/config.txt`.

**A stale `ux0:tai/config.txt` shadows `ur0:tai/config.txt`.** This is the most
common "my plugin isn't loading" cause. Recommended modern layout: keep one
config in `ur0:tai` and delete `ux0:tai` entirely.

### Format

```text
# comment
*KERNEL
ur0:tai/plugin1.skprx
ur0:tai/plugin2.skprx

*main
ur0:tai/shell_plugin.suprx

*NPXS10015
ur0:tai/settings_plugin.suprx

*ALL
ur0:tai/global_user_plugin.suprx

*PCSE00001
ur0:tai/game_specific.suprx
```

Rules:

- `#` starts a comment.
- `*` starts a section.
- Any other non-whitespace starts a path.
- Section names are case-sensitive: `*KERNEL`, `*ALL`, `*main` (lowercase for
  SceShell).
- Paths must belong to a section; a path before the first `*` makes the config
  invalid.
- Load order is top-to-bottom within a section; dependencies first.
- `*!Section` is a halt point (parsing stops), useful for blacklisting.
- After editing: user plugins → HENkaku Settings → Refresh taiHEN config;
  kernel plugins → reboot.

### `boot_config.txt` is not `config.txt`

Enso reads `ur0:tai/boot_config.txt` at the bootloader stage. It is
model-specific and dangerous to edit. Use the Enso installer's "fix boot config"
option, not a text editor. Corrupting it can prevent boot.

## Loading a kernel plugin

### At boot

Add to `*KERNEL` in `ur0:tai/config.txt`:

```text
*KERNEL
ur0:tai/myplugin.skprx
```

Reboot. This is how system-wide plugins load. A bad plugin causes a bootloop;
hold **L** at boot to skip taiHEN plugin loading.

### On demand

From a user app or user plugin, with unsafe homebrew enabled:

```c
#include <taihen.h>

SceUID modid = taiLoadStartKernelModule(
    "app0:myplugin.skprx", 0, NULL, 0);

if (modid < 0) {
    // error; see taiHEN return codes
}
```

`taiLoadStartKernelModule` is a helper around
`taiLoadStartKernelModuleForUser`. It loads and starts the module.

The `vita-kernel-demo` uses this path: the `.skprx` is packaged inside the VPK
and loaded from `app0:kernel_demo.skprx`. A reboot clears it because it is not
in `config.txt`.

## Hooks

Core API (from `taihen.h`):

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

User-space equivalents: `taiHookFunctionExport`, `taiHookFunctionImport`,
`taiHookFunctionOffset`, `taiHookRelease`.

### Rules that prevent most crashes

1. **Check every return value.** A wrong NID/module returns
   `TAI_ERROR_NOT_FOUND` (`0x90010002`). If you ignore it and call
   `TAI_CONTINUE` on an uninitialized ref, you jump through garbage.
2. **Always call `TAI_CONTINUE`**, even when overriding the return value. It
   preserves the hook chain for other plugins.
3. **Match the prototype exactly.** taiHEN does not type-check. A wrong
   signature corrupts the stack.
4. **Shared-library exports cannot be hooked** (modules in the `0xE0000000`
   range, e.g. `SceLibKernel`, `SceGxm`). Hook the imports of the modules that
   use them.
5. **Weak imports** (loaded on demand via `sceSysmoduleLoadModule` /
   `sceKernelLoadStartModule`) need load/unload hooks: install after load,
   release before unload.
6. **Hooking a function while it is being called can crash** (taiHEN bug #12).
   Avoid hot-patching tight loops; hook at module start.
7. **Never dereference user pointers in kernel mode.** Copy with
   `ksceKernelStrncpyUserToKernel` / `taiMemcpyUserToKernel`, bound the length.
8. **No recursion.** If you hook `ksceIoOpen` and then call `ksceIoOpen` or a
   logging path that re-enters it, you recurse. Use an in-memory ring buffer and
   flush from a thread.
9. **Keep stack frames small.** The syscall stack is 4 KiB.

### Kernel hook example — count file opens

This is the pattern used by `vita-kernel-demo`:

```c
#include <psp2kern/types.h>
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/sysmem/data_transfers.h>
#include <taihen.h>

#define SCE_IO_OPEN_NID 0xCC67B6FD   /* SceIofilemgr::_sceIoOpen */

static tai_hook_ref_t open_hook;
static SceUID open_hook_uid = -1;
static volatile int open_count;
static char last_path[256];

static SceUID hook_sceIoOpen(const char *path, int flags, SceMode mode,
                             void *args)
{
    if (path) {
        ksceKernelStrncpyUserToKernel(last_path, path, sizeof(last_path));
        last_path[sizeof(last_path) - 1] = '\0';
    }

    SceUID fd = TAI_CONTINUE(SceUID, open_hook, path, flags, mode, args);

    if (fd >= 0)
        open_count++;

    return fd;
}

int module_start(SceSize args, void *argp)
{
    open_hook_uid = taiHookFunctionExportForKernel(
        KERNEL_PID, &open_hook, "SceIofilemgr",
        TAI_ANY_LIBRARY, SCE_IO_OPEN_NID, hook_sceIoOpen);

    if (open_hook_uid < 0)
        return SCE_KERNEL_START_FAILED;

    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize args, void *argp)
{
    if (open_hook_uid >= 0) {
        taiHookReleaseForKernel(open_hook_uid, open_hook);
        open_hook_uid = -1;
    }
    return SCE_KERNEL_STOP_SUCCESS;
}
```

### User-space import hook example — redirect one app's open

```c
#include <taihen.h>
#include <string.h>

static tai_hook_ref_t open_ref;
static SceUID open_hook_uid = -1;

static SceUID open_hook(const char *path, int flags, SceMode mode)
{
    if (strcmp(path, "ux0:data/myapp/config.ini") == 0)
        path = "ux0:data/myapp/config.default.ini";

    return TAI_CONTINUE(SceUID, open_ref, path, flags, mode);
}

int module_start(SceSize argc, const void *args)
{
    open_hook_uid = taiHookFunctionImport(&open_ref, "MyGame",
                                          0xCAE9ACE6, 0x6C60AC61, open_hook);
    if (open_hook_uid < 0)
        return SCE_KERNEL_START_FAILED;

    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args)
{
    if (open_hook_uid >= 0) {
        taiHookRelease(open_hook_uid, open_ref);
        open_hook_uid = -1;
    }
    return SCE_KERNEL_STOP_SUCCESS;
}
```

### Offset hook — only after validation

```c
tai_module_info_t info = {0};
info.size = sizeof(info);

/* SceIofilemgr is a kernel module, so KERNEL_PID is valid here.
 * SceShell is userland; use taiGetModuleInfo() for it, not the kernel API. */
int ret = taiGetModuleInfoForKernel(KERNEL_PID, "SceIofilemgr", &info);
if (ret < 0)
    return ret;

/* Validate exact firmware/module identity before patching. */
if (info.module_nid != EXPECTED_MODULE_NID)
    return TAI_ERROR_INVALID_MODULE;

/* EXPECTED_OFFSET is a placeholder. Validate the text-segment size too. */
ret = taiHookFunctionOffsetForKernel(
    KERNEL_PID, &ref, info.modid, 0, EXPECTED_OFFSET, 1, my_hook);
```

Do not reuse an offset from another firmware. Validate module NID and segment
size first; a wrong offset can crash SceShell or the kernel.

## Injections

```c
SceUID taiInjectDataForKernel(SceUID pid, SceUID modid, int segidx,
                              uint32_t offset, const void *data, size_t size);
SceUID taiInjectAbsForKernel(SceUID pid, void *dest, const void *src,
                             size_t size);
int taiInjectReleaseForKernel(SceUID tai_uid);
```

- Use for read-only data or functions too small to hook.
- Only one module can patch a given module + address at a time.
- The kernel saves the original data; don't inject huge patches.
- Release in `module_stop`.
- Validate module/offset before injecting.

## Custom syscall exports

A kernel plugin can export a library to user mode as **syscalls**.

`exports.yml`:

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
        - my_kernel_function2
```

- The top-level key is the module name; it must start at column 0.
- `libraries:` declares exported libraries.
- `syscall: true` exposes the library to userland.
- Functions listed under `functions:` are exported by name; the tool computes
  NIDs.

Generate the user stub:

```cmake
vita_create_stubs(mykern_stubs mykern exports.yml KERNEL)

link_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(myapp src/main.c)
add_dependencies(myapp mykern_stubs)
target_link_libraries(myapp mykern_stub SceLibKernel_stub)
```

`vita_create_stubs` runs `vita-elf-export` + `vita-libs-gen` and produces:

```text
lib<module-name>_stub.a
lib<module-name>_stub_weak.a
```

Gotchas:

- The YAML first line must be the module name at column 0.
- `vita-libs-gen` has a 1024-entry limit per library in CMake mode;
  `vita-libs-gen-2` removes it.
- The syscall stack is **4096 bytes**. Large locals/deep call chains overflow.
  Use `ksceKernelRunWithStack` if needed.
- A module that exposes syscalls cannot be unloaded by default (see below).

## Module unloading

Trying to unload a kernel module that exposes syscalls returns:

```text
SCE_KERNEL_ERROR_MODULEMGR_IN_USE = 0x8002D002
```

The taiHEN docs give a workaround: hook the module manager's unload check so it
returns 1, then release the hook when the module unloads. The docs use:

```text
SceKernelModulemgr / SceSblACMgrForKernel 0x11F9B314
function 0xBBA13D9C
```

Other unload facts:

- `module_stop` of a user plugin is not automatically called when the kernel
  module it loaded is unloaded. Do not rely on it for kernel cleanup.
- Kernel patches are not auto-cleaned. Release every hook/injection in
  `module_stop`.
- If the target module unloads while your hook is installed, the trampoline can
  point into freed memory. Hook the target's unload path and release first.
- For fire-and-forget kernel modules, return `SCE_KERNEL_START_NO_RESIDENT`.

## Firmware and NID differences

What is stable: function NIDs are hashes of names/signatures and are often
stable across firmware.

What is not stable: module NIDs, library NIDs, segment layouts, offsets.

Examples:

- `SceIofilemgr::ksceIoOpen` is `0x75192972` in the 3.60 DB.
- `SceIofilemgr::_sceIoOpen` is `0xCC67B6FD` in the 3.60 DB.
- `SceLibKernel::sceIoOpen` is `0x6C60AC61`.
- `SceShell` module NID is `0x0552F692` on retail 3.60 and `0x5549BF1F` on
  retail 3.65 (community/real-patch validation).
- PSVshell resolves 3.60 and 3.65 pairs for `SceKernelModulemgr` and `SceSysmem`
  and falls back.

Practical rules:

1. Resolve by module name + function NID with `TAI_ANY_LIBRARY` when possible.
2. If you need a library NID, try known 3.60 then 3.65 pairs.
3. For offset hooks, validate module NID and segment size.
4. Fail closed on unknown firmware: log and do nothing.
5. Do not assume a 3.60 plugin works on 3.65 or vice versa.

## Walkthrough: `vita-kernel-demo`

Project: `vita-kernel-demo`

```text
${KERNEL_VPK}
├── eboot.bin              user app (UNSAFE)
├── kernel_demo.skprx      kernel plugin
└── sce_sys/...            LiveArea assets
```

### Kernel plugin

- Hooks `SceIofilemgr::_sceIoOpen` (`0xCC67B6FD`).
- Copies the user path with `ksceKernelStrncpyUserToKernel`.
- Calls `TAI_CONTINUE`.
- Counts successful opens and stores the last path.
- Exports `KernelDemo` with `syscall: true`:
  - `kdemo_get_open_count`
  - `kdemo_get_last_path`
- `module_stop` releases the hook.
- Imports: `taihenForKernel`, `SceSysmemForDriver`.

### Generated stub

`vita_create_stubs(kernel_demo_stubs kernel_demo kernel/exports.yml KERNEL)`
produces `libvita_kernel_demo_stub.a` with `kdemo_get_open_count` and
`kdemo_get_last_path`.

### App

- Loads `app0:kernel_demo.skprx` with `taiLoadStartKernelModule`.
- Calls the stub functions.
- Displays count and last path.
- Does one `sceIoOpen("app0:eboot.bin")` to guarantee a counted open.

### CMake gotchas used

- Reset `VITA_ELF_CREATE_FLAGS` / `VITA_MAKE_FSELF_FLAGS` cache between the
  kernel plugin and the app.
- `add_dependencies(${KERNEL_VPK}-vpk kernel_demo.skprx-self)`.
- Pass the generated `.skprx` path as `${CMAKE_CURRENT_BINARY_DIR}/kernel_demo.skprx`.
- `__sce_headroom=0x10000`.

## Safety checklist

Before shipping any `.skprx`:

- [ ] I know why this must be kernel-space; a user plugin cannot do it.
- [ ] I have read taiHEN USAGE and understand hooks vs injections vs peek/poke.
- [ ] I have a recovery path: hold **L**, Safe Mode, backup of `config.txt`.
- [ ] The plugin is **not** in `*KERNEL` until tested by on-demand load.
- [ ] Every `taiHookFunction*` return is checked.
- [ ] Every hook calls `TAI_CONTINUE`.
- [ ] Every user pointer is copied with a bound.
- [ ] No recursion in filesystem hooks.
- [ ] No blocking I/O/sleep/long loops in a hook.
- [ ] Stack frames are small (4 KiB syscall stack).
- [ ] Offset hooks validate module NID and segment size.
- [ ] Every hook/injection is released in `module_stop`.
- [ ] Unload races are handled.
- [ ] I do not unload a syscall-exporting module without the override.
- [ ] I use `SCE_KERNEL_START_NO_RESIDENT` for fire-and-forget modules.
- [ ] I do not casually patch `SceSbl*`/auth checks.
- [ ] I test on 3.60 and 3.65 if I claim both.
- [ ] Unknown module NID → do nothing, log, exit cleanly.
- [ ] I warn users about boot risk and recovery.
- [ ] I ship source/license and no Sony keys/firmware.

## What not to do

- Do not build a generic `read_kernel(addr)` / `write_kernel(addr)` export for
  an app unless the user explicitly understands the risk. Prefer a narrow,
  validated operation.
- Do not edit `boot_config.txt`.
- Do not add untested plugins to `*KERNEL`.
- Do not hook `SceSbl*`/license checks for piracy.
- Do not ignore hook return values.
- Do not dereference user pointers directly.
- Do not log through a filesystem path inside a filesystem hook.
