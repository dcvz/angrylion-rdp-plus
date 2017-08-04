#pragma once

#include <stdint.h>
#include <stdbool.h>

struct screen_api
{
    void (*init)(void);
    void (*swap)(void);
    void (*get_buffer)(int width, int height, int display_height, int** buffer, int* pitch);
    void (*set_fullscreen)(bool fullscreen);
    bool (*get_fullscreen)(void);
    void (*capture)(char* path);
    void (*close)(void);
};

struct core_config
{
    uint32_t num_workers;
    bool tv_fading;
    bool trace;
    void (*screen_api)(struct screen_api* api);
};

void core_init(struct core_config* config);
void core_close(void);
void core_update(void);
void core_update_dp(void);
void core_update_vi(void);
void core_screenshot(char* directory, char* name);
void core_toggle_fullscreen(void);
