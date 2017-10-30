#include "rdp.h"
#include "vi.h"
#include "common.h"
#include "plugin.h"
#include "rdram.h"
#include "trace_write.h"
#include "msg.h"
#include "irand.h"
#include "file.h"
#include "parallel_c.hpp"

#include <memory.h>
#include <string.h>
#include <stdlib.h>

#define SIGN16(x)   ((int16_t)(x))
#define SIGN8(x)    ((int8_t)(x))


#define SIGN(x, numb)   (((x) & ((1 << numb) - 1)) | -((x) & (1 << (numb - 1))))
#define SIGNF(x, numb)  ((x) | -((x) & (1 << (numb - 1))))

#define TRELATIVE(x, y)     ((x) - ((y) << 3))




// bit constants for DP_STATUS
#define DP_STATUS_XBUS_DMA      0x001   // DMEM DMA mode is set
#define DP_STATUS_FREEZE        0x002   // Freeze has been set
#define DP_STATUS_FLUSH         0x004   // Flush has been set
#define DP_STATUS_START_GCLK    0x008   // Unknown
#define DP_STATUS_TMEM_BUSY     0x010   // TMEM is in use on the RDP
#define DP_STATUS_PIPE_BUSY     0x020   // Graphics pipe is in use on the RDP
#define DP_STATUS_CMD_BUSY      0x040   // RDP is currently executing a command
#define DP_STATUS_CBUF_BUSY     0x080   // RDRAM RDP command buffer is in use
#define DP_STATUS_DMA_BUSY      0x100   // DMEM RDP command buffer is in use
#define DP_STATUS_END_VALID     0x200   // Unknown
#define DP_STATUS_START_VALID   0x400   // Unknown

static struct core_config* config;
static struct plugin_api* plugin;

static TLS int blshifta = 0, blshiftb = 0, pastblshifta = 0, pastblshiftb = 0;

static TLS struct
{
    int lx, rx;
    int unscrx;
    int validline;
    int32_t r, g, b, a, s, t, w, z;
    int32_t majorx[4];
    int32_t minorx[4];
    int32_t invalyscan[4];
} span[1024];

// span states
static TLS struct
{
    int ds;
    int dt;
    int dw;
    int dr;
    int dg;
    int db;
    int da;
    int dz;
    int dzpix;

    int drdy;
    int dgdy;
    int dbdy;
    int dady;
    int dzdy;
    int cdr;
    int cdg;
    int cdb;
    int cda;
    int cdz;

    int dsdy;
    int dtdy;
    int dwdy;
} spans;



struct color
{
    int32_t r, g, b, a;
};

struct fbcolor
{
    uint8_t r, g, b;
};

struct rectangle
{
    uint16_t xl, yl, xh, yh;
};

struct tex_rectangle
{
    int tilenum;
    uint16_t xl, yl, xh, yh;
    int16_t s, t;
    int16_t dsdx, dtdy;
    uint32_t flip;
};

struct other_modes
{
    int cycle_type;
    int persp_tex_en;
    int detail_tex_en;
    int sharpen_tex_en;
    int tex_lod_en;
    int en_tlut;
    int tlut_type;
    int sample_type;
    int mid_texel;
    int bi_lerp0;
    int bi_lerp1;
    int convert_one;
    int key_en;
    int rgb_dither_sel;
    int alpha_dither_sel;
    int blend_m1a_0;
    int blend_m1a_1;
    int blend_m1b_0;
    int blend_m1b_1;
    int blend_m2a_0;
    int blend_m2a_1;
    int blend_m2b_0;
    int blend_m2b_1;
    int force_blend;
    int alpha_cvg_select;
    int cvg_times_alpha;
    int z_mode;
    int cvg_dest;
    int color_on_cvg;
    int image_read_en;
    int z_update_en;
    int z_compare_en;
    int antialias_en;
    int z_source_sel;
    int dither_alpha_en;
    int alpha_compare_en;
    struct {
        int stalederivs;
        int dolod;
        int partialreject_1cycle;
        int partialreject_2cycle;
        int rgb_alpha_dither;
        int realblendershiftersneeded;
        int interpixelblendershiftersneeded;
        int getditherlevel;
        int textureuselevel0;
        int textureuselevel1;
    } f;
};



