#pragma once

#include <stdint.h>

#define PRESCALE_WIDTH 640
#define PRESCALE_HEIGHT 625

int rdp_init(void);
int rdp_close(void);
int rdp_update(void);
void rdp_process_list(void);
