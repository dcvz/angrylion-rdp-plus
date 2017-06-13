#pragma once

#include <stdint.h>
#include <stdbool.h>

#define CMD_MAX_INTS 44
#define CMD_MAX_SIZE (CMD_MAX_INTS * sizeof(int32_t))

struct rdp_config
{
    bool parallel;
};

int rdp_init(struct rdp_config config);
void rdp_update(void);
