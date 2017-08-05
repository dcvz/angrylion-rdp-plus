#include "core.h"

#include "rdp.h"
#include "vi.h"
#include "screen_headless.h"
#include "rdram.h"
#include "file.h"
#include "msg.h"
#include "trace_write.h"
#include "parallel_c.hpp"

#include <stdio.h>
#include <string.h>

static uint32_t screenshot_index;
static uint32_t trace_num_workers;
static uint32_t trace_index;
static uint32_t num_workers;

static struct core_config* config;
static struct screen_api screen;
static struct plugin_api plugin;

void core_init(struct core_config* _config)
{
    config = _config;

    // use headless mode if no screen adapter has been defined
    if (!config->screen_api) {
        config->screen_api = screen_headless;
    }

    config->screen_api(&screen);
    screen.init();

    if (!config->plugin_api) {
        msg_error("core: plugin API not defined!");
    }

    config->plugin_api(&plugin);
    plugin.init();

    rdram_init(&plugin);

    rdp_init(config, &plugin);
    vi_init(config, &plugin, &screen);

    num_workers = config->num_workers;

    if (num_workers != 1) {
        parallel_init(num_workers);
    }

    screenshot_index = 0;
    trace_index = 0;
}

void core_update(void)
{
    // open trace file when tracing has been enabled with no file open
    if (config->trace && !trace_write_is_open()) {
        // get ROM name from plugin and use placeholder if empty
        char rom_name[32];
        if (!plugin.get_rom_name(rom_name, sizeof(rom_name))) {
            strcpy_s(rom_name, sizeof(rom_name), "trace");
        }

        // generate trace path
        char trace_path[256];
        file_path_indexed(trace_path, sizeof(trace_path), ".", rom_name,
            "dpt", &trace_index);

        trace_write_open(trace_path);
        trace_write_header(plugin.get_rdram_size());
        trace_num_workers = config->num_workers;
        config->num_workers = 1;
    }

    // close trace file when tracing has been disabled
    if (!config->trace && trace_write_is_open()) {
        trace_write_close();
        config->num_workers = trace_num_workers;
    }

    // update number of workers
    if (config->num_workers != num_workers) {
        num_workers = config->num_workers;
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

void core_screenshot(char* directory)
{
    // get ROM name from plugin and use placeholder if empty
    char rom_name[32];
    if (!plugin.get_rom_name(rom_name, sizeof(rom_name))) {
        strcpy_s(rom_name, sizeof(rom_name), "screenshot");
    }

    // generate and find an unused file path
    char path[256];
    if (file_path_indexed(path, sizeof(path), directory, rom_name, "bmp", &screenshot_index)) {
        screen.capture(path);
    }
}

void core_toggle_fullscreen(void)
{
    screen.set_fullscreen(!screen.get_fullscreen());
}

void core_close(void)
{
    parallel_close();
    vi_close();
    plugin.close();
    screen.close();
    if (trace_write_is_open()) {
        trace_write_close();
    }
}
