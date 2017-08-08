#include "screen_headless.h"

#include <stdlib.h>

static int32_t* buffer;

static void screen_init(void)
{
}

static void screen_swap(void)
{
}

static void screen_get_buffer(int width, int height, int display_height, int** _buffer, int* _pitch)
{
    int pitch = width * sizeof(int32_t);
    buffer = realloc(buffer, height * pitch);
    *_buffer = buffer;
    *_pitch = pitch;
}

static void screen_set_fullscreen(bool fullscreen)
{
}

static bool screen_get_fullscreen(void)
{
    return false;
}

static void screen_capture(char* path)
{
}

static void screen_close(void)
{
    if (buffer) {
        free(buffer);
        buffer = NULL;
    }
}

void screen_headless(struct screen_api* api)
{
    api->init = screen_init;
    api->swap = screen_swap;
    api->get_buffer = screen_get_buffer;
    api->set_fullscreen = screen_set_fullscreen;
    api->get_fullscreen = screen_get_fullscreen;
    api->capture = screen_capture;
    api->close = screen_close;
}
