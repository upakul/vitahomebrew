#ifndef COCKPIT_GFX_LAYOUT_H
#define COCKPIT_GFX_LAYOUT_H

/*
 * Screen geometry (D11, D24).
 *
 * These live in gfx/ rather than app/ because panels/registry.c and
 * ui/header.c both need them, and both sit inward of app/. A constant its
 * consumers cannot include is a constant that gets hardcoded instead.
 */

#define SCREEN_W 960
#define SCREEN_H 544

/*
 * The panel area is a scrolling viewport. Content is laid out on a taller canvas
 * and offset by the scroll position, clipped to this band so nothing bleeds over
 * the header or the status strip (D45).
 */
#define VIEW_X 0
#define VIEW_Y 74
#define VIEW_W 960
#define VIEW_H 396

/* Distance from the screen edge to the panel area. */
#define MARGIN 18

/* Distance between the two columns. */
#define GAP 12

#endif
