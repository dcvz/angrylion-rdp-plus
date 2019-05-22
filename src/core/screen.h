#pragma once

#include "n64video.h"

#include <stdint.h>
#include <stdbool.h>

struct rgba
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

struct frame_buffer
{
    struct rgba* pixels;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
};

void screen_init(struct n64video_config* config);
void screen_swap(bool blank);
void screen_write(struct frame_buffer* fb, int32_t output_height);
void screen_read(struct frame_buffer* fb, bool alpha);
void screen_set_fullscreen(bool fullscreen);
bool screen_get_fullscreen(void);
void screen_toggle_fullscreen(void);
void screen_close(void);
