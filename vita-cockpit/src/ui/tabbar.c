#include "ui/tabbar.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "core/plugin_editor.h"
#include "core/plugins.h"
#include "gfx/font.h"
#include "gfx/layout.h"
#include "gfx/theme.h"

#define TAB_HOME_X 210.0f
#define TAB_PLUGINS_X 300.0f
#define TAB_Y 20.0f
#define TAB_UNDERLINE_Y 36.0f
#define TAB_UNDERLINE_H 2.0f
#define TAB_TRACK 1.0f

#define HINT_X 430.0f
#define HINT_Y 22.0f

#define SUMMARY_Y 48.0f
#define SUMMARY_MAX_W 430.0f

static void draw_tab(const char *label, float x, int active)
{
    uint32_t colour = active ? C_YELLOW : C_DIM;

    font_text_tracked(label, x, TAB_Y, PX_LABEL, colour, TAB_TRACK);

    if (active) {
        float w = font_width(label, PX_LABEL) + 8.0f;
        draw_rect(x - 2.0f, TAB_UNDERLINE_Y, w, TAB_UNDERLINE_H, C_YELLOW);
    }
}

static int group_count(PluginSection section)
{
    int n = 0;

    for (int i = 0; i < g_plugins.count; ++i) {
        if (g_plugins.items[i].section == section)
            ++n;
    }

    return n;
}

static void appendf(char *dst, size_t cap, const char *fmt, ...)
{
    size_t used = strlen(dst);
    if (used >= cap)
        return;

    va_list args;
    va_start(args, fmt);
    vsnprintf(dst + used, cap - used, fmt, args);
    va_end(args);
}

static void build_summary(char *out, size_t cap, int with_active,
                          int include_titles, int include_all,
                          int include_apps)
{
    int pending = g_editor.pending_count;
    int counts[PLUGIN_SECTION_COUNT];

    for (int s = 0; s < PLUGIN_SECTION_COUNT; ++s)
        counts[s] = group_count((PluginSection)s);

    if (with_active)
        snprintf(out, cap, "%d%s ACTIVE", g_plugins.count,
                 g_plugins.truncated ? "+" : "");
    else
        snprintf(out, cap, "%d%s", g_plugins.count,
                 g_plugins.truncated ? "+" : "");

    if (counts[PLUGIN_SECTION_KERNEL] > 0)
        appendf(out, cap, "  KERNEL %d", counts[PLUGIN_SECTION_KERNEL]);
    if (counts[PLUGIN_SECTION_SYSTEM] > 0)
        appendf(out, cap, "  SYSTEM %d", counts[PLUGIN_SECTION_SYSTEM]);
    if (include_apps && counts[PLUGIN_SECTION_SYSTEM_APP] > 0)
        appendf(out, cap, "  APPS %d", counts[PLUGIN_SECTION_SYSTEM_APP]);
    if (include_all && counts[PLUGIN_SECTION_ALL_TITLES] > 0)
        appendf(out, cap, "  ALL %d", counts[PLUGIN_SECTION_ALL_TITLES]);
    if (include_titles && counts[PLUGIN_SECTION_TITLE] > 0)
        appendf(out, cap, "  TITLES %d", counts[PLUGIN_SECTION_TITLE]);

    if (pending > 0)
        appendf(out, cap, "  %d PENDING", pending);
}

static void draw_home_summary(void)
{
    char text[256];

    if (!g_plugins.config_found) {
        const char *message = g_plugins.config_too_large ? "CONFIG TOO LARGE"
                                                         : "NO TAI CONFIG";
        font_text_right(message, (float)SCREEN_W - (float)MARGIN, SUMMARY_Y,
                        PX_MICRO, C_DIM);
        return;
    }

    if (g_plugins.config_invalid) {
        font_text_right("CONFIG INVALID", (float)SCREEN_W - (float)MARGIN,
                        SUMMARY_Y, PX_MICRO, C_DIM);
        return;
    }

    if (g_plugins.count == 0 && g_editor.pending_count == 0) {
        font_text_right("0 ACTIVE", (float)SCREEN_W - (float)MARGIN, SUMMARY_Y,
                        PX_MICRO, C_DIM);
        return;
    }

    /*
     * Keep the total and the pending token; drop the word ACTIVE first, then
     * the group counts from TITLES upward until the run clears the tab block.
     */
    for (int groups = 3; groups >= 0; --groups) {
        for (int active = 1; active >= 0; --active) {
            build_summary(text, sizeof(text), active, groups >= 3,
                          groups >= 2, groups >= 1);

            if (font_width(text, PX_MICRO) <= SUMMARY_MAX_W)
                goto done;
        }
    }

done:
    font_text_right(text, (float)SCREEN_W - (float)MARGIN, SUMMARY_Y,
                    PX_MICRO, C_DIM);
}

static void draw_plugins_summary(void)
{
    char line[160];

    if (g_editor.source[0] == '\0')
        snprintf(line, sizeof(line), "NO CONFIG");
    else if (g_editor.too_large)
        snprintf(line, sizeof(line), "%s  CONFIG TOO LARGE",
                 g_editor.source);
    else if (g_editor.invalid)
        snprintf(line, sizeof(line), "%s  CONFIG INVALID", g_editor.source);
    else if (g_editor.read_only)
        snprintf(line, sizeof(line), "%s  READ ONLY", g_editor.source);
    else if (g_editor.pending_count > 0)
        snprintf(line, sizeof(line), "%s  %d PENDING", g_editor.source,
                 g_editor.pending_count);
    else
        snprintf(line, sizeof(line), "%s", g_editor.source);

    font_text_right(line, (float)SCREEN_W - (float)MARGIN, SUMMARY_Y, PX_MICRO,
                    C_DIM);
}

void tabbar_draw(const UiState *ui)
{
    if (ui == NULL)
        return;

    draw_tab("HOME", TAB_HOME_X, ui->tab == UI_TAB_HOME);
    draw_tab("PLUGINS", TAB_PLUGINS_X, ui->tab == UI_TAB_PLUGINS);
    font_text("L/R", HINT_X, HINT_Y, PX_MICRO, C_DIM);

    if (ui->tab == UI_TAB_HOME)
        draw_home_summary();
    else
        draw_plugins_summary();
}
