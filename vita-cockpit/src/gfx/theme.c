#include "gfx/theme.h"

#include <math.h>
#include <stdio.h>

#include "gfx/font.h"

/* Chrome geometry (D18). Two sizes only: a third would flatten the hierarchy. */
#define PANEL_CHAMFER_HERO 28.0f
#define PANEL_CHAMFER_SATELLITE 14.0f
#define BRACKET_LEN 16.0f
#define BRACKET_THICK 2.0f
#define TITLE_BAR_W 3.0f
#define TITLE_BAR_GAP 9.0f
#define OUTLINE_DIM 0.45f
#define RULE_DIM 0.25f

#define METER_CELLS 10
#define METER_GAP 2.0f

/* An absent reading draws its empty track at this fraction of C_HAIR. It has to
 * stay lighter than C_PANEL or the track vanishes into the fill: 0.45 rendered
 * at (12,13,16) against a (13,14,18) panel, which is invisible. */
#define METER_ABSENT_DIM 0.7f

/* Trace thickness in pixels. A filled area at 200 samples over ~500 px reads
 * as a solid block, not as a signal. */
#define TRACE_LINE_H 2.0f

#define HAZARD_STRIPE_W 9.0f
#define HAZARD_PERIOD 22.0f

#define SWEEP_PERIOD 5.0f
#define SWEEP_CORE_DIM 0.12f
#define SWEEP_HALO_DIM 0.05f

/* Room for one formatted integer reading. */
#define READOUT_BUF 24

static void cut_top_right(Rect r, float c)
{
    draw_tri(r.x + r.w - c, r.y, r.x + r.w, r.y + c, r.x + r.w, r.y, C_FIELD);
}

static void cut_bottom_left(Rect r, float c)
{
    draw_tri(r.x, r.y + r.h - c, r.x + c, r.y + r.h, r.x, r.y + r.h, C_FIELD);
}

void theme_panel(Rect r, const char *title, uint32_t accent,
                 PanelChrome chrome)
{
    int hero = (chrome == CHROME_HERO);
    int inline_title = (chrome == CHROME_STRIP || chrome == CHROME_LIST);
    float c = hero ? PANEL_CHAMFER_HERO : PANEL_CHAMFER_SATELLITE;
    uint32_t outline = draw_dim(accent, OUTLINE_DIM);

    draw_rect(r.x, r.y, r.w, r.h, C_PANEL);

    /*
     * The chamfer is painted rather than assembled as a polygon: the corner is
     * filled with C_PANEL and then cut back to the field. This is only valid
     * because panels never overlap, which is a property of the layout table.
     */
    cut_top_right(r, c);
    if (hero)
        cut_bottom_left(r, c);

    draw_line(r.x, r.y, r.x + r.w - c, r.y, outline);
    draw_line(r.x + r.w - c, r.y, r.x + r.w, r.y + c, outline);
    draw_line(r.x + r.w, r.y + c, r.x + r.w, r.y + r.h, outline);

    if (hero) {
        draw_line(r.x + r.w, r.y + r.h, r.x + c, r.y + r.h, outline);
        draw_line(r.x + c, r.y + r.h, r.x, r.y + r.h - c, outline);
        draw_line(r.x, r.y + r.h - c, r.x, r.y, outline);

        draw_rect(r.x, r.y, BRACKET_LEN, BRACKET_THICK, accent);
        draw_rect(r.x, r.y, BRACKET_THICK, BRACKET_LEN, accent);
        draw_rect(r.x + r.w - BRACKET_LEN, r.y + r.h - BRACKET_THICK,
                  BRACKET_LEN, BRACKET_THICK, accent);
        draw_rect(r.x + r.w - BRACKET_THICK, r.y + r.h - BRACKET_LEN,
                  BRACKET_THICK, BRACKET_LEN, accent);
    } else {
        draw_line(r.x + r.w, r.y + r.h, r.x, r.y + r.h, outline);
        draw_line(r.x, r.y + r.h, r.x, r.y, outline);
    }

    float title_h = LINE_H(PANEL_TITLE_PX);

    /*
     * A strip is one row tall, so its title reads best centred against the row.
     * A list has several rows; centring the title there would put it beside an
     * arbitrary middle row, so it sits at the top like a normal panel.
     */
    float title_y = (chrome == CHROME_STRIP)
                        ? r.y + (r.h - title_h) * 0.5f
                        : r.y + PANEL_TITLE_Y;

    draw_rect(r.x + PANEL_PAD, title_y, TITLE_BAR_W, title_h, accent);
    font_text_tracked(title, r.x + PANEL_PAD + TITLE_BAR_GAP, title_y,
                      PANEL_TITLE_PX, accent, PANEL_TITLE_TRACK);

    if (!inline_title)
        draw_rect(r.x + PANEL_PAD, r.y + PANEL_RULE_Y, r.w - 2.0f * PANEL_PAD,
                  1.0f, draw_dim(accent, RULE_DIM));
}

void theme_readout_px(const char *label, const char *value, int valid, float x,
                      float y, float w, float label_px, float value_px)
{
    /* The two scales are baseline-aligned, never top-aligned (DESIGN.md §3). */
    font_text(label, x, y + LINE_H(value_px) - LINE_H(label_px), label_px,
              C_DIM);
    font_text_right(value, x + w, y, value_px, valid ? C_WHITE : C_DIM);
}

void theme_readout(const char *label, const char *value, int valid, float x,
                   float y, float w)
{
    theme_readout_px(label, value, valid, x, y, w, PX_LABEL, PX_VALUE);
}

