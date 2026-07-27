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
void graphics_draw_rect(RenderBuffer* rb, int x, int y, int w, int h, uint32_t color);
void graphics_draw_texture(RenderBuffer* rb, int x, int y, uint32_t* tex, int tw, int th, float scale);

// ===== Font =====
typedef struct Font Font;
int font_init(Font** out_font, const unsigned char* ttf_data, int data_size, float pixel_height);
void font_set_size(Font* f, float pixel_height);
void font_draw_text(Font* f, RenderBuffer* rb, int x, int y, const char* text, uint32_t color);
void font_free(Font* f);

// ===== Texture =====
typedef struct {
    uint32_t* data;
    int width;
    int height;
} Texture;

Texture* texture_load(AAssetManager* mgr, const char* path);
void texture_free(Texture* tex);

// ===== Spike =====
typedef struct {
    float x, y;
    int active;
} Spike;

// ===== Game =====
typedef struct {
    float x, y;
    float vy;
    float size;
    int grounded;
    int jumpCount;
} Player;

typedef struct {
    Player player;
    Font* font;
    Texture* playerTex;
    Texture* spikeTex;
    int screen_w, screen_h;
    int fontSize;
    int frameCount;
    float fps;
    struct timeval lastTime;
    Spike spikes[50];
    int spikeCount;
    int score;
    float groundY;
    float gravity;
    float jumpPower;
    float speed;
    int gameOver;
    float restartTimer;
    int spawnTimer;
} Game;

int game_init(Game* g, int w, int h, AAssetManager* mgr);
void game_update(Game* g, int w, int h);
void game_draw(Game* g, RenderBuffer* rb);
void game_free(Game* g);
void game_jump(Game* g);
void game_restart(Game* g);

#endif
