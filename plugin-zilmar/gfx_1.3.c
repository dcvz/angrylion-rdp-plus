#include "gfx_1.3.h"
#include "core.h"
#include "parallel_c.hpp"
#include "plugin.h"
#include "msg.h"
#include "rdram.h"

#include <stdio.h>
#include <ctype.h>

#define PLUGIN_BASE_NAME "angrylion's RDP Plus"

#ifdef _DEBUG
#define PLUGIN_NAME PLUGIN_BASE_NAME " (Debug)"
#else
#define PLUGIN_NAME PLUGIN_BASE_NAME
#endif

static bool warn_hle;
static struct core_config config;

GFX_INFO gfx;

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

    core_screenshot(directory, rom_name);
}

EXPORT void CALL ChangeWindow(void)
{
    core_toggle_fullscreen();
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
    core_update_dp();
}

EXPORT void CALL RomClosed(void)
{
    core_close();
}

EXPORT void CALL RomOpen(void)
{
    config.parallel = true;

    core_init(&config);
}

EXPORT void CALL ShowCFB(void)
{
}

EXPORT void CALL UpdateScreen(void)
{
    core_update_vi();
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
