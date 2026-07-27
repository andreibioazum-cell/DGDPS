#ifndef MAIN_H
#define MAIN_H

#include <android/asset_manager.h>
#include <android_native_app_glue.h>
#include <sys/time.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ===== Graphics =====
typedef struct {
    uint32_t* pixels;
    int width;
    int height;
    int stride;
} RenderBuffer;

void graphics_clear(RenderBuffer* rb, uint32_t color);
void graphics_draw_rect(RenderBuffer* rb, int x, int y, int size, uint32_t color);
void graphics_draw_circle(RenderBuffer* rb, int cx, int cy, int r, uint32_t color);
void graphics_draw_ring(RenderBuffer* rb, int cx, int cy, int r, int thickness, uint32_t color);

// ===== Font =====
typedef struct Font Font;
int font_init(Font** out_font, const unsigned char* ttf_data, int data_size, float pixel_height);
void font_set_size(Font* f, float pixel_height);
void font_draw_text(Font* f, RenderBuffer* rb, int x, int y, const char* text, uint32_t color);
void font_free(Font* f);

// ===== UI =====
typedef struct {
    int centerX, centerY, radius;
    float dirX, dirY, touchOffX, touchOffY;
} Joystick;

void ui_draw_joystick(RenderBuffer* rb, Joystick* joy);
void ui_handle_joystick(Joystick* joy, float x, float y, int action);

// ===== Game =====
typedef struct {
    float x, y;
    float angle;
    float last_angle;
    float scale;
} Player;

typedef struct {
    Player player;
    Joystick joy;
    Font* font;
    int screen_w, screen_h;
    int fontSize;
    int frameCount;
    float fps;
    struct timeval lastTime;
} Game;

int game_init(Game* g, int w, int h, AAssetManager* mgr);
void game_update(Game* g, int w, int h);
void game_draw(Game* g, RenderBuffer* rb);
void game_free(Game* g);

#endif