#define PIXEL_SIZE_4BIT         0
#define PIXEL_SIZE_8BIT         1
#define PIXEL_SIZE_16BIT        2
#define PIXEL_SIZE_32BIT        3

#define CYCLE_TYPE_1            0
#define CYCLE_TYPE_2            1
#define CYCLE_TYPE_COPY         2
#define CYCLE_TYPE_FILL         3


#define FORMAT_RGBA             0
#define FORMAT_YUV              1
#define FORMAT_CI               2
#define FORMAT_IA               3
#define FORMAT_I                4


#define TEXEL_RGBA4             0
#define TEXEL_RGBA8             1
#define TEXEL_RGBA16            2
#define TEXEL_RGBA32            3
#define TEXEL_YUV4              4
#define TEXEL_YUV8              5
#define TEXEL_YUV16             6
#define TEXEL_YUV32             7
#define TEXEL_CI4               8
#define TEXEL_CI8               9
#define TEXEL_CI16              0xa
#define TEXEL_CI32              0xb
#define TEXEL_IA4               0xc
#define TEXEL_IA8               0xd
#define TEXEL_IA16              0xe
#define TEXEL_IA32              0xf
#define TEXEL_I4                0x10
#define TEXEL_I8                0x11
#define TEXEL_I16               0x12
#define TEXEL_I32               0x13



static TLS struct other_modes other_modes;

static TLS struct color combined_color;
static TLS struct color texel0_color;
static TLS struct color texel1_color;
static TLS struct color nexttexel_color;
static TLS struct color shade_color;
static TLS int32_t noise = 0;
static TLS int32_t primitive_lod_frac = 0;
static int32_t one_color = 0x100;
static int32_t zero_color = 0x00;

static TLS struct color pixel_color;
static TLS struct color memory_color;
static TLS struct color pre_memory_color;

static TLS struct tile
{
    int format;
    int size;
    int line;
    int tmem;
    int palette;
    int ct, mt, cs, ms;
    int mask_t, shift_t, mask_s, shift_s;

    uint16_t sl, tl, sh, th;

    struct
    {
        int clampdiffs, clampdifft;
        int clampens, clampent;
        int masksclamped, masktclamped;
        int notlutswitch, tlutswitch;
    } f;
} tile[8];

#define PIXELS_TO_BYTES(pix, siz) (((pix) << (siz)) >> 1)

struct spansigs {
    int startspan;
    int endspan;
    int preendspan;
    int nextspan;
    int midspan;
    int longspan;
    int onelessthanmid;
};

static void deduce_derivatives(struct rdp_state* rdp);

static TLS int32_t k0_tf = 0, k1_tf = 0, k2_tf = 0, k3_tf = 0;
static TLS int32_t k4 = 0, k5 = 0;
static TLS int32_t lod_frac = 0;

static struct
{
    int copymstrangecrashes, fillmcrashes, fillmbitcrashes, syncfullcrash;
} onetimewarnings;

static TLS uint32_t max_level = 0;
static TLS int32_t min_level = 0;
static int rdp_pipeline_crashed = 0;

static STRICTINLINE int32_t clamp(int32_t value,int32_t min,int32_t max)
{
    if (value < min)
        return min;
    else if (value > max)
        return max;
    else
        return value;
}

struct rdp_state
{
    uint32_t worker_id;

    int blshifta;
    int blshiftb;
    int pastblshifta;
    int pastblshiftb;

    struct
    {
        int lx, rx;
        int unscrx;
        int validline;
        int32_t r, g, b, a, s, t, w, z;
        int32_t majorx[4];
        int32_t minorx[4];
        int32_t invalyscan[4];
    } span[1024];

    // span states
    struct
    {
        int ds;
        int dt;
        int dw;
        int dr;
        int dg;
        int db;
        int da;
        int dz;
        int dzpix;

        int drdy;
        int dgdy;
        int dbdy;
        int dady;
        int dzdy;
        int cdr;
        int cdg;
        int cdb;
        int cda;
        int cdz;

