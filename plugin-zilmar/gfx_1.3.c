#include "gfx_1.3.h"
#include "config.h"
#include "resource.h"

#include "core/core.h"
#include "core/screen.h"
#include "core/version.h"
#include "core/parallel_c.hpp"
#include "core/msg.h"
#include "core/rdram.h"
#include "core/plugin.h"
#include "core/file.h"

#include <Commctrl.h>
#include <Shlwapi.h>
#include <stdio.h>

#define CONFIG_FILE_NAME CORE_SIMPLE_NAME "-config.ini"

static bool warn_hle;
static struct core_config config;
static char config_path[MAX_PATH + 1];
static HINSTANCE hinst;

GFX_INFO gfx;

static void get_config_path(void)
{
    config_path[0] = 0;
    GetModuleFileName(hinst, config_path, sizeof(config_path));
    PathRemoveFileSpec(config_path);
    PathAppend(config_path, CONFIG_FILE_NAME);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            hinst = hinstDLL;
            get_config_path();
            break;
    }
    return TRUE;
}

INT_PTR CALLBACK ConfigDialogProc(HWND hwnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
    switch (iMessage) {
        case WM_INITDIALOG: {
            SetWindowText(hwnd, CORE_BASE_NAME " Config");

            config_load(&config, config_path);

            TCHAR vi_mode_strings[VI_MODE_NUM][16] = {
                TEXT("Filtered"),   // VI_MODE_NORMAL
                TEXT("Unfiltered"), // VI_MODE_COLOR
                TEXT("Depth"),      // VI_MODE_DEPTH
                TEXT("Coverage")    // VI_MODE_COVERAGE
            };

            HWND hCombo1 = GetDlgItem(hwnd, IDC_COMBO_VI_MODE);
            SendMessage(hCombo1, CB_RESETCONTENT, 0, 0);
            for (int i = 0; i < VI_MODE_NUM; i++) {
                SendMessage(hCombo1, CB_ADDSTRING, i, (LPARAM)vi_mode_strings[i]);
            }
            SendMessage(hCombo1, CB_SETCURSEL, (WPARAM)config.vi.mode, 0);

            HWND hCheckTrace = GetDlgItem(hwnd, IDC_CHECK_TRACE);
            SendMessage(hCheckTrace, BM_SETCHECK, (WPARAM)config.dp.trace_record, 0);

            HWND hCheckWidescreen = GetDlgItem(hwnd, IDC_CHECK_VI_WIDESCREEN);
            SendMessage(hCheckWidescreen, BM_SETCHECK, (WPARAM)config.vi.widescreen, 0);

            SetDlgItemInt(hwnd, IDC_EDIT_WORKERS, config.num_workers, FALSE);

            HWND hSpin1 = GetDlgItem(hwnd, IDC_SPIN_WORKERS);
            SendMessage(hSpin1, UDM_SETRANGE, 0, MAKELPARAM(128, 0));
            break;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK: {
                    HWND hCombo1 = GetDlgItem(hwnd, IDC_COMBO_VI_MODE);
                    config.vi.mode = SendMessage(hCombo1, CB_GETCURSEL, 0, 0);

                    HWND hCheckWidescreen = GetDlgItem(hwnd, IDC_CHECK_VI_WIDESCREEN);
                    config.vi.widescreen = SendMessage(hCheckWidescreen, BM_GETCHECK, 0, 0);

                    HWND hCheckTrace = GetDlgItem(hwnd, IDC_CHECK_TRACE);
                    config.dp.trace_record = SendMessage(hCheckTrace, BM_GETCHECK, 0, 0);

                    config.num_workers = GetDlgItemInt(hwnd, IDC_EDIT_WORKERS, FALSE, FALSE);

                    core_update_config(&config);

                    config_save(&config, config_path);
                }
                case IDCANCEL:
                    EndDialog(hwnd, 0);
                    break;
            }
            break;
        default:
            return FALSE;
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
    DialogBox(hinst, MAKEINTRESOURCE(IDD_DIALOG1), hParent, ConfigDialogProc);
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
    core_update_dp();
}

EXPORT void CALL RomClosed(void)
{
    core_close();
}

EXPORT void CALL RomOpen(void)
{
    config_load(&config, config_path);
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
