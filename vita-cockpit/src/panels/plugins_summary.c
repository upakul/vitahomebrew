#include "panels/panel.h"

#include <stdio.h>

#include "core/plugin_editor.h"
#include "core/plugins.h"
#include "gfx/font.h"

/*
 * PLUGINS — the activated-plugin summary, on HOME.
 *
 * This block replaces the header status line (D63, superseded by D67). It
 * reports the launch set: the plugins taiHEN actually loaded, grouped by the
 * load target that named them.
 *
 * A pending edit has not changed what is running yet, so it is a separate
 * yellow token beside the counts (D59). The block does not list every plugin;
 * that is the PLUGINS tab's job. Here it answers one question — how many
 * plugins are online, and where do they sit — in one glance.
 */

#define SUMMARY_NUMBER_Y PANEL_BODY_Y
#define SUMMARY_LABEL_DY (LINE_H(PX_PRIMARY) - LINE_H(PX_LABEL))
#define SUMMARY_LABEL_GAP SP_4
#define SUMMARY_GROUPS_Y (SUMMARY_NUMBER_Y + LINE_H(PX_PRIMARY) + SP_2)
#define SUMMARY_BADGE_GAP SP_4
#define SUMMARY_TEXT_MAX 192
#define SUMMARY_BADGE_MAX 40

static const PluginSection GROUP_ORDER[PLUGIN_SECTION_COUNT] = {
    PLUGIN_SECTION_KERNEL, PLUGIN_SECTION_SYSTEM, PLUGIN_SECTION_SYSTEM_APP,
    PLUGIN_SECTION_ALL_TITLES, PLUGIN_SECTION_TITLE};

static const char *const GROUP_LABEL[PLUGIN_SECTION_COUNT] = {
    "KERNEL", "SYSTEM", "SYSTEM APPS", "ALL TITLES", "TITLES"};

static int group_count(PluginSection section)
{
    int n = 0;

    for (int i = 0; i < g_plugins.count; ++i) {
        if (g_plugins.items[i].section == section)
            ++n;
    }

    return n;
}

/* One line of label/count pairs, separated by a full step of air. */
static void draw_groups(Rect r)
{
    char line[SUMMARY_TEXT_MAX];
    size_t used = 0;

    line[0] = '\0';

    for (int i = 0; i < PLUGIN_SECTION_COUNT; ++i) {
        int n = group_count(GROUP_ORDER[i]);

        if (n == 0)
            continue;

        if (used >= sizeof(line))
            break;

        int wrote = snprintf(line + used, sizeof(line) - used, "%s%s %d",
                             used > 0 ? "   " : "",
                             GROUP_LABEL[GROUP_ORDER[i]], n);

        if (wrote < 0)
            break;

        used += (size_t)wrote;
    }

    /* An empty config is a real state, not a drawing failure. */
    if (line[0] == '\0')
        snprintf(line, sizeof(line), "NO PLUGINS LISTED");

    font_text(line, r.x + PANEL_INSET, r.y + SUMMARY_GROUPS_Y, PX_MICRO, C_DIM);
}

/*
 * Pending and config warnings share the right end of the group line. Pending
 * comes first so it reads as the nearer state: a file change that has not taken
 * effect yet.
 */
static void draw_badges(Rect r, int warning)
{
    float right = r.x + r.w - PANEL_INSET;
    float y = r.y + SUMMARY_GROUPS_Y;
    char buf[SUMMARY_BADGE_MAX];

    if (g_editor.pending_count > 0) {
        snprintf(buf, sizeof(buf), "%d PENDING", g_editor.pending_count);
        font_text_right(buf, right, y, PX_MICRO, C_YELLOW);
        right -= font_width(buf, PX_MICRO) + SUMMARY_BADGE_GAP;
    }

    if (warning)
        font_text_right("CONFIG WARNING", right, y, PX_MICRO, C_YELLOW);
}

static void draw_hero(Rect r, const char *number, const char *headline,
                      uint32_t number_colour, uint32_t headline_colour)
{
    float x = r.x + PANEL_INSET;

    font_text(number, x, r.y + SUMMARY_NUMBER_Y, PX_PRIMARY, number_colour);

    float label_x = x + font_width(number, PX_PRIMARY) + SUMMARY_LABEL_GAP;
    font_text(headline, label_x, r.y + SUMMARY_NUMBER_Y + SUMMARY_LABEL_DY,
              PX_LABEL, headline_colour);
}

static void draw_plugins_summary(const Panel *self, Rect r)
{
    char number[8];
    const char *headline;
    uint32_t number_colour = C_WHITE;
    uint32_t headline_colour = C_WHITE;
    int warning = 0;

    (void)self;

    if (g_plugins.config_too_large) {
        snprintf(number, sizeof(number), "%s", THEME_ABSENT);
        headline = "CONFIG TOO LARGE";
        number_colour = C_DIM;
        headline_colour = C_YELLOW;
        warning = 1;
        font_text("READ ONLY", r.x + PANEL_INSET, r.y + SUMMARY_GROUPS_Y,
                  PX_MICRO, C_YELLOW);
    } else if (!g_plugins.config_found) {
        snprintf(number, sizeof(number), "%s", THEME_ABSENT);
        headline = g_plugins.config_unreadable ? "CONFIG UNREADABLE"
                                               : "NO CONFIG.TXT";
        number_colour = C_DIM;
        headline_colour = C_YELLOW;
        warning = 1;
        font_text(g_plugins.config_unreadable ? "READ ONLY"
                                              : "UR0:TAI OR UX0:TAI",
                  r.x + PANEL_INSET, r.y + SUMMARY_GROUPS_Y, PX_MICRO,
                  C_YELLOW);
    } else {
        int count = g_plugins.count;

        snprintf(number, sizeof(number), "%d", count);
        headline = count == 1 ? "PLUGIN ONLINE" : "PLUGINS ONLINE";
        number_colour = count > 0 ? C_GREEN : C_WHITE;
        headline_colour = count > 0 ? C_WHITE : C_DIM;
        warning = g_plugins.config_invalid || g_plugins.config_truncated;

        draw_groups(r);
    }

    draw_hero(r, number, headline, number_colour, headline_colour);

    if (g_plugins.config_found && !g_plugins.config_too_large)
        draw_badges(r, warning);
}

const Panel panel_plugins_summary = {"PLUGINS", C_CYAN, draw_plugins_summary};
