#include "main.h"
#include <arm_neon.h>
#include <android/input.h>
#include <android/log.h>

#define LOG_TAG "GeometryDash"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

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

void graphics_draw_rect(RenderBuffer* rb, int x, int y, int w, int h, uint32_t color) {
    int x1 = x - w/2, x2 = x + w/2;
    int y1 = y - h/2, y2 = y + h/2;
    if (x1 < 0) x1 = 0; if (x2 > rb->width) x2 = rb->width;
    if (y1 < 0) y1 = 0; if (y2 > rb->height) y2 = rb->height;

    for (int i = y1; i < y2; i++) {
        uint32_t* line = rb->pixels + (i * rb->stride);
        for (int j = x1; j < x2; j++) line[j] = color;
    }
}

void graphics_draw_texture(RenderBuffer* rb, int x, int y, uint32_t* tex, int tw, int th, float scale) {
    if (!tex || tw <= 0 || th <= 0) return;
    int sw = (int)(tw * scale);
    int sh = (int)(th * scale);
    if (sw <= 0 || sh <= 0) return;
    
    int x1 = x - sw/2, y1 = y - sh/2;
    int x2 = x + sw/2, y2 = y + sh/2;
    if (x1 < 0) x1 = 0; if (x2 > rb->width) x2 = rb->width;
    if (y1 < 0) y1 = 0; if (y2 > rb->height) y2 = rb->height;
    if (x1 >= x2 || y1 >= y2) return;

    for (int py = y1; py < y2; py++) {
        uint32_t* out = rb->pixels + py * rb->stride;
        float src_y = (float)(py - y1) / sh * th;
        int iy = (int)(src_y);
        if (iy < 0) iy = 0; if (iy >= th) iy = th-1;
        for (int px = x1; px < x2; px++) {
            float src_x = (float)(px - x1) / sw * tw;
            int ix = (int)(src_x);
            if (ix < 0) ix = 0; if (ix >= tw) ix = tw-1;
            uint32_t pix = tex[iy * tw + ix];
            if ((pix & 0xFF000000) != 0) out[px] = pix;
        }
    }
}

// ============================================================
// TEXTURE LOADER (stb_image)
// ============================================================
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

