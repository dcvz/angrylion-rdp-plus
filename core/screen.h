#pragma once

#include <stdbool.h>

void screen_init(void);
void screen_swap(void);
void screen_get_buffer(int width, int height, int display_height, int** buffer, int* pitch);
void screen_set_full(bool fullscreen);
void screen_capture(char* path);
void screen_close(void);
