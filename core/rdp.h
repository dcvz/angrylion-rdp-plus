#pragma once

#include <stdint.h>

int rdp_init(void);
int rdp_close(void);
int rdp_update(void);
void rdp_process_list(void);
