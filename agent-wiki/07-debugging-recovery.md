# 07 — Debugging and Recovery

The Vita has no official desktop-class debugger. The practical stack is:
on-screen text, logs, core dumps, and (optionally) a GDB stub.

## 1. On-screen debug text

The VitaSDK samples include `psvDebugScreenInit()` and
`psvDebugScreenPrintf()` in `samples/common/debugScreen.c`. They draw text
directly to the framebuffer.

Use this for early bring-up before graphics are working.

## 2. Logging

| Method | Scope | How to read |
|---|---|---|
| `sceClibPrintf` / `printf` | User app | TTY; Cat-A-Log or serial/UART |
| `ksceKernelPrintf` | Kernel plugin | Same |
| `psvDebugScreenPrintf` | User app | On screen |
| File logging | Any | `ux0:data/<app>/log.txt` |
| DebugNet | User app | UDP to a PC |
| Cat-A-Log | User + kernel | Network log viewer |

### Cat-A-Log

1. Copy `catlog.skprx` and `catlog.suprx` to `ur0:tai`.
2. `*KERNEL`: `ur0:tai/catlog.skprx`.
3. `*NPXS10015`: `ur0:tai/catlog.suprx`.
4. Reboot.
5. On the PC: `nc -kl <port>`.
6. On the Vita: Settings → Network → Cat Log settings → PC IP/port.

### DebugNet

Server:

```sh
socat udp-recv:18194 stdout
```

Vita:

```c
#include <debugnet.h>

debugNetInit("192.168.1.10", 18194, DEBUG);
debugNetPrintf(DEBUG, "value = %d\n", value);
debugNetFinish();
```

### Kernel logging rules

Do **not** put heavy logging inside a filesystem hook. `ksceKernelPrintf` is
slow, and any logging path that opens a file can re-enter the hook. Use an
in-memory ring buffer and a low-priority thread to flush it.

## 3. Core dumps

When a user app crashes (often `C2-12828-1`), the Vita can write a core dump to
`ux0:data`:

```text
ux0:data/psp2core-<...>.psp2dmp
```

The file is a gzip-compressed ELF core with Sony's own note layout:
`MODULE_INFO`, `THREAD_INFO`, `THREAD_REG_INFO`, `STACK_INFO`, `TTY_INFO`.
Generic `gdb target core` will not understand it.

### Parse it

Classic `vita-parse-core`:

```sh
python2 main.py psp2core-....psp2dmp build/my_app.elf
```

Requirements:

- `arm-vita-eabi-objdump` and `arm-vita-eabi-addr2line` on `PATH`.
- The **unstripped `.elf`**, not `.velf` and not `eboot.bin`.
- Build with `-g`.

Modern alternatives:

```sh
vita-crashdump psp2core-....psp2dmp build/my_app.elf
```

`vita-crashdump` is a Rust rewrite with ARM unwinding, fault-address extraction,
demangling, and a GUI.

`psp2-core-parse` is a Python 3 parser with many commands:

```sh
./psp2-core-parse analyze dump.psp2dmp --image myapp=myapp.elf
./psp2-core-parse threads dump.psp2dmp
./psp2-core-parse registers dump.psp2dmp crash
./psp2-core-parse backtrace dump.psp2dmp crash --image application=application.elf
```

`psp2-gpucrash-parse` is the GPU-crash-specific parser (Python 3, no runtime
dependencies). It understands the `GPUCRASH` note layout, SGX543 BIF/MMU
registers, firmware pipeline phases, and captured GPU allocations:

```sh
./psp2-gpucrash-parse triage *.psp2dmp
./psp2-gpucrash-parse analyze dump.psp2dmp
./psp2-gpucrash-parse gpu faults dump.psp2dmp
./psp2-gpucrash-parse gpu pipeline dump.psp2dmp
./psp2-gpucrash-parse address dump.psp2dmp 0x81180000
```

It reports the faulting GPU page, the requestor (PDS vertex/uniform fetch, HSR
depth/stencil, and so on), the MMU context, and the owning process. It does not
recover the exact byte or the SGX page tables, and its root-cause confidence is
deliberately low. Correlate the fault page with the CPU thread stacks in the
same dump: a fault page at `stack base + stack size - 0x1000` means the GPU was
handed a stack pointer. The `address` command prints the thread reference and
the captured bytes. `gpu-crash-study/ANALYSIS.md` has a worked example.

The parser rejects `.tmp` core dumps whose program headers run past the end of
the file. Those are ordinary dumps still being written, not `GPUCRASH` files.

