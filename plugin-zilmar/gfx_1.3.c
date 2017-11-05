#include "gfx_1.3.h"
#include "config.h"
#include "resource.h"

#include "core/core.h"
#include "core/screen.h"
#include "core/version.h"
#include "core/msg.h"
#include "core/plugin.h"

#include <stdio.h>

static bool warn_hle;
static HINSTANCE hinst;

GFX_INFO gfx;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            config_init(hinstDLL);
            break;
    }
    return TRUE;
}

EXPORT void CALL CaptureScreen(char* directory)
{
    core_screenshot(directory);
}

EXPORT void CALL ChangeWindow(void)
{
    screen_toggle_fullscreen();
}

EXPORT void CALL CloseDLL(void)
{
}

EXPORT void CALL DllAbout(HWND hParent)
{
    msg_warning(
        CORE_NAME "\n\n"
        "Build commit:\n"
        GIT_BRANCH "\n"
        GIT_COMMIT_HASH "\n"
        GIT_COMMIT_DATE "\n\n"
        "Build date: " __DATE__ " " __TIME__ "\n\n"
        "https://github.com/ata4/angrylion-rdp-plus"
    );
}

EXPORT void CALL DllConfig(HWND hParent)
{
    config_dialog(hParent);
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
    sprintf(PluginInfo->Name, CORE_NAME);

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
    core_dp_update();
}

EXPORT void CALL RomClosed(void)
{
    core_close();
}

EXPORT void CALL RomOpen(void)
{
    config_load();
    core_init(config_get());
}

EXPORT void CALL ShowCFB(void)
{
}

EXPORT void CALL UpdateScreen(void)
{
    core_vi_update();
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
