# Examples

Copies of the key source files from the working projects. The canonical,
buildable projects are:

| Example | Canonical project |
|---|---|
| `cap-test/` | `vita-cap-test` |
| `kernel-demo/` | `vita-kernel-demo` |
| `cockpit/` | `vita-cockpit` |

## cap-test

- `CMakeLists.txt` — links vita2d, I/O, audio, network, SDL2, curl, mbedTLS,
  libarchive, libwebp, mpg123, opus, vorbis, FLAC.
- `main.c` — file I/O, PCM audio, WiFi/DNS/HTTP, library version checks.

## kernel-demo

- `CMakeLists.txt` — builds the `.skprx`, generates the user stub, builds the
  app, packages both in the VPK.
- `kernel-main.c` — taiHEN hook on `SceIofilemgr::_sceIoOpen`, custom exports.
- `app-main.c` — loads the `.skprx`, calls the generated stub, displays state.
- `exports.yml` — kernel module/library export description.

## cockpit

The reference for the workspace architecture rules: a policy layer with no SDK
call, an adapter that owns every `[C]` claim, and a data-driven layout.

- `platform.h` — the port. Plain C types, no `Sce*`, every reader sets `ok`.
- `platform_psp2.c` — the SDK adapter. The only file that samples the device;
  every `[C]` claim and its sanity check lives here.
- `panel.h` — the panel contract: a title, an accent, one draw function, and a
  layout slot that carries the rect and the chrome.
- `registry.c` — the asymmetric layout table. Geometry is data, not a formula.

These copies are for reading; build the canonical projects with their
`build.sh` scripts so paths and assets resolve correctly.
