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

/*
 * The summary owns the line between the tab underline and the hazard band.
 * The head is label scale so the count is readable at arm's length; the detail
 * is micro. Both end at y 56, where the hazard band starts (D63).
 */
#define SUMMARY_HEAD_Y 42.0f
#define SUMMARY_DETAIL_Y 45.5f
#define SUMMARY_RIGHT ((float)SCREEN_W - (float)MARGIN)
#define SUMMARY_MAX_W 560.0f
#define SUMMARY_GAP 12.0f
#define SUMMARY_DOT 6.0f

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

static int append_token(char *dst, size_t cap, float avail, const char *fmt,
                        ...)
{
    char token[96];
    va_list args;

    if (avail <= 0.0f)
        return 0;

    va_start(args, fmt);
    vsnprintf(token, sizeof(token), fmt, args);
    va_end(args);

    size_t used = strlen(dst);
    size_t gap = used > 0 ? 2u : 0u;
    size_t need = strlen(token);

    if (used + gap + need >= cap)
        return 0;

    char candidate[192];
    snprintf(candidate, sizeof(candidate), "%s%s%s", dst, used > 0 ? "  " : "",
             token);

    if (font_width(candidate, PX_MICRO) > avail)
        return 0;

    size_t n = strlen(candidate);
    if (n >= cap)
        n = cap - 1;
    memcpy(dst, candidate, n);
    dst[n] = '\0';
    return 1;
}

static void draw_home_summary(void)
{
    char head[64];
    char detail[192];
    uint32_t head_colour = C_WHITE;
    int warning = 0;

    detail[0] = '\0';

    if (g_plugins.config_too_large) {
        font_text_right("CONFIG TOO LARGE", SUMMARY_RIGHT, SUMMARY_HEAD_Y,
                        PX_LABEL, C_YELLOW);
        font_text_right("READ ONLY", SUMMARY_RIGHT, SUMMARY_DETAIL_Y, PX_MICRO,
                        C_YELLOW);
        return;
    }

    if (!g_plugins.config_found) {
        if (g_plugins.config_unreadable) {
            snprintf(head, sizeof(head), "CONFIG UNREADABLE");
            snprintf(detail, sizeof(detail), "READ ONLY");
        } else {
            snprintf(head, sizeof(head), "NO CONFIG.TXT");
            snprintf(detail, sizeof(detail), "UR0:TAI OR UX0:TAI");
        }
        head_colour = C_YELLOW;
        warning = 1;
    } else {
        int count = g_plugins.count;

        if (count == 1)
            snprintf(head, sizeof(head), "1 PLUGIN ONLINE");
        else
            snprintf(head, sizeof(head), "%d PLUGINS ONLINE", count);

        head_colour = count > 0 ? C_GREEN : C_WHITE;

        int config_warning =
            g_plugins.config_invalid || g_plugins.config_truncated;
        if (config_warning)
            warning = 1;

        float head_w = font_width(head, PX_LABEL);
        float avail = SUMMARY_MAX_W - head_w - SUMMARY_GAP;

        if (avail > 0.0f) {
            static const PluginSection ORDER[PLUGIN_SECTION_COUNT] = {
                PLUGIN_SECTION_KERNEL, PLUGIN_SECTION_SYSTEM,
                PLUGIN_SECTION_SYSTEM_APP, PLUGIN_SECTION_ALL_TITLES,
                PLUGIN_SECTION_TITLE};
            static const char *const LABEL[PLUGIN_SECTION_COUNT] = {
                "KERNEL", "SYSTEM", "APPS", "ALL", "TITLES"};

            char pending[32];
            pending[0] = '\0';
            if (g_editor.pending_count > 0)
                snprintf(pending, sizeof(pending), "%d PENDING",
                         g_editor.pending_count);

            float pending_w =
                pending[0] != '\0' ? font_width(pending, PX_MICRO) + 18.0f : 0.0f;
            float group_avail = avail - pending_w;

            char groups[160];
            groups[0] = '\0';

            for (int i = 0; i < PLUGIN_SECTION_COUNT; ++i) {
                int n = group_count(ORDER[i]);
                if (n == 0)
                    continue;

                if (!append_token(groups, sizeof(groups), group_avail,
                                  "%s %d", LABEL[i], n))
                    break;

            }

            /*
             * Pending comes first so it reads as a state, not as another group
             * count. It is drawn in yellow as its own run below.
             */
            char body[160];
            body[0] = '\0';

            if (pending[0] != '\0' && groups[0] != '\0')
                snprintf(body, sizeof(body), "%s  %s", pending, groups);
            else if (pending[0] != '\0')
                snprintf(body, sizeof(body), "%s", pending);
            else if (groups[0] != '\0')
                snprintf(body, sizeof(body), "%s", groups);

            if (config_warning && body[0] != '\0')
                snprintf(detail, sizeof(detail), "CONFIG WARNING  %s", body);
            else if (config_warning)
                snprintf(detail, sizeof(detail), "CONFIG WARNING");
            else
                snprintf(detail, sizeof(detail), "%s", body);
        } else if (config_warning) {
            snprintf(detail, sizeof(detail), "CONFIG WARNING");
        }
    }

    if (warning)
        head_colour = C_YELLOW;

    uint32_t detail_colour = warning ? C_YELLOW : C_DIM;

    float head_w = font_width(head, PX_LABEL);
    float detail_w = detail[0] != '\0' ? font_width(detail, PX_MICRO) : 0.0f;
    float total = head_w + (detail_w > 0.0f ? detail_w + SUMMARY_GAP : 0.0f);

    if (total > SUMMARY_MAX_W) {
        detail[0] = '\0';
        detail_w = 0.0f;
        total = head_w;
    }

    float x = SUMMARY_RIGHT - total;

    if (detail_w > 0.0f) {
        if (!warning && g_editor.pending_count > 0) {
            char pending[32];
            int pending_len = snprintf(pending, sizeof(pending), "%d PENDING",
                                       g_editor.pending_count);

            font_text(pending, x, SUMMARY_DETAIL_Y, PX_MICRO, C_YELLOW);

            if (pending_len > 0 && pending_len + 2 < (int)strlen(detail)) {
                float detail_x =
                    x + font_width(pending, PX_MICRO) + 18.0f;
                font_text(detail + pending_len + 2, detail_x,
                          SUMMARY_DETAIL_Y, PX_MICRO, detail_colour);
            }
        } else {
            font_text(detail, x, SUMMARY_DETAIL_Y, PX_MICRO, detail_colour);
        }
    }

    float head_x = x + (detail_w > 0.0f ? detail_w + SUMMARY_GAP : 0.0f);

    if (!warning && g_plugins.config_found && g_plugins.count > 0) {
        float dot_x = head_x - SUMMARY_DOT - 8.0f;
        if (dot_x >= (float)MARGIN)
            draw_rect(dot_x, SUMMARY_HEAD_Y + 4.0f, SUMMARY_DOT, SUMMARY_DOT,
                      C_GREEN);
    }

    font_text(head, head_x, SUMMARY_HEAD_Y, PX_LABEL, head_colour);
}

