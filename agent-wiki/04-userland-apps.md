# 04 — Userland Apps

## Minimal project layout

```text
my-app/
├── CMakeLists.txt
├── src/main.c
└── sce_sys/
    ├── icon0.png
    ├── pic0.png                 # recommended
    └── livearea/contents/
        ├── bg.png
        ├── startup.png
        └── template.xml
```

## CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)

# Must be set before project().
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake"
        CACHE PATH "Vita toolchain")
  else()
    message(FATAL_ERROR "Set VITASDK to your VitaSDK root")
  endif()
endif()

project(my_app C)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "My App")
set(VITA_TITLEID  "MYAP00001")  # exactly 9 chars, unique
set(VITA_VERSION  "01.00")      # must match ##.##

set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -s VERSION=${VITA_VERSION}")
set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -s CONTENT_ID=HB0001-${VITA_TITLEID}_00-0000000000000000")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -std=gnu11 -g")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--defsym=__sce_headroom=0x10000")

add_executable(my_app src/main.c)

target_link_libraries(my_app
  vita2d
  SceDisplay_stub
  SceGxm_stub
  SceSysmodule_stub
  SceCtrl_stub
  ScePgf_stub
  ScePvf_stub
  SceCommonDialog_stub
  SceAppMgr_stub
  SceLibKernel_stub
  SceIofilemgr_stub
  freetype
  png
  jpeg
  z
  m
)

# Default: safe eboot (authid 0x2F00000000000002).
# Add UNSAFE only when you need private APIs/full filesystem.
vita_create_self(my_app.self my_app)

vita_create_vpk(my_app.vpk ${VITA_TITLEID} my_app.self
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE sce_sys/icon0.png sce_sys/icon0.png
  FILE sce_sys/pic0.png sce_sys/pic0.png
  FILE sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
  FILE sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
  FILE sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
)
```

## A vita2d main loop

```c
#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <vita2d.h>

int main(void)
{
    SceCtrlData pad;
    vita2d_pgf *font;

    vita2d_init();
    vita2d_set_clear_color(RGBA8(0x18, 0x18, 0x24, 0xFF));
    font = vita2d_load_default_pgf();

    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    for (;;) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        if (pad.buttons & SCE_CTRL_START)
            break;

        vita2d_start_drawing();
        vita2d_clear_screen();

        vita2d_pgf_draw_text(font, 100, 150,
                             RGBA8(0xFF, 0xFF, 0xFF, 0xFF),
                             1.5f, "Hello, Vita!");

        vita2d_end_drawing();
        vita2d_swap_buffers();
    }

    vita2d_free_pgf(font);
    vita2d_fini();
    sceKernelExitProcess(0);
    return 0;
}
```

Key points:

- Load the font once, not every frame.
- `vita2d_start_drawing` / `vita2d_end_drawing` / `vita2d_swap_buffers` per frame.
- `vita2d_fini()` waits for the GPU before freeing assets.
- `sceKernelExitProcess(0)` or return from `main`.

## App lifecycle

- `main()` runs on the process's main thread.
- Create threads with:

```c
int worker(SceSize argc, void *argp) { return 0; }

SceUID thid = sceKernelCreateThread("worker", worker, 0x40, 0x4000, 0, 0, NULL);
sceKernelStartThread(thid, 0, NULL);
sceKernelWaitThreadEnd(thid, NULL, NULL);
```

- Sleep with `sceKernelDelayThread(us)`.
- Exit with `sceKernelExitProcess(0)`.
- Handle suspend/resume with `sceAppMgrReceiveSystemEvent` if needed.

## Memory

```c
SceUID block = sceKernelAllocMemBlock(
    "buffer", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, 1024 * 1024, NULL);

void *base;
sceKernelGetMemBlockBase(block, &base);

// use base ...

sceKernelFreeMemBlock(block);
```

- The Vita has 512 MB main + 128 MB VRAM, shared with the OS and other apps.
- Normal app memory budget is the default; `MEMSIZE` is usually left 0.
- `ATTRIBUTE2=12` requests the enlarged-memory profile (~+109 MiB, community
  measurement). Use only if needed.
- GPU memory is managed by `SceGxm` / `vita2d`; don't assume all 128 MB is yours.

## File I/O

Headers:

```c
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
```

Write/read:

```c
const char *dir = "ux0:data/my-app";
const char *path = "ux0:data/my-app/state.txt";
char buf[128];

sceIoMkdir(dir, 0777);  // EEXIST is fine

SceUID fd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
if (fd >= 0) {
    sceIoWrite(fd, "hello", 5);
    sceIoClose(fd);
}

fd = sceIoOpen(path, SCE_O_RDONLY, 0);
if (fd >= 0) {
    int n = sceIoRead(fd, buf, sizeof(buf) - 1);
    sceIoClose(fd);
    if (n > 0) buf[n] = 0;
}
```

Rules:

- `app0:` is the installed app directory; treat it as **read-only**.
- Put writable data in `ux0:data/<your-app>/`.
- Save data goes through `SceAppUtil` / `savedata0:`.
- `sceIoMkdir`, `sceIoGetstat`, `sceIoDopen`/`sceIoDread`, `sceIoRemove`,
  `sceIoRename`, `sceIoRmdir` are available.
- A safe eboot has restricted filesystem access; use `UNSAFE` only if you need
  arbitrary `ux0:`/`ur0:` access.

## Audio (PCM)

Header:

```c
#include <math.h>
#include <psp2/audioout.h>
```

```c
enum { SAMPLES = 256, RATE = 44100 };
int16_t buffer[SAMPLES];

int port = sceAudioOutOpenPort(
    SCE_AUDIO_OUT_PORT_TYPE_BGM, SAMPLES, RATE,
    SCE_AUDIO_OUT_MODE_MONO);

