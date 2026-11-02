#include "ui/tabbar.h"

#include "gfx/font.h"
#include "gfx/layout.h"
#include "gfx/theme.h"
#include "ui/ui_state.h"

#define TAB_HOME_X 210.0f
#define TAB_PLUGINS_X 300.0f
#define TAB_Y 20.0f
#define TAB_UNDERLINE_Y 36.0f
#define TAB_UNDERLINE_H 2.0f
#define TAB_TRACK 1.0f

#define HINT_X 430.0f
#define HINT_Y 22.0f

static void draw_tab(const char *label, float x, int active)
{
    uint32_t colour = active ? C_YELLOW : C_DIM;

    font_text_tracked(label, x, TAB_Y, PX_LABEL, colour, TAB_TRACK);

    if (active) {
        float w = font_width(label, PX_LABEL) + 8.0f;
        draw_rect(x - 2.0f, TAB_UNDERLINE_Y, w, TAB_UNDERLINE_H, C_YELLOW);
    }
}

void tabbar_draw(const UiState *ui)
{
    if (ui == NULL)
        return;

    draw_tab("HOME", TAB_HOME_X, ui->tab == UI_TAB_HOME);
    draw_tab("PLUGINS", TAB_PLUGINS_X, ui->tab == UI_TAB_PLUGINS);
    font_text("L/R", HINT_X, HINT_Y, PX_MICRO, C_DIM);
}
