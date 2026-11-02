# Vita Capability Test

A single homebrew app that exercises the parts you asked about:

- **File I/O**: creates `ux0:data/vita-cap-test`, writes a file, reads it back.
- **Audio**: opens a BGM audio port and plays a 0.5 s 440 Hz sine wave.
- **WiFi**: loads `SceNet`, initializes the network stack, reads the IP with
  `SceNetCtl`, resolves `example.com`, connects to port 80, and sends an HTTP
  HEAD request.
- **Libraries**: prints versions from curl, SDL2, SDL_mixer, libarchive,
  libwebp, mpg123, opus, vorbis, FLAC, and mbedTLS.

It is deliberately built as an **UNSAFE** eboot to exercise the full userland
permission profile. Writing to `ux0:data` alone does not require `UNSAFE`; safe
eboots may use `ux0:data`, `app0:` (read-only), and `savedata0:`.

## Build

```sh
./build.sh
```

Output:

```text
build/cap-test-v<version>-b<build>.vpk
```

## Install

On the Vita, open VitaShell and press **Select** to start FTP, then:

```sh
./send.sh <vita-ip>
```

Install `cap-test-v<version>-b<build>.vpk` from `ux0:data`, then launch the bubble.

## What the result means

| Line | Meaning |
|---|---|
| `I/O: OK` | `sceIoOpen`/`sceIoWrite`/`sceIoRead` work on the memory card |
| `Audio: OK` | `sceAudioOut` opened and played PCM |
| `WiFi IP: ...` | `SceNetCtl` returned an IP address |
| `DNS: ...` | `gethostbyname` resolved `example.com` |
| `HTTP: HTTP/1.x ...` | a TCP connection and HTTP request succeeded |
| Library versions | the corresponding libraries linked and are callable |

If WiFi is off or the Vita has no route to the internet, the I/O and audio
tests can still pass; the network lines will show an error code.

## Notes

- The app needs **Enable Unsafe Homebrew** in HENkaku Settings.
- The title ID is `CAPT00001`. Change it before publishing.
- The LiveArea PNGs are the CC0 art from the VitaSDK samples.
