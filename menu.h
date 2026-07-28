#ifndef MENU_H
#define MENU_H

#include <android/asset_manager.h>
#include "graphics.h"

void menu_init(AAssetManager* assets, int w, int h);
void menu_update(int w, int h);
void menu_draw(RenderBuffer* rb);
void menu_free(void);

#endif
