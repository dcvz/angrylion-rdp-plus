#include "gfx_1.3.h"
#include "rdp.h"
#include "vi.h"
#include "parallel_c.hpp"
#include "plugin.h"
#include "msg.h"
#include "screen.h"
#include "rdram.h"

#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>

#define PLUGIN_BASE_NAME "angrylion's RDP Plus"

#ifdef _DEBUG
#define PLUGIN_NAME PLUGIN_BASE_NAME " (Debug)"
#else
#define PLUGIN_NAME PLUGIN_BASE_NAME
#endif

static bool warn_hle;
static bool fullscreen;
static int32_t screenshot_id;

GFX_INFO gfx;

static bool file_exists(char* path)
{
    FILE* fp = fopen(path, "rb");

    if (!fp) {
        return false;
    }

    fclose(fp);
    return true;
}

static char filter_char(char c)
{
    if (isalnum(c) || c == '_' || c == '-' || c == '.') {
        return c;
    } else {
        return ' ';
    }
}

EXPORT void CALL CaptureScreen(char* directory)
{
    // copy game name from ROM header, which is encoded in Shift_JIS.
    // most games just use the ASCII subset, so filter out the rest.
    char rom_name[21];
    int i;
    for (i = 0; i < 20; i++) {
        rom_name[i] = filter_char(gfx.HEADER[(32 + i) ^ BYTE_ADDR_XOR]);
    }

    // make sure there's at least one whitespace that will terminate the string
    // below
    rom_name[i] = ' ';

    // trim trailing whitespaces
    for (; i > 0; i--) {
        if (rom_name[i] != ' ') {
            break;
        }
        rom_name[i] = 0;
    }

    // game title is empty or invalid, use safe fallback using the four-character
    // game ID
    if (i == 0) {
        for (; i < 4; i++) {
            rom_name[i] = filter_char(gfx.HEADER[(59 + i) ^ BYTE_ADDR_XOR]);
        }
        rom_name[i] = 0;
    }

    // generate and find an unused file path
    char path[MAX_PATH];
    do {
        sprintf_s(path, sizeof(path), "%s/%s_%04d.bmp", directory, rom_name,
            screenshot_id++);
    } while (file_exists(path));

    screen_capture(path);
}

EXPORT void CALL ChangeWindow(void)
{
    fullscreen = !fullscreen;
    screen_set_full(fullscreen);
}

EXPORT void CALL CloseDLL(void)
{
}

EXPORT void CALL DllAbout(HWND hParent)
{
    msg_warning(PLUGIN_NAME ". MESS source code used.");
}

EXPORT void CALL ReadScreen(void **dest, long *width, long *height)
{
}

EXPORT void CALL DrawScreen(void)
{
}

EXPORT void CALL GetDllInfo(PLUGIN_INFO* PluginInfo)
{
    PluginInfo->Version = 0x0103;
    PluginInfo->Type  = PLUGIN_TYPE_GFX;
    sprintf(PluginInfo->Name, PLUGIN_NAME);

    PluginInfo->NormalMemory = TRUE;
    PluginInfo->MemoryBswaped = TRUE;
}

EXPORT BOOL CALL InitiateGFX(GFX_INFO Gfx_Info)
{
    gfx = Gfx_Info;

    return TRUE;
}

EXPORT void CALL MoveScreen(int xpos, int ypos)
{
}

EXPORT void CALL ProcessDList(void)
{
    if (!warn_hle) {
        msg_warning("Please disable 'Graphic HLE' in the plugin settings.");
        warn_hle = true;
    }
}

EXPORT void CALL ProcessRDPList(void)
{
    rdp_update();
}

EXPORT void CALL RomClosed(void)
{
    parallel_close();
    screen_close();
    plugin_close();
}

EXPORT void CALL RomOpen(void)
{
    // game name might have changed, so reset the screenshot ID
    screenshot_id = 0;

    screen_init();
    plugin_init();
    rdram_init();
    parallel_init(0);

    struct rdp_config rdp_config;
    rdp_config.parallel = true;
    rdp_init(rdp_config);

    struct vi_config vi_config;
    vi_config.parallel = true;
    vi_config.tv_fading = false;
    vi_init(vi_config);
}

EXPORT void CALL ShowCFB(void)
{
}

EXPORT void CALL UpdateScreen(void)
{
    vi_update();
}

EXPORT void CALL ViStatusChanged(void)
{
}

EXPORT void CALL ViWidthChanged(void)
{
}

EXPORT void CALL FBWrite(DWORD addr, DWORD val)
{
}

EXPORT void CALL FBWList(FrameBufferModifyEntry *plist, DWORD size)
{
}

EXPORT void CALL FBRead(DWORD addr)
{
}

EXPORT void CALL FBGetFrameBufferInfo(void *pinfo)
{
}
