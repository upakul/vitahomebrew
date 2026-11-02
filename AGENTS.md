# AGENTS.md — Workspace Operating Contract

The entry point for every agent working in this workspace. Read it before
deciding, editing, or building anything.

## Workspace map

| Path | What it is |
|---|---|
| `vitasdk/` | VitaSDK 2026.08: compiler, host tools, headers, stubs |
| `vita-cap-test/` | I/O + audio + WiFi + library capability test; unsafe |
| `vita-kernel-demo/` | User app + taiHEN `.skprx` + custom syscall |
| `vita-cockpit/` | System dashboard in Cyberpunk 2077 HUD style; unsafe eboot, runs on hardware |
| `agent-wiki/` | The knowledge base and operating details |
| `PROJECTS.md` | Live project tracker: state, last job, next step |

Target console: **PS Vita on 3.65 with permanent Enso CFW**. PSTV is out of
scope. Do not assume 3.60 offsets.

**The console reports firmware 3.74, and that is expected.** The system version
string is spoofed on this unit; the machine is genuinely on 3.65 Enso. So:

- `sceKernelGetSystemSwVersion()`, the system UI, and anything that prints a
  version will say 3.74. COCKPIT's SYSTEM panel shows 3.74 by design.
- Do not infer the real firmware from that string, and do not "correct" the
  target because an app prints 3.74.
- NIDs, offsets, and API availability follow **3.65**. The spoof changes what is
  reported, never what is loaded.

See `agent-wiki/01-platform.md` § Firmware baseline.

## Before you touch anything

Read, in this order:

1. **`PROJECTS.md`** — what every project's current state is, what the last job
   changed, and what the next action is.
2. **`agent-wiki/README.md`** and the topic doc that matches the task (routing
   table below).
3. **`agent-wiki/13-fact-check.md`** — what is verified and what is
   community-reported.
4. **The project's own docs**, if it has a `docs/` directory:
   `PROJECT.md`, `ARCHITECTURE.md`, `DESIGN.md`, `DECISIONS.md`. These are
   binding for that project the same way the wiki is binding for Vita facts.

Do not rely on memory or generic knowledge for Vita-specific decisions. When a
claim is tagged `[C]` (community) or `[L]` (lore), verify it against a primary
source before acting on it.

## Code architecture

These rules apply to every line of code written in this workspace, in any
project, in any language. A project may add constraints in its own
`docs/ARCHITECTURE.md`; it may not weaken these.

### The dependency rule

**Source dependencies point inward, toward policy. Details depend on policy;
policy never depends on details.**

- Policy is what the program decides: sampling cadence, thresholds, alert
  conditions, layout intent, state transitions.
- Details are how it is carried out: the Vita SDK, `vita2d`, filesystem calls,
  network calls, a specific renderer.
- Policy must be compilable and testable without the details. If it is not, the
  boundary is in the wrong place.

### Rules

1. **Every external dependency gets one adapter.** All SDK, renderer,
   filesystem, or network calls for a given concern live in a single module.
   Nothing else calls that dependency directly.
2. **Define a port when the policy needs the outside.** The inner layer declares
   the contract in its own types; the adapter implements it. No `Sce*` type, no
   framework type, and no file handle crosses inward.
3. **Keep a list of files allowed to include the platform header.** Every project
   records it in `docs/ARCHITECTURE.md`. A new file reaching for the SDK is a
   layering bug, not a shortcut.
4. **`main()` is a composition root.** It constructs and starts. Nothing else
   constructs. No inner layer instantiates a concrete adapter.
5. **One writer per piece of shared mutable state.** Document the writer. If a
   second writer appears, the design is wrong.
6. **Check every return value.** Degrade, never crash. When a value cannot be
   obtained, render its absence (`--`) rather than a guess, a stale number, or a
   zero. A fabricated value is worse than a missing one.
7. **No magic numbers.** Name every constant that carries meaning.
8. **Headers are contracts.** A module's `.h` states what it promises. Keep
   implementation types, internal structs, and helper functions out of it.
9. **No dead code, no unused links.** Do not link a library the source never
   calls. Do not keep a function nothing invokes.
10. **One reason to change per module.** If a file would change both because the
    UI moved and because a platform API changed, split it.
11. **Comments explain why.** The code explains what. A comment that restates
    the line is noise; a comment that records a platform quirk, a rejected
    alternative, or a `[C]` assumption is required.
12. **Naming.** `snake_case` for functions, `PascalCase` for types,
    `C_UPPER_SNAKE` for constants, `module_verb` for exported functions
    (`metrics_sample_slow`, `font_text_right`). Match the surrounding file.

### Marking unverified platform behaviour

Any place the code depends on a platform claim that has not been confirmed on
hardware carries a `[C]` comment naming the claim and the reason. Isolate those
in the adapter so a correction cannot reach into policy. See
`agent-wiki/13-fact-check.md` for the tag convention.

## Project documentation

