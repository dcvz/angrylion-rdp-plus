#include "gl_screen.h"
#include "core/msg.h"

#ifdef GLES
#include "screen/gl_screen_es_3_0.c"
#else
#include "screen/gl_screen_3_3.c"
#endif
