#pragma once

#include "core.h"

int rdp_init(struct core_config* config);
void rdp_cmd(const uint32_t* arg, uint32_t length);
void rdp_cmd_update(void);
void rdp_vi_update(void);
void rdp_screenshot(char* path);
void rdp_close(void);
