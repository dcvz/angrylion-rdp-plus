#pragma once

void screen_init(int width, int height, int screen_width, int screen_height);
void screen_swap(int visiblelines);
void screen_lock(int** prescale, int* pitch);
void screen_unlock();
void screen_move();
void screen_close();
