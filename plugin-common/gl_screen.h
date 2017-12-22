#pragma once

#include "core/rdp.h"

#include <stdint.h>
#include <stdbool.h>

void gl_screen_init(struct rdp_config* config);
bool gl_screen_upload(int32_t* buffer, int32_t width, int32_t height, int32_t output_width, int32_t output_height);
void gl_screen_download(int32_t* buffer, int32_t* width, int32_t* height);
void gl_screen_render(int32_t win_width, int32_t win_height, int32_t win_x, int32_t win_y);
void gl_screen_close(void);
