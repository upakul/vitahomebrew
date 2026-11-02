#include "panels/plugins.h"

#include "panels/panel.h"

#include "core/plugins.h"
#include "gfx/font.h"
#include "gfx/layout.h"

/*
 * ACTIVATED PLUGINS — what taiHEN is running, one row per plugin:
 *
 *   NAME  ------------------  (dot)
 *   description
 *
 * The leader line and the dot make the row read as a status entry rather than a
 * paragraph. The dot is green because every row here is loaded; a plugin whose
 * file is missing never reaches this list (D47).
 *
 * The panel is self-sizing: its height comes from the row count, so a console
 * with eight plugins grows the region and one with two shrinks it.
 */

#define DOT_SIZE 6.0f
#define DOT_DY 4.0f
#define LEADER_DY 6.0f
#define LEADER_GAP 8.0f
#define LEADER_END_GAP 6.0f
#define LEADER_DIM 0.6f

static void draw_message(Rect r, const char *msg)
{
    font_text(msg, r.x + PANEL_PAD, r.y + PLUGIN_TOP, PX_LABEL, C_DIM);
}

static void draw_row(Rect r, const PluginInfo *p, float y)
{
    float name_x = r.x + PANEL_PAD;

    font_text(p->name, name_x, y, PX_LABEL, C_WHITE);

    float dot_x = r.x + r.w - PANEL_PAD - DOT_SIZE;
    float leader_x0 = name_x + font_width(p->name, PX_LABEL) + LEADER_GAP;
    float leader_x1 = dot_x - LEADER_END_GAP;

    /* C_HAIR is too close to the panel fill to read as a line; a dimmed label
     * grey is the faintest tone that still registers. */
    if (leader_x1 > leader_x0)
        draw_rect(leader_x0, y + LEADER_DY, leader_x1 - leader_x0, 1.0f,
                  draw_dim(C_DIM, LEADER_DIM));

    draw_rect(dot_x, y + DOT_DY, DOT_SIZE, DOT_SIZE, C_GREEN);

    if (p->desc[0] != '\0')
        font_text(p->desc, name_x, y + PLUGIN_DESC_DY, PX_MICRO, C_DIM);
}

float plugins_panel_height(void)
{
    int rows = g_plugins.count > 0 ? g_plugins.count : 1;

    /*
     * The last row contributes its description and the bottom margin; the
     * stride already contains the gap that follows it. Counting a full stride
     * per row over-allocated one line of dead space in the first version.
     */
    float h = PLUGIN_TOP + (float)(rows - 1) * PLUGIN_STRIDE + PLUGIN_DESC_DY +
              PLUGIN_LINE_H + PLUGIN_BOTTOM;

    /* The truncation notice occupies the slot a further row would have used. */
    if (g_plugins.truncated)
        h += PLUGIN_STRIDE;

    return h;
}

static void draw_plugins(const Panel *self, Rect r)
{
    (void)self;

    if (!g_plugins.config_found) {
        draw_message(r, "NO TAI CONFIG FOUND");
        return;
    }

    if (g_plugins.count == 0) {
        draw_message(r, "NO ACTIVE PLUGINS");
        return;
    }

    for (int i = 0; i < g_plugins.count; ++i) {
        float y = r.y + PLUGIN_TOP + (float)i * PLUGIN_STRIDE;

        /* Off the band: the scissor would discard it, so skip the glyph walk. */
        if (y + PLUGIN_DESC_DY + PLUGIN_LINE_H <= (float)VIEW_Y ||
            y >= (float)(VIEW_Y + VIEW_H))
            continue;

        draw_row(r, &g_plugins.items[i], y);
    }

    /*
     * The array ceiling is high enough that a bootable config never reaches it,
     * but if it does, the missing rows are stated rather than silently cut.
     */
    if (g_plugins.truncated) {
        float y = r.y + PLUGIN_TOP + (float)g_plugins.count * PLUGIN_STRIDE;

        if (y + PLUGIN_LINE_H > (float)VIEW_Y &&
            y < (float)(VIEW_Y + VIEW_H))
            font_text("MORE PLUGINS NOT SHOWN", r.x + PANEL_PAD, y, PX_MICRO,
                      C_DIM);
    }
}

const Panel panel_plugins = {"ACTIVATED PLUGINS", C_CYAN, draw_plugins};
