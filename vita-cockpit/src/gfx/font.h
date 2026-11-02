#ifndef COCKPIT_GFX_FONT_H
#define COCKPIT_GFX_FONT_H

#include <stdint.h>

#include "gfx/draw.h"

/*
 * A hand-built 5x7 bitmap face.
 *
 * VitaSDK 2026.08 ships no font file, and a bundled TTF would add a licence
 * and ~200 KB for no legibility gain at HUD sizes. Block glyphs also render as
 * crisp rectangles under additive glow, which is the look we want.
 *
 * px is the size of one font pixel. A glyph is 5*px wide, 7*px tall, and
 * advances 6*px. Fractional px is allowed but integers stay sharpest.
 */

#define FONT_GLYPH_W 5
#define FONT_GLYPH_H 7
#define FONT_ADVANCE 6

/* Most horizontal runs a single glyph can produce: 7 rows, and no row of a 5px
 * face has more than 3 runs. */
#define FONT_MAX_RUNS 24

void font_text(const char *s, float x, float y, float px, uint32_t c);

/* Adds `extra` pixels of tracking between glyphs. */
void font_text_tracked(const char *s, float x, float y, float px, uint32_t c,
                       float extra);

float font_width(const char *s, float px);

/*
 * Enumerate the rects a string would draw, without drawing them. Returns the
 * number written, clamped to `max`.
 *
 * A string that never changes -- the wordmark -- can be baked once into a rect
 * list and replayed with draw_rects() instead of walking the glyph table on
 * every frame.
 */
int font_rects(const char *s, float x, float y, float px, Rect *out, int max);

/* Draw so the string's right edge lands on x_right. */
void font_text_right(const char *s, float x_right, float y, float px,
                     uint32_t c);

#endif
