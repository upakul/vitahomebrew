#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/audioout.h>
#include <psp2/sysmodule.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <vita2d.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#include <curl/curl.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <archive.h>
#include <webp/decode.h>
#include <mpg123.h>
#include <opus/opus.h>
#include <vorbis/codec.h>
#include <FLAC/stream_decoder.h>
#include <mbedtls/version.h>

#define MAX_LINES 20
#define LINE_LEN  160

static char lines[MAX_LINES][LINE_LEN];
static int line_count;
static vita2d_pgf *font;

static void add_line(const char *fmt, ...)
{
    if (line_count >= MAX_LINES)
        return;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(lines[line_count], LINE_LEN, fmt, ap);
    va_end(ap);
    line_count++;
}

static void draw_screen(void)
{
    int y = 92;

    vita2d_start_drawing();
    vita2d_clear_screen();

    vita2d_pgf_draw_text(font, 30, 50,
                         RGBA8(0xFF, 0xFF, 0xFF, 0xFF),
                         1.2f, "Vita capability test");

    for (int i = 0; i < line_count; i++) {
        vita2d_pgf_draw_text(font, 30, y,
                             RGBA8(0xC8, 0xC8, 0xC8, 0xFF),
                             0.82f, lines[i]);
        y += 24;
    }

    vita2d_pgf_draw_text(font, 30, 520,
                         RGBA8(0x80, 0x80, 0x80, 0xFF),
                         0.8f, "Press START to exit.");

    vita2d_end_drawing();
    vita2d_swap_buffers();
}

static void test_io(void)
{
    const char *dir = "ux0:data/vita-cap-test";
    const char *path = "ux0:data/vita-cap-test/io.txt";
    const char *message = "Vita file I/O works.";
    char buffer[64] = {0};

    /* EEXIST is fine; the open below is the real test. */
    sceIoMkdir(dir, 0777);

    SceUID fd = sceIoOpen(path,
                          SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd < 0) {
        add_line("I/O: open for write failed 0x%08X", fd);
        return;
    }

    int written = sceIoWrite(fd, message, strlen(message));
    sceIoClose(fd);

    if (written != (int)strlen(message)) {
        add_line("I/O: write failed 0x%08X", written);
        return;
    }

    fd = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd < 0) {
        add_line("I/O: open for read failed 0x%08X", fd);
        return;
    }

    int read = sceIoRead(fd, buffer, sizeof(buffer) - 1);
    sceIoClose(fd);

    if (read <= 0) {
        add_line("I/O: read failed 0x%08X", read);
        return;
    }

    buffer[read] = 0;
    if (strcmp(buffer, message) == 0)
        add_line("I/O: OK (wrote and read %d bytes)", read);
    else
        add_line("I/O: mismatch: '%s'", buffer);
}

static void test_audio(void)
{
    enum {
        SAMPLES = 256,
        RATE = 44100,
        BLOCKS = 86
    };
    int16_t buffer[SAMPLES];

    int port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM,
                                   SAMPLES, RATE,
                                   SCE_AUDIO_OUT_MODE_MONO);
    if (port < 0) {
        add_line("Audio: open failed 0x%08X", port);
        return;
    }

    for (int block = 0; block < BLOCKS; block++) {
        for (int i = 0; i < SAMPLES; i++) {
            float t = (float)(block * SAMPLES + i) / (float)RATE;
            float sample = sinf(2.0f * 3.14159265358979323846f * 440.0f * t);
            buffer[i] = (int16_t)(12000.0f * sample);
        }

        int ret = sceAudioOutOutput(port, buffer);
        if (ret < 0) {
            sceAudioOutReleasePort(port);
            add_line("Audio: output failed 0x%08X", ret);
            return;
        }
    }

    sceAudioOutReleasePort(port);
    add_line("Audio: OK (440 Hz mono, 0.5 s)");
}

static char net_memory[1024 * 1024];

