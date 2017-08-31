#include "screen_opengl_m64p.h"
#include "plugin.h"
#include "core/msg.h"

#include <stdlib.h>
#include <GL/gl.h>
#include "glext.h"

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

PFNGLCREATEPROGRAMPROC g_glCreateProgram;
PFNGLATTACHSHADERPROC g_glAttachShader;
PFNGLLINKPROGRAMPROC g_glLinkProgram;
PFNGLGETPROGRAMIVPROC g_glGetProgramiv;
PFNGLDELETESHADERPROC g_glDeleteShader;
PFNGLGETPROGRAMINFOLOGPROC g_glGetProgramInfoLog;
PFNGLGETSHADERINFOLOGPROC g_glGetShaderInfoLog;
PFNGLCREATESHADERPROC g_glCreateShader;
PFNGLCOMPILESHADERPROC g_glCompileShader;
PFNGLSHADERSOURCEPROC g_glShaderSource;
PFNGLGETSHADERIVPROC g_glGetShaderiv;
PFNGLUSEPROGRAMPROC g_glUseProgram;
PFNGLGENVERTEXARRAYSPROC g_glGenVertexArrays;
PFNGLBINDVERTEXARRAYPROC g_glBindVertexArray;
PFNGLDELETEVERTEXARRAYSPROC g_glDeleteVertexArrays;
PFNGLDELETEPROGRAMPROC g_glDeleteProgram;

// OpenGL objects
static GLuint program;
static GLuint vao;
static GLuint texture;

static int32_t toggle_fs;

// framebuffer texture states
static int32_t tex_width;
static int32_t tex_height;
static int32_t tex_display_width;
static int32_t tex_display_height;
int32_t window_width;
int32_t window_height;
int32_t window_fullscreen;

#define TEX_INTERNAL_FORMAT GL_RGBA8
#define TEX_FORMAT GL_BGRA
#define TEX_TYPE GL_UNSIGNED_INT_8_8_8_8_REV

static void glSetupFunctions(void)
{
    g_glCreateProgram = (PFNGLCREATEPROGRAMPROC) CoreVideo_GL_GetProcAddress("glCreateProgram");
    g_glAttachShader = (PFNGLATTACHSHADERPROC) CoreVideo_GL_GetProcAddress("glAttachShader");
    g_glLinkProgram = (PFNGLLINKPROGRAMPROC) CoreVideo_GL_GetProcAddress("glLinkProgram");
    g_glGetProgramiv = (PFNGLGETPROGRAMIVPROC) CoreVideo_GL_GetProcAddress("glGetProgramiv");
    g_glDeleteShader = (PFNGLDELETESHADERPROC) CoreVideo_GL_GetProcAddress("glDeleteShader");
    g_glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC) CoreVideo_GL_GetProcAddress("glGetProgramInfoLog");
    g_glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC) CoreVideo_GL_GetProcAddress("glGetShaderInfoLog");
    g_glCreateShader = (PFNGLCREATESHADERPROC) CoreVideo_GL_GetProcAddress("glCreateShader");
    g_glCompileShader = (PFNGLCOMPILESHADERPROC) CoreVideo_GL_GetProcAddress("glCompileShader");
    g_glShaderSource = (PFNGLSHADERSOURCEPROC) CoreVideo_GL_GetProcAddress("glShaderSource");
    g_glGetShaderiv = (PFNGLGETSHADERIVPROC) CoreVideo_GL_GetProcAddress("glGetShaderiv");
    g_glUseProgram = (PFNGLUSEPROGRAMPROC) CoreVideo_GL_GetProcAddress("glUseProgram");
    g_glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC) CoreVideo_GL_GetProcAddress("glGenVertexArrays");
    g_glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC) CoreVideo_GL_GetProcAddress("glBindVertexArray");
    g_glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC) CoreVideo_GL_GetProcAddress("glDeleteVertexArrays");
    g_glDeleteProgram = (PFNGLDELETEPROGRAMPROC) CoreVideo_GL_GetProcAddress("glDeleteProgram");
}

// OpenGL helpers
static GLuint gl_shader_compile(GLenum type, const GLchar* source)
{
    GLuint shader = g_glCreateShader(type);
    g_glShaderSource(shader, 1, &source, NULL);
    g_glCompileShader(shader);

    GLint param;
    g_glGetShaderiv(shader, GL_COMPILE_STATUS, &param);

    if (!param) {
        GLchar log[4096];
        g_glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        msg_error("%s shader error: %s\n", type == GL_FRAGMENT_SHADER ? "Frag" : "Vert", log);
    }

    return shader;
}

