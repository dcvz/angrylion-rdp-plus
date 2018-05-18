#include "gl_screen.h"
#include "core/msg.h"

#ifdef GLES
#include "screen/gl_screen_es_3_0.c"
#else
#include "gl_3_3/gl_core_3_3.h"
#include "gl_3_3/gl_core_3_3.c"
#include "gl_3_3/gl_screen_3_3.c"
#endif