static void test_network(void)
{
    int ret;
    SceNetCtlInfo info;
    char ip[16] = "unknown";
    int net_initialized = 0;
    int ctl_initialized = 0;

    ret = sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    if (ret < 0)
        add_line("Net: sysmodule load 0x%08X", ret);

    SceNetInitParam param;
    param.memory = net_memory;
    param.size = sizeof(net_memory);
    param.flags = 0;

    ret = sceNetInit(&param);
    if (ret < 0) {
        add_line("Net: sceNetInit failed 0x%08X", ret);
        goto done;
    }
    net_initialized = 1;

    ret = sceNetCtlInit();
    if (ret < 0) {
        add_line("NetCtl: init failed 0x%08X", ret);
    } else {
        ctl_initialized = 1;
        memset(&info, 0, sizeof(info));
        ret = sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_IP_ADDRESS, &info);
        if (ret >= 0)
            snprintf(ip, sizeof(ip), "%s", info.ip_address);
        else
            add_line("NetCtl: IP query failed 0x%08X", ret);
    }

    add_line("WiFi IP: %s", ip);

    struct hostent *host = gethostbyname("example.com");
    if (!host) {
        add_line("DNS: example.com lookup failed");
        goto done;
    }

    add_line("DNS: example.com -> %s",
             inet_ntoa(*(struct in_addr *)host->h_addr_list[0]));

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        add_line("Socket: create failed 0x%08X", sock);
        goto done;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    memcpy(&addr.sin_addr, host->h_addr_list[0], host->h_length);

    ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        add_line("HTTP: connect failed 0x%08X", ret);
        close(sock);
        goto done;
    }

    const char *request =
        "HEAD / HTTP/1.0\r\n"
        "Host: example.com\r\n"
        "Connection: close\r\n"
        "\r\n";

    ret = send(sock, request, strlen(request), 0);
    if (ret < 0) {
        add_line("HTTP: send failed 0x%08X", ret);
        close(sock);
        goto done;
    }

    char response[256] = {0};
    ret = recv(sock, response, sizeof(response) - 1, 0);
    close(sock);

    if (ret > 0) {
        char *end = strchr(response, '\r');
        if (end)
            *end = 0;
        add_line("HTTP: %s", response);
    } else {
        add_line("HTTP: recv failed 0x%08X", ret);
    }

done:
    if (ctl_initialized)
        sceNetCtlTerm();
    if (net_initialized)
        sceNetTerm();
    sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
}

static void test_libraries(void)
{
    SDL_version sdl_version;
    const SDL_version *mixer_version;
    int webp_version;
    char mbedtls_version[64];

    add_line("curl: %s", curl_version());

    SDL_GetVersion(&sdl_version);
    add_line("SDL2: %d.%d.%d",
             sdl_version.major, sdl_version.minor, sdl_version.patch);

    mixer_version = Mix_Linked_Version();
    add_line("SDL_mixer: %d.%d.%d",
             mixer_version->major, mixer_version->minor, mixer_version->patch);

    add_line("libarchive: %s", archive_version_string());

    webp_version = WebPGetDecoderVersion();
    add_line("libwebp: %d.%d.%d",
             (webp_version >> 16) & 0xFF,
             (webp_version >> 8) & 0xFF,
             webp_version & 0xFF);

    if (mpg123_init() == MPG123_OK) {
        add_line("mpg123: init OK");
        mpg123_exit();
    } else {
        add_line("mpg123: init failed");
    }

    add_line("opus: %s", opus_get_version_string());
    add_line("vorbis: %s", vorbis_version_string());

    FLAC__StreamDecoder *decoder = FLAC__stream_decoder_new();
    if (decoder) {
        add_line("FLAC: %s", FLAC__VERSION_STRING);
        FLAC__stream_decoder_delete(decoder);
    } else {
        add_line("FLAC: decoder allocation failed");
    }

    mbedtls_version_get_string_full(mbedtls_version);
    add_line("mbedTLS: %s", mbedtls_version);
}

int main(void)
{
    SceCtrlData pad;

    vita2d_init();
    vita2d_set_clear_color(RGBA8(0x10, 0x10, 0x18, 0xFF));
    font = vita2d_load_default_pgf();

    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    add_line("Running tests...");
    draw_screen();

    test_io();
    test_audio();
    test_network();
    test_libraries();

    for (;;) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        if (pad.buttons & SCE_CTRL_START)
            break;

        draw_screen();
        sceKernelDelayThread(16 * 1000);
    }

    vita2d_free_pgf(font);
    vita2d_fini();

    sceKernelExitProcess(0);
    return 0;
}
