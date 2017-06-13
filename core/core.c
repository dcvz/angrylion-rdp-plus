#include "core.h"

#include "rdp.h"
#include "vi.h"
#include "screen.h"
#include "rdram.h"
#include "plugin.h"
#include "parallel_c.hpp"

#include <stdio.h>

static bool fullscreen;
static int32_t screenshot_id;
static struct core_config* cfg;

static bool file_exists(char* path)
{
    FILE* fp = fopen(path, "rb");

    if (!fp) {
        return false;
    }

    fclose(fp);
    return true;
}

void core_init(struct core_config* config)
{
    cfg = config;
    if (!cfg->headless) {
        screen_init();
    }
    plugin_init();
    rdram_init();
    parallel_init(0);
    rdp_init(config);
    vi_init(config);

    screenshot_id = 0;
}

void core_update_dp(void)
{
    rdp_update();
}

void core_update_vi(void)
{
    vi_update();
}

void core_screenshot(char* directory, char* name)
{
    if (cfg->headless) {
        return;
    }

    // generate and find an unused file path
    char path[256];
    do {
        sprintf_s(path, sizeof(path), "%s\\%s_%04d.bmp", directory, name,
            screenshot_id++);
    } while (file_exists(path));

    screen_capture(path);
}

void core_toggle_fullscreen(void)
{
    if (!cfg->headless) {
        fullscreen = !fullscreen;
        screen_set_full(fullscreen);
    }
}

void core_close(void)
{
    parallel_close();
    plugin_close();
    vi_close();
    if (!cfg->headless) {
        screen_close();
    }
}
