#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>

typedef struct {
    uint32_t* pixels;
    int width, height, stride;
} RenderBuffer;

void graphics_clear(RenderBuffer* rb, uint32_t color);
void graphics_draw_rect(RenderBuffer* rb, int x, int y, int w, int h, uint32_t color);
void graphics_draw_texture_rotated(RenderBuffer* rb, int x, int y,
                                   uint32_t* tex, int tw, int th,
                                   float scale, float angle);

#endif
