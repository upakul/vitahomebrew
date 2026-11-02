#include "app/screen.h"

#include "gfx/draw.h"
#include "gfx/layout.h"
#include "gfx/theme.h"
#include "panels/registry.h"
#include "ui/header.h"

/* Thin track on the right edge: without an affordance a scrollable screen
 * looks like a screen that simply ends. */
#define SCROLLBAR_X ((float)SCREEN_W - 8.0f)
#define SCROLLBAR_W 4.0f

static void draw_scrollbar(float scroll)
{
    int canvas_h = layout_canvas_height();

    if (canvas_h <= VIEW_H)
        return;

    float track_h = (float)VIEW_H;
    float thumb_h = track_h * (float)VIEW_H / (float)canvas_h;
    float max_scroll = (float)layout_max_scroll();
    float t = max_scroll > 0.0f ? scroll / max_scroll : 0.0f;

    if (t < 0.0f)
        t = 0.0f;
    if (t > 1.0f)
        t = 1.0f;

    draw_rect(SCROLLBAR_X, (float)VIEW_Y, SCROLLBAR_W, track_h, C_HAIR);
    draw_rect(SCROLLBAR_X, (float)VIEW_Y + t * (track_h - thumb_h), SCROLLBAR_W,
              thumb_h, C_DIM);
}

void screen_draw(float t, float scroll)
{
    header_draw();

    /*
     * Panels are laid out on a canvas taller than the viewport. Each rect is
     * offset by the scroll and the whole band is clipped, so a panel cannot
     * bleed over the header or the status strip (D45).
     */
    draw_set_clip(VIEW_X, VIEW_Y, VIEW_W, VIEW_H);

    for (int i = 0; i < g_layout_count; ++i) {
        const PanelSlot *slot = &g_layout[i];

        Rect r = slot->rect;
        r.y += (float)VIEW_Y - scroll;

        /*
         * Off the band: the scissor would discard every pixel, so skip the
         * whole panel. The plugin list is taller than the viewport, so one end
         * of the canvas is always off-screen.
         */
        if (r.y + r.h <= (float)VIEW_Y || r.y >= (float)(VIEW_Y + VIEW_H))
            continue;

        /* Chrome first, then contents. A panel never draws its own frame, so
         * promoting one is a layout-table edit alone (D18). */
        theme_panel(r, slot->panel->title, slot->panel->accent, slot->chrome);
        slot->panel->draw(slot->panel, r);
    }

    draw_clear_clip();

    draw_scrollbar(scroll);

    /* Pinned: frame health is worth seeing at every scroll position. */
    theme_panel(g_frame.rect, g_frame.panel->title, g_frame.panel->accent,
                g_frame.chrome);
    g_frame.panel->draw(g_frame.panel, g_frame.rect);

    /* Last, so it reads as a scan over the whole screen. */
    theme_sweep(0.0f, 0.0f, (float)SCREEN_W, (float)SCREEN_H, t);
}
