#include "ui/modal.h"

#include <stdio.h>
#include <string.h>

#include "gfx/draw.h"
#include "gfx/font.h"
#include "gfx/layout.h"
#include "gfx/theme.h"

#define MODAL_W 600.0f
#define MODAL_H 240.0f
#define MODAL_X ((float)(SCREEN_W / 2) - MODAL_W / 2.0f)
#define MODAL_Y 152.0f
#define MODAL_PAD 24.0f
#define MODAL_BODY_Y 54.0f
#define MODAL_KEY_Y 206.0f
#define MODAL_BODY_W (MODAL_W - 2.0f * MODAL_PAD)
#define MODAL_MAX_LINES 6
#define MODAL_CHOICE_Y 100.0f
#define MODAL_CHOICE_H 30.0f

static void copy_text(char *dst, size_t cap, const char *src)
{
    if (cap == 0)
        return;

    snprintf(dst, cap, "%s", src == NULL ? "" : src);
}

void modal_open_confirm(UiState *ui, int row, int mode, const char *text)
{
    if (ui == NULL)
        return;

    ui->modal = UI_MODAL_CONFIRM;
    ui->modal_row = row;
    ui->modal_mode = mode;
    ui->modal_choice = 0;
    ui->modal_choice_count = 0;
    copy_text(ui->modal_title, sizeof(ui->modal_title), "CONFIRM CHANGE");
    copy_text(ui->modal_body, sizeof(ui->modal_body), text);
}

void modal_open_error(UiState *ui, const char *text)
{
    if (ui == NULL)
        return;

    ui->modal = UI_MODAL_ERROR;
    ui->modal_row = -1;
    ui->modal_mode = -1;
    ui->modal_choice = 0;
    ui->modal_choice_count = 0;
    copy_text(ui->modal_title, sizeof(ui->modal_title), "NOT WRITTEN");
    copy_text(ui->modal_body, sizeof(ui->modal_body), text);
}

void modal_open_info(UiState *ui, const char *text)
{
    if (ui == NULL)
        return;

    ui->modal = UI_MODAL_INFO;
    ui->modal_row = -1;
    ui->modal_mode = -1;
    ui->modal_choice = 0;
    ui->modal_choice_count = 0;
    copy_text(ui->modal_title, sizeof(ui->modal_title), "CANNOT CHANGE");
    copy_text(ui->modal_body, sizeof(ui->modal_body), text);
}

void modal_open_receipt(UiState *ui, const char *title, const char *text)
{
    if (ui == NULL)
        return;

    ui->modal = UI_MODAL_RECEIPT;
    ui->modal_row = -1;
    ui->modal_mode = -1;
    ui->modal_choice = 0;
    ui->modal_choice_count = 0;
    copy_text(ui->modal_title, sizeof(ui->modal_title),
              title == NULL ? "WRITTEN" : title);
    copy_text(ui->modal_body, sizeof(ui->modal_body), text);
}

void modal_open_session(UiState *ui, const char *text, const char *const *choices,
                        int count)
{
    if (ui == NULL)
        return;

    ui->modal = UI_MODAL_SESSION;
    ui->modal_row = -1;
    ui->modal_mode = -1;
    ui->modal_choice = 0;
    ui->modal_choice_count = 0;

    if (count < 0)
        count = 0;
    if (count > UI_MODAL_CHOICES_MAX)
        count = UI_MODAL_CHOICES_MAX;

    for (int i = 0; i < count; ++i)
        copy_text(ui->modal_choices[i], UI_MODAL_CHOICE_TEXT, choices[i]);

    ui->modal_choice_count = count;
    copy_text(ui->modal_title, sizeof(ui->modal_title), "PLUGIN SESSION");
    copy_text(ui->modal_body, sizeof(ui->modal_body), text);
}

