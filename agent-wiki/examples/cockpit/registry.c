#include "panels/registry.h"

/*
 * The layout table (D11, D17).
 *
 * Geometry is data here, not a computation: the screen is deliberately
 * asymmetric, so there is no formula that could produce these rects. Adding a
 * region means deciding which other region gives up space.
 *
 *   CORE    552 x 240   hero, clocks and memory
 *   STORAGE 552 x 144   satellites below and beside it
 *   POWER   360 x 216
 *   SYSTEM  360 x 168
 *   FRAME   924 x  44   status strip, spans both columns
 */

extern const Panel panel_core;
extern const Panel panel_storage;
extern const Panel panel_power;
extern const Panel panel_system;
extern const Panel panel_frame;

const PanelSlot g_layout[] = {
    {&panel_core, {18.0f, 74.0f, 552.0f, 240.0f}, CHROME_HERO},
    {&panel_storage, {18.0f, 326.0f, 552.0f, 144.0f}, CHROME_SATELLITE},
    {&panel_power, {582.0f, 74.0f, 360.0f, 216.0f}, CHROME_SATELLITE},
    {&panel_system, {582.0f, 302.0f, 360.0f, 168.0f}, CHROME_SATELLITE},
    {&panel_frame, {18.0f, 482.0f, 924.0f, 44.0f}, CHROME_STRIP},
};

const int g_layout_count = (int)(sizeof(g_layout) / sizeof(g_layout[0]));
