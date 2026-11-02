#include "panels/plugins.h"

#include <stdio.h>
#include <string.h>

#include "core/plugin_editor.h"
#include "core/tai_config.h"
#include "gfx/font.h"
#include "gfx/layout.h"
#include "gfx/theme.h"

/*
 * The editable ledger (D57, D62, D66). One panel, one row per path, grouped by
 * the most privileged line that names the path. Each row carries a status badge
 * and an action control so the user can see what the row is and whether the
 * action is possible before pressing Cross.
 */

#define PLUGIN_TOP (PANEL_TITLE_Y + LINE_H(PANEL_TITLE_PX) + SP_4)
#define PLUGIN_STRIDE PLUGINS_ROW_STRIDE
#define PLUGIN_DESC_DY 22.0f
#define PLUGIN_LINE_H 14.0f
#define GROUP_HEADER_H 26.0f
#define GROUP_GAP 10.0f
#define PLUGIN_BOTTOM 16.0f
#define CONFIG_LINE_H 14.0f
#define CONFIG_LINE_COUNT 3
#define ROWS_TOP (PLUGIN_TOP + CONFIG_LINE_H * CONFIG_LINE_COUNT + SP_2)
#define PLUGIN_EMPTY_H 30.0f

#define DOT_SIZE 6.0f
#define DOT_DY 4.0f
#define NAME_GAP 12.0f
#define CONTROL_W 78.0f
#define CONTROL_H 16.0f
#define CONTROL_GAP 10.0f
#define BADGE_H 12.0f
#define BADGE_PAD 5.0f
#define BADGE_GAP 6.0f

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
    float h = ROWS_TOP;

    if (g_editor.row_count == 0) {
        h += PLUGIN_EMPTY_H;
    } else {
        int groups = group_count();

        h += (float)groups * GROUP_HEADER_H +
             (float)g_editor.row_count * PLUGIN_STRIDE;

        if (groups > 1)
            h += (float)(groups - 1) * GROUP_GAP;
    }

    if (g_editor.orphan_count > 0 || g_editor.orphans_unverified) {
        h += GROUP_GAP + GROUP_HEADER_H + PLUGIN_LINE_H;
        h += (float)g_editor.orphan_count * PLUGIN_LINE_H;
        if (g_editor.orphans_unverified)
            h += PLUGIN_LINE_H;
        if (g_editor.orphan_truncated)
            h += PLUGIN_LINE_H;
    }

    if (g_editor.truncated)
        h += GROUP_GAP + PLUGIN_LINE_H;

    return h + PLUGIN_BOTTOM;
}

