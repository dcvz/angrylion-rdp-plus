#ifndef PLUGIN_H
#define PLUGIN_H

#include "core/core.h"
#include "api/m64p_plugin.h"

#ifdef _WIN32
#define DLSYM(a, b) GetProcAddress(a, b)
#else
#include <dlfcn.h>
#define DLSYM(a, b) dlsym(a, b)
#endif

void plugin_mupen64plus(struct plugin_api* api);
extern void(*renderCallback)(int);

#endif