Texture* texture_load(AAssetManager* mgr, const char* path) {
    LOGI("Loading texture: %s", path);
    AAsset* asset = AAssetManager_open(mgr, path, AASSET_MODE_BUFFER);
    if (!asset) {
        LOGI("Failed to open asset: %s", path);
        return NULL;
    }
    size_t size = AAsset_getLength(asset);
    unsigned char* filedata = (unsigned char*)malloc(size);
    AAsset_read(asset, filedata, size);
    AAsset_close(asset);
    
    int w, h, n;
    unsigned char* img = stbi_load_from_memory(filedata, size, &w, &h, &n, 4);
    free(filedata);
    if (!img) {
        LOGI("stbi_load_from_memory failed for %s", path);
        return NULL;
    }
    
    Texture* tex = (Texture*)malloc(sizeof(Texture));
    tex->width = w;
    tex->height = h;
    tex->data = (uint32_t*)malloc(w * h * 4);
    for (int i = 0; i < w * h; i++) {
        uint8_t r = img[i*4], g = img[i*4+1], b = img[i*4+2], a = img[i*4+3];
        tex->data[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
    stbi_image_free(img);
    LOGI("Loaded texture %s: %dx%d", path, w, h);
    return tex;
}

void texture_free(Texture* tex) {
    if (tex) {
        if (tex->data) free(tex->data);
        free(tex);
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

#define GRAVITY 0.6f
#define JUMP_POWER -12.0f
#define GAME_SPEED 7.0f
#define PLAYER_SCALE 0.35f
#define SPIKE_SCALE 0.6f

void spawn_spike(Game* g) {
    if (g->spikeCount < 50) {
        g->spikes[g->spikeCount].x = g->screen_w + 50 + (rand() % 300);
        g->spikes[g->spikeCount].y = g->groundY;
        g->spikes[g->spikeCount].active = 1;
        g->spikeCount++;
        LOGI("Spike spawned at x=%.1f, count=%d", g->spikes[g->spikeCount-1].x, g->spikeCount);
    }
}

int game_init(Game* g, int w, int h, AAssetManager* m) {
    memset(g, 0, sizeof(Game));
    g->screen_w = w;
    g->screen_h = h;
    g->groundY = h - 120;
    g->gravity = GRAVITY;
    g->jumpPower = JUMP_POWER;
    g->speed = GAME_SPEED;
    g->score = 0;
    g->gameOver = 0;
    g->restartTimer = 0;
    g->spawnTimer = 0;
    g->spikeCount = 0;
    
    g->playerTex = texture_load(m, "cube1.png");
    g->spikeTex = texture_load(m, "spike.png");
    LOGI("Player tex: %p, Spike tex: %p", g->playerTex, g->spikeTex);
    
    g->player.x = 200;
    g->player.y = g->groundY - (150 * PLAYER_SCALE)/2;
    g->player.vy = 0;
    g->player.size = 150 * PLAYER_SCALE;
    g->player.grounded = 1;
    g->player.jumpCount = 0;
    
    for (int i = 0; i < 5; i++) {
        spawn_spike(g);
    }
    
    g->fontSize = h/25; 
    if(g->fontSize<16) g->fontSize=16; 
    if(g->fontSize>64) g->fontSize=64;
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
    g->player.x = 200;
    g->player.y = g->groundY - (150 * PLAYER_SCALE)/2;
    g->player.vy = 0;
    g->player.grounded = 1;
    g->gameOver = 0;
    g->score = 0;
    g->restartTimer = 0;
    g->spikeCount = 0;
    g->spawnTimer = 0;
    for (int i = 0; i < 5; i++) {
        spawn_spike(g);
    }
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
    g->groundY = h - 120;
    
    int ns = h/25; 
    if(ns<16) ns=16; 
    if(ns>64) ns=64;
    if (ns != g->fontSize && g->font) { 
        g->fontSize=ns; 
        font_set_size(g->font, (float)ns); 
    }
    
    if (g->gameOver) {
        g->restartTimer += 0.016f;
        return;
    }
    
    g->player.vy += g->gravity;
    g->player.y += g->player.vy;
    
    if (g->player.y + g->player.size/2 >= g->groundY) {
        g->player.y = g->groundY - g->player.size/2;
        g->player.vy = 0;
        g->player.grounded = 1;
    }
    if (g->player.y - g->player.size/2 < 0) {
        g->player.y = g->player.size/2;
        g->player.vy = 0;
    }
    
    float spikeSize = 49 * SPIKE_SCALE;
    for (int i = 0; i < g->spikeCount; i++) {
        if (!g->spikes[i].active) continue;
        g->spikes[i].x -= g->speed;
        
        float dx = g->player.x - g->spikes[i].x;
        float dy = g->player.y - g->spikes[i].y;
        float dist = sqrtf(dx*dx + dy*dy);
        if (dist < (g->player.size/2 + spikeSize/2)) {
            g->gameOver = 1;
            g->restartTimer = 0;
            LOGI("Game Over by spike %d", i);
            return;
        }
        
        if (g->spikes[i].x < -50) {
            g->spikes[i].active = 0;
        }
    }
    
    g->spawnTimer++;
    if (g->spawnTimer > 60) {
        g->spawnTimer = 0;
        int activeCount = 0;
        for (int i = 0; i < g->spikeCount; i++) if (g->spikes[i].active) activeCount++;
        if (activeCount < 8) spawn_spike(g);
    }
    
    int write = 0;
    for (int i = 0; i < g->spikeCount; i++) {
        if (g->spikes[i].active) {
            g->spikes[write++] = g->spikes[i];
        }
    }
    g->spikeCount = write;
    
    g->score++;
    
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

void game_draw(Game* g, RenderBuffer* rb) {
    graphics_clear(rb, 0xFF16213e);
    
    graphics_draw_rect(rb, rb->width/2, (int)g->groundY + 10, rb->width, 8, 0xFF0a0a1a);
    graphics_draw_rect(rb, rb->width/2, (int)g->groundY, rb->width, 8, 0xFF1a1a2e);
    
    if (g->spikeTex && g->spikeTex->data) {
        for (int i = 0; i < g->spikeCount; i++) {
            if (g->spikes[i].active) {
                int drawX = (int)g->spikes[i].x;
                int drawY = (int)(g->spikes[i].y - (49 * SPIKE_SCALE) / 2);
                if (drawX > -50 && drawX < g->screen_w + 50) {
                    graphics_draw_texture(rb, drawX, drawY,
                                          g->spikeTex->data,
                                          g->spikeTex->width,
                                          g->spikeTex->height,
                                          SPIKE_SCALE);
                }
            }
        }
    } else {
        for (int i = 0; i < g->spikeCount; i++) {
            if (g->spikes[i].active) {
                int x = (int)g->spikes[i].x;
                int y = (int)g->spikes[i].y;
                graphics_draw_rect(rb, x, y - 10, 20, 20, 0xFFFF0000);
            }
        }
    }
    
    if (g->playerTex && g->playerTex->data) {
        graphics_draw_texture(rb, 
                              (int)g->player.x, 
                              (int)g->player.y, 
                              g->playerTex->data, 
                              g->playerTex->width, 
                              g->playerTex->height, 
                              PLAYER_SCALE);
    } else {
        graphics_draw_rect(rb, (int)g->player.x, (int)g->player.y, 40, 40, 0xFF4CAF50);
    }
    
    char scoreText[32];
    snprintf(scoreText, sizeof(scoreText), "%d", g->score);
    font_draw_text(g->font, rb, rb->width/2 - 50, 60, scoreText, 0xFFFFFFFF);
    
    char fps[32];
    snprintf(fps, sizeof(fps), "FPS: %.0f", g->fps);
    font_draw_text(g->font, rb, rb->width-140, 30, fps, 0x88FFFFFF);
    
    font_draw_text(g->font, rb, 20, 30, "GEOMETRY DASH", 0xFFe94560);
    
    if (g->gameOver) {
        font_draw_text(g->font, rb, rb->width/2 - 120, rb->height/2 - 40, "GAME OVER", 0xFFFF0000);
        font_draw_text(g->font, rb, rb->width/2 - 150, rb->height/2 + 30, "TAP TO RESTART", 0xFFFFFFFF);
    }
    
    if (g->player.jumpCount == 0 && !g->gameOver) {
        font_draw_text(g->font, rb, rb->width/2 - 100, rb->height/2 + 80, "TAP TO JUMP", 0x88FFFFFF);
    }
}

void game_free(Game* g) {
    if (g->font) font_free(g->font);
    if (g->playerTex) texture_free(g->playerTex);
    if (g->spikeTex) texture_free(g->spikeTex);
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
