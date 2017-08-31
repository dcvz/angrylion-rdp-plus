#ifndef SCREEN_M64P_H
#define SCREEN_M64P_H

#include "core/core.h"
#include "api/m64p_vidext.h"

void screen_opengl_m64p(struct screen_api* api);
void ogl_readscreen(void *dest, int *width, int *height, int front);

#endif
