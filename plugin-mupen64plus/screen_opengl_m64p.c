#include "screen_opengl_m64p.h"
#include "plugin.h"

#include "core/msg.h"
#include "gl-screen/gl_screen.h"

#include <stdlib.h>

extern GFX_INFO gfx;
extern m64p_dynlib_handle CoreLibHandle;

/* definitions of pointers to Core video extension functions */
ptr_VidExt_Init                  CoreVideo_Init = NULL;
ptr_VidExt_Quit                  CoreVideo_Quit = NULL;
ptr_VidExt_ListFullscreenModes   CoreVideo_ListFullscreenModes = NULL;
ptr_VidExt_SetVideoMode          CoreVideo_SetVideoMode = NULL;
ptr_VidExt_SetCaption            CoreVideo_SetCaption = NULL;
ptr_VidExt_ToggleFullScreen      CoreVideo_ToggleFullScreen = NULL;
ptr_VidExt_ResizeWindow          CoreVideo_ResizeWindow = NULL;
ptr_VidExt_GL_GetProcAddress     CoreVideo_GL_GetProcAddress = NULL;
ptr_VidExt_GL_SetAttribute       CoreVideo_GL_SetAttribute = NULL;
ptr_VidExt_GL_GetAttribute       CoreVideo_GL_GetAttribute = NULL;
ptr_VidExt_GL_SwapBuffers        CoreVideo_GL_SwapBuffers = NULL;

static int32_t toggle_fs;

// framebuffer texture states
int32_t window_width;
int32_t window_height;
int32_t window_fullscreen;

static void screen_init(void)
{
    /* Get the core Video Extension function pointers from the library handle */
    CoreVideo_Init = (ptr_VidExt_Init) DLSYM(CoreLibHandle, "VidExt_Init");
    CoreVideo_Quit = (ptr_VidExt_Quit) DLSYM(CoreLibHandle, "VidExt_Quit");
    CoreVideo_ListFullscreenModes = (ptr_VidExt_ListFullscreenModes) DLSYM(CoreLibHandle, "VidExt_ListFullscreenModes");
    CoreVideo_SetVideoMode = (ptr_VidExt_SetVideoMode) DLSYM(CoreLibHandle, "VidExt_SetVideoMode");
    CoreVideo_SetCaption = (ptr_VidExt_SetCaption) DLSYM(CoreLibHandle, "VidExt_SetCaption");
    CoreVideo_ToggleFullScreen = (ptr_VidExt_ToggleFullScreen) DLSYM(CoreLibHandle, "VidExt_ToggleFullScreen");
    CoreVideo_ResizeWindow = (ptr_VidExt_ResizeWindow) DLSYM(CoreLibHandle, "VidExt_ResizeWindow");
    CoreVideo_GL_GetProcAddress = (ptr_VidExt_GL_GetProcAddress) DLSYM(CoreLibHandle, "VidExt_GL_GetProcAddress");
    CoreVideo_GL_SetAttribute = (ptr_VidExt_GL_SetAttribute) DLSYM(CoreLibHandle, "VidExt_GL_SetAttribute");
    CoreVideo_GL_GetAttribute = (ptr_VidExt_GL_GetAttribute) DLSYM(CoreLibHandle, "VidExt_GL_GetAttribute");
    CoreVideo_GL_SwapBuffers = (ptr_VidExt_GL_SwapBuffers) DLSYM(CoreLibHandle, "VidExt_GL_SwapBuffers");

    CoreVideo_Init();

    CoreVideo_GL_SetAttribute(M64P_GL_CONTEXT_PROFILE_MASK, M64P_GL_CONTEXT_PROFILE_CORE);
    CoreVideo_GL_SetAttribute(M64P_GL_CONTEXT_MAJOR_VERSION, 3);
    CoreVideo_GL_SetAttribute(M64P_GL_CONTEXT_MINOR_VERSION, 3);

    CoreVideo_SetVideoMode(window_width, window_height, 0, window_fullscreen ? M64VIDEO_FULLSCREEN : M64VIDEO_WINDOWED, M64VIDEOFLAG_SUPPORT_RESIZING);

    gl_screen_init();
}

static void screen_swap(void)
{
    if (toggle_fs)
    {
        CoreVideo_ToggleFullScreen();
        toggle_fs = 0;
    }

    gl_screen_render(window_width, window_height, 0, 0);

    (*renderCallback)(1);
    CoreVideo_GL_SwapBuffers();
}

static void screen_upload(int* buffer, int width, int height, int output_width, int output_height)
{
    gl_screen_upload(buffer, width, height, output_width, output_height);
}

static void screen_set_fullscreen(bool _fullscreen)
{
    toggle_fs = 1;
}

static bool screen_get_fullscreen(void)
{
    return 0;
}

static void screen_close(void)
{
    gl_screen_close();

    CoreVideo_Quit();
}

void screen_opengl_m64p(struct screen_api* api)
{
    api->init = screen_init;
    api->swap = screen_swap;
    api->upload = screen_upload;
    api->set_fullscreen = screen_set_fullscreen;
    api->get_fullscreen = screen_get_fullscreen;
    api->close = screen_close;
}

void ogl_readscreen(void *_dest, int *_width, int *_height, int _front)
{
    if (_width == NULL || _height == NULL)
        return;

    *_width = window_width;
    *_height = window_height;

    if (_dest == NULL)
        return;

    unsigned char *pBufferData = (unsigned char*)malloc((*_width)*(*_height) * 4);
    unsigned char *pDest = (unsigned char*)_dest;

    glReadPixels(0, 0, window_width, window_height, GL_RGBA, GL_UNSIGNED_BYTE, pBufferData);

    //Convert RGBA to RGB
    for (int32_t y = 0; y < *_height; ++y) {
        unsigned char *ptr = pBufferData + ((*_width) * 4 * y);
        for (int32_t x = 0; x < *_width; ++x) {
            pDest[x * 3] = ptr[0]; // red
            pDest[x * 3 + 1] = ptr[1]; // green
            pDest[x * 3 + 2] = ptr[2]; // blue
            ptr += 4;
        }
        pDest += (*_width) * 3;
    }

    free(pBufferData);
}
