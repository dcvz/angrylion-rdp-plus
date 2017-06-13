#pragma once

#include <stdint.h>
#include <stdbool.h>

struct core_config
{
    bool parallel;
    bool tv_fading;
    bool trace;
};

void core_init(struct core_config* config);
void core_close(void);
void core_update_dp(void);
void core_update_vi(void);
void core_screenshot(char* directory, char* name);
void core_toggle_fullscreen(void);
