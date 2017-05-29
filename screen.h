#pragma once

#include "gfx_1.3.h"

#include <stdbool.h>

void screen_init(GFX_INFO* info);
void screen_swap(void);
void screen_get_buffer(int width, int height, int display_width, int display_height, int** buffer, int* pitch);
void screen_set_full(bool fullscreen);
void screen_close(void);