        int dsdy;
        int dtdy;
        int dwdy;
    } spans;

    struct other_modes other_modes;

    struct color combined_color;
    struct color texel0_color;
    struct color texel1_color;
    struct color nexttexel_color;
    struct color shade_color;
    int32_t noise;
    int32_t primitive_lod_frac;

    struct color pixel_color;
    struct color memory_color;
    struct color pre_memory_color;

    struct
    {
        int format;
        int size;
        int line;
        int tmem;
        int palette;
        int ct, mt, cs, ms;
        int mask_t, shift_t, mask_s, shift_s;

        uint16_t sl, tl, sh, th;

        struct
        {
            int clampdiffs, clampdifft;
            int clampens, clampent;
            int masksclamped, masktclamped;
            int notlutswitch, tlutswitch;
        } f;
    } tile[8];

    int32_t k0_tf;
    int32_t k1_tf;
    int32_t k2_tf;
    int32_t k3_tf;
    int32_t k4;
    int32_t k5;
    int32_t lod_frac;

    uint32_t max_level;
    int32_t min_level;

    // irand
    int32_t iseed;

    // blender
    struct
    {
        int32_t *i1a_r[2];
        int32_t *i1a_g[2];
        int32_t *i1a_b[2];
        int32_t *i1b_a[2];
        int32_t *i2a_r[2];
        int32_t *i2a_g[2];
        int32_t *i2a_b[2];
        int32_t *i2b_a[2];
    } blender;

    struct color blend_color;
    struct color fog_color;
    struct color inv_pixel_color;
    struct color blended_pixel_color;

    // combiner
    struct
    {
        int sub_a_rgb0;
        int sub_b_rgb0;
        int mul_rgb0;
        int add_rgb0;
        int sub_a_a0;
        int sub_b_a0;
        int mul_a0;
        int add_a0;

        int sub_a_rgb1;
        int sub_b_rgb1;
        int mul_rgb1;
        int add_rgb1;
        int sub_a_a1;
        int sub_b_a1;
        int mul_a1;
        int add_a1;
    } combine;

    struct
    {
        int32_t *rgbsub_a_r[2];
        int32_t *rgbsub_a_g[2];
        int32_t *rgbsub_a_b[2];
        int32_t *rgbsub_b_r[2];
        int32_t *rgbsub_b_g[2];
        int32_t *rgbsub_b_b[2];
        int32_t *rgbmul_r[2];
        int32_t *rgbmul_g[2];
        int32_t *rgbmul_b[2];
        int32_t *rgbadd_r[2];
        int32_t *rgbadd_g[2];
        int32_t *rgbadd_b[2];

        int32_t *alphasub_a[2];
        int32_t *alphasub_b[2];
        int32_t *alphamul[2];
        int32_t *alphaadd[2];
    } combiner;

    struct color prim_color;
    struct color env_color;
    struct color key_scale;
    struct color key_center;
    struct color key_width;

    int32_t keyalpha;

    // coverage
    uint8_t cvgbuf[1024];

    // fbuffer
    void (*fbread1_ptr)(uint32_t, uint32_t*);
    void (*fbread2_ptr)(uint32_t, uint32_t*);
    void (*fbwrite_ptr)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

    int fb_format;
    int fb_size;
    int fb_width;
    uint32_t fb_address;
    uint32_t fill_color;

    // rasterizer
    struct rectangle clip;
    int scfield;
    int sckeepodd;

    uint32_t primitive_z;
    uint16_t primitive_delta_z;

    // tcoord
    void (*tcdiv_ptr)(int32_t, int32_t, int32_t, int32_t*, int32_t*);

    // tex
    int ti_format;
    int ti_size;
    int ti_width;
    uint32_t ti_address;

    // tmem
    uint8_t tmem[0x1000];

    // zbuffer
    uint32_t zb_address;
    int32_t pastrawdzmem;
};

struct rdp_state* rdp_states;

