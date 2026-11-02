#include "ui/tabbar.h"

#include <stdio.h>
#include <string.h>

#include "core/plugin_editor.h"
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
 * The PLUGINS tab keeps one header line: the active config and the editor's
 * state. It stays visible when the ledger is scrolled away from its own
 * header. HOME carries no header summary; its plugin count is a block below
 * the panels (D67).
 */
#define SUMMARY_Y 45.5f
#define SUMMARY_RIGHT ((float)SCREEN_W - (float)MARGIN)

static void draw_tab(const char *label, float x, int active)
{
    uint32_t colour = active ? C_YELLOW : C_DIM;

    font_text_tracked(label, x, TAB_Y, PX_LABEL, colour, TAB_TRACK);

    if (active) {
        float w = font_width(label, PX_LABEL) + 8.0f;
        draw_rect(x - 2.0f, TAB_UNDERLINE_Y, w, TAB_UNDERLINE_H, C_YELLOW);
    }
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
                     g_editor.invalid ? "  CONFIG WARNING"
                                      : "  BACKUP WARNING");
            warning = 1;
        }
    }

    font_text_right(line, SUMMARY_RIGHT, SUMMARY_Y, PX_MICRO,
                    warning ? C_YELLOW : C_DIM);
}

void tabbar_draw(const UiState *ui)
{
    if (ui == NULL)
        return;

    draw_tab("HOME", TAB_HOME_X, ui->tab == UI_TAB_HOME);
    draw_tab("PLUGINS", TAB_PLUGINS_X, ui->tab == UI_TAB_PLUGINS);
    font_text("L/R", HINT_X, HINT_Y, PX_MICRO, C_DIM);

    if (ui->tab == UI_TAB_PLUGINS)
        draw_plugins_summary();
}
