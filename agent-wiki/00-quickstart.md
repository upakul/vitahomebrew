# 00 — Quickstart

This is the shortest path from a clean Mac to a running Vita app and a kernel
plugin demo.

Run commands from the workspace root:

```sh
cd /Users/ghostmeson/developer/builds/psvitahomebrew
```

This workspace already contains `vitasdk/`, the three projects, and this wiki.
If `vitasdk/` exists, skip step 2.

Minimal path: steps 1, 2, 3 (minimal package line), 4, 5, 7, 8, 9. Step 6 is
optional and needs the full capability-test stack from step 3.

## 1. Host prerequisites (macOS)

```sh
xcode-select --install
brew install cmake git
```

## 2. Install VitaSDK 2026.08

```sh
export VITASDK="$PWD/vitasdk"
export PATH="$VITASDK/bin:$PATH"

git clone https://github.com/vitasdk/vdpm
(cd vdpm && ./bootstrap-vitasdk.sh)

vdpm status
```

Expected:

```text
Release   2026.08
Sequence  2026102473
Toolchain sdk-snapshot-20260825.611.1
Packages  packages-2026.08-snapshot-20261024.61.1
Installed 2026.08.1-1
```

Add the exports to `~/.zshrc`:

```sh
export VITASDK="/Users/ghostmeson/developer/builds/psvitahomebrew/vitasdk"
export PATH="$VITASDK/bin:$PATH"
```

## 3. Install packages (minimal + optional capability stack)

Minimal app + kernel demo:

```sh
yes | vdpm install libvita2d taihen
```

Optional: the full capability-test stack (SDL2, codecs, curl, archive, etc.):

```sh
yes | vdpm install \
  imgui-vita2d \
  sdl2 sdl2_image sdl2_mixer sdl2_ttf sdl2_gfx \
  mpg123 libvorbis libogg opus opusfile flac \
  curl-mbedtls mbedtls zstd \
  libarchive libwebp \
  libconfig jsoncpp
```

This covers:

- 2D UI (`vita2d`, ImGui)
- SDL2 and its image/mixer/font add-ons
- MP3, OGG/Vorbis, Opus, FLAC, MOD
- HTTPS (`curl` + mbedTLS)
- ZIP/TAR (`libarchive`), WebP, PNG/JPEG/FreeType
- config files (`libconfig`), JSON (`jsoncpp`)
- taiHEN headers and stubs

## 4. Verify the toolchain

```sh
arm-vita-eabi-gcc --version
vita-elf-create --help 2>&1 | head
vdpm list
```

## 5. Build the minimal app

```sh
(cd vita-cockpit && ./build.sh)
```

Output:

```text
vita-cockpit/build/cockpit-v<version>-b<build>.vpk
```

The VPK contains:

```text
sce_sys/param.sfo
eboot.bin
sce_sys/icon0.png
sce_sys/livearea/contents/bg.png
sce_sys/livearea/contents/startup.png
sce_sys/livearea/contents/template.xml
```

The app is **safe** (`authid 0x2F00000000000002`).

## 6. Optional: build the capability test

```sh
(cd vita-cap-test && ./build.sh)
```

Output:

```text
vita-cap-test/build/cap-test-v<version>-b<build>.vpk
```

This app is deliberately **unsafe** (`authid 0x2F00000000000001`) to exercise
the full userland permission profile. Writing to `ux0:data` alone does not
require `UNSAFE`; safe eboots may use `ux0:data`, `app0:` (read-only), and
`savedata0:`. It tests file I/O, PCM audio, WiFi IP, DNS, HTTP, and library
versions.

## 7. Build the kernel demo

```sh
(cd vita-kernel-demo && ./build.sh)
```

Output:

```text
vita-kernel-demo/build/kernel_demo.skprx
vita-kernel-demo/build/kernel-demo-v<version>-b<build>.vpk
```

This app loads the `.skprx` on demand, hooks `SceIofilemgr::_sceIoOpen`, and
reads kernel state through a generated syscall stub.

## 8. Prepare the Vita

You need:

- 3.65 permanent CFW (Enso) — this is the assumed target.
- VitaShell installed.
- **Settings → HENkaku Settings → Enable Unsafe Homebrew** for the cap test and
  kernel demo.
- Vita and Mac on the same network.

In VitaShell, press **Select** to start FTP. Note the IP.

## 9. Upload and install

```sh
(cd vita-cockpit && ./send.sh 192.168.1.42)
(cd vita-cap-test && ./send.sh 192.168.1.42)
(cd vita-kernel-demo && ./send.sh 192.168.1.42)
```

Replace the IP with the one VitaShell shows.

On the Vita:

1. Open VitaShell.
2. Go to `ux0:data`.
3. Select a `.vpk` and press **Cross**.
4. Launch the new bubble.

## 10. Recovery card

If a kernel plugin or app misbehaves:

1. **Hold L during boot** to skip taiHEN plugin loading.
2. If it still fails, enter **Safe Mode**:
   power off, then hold **R + Power + PS** until the PS logo appears.
3. Try **Restart** first; **Rebuild Database** if needed; **Restore the PS Vita
   System** as the heavy option.
4. Because `vita-kernel-demo` loads the plugin on demand, a normal reboot clears
   it. It is **not** in `ur0:tai/config.txt`.

## Common first-run failures

| Symptom | Fix |
|---|---|
| `VitaSDK not found` | Export `VITASDK`; run `./build.sh` from the project dir |
| `arm-vita-eabi-gcc not found` | Add `$VITASDK/bin` to `PATH` |
| `vita2d.h: No such file` | `vdpm install libvita2d` |
| VPK install fails at 99% (`0x8010113D`) | Re-encode LiveArea PNGs as indexed/palette |
| App installs but does not launch (`C2-12828-1`) | Check unsafe setting, pull core dump, keep ELF |
| Kernel demo says load failed | Enable unsafe homebrew; reboot to clear a previous load |
| FTP upload fails | VitaShell FTP not running, wrong IP, port 1337 blocked |

See [10-troubleshooting.md](10-troubleshooting.md) for the full list.