static void draw_plugins_summary(void)
{
    char line[224];
    int warning = 0;

    if (g_editor.unreadable) {
        snprintf(line, sizeof(line), "CONFIG UNREADABLE");
        warning = 1;
    } else if (g_editor.source[0] == '\0') {
        snprintf(line, sizeof(line), "NO CONFIG.TXT");
        warning = 1;
    } else if (g_editor.too_large) {
        snprintf(line, sizeof(line), "%s  CONFIG TOO LARGE", g_editor.source);
        warning = 1;
    } else if (g_editor.unreadable) {
        snprintf(line, sizeof(line), "%s  CONFIG UNREADABLE",
                 g_editor.source);
        warning = 1;
    } else if (g_editor.lines_truncated) {
        snprintf(line, sizeof(line), "%s  CONFIG TOO LONG", g_editor.source);
        warning = 1;
    } else if (g_editor.rows_truncated) {
        snprintf(line, sizeof(line), "%s  TOO MANY PLUGIN ROWS",
                 g_editor.source);
        warning = 1;
    } else {
        snprintf(line, sizeof(line), "%s  %d ROWS  %d ACTIVATED",
                 g_editor.source, g_editor.row_count,
                 g_editor.activated_total);

        if (g_editor.pending_count > 0) {
            size_t used = strlen(line);
            snprintf(line + used, sizeof(line) - used, "  %d PENDING",
                     g_editor.pending_count);
        }

        if (g_editor.invalid || g_editor.backup_warning) {
            size_t used = strlen(line);
            snprintf(line + used, sizeof(line) - used,
                     g_editor.invalid ? "  CONFIG WARNING" : "  BACKUP WARNING");
            warning = 1;
        }
    }

    font_text_right(line, SUMMARY_RIGHT, SUMMARY_HEAD_Y, PX_MICRO,
                    warning ? C_YELLOW : C_DIM);
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
