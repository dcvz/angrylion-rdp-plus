#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SECTION_GENERAL "General"
#define SECTION_VIDEO_INTERFACE "VideoInterface"

#define KEY_GEN_NUM_WORKERS "num_workers"

#define KEY_VI_MODE "mode"
#define KEY_VI_WIDESCREEN "widescreen"
#define KEY_VI_OVERSCAN "overscan"

static void config_handle(struct core_config* config, const char* key, const char* value, const char* section)
{
    if (!_strcmpi(section, SECTION_GENERAL)) {
        if (!_strcmpi(key, KEY_GEN_NUM_WORKERS)) {
            config->num_workers = strtoul(value, NULL, 0);
        }
    } else if (!_strcmpi(section, SECTION_VIDEO_INTERFACE)) {
        if (!_strcmpi(key, KEY_VI_MODE)) {
            config->vi.mode = strtol(value, NULL, 0);
        } else if (!_strcmpi(key, KEY_VI_WIDESCREEN)) {
            config->vi.widescreen = strtol(value, NULL, 0) != 0;
        } else if (!_strcmpi(key, KEY_VI_OVERSCAN)) {
            config->vi.overscan = strtol(value, NULL, 0) != 0;
        }
    }
}

bool config_load(struct core_config* config, const char* path)
{
    FILE* fp = fopen(path, "r");
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
            config_handle(config, key, value, section);
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

bool config_save(struct core_config* config, const char* path)
{
    FILE* fp = fopen(path, "w");
    if (!fp) {
        return false;
    }

    config_write_section(fp, SECTION_GENERAL);
    config_write_uint32(fp, KEY_GEN_NUM_WORKERS, config->num_workers);
    fputs("\n", fp);

    config_write_section(fp, SECTION_VIDEO_INTERFACE);
    config_write_int32(fp, KEY_VI_MODE, config->vi.mode);
    config_write_int32(fp, KEY_VI_WIDESCREEN, config->vi.widescreen);
    config_write_int32(fp, KEY_VI_OVERSCAN, config->vi.overscan);

    fclose(fp);

    return true;
}
