#include "n64video.h"
#include "rdp.h"
#include "common.h"
#include "plugin.h"
#include "msg.h"
#include "screen.h"
#include "parallel.h"

#include <memory.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define CLAMP(x, lo, hi) (((x) > (hi)) ? (hi) : (((x) < (lo)) ? (lo) : (x)))

#define SIGN16(x)   ((int16_t)(x))
#define SIGN8(x)    ((int8_t)(x))

#define SIGN(x, numb)	(((x) & ((1 << (numb)) - 1)) | -((x) & (1 << ((numb) - 1))))
#define SIGNF(x, numb)	((x) | -((x) & (1 << ((numb) - 1))))

#define TRELATIVE(x, y)     ((x) - ((y) << 3))

#define PIXELS_TO_BYTES(pix, siz) (((pix) << (siz)) >> 1)

// RGBA5551 to RGBA8888 helper
#define RGBA16_R(x) (((x) >> 8) & 0xf8)
#define RGBA16_G(x) (((x) & 0x7c0) >> 3)
#define RGBA16_B(x) (((x) & 0x3e) << 2)

// RGBA8888 helper
#define RGBA32_R(x) (((x) >> 24) & 0xff)
#define RGBA32_G(x) (((x) >> 16) & 0xff)
#define RGBA32_B(x) (((x) >> 8) & 0xff)
#define RGBA32_A(x) ((x) & 0xff)

// inlining
#define INLINE inline

#ifdef _MSC_VER
#define STRICTINLINE __forceinline
#elif defined(__GNUC__)
#define STRICTINLINE __attribute__((always_inline))
#else
#define STRICTINLINE inline
#endif

static struct rdp_state* rdp_states;
static struct n64video_config config;
static struct n64video_config config_new;
static struct plugin_api* plugin;

static bool init_lut;
static bool config_update;

static struct
{
    bool fillmbitcrashes, vbusclock, nolerp;
} onetimewarnings;

static int rdp_pipeline_crashed = 0;

static STRICTINLINE int32_t clamp(int32_t value, int32_t min, int32_t max)
{
    if (value < min)
        return min;
    else if (value > max)
        return max;
    else
        return value;
}

