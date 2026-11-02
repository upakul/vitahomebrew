#include "panels/plugins.h"

#include <stdio.h>
#include <string.h>

#include "core/plugin_editor.h"
#include "gfx/font.h"
#include "gfx/layout.h"
#include "gfx/theme.h"

/*
 * The editable ledger (D57). One panel, one row per path, grouped by the most
 * privileged line that names the path. The dot keeps its old meaning: it is
 * drawn only when the config actually activates the file.
 */

#define PLUGIN_TOP (PANEL_TITLE_Y + LINE_H(PANEL_TITLE_PX) + SP_4)
#define PLUGIN_STRIDE PLUGINS_ROW_STRIDE
#define PLUGIN_DESC_DY 22.0f
#define PLUGIN_LINE_H 14.0f
#define GROUP_HEADER_H 26.0f
#define GROUP_GAP 10.0f
#define PLUGIN_BOTTOM 16.0f
#define CONFIG_LINE_H 14.0f

#define DOT_SIZE 6.0f
#define DOT_DY 4.0f
#define LEADER_DY 6.0f
#define LEADER_GAP 8.0f
#define LEADER_END_GAP 6.0f
#define LEADER_DIM 0.6f

static int rows_in_section(PluginSection section)
{
    int n = 0;

    for (int i = 0; i < g_editor.row_count; ++i) {
        if (g_editor.rows[i].section == section)
            ++n;
    }

    return n;
}

static int group_count(void)
{
    int n = 0;

    for (int s = 0; s < PLUGIN_SECTION_COUNT; ++s) {
        if (rows_in_section((PluginSection)s) > 0)
            ++n;
    }

    return n;
}

float plugins_content_height(void)
{
    float h = PLUGIN_TOP + CONFIG_LINE_H * 2.0f + SP_2;
    int groups = group_count();

    h += (float)groups * GROUP_HEADER_H +
         (float)g_editor.row_count * PLUGIN_STRIDE;

    if (groups > 1)
        h += (float)(groups - 1) * GROUP_GAP;

    if (g_editor.orphan_count > 0 || g_editor.orphans_unverified) {
        h += GROUP_GAP + GROUP_HEADER_H + PLUGIN_LINE_H;
        h += (float)g_editor.orphan_count * PLUGIN_LINE_H;
        if (g_editor.orphans_unverified)
            h += PLUGIN_LINE_H;
    }

    if (g_editor.truncated)
        h += GROUP_GAP + PLUGIN_LINE_H;

    return h + PLUGIN_BOTTOM;
}

int plugins_row_y(int row)
{
    float y = PLUGIN_TOP + CONFIG_LINE_H * 2.0f + SP_2;
    int groups_drawn = 0;

    for (int s = 0; s < PLUGIN_SECTION_COUNT; ++s) {
        PluginSection section = (PluginSection)s;

        if (rows_in_section(section) == 0)
            continue;

        if (groups_drawn > 0)
            y += GROUP_GAP;

        y += GROUP_HEADER_H;
        ++groups_drawn;

        for (int i = 0; i < g_editor.row_count; ++i) {
            if (g_editor.rows[i].section != section)
                continue;

            if (i == row)
                return (int)y;

            y += PLUGIN_STRIDE;
        }
    }

    return (int)y;
}

int plugins_max_scroll(void)
{
    int max = (int)plugins_content_height() - (VIEW_H - VIEW_TOP_GAP);

    return max > 0 ? max : 0;
}

static uint32_t status_colour(EditorStatus status)
{
    switch (status) {
    case EDITOR_ENABLED:
        return C_GREEN;
    case EDITOR_MISSING:
    case EDITOR_PARTIAL:
        return C_YELLOW;
    case EDITOR_BOOT:
        return C_CYAN;
    case EDITOR_DISABLED:
    case EDITOR_NOT_REACHED:
    default:
        return C_DIM;
    }
}

static void draw_row(const UiState *ui, Rect r, int index, float y)
{
    const EditorRow *p = &g_editor.rows[index];
    float name_x = r.x + PANEL_PAD + 10.0f;

    int duplicate = 0;
    for (int i = 0; i < g_editor.row_count; ++i) {
        if (i != index && strcmp(g_editor.rows[i].name, p->name) == 0)
            duplicate = 1;
    }

    const char *label = duplicate ? p->path : p->name;

    if (index == ui->selection) {
        uint32_t c = editor_row_actionable(index) ? C_WHITE : C_DIM;
        draw_rect(r.x + 4.0f, y - 2.0f, 2.0f, PLUGIN_STRIDE - 6.0f, c);
    }

    font_text(label, name_x, y, PX_LABEL, C_WHITE);

    if (p->status == EDITOR_ENABLED || p->status == EDITOR_PARTIAL) {
        float dot_x = r.x + r.w - PANEL_PAD - DOT_SIZE;
        float leader_x0 = name_x + font_width(label, PX_LABEL) + LEADER_GAP;
        float leader_x1 = dot_x - LEADER_END_GAP;

        if (leader_x1 > leader_x0)
            draw_rect(leader_x0, y + LEADER_DY, leader_x1 - leader_x0, 1.0f,
                      draw_dim(C_DIM, LEADER_DIM));

        draw_rect(dot_x, y + DOT_DY, DOT_SIZE, DOT_SIZE, C_GREEN);
    }

    char status[64];
    snprintf(status, sizeof(status), "%s%s%s",
             editor_status_text(p->status), p->locked ? "  LOCKED" : "",
             p->pending ? "  PENDING" : "");

    font_text_right(status, r.x + r.w - PANEL_PAD, y + PLUGIN_DESC_DY, PX_MICRO,
                    status_colour(p->status));

    font_text(p->desc[0] != '\0' ? p->desc : "N/A", name_x, y + PLUGIN_DESC_DY,
              PX_MICRO, C_DIM);
}

