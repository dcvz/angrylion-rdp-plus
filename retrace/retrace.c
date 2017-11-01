#include "plugin.h"
#include "clock.h"
#include "argparse.h"

#include "core/core.h"
#include "core/rdp.h"
#include "core/vi.h"
#include "core/trace_read.h"
#include "core/screen.h"

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#ifdef _WIN32
#include <Windows.h>
#endif

#ifdef RETRACE_SDL

#ifdef _WIN32
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#endif

static struct core_config config;

static bool loop;
static bool benchmark;
static uint64_t start;
static uint32_t dp_frames;
static uint32_t vi_frames;

void retrace_stats(void)
{
    uint64_t now = millis();

    float seconds_passed = (now - start) / 1000.f;
    float dp_fps = dp_frames / seconds_passed;
    float vi_fps = vi_frames / seconds_passed;

    printf("Render time: %.2fs\n", seconds_passed);
    printf("DP: %d frames, %.1f avg FPS\n", dp_frames, dp_fps);
    printf("VI: %d frames, %.1f avg FPS\n\n", vi_frames, vi_fps);

    start = millis();
}

void retrace_reset(void)
{
    dp_frames = 0;
    vi_frames = 0;
    trace_read_reset();
}

bool retrace_frame(void)
{
    while (1) {
        char id = trace_read_id();

        switch (id) {
            case TRACE_EOF:
                if (benchmark) {
                    retrace_stats();
                }

                if (loop) {
                    retrace_reset();
                }

                return loop;

            case TRACE_CMD: {
                uint32_t cmd[CMD_MAX_INTS];
                uint32_t length;
                trace_read_cmd(cmd, &length);

                rdp_cmd(cmd, length);

                if (CMD_ID(cmd) == CMD_ID_SYNC_FULL) {
                    dp_frames++;
                    return true;
                }

                break;
            }

            case TRACE_RDRAM:
                trace_read_rdram();
                break;

            case TRACE_VI:
                trace_read_vi(plugin_get_vi_registers());
                vi_update();
                vi_frames++;
                break;
        }
    }
}

void retrace_frames(void)
{
    bool run = true;
    bool pause = false;
    start = millis();

    while (run) {
        bool render = !pause;

#ifdef RETRACE_SDL
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_KEYDOWN:
                    switch (event.key.keysym.sym) {
                        // toggle fullscreen mode
                        case SDLK_RETURN: {
                            if (SDL_GetModState() & KMOD_ALT) {
                                screen_toggle_fullscreen();
                                break;
                            }
                        }

                        // toggle pause mode
                        case SDLK_PAUSE:
                            render = pause = !pause;
                            break;

                        // render one frame while paused
                        case SDLK_SPACE:
                            if (pause) {
                                render = true;
                            }
                            break;
                    }
                    break;

                case SDL_QUIT:
                    run = false;
                    break;
            }
        }
#endif

        if (render) {
            if (!retrace_frame()) {
                run = false;
            }
        } else {
            screen_swap();
        }
    }
}

int main(int argc, char** argv)
{
    char usage[80];
    snprintf(usage, sizeof(usage), "%s [options] <trace file>", argv[0]);
    char* usages[] = {usage, NULL};

    static struct argparse_option options[] = {
        OPT_HELP(),
        OPT_GROUP("Retracing options"),
        OPT_BOOLEAN('l', "loop", &loop, "restart retracing after rendering the last frame"),
        OPT_BOOLEAN('b', "benchmark", &benchmark, "display elapsed time after rendering the last frame"),
        OPT_GROUP("Rendering options"),
        OPT_BOOLEAN('p', "parallel", &config.parallel, "enables multithreaded parallel rendering"),
        OPT_INTEGER('t', "workers", &config.num_workers, "set the number of workers used for multithreading (0 = auto)"),
        OPT_GROUP("Video interface options"),
        OPT_INTEGER('v', "vimode", &config.vi.mode, "set VI mode (0 = filtered, 1 = unfiltered, 2 = depth, 3 = coverage)"),
        OPT_BOOLEAN('o', "overscan", &config.vi.overscan, "enable overscan output"),
        OPT_BOOLEAN('w', "widescreen", &config.vi.overscan, "force 16:9 aspect ratio"),
        OPT_END(),
    };

    struct argparse argparse;
    argparse_init(&argparse, options, usages, 0);
    argparse_describe(&argparse, "\nPlays back a recorded RDP trace file (*.dpt).", "\n");
    argc = argparse_parse(&argparse, argc, argv);

    if (argc < 1) {
        argparse_usage(&argparse);
        return EXIT_FAILURE;
    }

    char* trace_path = argv[argc - 1];

    if (!trace_read_open(trace_path)) {
        fprintf(stderr, "Can't open trace file '%s'.\n", trace_path);
        return EXIT_FAILURE;
    }

    uint32_t rdram_size;
    trace_read_header(&rdram_size);
    plugin_set_rdram_size(rdram_size);

    core_init(&config);
    retrace_frames();
    core_close();
    trace_read_close();

    return EXIT_SUCCESS;
}
