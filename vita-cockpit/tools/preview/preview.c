#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "app/screen.h"
#include "ui/ui_state.h"
#include "core/history.h"
#include "core/metrics.h"
#include "core/plugin_editor.h"
#include "core/platform.h"
#include "core/plugins.h"
#include "gfx/layout.h"
#include "panels/plugins.h"
#include "panels/registry.h"
#include "ui/modal.h"
#include "fake_platform.h"

/*
 * Host preview renderer.
 *
 * Draws the real screen through the shipped composition (app/screen.c), the
 * real theme, panels and header, a software vita2d shim, and the shared fake
 * platform. It is the only way to look at the layout without a Vita, and it is
 * deliberately outside the app: none of this is linked into cockpit.self.
 *
 * Scenes are produced by driving the fake port and the real sampler, never by
 * writing g_metrics directly -- so a PNG here proves the policy ran (D26).
 *
 * Run with tools/preview/run.sh.
 */

extern void preview_clear(void);
extern const uint32_t *preview_pixels(void);
extern int preview_width(void);
extern int preview_height(void);
extern int preview_draw_calls(void);

/*
 * Ceiling on one frame's draw calls. The renderer deliberately issues one call
 * per rect (D31), so a full frame with the hazard band up is ~3,000. The budget
 * catches accidental growth, not the batching that was removed.
 */
#define DRAW_CALL_BUDGET 6500

/* 200 slow samples at 10 Hz fills the 20 s window the trace claims. */
static void seed_trace(void)
{
    for (int i = 0; i < HISTORY_CAP; ++i) {
        float ph = (float)i * 0.06f;
        int mem = 182 + (int)(9.0f * sinf(ph) + 3.0f * sinf(ph * 2.7f));
        float ms = 16.6f + 1.6f * sinf(ph * 1.9f);

        if (i % 47 == 0)
            ms += 9.0f;

        fake_set_memory_mb(mem);
        metrics_sample_frame(ms / 1000.0f);
        metrics_sample_slow();
    }
}

static void be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static void png_chunk(FILE *f, const char *type, const uint8_t *data,
                      uint32_t len)
{
    uint8_t hdr[8];
    be32(hdr, len);
    memcpy(hdr + 4, type, 4);
    fwrite(hdr, 1, sizeof(hdr), f);
    if (len > 0)
        fwrite(data, 1, len, f);

    uint32_t c = (uint32_t)crc32(0L, Z_NULL, 0);
    c = (uint32_t)crc32(c, (const Bytef *)type, 4);
    if (len > 0)
        c = (uint32_t)crc32(c, data, len);

    uint8_t cb[4];
    be32(cb, c);
    fwrite(cb, 1, sizeof(cb), f);
}

static int write_png(const char *path)
{
    int w = preview_width();
    int h = preview_height();
    const uint32_t *fb = preview_pixels();

    size_t stride = 1 + (size_t)w * 3;
    size_t raw_len = (size_t)h * stride;
    uint8_t *raw = malloc(raw_len);
    if (raw == NULL)
        return 0;

    for (int y = 0; y < h; ++y) {
        uint8_t *row = raw + (size_t)y * stride;
        row[0] = 0; /* filter: none */
        for (int x = 0; x < w; ++x) {
            uint32_t c = fb[(size_t)y * (size_t)w + (size_t)x];
            row[1 + x * 3 + 0] = (uint8_t)(c & 0xFF);
            row[1 + x * 3 + 1] = (uint8_t)((c >> 8) & 0xFF);
            row[1 + x * 3 + 2] = (uint8_t)((c >> 16) & 0xFF);
        }
    }

    uLongf comp_len = compressBound((uLong)raw_len);
    uint8_t *comp = malloc(comp_len);
    if (comp == NULL) {
        free(raw);
        return 0;
    }

    if (compress2(comp, &comp_len, raw, (uLong)raw_len, 9) != Z_OK) {
        free(raw);
        free(comp);
        return 0;
    }

    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        free(raw);
        free(comp);
        return 0;
    }

    static const uint8_t SIG[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    fwrite(SIG, 1, sizeof(SIG), f);

    uint8_t ihdr[13];
    be32(ihdr, (uint32_t)w);
    be32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8;  /* bit depth */
    ihdr[9] = 2;  /* truecolour */
    ihdr[10] = 0; /* deflate */
    ihdr[11] = 0; /* adaptive filtering */
    ihdr[12] = 0; /* no interlace */

    png_chunk(f, "IHDR", ihdr, sizeof(ihdr));
    png_chunk(f, "IDAT", comp, (uint32_t)comp_len);
    png_chunk(f, "IEND", NULL, 0);

    fclose(f);
    free(raw);
    free(comp);
    return 1;
}

