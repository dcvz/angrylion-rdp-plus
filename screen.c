#include "screen.h"
#include "msg.h"
#include "gl_core_3_3.h"
#include "wgl_ext.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#pragma comment (lib, "opengl32.lib")

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
static bool tex_buffer_resize;
static int32_t tex_width;
static int32_t tex_height;
static int32_t tex_display_width;
static int32_t tex_display_height;

// context states
static HDC dc;
static HGLRC glrc;
static HGLRC glrc_core;
static HWND gfx_hwnd;
static HWND gfx_hwnd_status;
static bool prev_fullscreen;

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

void screen_init(GFX_INFO* info)
{
    gfx_hwnd = info->hWnd;
    gfx_hwnd_status = info->hStatusBar;

    if (!prev_fullscreen) {
        // make window resizable for the user
        LONG style = GetWindowLong(gfx_hwnd, GWL_STYLE);
        style |= WS_SIZEBOX | WS_MAXIMIZEBOX;
        SetWindowLong(gfx_hwnd, GWL_STYLE, style);

        // resize window to 640x480
        RECT rect;
        GetWindowRect(gfx_hwnd, &rect);

        rect.right = rect.left + WINDOW_DEFAULT_WIDTH;
        rect.bottom = rect.top + WINDOW_DEFAULT_HEIGHT;

        if (gfx_hwnd_status) {
            RECT statusrect;
            GetClientRect(gfx_hwnd_status, &statusrect);
            rect.bottom += statusrect.bottom - statusrect.top;
        }

        AdjustWindowRect(&rect, style, FALSE);
        SetWindowPos(gfx_hwnd, HWND_TOP, rect.left, rect.top,
            rect.right - rect.left, rect.bottom - rect.top, SWP_SHOWWINDOW);
    }

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

    dc = GetDC(gfx_hwnd);
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
    GLint attribs[] =
    {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
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

void screen_get_buffer(int width, int height, int display_width, int display_height, int** buffer, int* pitch)
{
    // check if the texture memory needs to be re-allocated to change the size
    tex_buffer_resize = tex_width != width || tex_height != height;

    if (tex_buffer_resize) {
        tex_buffer = realloc(tex_buffer, width * height * TEX_BYTES_PER_PIXEL);
    }

    tex_width = width;
    tex_height = height;
    tex_display_width = display_width;
    tex_display_height = display_height;

    *buffer = (int*)tex_buffer;
    *pitch = width * TEX_BYTES_PER_PIXEL;
}

void screen_swap(void)
{
    // copy local framebuffer to texture
    if (tex_buffer_resize) {
        // texture size has changed, create new texture
        glTexImage2D(GL_TEXTURE_2D, 0, TEX_INTERNAL_FORMAT, tex_width,
            tex_height, 0, TEX_FORMAT, TEX_TYPE, tex_buffer);
        tex_buffer_resize = false;
    } else {
        // same texture size, just update data to reduce overhead
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tex_width, tex_height,
            TEX_FORMAT, TEX_TYPE, tex_buffer);
    }

    RECT rect;
    GetClientRect(gfx_hwnd, &rect);

    int32_t vp_width = rect.right - rect.left;
    int32_t vp_height = rect.bottom - rect.top;

    // default to bottom left corner of the window
    int32_t vp_x = 0;
    int32_t vp_y = 0;

    int32_t hw = tex_display_height * vp_width;
    int32_t wh = tex_display_width * vp_height;

    // add letterboxes or pillarboxes if the window has a different aspect ratio
    // than the current display mode
    if (hw > wh) {
        int32_t w_max = wh / tex_display_height;
        vp_x = (vp_width - w_max) / 2;
        vp_width = w_max;
    } else if (hw < wh) {
        int32_t h_max = hw / tex_display_width;
        vp_y = (vp_height - h_max) / 2;
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

void screen_set_full(bool fullscreen)
{
    static HMENU old_menu;
    static LONG old_style;
    static RECT old_rect;

    if (fullscreen) {
        // hide curser
        ShowCursor(FALSE);

        // hide status bar
        if (gfx_hwnd_status) {
            ShowWindow(gfx_hwnd_status, SW_HIDE);
        }

        // disable menu and save it to restore it later
        old_menu = GetMenu(gfx_hwnd);
        if (old_menu) {
            SetMenu(gfx_hwnd, NULL);
        }

        // save old window position and size
        GetWindowRect(gfx_hwnd, &old_rect);

        // use virtual screen dimensions for fullscreen mode
        int vs_width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int vs_height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

        // disable all styles to get a borderless window and save it to restore
        // it later
        old_style = GetWindowLong(gfx_hwnd, GWL_STYLE);
        SetWindowLong(gfx_hwnd, GWL_STYLE, WS_VISIBLE);

        // resize window so it covers the entire virtual screen
        SetWindowPos(gfx_hwnd, HWND_TOP, 0, 0, vs_width, vs_height, SWP_SHOWWINDOW);
    } else {
        // restore cursor
        ShowCursor(TRUE);

        // restore status bar
        if (gfx_hwnd_status) {
            ShowWindow(gfx_hwnd_status, SW_SHOW);
        }

        // restore menu
        if (old_menu) {
            SetMenu(gfx_hwnd, old_menu);
            old_menu = NULL;
        }

        // restore style
        SetWindowLong(gfx_hwnd, GWL_STYLE, old_style);

        // restore window size and position
        SetWindowPos(gfx_hwnd, HWND_TOP, old_rect.left, old_rect.top,
            old_rect.right - old_rect.left, old_rect.bottom - old_rect.top,
            SWP_SHOWWINDOW);
    }

    prev_fullscreen = fullscreen;
}

void screen_capture(char* path)
{
    // prepare bitmap headers
    size_t pitch = tex_width * TEX_BYTES_PER_PIXEL;
    size_t img_size = tex_height * pitch;

    BITMAPINFOHEADER ihdr = {0};
    ihdr.biSize = sizeof(ihdr);
    ihdr.biWidth = tex_width;
    ihdr.biHeight = tex_height;
    ihdr.biPlanes = 1;
    ihdr.biBitCount = 32;
    ihdr.biSizeImage = img_size;

    // calculate aspect ratio
    const int32_t ppmbase = 2835; // 72 DPI × 39.3701 inches per meter
    ihdr.biXPelsPerMeter = (ppmbase * tex_width) / tex_display_width;
    ihdr.biYPelsPerMeter = (ppmbase * tex_height) / tex_display_height;

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

void screen_close(void)
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
