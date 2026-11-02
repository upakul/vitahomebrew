# 02 — Toolchain

## What VitaSDK is

VitaSDK is the community's reverse-engineered SDK for the PS Vita. It contains:

| Piece | Purpose |
|---|---|
| `arm-vita-eabi-gcc` / `g++` | Cross-compiler |
| binutils | `ld`, `as`, `objdump`, `nm`, `readelf`, ... |
| newlib | C library port |
| pthread-embedded | POSIX threads |
| `vita-headers` | `psp2/` userland headers, `psp2kern/` kernel headers, NID DBs |
| `vita-toolchain` | Host tools for SELF/VPK packaging |
| `vdpm` | Package manager |
| `vita-makepkg` | Port third-party libraries into the SDK |

The compiler is configured for:

```text
--with-arch=armv7-a
--with-tune=cortex-a9
--with-fpu=neon
--with-float=hard        # default; softfp is an option
--with-mode=thumb
```

So the default ABI is ARMv7-A, Cortex-A9-tuned, NEON, hard-float, Thumb.

## Install

### macOS / Linux

```sh
export VITASDK="$PWD/vitasdk"
export PATH="$VITASDK/bin:$PATH"

git clone https://github.com/vitasdk/vdpm
cd vdpm
./bootstrap-vitasdk.sh

vdpm status
```

The current supported series in late 2026 is **2026.08**. The bootstrap installs
into `$VITASDK` and records the installation in the SDK package database.

In this workspace, `vitasdk/` already exists at the workspace root. From
`/Users/ghostmeson/developer/builds/psvitahomebrew`:

```sh
export VITASDK="$PWD/vitasdk"
export PATH="$VITASDK/bin:$PATH"
```

### Docker

```sh
docker run --rm -v "$PWD:/workspace" vitasdk/vitasdk:2026.08 \
  sh -c 'cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake && cmake --build build'
```

The full image is ~1.6 GB; `-minimal` is ~764 MB but lacks target packages.
Tags are multi-arch (`linux/amd64`, `linux/arm64`).

## Installed packages in this environment

```text
bzip2 1.0.8-1
curl-mbedtls 8.22.0-1
flac 1.3.4-1
freetype 2.14.3-1
harfbuzz 14.5.0-1
imgui-vita2d 0.0.0.r19.ga849382-1
jsoncpp 1.9.8-1
libarchive 3.8.9-1
libconfig 1.8.2-2
libjpeg-turbo 3.2.0-1
libmodplug 0.8.9.0.r461.gd1b97ed-2
libogg 1.3.6-2
libpng 1.6.58-1
libvita2d 0.0.0.r188.ga8f15ab-1
libvorbis 1.3.7-3
libwebp 1.6.0-1
libxmp-lite 4.7.3-1
mbedtls 3.6.5-1
mpg123 1.33.7-1
opus 1.6.1-1
opusfile 0.12-1
sdl2 2.32.8-1
sdl2_gfx 1.0.4-2
sdl2_image 2.8.12-1
sdl2_mixer 2.8.2-1
sdl2_ttf 2.24.0-1
taihen 0.11-1
vdpm 0.1.3-1
vitasdk-core 2026.08.1-1
zlib 1.3.2-2
zstd 1.5.7-1
```

`vdpm list` is the source of truth.

## `vdpm` commands

```sh
vdpm install <pkg>...    # install packages and dependencies
vdpm remove <pkg>...     # remove
vdpm upgrade             # package fixes within the release
vdpm refresh 2026.08     # move/re-sync a release series
vdpm list                # installed packages
vdpm search <term>       # search the repository
vdpm info <pkg>          # dependencies, sizes, version
vdpm files <pkg>         # files owned by a package
vdpm status              # release/sequence/toolchain/package state
vdpm channels            # available release channels
```

`vdpm install` prompts for confirmation; in scripts use:

```sh
yes | vdpm install <pkg>
```

## Paths

