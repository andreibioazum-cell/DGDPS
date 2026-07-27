#include "main.h"
#include <arm_neon.h>
#include <android/input.h>

// ============================================================
// GRAPHICS
// ============================================================

void graphics_clear(RenderBuffer* rb, uint32_t color) {
    uint32x4_t v_color = vdupq_n_u32(color);
    int total_pixels = rb->stride * rb->height;
    int i = 0;
    for (; i <= total_pixels - 4; i += 4) {
        vst1q_u32(&rb->pixels[i], v_color);
    }
    for (; i < total_pixels; i++) rb->pixels[i] = color;
}

void graphics_draw_rect(RenderBuffer* rb, int x, int y, int size, uint32_t color) {
    int x1 = x - size/2, x2 = x + size/2;
    int y1 = y - size/2, y2 = y + size/2;
    if (x1 < 0) x1 = 0; if (x2 > rb->width) x2 = rb->width;
    if (y1 < 0) y1 = 0; if (y2 > rb->height) y2 = rb->height;

    for (int i = y1; i < y2; i++) {
        uint32_t* line = rb->pixels + (i * rb->stride);
        for (int j = x1; j < x2; j++) line[j] = color;
    }
}

void graphics_draw_triangle(RenderBuffer* rb, int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color) {
    // Простая отрисовка треугольника через заливку построчно
    int minX = x1, maxX = x1;
    if (x2 < minX) minX = x2; if (x3 < minX) minX = x3;
    if (x2 > maxX) maxX = x2; if (x3 > maxX) maxX = x3;
    int minY = y1, maxY = y1;
    if (y2 < minY) minY = y2; if (y3 < minY) minY = y3;
    if (y2 > maxY) maxY = y2; if (y3 > maxY) maxY = y3;
    
    if (minX < 0) minX = 0; if (maxX > rb->width) maxX = rb->width;
    if (minY < 0) minY = 0; if (maxY > rb->height) maxY = rb->height;
    
    // Для каждой строки проверяем принадлежность треугольнику
    for (int y = minY; y < maxY; y++) {
        uint32_t* line = rb->pixels + (y * rb->stride);
        for (int x = minX; x < maxX; x++) {
            // Вычисляем барицентрические координаты
            float d1 = (x - x2) * (y1 - y2) - (x1 - x2) * (y - y2);
            float d2 = (x - x3) * (y2 - y3) - (x2 - x3) * (y - y3);
            float d3 = (x - x1) * (y3 - y1) - (x3 - x1) * (y - y1);
            int hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
            int hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
            if (!(hasNeg && hasPos)) {
                line[x] = color;
            }
        }
    }
}

// ============================================================
// FONT (STB Truetype)
// ============================================================
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

struct Font { stbtt_fontinfo info; float scale; unsigned char* buf; };

int font_init(Font** f, const unsigned char* data, int sz, float height) {
    Font* n = (Font*)calloc(1, sizeof(Font));
    if (!n) return 0;
    n->buf = (unsigned char*)malloc(sz);
    if (!n->buf) { free(n); return 0; }
    memcpy(n->buf, data, sz);
    if (!stbtt_InitFont(&n->info, n->buf, 0)) { free(n->buf); free(n); return 0; }
    n->scale = stbtt_ScaleForPixelHeight(&n->info, height);
    *f = n;
    return 1;
}

void font_set_size(Font* f, float height) { if (f) f->scale = stbtt_ScaleForPixelHeight(&f->info, height); }

void font_draw_text(Font* f, RenderBuffer* rb, int x, int y, const char* txt, uint32_t col) {
    if (!f || !rb || !txt) return;
    int cx = x;
    for (const char* p = txt; *p; ++p) {
        int gi = stbtt_FindGlyphIndex(&f->info, *p);
        if (!gi) { cx += 8; continue; }
        int x0,y0,x1,y1;
        stbtt_GetGlyphBitmapBox(&f->info, gi, f->scale, f->scale, &x0,&y0,&x1,&y1);
        int w = x1-x0, h = y1-y0;
        if (w>0 && h>0) {
            unsigned char* bm = (unsigned char*)malloc(w*h);
            if (bm) {
                stbtt_MakeGlyphBitmap(&f->info, bm, w, h, w, f->scale, f->scale, gi);
                for (int row=0; row<h; ++row) {
                    int sy = y + y0 + row;
                    if (sy<0 || sy>=rb->height) continue;
                    uint32_t* line = rb->pixels + sy * rb->stride;
                    for (int col=0; col<w; ++col) {
                        unsigned char a = bm[row*w + col];
                        if (a) {
                            int sx = cx + x0 + col;
                            if (sx>=0 && sx<rb->width) line[sx] = col;
                        }
                    }
                }
                free(bm);
            }
        }
        int adv; stbtt_GetGlyphHMetrics(&f->info, gi, &adv, 0);
        cx += (int)(adv * f->scale);
    }
}

void font_free(Font* f) { if (f) { free(f->buf); free(f); } }