#include "rdp/cmd.c"
#include "rdp/dither.c"
#include "rdp/blender.c"
#include "rdp/combiner.c"
#include "rdp/coverage.c"
#include "rdp/zbuffer.c"
#include "rdp/fbuffer.c"
#include "rdp/tex.c"
#include "rdp/rasterizer.c"

void rdp_init_worker(uint32_t worker_id)
{
    struct rdp_state* rdp = &rdp_states[worker_id];
    rdp->worker_id = worker_id;

    uint32_t tmp[2] = {0};
    rdp_set_other_modes(rdp, tmp);

    for (int i = 0; i < 8; i++)
    {
        calculate_tile_derivs(rdp, i);
        calculate_clamp_diffs(rdp, i);
    }

    precalc_cvmask_derivatives(rdp);
    z_init(rdp);
    dither_init(rdp);
    fb_init(rdp);
    blender_init(rdp);
    combiner_init(rdp);
    tex_init(rdp);
    rasterizer_init(rdp);
}

int rdp_init(struct core_config* _config)
{
    config = _config;

    memset(&combined_color, 0, sizeof(struct color));
    memset(&prim_color, 0, sizeof(struct color));
    memset(&env_color, 0, sizeof(struct color));
    memset(&key_scale, 0, sizeof(struct color));
    memset(&key_center, 0, sizeof(struct color));

    memset(tile, 0, sizeof(tile));

    rdp_pipeline_crashed = 0;
    memset(&onetimewarnings, 0, sizeof(onetimewarnings));

    if (config->parallel) {
        rdp_states = calloc(parallel_worker_num(), sizeof(struct rdp_state));
        parallel_run(rdp_init_worker);
    } else {
        rdp_states = calloc(1, sizeof(struct rdp_state));
        rdp_init_worker(0);
    }

    return 0;
}

static void rdp_invalid(struct rdp_state* rdp, const uint32_t* args)
{
}

static void rdp_noop(struct rdp_state* rdp, const uint32_t* args)
{
}

static void rdp_sync_load(struct rdp_state* rdp, const uint32_t* args)
{
}

static void rdp_sync_pipe(struct rdp_state* rdp, const uint32_t* args)
{
}

static void rdp_sync_tile(struct rdp_state* rdp, const uint32_t* args)
{
}

static void rdp_sync_full(struct rdp_state* rdp, const uint32_t* args)
{
    core_dp_sync();
}

