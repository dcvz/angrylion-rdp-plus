#pragma once

#include <stdbool.h>

struct rdp_config
{
    bool parallel;
};

int rdp_init(struct rdp_config config);
void rdp_update(void);
