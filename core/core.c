#include "core.h"

#include "rdp.h"
#include "vi.h"
#include "screen.h"
#include "rdram.h"
#include "plugin.h"
#include "file.h"
#include "trace_write.h"
#include "parallel_c.hpp"

#include <stdio.h>

static bool fullscreen;
static uint32_t screenshot_index;
static struct core_config* cfg;

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

    screenshot_index = 0;
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
    if (file_path_indexed(path, sizeof(path), directory, name, "bmp", &screenshot_index)) {
        screen_capture(path);
    }
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
    if (trace_write_is_open()) {
        trace_write_close();
    }
}
