#pragma once

#include "core/core.h"

bool config_load(struct core_config* config, const char* path);
bool config_save(struct core_config* config, const char* path);