// ============================================================
// GAME
// ============================================================
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define GRAVITY 0.8f
#define JUMP_POWER -14.0f
#define MOVE_SPEED 6.0f
#define PLAYER_SIZE 30

void spawn_spikes(Game* g) {
    g->spikeCount = 0;
    // Спавним шипы на платформе через равные промежутки
    for (int i = 0; i < 20; i++) {
        g->spikes[i].x = 150 + i * 100 + (rand() % 60);
        g->spikes[i].y = g->groundY - 15;
        g->spikes[i].active = 1;
        g->spikeCount++;
    }
}

int game_init(Game* g, int w, int h, AAssetManager* m) {
    memset(g, 0, sizeof(Game));
    g->screen_w = w;
    g->screen_h = h;
    g->groundY = h - 100;
    g->gravity = GRAVITY;
    g->jumpPower = JUMP_POWER;
    g->moveSpeed = MOVE_SPEED;
    g->score = 0;
    g->gameOver = 0;
    g->restartTimer = 0;
    
    // Инициализация игрока
    g->player.x = 150;
    g->player.y = g->groundY - PLAYER_SIZE/2;
    g->player.vy = 0;
    g->player.size = PLAYER_SIZE;
    g->player.grounded = 0;
    g->player.jumpCount = 0;
    
    // Спавн шипов
    spawn_spikes(g);
    
    g->fontSize = h/30; 
    if(g->fontSize<12) g->fontSize=12; 
    if(g->fontSize>48) g->fontSize=48;
    
    AAsset* fa = AAssetManager_open(m, "Roboto-Regular.ttf", AASSET_MODE_BUFFER);
    if (fa) {
        size_t sz = AAsset_getLength(fa);
        unsigned char* fd = (unsigned char*)malloc(sz);
        AAsset_read(fa, fd, sz); 
        AAsset_close(fa);
        font_init(&g->font, fd, sz, (float)g->fontSize);
        free(fd);
    }
    
    gettimeofday(&g->lastTime, 0);
    return 1;
}

void game_restart(Game* g) {
    g->player.x = 150;
    g->player.y = g->groundY - PLAYER_SIZE/2;
    g->player.vy = 0;
    g->player.grounded = 0;
    g->gameOver = 0;
    g->score = 0;
    g->restartTimer = 0;
    spawn_spikes(g);
}

void game_jump(Game* g) {
    if (g->gameOver) {
        game_restart(g);
        return;
    }
    if (g->player.grounded) {
        g->player.vy = g->jumpPower;
        g->player.grounded = 0;
        g->player.jumpCount++;
    }
}

void game_update(Game* g, int w, int h) {
    g->screen_w = w;
    g->screen_h = h;
    g->groundY = h - 100;
    
    // Обновление размера шрифта
    int ns = h/30; 
    if(ns<12) ns=12; 
    if(ns>48) ns=48;
    if (ns != g->fontSize && g->font) { 
        g->fontSize=ns; 
        font_set_size(g->font, (float)ns); 
    }
    
    if (g->gameOver) {
        g->restartTimer += 0.016f;
        return;
    }
    
    // Физика игрока
    g->player.vy += g->gravity;
    g->player.y += g->player.vy;
    
    // Проверка пола
    if (g->player.y + g->player.size/2 >= g->groundY) {
        g->player.y = g->groundY - g->player.size/2;
        g->player.vy = 0;
        g->player.grounded = 1;
    } else {
        g->player.grounded = 0;
    }
    
    // Движение вперёд
    g->player.x += g->moveSpeed;
    
    // Проверка столкновений с шипами
    int spikeSize = 20;
    for (int i = 0; i < g->spikeCount; i++) {
        if (!g->spikes[i].active) continue;
        
        float dx = g->player.x - g->spikes[i].x;
        float dy = g->player.y - g->spikes[i].y;
        float dist = sqrtf(dx*dx + dy*dy);
        
        if (dist < (g->player.size/2 + spikeSize/2)) {
            g->gameOver = 1;
            g->restartTimer = 0;
            return;
        }
    }
    
    // Обновление счёта
    g->score++;
    
    // Удаление старых шипов и создание новых
    for (int i = 0; i < g->spikeCount; i++) {
        if (g->spikes[i].x < g->player.x - 200) {
            g->spikes[i].x = g->player.x + 300 + (rand() % 200);
            g->spikes[i].y = g->groundY - 15;
            g->spikes[i].active = 1;
        }
    }
    
    // FPS
    g->frameCount++;
    struct timeval now;
    gettimeofday(&now, 0);
    float dt = (now.tv_sec - g->lastTime.tv_sec) + (now.tv_usec - g->lastTime.tv_usec)/1000000.0f;
    if (dt >= 1.0f) { 
        g->fps = g->frameCount/dt; 
        g->frameCount=0; 
        g->lastTime=now; 
    }
}