static STRICTINLINE uint32_t irand(uint32_t* state)
{
    // based on xorshift32 implementation on Wikipedia
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

#include "rdp/rdp.c"
#include "vi/vi.c"

void n64video_config_defaults(struct n64video_config* config)
{
    config->parallel = true;
    config->num_workers = 0;
    config->vi.interp = VI_INTERP_NEAREST;
    config->vi.mode = VI_MODE_NORMAL;
    config->vi.widescreen = false;
    config->vi.hide_overscan = false;
}

void rdp_init_worker(uint32_t worker_id)
{
    int i;
    struct rdp_state* rdp = &rdp_states[worker_id];
    memset(rdp, 0, sizeof(*rdp));

    rdp->worker_id = worker_id;
    rdp->rand_dp = rdp->rand_vi = 3 + worker_id * 13;

    uint32_t tmp[2] = {0};
    rdp_set_other_modes(rdp, tmp);

    for (i = 0; i < 8; i++)
    {
        calculate_tile_derivs(&rdp->tile[i]);
        calculate_clamp_diffs(&rdp->tile[i]);
    }

    fb_init(rdp);
    combiner_init(rdp);
    tex_init(rdp);
    rasterizer_init(rdp);
}

void n64video_init(struct n64video_config* _config)
{
    if (_config) {
        config = *_config;
    }

    // initialize static lookup tables, once is enough
    if (!init_lut) {
        blender_init_lut();
        coverage_init_lut();
        combiner_init_lut();
        tex_init_lut();
        z_init_lut();

        init_lut = true;
    }

    // init externals
    screen_init(_config);
    plugin_init();

    // init internals
    rdram_init();
    vi_init();
    cmd_init();

    rdp_pipeline_crashed = 0;
    memset(&onetimewarnings, 0, sizeof(onetimewarnings));

    if (config.parallel) {
        parallel_init(config.num_workers);
        rdp_states = malloc(parallel_num_workers() * sizeof(struct rdp_state));
        parallel_run(rdp_init_worker);
    } else {
        rdp_states = malloc(sizeof(struct rdp_state));
        rdp_init_worker(0);
    }
}

void n64video_update_config(struct n64video_config* config)
{
    // updating the config directly would be dangerous and can cause crashes,
    // so wait for the next sync_full before applying it
    config_new = *config;
    config_update = true;
}

void rdp_invalid(struct rdp_state* rdp, const uint32_t* args)
{
}

void rdp_noop(struct rdp_state* rdp, const uint32_t* args)
{
}

void rdp_sync_load(struct rdp_state* rdp, const uint32_t* args)
{
}

void rdp_sync_pipe(struct rdp_state* rdp, const uint32_t* args)
{
}

void rdp_sync_tile(struct rdp_state* rdp, const uint32_t* args)
{
}

void rdp_sync_full(struct rdp_state* rdp, const uint32_t* args)
{
    // update config if set
    if (config_update) {
        n64video_close();
        n64video_init(&config_new);

        config_update = false;
    }

    // signal plugin to handle interrupts
    plugin_sync_dp();
}

void rdp_set_other_modes(struct rdp_state* rdp, const uint32_t* args)
{
    rdp->other_modes.cycle_type          = (args[0] >> 20) & 3;
    rdp->other_modes.persp_tex_en        = (args[0] >> 19) & 1;
    rdp->other_modes.detail_tex_en       = (args[0] >> 18) & 1;
    rdp->other_modes.sharpen_tex_en      = (args[0] >> 17) & 1;
    rdp->other_modes.tex_lod_en          = (args[0] >> 16) & 1;
    rdp->other_modes.en_tlut             = (args[0] >> 15) & 1;
    rdp->other_modes.tlut_type           = (args[0] >> 14) & 1;
    rdp->other_modes.sample_type         = (args[0] >> 13) & 1;
    rdp->other_modes.mid_texel           = (args[0] >> 12) & 1;
    rdp->other_modes.bi_lerp0            = (args[0] >> 11) & 1;
    rdp->other_modes.bi_lerp1            = (args[0] >> 10) & 1;
    rdp->other_modes.convert_one         = (args[0] >>  9) & 1;
    rdp->other_modes.key_en              = (args[0] >>  8) & 1;
    rdp->other_modes.rgb_dither_sel      = (args[0] >>  6) & 3;
    rdp->other_modes.alpha_dither_sel    = (args[0] >>  4) & 3;
    rdp->other_modes.blend_m1a_0         = (args[1] >> 30) & 3;
    rdp->other_modes.blend_m1a_1         = (args[1] >> 28) & 3;
    rdp->other_modes.blend_m1b_0         = (args[1] >> 26) & 3;
    rdp->other_modes.blend_m1b_1         = (args[1] >> 24) & 3;
    rdp->other_modes.blend_m2a_0         = (args[1] >> 22) & 3;
    rdp->other_modes.blend_m2a_1         = (args[1] >> 20) & 3;
    rdp->other_modes.blend_m2b_0         = (args[1] >> 18) & 3;
    rdp->other_modes.blend_m2b_1         = (args[1] >> 16) & 3;
    rdp->other_modes.force_blend         = (args[1] >> 14) & 1;
    rdp->other_modes.alpha_cvg_select    = (args[1] >> 13) & 1;
    rdp->other_modes.cvg_times_alpha     = (args[1] >> 12) & 1;
    rdp->other_modes.z_mode              = (args[1] >> 10) & 3;
    rdp->other_modes.cvg_dest            = (args[1] >>  8) & 3;
    rdp->other_modes.color_on_cvg        = (args[1] >>  7) & 1;
    rdp->other_modes.image_read_en       = (args[1] >>  6) & 1;
    rdp->other_modes.z_update_en         = (args[1] >>  5) & 1;
    rdp->other_modes.z_compare_en        = (args[1] >>  4) & 1;
    rdp->other_modes.antialias_en        = (args[1] >>  3) & 1;
    rdp->other_modes.z_source_sel        = (args[1] >>  2) & 1;
    rdp->other_modes.dither_alpha_en     = (args[1] >>  1) & 1;
    rdp->other_modes.alpha_compare_en    = (args[1] >>  0) & 1;

    set_blender_input(rdp, 0, 0, &rdp->blender1a_r[0], &rdp->blender1a_g[0], &rdp->blender1a_b[0], &rdp->blender1b_a[0],
                      rdp->other_modes.blend_m1a_0, rdp->other_modes.blend_m1b_0);
    set_blender_input(rdp, 0, 1, &rdp->blender2a_r[0], &rdp->blender2a_g[0], &rdp->blender2a_b[0], &rdp->blender2b_a[0],
                      rdp->other_modes.blend_m2a_0, rdp->other_modes.blend_m2b_0);
    set_blender_input(rdp, 1, 0, &rdp->blender1a_r[1], &rdp->blender1a_g[1], &rdp->blender1a_b[1], &rdp->blender1b_a[1],
                      rdp->other_modes.blend_m1a_1, rdp->other_modes.blend_m1b_1);
    set_blender_input(rdp, 1, 1, &rdp->blender2a_r[1], &rdp->blender2a_g[1], &rdp->blender2a_b[1], &rdp->blender2b_a[1],
                      rdp->other_modes.blend_m2a_1, rdp->other_modes.blend_m2b_1);

    rdp->other_modes.f.stalederivs = 1;
}

void n64video_close(void)
{
    vi_close();
    parallel_close();
    plugin_close();
    screen_close();

    if (rdp_states) {
        free(rdp_states);
        rdp_states = NULL;
    }
}