static void draw_config_line(Rect r)
{
    char line[256];

    if (g_editor.source[0] == '\0') {
        snprintf(line, sizeof(line), "NO CONFIG");
    } else if (g_editor.too_large) {
        snprintf(line, sizeof(line), "%s  CONFIG TOO LARGE", g_editor.source);
    } else if (g_editor.invalid) {
        snprintf(line, sizeof(line), "%s  CONFIG INVALID", g_editor.source);
    } else if (g_editor.lines_truncated) {
        snprintf(line, sizeof(line), "%s  CONFIG TOO LONG", g_editor.source);
    } else if (g_editor.read_only) {
        snprintf(line, sizeof(line), "%s  READ ONLY", g_editor.source);
    } else if (g_editor.ignored[0] != '\0') {
        snprintf(line, sizeof(line), "%s  ACTIVE  %s IGNORED",
                 g_editor.source, g_editor.ignored);
    } else {
        snprintf(line, sizeof(line), "%s", g_editor.source);
    }

    font_text(line, r.x + PANEL_PAD, r.y + PLUGIN_TOP, PX_MICRO, C_DIM);

    const char *caption = g_editor.read_only
                              ? "CONFIG IS READ ONLY"
                              : (g_editor.backup_warning
                                     ? "CHANGES APPLY NEXT BOOT  BACKUP WARNING"
                                     : "CHANGES APPLY NEXT BOOT");
    font_text(caption, r.x + PANEL_PAD, r.y + PLUGIN_TOP + CONFIG_LINE_H,
              PX_MICRO, C_DIM);
}

void plugins_screen_draw(const UiState *ui)
{
    if (ui == NULL)
        return;

    Rect r = {(float)MARGIN,
              (float)(VIEW_Y + VIEW_TOP_GAP) - ui->plugins_scroll,
              (float)(SCREEN_W - 2 * MARGIN), plugins_content_height()};

    draw_set_clip(VIEW_X, VIEW_Y, VIEW_W, VIEW_H);
    theme_panel(r, "PLUGINS", C_CYAN, CHROME_LIST);
    draw_config_line(r);

    float y = r.y + PLUGIN_TOP + CONFIG_LINE_H * 2.0f + SP_2;
    int groups_drawn = 0;

    for (int s = 0; s < PLUGIN_SECTION_COUNT; ++s) {
        PluginSection section = (PluginSection)s;

        if (rows_in_section(section) == 0)
            continue;

        if (groups_drawn > 0)
            y += GROUP_GAP;

        if (y + LINE_H(PX_MICRO) > (float)VIEW_Y &&
            y < (float)(VIEW_Y + VIEW_H))
            font_text_tracked(editor_section_text(section), r.x + PANEL_PAD, y,
                              PX_MICRO, C_CYAN, 1.0f);

        y += GROUP_HEADER_H;
        ++groups_drawn;

        for (int i = 0; i < g_editor.row_count; ++i) {
            if (g_editor.rows[i].section != section)
                continue;

            if (y + PLUGIN_DESC_DY + PLUGIN_LINE_H > (float)VIEW_Y &&
                y < (float)(VIEW_Y + VIEW_H))
                draw_row(ui, r, i, y);

            y += PLUGIN_STRIDE;
        }
    }

    if (g_editor.orphan_count > 0 || g_editor.orphans_unverified) {
        y += GROUP_GAP;
        if (y + LINE_H(PX_MICRO) > (float)VIEW_Y &&
            y < (float)(VIEW_Y + VIEW_H))
            font_text_tracked("ORPHANS", r.x + PANEL_PAD, y, PX_MICRO, C_CYAN,
                              1.0f);

        y += GROUP_HEADER_H;

        if (g_editor.orphans_unverified) {
            if (y + PLUGIN_LINE_H > (float)VIEW_Y &&
                y < (float)(VIEW_Y + VIEW_H))
                font_text("EXCLUSIONS UNVERIFIED", r.x + PANEL_PAD, y,
                          PX_MICRO, C_YELLOW);
        } else {
            if (y + PLUGIN_LINE_H > (float)VIEW_Y &&
                y < (float)(VIEW_Y + VIEW_H))
                font_text("NOT LOADED", r.x + PANEL_PAD, y, PX_MICRO, C_DIM);
        }
        y += PLUGIN_LINE_H;

        for (int i = 0; i < g_editor.orphan_count; ++i) {
            if (y + PLUGIN_LINE_H > (float)VIEW_Y &&
                y < (float)(VIEW_Y + VIEW_H)) {
                char label[PLUGIN_PATH_MAX + 16];
                if (g_editor.orphan_shadowed[i])
                    snprintf(label, sizeof(label), "SHADOWED  %s",
                             g_editor.orphans[i]);
                else
                    snprintf(label, sizeof(label), "%s", g_editor.orphans[i]);
                font_text(label, r.x + PANEL_PAD, y, PX_MICRO, C_DIM);
            }
            y += PLUGIN_LINE_H;
        }
    }

    if (g_editor.truncated) {
        y += GROUP_GAP;
        if (y + PLUGIN_LINE_H > (float)VIEW_Y &&
            y < (float)(VIEW_Y + VIEW_H))
            font_text("MORE PLUGINS NOT SHOWN", r.x + PANEL_PAD, y, PX_MICRO,
                      C_YELLOW);
    }

    draw_clear_clip();
}
