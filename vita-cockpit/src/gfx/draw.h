#ifndef COCKPIT_GFX_DRAW_H
#define COCKPIT_GFX_DRAW_H

#include <stdint.h>

/* Axis-aligned rectangle in screen pixels. */
typedef struct {
    float x, y, w, h;
} Rect;

/* Scale a colour's RGB by f in 0..1, preserving alpha. */
uint32_t draw_dim(uint32_t c, float f);

void draw_rect(float x, float y, float w, float h, uint32_t c);

/*
 * Fill many rects of one colour. Each one is its own vita2d_draw_rectangle:
 * batching through vita2d_draw_array was tried and produced nothing on
 * hardware (D31), so this is a convenience loop, not an optimisation.
 */
void draw_rects(const Rect *rects, int count, uint32_t c);

void draw_line(float x0, float y0, float x1, float y1, uint32_t c);
void draw_tri(float x0, float y0, float x1, float y1, float x2, float y2,
              uint32_t c);

/*
 * Additive blending. In this mode a colour adds to what is already there
 * instead of replacing it, which is how every glow in the app is built.
 *
 * Without it, a dimmed colour is opaque and *overwrites*: a 12 %-cyan scanline
 * would punch a dark stripe through white text rather than brighten it.
 *
 * This is the whole glow mechanism (D3). A caller that wants a halo draws its
 * own shape more than once at falling intensity — the wordmark does exactly
 * that with four offset passes — rather than calling a fixed "glow" helper.
 */
void draw_set_additive(int on);

/*
 * Rectangular clipping, in screen pixels. The scrolled panel area uses it so a
 * panel cannot bleed over the header or the status strip.
 */
void draw_set_clip(int x, int y, int w, int h);
void draw_clear_clip(void);

#endif
