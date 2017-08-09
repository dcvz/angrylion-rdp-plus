#include "screen_opengl.h"
#include "msg.h"
#include "gfx_1.3.h"
#include "gl_core_3_3.h"
#include "wgl_ext.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#pragma comment (lib, "opengl32.lib")

extern GFX_INFO gfx;

// default size of the window
#define WINDOW_DEFAULT_WIDTH 640
#define WINDOW_DEFAULT_HEIGHT 480

// supposedly, these settings are most hardware-friendly on all platforms
#define TEX_INTERNAL_FORMAT GL_RGBA8
#define TEX_FORMAT GL_BGRA
#define TEX_TYPE GL_UNSIGNED_INT_8_8_8_8_REV
#define TEX_BYTES_PER_PIXEL sizeof(uint32_t)

// OpenGL objects
static GLuint program;
static GLuint vao;
static GLuint texture;

// framebuffer texture states
static uint32_t* tex_buffer;
static int32_t tex_width;
static int32_t tex_height;
static int32_t tex_display_height;

// context states
static HDC dc;
static HGLRC glrc;
static HGLRC glrc_core;
static bool fullscreen;

// Win32 helpers
void win32_client_resize(HWND hWnd, int nWidth, int nHeight)
{
    RECT rclient;
    GetClientRect(hWnd, &rclient);

    RECT rwin;
    GetWindowRect(hWnd, &rwin);

    POINT pdiff;
    pdiff.x = (rwin.right - rwin.left) - rclient.right;
    pdiff.y = (rwin.bottom - rwin.top) - rclient.bottom;

    MoveWindow(hWnd, rwin.left, rwin.top, nWidth + pdiff.x, nHeight + pdiff.y, TRUE);
}

// OpenGL helpers
static GLuint gl_shader_compile(GLenum type, const GLchar* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint param;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &param);

    if (!param) {
        GLchar log[4096];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        msg_error("%s shader error: %s\n", type == GL_FRAGMENT_SHADER ? "Frag" : "Vert", log);
    }

    return shader;
}

static GLuint gl_shader_link(GLuint vert, GLuint frag)
{
    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    GLint param;
    glGetProgramiv(program, GL_LINK_STATUS, &param);

    if (!param) {
        GLchar log[4096];
        glGetProgramInfoLog(program, sizeof(log), NULL, log);
        msg_error("Shader link error: %s\n", log);
    }

    glDeleteShader(frag);
    glDeleteShader(vert);

    return program;
}

static void gl_check_errors(void)
{
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        char* err_str;
        switch (err) {
            case GL_INVALID_OPERATION:
                err_str = "INVALID_OPERATION";
                break;
            case GL_INVALID_ENUM:
                err_str = "INVALID_ENUM";
                break;
            case GL_INVALID_VALUE:
                err_str = "INVALID_VALUE";
                break;
            case GL_OUT_OF_MEMORY:
                err_str = "OUT_OF_MEMORY";
                break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:
                err_str = "INVALID_FRAMEBUFFER_OPERATION";
                break;
            default:
                err_str = "unknown";
        }
        msg_warning("OpenGL error: %d (%s)", err, err_str);
    }
}

static void screen_update_size(int32_t width, int32_t height)
{
    BOOL zoomed = IsZoomed(gfx.hWnd);

    if (zoomed) {
        ShowWindow(gfx.hWnd, SW_RESTORE);
    }

    if (!fullscreen) {
        // reserve some pixels for the status bar
        RECT statusrect;
        SetRectEmpty(&statusrect);

        if (gfx.hStatusBar) {
            GetClientRect(gfx.hStatusBar, &statusrect);
        }

        // resize window
        win32_client_resize(gfx.hWnd, width, height + statusrect.bottom);
    }

    if (zoomed) {
        ShowWindow(gfx.hWnd, SW_MAXIMIZE);
    }
}

