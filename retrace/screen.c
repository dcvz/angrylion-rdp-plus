#include "screen.h"
#include "msg.h"
#include "retrace.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <SDL.h>

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* texture = NULL;

static int32_t texture_width;
static int32_t texture_height;

void screen_init(void)
{
    SDL_Init(SDL_INIT_VIDEO);
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);

    window = SDL_CreateWindow(
        "RDP Retracer",             // window title
        SDL_WINDOWPOS_CENTERED,     // initial x position
        SDL_WINDOWPOS_CENTERED,     // initial y position
        640, 480,                   // width and height, in pixels
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL    // flags
    );

    if (!window) {
        msg_error("Can't create main window: %s", SDL_GetError());
    }

    renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (!renderer) {
        msg_error("Can't create renderer: %s", SDL_GetError());
    }

    // reset state
    texture_width = 0;
    texture_height = 0;
}

void screen_swap(void)
{
    SDL_UnlockTexture(texture);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void screen_get_buffer(int width, int height, int display_height, int** buffer, int* pitch)
{
    if (texture_width != width || texture_height != height) {
        SDL_DisplayMode mode;
        SDL_GetDisplayMode(0, 0, &mode);

        // update window size and position
        SDL_SetWindowSize(window, width, display_height);
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        SDL_RenderSetLogicalSize(renderer, width, display_height);

        // (re)create frame buffer texture
        if (texture) {
            SDL_DestroyTexture(texture);
        }

        texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            width,
            height
        );

        if (!texture) {
            msg_error("Can't create texture: %s", SDL_GetError());
        }

        texture_width = width;
        texture_height = height;
    }

    SDL_LockTexture(texture, NULL, buffer, pitch);
}

void screen_set_full(bool fullscreen)
{
    uint32_t flags = fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
    SDL_SetWindowFullscreen(window, flags);
}

void screen_capture(char* path)
{
}

void screen_close(void)
{
    SDL_DestroyTexture(texture);
    texture = NULL;

    SDL_DestroyRenderer(renderer);
    renderer = NULL;

    SDL_DestroyWindow(window);
    window = NULL;
}
