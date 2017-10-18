#include "config.h"
#include "resource.h"

#include "core/version.h"

#include <Commctrl.h>
#include <Shlwapi.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SECTION_GENERAL "General"
#define SECTION_VIDEO_INTERFACE "VideoInterface"

#define KEY_GEN_NUM_WORKERS "num_workers"

#define KEY_VI_MODE "mode"
#define KEY_VI_WIDESCREEN "widescreen"
#define KEY_VI_OVERSCAN "overscan"

#define CONFIG_FILE_NAME CORE_SIMPLE_NAME "-config.ini"

static HINSTANCE inst;
static struct core_config config;
static char config_path[MAX_PATH + 1];

static HWND dlg_combo_vi_mode;
static HWND dlg_check_trace;
static HWND dlg_check_vi_widescreen;
static HWND dlg_spin_workers;

INT_PTR CALLBACK config_dialog_proc(HWND hwnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
    switch (iMessage) {
        case WM_INITDIALOG: {
            SetWindowText(hwnd, CORE_BASE_NAME " Config");

            config_load();

            TCHAR vi_mode_strings[VI_MODE_NUM][16] = {
                TEXT("Filtered"),   // VI_MODE_NORMAL
                TEXT("Unfiltered"), // VI_MODE_COLOR
                TEXT("Depth"),      // VI_MODE_DEPTH
                TEXT("Coverage")    // VI_MODE_COVERAGE
            };

            dlg_combo_vi_mode = GetDlgItem(hwnd, IDC_COMBO_VI_MODE);
            SendMessage(dlg_combo_vi_mode, CB_RESETCONTENT, 0, 0);
            for (int i = 0; i < VI_MODE_NUM; i++) {
                SendMessage(dlg_combo_vi_mode, CB_ADDSTRING, i, (LPARAM)vi_mode_strings[i]);
            }
            SendMessage(dlg_combo_vi_mode, CB_SETCURSEL, (WPARAM)config.vi.mode, 0);

            dlg_check_trace = GetDlgItem(hwnd, IDC_CHECK_TRACE);
            SendMessage(dlg_check_trace, BM_SETCHECK, (WPARAM)config.dp.trace_record, 0);

            dlg_check_vi_widescreen = GetDlgItem(hwnd, IDC_CHECK_VI_WIDESCREEN);
            SendMessage(dlg_check_vi_widescreen, BM_SETCHECK, (WPARAM)config.vi.widescreen, 0);

            SetDlgItemInt(hwnd, IDC_EDIT_WORKERS, config.num_workers, FALSE);

            dlg_spin_workers = GetDlgItem(hwnd, IDC_SPIN_WORKERS);
            SendMessage(dlg_spin_workers, UDM_SETRANGE, 0, MAKELPARAM(128, 0));
            break;
        }
        case WM_COMMAND: {
            WORD cmdid = LOWORD(wParam);
            switch (cmdid) {
                case IDOK:
                case IDAPPLY:
                    config.vi.mode = SendMessage(dlg_combo_vi_mode, CB_GETCURSEL, 0, 0);
                    config.vi.widescreen = SendMessage(dlg_check_vi_widescreen, BM_GETCHECK, 0, 0);
                    config.dp.trace_record = SendMessage(dlg_check_trace, BM_GETCHECK, 0, 0);
                    config.num_workers = GetDlgItemInt(hwnd, IDC_EDIT_WORKERS, FALSE, FALSE);

                    core_update_config(&config);
                    config_save();

                    // don't close dialog if "Apply" was pressed
                    if (cmdid == IDAPPLY) {
                        break;
                    }
                case IDCANCEL:
                    EndDialog(hwnd, 0);
                    break;
            }
            break;
        }
        default:
            return FALSE;
    }
    return TRUE;
}

static void config_handle(const char* key, const char* value, const char* section)
{
    if (!_strcmpi(section, SECTION_GENERAL)) {
        if (!_strcmpi(key, KEY_GEN_NUM_WORKERS)) {
            config.num_workers = strtoul(value, NULL, 0);
        }
    } else if (!_strcmpi(section, SECTION_VIDEO_INTERFACE)) {
        if (!_strcmpi(key, KEY_VI_MODE)) {
            config.vi.mode = strtol(value, NULL, 0);
        } else if (!_strcmpi(key, KEY_VI_WIDESCREEN)) {
            config.vi.widescreen = strtol(value, NULL, 0) != 0;
        } else if (!_strcmpi(key, KEY_VI_OVERSCAN)) {
            config.vi.overscan = strtol(value, NULL, 0) != 0;
        }
    }
}

void config_init(HINSTANCE hInst)
{
    inst = hInst;
    config_path[0] = 0;
    GetModuleFileName(inst, config_path, sizeof(config_path));
    PathRemoveFileSpec(config_path);
    PathAppend(config_path, CONFIG_FILE_NAME);
}

void config_dialog(HWND hParent)
{
    DialogBox(inst, MAKEINTRESOURCE(IDD_DIALOG1), hParent, config_dialog_proc);
}

struct core_config* config_get(void)
{
    return &config;
}

bool config_load()
{
    FILE* fp = fopen(config_path, "r");
    if (!fp) {
        return false;
    }

    char line[128];
    char section[128];
    while (fgets(line, sizeof(line), fp) != NULL) {
        // remove newline characters
        size_t trim_pos = strcspn(line, "\r\n");
        line[trim_pos] = 0;

        // ignore blank lines
        size_t len = strlen(line);
        if (!len) {
            continue;
        }

        // key-values
        char* eq_ptr = strchr(line, '=');
        if (eq_ptr) {
            *eq_ptr = 0;
            char* key = line;
            char* value = eq_ptr + 1;
            config_handle(key, value, section);
            continue;
        }

        // sections
        if (line[0] == '[' && line[len - 1] == ']') {
            section[0] = 0;
            strncat(section, line + 1, len - 2);
            continue;
        }
    }

    fclose(fp);

    return true;
}

static void config_write_section(FILE* fp, const char* section)
{
    fprintf(fp, "[%s]\n", section);
}

static void config_write_uint32(FILE* fp, const char* key, uint32_t value)
{
    fprintf(fp, "%s=%u\n", key, value);
}

static void config_write_int32(FILE* fp, const char* key, int32_t value)
{
    fprintf(fp, "%s=%d\n", key, value);
}

bool config_save(void)
{
    FILE* fp = fopen(config_path, "w");
    if (!fp) {
        return false;
    }

    config_write_section(fp, SECTION_GENERAL);
    config_write_uint32(fp, KEY_GEN_NUM_WORKERS, config.num_workers);
    fputs("\n", fp);

    config_write_section(fp, SECTION_VIDEO_INTERFACE);
    config_write_int32(fp, KEY_VI_MODE, config.vi.mode);
    config_write_int32(fp, KEY_VI_WIDESCREEN, config.vi.widescreen);
    config_write_int32(fp, KEY_VI_OVERSCAN, config.vi.overscan);

    fclose(fp);

    return true;
}
