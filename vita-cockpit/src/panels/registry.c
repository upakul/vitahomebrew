#include "panels/registry.h"

#include "gfx/layout.h"
#include "gfx/theme.h"
#include "panels/plugins.h"

extern const Panel panel_core;
extern const Panel panel_storage;
extern const Panel panel_power;
extern const Panel panel_system;
extern const Panel panel_plugins;
extern const Panel panel_frame;

/*
 * The layout table (D11, D17, D43, D45).
 *
 * Coordinates are canvas coordinates: y = 0 is the top of the scrolling panel
 * area, not the top of the screen. The app offsets every rect by the current
 * scroll and clips to the viewport, so the canvas can be taller than the screen
 * without any panel knowing about scrolling.
 *
 *   CORE    552 x 210   hero, clocks and memory
 *   POWER   360 x 210   battery
 *   STORAGE 552 x 172   volumes
 *   SYSTEM  360 x 172   identity
 *   PLUGINS 924 x self-sizing, below both columns
 *
 * Both columns share both seams: row one is 210 tall, row two 172, so the
 * horizontal gap between the rows runs unbroken across the screen. The first
 * version ended both columns at the same y but started STORAGE 24 px below
 * SYSTEM, which read as a misaligned grid rather than a deliberate one (D48).
 */

#define LEFT_W 552.0f
#define RIGHT_W 360.0f

/*
 * Row one is sized by its tallest content: POWER's hero, state and two readouts
 * end at 183.5, CORE's trace at 184.5. Both panels take the same height so the
 * columns share a seam, with at least SP_5 of air below the content.
 */
#define CORE_Y 0.0f
#define CORE_H 210.0f
#define POWER_Y 0.0f
#define POWER_H CORE_H

/*
 * Row two is sized by SYSTEM's three readouts, which end at 147. STORAGE's two
 * volume blocks fill the same box, so the row ends flush across both columns.
 */
#define STORAGE_Y (CORE_H + SP_3)
#define STORAGE_H 172.0f
#define SYSTEM_Y STORAGE_Y
#define SYSTEM_H STORAGE_H

#define PLUGINS_Y (STORAGE_Y + STORAGE_H + SP_3)

/*
 * The plugin list sizes itself to its content (D47), so the PLUGINS rect is
 * computed rather than compiled in. layout_update() must be called after the
 * plugin scan, before the first frame.
 */
static PanelSlot *plugins_slot(void)
{
    for (int i = 0; i < g_layout_count; ++i) {
        if (g_layout[i].panel == &panel_plugins)
            return &g_layout[i];
    }

    return NULL;
}

void layout_update(void)
{
    PanelSlot *slot = plugins_slot();

    /* Fail closed: an unfound slot keeps the compiled-in height. */
    if (slot != NULL)
        slot->rect.h = plugins_panel_height();
}

int layout_canvas_height(void)
{
    float bottom = 0.0f;

    for (int i = 0; i < g_layout_count; ++i) {
        float b = g_layout[i].rect.y + g_layout[i].rect.h;

        if (b > bottom)
            bottom = b;
    }

    return (int)(bottom + SP_3);
}

int layout_max_scroll(void)
{
    int max = VIEW_TOP_GAP + layout_canvas_height() - VIEW_H;

    return max > 0 ? max : 0;
}

/*
 * The status strip is pinned to the screen rather than living on the canvas:
 * frame health is worth seeing at every scroll position.
 */
const PanelSlot g_frame = {
    &panel_frame,
    {(float)MARGIN, 482.0f, (float)(SCREEN_W - 2 * MARGIN), 44.0f},
    CHROME_STRIP,
};

PanelSlot g_layout[] = {
    {&panel_core,
     {(float)MARGIN, CORE_Y, LEFT_W, CORE_H},
     CHROME_HERO},
    {&panel_power,
     {(float)(MARGIN + LEFT_W + GAP), POWER_Y, RIGHT_W, POWER_H},
     CHROME_SATELLITE},
    {&panel_storage,
     {(float)MARGIN, STORAGE_Y, LEFT_W, STORAGE_H},
     CHROME_SATELLITE},
    {&panel_system,
     {(float)(MARGIN + LEFT_W + GAP), SYSTEM_Y, RIGHT_W, SYSTEM_H},
     CHROME_SATELLITE},
    {&panel_plugins,
     {(float)MARGIN, PLUGINS_Y, (float)(SCREEN_W - 2 * MARGIN), 0.0f},
     CHROME_LIST},
};

const int g_layout_count = (int)(sizeof(g_layout) / sizeof(g_layout[0]));
