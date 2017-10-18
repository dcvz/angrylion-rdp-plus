#pragma once

#include "core/core.h"

#include <Windows.h>

void config_init(HINSTANCE hInst);
void config_dialog(HWND hParent);
struct core_config* config_get(void);
bool config_load(void);
bool config_save(void);
