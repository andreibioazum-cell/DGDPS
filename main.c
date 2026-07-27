#include "main.h"
#include <arm_neon.h>

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

void graphics_draw_circle(RenderBuffer* rb, int cx, int cy, int r, uint32_t color) {
    int r2 = r * r;
    for (int y = -r; y <= r; y++) {
        int screen_y = cy + y;
        if (screen_y < 0 || screen_y >= rb->height) continue;
        uint32_t* line = rb->pixels + (screen_y * rb->stride);
        int y2 = y * y;
        for (int x = -r; x <= r; x++) {
            int screen_x = cx + x;
            if (screen_x < 0 || screen_x >= rb->width) continue;
            if (x * x + y2 <= r2) line[screen_x] = color;
        }
    }
}

void graphics_draw_ring(RenderBuffer* rb, int cx, int cy, int r, int thickness, uint32_t color) {
    int r_out2 = r * r;
    int r_in2 = (r - thickness) * (r - thickness);
    for (int y = -r; y <= r; y++) {
        int screen_y = cy + y;
        if (screen_y < 0 || screen_y >= rb->height) continue;
        uint32_t* line = rb->pixels + (screen_y * rb->stride);
        int y2 = y * y;
        for (int x = -r; x <= r; x++) {
            int screen_x = cx + x;
            if (screen_x < 0 || screen_x >= rb->width) continue;
            int dist2 = x * x + y2;
            if (dist2 <= r_out2 && dist2 >= r_in2) line[screen_x] = color;
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
// UI
// ============================================================

void ui_draw_joystick(RenderBuffer* rb, Joystick* joy) {
    graphics_draw_ring(rb, joy->centerX, joy->centerY, joy->radius, 4, 0xFF000000);
    int sx = joy->centerX + (int)joy->touchOffX;
    int sy = joy->centerY + (int)joy->touchOffY;
    graphics_draw_circle(rb, sx, sy, 35, 0xFF000000);
}

void ui_handle_joystick(Joystick* joy, float x, float y, int action) {
    if (action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_CANCEL) {
        joy->dirX = joy->dirY = joy->touchOffX = joy->touchOffY = 0.0f;
        return;
    }
    float dx = x - joy->centerX, dy = y - joy->centerY;
    float dist = hypotf(dx, dy);
    if (dist > joy->radius + 30.0f) return;
    if (dist < 15.0f) {
        joy->dirX = joy->dirY = joy->touchOffX = joy->touchOffY = 0.0f;
        return;
    }
    float clamped = (dist > joy->radius) ? joy->radius : dist;
    joy->dirX = dx / dist; joy->dirY = dy / dist;
    joy->touchOffX = joy->dirX * clamped;
    joy->touchOffY = joy->dirY * clamped;
}

// ============================================================
// GAME
// ============================================================
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define OFF 0.0f

int game_init(Game* g, int w, int h, AAssetManager* m) {
    memset(g, 0, sizeof(Game));
    g->screen_w=w; g->screen_h=h;
    g->player.x=w/2.0f; g->player.y=h/2.0f; g->player.scale=1.5f;
    g->joy.centerX=150; g->joy.centerY=h-150; g->joy.radius=80;
    
    g->fontSize = h/30; if(g->fontSize<12) g->fontSize=12; if(g->fontSize>48) g->fontSize=48;
    AAsset* fa = AAssetManager_open(m, "Roboto-Regular.ttf", AASSET_MODE_BUFFER);
    if (fa) {
        size_t sz = AAsset_getLength(fa);
        unsigned char* fd = (unsigned char*)malloc(sz);
        AAsset_read(fa, fd, sz); AAsset_close(fa);
        font_init(&g->font, fd, sz, (float)g->fontSize);
        free(fd);
    }
    gettimeofday(&g->lastTime, 0);
    return 1;
}

void game_update(Game* g, int w, int h) {
    g->screen_w=w; g->screen_h=h; g->joy.centerY=h-150;
    int ns = h/30; if(ns<12) ns=12; if(ns>48) ns=48;
    if (ns != g->fontSize && g->font) { g->fontSize=ns; font_set_size(g->font, (float)ns); }
    
    g->player.x += g->joy.dirX * 10.0f;
    g->player.y += g->joy.dirY * 10.0f;
    
    // Простой квадрат без обводки
    float cubeSize = 40.0f * g->player.scale;
    if (g->player.x < cubeSize/2) g->player.x = cubeSize/2;
    if (g->player.x > w - cubeSize/2) g->player.x = w - cubeSize/2;
    if (g->player.y < cubeSize/2) g->player.y = cubeSize/2;
    if (g->player.y > h - cubeSize/2) g->player.y = h - cubeSize/2;
    
    float len = hypotf(g->joy.dirX, g->joy.dirY);
    if (len > 0.001f) {
        g->player.angle = atan2f(g->joy.dirX, -g->joy.dirY) + OFF;
        g->player.last_angle = g->player.angle;
    } else g->player.angle = g->player.last_angle;
    
    g->frameCount++;
    struct timeval now;
    gettimeofday(&now, 0);
    float dt = (now.tv_sec - g->lastTime.tv_sec) + (now.tv_usec - g->lastTime.tv_usec)/1000000.0f;
    if (dt >= 1.0f) { g->fps = g->frameCount/dt; g->frameCount=0; g->lastTime=now; }
}

void game_draw(Game* g, RenderBuffer* rb) {
    graphics_clear(rb, 0xFFCCCCCC);
    
    // Простой квадрат без обводки, как в Geometry Dash
    int size = (int)(40 * g->player.scale);
    graphics_draw_rect(rb, (int)g->player.x, (int)g->player.y, size, 0xFF44BBFF);
    
    ui_draw_joystick(rb, &g->joy);
    
    char fps[32];
    snprintf(fps, sizeof(fps), "FPS: %.1f", g->fps);
    font_draw_text(g->font, rb, rb->width-120, 20, fps, 0xFFFFFFFF);
    
    // Название игры
    font_draw_text(g->font, rb, rb->width/2 - 80, 50, "GEOMETRY DASH", 0xFF000000);
}

void game_free(Game* g) {
    if (g->font) { font_free(g->font); g->font = 0; }
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
    } else if (cmd == APP_CMD_TERM_WINDOW)
        game_free(&e->game);
}

static int32_t handle_input(struct android_app* app, AInputEvent* event) {
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        struct engine* e = (struct engine*)app->userData;
        int action = AMotionEvent_getAction(event);
        float x = AMotionEvent_getX(event, 0);
        float y = AMotionEvent_getY(event, 0);
        ui_handle_joystick(&e->game.joy, x, y, action);
        return 1;
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
            if (app->destroyRequested) { game_free(&e.game); return; }
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