static GLuint gl_shader_link(GLuint vert, GLuint frag)
{
    GLuint program = g_glCreateProgram();
    g_glAttachShader(program, vert);
    g_glAttachShader(program, frag);
    g_glLinkProgram(program);

    GLint param;
    g_glGetProgramiv(program, GL_LINK_STATUS, &param);

    if (!param) {
        GLchar log[4096];
        g_glGetProgramInfoLog(program, sizeof(log), NULL, log);
        msg_error("Shader link error: %s\n", log);
    }

    g_glDeleteShader(frag);
    g_glDeleteShader(vert);

    return program;
}

static void glSetup(void)
{
    // shader sources for drawing a clipped full-screen triangle. the geometry
    // is defined by the vertex ID, so a VBO is not required.
    const GLchar* vert_shader =
        "#version 330 core\n"
        "out vec2 uv;\n"
        "void main(void) {\n"
        "    uv = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"
        "    gl_Position = vec4(uv * vec2(2.0, -2.0) + vec2(-1.0, 1.0), 0.0, 1.0);\n"
        "}\n";

    const GLchar* frag_shader =
        "#version 330 core\n"
        "in vec2 uv;\n"
        "layout(location = 0) out vec4 color;\n"
        "uniform sampler2D tex0;\n"
        "void main(void) {\n"
        "    color = texture(tex0, uv);\n"
        "}\n";

    // compile and link OpenGL program
    GLuint vert = gl_shader_compile(GL_VERTEX_SHADER, vert_shader);
    GLuint frag = gl_shader_compile(GL_FRAGMENT_SHADER, frag_shader);
    program = gl_shader_link(vert, frag);
    g_glUseProgram(program);

    // prepare dummy VAO
    g_glGenVertexArrays(1, &vao);
    g_glBindVertexArray(vao);

    // prepare texture
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

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

    glSetupFunctions();

    glSetup();
}

static void screen_swap(void)
{
    if (toggle_fs)
    {
        CoreVideo_ToggleFullScreen();
        toggle_fs = 0;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    int32_t vp_width = window_width;
    int32_t vp_height = window_height;

    // default to bottom left corner of the window above the status bar
    int32_t vp_x = 0;
    int32_t vp_y = 0;

    int32_t hw = tex_display_height * vp_width;
    int32_t wh = tex_display_width * vp_height;

    // add letterboxes or pillarboxes if the window has a different aspect ratio
    // than the current display mode
    if (hw > wh) {
        int32_t w_max = wh / tex_display_height;
        vp_x += (vp_width - w_max) / 2;
        vp_width = w_max;
    } else if (hw < wh) {
        int32_t h_max = hw / tex_display_width;
        vp_y += (vp_height - h_max) / 2;
        vp_height = h_max;
    }

    // configure viewport
    glViewport(vp_x, vp_y, vp_width, vp_height);

    glDrawArrays(GL_TRIANGLES, 0, 3);
    (*renderCallback)(1);
    CoreVideo_GL_SwapBuffers();
}

static void screen_upload(int* buffer, int width, int height, int output_width, int output_height)
{
    // check if the framebuffer size has changed
    if (tex_width != width || tex_height != height) {
        tex_width = width;
        tex_height = height;

        // reallocate texture buffer on GPU
        glTexImage2D(GL_TEXTURE_2D, 0, TEX_INTERNAL_FORMAT, tex_width,
            tex_height, 0, TEX_FORMAT, TEX_TYPE, buffer);

        // update output size
        tex_display_width = output_width;
        tex_display_height = output_height;

        msg_debug("screen: resized framebuffer texture: %dx%d", tex_width, tex_height);
    } else {
        // copy local buffer to GPU texture buffer
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tex_width, tex_height,
            TEX_FORMAT, TEX_TYPE, buffer);
    }
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
    tex_width = 0;
    tex_height = 0;

    glDeleteTextures(1, &texture);
    g_glDeleteVertexArrays(1, &vao);
    g_glDeleteProgram(program);

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
    for (uint32_t y = 0; y < *_height; ++y) {
        unsigned char *ptr = pBufferData + ((*_width) * 4 * y);
        for (uint32_t x = 0; x < *_width; ++x) {
            pDest[x * 3] = ptr[0]; // red
            pDest[x * 3 + 1] = ptr[1]; // green
            pDest[x * 3 + 2] = ptr[2]; // blue
            ptr += 4;
        }
        pDest += (*_width) * 3;
    }

    free(pBufferData);
}