int plugins_row_y(int row)
{
    float y = ROWS_TOP;
    int groups_drawn = 0;

    if (g_editor.row_count == 0)
        return (int)y;

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
    case EDITOR_LOADED:
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

static float badge_width(const char *text)
{
    return font_width(text, PX_MICRO) + BADGE_PAD * 2.0f;
}

static void draw_badge(float x, float y, const char *text, uint32_t colour)
{
    float w = badge_width(text);

    draw_rect(x, y, w, BADGE_H, draw_dim(colour, 0.16f));
    font_text(text, x + BADGE_PAD, y + 2.0f, PX_MICRO, colour);
}

static void draw_control(float right, float y, const char *label,
                         int actionable, int action)
{
    uint32_t colour = C_DIM;

    if (actionable)
        colour = action == EDITOR_ACTION_ENABLE ? C_GREEN : C_CYAN;

    float x = right - CONTROL_W;

    draw_rect(x, y - 1.0f, CONTROL_W, CONTROL_H,
              actionable ? draw_dim(colour, 0.20f)
                         : draw_dim(C_HAIR, 0.45f));

    float tw = font_width(label, PX_MICRO);
    font_text(label, x + (CONTROL_W - tw) * 0.5f, y + 2.0f, PX_MICRO,
              actionable ? colour : C_DIM);
}

static void draw_name(const char *label, float x, float y, float avail)
{
    char display[PLUGIN_PATH_MAX];
    float px = PX_LABEL;

    tai_copy(display, sizeof(display), label);

    if (font_width(display, px) > avail) {
        px = PX_MICRO;
        if (font_width(display, px) > avail) {
            int max_chars = (int)((avail + px) / (6.0f * px));
            if (max_chars < 3)
                max_chars = 3;
            if (max_chars < (int)sizeof(display)) {
                display[max_chars - 2] = '.';
                display[max_chars - 1] = '.';
                display[max_chars] = '\0';
            }
        }
    }

    font_text(display, x, y, px, C_WHITE);
}

static void draw_row(const UiState *ui, Rect r, int index, float y)
{
    const EditorRow *p = &g_editor.rows[index];
    float name_x = r.x + PANEL_PAD + NAME_GAP;
    float right = r.x + r.w - PANEL_PAD;

    const char *label = editor_row_label(index);
    uint32_t colour = status_colour(p->status);
    int action = editor_next_action(index);

    if (index == ui->selection)
        draw_rect(r.x + 4.0f, y - 2.0f, 2.0f, PLUGIN_STRIDE - 6.0f, C_WHITE);

    draw_rect(r.x + PANEL_PAD, y + DOT_DY, DOT_SIZE, DOT_SIZE, colour);

    draw_name(label, name_x, y, right - CONTROL_W - CONTROL_GAP - name_x);
    draw_control(right, y, editor_action_text(index),
                 action != EDITOR_ACTION_NONE, action);

    const char *status = editor_status_text(p->status);
    float status_x = right - badge_width(status);
    float badge_y = y + PLUGIN_DESC_DY - 2.0f;

    draw_badge(status_x, badge_y, status, colour);

    if (p->next_change != 0) {
        const char *next = p->next_change > 0 ? "NEXT ON" : "NEXT OFF";
        float next_x = status_x - BADGE_GAP - badge_width(next);

        draw_badge(next_x, badge_y, next, C_YELLOW);
    }

    font_text(p->desc[0] != '\0' ? p->desc : "N/A", name_x,
              y + PLUGIN_DESC_DY, PX_MICRO, C_DIM);
}

static void draw_config_lines(Rect r)
{
    char line[256];
    uint32_t colour = C_DIM;
    float y = r.y + PLUGIN_TOP;

    if (g_editor.source[0] == '\0') {
        snprintf(line, sizeof(line), "NO CONFIG.TXT");
        colour = C_YELLOW;
    } else if (g_editor.ignored[0] != '\0') {
        snprintf(line, sizeof(line), "USING %s   %s IS NOT USED",
                 g_editor.source, g_editor.ignored);
    } else {
        snprintf(line, sizeof(line), "USING %s", g_editor.source);
    }

    if (g_editor.recovered) {
        size_t used = strlen(line);
        snprintf(line + used, sizeof(line) - used, "   RECOVERED");
    }

    font_text(line, r.x + PANEL_PAD, y, PX_MICRO, colour);

    y += CONFIG_LINE_H;
    colour = C_DIM;

    const char *read_only = editor_readonly_reason();

    if (read_only != NULL) {
        snprintf(line, sizeof(line), "READ ONLY - %s", read_only);
        colour = C_YELLOW;
    } else {
        snprintf(line, sizeof(line), "%d ROWS  %d LOADED  %d NEXT BOOT",
                 g_editor.row_count, g_editor.loaded_total,
                 g_editor.pending_count);

        if (g_editor.invalid || g_editor.paths_truncated) {
            size_t used = strlen(line);
            snprintf(line + used, sizeof(line) - used, "  CONFIG WARNING");
            colour = C_YELLOW;
        }

        if (g_editor.backup_warning) {
            size_t used = strlen(line);
            snprintf(line + used, sizeof(line) - used, "  BACKUP WARNING");
            colour = C_YELLOW;
        }
    }

    font_text(line, r.x + PANEL_PAD, y, PX_MICRO, colour);

    y += CONFIG_LINE_H;

    const char *hint = read_only != NULL ? "TRIANGLE SESSION"
                                         : "CROSS CHANGE   TRIANGLE SESSION";

    font_text(hint, r.x + PANEL_PAD, y, PX_MICRO, C_DIM);
    font_text_right(g_editor.undo_available ? "UNDO READY" : "NO RESTORE POINT",
                    r.x + r.w - PANEL_PAD, y, PX_MICRO, C_DIM);
}

static void draw_empty_state(Rect r)
{
    float y = r.y + ROWS_TOP;

    if (g_editor.source[0] == '\0') {
        font_text("NO CONFIG.TXT FOUND", r.x + PANEL_PAD, y, PX_MICRO,
                  C_YELLOW);
        font_text("EXPECTED UR0:TAI OR UX0:TAI", r.x + PANEL_PAD,
                  y + PLUGIN_LINE_H, PX_MICRO, C_DIM);
    } else {
        font_text("NO PLUGIN ENTRIES IN THE ACTIVE CONFIG", r.x + PANEL_PAD, y,
                  PX_MICRO, C_DIM);
    }
}

void plugins_screen_draw(const UiState *ui, float content_h)
{
    if (ui == NULL)
        return;

    Rect r = {(float)MARGIN,
              (float)(VIEW_Y + VIEW_TOP_GAP) - ui->plugins_scroll,
              (float)(SCREEN_W - 2 * MARGIN), content_h};

    draw_set_clip(VIEW_X, VIEW_Y, VIEW_W, VIEW_H);
    theme_panel(r, "PLUGINS", C_CYAN, CHROME_LIST);
    draw_config_lines(r);

    if (g_editor.row_count == 0)
        draw_empty_state(r);

    float y =
        r.y + ROWS_TOP +
        (g_editor.row_count == 0 ? PLUGIN_EMPTY_H : 0.0f);
    PluginSection group = PLUGIN_SECTION_COUNT;

    for (int i = 0; i < g_editor.row_count; ++i) {
        const EditorRow *row = &g_editor.rows[i];

        if (row->section != group) {
            if (group != PLUGIN_SECTION_COUNT)
                y += GROUP_GAP;

            group = row->section;

            if (y + LINE_H(PX_MICRO) > (float)VIEW_Y &&
                y < (float)(VIEW_Y + VIEW_H))
                font_text_tracked(editor_section_text(group), r.x + PANEL_PAD,
                                  y, PX_MICRO, C_CYAN, 1.0f);

            y += GROUP_HEADER_H;
        }

        if (y + PLUGIN_DESC_DY + PLUGIN_LINE_H > (float)VIEW_Y &&
            y < (float)(VIEW_Y + VIEW_H))
            draw_row(ui, r, i, y);

        y += PLUGIN_STRIDE;
    }

    if (g_editor.orphan_count > 0 || g_editor.orphans_unverified) {
        y += GROUP_GAP;
        if (y + LINE_H(PX_MICRO) > (float)VIEW_Y &&
            y < (float)(VIEW_Y + VIEW_H))
            font_text_tracked("INSTALLED - NOT IN CONFIG", r.x + PANEL_PAD, y,
                              PX_MICRO, C_CYAN, 1.0f);

        y += GROUP_HEADER_H;

        if (g_editor.orphans_unverified) {
            if (y + PLUGIN_LINE_H > (float)VIEW_Y &&
                y < (float)(VIEW_Y + VIEW_H))
                font_text("EXCLUSIONS UNVERIFIED", r.x + PANEL_PAD, y,
                          PX_MICRO, C_YELLOW);
        } else {
            if (y + PLUGIN_LINE_H > (float)VIEW_Y &&
                y < (float)(VIEW_Y + VIEW_H))
                font_text("NOT LISTED IN THE ACTIVE CONFIG", r.x + PANEL_PAD,
                          y, PX_MICRO, C_DIM);
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

        if (g_editor.orphan_truncated) {
            if (y + PLUGIN_LINE_H > (float)VIEW_Y &&
                y < (float)(VIEW_Y + VIEW_H))
                font_text("MORE FILES NOT SHOWN", r.x + PANEL_PAD, y,
                          PX_MICRO, C_YELLOW);
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