if (port >= 0) {
    for (int i = 0; i < SAMPLES; i++) {
        float t = (float)i / (float)RATE;
        buffer[i] = (int16_t)(12000.0f * sinf(2.0f * 3.14159265f * 440.0f * t));
    }
    sceAudioOutOutput(port, buffer);
    sceAudioOutReleasePort(port);
}
```

- Sample rates: 8 kHz–48 kHz.
- Mono/stereo; `SCE_AUDIO_MAX_LEN` 65472, `SCE_AUDIO_MIN_LEN` 64.
- `sceAudioOutOutput` blocks until the buffer is queued.
- For compressed audio, use `SDL2_mixer`, `mpg123`, `libvorbis`, `opusfile`,
  `flac`, or decode yourself and feed PCM to `SceAudio`.

## WiFi / networking

Headers:

```c
#include <string.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
```

Initialize:

```c
static char net_memory[1024 * 1024];

sceSysmoduleLoadModule(SCE_SYSMODULE_NET);

SceNetInitParam param = {
    .memory = net_memory,
    .size = sizeof(net_memory),
    .flags = 0,
};
sceNetInit(&param);
sceNetCtlInit();
```

Get IP:

```c
SceNetCtlInfo info;
memset(&info, 0, sizeof(info));
if (sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_IP_ADDRESS, &info) >= 0)
    printf("IP: %s\n", info.ip_address);
```

TCP client with BSD sockets:

```c
struct hostent *host = gethostbyname("example.com");
if (!host || !host->h_addr_list[0]) {
    /* handle DNS failure, then return from your function */
    return;
}

int s = socket(AF_INET, SOCK_STREAM, 0);

struct sockaddr_in addr;
memset(&addr, 0, sizeof(addr));
addr.sin_family = AF_INET;
addr.sin_port = htons(80);
memcpy(&addr.sin_addr, host->h_addr_list[0], host->h_length);

connect(s, (struct sockaddr *)&addr, sizeof(addr));

const char *request =
    "HEAD / HTTP/1.0\r\n"
    "Host: example.com\r\n"
    "\r\n";
send(s, request, strlen(request), 0);

char response[256];
int n = recv(s, response, sizeof(response) - 1, 0);
close(s);
```

Cleanup:

```c
sceNetCtlTerm();
sceNetTerm();
sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
```

Notes:

- `sceNetInit` needs about 1 MB of memory (the sample uses a static 1 MB buffer).
- `SceNetCtl` gives IP/netmask/gateway/DNS/SSID/link status.
- `SceHttp`/`SceSsl` are available as Sony APIs; `curl-mbedtls` is available for
  a higher-level HTTP/HTTPS client.
- Plain HTTP is enough to prove connectivity; HTTPS needs a CA setup for curl or
  the Sony SSL APIs.

## Input, touch, motion, camera

| Input | Header / stub | Example |
|---|---|---|
| Buttons/analog | `psp2/ctrl.h`, `SceCtrl_stub` | `sceCtrlPeekBufferPositive(0, &pad, 1)` |
| Touch | `psp2/touch.h`, `SceTouch_stub` | `sceTouchPeek(SCE_TOUCH_PORT_FRONT, &data, 1)` |
| Motion | `psp2/motion.h`, `SceMotion_stub` | `sceMotionGetState(&state)` |
| Camera | `psp2/camera.h`, `SceCamera_stub` | `sceCameraOpen`, `sceCameraRead` |
| Microphone | `psp2/audioin.h`, `SceAudioIn_stub` | `sceAudioInOpenPort` |

## UI and graphics options

| Library | Best for | Notes |
|---|---|---|
| `vita2d` | 2D apps, tools, simple UI | GPU-accelerated; textures, shapes, PGF/PVF text |
| `imgui-vita2d` | Debug/tool UIs | Immediate-mode UI on top of vita2d |
| SDL2 | Ports/cross-platform | `find_package(SDL2 REQUIRED)`; `SDL2::SDL2` |
| `vitaGL` | OpenGL ports | Requires `libshacccg.suprx` extracted on console |
| Raw `SceGxm` | Maximum control | Shaders, render targets, manual sync |

For a homebrew app rather than a 3D game, `vita2d` + optional `imgui-vita2d`
is the recommended stack.

## Images, fonts, archives, config

| Need | Library |
|---|---|
| PNG | `libpng` (pulled by libvita2d) |
| JPEG | `libjpeg-turbo` |
| WebP | `libwebp` |
| Fonts | `freetype`, `harfbuzz`, vita2d PGF/PVF |
| ZIP/TAR | `libarchive` |
| Config | `libconfig` |
| JSON | `jsoncpp` (C++) or `SceLibJson` system stub |
| Database | `SceSqlite` system stub (no user-space sqlite3 package) |

## Example projects

| Project | Demonstrates |
|---|---|
| `vita-cap-test` | File I/O, PCM audio, WiFi/DNS/HTTP, library version checks |
| `vita-kernel-demo` | App + kernel plugin + custom syscall |

See [12-projects.md](12-projects.md) for code walkthroughs.

## Gotchas

- **Safe vs unsafe:** safe is the normal default; `UNSAFE` is a system-wide
  permission relaxation. Don't ship unsafe unless needed.
- **`app0:` is read-only.** Use `ux0:data/<app>`.
- **Title ID must be unique** and 9 uppercase characters.
- **Version must be `##.##`**.
- **LiveArea PNGs should be indexed/palette** (color type 3); non-indexed PNGs
  can fail install with `0x8010113D`.
- **Keep the unstripped ELF** for crash dumps.
- **Add `__sce_headroom`** as code grows.
- **Don't mix SDK versions**; link `*_stub` libraries, not real Sony libs.
