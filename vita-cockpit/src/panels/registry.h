#ifndef COCKPIT_PANELS_REGISTRY_H
#define COCKPIT_PANELS_REGISTRY_H

#include "panels/panel.h"

extern PanelSlot g_layout[];
extern const int g_layout_count;

/* Height of the scrolling canvas, derived from the lowest panel edge. */
int layout_canvas_height(void);

/* Largest valid scroll offset: the canvas height minus one viewport. */
int layout_max_scroll(void);

/* Pinned status strip, drawn in screen coordinates and never scrolled. */
extern const PanelSlot g_frame;

#endif
