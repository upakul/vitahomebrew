#ifndef PREVIEW_VITA2D_H
#define PREVIEW_VITA2D_H

#include <stddef.h>

#include "psp2/gxm.h"

/*
 * Host stand-in for the four vita2d entry points gfx/draw.c calls. The preview
 * rasteriser implements these into a plain RGBA buffer.
 */

typedef struct vita2d_color_vertex {
    float x;
    float y;
    float z;
    unsigned int color;
} vita2d_color_vertex;

void vita2d_draw_rectangle(float x, float y, float w, float h,
                           unsigned int color);
void vita2d_draw_line(float x0, float y0, float x1, float y1,
                      unsigned int color);
void vita2d_draw_array(SceGxmPrimitiveType mode,
                       const vita2d_color_vertex *vertices, size_t count);
void vita2d_set_blend_mode_add(int enable);

/*
 * GPU-mapped scratch for vertex data. On device this is the only memory
 * vita2d_draw_array may be given; here it is a bump allocator with the same
 * size as the app's pool, so the preview can measure real usage.
 */
void *vita2d_pool_memalign(unsigned int size, unsigned int alignment);

/* Clipping, used by the scrolling panel viewport. */
void vita2d_enable_clipping(void);
void vita2d_disable_clipping(void);
void vita2d_set_clip_rectangle(int x_min, int y_min, int x_max, int y_max);

#endif
