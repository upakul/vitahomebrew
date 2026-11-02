#include "ui/header.h"

#include <stdint.h>
#include <stdio.h>

#include "core/metrics.h"
#include "gfx/font.h"
#include "gfx/layout.h"
#include "gfx/theme.h"
#include "ui/tabbar.h"

/*
 * The header is chrome plus a clock. It carries no metric that a panel already
 * shows: the device model lives in SYSTEM, so identity appears once.
 */

#define HEADER_MARGIN ((float)MARGIN)
#define WORDMARK_X 18.0f
#define WORDMARK_Y 16.0f
#define WORDMARK_TEXT "COCKPIT"

#define CLOCK_Y 23.0f

/*
 * The band sits below the tabs, on the last line the header owns. HOME no
 * longer carries a summary here (D67), so a fault never competes with a count.
 */
#define HAZARD_Y 48.0f
#define HAZARD_H 8.0f

#define GLOW_OFFSET 2.0f
#define GLOW_DIM 0.10f
#define CHROMA_LEFT_DIM 0.45f
#define CHROMA_RIGHT_DIM 0.35f

/*
 * The wordmark is a constant string at a constant position, so its glyph rects
 * are enumerated once and replayed. Walking the glyph table seven times per
 * frame cost 462 of the frame's draw calls for an image that never changes.
 */
#define WORDMARK_RECTS 128
#define WORDMARK_HALO_PASSES 4

static Rect s_wordmark[WORDMARK_RECTS];
static int s_wordmark_count;
static Rect s_scratch[WORDMARK_RECTS * WORDMARK_HALO_PASSES];

static const float HALO_OFFSET[WORDMARK_HALO_PASSES][2] = {
    {-GLOW_OFFSET, 0.0f},
    {GLOW_OFFSET, 0.0f},
    {0.0f, -GLOW_OFFSET},
    {0.0f, GLOW_OFFSET},
};

static int shifted(const Rect *src, int n, float dx, float dy, Rect *out)
{
    for (int i = 0; i < n; ++i) {
        out[i] = src[i];
        out[i].x += dx;
        out[i].y += dy;
    }
    return n;
}

static void draw_wordmark(void)
{
    /*
     * The one loud element on screen (DESIGN.md §7). Glow is permitted here and
     * on the hazard band, nowhere else (D3).
     */
    if (s_wordmark_count <= 0)
        s_wordmark_count = font_rects(WORDMARK_TEXT, WORDMARK_X, WORDMARK_Y,
                                      PX_HERO, s_wordmark, WORDMARK_RECTS);
    if (s_wordmark_count <= 0)
        return;

    int n = s_wordmark_count;

    /* Halo: four offset copies. Additive, so it brightens the field instead of
     * painting a dim opaque copy beside it. */
    int total = 0;
    for (int k = 0; k < WORDMARK_HALO_PASSES; ++k)
        total += shifted(s_wordmark, n, HALO_OFFSET[k][0], HALO_OFFSET[k][1],
                         &s_scratch[total]);

    draw_set_additive(1);
    draw_rects(s_scratch, total, draw_dim(C_YELLOW, GLOW_DIM));
    draw_set_additive(0);

    float d = PX_HERO * 0.5f;
    if (d < 1.0f)
        d = 1.0f;

    draw_rects(s_scratch, shifted(s_wordmark, n, -d, 0.0f, s_scratch),
               draw_dim(C_YELLOW, CHROMA_LEFT_DIM));
    draw_rects(s_scratch, shifted(s_wordmark, n, d, 0.0f, s_scratch),
               draw_dim(C_CYAN, CHROMA_RIGHT_DIM));
    draw_rects(s_wordmark, n, C_YELLOW);
}

static void draw_clock(void)
{
    char buf[16];

    if (g_metrics.clock_valid)
        theme_fmt_hms(buf, sizeof(buf),
                      g_metrics.clock_h * 3600 + g_metrics.clock_m * 60 +
                          g_metrics.clock_s);
    else
        snprintf(buf, sizeof(buf), "--:--:--");

    font_text_right(buf, (float)SCREEN_W - HEADER_MARGIN, CLOCK_Y, PX_VALUE,
                    g_metrics.clock_valid ? C_WHITE : C_DIM);
}

void header_draw(const UiState *ui)
{
    draw_wordmark();
    draw_clock();
    tabbar_draw(ui);

    /* D15: only ever drawn for a real fault, and it never reflows the layout.
     * D67: HOME's plugin count lives in a panel, so nothing competes with it. */
    if (g_metrics.alert)
        theme_hazard(0.0f, HAZARD_Y, (float)SCREEN_W, HAZARD_H);
}
