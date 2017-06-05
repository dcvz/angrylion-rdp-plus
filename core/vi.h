#pragma once

#include <stdbool.h>

struct vi_config
{
    bool parallel;
    bool tv_fading;
};

void vi_init(struct vi_config config);
void vi_update(void);