static void rdp_set_other_modes(struct rdp_state* rdp, const uint32_t* args)
{
    other_modes.cycle_type          = (args[0] >> 20) & 3;
    other_modes.persp_tex_en        = (args[0] >> 19) & 1;
    other_modes.detail_tex_en       = (args[0] >> 18) & 1;
    other_modes.sharpen_tex_en      = (args[0] >> 17) & 1;
    other_modes.tex_lod_en          = (args[0] >> 16) & 1;
    other_modes.en_tlut             = (args[0] >> 15) & 1;
    other_modes.tlut_type           = (args[0] >> 14) & 1;
    other_modes.sample_type         = (args[0] >> 13) & 1;
    other_modes.mid_texel           = (args[0] >> 12) & 1;
    other_modes.bi_lerp0            = (args[0] >> 11) & 1;
    other_modes.bi_lerp1            = (args[0] >> 10) & 1;
    other_modes.convert_one         = (args[0] >>  9) & 1;
    other_modes.key_en              = (args[0] >>  8) & 1;
    other_modes.rgb_dither_sel      = (args[0] >>  6) & 3;
    other_modes.alpha_dither_sel    = (args[0] >>  4) & 3;
    other_modes.blend_m1a_0         = (args[1] >> 30) & 3;
    other_modes.blend_m1a_1         = (args[1] >> 28) & 3;
    other_modes.blend_m1b_0         = (args[1] >> 26) & 3;
    other_modes.blend_m1b_1         = (args[1] >> 24) & 3;
    other_modes.blend_m2a_0         = (args[1] >> 22) & 3;
    other_modes.blend_m2a_1         = (args[1] >> 20) & 3;
    other_modes.blend_m2b_0         = (args[1] >> 18) & 3;
    other_modes.blend_m2b_1         = (args[1] >> 16) & 3;
    other_modes.force_blend         = (args[1] >> 14) & 1;
    other_modes.alpha_cvg_select    = (args[1] >> 13) & 1;
    other_modes.cvg_times_alpha     = (args[1] >> 12) & 1;
    other_modes.z_mode              = (args[1] >> 10) & 3;
    other_modes.cvg_dest            = (args[1] >>  8) & 3;
    other_modes.color_on_cvg        = (args[1] >>  7) & 1;
    other_modes.image_read_en       = (args[1] >>  6) & 1;
    other_modes.z_update_en         = (args[1] >>  5) & 1;
    other_modes.z_compare_en        = (args[1] >>  4) & 1;
    other_modes.antialias_en        = (args[1] >>  3) & 1;
    other_modes.z_source_sel        = (args[1] >>  2) & 1;
    other_modes.dither_alpha_en     = (args[1] >>  1) & 1;
    other_modes.alpha_compare_en    = (args[1] >>  0) & 1;

    set_blender_input(rdp, 0, 0, &blender.i1a_r[0], &blender.i1a_g[0], &blender.i1a_b[0], &blender.i1b_a[0],
                      other_modes.blend_m1a_0, other_modes.blend_m1b_0);
    set_blender_input(rdp, 0, 1, &blender.i2a_r[0], &blender.i2a_g[0], &blender.i2a_b[0], &blender.i2b_a[0],
                      other_modes.blend_m2a_0, other_modes.blend_m2b_0);
    set_blender_input(rdp, 1, 0, &blender.i1a_r[1], &blender.i1a_g[1], &blender.i1a_b[1], &blender.i1b_a[1],
                      other_modes.blend_m1a_1, other_modes.blend_m1b_1);
    set_blender_input(rdp, 1, 1, &blender.i2a_r[1], &blender.i2a_g[1], &blender.i2a_b[1], &blender.i2b_a[1],
                      other_modes.blend_m2a_1, other_modes.blend_m2b_1);

    other_modes.f.stalederivs = 1;
}

