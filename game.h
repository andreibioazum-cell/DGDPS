#ifndef GAME_H
#define GAME_H

#include <android/asset_manager.h>
#include "graphics.h"

void game_init(AAssetManager* assets, int w, int h);
void game_update(int w, int h);
void game_draw(RenderBuffer* rb);
void game_free(void);
void game_jump(void);

#endif
