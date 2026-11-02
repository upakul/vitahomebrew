#include "panels/panel.h"

#include <stdio.h>

#include "core/metrics.h"
#include "gfx/font.h"

/*
 * POWER — the one panel where a single number is the point.
 *
 * Percent escalates to primary scale; everything else stays at value scale so
 * the panel has exactly one focal element (DESIGN.md §5).
 */

#define POWER_BIG_Y PANEL_BODY_Y
#define POWER_STATE_Y (POWER_BIG_Y + LINE_H(PX_PRIMARY) + SP_3)
#define POWER_ROW1_Y (POWER_STATE_Y + LINE_H(PX_MICRO) + SP_4)
#define POWER_ROW2_Y (POWER_ROW1_Y + READOUT_STRIDE)

#define POWER_COL_W 130.0f
#define POWER_COL_GAP 32.0f

#define VALUE_BUF 24

static float col_x(Rect r, int i)
{
    return r.x + PANEL_INSET + (float)i * (POWER_COL_W + POWER_COL_GAP);
}

static void draw_primary(Rect r)
{
    char pct[VALUE_BUF];

    if (g_metrics.power_valid)
        snprintf(pct, sizeof(pct), "%d%%", g_metrics.batt_pct);
    else
        snprintf(pct, sizeof(pct), "%s", THEME_ABSENT);

    float w = font_width(pct, PX_PRIMARY);
    font_text(pct, r.x + (r.w - w) * 0.5f, r.y + POWER_BIG_Y, PX_PRIMARY,
              g_metrics.power_valid ? C_WHITE : C_DIM);

    if (!g_metrics.power_valid)
        return;

    const char *state = g_metrics.charging ? "CHARGING" : "ON BATTERY";
    float sw = font_width(state, PX_MICRO);
    font_text(state, r.x + (r.w - sw) * 0.5f, r.y + POWER_STATE_Y, PX_MICRO,
              C_DIM);
}

static void draw_detail(Rect r)
{
    theme_readout_int("TEMP", g_metrics.batt_temp_c, g_metrics.power_valid,
                      col_x(r, 0), r.y + POWER_ROW1_Y, POWER_COL_W);

    theme_readout_int("VOLT", g_metrics.batt_mv, g_metrics.power_valid,
                      col_x(r, 1), r.y + POWER_ROW1_Y, POWER_COL_W);

    theme_readout_int("CYC", g_metrics.batt_cycles, g_metrics.power_valid,
                      col_x(r, 0), r.y + POWER_ROW2_Y, POWER_COL_W);

    theme_readout_int("SOH", g_metrics.batt_health, g_metrics.power_valid,
                      col_x(r, 1), r.y + POWER_ROW2_Y, POWER_COL_W);
}

static void draw_power(const Panel *self, Rect r)
{
    (void)self;

    draw_primary(r);
    draw_detail(r);
}

const Panel panel_power = {"POWER", C_YELLOW, draw_power};