static void deduce_derivatives(struct rdp_state* rdp)
{
    int special_bsel0, special_bsel1;


    other_modes.f.partialreject_1cycle = (blender.i2b_a[0] == &inv_pixel_color.a && blender.i1b_a[0] == &pixel_color.a);
    other_modes.f.partialreject_2cycle = (blender.i2b_a[1] == &inv_pixel_color.a && blender.i1b_a[1] == &pixel_color.a);


    special_bsel0 = (blender.i2b_a[0] == &memory_color.a);
    special_bsel1 = (blender.i2b_a[1] == &memory_color.a);


    other_modes.f.realblendershiftersneeded = (special_bsel0 && other_modes.cycle_type == CYCLE_TYPE_1) || (special_bsel1 && other_modes.cycle_type == CYCLE_TYPE_2);
    other_modes.f.interpixelblendershiftersneeded = (special_bsel0 && other_modes.cycle_type == CYCLE_TYPE_2);

    other_modes.f.rgb_alpha_dither = (other_modes.rgb_dither_sel << 2) | other_modes.alpha_dither_sel;

    tcdiv_ptr = tcdiv_func[other_modes.persp_tex_en];


    int texel1_used_in_cc1 = 0, texel0_used_in_cc1 = 0, texel0_used_in_cc0 = 0, texel1_used_in_cc0 = 0;
    int texels_in_cc0 = 0, texels_in_cc1 = 0;
    int lod_frac_used_in_cc1 = 0, lod_frac_used_in_cc0 = 0;

    if ((combiner.rgbmul_r[1] == &lod_frac) || (combiner.alphamul[1] == &lod_frac))
        lod_frac_used_in_cc1 = 1;
    if ((combiner.rgbmul_r[0] == &lod_frac) || (combiner.alphamul[0] == &lod_frac))
        lod_frac_used_in_cc0 = 1;

    if (combiner.rgbmul_r[1] == &texel1_color.r || combiner.rgbsub_a_r[1] == &texel1_color.r || combiner.rgbsub_b_r[1] == &texel1_color.r || combiner.rgbadd_r[1] == &texel1_color.r || \
        combiner.alphamul[1] == &texel1_color.a || combiner.alphasub_a[1] == &texel1_color.a || combiner.alphasub_b[1] == &texel1_color.a || combiner.alphaadd[1] == &texel1_color.a || \
        combiner.rgbmul_r[1] == &texel1_color.a)
        texel1_used_in_cc1 = 1;
    if (combiner.rgbmul_r[1] == &texel0_color.r || combiner.rgbsub_a_r[1] == &texel0_color.r || combiner.rgbsub_b_r[1] == &texel0_color.r || combiner.rgbadd_r[1] == &texel0_color.r || \
        combiner.alphamul[1] == &texel0_color.a || combiner.alphasub_a[1] == &texel0_color.a || combiner.alphasub_b[1] == &texel0_color.a || combiner.alphaadd[1] == &texel0_color.a || \
        combiner.rgbmul_r[1] == &texel0_color.a)
        texel0_used_in_cc1 = 1;
    if (combiner.rgbmul_r[0] == &texel1_color.r || combiner.rgbsub_a_r[0] == &texel1_color.r || combiner.rgbsub_b_r[0] == &texel1_color.r || combiner.rgbadd_r[0] == &texel1_color.r || \
        combiner.alphamul[0] == &texel1_color.a || combiner.alphasub_a[0] == &texel1_color.a || combiner.alphasub_b[0] == &texel1_color.a || combiner.alphaadd[0] == &texel1_color.a || \
        combiner.rgbmul_r[0] == &texel1_color.a)
        texel1_used_in_cc0 = 1;
    if (combiner.rgbmul_r[0] == &texel0_color.r || combiner.rgbsub_a_r[0] == &texel0_color.r || combiner.rgbsub_b_r[0] == &texel0_color.r || combiner.rgbadd_r[0] == &texel0_color.r || \
        combiner.alphamul[0] == &texel0_color.a || combiner.alphasub_a[0] == &texel0_color.a || combiner.alphasub_b[0] == &texel0_color.a || combiner.alphaadd[0] == &texel0_color.a || \
        combiner.rgbmul_r[0] == &texel0_color.a)
        texel0_used_in_cc0 = 1;
    texels_in_cc0 = texel0_used_in_cc0 || texel1_used_in_cc0;
    texels_in_cc1 = texel0_used_in_cc1 || texel1_used_in_cc1;


    if (texel1_used_in_cc1)
        other_modes.f.textureuselevel0 = 0;
    else if (texel0_used_in_cc1 || lod_frac_used_in_cc1)
        other_modes.f.textureuselevel0 = 1;
    else
        other_modes.f.textureuselevel0 = 2;

    if (texel1_used_in_cc1)
        other_modes.f.textureuselevel1 = 0;
    else if (texel1_used_in_cc0 || texel0_used_in_cc1)
        other_modes.f.textureuselevel1 = 1;
    else if (texel0_used_in_cc0 || lod_frac_used_in_cc0 || lod_frac_used_in_cc1)
        other_modes.f.textureuselevel1 = 2;
    else
        other_modes.f.textureuselevel1 = 3;


    int lodfracused = 0;

    if ((other_modes.cycle_type == CYCLE_TYPE_2 && (lod_frac_used_in_cc0 || lod_frac_used_in_cc1)) || \
        (other_modes.cycle_type == CYCLE_TYPE_1 && lod_frac_used_in_cc1))
        lodfracused = 1;

    if ((other_modes.cycle_type == CYCLE_TYPE_1 && combiner.rgbsub_a_r[1] == &noise) || \
        (other_modes.cycle_type == CYCLE_TYPE_2 && (combiner.rgbsub_a_r[0] == &noise || combiner.rgbsub_a_r[1] == &noise)) || \
        other_modes.alpha_dither_sel == 2)
        other_modes.f.getditherlevel = 0;
    else if (other_modes.f.rgb_alpha_dither != 0xf)
        other_modes.f.getditherlevel = 1;
    else
        other_modes.f.getditherlevel = 2;

    other_modes.f.dolod = other_modes.tex_lod_en || lodfracused;
}
