#ifndef COCKPIT_PANELS_PLUGINS_H
#define COCKPIT_PANELS_PLUGINS_H

#include "gfx/theme.h"

/*
 * Geometry of the plugin list, shared with the layout table.
 *
 * The panel sizes itself to its content (D47): two lines per plugin, so the
 * region grows and shrinks with the number of loaded plugins instead of
 * clipping them. The layout asks for the height rather than hard-coding one.
 *
 * One row is: name (PX_LABEL, 14 px), SP_2 of air, description (PX_MICRO in a
 * 14 px line box), then SP_4 to the next name -- 52 px in all. The description
 * is one step down the type ladder so the name stays the identifier and the
 * gloss stays a gloss (D48).
 */

/* First name line: one step below the title's own line. */
#define PLUGIN_TOP (PANEL_TITLE_Y + LINE_H(PANEL_TITLE_PX) + SP_4)
#define PLUGIN_STRIDE 52.0f   /* one name, one description, gap to the next */
#define PLUGIN_DESC_DY 22.0f  /* description line below the name */
#define PLUGIN_LINE_H 14.0f   /* line box, not the 10.5 px glyph height */
#define PLUGIN_BOTTOM 16.0f   /* breathing room under the last description */

/* Height for the current plugin count; one row minimum for the empty state. */
float plugins_panel_height(void);

#endif
