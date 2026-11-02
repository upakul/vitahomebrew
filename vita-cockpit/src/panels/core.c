#include "panels/panel.h"

#include <stdio.h>

#include "core/metrics.h"
#include "gfx/font.h"

/*
 * CORE — the hero. Clocks and memory are read as one thing: how fast the
 * machine is running and how much headroom it has left. The trace is the
 * escalated element (DESIGN.md §5), so the numbers stay at value scale.
 */

#define CORE_COL_W 120.0f
#define CORE_COL_GAP 62.0f

#define CORE_ROW1_Y PANEL_BODY_Y
#define CORE_ROW2_Y (CORE_ROW1_Y + READOUT_STRIDE)
#define CORE_CAPTION_Y (CORE_ROW2_Y + LINE_H(PX_VALUE) + SP_4)
#define CORE_TRACE_Y (CORE_CAPTION_Y + LINE_H(PX_MICRO) + SP_2)
#define CORE_TRACE_H 40.0f

#define NUM_BUF 24

static float col_x(Rect r, int i)
{
    return r.x + PANEL_INSET + (float)i * (CORE_COL_W + CORE_COL_GAP);
}

static void draw_triplet(Rect r, float row_y, const char *const labels[3],
                         const int values[3], int valid)
{
    for (int i = 0; i < 3; ++i)
        theme_readout_int(labels[i], values[i], valid, col_x(r, i),
                          r.y + row_y, CORE_COL_W);
}

static void draw_trace(Rect r)
{
    float x = r.x + PANEL_INSET;
    float w = r.w - 2.0f * PANEL_INSET;

    /*
     * The trace is scaled to the window's own range, not to zero. A free-memory
     * trace sits at ~95 % of capacity all day, so a zero-based axis would pin
     * the line to the top edge and show nothing. The axis is printed at the
     * right, so the autoscale is declared rather than implied.
     */
    font_text("FREE USER MEM (MB)", x, r.y + CORE_CAPTION_Y, PX_MICRO, C_DIM);

    /* No reading, or no samples, means no trace. Drawing an axis anyway would
     * put a fabricated range under a label that says memory. */
    if (!g_metrics.mem_valid || g_metrics.hist_mem.len <= 0)
        return;

    /* Axis cached by the sampler at 10 Hz, so the 60 Hz loop does not rescan
     * 200 samples twice per frame (D21, D27). */
    float lo = g_metrics.hist_mem_lo;
    float hi = g_metrics.hist_mem_hi;

    char range_buf[NUM_BUF];
    snprintf(range_buf, sizeof(range_buf), "%d-%d", (int)lo, (int)hi);
    font_text_right(range_buf, x + w, r.y + CORE_CAPTION_Y, PX_MICRO, C_DIM);

    theme_sparkline(&g_metrics.hist_mem, x, r.y + CORE_TRACE_Y, w,
                    CORE_TRACE_H, C_YELLOW, lo, hi);
}

static void draw_core(const Panel *self, Rect r)
{
    static const char *const CLOCK_LABELS[3] = {"ARM", "GPU", "BUS"};
    static const char *const MEM_LABELS[3] = {"USER", "CDRAM", "PHY"};
    int mhz[3] = {g_metrics.arm_mhz, g_metrics.gpu_mhz, g_metrics.bus_mhz};
    int mb[3] = {g_metrics.mem_user_mb, g_metrics.mem_cdram_mb,
                 g_metrics.mem_phy_mb};

    (void)self;

    draw_triplet(r, CORE_ROW1_Y, CLOCK_LABELS, mhz, g_metrics.clocks_valid);
    draw_triplet(r, CORE_ROW2_Y, MEM_LABELS, mb, g_metrics.mem_valid);
    draw_trace(r);
}

const Panel panel_core = {"CORE", C_YELLOW, draw_core};