static void screen_init(void)
{
    // make window resizable for the user
    if (!fullscreen) {
        LONG style = GetWindowLong(gfx.hWnd, GWL_STYLE);
        style |= WS_SIZEBOX | WS_MAXIMIZEBOX;
        SetWindowLong(gfx.hWnd, GWL_STYLE, style);
    }

    screen_update_size(WINDOW_DEFAULT_WIDTH, WINDOW_DEFAULT_HEIGHT);

    PIXELFORMATDESCRIPTOR win_pfd = {
        sizeof(PIXELFORMATDESCRIPTOR), 1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, // Flags
        PFD_TYPE_RGBA, // The kind of framebuffer. RGBA or palette.
        32,            // Colordepth of the framebuffer.
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        24, // Number of bits for the depthbuffer
        8,  // Number of bits for the stencilbuffer
        0,  // Number of Aux buffers in the framebuffer.
        PFD_MAIN_PLANE, 0, 0, 0, 0
    };

    dc = GetDC(gfx.hWnd);
    if (!dc) {
        msg_error("Can't get device context.");
    }

    int win_pf = ChoosePixelFormat(dc, &win_pfd);
    if (!win_pf) {
        msg_error("Can't choose pixel format.");
    }
    SetPixelFormat(dc, win_pf, &win_pfd);

    // create legacy context, required for wglGetProcAddress to work properly
    glrc = wglCreateContext(dc);
    if (!glrc) {
        msg_error("Can't create OpenGL context.");
    }
    wglMakeCurrent(dc, glrc);

    // attributes for a 3.3 core profile without all the legacy stuff
    GLint attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };

    // create the actual context
    glrc_core = wglCreateContextAttribsARB(dc, glrc, attribs);
    if (glrc_core) {
        wglMakeCurrent(dc, glrc_core);
    } else {
        // rendering probably still works with the legacy context, so just send
        // a warning
        msg_warning("Can't create OpenGL 3.3 core context.");
    }

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
    glUseProgram(program);

    // prepare dummy VAO
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // prepare texture
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // check if there was an error when using any of the commands above
    gl_check_errors();
}

static void screen_get_buffer(int width, int height, int display_height, int** buffer, int* pitch)
{
    // check if the framebuffer size has changed
    if (tex_width != width || tex_height != height) {
        // reallocate and clear local buffer
        size_t tex_buffer_size = width * height * TEX_BYTES_PER_PIXEL;
        tex_buffer = realloc(tex_buffer, tex_buffer_size);
        memset(tex_buffer, 0, tex_buffer_size);

        // reallocate texture buffer on GPU, but don't download anything yet
        glTexImage2D(GL_TEXTURE_2D, 0, TEX_INTERNAL_FORMAT, width,
            height, 0, TEX_FORMAT, TEX_TYPE, 0);

        screen_update_size(width, display_height);

        msg_debug("screen: resized framebuffer texture: %dx%d -> %dx%d",
            tex_width, tex_height, width, height);

        tex_width = width;
        tex_height = height;
    }

    // texture may have non-square pixels, so save the display height separately
    tex_display_height = display_height;

    *buffer = (int*)tex_buffer;
    *pitch = width * TEX_BYTES_PER_PIXEL;
}

static void screen_swap(void)
{
    // clear current buffer, indicating the start of a new frame
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // copy local buffer to GPU texture buffer
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tex_width, tex_height,
        TEX_FORMAT, TEX_TYPE, tex_buffer);

    RECT rect;
    GetClientRect(gfx.hWnd, &rect);

    // status bar covers the client area, so exclude it from calculation
    RECT statusrect;
    SetRectEmpty(&statusrect);

    if (gfx.hStatusBar) {
        GetClientRect(gfx.hStatusBar, &statusrect);
        rect.bottom -= statusrect.bottom;
    }

    int32_t vp_width = rect.right - rect.left;
    int32_t vp_height = rect.bottom - rect.top;

    // default to bottom left corner of the window above the status bar
    int32_t vp_x = 0;
    int32_t vp_y = statusrect.bottom;

    int32_t hw = tex_display_height * vp_width;
    int32_t wh = tex_width * vp_height;

    // add letterboxes or pillarboxes if the window has a different aspect ratio
    // than the current display mode
    if (hw > wh) {
        int32_t w_max = wh / tex_display_height;
        vp_x += (vp_width - w_max) / 2;
        vp_width = w_max;
    } else if (hw < wh) {
        int32_t h_max = hw / tex_width;
        vp_y += (vp_height - h_max) / 2;
        vp_height = h_max;
    }

    // configure viewport
    glViewport(vp_x, vp_y, vp_width, vp_height);

    // draw fullscreen triangle
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // swap front and back buffers
    SwapBuffers(dc);

    // check if there was an error when using any of the commands above
    gl_check_errors();
}

