#include "main.h"
#include "graphics.h"
#include "game.h"
#include "menu.h"
#include <android/input.h>
#include <string.h>

static App g_app;

void app_init(App* a, struct android_app* app) {
    memset(a,0,sizeof(App));
    a->app=app; a->state=STATE_MENU; a->assets=app->activity->assetManager;
    a->w=ANativeWindow_getWidth(app->window); a->h=ANativeWindow_getHeight(app->window);
    gettimeofday(&a->lastTime,0);
    menu_init(a->assets,a->w,a->h);
}

void app_update(App* a) {
    a->frames++;
    struct timeval now; gettimeofday(&now,0);
    float dt=(now.tv_sec-a->lastTime.tv_sec)+(now.tv_usec-a->lastTime.tv_usec)/1e6f;
    if(dt>=1.0f){ a->fps=a->frames/dt; a->frames=0; a->lastTime=now; }
    if(a->state==STATE_MENU) menu_update(a->w,a->h);
    else game_update(a->w,a->h);
}

void app_draw(App* a) {
    RenderBuffer rb; ANativeWindow_Buffer buf;
    if(ANativeWindow_lock(a->app->window,&buf,0)==0){
        rb.pixels=(uint32_t*)buf.bits; rb.width=buf.width; rb.height=buf.height; rb.stride=buf.stride;
        if(a->state==STATE_MENU) menu_draw(&rb);
        else game_draw(&rb);
        ANativeWindow_unlockAndPost(a->app->window);
    }
}

void app_free(App* a){ menu_free(); game_free(); }

static void handle_cmd(struct android_app* app, int32_t cmd){
    App* a=(App*)app->userData;
    if(cmd==APP_CMD_INIT_WINDOW){ a->w=ANativeWindow_getWidth(app->window); a->h=ANativeWindow_getHeight(app->window); ANativeWindow_setBuffersGeometry(app->window,0,0,WINDOW_FORMAT_RGBA_8888); }
}
static int32_t handle_input(struct android_app* app, AInputEvent* e){
    App* a=(App*)app->userData;
    if(AInputEvent_getType(e)==AINPUT_EVENT_TYPE_MOTION && AMotionEvent_getAction(e)==AMOTION_EVENT_ACTION_DOWN){
        if(a->state==STATE_MENU){ game_init(a->assets,a->w,a->h); a->state=STATE_GAME; }
        else game_jump();
        return 1;
    }
    return 0;
}

void android_main(struct android_app* app){
    memset(&g_app,0,sizeof(g_app));
    app->userData=&g_app; app->onAppCmd=handle_cmd; app->onInputEvent=handle_input;
    app_init(&g_app,app);
    while(1){
        int ident; struct android_poll_source* src;
        while((ident=ALooper_pollOnce(0,0,0,(void**)&src))>=0){
            if(src) src->process(app,src);
            if(app->destroyRequested){ app_free(&g_app); return; }
        }
        if(app->window){ app_update(&g_app); app_draw(&g_app); }
    }
}
