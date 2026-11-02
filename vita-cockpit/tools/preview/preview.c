#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "app/screen.h"
#include "core/history.h"
#include "core/metrics.h"
#include "core/platform.h"
#include "core/plugins.h"
#include "gfx/layout.h"
#include "panels/registry.h"
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
#define DRAW_CALL_BUDGET 5500

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

static int render(const char *dir, const char *name, float t, float scroll)
{
    char path[1024];

    preview_clear();
    screen_draw(t, scroll);

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

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";

    fake_reset();
    metrics_init();
    plugins_scan();
    layout_update();
    seed_trace();
    if (!render(dir, "preview-healthy.png", 2.5f, 0.0f))
        return 1;

    /* Low battery while discharging: the real alert path, not a forced flag. */
    fake_set_power(14, 0);
    metrics_sample_slow();
    if (!render(dir, "preview-alert.png", 4.0f, 0.0f))
        return 1;

    /* Scrolled to the bottom: proves the canvas offset and the clipping. */
    if (!render(dir, "preview-scrolled.png", 3.0f,
                (float)layout_max_scroll()))
        return 1;

    /* Nothing readable: every panel must show "--" and no trace. */
    fake_set_memory_absent();
    fake_set_power_absent();
    fake_set_identity_absent();
    fake_set_clock_absent();
    for (int i = 0; i < PLATFORM_STORAGE_COUNT; ++i)
        fake_set_storage_absent(i);
    metrics_sample_slow();
    if (!render(dir, "preview-absent.png", 5.5f, 0.0f))
        return 1;

    return 0;
}