### Kernel coredump trigger

VitaSDK 2026.08 does **not** declare `ksceCoredumpCreateDump` in
`psp2kern/coredump.h`. The kernel-side hook is
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

For userland crashes, the practical path is the system-generated `.psp2dmp`
described above.

### ASLR

Crash addresses vary between runs. Use `NOASLR` in `vita_create_self` when you
need reproducible addresses, or use module+offset resolution in the parser.

## 4. GDB

### `arm-vita-eabi-gdb`

The SDK ships a Vita-target GDB. There is no official on-device stub.

### KVDB + vdbtcp

KVDB is a kernel-mode GDB stub; by default it listens on UART. For TCP, use the
`vdbtcp` companion.

- `kvdb.skprx` under `*KERNEL`.
- `vdbtcp.suprx` under `*main`; provides GDB on 31337, FTP on 1337, and
  vitacompanion-compatible commands on 1338.

```gdb
set architecture arm
target remote 192.168.1.50:31337
```

KVDB is alpha-quality: one debugging session per boot, memory faults are not
handled cleanly, and it does not launch the target itself. vdbtcp can launch a
SELF via its command service.

### VitaDebugger / vita-uvdb

`VitaDebugger` is an experimental application-linked GDB server (GDB over TCP,
breakpoints, registers, memory, stepping, profiler). It is firmware-specific
(currently validated on retail 3.65) and not a system-wide debugger. It embeds a
stub in the app, so it does not attach to an unmodified running app.

## 5. Common crash causes

| Cause | Symptom | Prevention |
|---|---|---|
| NULL/invalid user pointer in kernel hook | Kernel data abort, reboot | Copy with `ksceKernelStrncpyUserToKernel`; check return |
| Wrong NID, hook failed, code still uses ref | Jump to garbage | Check `taiHookFunction*` return; only use if `>= 0` |
| Stack overflow | Data abort near stack guard; corruption | Small frames; 4 KiB syscall stack; `ksceKernelRunWithStack` |
| Module unload race | Crash after unload | Release hooks before unload; hook load/unload for weak imports |
| Missing `TAI_CONTINUE` | Other plugins stop working | Always continue the chain |
| Recursion in FS hook | Hang/stack overflow | Don't call hooked function or logging that re-enters |
| Unaligned access | Data abort | Use `memcpy` for packed structs |
| Missing `libshacccg.suprx` | `C2-12828-1` for vitaGL apps | Extract from own firmware; don't redistribute |
| Unsafe homebrew disabled | Launch failure/restricted API | Enable in HENkaku Settings |

## 6. Recovery

### Soft bootloop from a plugin

1. **Hold L during boot.** taiHEN skips plugin loading.
2. If it boots, edit the active `config.txt` (usually `ur0:tai/config.txt`) and
   remove the new line.
3. If it still fails, enter **Safe Mode**:
   power off, then hold **R + Power + PS** until the PS logo appears.
4. Safe Mode options:
   - **Restart** — reboot.
   - **Rebuild Database** — rebuilds the app database; can restore boot with
     plugins disabled. Deletes LiveArea layout/messages/playlists, not saves.
   - **Restore the PS Vita System** — resets settings; can regenerate
     `boot_config.txt`; heavy option.
5. Do **not** edit `boot_config.txt`. To deactivate Enso, delete
   `ur0:tai/boot_config.txt`; to reactivate, run the Enso installer and use
   "fix boot config".

### enso_ex recovery

If using enso_ex:

- Hold **VOLUP** at boot if `boot_config.txt` is corrupt.
- Hold **START** for SD-as-os0 recovery.
- Hold **VOLDOWN** to skip a failing module.

### Permanent brick

Normal plugin/config mistakes are recoverable soft bricks. Permanent bricks come
from interrupted firmware flashes, corrupted bootloaders, or writing to the
wrong eMMC/boot area. Treat any bootloader/firmware write as high-risk.

## 7. Testing checklist

- [ ] Build with `-g`; keep the unstripped `.elf`.
- [ ] Test a cold boot, not just a relaunch.
- [ ] Test with the plugin not in `*KERNEL` first.
- [ ] Have hold-L and Safe Mode ready.
- [ ] Back up `ur0:tai/config.txt`.
- [ ] Check `ux0:data` for `.psp2dmp` after a crash.
- [ ] Log to a ring buffer, not a file, inside kernel hooks.
- [ ] Test on 3.60 and 3.65 if claiming both.
- [ ] Verify the installed VPK matches the ELF you kept.
