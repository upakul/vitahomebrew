#ifndef COCKPIT_PANELS_PANEL_H
#define COCKPIT_PANELS_PANEL_H

#include <stdint.h>

#include "gfx/draw.h"
#include "gfx/theme.h"

/*
 * A panel is data plus one draw function (D1).
 *
 * A panel draws only its contents. The fill, outline, chamfer, brackets, title
 * and title rule are the theme's job, so a panel never knows whether it is the
 * hero or a satellite (D18).
 */
typedef struct Panel {
    const char *title;
    uint32_t accent;
    void (*draw)(const struct Panel *self, Rect r);
} Panel;

/*
 * Geometry lives here rather than in the panel, which is what lets the screen
 * be asymmetric (D17): a panel computing its own rect from a grid index could
 * not be 552 wide beside a 360 neighbour.
 */
typedef struct {
    const Panel *panel;
    Rect rect;
    PanelChrome chrome;
} PanelSlot;

#endif
