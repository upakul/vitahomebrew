# Agent Wiki: PS Vita Homebrew

A self-contained knowledge base for an AI agent that needs to understand PS Vita
homebrew, build a userland app, or write a taiHEN kernel plugin.

**Target console:** PS Vita on **3.65 with permanent Enso CFW**, set up with
VitaDeploy. The same material applies to 3.60 with small NID/offset
differences. PSTV is not a supported target in this wiki; its missing touch,
motion, camera, and USB/HDMI differences are not documented here.

**Host used for this wiki:** macOS (Apple Silicon), user `ghostmeson`.

**Workspace root:** `/Users/ghostmeson/developer/builds/psvitahomebrew`

All relative paths in this wiki are from that root. The workspace contains
`vitasdk/`, `vita-cap-test/`, `vita-kernel-demo/`, and
`agent-wiki/`.

The workspace-level entry point is [`../AGENTS.md`](../AGENTS.md). It defines
the decision protocol and the documentation-maintenance rules.

---

## Environment snapshot

| Item | Value |
|---|---|
| Workspace root | `/Users/ghostmeson/developer/builds/psvitahomebrew` |
| VitaSDK | `2026.08` |
| SDK root | `vitasdk/` |
| Compiler | `arm-vita-eabi-gcc` 15.2.0 |
| Package manager | `vdpm` 0.1.3 |
| SDK sequence | `2026102473` |
| Toolchain snapshot | `sdk-snapshot-20260825.611.1` |
| Package snapshot | `packages-2026.08-snapshot-20261024.61.1` |
| Vita firmware | 3.65, Enso permanent CFW |
| CFW installer | VitaDeploy |

Shell setup for this workspace:

```sh
cd /Users/ghostmeson/developer/builds/psvitahomebrew
export VITASDK="$PWD/vitasdk"
export PATH="$VITASDK/bin:$PATH"
```

### Working projects

| Project | Path | What it is |
|---|---|---|
| `vita-cap-test` | `vita-cap-test` | I/O + audio + WiFi + library capability test, unsafe |
| `vita-kernel-demo` | `vita-kernel-demo` | User app + `.skprx` kernel hook + custom syscall |
| `vita-cockpit` | `vita-cockpit` | System dashboard in Cyberpunk 2077 HUD style; unsafe by default, 155-row plugin catalog |

Build outputs:

```text
vita-cap-test/build/cap-test-v<version>-b<build>.vpk
vita-kernel-demo/build/kernel-demo-v<version>-b<build>.vpk
vita-kernel-demo/build/kernel_demo.skprx
vita-cockpit/build/cockpit-v<version>-b<build>.vpk
```

---

## Read this in order

| File | Contents |
|---|---|
| [00-quickstart.md](00-quickstart.md) | Install, build, upload, run |
| [01-platform.md](01-platform.md) | Hardware, OS, executable model, control layers |
| [02-toolchain.md](02-toolchain.md) | VitaSDK, vdpm, packages, compiler, host tools |
| [03-build-pipeline.md](03-build-pipeline.md) | ELF → VELF → SELF → VPK, CMake macros |
| [04-userland-apps.md](04-userland-apps.md) | App structure, vita2d, I/O, audio, WiFi |
| [05-packaging-install.md](05-packaging-install.md) | VPK, param.sfo, LiveArea, safe/unsafe, VitaShell |
| [06-kernel-plugins.md](06-kernel-plugins.md) | taiHEN, hooks, custom syscalls, generated stubs |
| [07-debugging-recovery.md](07-debugging-recovery.md) | Core dumps, logging, GDB, recovery |
| [08-firmware-jailbreak.md](08-firmware-jailbreak.md) | 3.65 Enso, VitaDeploy, exploits, SD2Vita |
| [09-community-field-notes.md](09-community-field-notes.md) | Forum/community research, [V]/[C]/[L] tags |
| [10-troubleshooting.md](10-troubleshooting.md) | Exact errors and fixes |
| [11-reference.md](11-reference.md) | NIDs, APIs, formats, error codes, glossary |
| [12-projects.md](12-projects.md) | The three working projects and their verification |
| [13-fact-check.md](13-fact-check.md) | What was verified, how, and what was not |
| [14-sources.md](14-sources.md) | Primary and community sources |
| [15-chat-summary.md](15-chat-summary.md) | Summary of the investigation and current state |
| [examples/](examples/) | Copies of the key project source files |

---

## Golden rules

1. **Start safe.** A safe eboot (`authid 0x2F00000000000002`) runs normally with
   restricted filesystem/API access. Add `UNSAFE` only when a specific API
   requires it.
2. **Kernel plugins are dangerous.** Test with a recovery path: hold **L** at
   boot, Safe Mode, and a backup of `ur0:tai/config.txt`.
3. **Do not add a kernel plugin to `*KERNEL` until it has been tested by
   on-demand load.** A boot-time plugin that crashes causes a bootloop.
4. **Do not build a generic kernel peek/poke primitive** unless the user
   explicitly asks and understands the risk. Prefer a narrow, validated export.
5. **Do not claim a kernel plugin works on firmware you have not tested.**
   Resolve NIDs/offsets at runtime and fail closed on unknown module identities.
6. **Never edit `ur0:tai/boot_config.txt`.** Use the Enso installer's "fix boot
   config" option. A bad boot config can prevent boot.
7. **Keep the unstripped `.elf`.** Crash dumps need it; `eboot.bin` and `.velf`
   are not enough.
8. **Check every taiHEN return value.** If a hook fails, do not call
   `TAI_CONTINUE` on an uninitialized ref.
9. **Always call `TAI_CONTINUE`** in a hook, even when overriding the result.
10. **Never dereference a user pointer in kernel mode.** Copy it with
    `ksceKernelStrncpyUserToKernel` / `taiMemcpyUserToKernel` and bound it.
11. **No recursion in filesystem hooks.** Do not call the hooked function or a
    logging path that re-enters it.
12. **Add `__sce_headroom`** to projects that grow:
    `-Wl,--defsym=__sce_headroom=0x10000`.
13. **Respect the legal boundary.** No Sony keys, no copyrighted firmware, no
    game backups, no leaked official SDK.

`../AGENTS.md` is the canonical hard-rule list; these are the entry-point
summary.

---

## What is verified

The packaging, SFO, authid, import/export, and build claims in this wiki were
checked against the actual VitaSDK/VitaShell/taiHEN sources and, where noted, by
building the host tools and the three projects. See
[13-fact-check.md](13-fact-check.md).

Not verified in this environment: running the apps or the kernel plugin on
physical Vita hardware. The on-device install path is from VitaShell source and
official documentation.
