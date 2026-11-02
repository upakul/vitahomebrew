#include "panels/panel.h"

#include "core/metrics.h"
#include "gfx/font.h"

/*
 * FRAME — a status strip, not a panel. It measures COCKPIT rather than the
 * machine, so it gets 44 px across the full width and no more (D17).
 *
 * The trace scale is fixed at two frame periods at 60 Hz: a sample reaching
 * the top of the box is a dropped frame, which is the only thing this trace is
 * for. Autoscaling here would turn normal jitter into drama.
 */

#define FRAME_MS_X 250.0f
#define FRAME_TRACE_X 370.0f
#define FRAME_TRACE_Y 8.0f
#define FRAME_TRACE_H 28.0f
#define FRAME_TRACE_RIGHT_PAD 34.0f

#define FRAME_SCALE_MS 33.3f
#define FRAME_VALUE_Y 11.5f
/* Baseline-aligned with the value: the value is 21 px, the unit 14 px, so the
 * unit starts 7 px lower. */
#define FRAME_UNIT_Y (FRAME_VALUE_Y + LINE_H(PX_VALUE) - LINE_H(PX_LABEL))
#define FRAME_UNIT_GAP 8.0f

#define VALUE_BUF 24

static void draw_metric(Rect r, float x, const char *label, int tenths)
{
    char buf[VALUE_BUF];

    theme_fmt_tenths(buf, sizeof(buf), tenths);
    font_text(buf, r.x + x, r.y + FRAME_VALUE_Y, PX_VALUE, C_WHITE);

    font_text(label, r.x + x + font_width(buf, PX_VALUE) + FRAME_UNIT_GAP,
              r.y + FRAME_UNIT_Y, PX_LABEL, C_DIM);
}

static void draw_frame(const Panel *self, Rect r)
{
    draw_metric(r, STRIP_BODY_X, "FPS", (int)(g_metrics.fps * 10.0f + 0.5f));
    draw_metric(r, FRAME_MS_X, "MS", (int)(g_metrics.frame_ms * 10.0f + 0.5f));

    theme_sparkline(&g_metrics.hist_frame, r.x + FRAME_TRACE_X,
                    r.y + FRAME_TRACE_Y,
                    r.w - FRAME_TRACE_X - FRAME_TRACE_RIGHT_PAD, FRAME_TRACE_H,
                    self->accent, 0.0f, FRAME_SCALE_MS);
}

const Panel panel_frame = {"FRAME", C_CYAN_DIM, draw_frame};
