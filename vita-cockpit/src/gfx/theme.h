#ifndef COCKPIT_GFX_THEME_H
#define COCKPIT_GFX_THEME_H

#include <stddef.h>
#include <stdint.h>

#include "core/history.h"
#include "gfx/draw.h"
#include "gfx/font.h"

/*
 * The visual vocabulary: palette, panel chrome, readouts, meters, traces.
 *
 * Nothing here decides *what* is shown, only how a given thing looks. A panel
 * that hard-codes a colour or a corner size is a bug: change the theme instead.
 * See docs/DESIGN.md.
 */

/*
 * vita2d packs colour as ABGR in a uint32; red is the low byte.
 *
 * Prefixed because vita2d.h defines a bare RGBA8 with the same meaning; this
 * header deliberately does not include vita2d.h, so the two must not collide.
 */
#define COCKPIT_RGBA8(r, g, b, a)                                    \
    ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(g) << 8 | \
     (uint32_t)(r))

#define C_FIELD COCKPIT_RGBA8(0x00, 0x00, 0x00, 0xFF)
#define C_PANEL COCKPIT_RGBA8(0x0D, 0x0E, 0x12, 0xFF)
#define C_HAIR COCKPIT_RGBA8(0x1C, 0x1D, 0x24, 0xFF)
#define C_DIM COCKPIT_RGBA8(0x6E, 0x70, 0x7E, 0xFF)
#define C_WHITE COCKPIT_RGBA8(0xED, 0xED, 0xF0, 0xFF)
#define C_YELLOW COCKPIT_RGBA8(0xFC, 0xEE, 0x0A, 0xFF)
#define C_CYAN COCKPIT_RGBA8(0x2A, 0xD8, 0xE8, 0xFF)
/*
 * Status green, used only for the plugin list's loaded marker (D47). It is not
 * an accent: nothing else may use it, so a green mark means "this is loaded"
 * and never "look here".
 */
#define C_GREEN COCKPIT_RGBA8(0x3B, 0xE0, 0x7A, 0xFF)
/* FRAME's accent, per the accent table: cyan at 60 %. */
#define C_CYAN_DIM COCKPIT_RGBA8(0x19, 0x81, 0x8B, 0xFF)

/*
 * Hierarchy is drawn by the theme, not decided by the panel (D18). A panel is
 * promoted by changing its PanelSlot's chrome, never by editing the panel.
 */
typedef enum {
    CHROME_HERO = 0, /* 28 px chamfer, bottom-left cut, corner brackets */
    CHROME_SATELLITE,/* 14 px chamfer, title band and rule */
    CHROME_STRIP,    /* 14 px chamfer, title centred inline, no rule */
    CHROME_LIST,     /* 14 px chamfer, title top-left inline, no rule */
} PanelChrome;

/* Geometry shared by the theme and by panels laying out their contents. */
#define PANEL_PAD 14.0f
/*
 * Content inset from a panel's own edge. Every panel uses the same value; it is
 * what makes CORE and STORAGE share a left content edge.
 */
#define PANEL_INSET 34.0f
#define PANEL_TITLE_PX 2.0f
#define PANEL_TITLE_TRACK 2.0f
#define PANEL_TITLE_Y 11.0f
#define PANEL_RULE_Y 36.0f

/* The strip is 44 px tall, so its title sits inline and content starts here. */
#define STRIP_BODY_X 110.0f

/* Font scale ladder (DESIGN.md §3). No size between these. */
#define PX_MICRO 1.5f
#define PX_LABEL 2.0f
#define PX_VALUE 3.0f
#define PX_HERO 4.0f
#define PX_PRIMARY 5.0f

/* One line of text at a given scale, in screen pixels. */
#define LINE_H(px) ((px) * (float)FONT_GLYPH_H)

/*
 * The vertical rhythm (D48).
 *
 * Every gap between two blocks is one of these steps. A gap off the scale is a
 * spacing bug the same way a colour off the palette is a colour bug: the first
 * version of the screen used 2-7 px gaps against 21-35 px type, so a label, its
 * value and the row below read as one dense lump with no grouping cue.
 *
 * 4 px is the base because it divides the 12 px column gap and the 18 px
 * margin, so the layout grid and the type rhythm are the same grid.
 */
#define SP_1 4.0f
#define SP_2 8.0f
#define SP_3 12.0f
#define SP_4 16.0f
#define SP_5 24.0f
#define SP_6 32.0f

/*
 * First line of panel content: one step below the title rule, so the first row
 * of CORE, POWER, STORAGE and SYSTEM share a horizontal line.
 */
#define PANEL_BODY_Y (PANEL_RULE_Y + SP_4)

/*
 * One readout row plus the gap to the next. The gap must be larger than the
 * offset between a label and its own value or the rows merge, so it is a full
 * step at value scale.
 */
#define READOUT_STRIDE (LINE_H(PX_VALUE) + SP_4)

/*
 * Fill, chamfered outline, title, and the title rule. Called by the app before
 * the panel's own draw, so the panel only ever draws data.
 */
void theme_panel(Rect r, const char *title, uint32_t accent,
                 PanelChrome chrome);

/*
 * LABEL (dim) with VALUE (white) right-aligned to x + w. `valid` false renders
 * the value dim, which is how absence reads.
 *
 * theme_readout is the px=2 / px=3 pair. theme_readout_px takes the two scales
 * so a panel can escalate one row without re-deriving the baseline rule, which
 * is what keeps "label bottom == value bottom" true everywhere (D27).
 */
void theme_readout(const char *label, const char *value, int valid, float x,
                   float y, float w);

void theme_readout_px(const char *label, const char *value, int valid, float x,
                      float y, float w, float label_px, float value_px);

/*
 * Format an integer reading and draw it as a readout in one step. `valid` false
 * renders the absence token and dims the value, so no panel spells either
 * itself.
 */
void theme_readout_int(const char *label, int value, int valid, float x,
                       float y, float w);

/* Ten discrete cells. Segments communicate capacity, a bar would imply
 * precision the reading does not have. `valid` false draws an empty, dimmed
 * track so "no reading" cannot be mistaken for "nothing used". */
void theme_meter(float x, float y, float w, float h, float fill, int valid,
                 uint32_t accent);

/*
 * Polyline trace of a History, newest at the right edge. Samples are scaled
 * into [scale_min, scale_max], which the caller owns because it is a statement
 * about the data, not about the look.
 */
void theme_sparkline(const History *hist, float x, float y, float w,
                     float height, uint32_t c, float scale_min,
                     float scale_max);

/* Yellow/black hazard band. Only on a real fault. */
void theme_hazard(float x, float y, float w, float h);

/* Interference sweep: proves the render loop is running. */
void theme_sweep(float x, float y, float w, float h, float t);

/* "12.4" from 124. Integer math only: newlib's float printf is not linked. */
void theme_fmt_tenths(char *buf, size_t n, int tenths);

/*
 * The token every absent reading renders. One definition, so "no value" cannot
 * drift into a zero or an empty cell.
 */
#define THEME_ABSENT "--"

/*
 * The decimal value, or THEME_ABSENT when !valid. Returns `valid`, so the result
 * can be passed straight to theme_readout(). Every panel uses this: absence must
 * look the same everywhere, and it must never look like zero.
 */
int theme_fmt_int(char *buf, size_t n, int value, int valid);

/* "00:14:22". Clamps negatives to zero. */
void theme_fmt_hms(char *buf, size_t n, int total_seconds);

#endif
