#include "panels/plugins.h"

#include "panels/panel.h"

#include "core/plugins.h"
#include "gfx/font.h"
#include "gfx/layout.h"

/*
 * ACTIVATED PLUGINS — what taiHEN is running, grouped by load target:
 *
 *   KERNEL
 *   NAME  ------------------  (dot)
 *   description
 *
 * The leader line and the dot make each row read as a status entry rather than
 * a paragraph. The dot is green because every row here is loaded; a plugin
 * whose file is missing never reaches this list (D47).
 *
 * The panel is self-sizing: one header per non-empty group and one row per
 * plugin, so a console with twenty plugins grows the region and two shrinks it.
 * A plugin outside the description catalog says N/A instead of leaving a blank
 * second line (D51, D52).
 */

#define DOT_SIZE 6.0f
#define DOT_DY 4.0f
#define LEADER_DY 6.0f
#define LEADER_GAP 8.0f
#define LEADER_END_GAP 6.0f
#define LEADER_DIM 0.6f

/* A group header, then the air before its first row. */
#define GROUP_HEADER_H 26.0f

/* Air between the last description of one group and the next header. */
#define GROUP_GAP 10.0f

#define PLUGIN_DESC_NONE "N/A"

/*
 * Friendly names for the load targets in config.txt (D51). The order matches
 * PluginSection, which is also the merge priority: a file listed in several
 * sections is shown under the most privileged one.
 */
static const char *const GROUP_LABELS[PLUGIN_SECTION_COUNT] = {
    "KERNEL",
    "SYSTEM",
    "SYSTEM APPS",
    "ALL TITLES",
    "TITLES",
};

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

    /* Every row is two lines. An unknown plugin keeps its row and says so,
     * because an empty line reads as a drawing failure rather than a gap in the
     * catalog. */
    font_text(p->desc[0] != '\0' ? p->desc : PLUGIN_DESC_NONE, name_x,
              y + PLUGIN_DESC_DY, PX_MICRO, C_DIM);
}

static int rows_in_section(PluginSection section)
{
    int count = 0;

    for (int i = 0; i < g_plugins.count; ++i) {
        if (g_plugins.items[i].section == section)
            ++count;
    }

    return count;
}

static int group_count(void)
{
    int groups = 0;

    for (int s = 0; s < PLUGIN_SECTION_COUNT; ++s) {
        if (rows_in_section((PluginSection)s) > 0)
            ++groups;
    }

    return groups;
}

float plugins_panel_height(void)
{
    if (g_plugins.count == 0)
        return PLUGIN_TOP + PLUGIN_STRIDE; /* room for the message row */

    int groups = group_count();

    /*
     * One header per group, one stride per row. The last row's stride already
     * carries the bottom margin, so only the gaps between groups are added.
     */
    float h = PLUGIN_TOP + (float)groups * GROUP_HEADER_H +
              (float)g_plugins.count * PLUGIN_STRIDE;

    if (groups > 1)
        h += (float)(groups - 1) * GROUP_GAP;

    /* The truncation notice sits below the last row, with air above and below. */
    if (g_plugins.truncated)
        h += GROUP_GAP + PLUGIN_LINE_H + SP_2;

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

    float y = r.y + PLUGIN_TOP;
    int groups_drawn = 0;

    for (int s = 0; s < PLUGIN_SECTION_COUNT; ++s) {
        PluginSection section = (PluginSection)s;

        if (rows_in_section(section) == 0)
            continue;

        if (groups_drawn > 0)
            y += GROUP_GAP;

        /* Off the band: the scissor would discard it, so skip the glyph walk. */
        if (y + LINE_H(PX_MICRO) > (float)VIEW_Y &&
            y < (float)(VIEW_Y + VIEW_H))
            font_text_tracked(GROUP_LABELS[section], r.x + PANEL_PAD, y,
                              PX_MICRO, C_CYAN, 1.0f);

        y += GROUP_HEADER_H;
        ++groups_drawn;

        for (int i = 0; i < g_plugins.count; ++i) {
            if (g_plugins.items[i].section != section)
                continue;

            if (y + PLUGIN_DESC_DY + PLUGIN_LINE_H > (float)VIEW_Y &&
                y < (float)(VIEW_Y + VIEW_H))
                draw_row(r, &g_plugins.items[i], y);

            y += PLUGIN_STRIDE;
        }
    }

    /*
     * The array ceiling is high enough that a bootable config never reaches it,
     * but if it does, the missing rows are stated rather than silently cut.
     */
    if (g_plugins.truncated) {
        y += GROUP_GAP;

        if (y + PLUGIN_LINE_H > (float)VIEW_Y &&
            y < (float)(VIEW_Y + VIEW_H))
            font_text("MORE PLUGINS NOT SHOWN", r.x + PANEL_PAD, y, PX_MICRO,
                      C_DIM);
    }
}

const Panel panel_plugins = {"ACTIVATED PLUGINS", C_CYAN, draw_plugins};
