#ifndef MAIN_H
#define MAIN_H

#include <android/asset_manager.h>
#include <android_native_app_glue.h>
#include <sys/time.h>

typedef enum { STATE_MENU, STATE_GAME } AppState;

typedef struct {
    struct android_app* app;
    AppState state;
    AAssetManager* assets;
    int w, h;
    struct timeval lastTime;
    int frames;
    float fps;
} App;

void app_init(App* a, struct android_app* app);
void app_update(App* a);
void app_draw(App* a);
void app_free(App* a);

#endif
