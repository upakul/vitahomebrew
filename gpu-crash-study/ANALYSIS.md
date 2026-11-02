# GPU crash dump study

Analysis of the crash dumps captured during COCKPIT's first hardware sessions,
performed with
[`psp2_gpucrash_parse`](https://github.com/shoui520/psp2_gpucrash_parse) 0.8.0
(Python 3, no runtime dependencies). The dumps themselves are not published:
a `.psp2dmp` is a full copy of console process memory, including mapped system
modules, so it is analysis input rather than source (see `.gitignore`). Clone
the parser into `tools/psp2_gpucrash_parse/` and run it from there:

```sh
./psp2-gpucrash-parse triage   *.psp2dmp
./psp2-gpucrash-parse analyze  <dump>.psp2dmp
./psp2-gpucrash-parse gpu faults    <dump>.psp2dmp
./psp2-gpucrash-parse gpu pipeline  <dump>.psp2dmp
./psp2-gpucrash-parse address  <dump>.psp2dmp <fault-va>
./psp2-gpucrash-parse compare  <a>.psp2dmp <b>.psp2dmp
```

Captures are local time, 2026-11-01; timezone is not retained. The console is
the 3.65 unit (`release_branches/release_03.650`, build 757166).

The findings below are kept because they explain the renderer decision in
`vita-cockpit/docs/DECISIONS.md` (D29, D31): the stack-vertex-buffer diagnosis
and the removal of `vita2d_draw_array`. Reproducing them requires new captures
from your own console.

## Result

Six of the eight files are GPUCRASH dumps and parse. Two `.tmp` files are
truncated ordinary core dumps, not GPUCRASH, and are rejected by the parser:

```text
psp2core-1793525382-0x000005208b-eboot.bin.psp2dmp.tmp: program segment 68 exceeds core image
psp2core-1793528648-0x00003a2cfb-eboot.bin.psp2dmp.tmp: program segment 47 exceeds core image
```

Both are valid gzip/ELF32 core files, but their program headers run past the
end of the file, which is what a dump still being written looks like. One
contains `PKGJ00000`; neither is a GPU fault. They are excluded from the
findings below.

| Dump | App | Captured | Class | Faulting GPU page | Requestor | Core(s) |
|---|---|---|---|---|---|---|
| `...1793512064` | EMU4VPLUS (gpSP) | 11:17:45 | BIF access fault | `0x00008000` | HSR depth/stencil/visibility | core2 |
| `...1793512475` | EMU4VPLUS (gpSP) | 11:24:35 | 3D lockup | — | — | fragment phase |
| `...1793523757` | VITASHELL 02.02 | 14:32:37 | 3D lockup | — | — | fragment phase |
| `...1793524658` | YTMV00001 (ViTube Music 1.2) | 14:47:38 | BIF access fault | `0x3f800000` (= `1.0f`) | PDS vertex/uniform fetch | core0–3 |
| `...1793536179` | COCKPIT01 | 17:59:39 | BIF access fault | `0x81180000` | PDS vertex/uniform fetch | core1 |
| `...1793536348` | COCKPIT01 | 18:02:28 | BIF access fault | `0x811c0000` | PDS vertex/uniform fetch | core2 |

Failure buckets: four `GPU_BIF_ACCESS_FAULT`, two `GPU_3D_LOCKUP_PHASE_FRAGMENT`.
The two lockups are the same exact failure signature (`614def7cfe14…`) despite
being different applications.

## Significant findings

### 1. COCKPIT: the GPU fetched a CPU stack pointer (confirmed)

This is the strongest result in the set. Both COCKPIT dumps fault on the last
4 KiB page of the main thread's stack:

| | Dump `...1793536179` | Dump `...1793536348` |
|---|---|---|
| Main thread stack | `0x81141000`–`0x81181000` | `0x81181000`–`0x811c1000` |
| Thread SP at capture | `0x81180d08` | `0x811c0d08` |
| GPU fault page | `0x81180000` | `0x811c0000` |
| Offset into stack | `+0x3f000` (last page) | `+0x3f000` (last page) |
| Requestor | PDS (vertex data/uniform fetch) | PDS (vertex data/uniform fetch) |

The stack base moved between runs (ASLR/relaunch), but the fault stayed exactly
one page below the stack top. `psp2-gpucrash-parse address` reports the page as
`THREAD_REFERENCES ... stack+0x3f000` and the captured bytes at the page start
are `0xFF` fill (unused stack). This is not a random GPU address: it is a
pointer into the thread's live stack.

PDS is the unit that reads vertex and uniform data. The app's caller-provided
pointer on this path is the vertex array passed to `vita2d_draw_array`, and
`libvita2d.a` forwards it to `sceGxmSetVertexStream` without copying. A stack
array therefore becomes a GPU read from an unmapped CPU page.

These two dumps predate the fix: `src/gfx/draw.c` is dated 23:55, the crashes
are 17:59 and 18:02. They are the hardware evidence behind D29. The D29 fix
allocated both direct vertex paths from `vita2d_pool_memalign`; D31 later
removed `vita2d_draw_array` entirely for a separate rendering issue (text and
traces drew nothing on hardware while rectangle/line chrome rendered). Either
change removes the stack path. The remaining action is the hardware re-test of
the current b2 VPK, not another draw-code change.

### 2. YTMV00001: a float was used as a pointer

The fault page is `0x3f800000`, which is exactly the IEEE-754 bit pattern of
`1.0f`. All four GPU cores fault on the same page, same MMU context (6), same
PDS requestor. That is the signature of every core fetching the same
vertex/uniform descriptor whose base address was built from float data, or of a
pointer field overwritten with `1.0f`. It is a bad GPU pointer, not a stack
address. The app is ViTube Music 1.2, not one of this workspace's projects;
report it upstream with the dump.

The capture also has GPU clock words `0xde, 0xde, 0x6f` = 222/222/111 MHz
(the PSVshell clock table in `agent-wiki/09-community-field-notes.md`), versus
the 111 MHz default. That does not explain a `1.0f` pointer, but it is a second
variable to remove when reproducing.

### 3. EMU4VPLUS: near-null depth/stencil surface

The fault page is `0x00008000` (32 KiB), requestor HSR depth/stencil/visibility.
HSR is trying to read a depth/stencil surface or visibility buffer from an
unmapped page. A near-null address here usually means the depth surface was
never created or bound, or a stale surface pointer survived a render-target
change. This is a resource-lifetime bug in the emulator's render path, not a
vertex-buffer bug. Test with depth testing disabled to isolate it.

### 4. The two lockups are a different failure class

EMU4VPLUS and VITASHELL both show `GPU_3D_LOCKUP_PHASE_FRAGMENT`: `TRIG_3D` is
set, there is no BIF/MMU fault, all four cores sit in ISP parameter fetch, and
the completion markers (`dcu-invalidation-complete`,
`region-headers-initialized`) are present. The parser's own diagnosis says it
"does not identify the malformed command, shader, or resource responsible."
Root-cause confidence is deliberately low.

The same signature in two unrelated apps points at a common trigger rather
than one app's bad pointer: a shader that never retires, an invalid command
stream, or system-level instability. PSVshell is loaded in the COCKPIT captures,
and the YTMV capture was at 222 MHz, so GPU overclock is a variable worth
eliminating before blaming either app.

## Usual culprit

Across this set, the common thread is **the GPU being given an address it
cannot access**, not a failed GPU. The PS Vita's SGX543 has its own MMU and page
tables; the CPU stack, `.bss`, and ordinary `malloc` memory are not GPU-mapped.
The recurring causes, in order of how often they explain homebrew GPUCRASH:

1. **A vertex or uniform buffer in CPU memory.** Stack, static, or plain
   `malloc` memory handed to `sceGxmSetVertexStream`/`vita2d_draw_array`. This
   is exactly the COCKPIT pair. `vita2d_draw_array` does not copy; the buffer
   must be in GPU-mapped memory until the scene is submitted. Use
   `vita2d_pool_memalign` or `sceGxmMapMemory`.
2. **A null, stale, or uninitialized resource pointer.** Depth/stencil/color
   surfaces and textures that were never created, were freed, or were replaced
   without rebinding. This is the `0x00008000` EMU4VPLUS fault.
3. **Type confusion / data written into a pointer field.** The `0x3f800000`
   (`1.0f`) YTMV fault is the clean example: a float ended up where an address
   belonged.
4. **Use-after-free or pool reset while the GPU still reads the buffer.** The
   buffer must live until `sceGxmEndScene`/submit, not just until the draw call
   returns.
5. **A genuine pipeline hang** with no fault address: shader loop, invalid
   command stream, or unstable clock. The dump can localize the phase but not
   the instruction.

## Recommendations

- **COCKPIT:** keep D29/D30 and the D31/D32 renderer change. Reinstall the
  current b2 VPK and confirm text, values, traces and hazard stripes render.
  The dump evidence explains the pre-fix crash, not the b1 blank-text fault; do
  not re-add `vita2d_draw_array` on the strength of this dump alone.
- **YTMV00001:** inspect the vertex/uniform setup for a float-to-pointer or
  uninitialized-struct path; reproduce at stock 111 MHz.
- **EMU4VPLUS:** audit depth/stencil surface creation, binding, and lifetime;
  reproduce with depth test off.
- **Lockups:** reproduce at stock clocks with PSVshell/taiHEN plugins disabled.
  If the hang persists, bisect the app's render path and capture
  `gpu pipeline` at the hang.
- **General:** run `psp2-gpucrash-parse triage *.psp2dmp` first, then
  `gpu faults` and `address` on each fault page. A fault page that matches a
  thread's `stack+size-0x1000` is a stack pointer; a page that is a small value
  or a float bit pattern is a bad resource pointer.

## Limits

The BIF fault record retains the faulting 4 KiB page, not the exact byte, and
the dump does not carry the SGX page tables. The parser cannot identify which
local array was passed, only that the page is the thread stack. The clock words
are raw producer words; the 41/111/222 MHz reading follows the PSVshell table,
not a decoded register. The `.tmp` files are not analyzable and are not part of
the GPU findings.

Both COCKPIT dumps fail one validation check, and only one: the fixed note set
lacks `APP_LIST_INFO`, `BUDGET_INFO`, `BUILD_VER_INFO`, `DEVICE_INFO`, `HW_INFO`,
and `SYS_DEVICE_INFO`. Their GPU, thread, stack, module, and summary checks all
pass, and `GPU_INFO` is the full 0x3404-byte version-5 export. The missing
notes are non-GPU metadata and do not affect the fault or stack evidence.

## Files

| File | SHA-256 |
|---|---|
| `psp2core-1793512064-GPUCRASH.psp2dmp` | `9944bbe4e6beb92c80f4fb8d04bdf481b4eda5d3c1ae9c9a632fa45e2582bf70` |
| `psp2core-1793512475-GPUCRASH.psp2dmp` | `86333507e3bbd3928425574ecf3c5f6b91b523cef2b3b6971fc4f305d9ff6e6d` |
| `psp2core-1793523757-GPUCRASH.psp2dmp` | `708a2168b474cbb72133d6df4e94b2ff382053aa6ad8f57dbce9569be8c87246` |
| `psp2core-1793524658-GPUCRASH.psp2dmp` | `a25f81f05ea25e298b6015e5bcef56dd67597bbf9982893de69cc146f8bd9a7d` |
| `psp2core-1793536179-GPUCRASH.psp2dmp` | `3dcc25a71de820eae1a70897a70025ab9d3f358569d9a750d203fc07332ba171` |
| `psp2core-1793536348-GPUCRASH.psp2dmp` | `ad9d3e117fd73b36a2d4b99bf8b99762bbbf14d683e77d78eb91cb93c868753a` |
| `psp2core-1793525382-...psp2dmp.tmp` | `df6f890a9116e3720d6e4a8ada2c2f912879163596c65bfff7dc0f560e4be155` |
| `psp2core-1793528648-...psp2dmp.tmp` | `9ab2510ee73affe7c6e67f8a49d9fb7e071858f8b34f10c22ed4bdec93208e5a` |
