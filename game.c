#include "game.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

typedef struct { uint32_t* data; int w,h; } Texture;
typedef struct { stbtt_fontinfo info; float scale; unsigned char* buf; } Font;

static Texture* load_texture(AAssetManager* mgr, const char* path) {
    AAsset* a=AAssetManager_open(mgr,path,AASSET_MODE_BUFFER); if(!a) return NULL;
    size_t sz=AAsset_getLength(a); unsigned char* d=malloc(sz); AAsset_read(a,d,sz); AAsset_close(a);
    int w,h,n; unsigned char* img=stbi_load_from_memory(d,sz,&w,&h,&n,4); free(d); if(!img) return NULL;
    Texture* t=malloc(sizeof(Texture)); t->w=w; t->h=h; t->data=malloc(w*h*4);
    for(int i=0;i<w*h;i++){ uint8_t r=img[i*4],g=img[i*4+1],b=img[i*4+2],a=img[i*4+3]; t->data[i]=(a<<24)|(r<<16)|(g<<8)|b; }
    stbi_image_free(img); return t;
}
static void free_texture(Texture* t){ if(t){ free(t->data); free(t); } }

static Font* load_font(AAssetManager* mgr, const char* path, float height) {
    AAsset* a=AAssetManager_open(mgr,path,AASSET_MODE_BUFFER); if(!a) return NULL;
    size_t sz=AAsset_getLength(a); unsigned char* d=malloc(sz); AAsset_read(a,d,sz); AAsset_close(a);
    Font* f=malloc(sizeof(Font)); f->buf=d;
    if(!stbtt_InitFont(&f->info, f->buf, 0)){ free(d); free(f); return NULL; }
    f->scale=stbtt_ScaleForPixelHeight(&f->info, height); return f;
}
static void free_font(Font* f){ if(f){ free(f->buf); free(f); } }

static void draw_text(Font* f, RenderBuffer* rb, int x, int y, const char* text, uint32_t color) {
    if(!f || !rb || !text) return;
    int cx=x;
    for(const char* p=text; *p; ++p){
        int gi=stbtt_FindGlyphIndex(&f->info, *p); if(!gi){ cx+=8; continue; }
        int x0,y0,x1,y1; stbtt_GetGlyphBitmapBox(&f->info, gi, f->scale, f->scale, &x0,&y0,&x1,&y1);
        int w=x1-x0, h=y1-y0;
        if(w>0 && h>0){
            unsigned char* bm=malloc(w*h);
            if(bm){
                stbtt_MakeGlyphBitmap(&f->info, bm, w, h, w, f->scale, f->scale, gi);
                for(int row=0; row<h; row++){
                    int sy=y+y0+row; if(sy<0||sy>=rb->height) continue;
                    uint32_t* line=rb->pixels+sy*rb->stride;
                    for(int col=0; col<w; col++){
                        unsigned char a=bm[row*w+col];
                        if(a){ int sx=cx+x0+col; if(sx>=0&&sx<rb->width) line[sx]=color; }
                    }
                }
                free(bm);
            }
        }
        int adv; stbtt_GetGlyphHMetrics(&f->info, gi, &adv,0);
        cx += (int)(adv * f->scale);
    }
}

static struct {
    Texture *playerTex, *spikeTex;
    Font* font;
    float px,py,vy,size,angle,targetAngle,jumpTime,jumpDur;
    int grounded,jumpCount,gameOver,score;
    float groundY,gravity,jumpPower,speed;
    struct { float x,y; int active; } spikes[50];
    int spikeCount,spawnTimer,w,h;
    AAssetManager* assets;
} G;

static void spawn_spike() {
    if(G.spikeCount<50){ G.spikes[G.spikeCount].x=G.w+50+rand()%300; G.spikes[G.spikeCount].y=G.groundY; G.spikes[G.spikeCount].active=1; G.spikeCount++; }
}