void draw_spike(RenderBuffer* rb, float x, float y, float size, uint32_t color) {
    // Рисуем треугольный шип
    int x1 = (int)(x - size/2);
    int y1 = (int)(y + size/2);
    int x2 = (int)(x + size/2);
    int y2 = (int)(y + size/2);
    int x3 = (int)(x);
    int y3 = (int)(y - size/2);
    graphics_draw_triangle(rb, x1, y1, x2, y2, x3, y3, color);
}

void game_draw(Game* g, RenderBuffer* rb) {
    // Фон
    graphics_clear(rb, 0xFF1a1a2e);
    
    // Пол
    graphics_draw_rect(rb, rb->width/2, (int)g->groundY + 5, rb->width, 0xFF16213e);
    graphics_draw_rect(rb, rb->width/2, (int)g->groundY, rb->width, 0xFF0f3460);
    
    // Шипы
    for (int i = 0; i < g->spikeCount; i++) {
        if (g->spikes[i].active) {
            draw_spike(rb, g->spikes[i].x, g->spikes[i].y, 20, 0xFFe94560);
        }
    }
    
    // Игрок (квадрат как в Geometry Dash)
    int size = (int)g->player.size;
    uint32_t playerColor = g->gameOver ? 0xFFFF0000 : 0xFF4CAF50;
    graphics_draw_rect(rb, (int)g->player.x, (int)g->player.y, size, playerColor);
    
    // Глаз игрока
    if (!g->gameOver) {
        int eyeSize = size/6;
        graphics_draw_rect(rb, (int)(g->player.x + size/4), (int)(g->player.y - size/6), eyeSize, 0xFFFFFFFF);
        graphics_draw_rect(rb, (int)(g->player.x + size/4 + eyeSize/2), (int)(g->player.y - size/6 - eyeSize/2), eyeSize/2, 0xFF000000);
    }
    
    // Счёт
    char scoreText[32];
    snprintf(scoreText, sizeof(scoreText), "SCORE: %d", g->score);
    font_draw_text(g->font, rb, 20, 20, scoreText, 0xFFFFFFFF);
    
    // FPS
    char fps[32];
    snprintf(fps, sizeof(fps), "FPS: %.1f", g->fps);
    font_draw_text(g->font, rb, rb->width-120, 20, fps, 0xFFFFFFFF);
    
    // Название игры
    font_draw_text(g->font, rb, rb->width/2 - 80, 50, "GEOMETRY DASH", 0xFFe94560);
    
    // Game Over
    if (g->gameOver) {
        font_draw_text(g->font, rb, rb->width/2 - 80, rb->height/2 - 30, "GAME OVER", 0xFFFF0000);
        font_draw_text(g->font, rb, rb->width/2 - 100, rb->height/2 + 20, "TAP TO RESTART", 0xFFFFFFFF);
    }
    
    // Инструкция
    if (g->player.jumpCount == 0 && !g->gameOver) {
        font_draw_text(g->font, rb, rb->width/2 - 120, rb->height/2 + 50, "TAP TO JUMP", 0xFFFFFFFF);
    }
}

void game_free(Game* g) {
    if (g->font) { 
        font_free(g->font); 
        g->font = 0; 
    }
}

// ============================================================
// MAIN
// ============================================================

struct engine { struct android_app* app; Game game; };

static void handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* e = (struct engine*)app->userData;
    if (cmd == APP_CMD_INIT_WINDOW) {
        int w = ANativeWindow_getWidth(app->window);
        int h = ANativeWindow_getHeight(app->window);
        ANativeWindow_setBuffersGeometry(app->window, 0, 0, WINDOW_FORMAT_RGBA_8888);
        game_init(&e->game, w, h, app->activity->assetManager);
    } else if (cmd == APP_CMD_TERM_WINDOW) {
        game_free(&e->game);
    }
}

static int32_t handle_input(struct android_app* app, AInputEvent* event) {
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        struct engine* e = (struct engine*)app->userData;
        int action = AMotionEvent_getAction(event);
        if (action == AMOTION_EVENT_ACTION_DOWN) {
            game_jump(&e->game);
            return 1;
        }
    }
    return 0;
}

void android_main(struct android_app* app) {
    struct engine e = {0};
    app->userData = &e;
    app->onAppCmd = handle_cmd;
    app->onInputEvent = handle_input;
    
    while (1) {
        int ident;
        struct android_poll_source* source;
        while ((ident = ALooper_pollOnce(0, 0, 0, (void**)&source)) >= 0) {
            if (source) source->process(app, source);
            if (app->destroyRequested) { 
                game_free(&e.game); 
                return; 
            }
        }
        if (app->window) {
            int w = ANativeWindow_getWidth(app->window);
            int h = ANativeWindow_getHeight(app->window);
            game_update(&e.game, w, h);
            ANativeWindow_Buffer winBuf;
            if (ANativeWindow_lock(app->window, &winBuf, 0) == 0) {
                RenderBuffer rb = { (uint32_t*)winBuf.bits, winBuf.width, winBuf.height, winBuf.stride };
                game_draw(&e.game, &rb);
                ANativeWindow_unlockAndPost(app->window);
            }
        }
    }
}