Every non-trivial project carries its own docs, and they are part of the
definition of done:

| File | Contains |
|---|---|
| `README.md` | What it is, build, run, doc map |
| `docs/PROJECT.md` | Scope, non-goals, status, roadmap, failure policy |
| `docs/ARCHITECTURE.md` | Layers, dependency rule, module contracts, how to extend |
| `docs/DESIGN.md` | Visual language, layout, palette, typography (if it has a UI) |
| `docs/DECISIONS.md` | Numbered decisions with context and consequences |

Read them before writing code in that project. Update them in the same turn as
the change they describe. If a decision is reversed, add a new entry that
supersedes it — do not silently rewrite history.

## Project tracker

`PROJECTS.md` at the workspace root is the live board. One row per project.

**Update it after every job.** A job is not finished until its row reflects it.

| Field | Rule |
|---|---|
| Status | exactly one of `planning`, `building`, `built`, `verified`, `blocked` |
| Last job | date and one line describing what actually changed |
| Next | the single next action, not a list |
| Notes | blockers, unverified claims, or a link to the decision that governs it |

`built` means it compiles and packages. `verified` means it has run on the
target console. Do not promote a project to `verified` without hardware
evidence.

## Documentation maintenance protocol

When you learn something new, fix something wrong, or change a project, update
the docs in the same turn. Do not leave the knowledge only in chat.

| What you learned | Update |
|---|---|
| Setup, install, first build | `agent-wiki/00-quickstart.md` |
| Hardware, OS, SELF/NID model | `agent-wiki/01-platform.md` |
| Toolchain, packages, compiler flags | `agent-wiki/02-toolchain.md` |
| ELF/VELF/SELF/VPK build details | `agent-wiki/03-build-pipeline.md` |
| App APIs: I/O, audio, WiFi, UI | `agent-wiki/04-userland-apps.md` |
| VPK, param.sfo, LiveArea, install | `agent-wiki/05-packaging-install.md` |
| taiHEN hooks, syscalls, kernel plugins | `agent-wiki/06-kernel-plugins.md` |
| Crash dumps, logging, GDB, recovery | `agent-wiki/07-debugging-recovery.md` |
| Firmware, Enso, exploits, storage | `agent-wiki/08-firmware-jailbreak.md` |
| Exact errors and fixes | `agent-wiki/10-troubleshooting.md` |
| NIDs, APIs, formats, error codes | `agent-wiki/11-reference.md` |
| Project behaviour or artifacts | `agent-wiki/12-projects.md` |
| A new claim, correction, or verification | `agent-wiki/13-fact-check.md` |
| A new source or link | `agent-wiki/14-sources.md` |
| Scope or state of the investigation | `agent-wiki/15-chat-summary.md` |
| Any change to a project | that project's `docs/`, and `PROJECTS.md` |

Rules for edits:

- Cite a URL or a local source path. If a forum is inaccessible, say so.
- Use the `[V]` / `[C]` / `[L]` tags in the community notes.
- Do not duplicate a fact across files; put it where it belongs and link.
- Keep cross-references working. After editing, check links and code fences.
- If a project changed, update `12-projects.md` and the source copies in
  `agent-wiki/examples/`.
- If a kernel/API claim changes, update `06`, `11`, and `13` together.
- Do not rewrite a whole document; make targeted corrections.

Verification check, covering the wiki, the root, and every project's docs:

```sh
cd /Users/ghostmeson/developer/builds/psvitahomebrew
python3 - <<'PY'
import os, re
bad = []
skip = {'vitasdk', 'build', '.git', 'node_modules'}
for root, dirs, files in os.walk('.'):
    dirs[:] = [d for d in dirs if d not in skip]
    for f in files:
        if not f.endswith('.md'):
            continue
        p = os.path.join(root, f)
        txt = open(p, errors='replace').read()
        for m in re.finditer(r'\[[^\]]*\]\(([^)]+)\)', txt):
            link = m.group(1)
            if link.startswith(('http://', 'https://', '#')):
                continue
            target = link.split('#')[0]
            if target and not os.path.exists(
                    os.path.normpath(os.path.join(root, target))):
                bad.append((p, link))
        if txt.count(chr(96) * 3) % 2:
            bad.append((p, 'odd code fence'))
print('issues:', bad if bad else 'none')
PY
```

## Build and upload

```sh
cd /Users/ghostmeson/developer/builds/psvitahomebrew
export VITASDK="$PWD/vitasdk"
export PATH="$VITASDK/bin:$PATH"

(cd vita-cap-test && ./build.sh)
(cd vita-kernel-demo && ./build.sh)
(cd vita-cockpit && ./build.sh)
```

Each project has a `send.sh`:

```sh
(cd vita-cockpit && ./send.sh <vita-ip>)
```

VitaShell must be running its FTP server on port 1337. The correct URL form is
`ftp://<ip>:1337/ux0:/data/` (colon followed by slash).

