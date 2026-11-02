# Vita Kernel Demo

A userland app plus a real taiHEN kernel plugin.

The app loads `kernel_demo.skprx` on demand, the plugin hooks
`SceIofilemgr::ksceIoOpen`, and the app reads the hook's state through a
generated syscall stub.

This is **kernel-level** code. Read the safety section before running it.

## Architecture

```text
kernel-demo-v<version>-b<build>.vpk
├── eboot.bin              userland app (UNSAFE)
├── kernel_demo.skprx      kernel plugin
└── sce_sys/...            LiveArea metadata and art

App:
  taiLoadStartKernelModule("app0:kernel_demo.skprx")
        │
        ▼
Kernel plugin module_start:
  taiHookFunctionExportForKernel(
      KERNEL_PID,
      &open_hook,
      "SceIofilemgr",
      TAI_ANY_LIBRARY,
      0x75192972,          // ksceIoOpen
      hook_ksceIoOpen)

  hook_ksceIoOpen:
      fd = TAI_CONTINUE(ksceIoOpen, ...)
      count successful opens
      remember the last path

App:
  kdemo_get_open_count()   ── generated syscall stub ──► kernel plugin
  kdemo_get_last_path()    ── generated syscall stub ──► kernel plugin
```

## Files

| Path | Purpose |
|---|---|
| `kernel/main.c` | The `.skprx`: taiHEN hook and exported functions |
| `kernel/exports.yml` | Kernel module/library export description |
| `app/main.c` | The userland app that loads the plugin and reads stats |
| `CMakeLists.txt` | Builds the kernel plugin, generates the user stub, builds the VPK |

## Build

```sh
./build.sh
```

Output:

```text
build/kernel_demo.skprx
build/kernel-demo-v<version>-b<build>.vpk
```

## Install

On the Vita, open VitaShell and press **Select** to start FTP, then:

```sh
./send.sh <vita-ip>
```

Install `kernel-demo-v<version>-b<build>.vpk` from `ux0:data` and launch **Kernel Demo**.

You need:

- 3.65 Enso / taiHEN
- **HENkaku Settings → Enable Unsafe Homebrew**
- The app is `UNSAFE` because it loads a kernel module

## What you should see

```text
Kernel plugin demo
Plugin: loaded (modid 0x...)
Kernel opens seen: N
Last path: app0:eboot.bin
```

The count grows as the system and the app open files. The plugin is loaded
on demand; it is **not** added to `ur0:tai/config.txt`, so a reboot clears it.

## Safety

This plugin only **reads** the path and counts successful opens. It does not
patch code, write kernel memory, or modify the boot configuration. That makes
it a relatively safe first kernel plugin, but it is still kernel code.

If something goes wrong:

1. Hold **L** during boot to skip taiHEN plugin loading.
2. If needed, enter safe mode and restore settings.
3. Reboot. Because this plugin is loaded on demand, a reboot removes it.

Do **not** add it to `*KERNEL` in `ur0:tai/config.txt` until you have tested it
and understand the recovery path.

## How the pieces fit

### 1. The hook

`ksceIoOpen` is the kernel export in `SceIofilemgr`. Its NID in the 3.60
database is:

```text
SceIofilemgr / ksceIoOpen = 0x75192972
```

The hook uses `TAI_CONTINUE` to call the original function, then increments a
counter and copies the path into a kernel buffer.

### 2. The export

`kernel/exports.yml` declares a syscall library:

```yaml
libraries:
  KernelDemo:
    syscall: true
    version: 1
    functions:
      - kdemo_get_open_count
      - kdemo_get_last_path
```

`syscall: true` is what lets userland call these functions.

### 3. The generated stub

`vita_create_stubs(... KERNEL)` runs `vita-elf-export` and `vita-libs-gen` to
produce:

```text
build/libvita_kernel_demo_stub.a
```

The app links that archive, so it can call `kdemo_get_open_count()` and
`kdemo_get_last_path()` like normal C functions.

### 4. The load

The app calls:

```c
taiLoadStartKernelModule("app0:kernel_demo.skprx", 0, NULL, 0);
```

The plugin is packaged inside the VPK, so `app0:` resolves to the installed
app directory.

## Extending it

Good next steps:

- Hook `ksceIoClose` as well and track open/close pairs.
- Add a `kdemo_reset()` export.
- Add a `kdemo_get_last_path` length check.
- Hook a function in a game or system app and filter by title ID.
- Add a kernel-to-user buffer copy for a larger log ring.

If you want arbitrary kernel memory read/write from the app, that is a much
more dangerous step. It is doable through a kernel export using
`taiMemcpyKernelToUser` or `ksceKernelMemcpyKernelToUser`, but a bad address
will crash the Vita. Add it only with a whitelist and a recovery plan.

## Notes

- The app is built with `-Wl,--defsym=__sce_headroom=0x10000` to avoid the
  `vita-elf-create: segment 1 overlaps` failure as code grows.
- The kernel plugin is built with `-nostdlib` and links only
  `taihenForKernel`, `SceSysclibForDriver`, and `SceSysmemForDriver`.
- The app is `UNSAFE` (`authid 0x2F00000000000001`).
