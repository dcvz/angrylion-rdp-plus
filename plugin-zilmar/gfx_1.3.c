#include "gfx_1.3.h"
#include "core.h"
#include "parallel_c.hpp"
#include "plugin.h"
#include "msg.h"
#include "rdram.h"
#include "screen_opengl.h"

#include <stdio.h>

#define PLUGIN_BASE_NAME "angrylion's RDP Plus"

#ifdef _DEBUG
#define PLUGIN_NAME PLUGIN_BASE_NAME " (Debug)"
#else
#define PLUGIN_NAME PLUGIN_BASE_NAME
#endif

static bool warn_hle;
static struct core_config config;

GFX_INFO gfx;

EXPORT void CALL CaptureScreen(char* directory)
{
    char rom_name[32];
    if (plugin_rom_name(rom_name, sizeof(rom_name))) {
        core_screenshot(directory, rom_name);
    }
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
    config.screen_api = screen_opengl;
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