static void screen_set_fullscreen(bool _fullscreen)
{
    static HMENU old_menu;
    static LONG old_style;
    static WINDOWPLACEMENT old_pos;

    if (_fullscreen) {
        // hide curser
        ShowCursor(FALSE);

        // hide status bar
        if (gfx.hStatusBar) {
            ShowWindow(gfx.hStatusBar, SW_HIDE);
        }

        // disable menu and save it to restore it later
        old_menu = GetMenu(gfx.hWnd);
        if (old_menu) {
            SetMenu(gfx.hWnd, NULL);
        }

        // save old window position and size
        GetWindowPlacement(gfx.hWnd, &old_pos);

        // use virtual screen dimensions for fullscreen mode
        int vs_width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int vs_height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

        // disable all styles to get a borderless window and save it to restore
        // it later
        old_style = GetWindowLong(gfx.hWnd, GWL_STYLE);
        SetWindowLong(gfx.hWnd, GWL_STYLE, WS_VISIBLE);

        // resize window so it covers the entire virtual screen
        SetWindowPos(gfx.hWnd, HWND_TOP, 0, 0, vs_width, vs_height, SWP_SHOWWINDOW);
    } else {
        // restore cursor
        ShowCursor(TRUE);

        // restore status bar
        if (gfx.hStatusBar) {
            ShowWindow(gfx.hStatusBar, SW_SHOW);
        }

        // restore menu
        if (old_menu) {
            SetMenu(gfx.hWnd, old_menu);
            old_menu = NULL;
        }

        // restore style
        SetWindowLong(gfx.hWnd, GWL_STYLE, old_style);

        // restore window size and position
        SetWindowPlacement(gfx.hWnd, &old_pos);
    }

    fullscreen = _fullscreen;
}

static bool screen_get_fullscreen(void)
{
    return fullscreen;
}

static void screen_capture(char* path)
{
    msg_debug("screen: writing screenshot to '%s'", path);

    // prepare bitmap headers
    uint32_t pitch = tex_width * TEX_BYTES_PER_PIXEL;
    uint32_t img_size = tex_height * pitch;

    BITMAPINFOHEADER ihdr = {0};
    ihdr.biSize = sizeof(ihdr);
    ihdr.biWidth = tex_width;
    ihdr.biHeight = tex_height;
    ihdr.biPlanes = 1;
    ihdr.biBitCount = 32;
    ihdr.biSizeImage = img_size;

    // calculate aspect ratio for non-square pixel buffers
    if (tex_height != tex_display_height) {
        ihdr.biXPelsPerMeter = 2835; // 72 DPI × 39.3701 inches per meter
        ihdr.biYPelsPerMeter = (ihdr.biXPelsPerMeter * tex_height) / tex_display_height;
    }

    BITMAPFILEHEADER fhdr = {0};
    fhdr.bfType = 'B' | ('M' << 8);
    fhdr.bfOffBits = sizeof(fhdr) + sizeof(ihdr) + 10;
    fhdr.bfSize = img_size + fhdr.bfOffBits;

    FILE* fp = fopen(path, "wb");

    // write bitmap headers
    fwrite(&fhdr, sizeof(fhdr), 1, fp);
    fwrite(&ihdr, sizeof(ihdr), 1, fp);

    // write bitmap contents
    fseek(fp, fhdr.bfOffBits, SEEK_SET);

    uint8_t* buffer = (uint8_t*) tex_buffer;
    for (int32_t y = (tex_height - 1); y >= 0; y--) {
        fwrite(buffer + pitch * y, pitch, 1, fp);
    }

    fclose(fp);
}

static void screen_close(void)
{
    if (tex_buffer) {
        free(tex_buffer);
        tex_buffer = NULL;
    }

    tex_width = 0;
    tex_height = 0;

    glDeleteTextures(1, &texture);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program);

    if (glrc_core) {
        wglDeleteContext(glrc_core);
    }

    wglDeleteContext(glrc);
}

void screen_opengl(struct screen_api* api)
{
    api->init = screen_init;
    api->swap = screen_swap;
    api->get_buffer = screen_get_buffer;
    api->set_fullscreen = screen_set_fullscreen;
    api->get_fullscreen = screen_get_fullscreen;
    api->capture = screen_capture;
    api->close = screen_close;
}