| Path | Contents |
|---|---|
| `$VITASDK/bin` | host tools and cross-compiler |
| `$VITASDK/arm-vita-eabi/bin` | target binutils |
| `$VITASDK/arm-vita-eabi/include` | headers (`psp2/`, `psp2kern/`, third-party) |
| `$VITASDK/arm-vita-eabi/lib` | stubs and libraries |
| `$VITASDK/share/vita.cmake` | CMake helper macros |
| `$VITASDK/share/vita.toolchain.cmake` | CMake toolchain file |
| `$VITASDK/share/vita-headers/db/` | NID databases |

The `yml_template/` directory is in the `vita-toolchain` source repository, not
installed under `$VITASDK/share/`.
| `$VITASDK/share/gcc-arm-vita-eabi/samples` | SDK samples |

## Host tools

| Tool | Input → output | Purpose |
|---|---|---|
| `vita-elf-create` | ELF → VELF | Adds SCE module info and import/export tables |
| `vita-make-fself` | VELF → FSELF | SCE header, compression, authid, ASLR |
| `vita-mksfoex` | title → `param.sfo` | System metadata |
| `vita-pack-vpk` | SELF + SFO + assets → VPK | ZIP package |
| `vita-elf-export` | ELF + config → YAML | NID/import mapping for custom libraries |
| `vita-libs-gen` | YAML → `lib*_stub.a` | Userland stubs |
| `vita-libs-gen-2` | same, no 1024-entry limit | Alternative stub generator |
| `vita-nid-check` | NID DB | Validate sorting/duplicates/firmware consistency |
| `vita-makepkg` | `VITABUILD` | Build a package into the SDK |

## `vita-makepkg`

To port a library:

```sh
git clone https://github.com/vitasdk/packages
cd packages/<library>
vita-makepkg
vdpm install ./<name>-<version>-arm.tar.xz
```

A `VITABUILD` describes source, dependencies, build, and install. Use the
existing recipes as templates.

## Verification

```sh
arm-vita-eabi-gcc --version
arm-vita-eabi-gcc -dumpmachine        # arm-vita-eabi
arm-vita-eabi-gcc -v 2>&1 | tail -20  # configured arch/fpu/float
vdpm status
ls "$VITASDK/arm-vita-eabi/include/psp2"
ls "$VITASDK/arm-vita-eabi/lib" | grep SceIofilemgr
```

## Common setup mistakes

| Mistake | Result | Fix |
|---|---|---|
| `VITASDK` not exported | CMake cannot find toolchain | `export VITASDK=...` |
| `$VITASDK/bin` not on `PATH` | `arm-vita-eabi-gcc not found` | `export PATH=...` |
| Toolchain file set after `project()` | Wrong compiler / host build | Set `CMAKE_TOOLCHAIN_FILE` before `project()` |
| Mixing SDK versions | Missing NIDs/stubs, link errors | Reinstall/refresh the SDK; don't mix trees |
| Linking a real library instead of `*_stub` | `Unable to find module with NID ...` | Link `libSce*_stub.a` |
| Using a system `libelf` when building the toolchain | `vita-elf-create` segfault | Use prebuilt VitaSDK or libelf 0.8.13 |

## Package selection guide

| Need | Package(s) |
|---|---|
| 2D drawing/UI | `libvita2d`, `imgui-vita2d` |
| SDL port | `sdl2`, `sdl2_image`, `sdl2_mixer`, `sdl2_ttf`, `sdl2_gfx` |
| MP3 | `mpg123` |
| OGG/Vorbis | `libvorbis`, `libogg` |
| Opus | `opus`, `opusfile` |
| FLAC | `flac` |
| MOD/tracker | `libmodplug`, `libxmp-lite` |
| HTTPS/HTTP client | `curl-mbedtls`, `mbedtls` |
| ZIP/TAR | `libarchive` |
| WebP | `libwebp` |
| Config | `libconfig` |
| JSON (C++) | `jsoncpp` |
| Kernel plugins | `taihen` |
| PNG/JPEG/FreeType | pulled by `libvita2d` |

The core Sony stubs for file I/O, audio, networking, camera, touch, motion,
dialogs, power, and RTC are part of `vitasdk-core`; no extra package needed.
