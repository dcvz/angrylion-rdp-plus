#pragma once

#include "core/n64video.h"

#include <Windows.h>

void config_init(HINSTANCE hInst);
void config_dialog(HWND hParent);
struct rdp_config* config_get(void);
bool config_load(void);
bool config_save(void);