### Artifact naming — binding for every project

**Every VPK build must produce a uniquely named artifact.** A build that
overwrites its predecessor cannot be identified on the console, in `ux0:data`, or
in a bug report, and "which build is on the Vita?" stops being answerable.

The name is `<project>-v<version>-b<build>.vpk`:

| Part | Source | Changes |
|---|---|---|
| `version` | `VITA_VERSION` in `CMakeLists.txt` | only on a release; must stay `##.##` because it also becomes `APP_VER` |
| `build` | `BUILD_NUMBER`, incremented by `build.sh` | **every build**, automatically |

Rules:

1. `build.sh` increments `BUILD_NUMBER` on every run and passes it to CMake as
   `-D<PROJECT>_BUILD=<n>`. No build path may skip the increment.
2. `BUILD_NUMBER` lives in the project root, **never** in `build/`. Wiping the
   build directory must not restart numbering into a collision with VPKs already
   copied to the console.
3. `send.sh` uploads the newest `build/<project>-v*.vpk`.
4. A number identifies a *distinct build*. `build.sh` hashes the sources and
   reuses the previous number when nothing changed, so repeated builds do not
   inflate it. Never decrement a number.
5. `build.sh` deletes every superseded VPK. The build directory holds exactly one
   artifact — the one to flash — because a directory of forty near-identical
   files is worse than none.
6. A fixed name (a store upload, for instance) is a deliberate exception,
   recorded in `DECISIONS.md`. It is never the default.

The full argument is in `vita-cockpit/docs/PROJECT.md` §4, the first project to
adopt it. `vita-cap-test`, `vita-kernel-demo` and `vita-cockpit` follow the same
scheme.

## Task routing

| User asks for | Read first |
|---|---|
| Build a normal app | `03-build-pipeline.md`, `04-userland-apps.md` |
| Add file I/O / audio / WiFi | `04-userland-apps.md` |
| Package or install a VPK | `05-packaging-install.md` |
| Write a taiHEN plugin | `06-kernel-plugins.md` |
| Debug a crash | `07-debugging-recovery.md` |
| Fix a build/install error | `10-troubleshooting.md` |
| Understand a NID or format | `11-reference.md` |
| Check a claim | `13-fact-check.md`, `09-community-field-notes.md` |
| Change a project | `12-projects.md`, that project's `docs/`, then the API doc |
| Add or change a UI | the project's `docs/DESIGN.md` first, then `04-userland-apps.md` |
| Add a module or a dependency | the project's `docs/ARCHITECTURE.md`, then this file's code rules |

## Hard safety rules

1. **Do not edit `ur0:tai/boot_config.txt`.** It is model-specific and can
   prevent boot. Use the Enso installer's "fix boot config" option.
2. **Do not add a kernel plugin to `*KERNEL` until it has been tested by
   on-demand load.** A boot-time plugin that crashes causes a bootloop.
3. **Do not use `UNSAFE` unless a specific API requires it.** Safe eboots run
   normally; unsafe is a system-wide permission relaxation.
4. **Do not build a generic kernel peek/poke primitive** unless the user
   explicitly asks and understands the risk. Prefer a narrow, validated export.
5. **Do not claim a kernel plugin works on firmware you have not tested.**
   Resolve NIDs/offsets at runtime and fail closed on unknown identities.
6. **Keep the unstripped `.elf`** for any app you ask the user to test.
7. **Do not include Sony keys, firmware, game backups, or leaked SDK files.**

## When docs conflict with reality

Verify against the SDK headers, the tool source, or the project code. If a doc is
wrong, fix it and record the correction in `13-fact-check.md`. If the code is
wrong, fix the code and update the docs to match. Never silently keep two
versions of the truth.

A decision recorded in a project's `DECISIONS.md` is not permanent. If evidence
contradicts it, add a superseding entry with the reason — do not delete the old
one.

## Definition of done

A job is finished when all of these hold:

- It builds, or the build failure is documented with its cause.
- The VPK is uniquely named `<project>-v<version>-b<build>.vpk`, with the build
  number incremented from the previous build.
- The project's `docs/` reflect the change.
- `PROJECTS.md` has an updated row.
- `agent-wiki/12-projects.md` and `13-fact-check.md` are updated if a claim or
  artifact changed.
- The verification check above reports no broken links or odd code fences.
- Every new `[C]` assumption is tagged and isolated in an adapter.
- No finding lives only in chat.

## What not to do

- Do not start a kernel-plugin change from memory.
- Do not add a plugin to `*KERNEL` "just to test".
- Do not recommend formatting the memory card as a first fix.
- Do not present an LLM-generated wiki or forum post as authoritative.
- Do not leave a new finding only in chat; add it to the docs.
- Do not let a policy layer import a platform, renderer, or network header.
- Do not render a number the platform cannot report.
- Do not mark a project `verified` without hardware evidence.
- Do not ship a VPK with a fixed filename, and never reuse a build number.