void game_init(AAssetManager* assets, int w, int h) {
    memset(&G,0,sizeof(G));
    G.assets=assets; G.w=w; G.h=h;
    G.groundY=h-120; G.gravity=0.6f; G.jumpPower=-12.0f; G.speed=7.0f; G.jumpDur=0.48f;
    G.size=150*0.35f; G.px=200; G.py=G.groundY-G.size/2; G.grounded=1;
    G.playerTex=load_texture(assets,"cube1.png");
    G.spikeTex=load_texture(assets,"spike.png");
    G.font=load_font(assets,"Roboto-Regular.ttf",h/25.0f);
    srand((unsigned)time(NULL));
    for(int i=0;i<5;i++) spawn_spike();
}

void game_free() {
    free_texture(G.playerTex); free_texture(G.spikeTex); free_font(G.font);
    memset(&G,0,sizeof(G));
}

void game_jump() {
    if(G.gameOver){ game_init(G.assets, G.w, G.h); return; }
    if(G.grounded){ G.vy=G.jumpPower; G.grounded=0; G.jumpCount++; G.jumpTime=0; G.targetAngle=G.angle+M_PI; }
}

void game_update(int w, int h) {
    G.w=w; G.h=h; G.groundY=h-120;
    if(G.gameOver) return;
    G.vy+=G.gravity; G.py+=G.vy;
    if(G.py+G.size/2>=G.groundY){
        G.py=G.groundY-G.size/2; G.vy=0;
        if(!G.grounded){ float snap=roundf(G.angle/(M_PI/2))*(M_PI/2); G.angle=snap; G.targetAngle=snap; }
        G.grounded=1; G.jumpTime=0;
    }
    if(G.py-G.size/2<0){ G.py=G.size/2; G.vy=0; }
    if(!G.grounded){
        G.jumpTime+=0.016f; float prog=G.jumpTime/G.jumpDur; if(prog>1) prog=1;
        G.angle=G.targetAngle-M_PI + prog*M_PI;
    }
    float spikeSize=49*0.6f;
    for(int i=0;i<G.spikeCount;i++){
        if(!G.spikes[i].active) continue;
        G.spikes[i].x-=G.speed;
        float dx=G.px-G.spikes[i].x, dy=G.py-G.spikes[i].y;
        if(sqrtf(dx*dx+dy*dy) < (G.size/2+spikeSize/2)){ G.gameOver=1; return; }
        if(G.spikes[i].x<-50) G.spikes[i].active=0;
    }
    G.spawnTimer++;
    if(G.spawnTimer>60){
        G.spawnTimer=0;
        int active=0; for(int i=0;i<G.spikeCount;i++) if(G.spikes[i].active) active++;
        if(active<8) spawn_spike();
    }
    int write=0; for(int i=0;i<G.spikeCount;i++) if(G.spikes[i].active) G.spikes[write++]=G.spikes[i];
    G.spikeCount=write;
    G.score++;
}

void game_draw(RenderBuffer* rb) {
    graphics_clear(rb,0xFF16213e);
    graphics_draw_rect(rb,rb->width/2,(int)G.groundY+10,rb->width,8,0xFF0a0a1a);
    graphics_draw_rect(rb,rb->width/2,(int)G.groundY,rb->width,8,0xFF1a1a2e);
    if(G.spikeTex && G.spikeTex->data){
        for(int i=0;i<G.spikeCount;i++) if(G.spikes[i].active){
            int dx=(int)G.spikes[i].x, dy=(int)(G.spikes[i].y-(49*0.6f)/2);
            if(dx>-50 && dx<rb->width+50) graphics_draw_texture_rotated(rb,dx,dy,G.spikeTex->data,G.spikeTex->w,G.spikeTex->h,0.6f,0);
        }
    }
    if(G.playerTex && G.playerTex->data){
        graphics_draw_texture_rotated(rb,(int)G.px,(int)G.py,G.playerTex->data,G.playerTex->w,G.playerTex->h,0.35f,G.angle);
    } else {
        // fallback на случай, если текстура не загружена
        graphics_draw_rect(rb, (int)G.px, (int)G.py, (int)G.size, (int)G.size, 0xFF4CAF50);
    }
    char buf[32]; snprintf(buf,sizeof(buf),"%d",G.score);
    if(G.font) draw_text(G.font,rb,rb->width/2-50,60,buf,0xFFFFFFFF);
}
