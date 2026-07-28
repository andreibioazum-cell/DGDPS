#include "menu.h"

void menu_init(AAssetManager* assets, int w, int h) {}
void menu_update(int w, int h) {}
void menu_draw(RenderBuffer* rb) {
    graphics_clear(rb, 0xFF1a1a2e);
    int cx=rb->width/2, cy=rb->height/2;
    for(int i=-6;i<=6;i++) graphics_draw_rect(rb, cx+i*14, cy, 6,6, 0xFFe94560);
}
void menu_free(void) {}