void theme_readout_int(const char *label, int value, int valid, float x,
                       float y, float w)
{
    char buf[READOUT_BUF];
    int ok = theme_fmt_int(buf, sizeof(buf), value, valid);

    theme_readout(label, buf, ok, x, y, w);
}

void theme_meter(float x, float y, float w, float h, float fill, int valid,
                 uint32_t accent)
{
    if (fill < 0.0f)
        fill = 0.0f;
    if (fill > 1.0f)
        fill = 1.0f;

    /* Ceiling, not rounding: a card that is 1 % used must still light a cell,
     * otherwise "almost empty" and "completely empty" look identical. */
    int lit = valid ? (int)ceilf(fill * (float)METER_CELLS) : 0;
    if (lit > METER_CELLS)
        lit = METER_CELLS;

    uint32_t empty = valid ? C_HAIR : draw_dim(C_HAIR, METER_ABSENT_DIM);
    float cw = (w - (float)(METER_CELLS - 1) * METER_GAP) / (float)METER_CELLS;

    for (int i = 0; i < METER_CELLS; ++i)
        draw_rect(x + (float)i * (cw + METER_GAP), y, cw, h,
                  i < lit ? accent : empty);
}

static float trace_y(float v, float y, float height, float lo, float hi)
{
    float span = hi - lo;
    if (span <= 0.0f)
        span = 1.0f;

    float t = (v - lo) / span;
    if (t < 0.0f)
        t = 0.0f;
    if (t > 1.0f)
        t = 1.0f;

    return y + (height - TRACE_LINE_H) * (1.0f - t);
}

static Rect s_trace_cols[HISTORY_CAP];

void theme_sparkline(const History *hist, float x, float y, float w,
                     float height, uint32_t c, float scale_min,
                     float scale_max)
{
    if (hist == NULL || hist->len <= 0 || w <= 0.0f || height <= 0.0f)
        return;
    if (scale_max <= scale_min)
        return;

    /* Fixed column width, so the trace scrolls at a steady rate instead of
     * rescaling as samples arrive. */
    float step = w / (float)HISTORY_CAP;
    float prev_y = 0.0f;
    int have_prev = 0;
    int n = 0;

    for (int i = 0; i < hist->len && n < HISTORY_CAP; ++i) {
        float py = trace_y(history_at(hist, i), y, height, scale_min,
                           scale_max);

        /* history_at(0) is newest, and newest enters at the right edge. */
        float bx = x + w - (float)(i + 1) * step;

        /*
         * Join to the previous sample with a vertical segment. Without it a
         * one-sample spike renders as a dash floating above the line instead of
         * as a spike.
         */
        float top = py;
        float hgt = TRACE_LINE_H;

        if (have_prev) {
            float lo = py < prev_y ? py : prev_y;
            float hi = py < prev_y ? prev_y : py;
            top = lo;
            hgt = hi - lo + TRACE_LINE_H;
        }

        s_trace_cols[n].x = bx;
        s_trace_cols[n].y = top;
        s_trace_cols[n].w = step + 0.5f;
        s_trace_cols[n].h = hgt;
        ++n;

        prev_y = py;
        have_prev = 1;
    }

    draw_rects(s_trace_cols, n, c);
}

void theme_hazard(float x, float y, float w, float h)
{
    draw_rect(x, y, w, h, C_FIELD);

    /*
     * 45-degree parallelograms. No clipping is needed because the band always
     * spans the full screen width, so the overhang at either end falls outside
     * the framebuffer.
     */
    for (float px = x - h; px < x + w + h; px += HAZARD_PERIOD) {
        draw_tri(px, y + h, px + h, y, px + h + HAZARD_STRIPE_W, y, C_YELLOW);
        draw_tri(px, y + h, px + h + HAZARD_STRIPE_W, y, px + HAZARD_STRIPE_W,
                 y + h, C_YELLOW);
    }
}

void theme_sweep(float x, float y, float w, float h, float t)
{
    float p = fmodf(t, SWEEP_PERIOD) / SWEEP_PERIOD;
    if (p < 0.0f)
        p += 1.0f;

    float sx = x + p * w;

    /* Additive, or the scanline would overwrite the text it crosses. */
    draw_set_additive(1);
    draw_rect(sx - 1.0f, y, 3.0f, h, draw_dim(C_CYAN, SWEEP_HALO_DIM));
    draw_rect(sx, y, 1.0f, h, draw_dim(C_CYAN, SWEEP_CORE_DIM));
    draw_set_additive(0);
}

void theme_fmt_tenths(char *buf, size_t n, int tenths)
{
    if (buf == NULL || n == 0)
        return;

    int v = tenths < 0 ? -tenths : tenths;
    snprintf(buf, n, "%s%d.%d", tenths < 0 ? "-" : "", v / 10, v % 10);
}

int theme_fmt_int(char *buf, size_t n, int value, int valid)
{
    if (buf == NULL || n == 0)
        return 0;

    if (!valid) {
        snprintf(buf, n, "%s", THEME_ABSENT);
        return 0;
    }

    snprintf(buf, n, "%d", value);
    return 1;
}

void theme_fmt_hms(char *buf, size_t n, int total_seconds)
{
    if (buf == NULL || n == 0)
        return;

    if (total_seconds < 0)
        total_seconds = 0;

    snprintf(buf, n, "%02d:%02d:%02d", total_seconds / 3600,
             (total_seconds / 60) % 60, total_seconds % 60);
}
