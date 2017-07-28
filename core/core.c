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
#include <string.h>

static bool fullscreen;
static uint32_t screenshot_index;
static uint32_t trace_num_workers;
static uint32_t trace_index;
static uint32_t num_workers;

static struct core_config* cfg;

void core_init(struct core_config* config)
{
    cfg = config;

    if (!cfg->headless) {
        screen_init();
    }

    plugin_init();
    rdram_init();

    rdp_init(config);
    vi_init(config);

    num_workers = cfg->num_workers;

    if (num_workers != 1) {
        parallel_init(num_workers);
    }

    screenshot_index = 0;
    trace_index = 0;
}

void core_update(void)
{
    // open trace file when tracing has been enabled with no file open
    if (cfg->trace && !trace_write_is_open()) {
        // get ROM name from plugin and use placeholder if empty
        char rom_name[32];
        if (!plugin_rom_name(rom_name, sizeof(rom_name))) {
            strcpy_s(rom_name, sizeof(rom_name), "trace");
        }

        // generate trace path
        char trace_path[256];
        file_path_indexed(trace_path, sizeof(trace_path), ".", rom_name,
            "dpt", &trace_index);

        trace_write_open(trace_path);
        trace_write_header(plugin_rdram_size());
        trace_num_workers = cfg->num_workers;
        cfg->num_workers = 1;
    }

    // close trace file when tracing has been disabled
    if (!cfg->trace && trace_write_is_open()) {
        trace_write_close();
        cfg->num_workers = trace_num_workers;
    }

    // update number of workers
    if (cfg->num_workers != num_workers) {
        num_workers = cfg->num_workers;
        parallel_close();

        if (num_workers != 1) {
            parallel_init(num_workers);
        }
    }
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
