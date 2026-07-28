#include "graphics.h"
#include <arm_neon.h>
#include <math.h>

void graphics_clear(RenderBuffer* rb, uint32_t color){
    uint32x4_t v=vdupq_n_u32(color);
    int n=rb->stride*rb->height, i=0;
    for(; i<=n-4; i+=4) vst1q_u32(&rb->pixels[i], v);
    for(; i<n; i++) rb->pixels[i]=color;
}

void graphics_draw_rect(RenderBuffer* rb, int x, int y, int w, int h, uint32_t color){
    int x1=x-w/2, x2=x+w/2, y1=y-h/2, y2=y+h/2;
    if(x1<0)x1=0; if(x2>rb->width)x2=rb->width;
    if(y1<0)y1=0; if(y2>rb->height)y2=rb->height;
    for(int i=y1;i<y2;i++){
        uint32_t* line=rb->pixels + i*rb->stride;
        for(int j=x1;j<x2;j++) line[j]=color;
    }
}

void graphics_draw_texture_rotated(RenderBuffer* rb, int x, int y,
                                   uint32_t* tex, int tw, int th,
                                   float scale, float angle){
    if(!tex || tw<=0 || th<=0) return;
    int sw=(int)(tw*scale), sh=(int)(th*scale);
    if(sw<=0 || sh<=0) return;
    float cos_a=cosf(angle), sin_a=sinf(angle);
    float hw=sw/2.0f, hh=sh/2.0f;
    int x1=x-sw/2, x2=x+sw/2, y1=y-sh/2, y2=y+sh/2;
    if(x1<0)x1=0; if(x2>rb->width)x2=rb->width;
    if(y1<0)y1=0; if(y2>rb->height)y2=rb->height;
    for(int py=y1; py<y2; py++){
        uint32_t* out=rb->pixels + py*rb->stride;
        for(int px=x1; px<x2; px++){
            float dx=px-x, dy=py-y;
            float sx=dx*cos_a + dy*sin_a + hw;
            float sy=-dx*sin_a + dy*cos_a + hh;
            int ix=(int)(sx/scale + 0.5f), iy=(int)(sy/scale + 0.5f);
            if(ix>=0 && ix<tw && iy>=0 && iy<th){
                uint32_t p=tex[iy*tw+ix];
                if(p & 0xFF000000) out[px]=p;
            }
        }
    }
}
