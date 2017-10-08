#include "config.h"
#include "core/version.h"

#include <Windows.h>

#define REG_PATH "SOFTWARE\\" CORE_SIMPLE_NAME

static bool config_set_int(HKEY key, const char* name, int32_t value)
{
    LSTATUS res = RegSetValueEx(key, name, 0, REG_DWORD, (LPBYTE)&value, sizeof(DWORD));
    return res == ERROR_SUCCESS;
}

static int32_t config_get_int(HKEY key, const char* name, int32_t value_default)
{
    DWORD value;
    DWORD type;
    DWORD size;
    LSTATUS res = RegQueryValueEx(key, name, 0, &type, (LPBYTE)&value, &size);

    if (res == ERROR_SUCCESS && type == REG_DWORD && size == sizeof(DWORD)) {
        return value;
    } else {
        return value_default;
    }
}

bool config_load(struct core_config* config)
{
    HKEY key;
    LSTATUS res = RegCreateKeyEx(HKEY_CURRENT_USER, REG_PATH, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ, NULL, &key, NULL);
    if (res != ERROR_SUCCESS) {
        return false;
    }

    config->num_workers = config_get_int(key, "NumWorkers", 0);
    config->vi.mode = config_get_int(key, "ViMode", VI_MODE_NORMAL);
    config->vi.widescreen = config_get_int(key, "ViWidescreen", 0);

    RegCloseKey(key);
    return true;
}

bool config_save(struct core_config* config)
{
    HKEY key;
    LONG res = RegCreateKeyEx(HKEY_CURRENT_USER, REG_PATH, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &key, NULL);
    if (res != ERROR_SUCCESS) {
        return false;
    }

    config_set_int(key, "NumWorkers", config->num_workers);
    config_set_int(key, "ViMode", config->vi.mode);
    config_set_int(key, "ViWidescreen", config->vi.widescreen);

    RegCloseKey(key);
    return true;
}