static int render(const char *dir, const char *name, float t,
                  const UiState *ui)
{
    char path[1024];

    preview_clear();
    screen_draw(t, ui);

    int calls = preview_draw_calls();

    printf("%-22s %5d draw calls\n", name, calls);

    if (calls > DRAW_CALL_BUDGET) {
        printf("FAIL %s: %d draw calls exceeds budget %d\n", name, calls,
               DRAW_CALL_BUDGET);
        return 0;
    }

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (!write_png(path))
        return 0;

    printf("wrote %s\n", path);
    return 1;
}

static void reset_scene(UiState *ui)
{
    fake_reset();
    metrics_init();
    seed_trace();
    editor_scan();
    plugins_scan();

    memset(ui, 0, sizeof(*ui));
    ui->tab = UI_TAB_HOME;
    ui->selection = g_editor.row_count > 0 ? 0 : -1;
}

static void make_long_config(void)
{
    char cfg[EDITOR_LINES_MAX * 24 + 64];
    size_t off = 0;

    off += (size_t)snprintf(cfg + off, sizeof(cfg) - off, "*KERNEL\n");
    for (int i = 0; i < EDITOR_LINES_MAX + 1; ++i)
        off += (size_t)snprintf(cfg + off, sizeof(cfg) - off,
                                "ur0:tai/p%03d.skprx\n", i);

    fake_add_file("ur0:tai/config.txt", cfg);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    UiState ui;

    fake_reset();
    metrics_init();
    editor_scan();
    plugins_scan();
    seed_trace();
    reset_scene(&ui);

    if (!render(dir, "preview-healthy.png", 2.5f, &ui))
        return 1;

    /* Low battery while discharging: the real alert path, not a forced flag. */
    fake_set_power(14, 0);
    metrics_sample_slow();
    if (!render(dir, "preview-alert.png", 4.0f, &ui))
        return 1;

    /* The editable ledger. */
    reset_scene(&ui);
    ui.tab = UI_TAB_PLUGINS;
    ui.selection = 0;
    if (!render(dir, "preview-plugins.png", 3.0f, &ui))
        return 1;

    /* The bottom of the ledger: the orphan section must be reachable. */
    reset_scene(&ui);
    ui.tab = UI_TAB_PLUGINS;
    ui.selection = g_editor.row_count > 0 ? g_editor.row_count - 1 : -1;
    ui.plugins_scroll = (float)plugins_max_scroll();
    if (!render(dir, "preview-plugins-bottom.png", 3.05f, &ui))
        return 1;

    /* A stray path is a warning: the valid rows stay editable. */
    reset_scene(&ui);
    fake_add_file("ur0:tai/config.txt",
                  "ur0:tai/stray.skprx\n"
                  "*KERNEL\nur0:tai/kubridge.skprx\n"
                  "*main\n#ur0:tai/shellbat.suprx\n");
    editor_scan();
    plugins_scan();
    ui.tab = UI_TAB_PLUGINS;
    ui.selection = 0;
    if (!render(dir, "preview-warning.png", 3.08f, &ui))
        return 1;

    /* Missing active and missing inactive rows: one can be disabled, one
     * cannot be enabled, and each shows the control state. */
    reset_scene(&ui);
    fake_add_file("ur0:tai/config.txt",
                  "*KERNEL\nur0:tai/absent.skprx\n"
                  "#ur0:tai/also-absent.skprx\n"
                  "*main\nur0:tai/kubridge.skprx\n"
                  "#ur0:tai/shellbat.suprx\n");
    editor_scan();
    plugins_scan();
    ui.tab = UI_TAB_PLUGINS;
    ui.selection = 0;
    if (!render(dir, "preview-missing.png", 3.1f, &ui))
        return 1;

    /* A failed snapshot is a warning, not a lock. */
    reset_scene(&ui);
    fake_fail_backup(1);
    editor_scan();
    plugins_scan();
    fake_fail_backup(0);
    ui.tab = UI_TAB_PLUGINS;
    ui.selection = 0;
    if (!render(dir, "preview-backup-warning.png", 3.2f, &ui))
        return 1;

    /* Five groups on HOME: the summary must not cross the tab block. */
    reset_scene(&ui);
    fake_add_file("ur0:tai/config.txt",
                  "# five groups\n"
                  "*KERNEL\nur0:tai/kubridge.skprx\n"
                  "*main\nur0:tai/henkaku.suprx\n"
                  "*NPXS10015\nur0:tai/yamt.suprx\n"
                  "*ALL\nur0:tai/vita3g-disable.suprx\n"
                  "*PCSE00001\nur0:tai/extra.skprx\n");
    editor_scan();
    plugins_scan();
    ui.tab = UI_TAB_HOME;
    if (!render(dir, "preview-groups.png", 3.3f, &ui))
        return 1;

    /* A path with the same basename in both mounts must stay two rows. */
    reset_scene(&ui);
    fake_add_file("ur0:tai/config.txt",
                  "*main\nur0:tai/kubridge.skprx\nux0:tai/kubridge.skprx\n");
    fake_add_file("ux0:tai/kubridge.skprx", "");
    editor_scan();
    plugins_scan();
    ui.tab = UI_TAB_PLUGINS;
    ui.selection = 0;
    if (!render(dir, "preview-duplicate.png", 3.5f, &ui))
        return 1;

    /* The warning confirmation, opened through the real policy. */
    reset_scene(&ui);
    ui.tab = UI_TAB_PLUGINS;
    int warned = editor_row_for_path("ur0:tai/storagemgr.skprx");
    ui.selection = warned;
    const char *message = editor_confirm_text(warned, 0);
    modal_open_confirm(&ui, warned, 0, message);
    snprintf(ui.modal_title, sizeof(ui.modal_title), "DISABLE %s",
             g_editor.rows[warned].name);
    if (!render(dir, "preview-confirm.png", 4.0f, &ui))
        return 1;

    /* A failed write must keep the row and show the error. */
    reset_scene(&ui);
    ui.tab = UI_TAB_PLUGINS;
    int row = editor_row_for_path("ur0:tai/kubridge.skprx");
    ui.selection = row;
    fake_fail_write(1);
    char err[64];
    (void)editor_toggle(row, 0, err, (int)sizeof(err));
    modal_open_error(&ui, err);
    if (!render(dir, "preview-write-failure.png", 5.0f, &ui))
        return 1;
    fake_fail_write(0);

    /* A genuinely unreadable-to-the-parser config is read-only. */
    reset_scene(&ui);
    make_long_config();
    editor_scan();
    plugins_scan();
    ui.tab = UI_TAB_PLUGINS;
    ui.selection = 0;
    if (!render(dir, "preview-readonly.png", 5.5f, &ui))
        return 1;

    /* HOME after a toggle: counts stay at the launch set, pending appears. */
    reset_scene(&ui);
    row = editor_row_for_path("ur0:tai/kubridge.skprx");
    (void)editor_toggle(row, 0, err, (int)sizeof(err));
    ui.tab = UI_TAB_HOME;
    if (!render(dir, "preview-home-pending.png", 6.0f, &ui))
        return 1;

    return 0;
}
