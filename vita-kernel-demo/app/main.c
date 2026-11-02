#include <stdio.h>
#include <string.h>

#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/io/fcntl.h>
#include <vita2d.h>
#include <taihen.h>

/* Provided by the stub generated from the kernel plugin's exports.yml. */
extern int kdemo_get_open_count(void);
extern int kdemo_get_last_path(char *buf, int size);

static vita2d_pgf *font;

static void draw_text(int x, int y, unsigned int color, float scale,
                      const char *text)
{
    vita2d_pgf_draw_text(font, x, y, color, scale, text);
}

int main(void)
{
    SceCtrlData pad;
    SceUID modid;
    int count = -1;
    char last_path[256] = "none";
    char line[256];
    int loaded = 0;

    vita2d_init();
    vita2d_set_clear_color(RGBA8(0x10, 0x10, 0x18, 0xFF));
    font = vita2d_load_default_pgf();

    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    /*
     * Load the kernel plugin from the app's own directory.
     * This requires unsafe homebrew and taiHEN.
     */
    modid = taiLoadStartKernelModule("app0:kernel_demo.skprx", 0, NULL, 0);
    if (modid >= 0) {
        loaded = 1;
        count = kdemo_get_open_count();
        kdemo_get_last_path(last_path, sizeof(last_path));
    }

    /* Make one known file open so the hook has something to count. */
    SceUID fd = sceIoOpen("app0:eboot.bin", SCE_O_RDONLY, 0);
    if (fd >= 0)
        sceIoClose(fd);

    for (;;) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        if (pad.buttons & SCE_CTRL_START)
            break;

        if (loaded) {
            count = kdemo_get_open_count();
            kdemo_get_last_path(last_path, sizeof(last_path));
        }

        vita2d_start_drawing();
        vita2d_clear_screen();

        draw_text(30, 50, RGBA8(0xFF, 0xFF, 0xFF, 0xFF), 1.2f,
                  "Kernel plugin demo");

        if (loaded) {
            snprintf(line, sizeof(line), "Plugin: loaded (modid 0x%08X)", modid);
            draw_text(30, 110, RGBA8(0x80, 0xFF, 0x80, 0xFF), 0.9f, line);

            snprintf(line, sizeof(line), "Kernel opens seen: %d", count);
            draw_text(30, 150, RGBA8(0xC8, 0xC8, 0xC8, 0xFF), 0.9f, line);

            snprintf(line, sizeof(line), "Last path: %s", last_path);
            draw_text(30, 190, RGBA8(0xC8, 0xC8, 0xC8, 0xFF), 0.8f, line);

            draw_text(30, 250, RGBA8(0xA0, 0xA0, 0xA0, 0xFF), 0.8f,
                      "The .skprx is hooking SceIofilemgr::ksceIoOpen.");
            draw_text(30, 280, RGBA8(0xA0, 0xA0, 0xA0, 0xFF), 0.8f,
                      "It counts every successful kernel file open.");
        } else {
            snprintf(line, sizeof(line), "Plugin load failed: 0x%08X", modid);
            draw_text(30, 110, RGBA8(0xFF, 0x80, 0x80, 0xFF), 0.9f, line);

            draw_text(30, 160, RGBA8(0xA0, 0xA0, 0xA0, 0xFF), 0.8f,
                      "Check that HENkaku Settings -> Enable Unsafe");
            draw_text(30, 190, RGBA8(0xA0, 0xA0, 0xA0, 0xFF), 0.8f,
                      "Homebrew is enabled, and that taiHEN is active.");
        }

        draw_text(30, 500, RGBA8(0x80, 0x80, 0x80, 0xFF), 0.8f,
                  "Press START to exit.");

        vita2d_end_drawing();
        vita2d_swap_buffers();

        sceKernelDelayThread(100 * 1000);
    }

    /*
     * The plugin exports a syscall library, so the Vita normally refuses to
     * unload it. Leave it loaded; reboot the Vita to clear it.
     */

    vita2d_free_pgf(font);
    vita2d_fini();
    sceKernelExitProcess(0);
    return 0;
}