void modal_open_restart(UiState *ui, const char *text)
{
    if (ui == NULL)
        return;

    ui->modal = UI_MODAL_RESTART;
    ui->modal_row = -1;
    ui->modal_mode = -1;
    ui->modal_choice = 0;
    ui->modal_choice_count = 0;
    copy_text(ui->modal_title, sizeof(ui->modal_title), "RESTART THE VITA");
    copy_text(ui->modal_body, sizeof(ui->modal_body), text);
}

static int draw_wrapped(const char *text, float x, float y, float max_w,
                        float px, uint32_t colour, int max_lines)
{
    char line[256];
    int used = 0;
    int lines = 0;
    const char *p = text;

    if (text == NULL || *text == '\0')
        return 0;

    line[0] = '\0';

    while (*p != '\0' && lines < max_lines) {
        while (*p == ' ' || *p == '\n')
            ++p;
        if (*p == '\0')
            break;

        const char *word = p;
        while (*p != '\0' && *p != ' ' && *p != '\n')
            ++p;

        int word_len = (int)(p - word);
        char candidate[256];

        if (used > 0)
            snprintf(candidate, sizeof(candidate), "%s %.*s", line, word_len,
                     word);
        else
            snprintf(candidate, sizeof(candidate), "%.*s", word_len, word);

        if (used > 0 && font_width(candidate, px) > max_w) {
            font_text(line, x, y + (float)lines * LINE_H(px), px, colour);
            ++lines;
            if (lines >= max_lines)
                break;

            snprintf(line, sizeof(line), "%.*s", word_len, word);
            used = (int)strlen(line);
        } else {
            snprintf(line, sizeof(line), "%s", candidate);
            used = (int)strlen(line);
        }
    }

    if (lines < max_lines && line[0] != '\0')
        font_text(line, x, y + (float)lines * LINE_H(px), px, colour);

    return lines;
}

static void draw_choices(const UiState *ui, Rect r)
{
    for (int i = 0; i < ui->modal_choice_count; ++i) {
        float y = r.y + MODAL_CHOICE_Y + (float)i * MODAL_CHOICE_H;
        int selected = i == ui->modal_choice;

        if (selected)
            draw_rect(r.x + MODAL_PAD - 8.0f, y - 2.0f, 2.0f,
                      MODAL_CHOICE_H - 6.0f, C_WHITE);

        font_text(ui->modal_choices[i], r.x + MODAL_PAD, y, PX_LABEL,
                  selected ? C_WHITE : C_DIM);
    }
}

void modal_draw(const UiState *ui)
{
    if (ui == NULL || ui->modal == UI_MODAL_NONE)
        return;

    Rect r = {MODAL_X, MODAL_Y, MODAL_W, MODAL_H};
    int danger = ui->modal == UI_MODAL_CONFIRM ||
                 ui->modal == UI_MODAL_ERROR ||
                 ui->modal == UI_MODAL_RESTART;
    uint32_t accent = danger ? C_YELLOW : C_CYAN;

    theme_panel(r, ui->modal_title, accent, CHROME_SATELLITE);

    draw_wrapped(ui->modal_body, r.x + MODAL_PAD, r.y + MODAL_BODY_Y,
                 MODAL_BODY_W, PX_MICRO, C_WHITE, MODAL_MAX_LINES);

    if (ui->modal == UI_MODAL_SESSION)
        draw_choices(ui, r);

    const char *keys;

    switch (ui->modal) {
    case UI_MODAL_CONFIRM:
        keys = ui->modal_mode == 2 ? "CROSS RESTORE  CIRCLE CANCEL"
                                   : "CROSS CONFIRM  CIRCLE CANCEL";
        break;
    case UI_MODAL_RESTART:
        keys = "CROSS RESTART  CIRCLE CANCEL";
        break;
    case UI_MODAL_SESSION:
        keys = "CROSS SELECT  CIRCLE CLOSE";
        break;
    case UI_MODAL_RECEIPT:
        keys = "CROSS OK";
        break;
    default:
        keys = "CROSS CLOSE";
        break;
    }

    font_text(keys, r.x + MODAL_PAD, r.y + MODAL_KEY_Y, PX_MICRO, C_DIM);
}
