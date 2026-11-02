#include "panels/panel.h"

#include "core/metrics.h"

/*
 * SYSTEM — identity. Firmware and uptime, with the device model.
 *
 * The wall clock and the wordmark are header chrome; they are deliberately not
 * repeated here.
 */

#define SYSTEM_FW_Y PANEL_BODY_Y
#define SYSTEM_UP_Y (SYSTEM_FW_Y + READOUT_STRIDE)
#define SYSTEM_MODEL_Y (SYSTEM_UP_Y + READOUT_STRIDE)

#define VALUE_BUF 24

static const char *model_name(PlatformModel m)
{
    switch (m) {
    case PLATFORM_MODEL_VITA:
        return "PSVITA";
    case PLATFORM_MODEL_VITA_TV:
        return "PSTV";
    case PLATFORM_MODEL_UNKNOWN:
    default:
        return THEME_ABSENT;
    }
}

static void draw_firmware(Rect r)
{
    float y = r.y + SYSTEM_FW_Y;
    const char *fw = g_metrics.fw_valid ? g_metrics.fw : THEME_ABSENT;

    /*
     * Value scale, not primary. The firmware string is unbounded and the
     * content box is 292 px; at px=5 a ten-character version is 295 px wide and
     * overprints the FW caption. At px=3 it is 177 px, which leaves the caption
     * clear for any string the SDK can return (D27).
     */
    theme_readout("FW", fw, g_metrics.fw_valid, r.x + PANEL_INSET, y,
                  r.w - 2.0f * PANEL_INSET);
}

static void draw_rows(Rect r)
{
    float w = r.w - 2.0f * PANEL_INSET;
    char buf[VALUE_BUF];

    theme_fmt_hms(buf, sizeof(buf), (int)g_metrics.uptime_s);
    theme_readout("RUN", buf, 1, r.x + PANEL_INSET, r.y + SYSTEM_UP_Y, w);

    int known = g_metrics.model != PLATFORM_MODEL_UNKNOWN;
    theme_readout("MODEL", model_name(g_metrics.model), known,
                  r.x + PANEL_INSET, r.y + SYSTEM_MODEL_Y, w);
}

static void draw_system(const Panel *self, Rect r)
{
    (void)self;

    draw_firmware(r);
    draw_rows(r);
}

const Panel panel_system = {"SYSTEM", C_CYAN, draw_system};
