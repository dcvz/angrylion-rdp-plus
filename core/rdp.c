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
static TLS int32_t pastrawdzmem = 0;

struct span
{
    int lx, rx;
    int unscrx;
    int validline;
    int32_t r, g, b, a, s, t, w, z;
    int32_t majorx[4];
    int32_t minorx[4];
    int32_t invalyscan[4];
};

static TLS struct span span[1024];

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

struct tile
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
};

struct modederivs
{
    int stalederivs;
    int dolod;
    int partialreject_1cycle;
    int partialreject_2cycle;
    int special_bsel0;
    int special_bsel1;
    int rgb_alpha_dither;
    int realblendershiftersneeded;
    int interpixelblendershiftersneeded;
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
    struct modederivs f;
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

static TLS uint32_t primitive_z;
static TLS uint16_t primitive_delta_z;

static TLS int ti_format = FORMAT_RGBA;
static TLS int ti_size = PIXEL_SIZE_4BIT;
static TLS int ti_width = 0;
static TLS uint32_t ti_address = 0;

static TLS struct tile tile[8];

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

static void tile_tlut_common_cs_decoder(const uint32_t* args);
static void loading_pipeline(int start, int end, int tilenum, int coord_quad, int ltlut);
static STRICTINLINE void texture_pipeline_cycle(struct color* TEX, struct color* prev, int32_t SSS, int32_t SST, uint32_t tilenum, uint32_t cycle);
static INLINE void precalculate_everything(void);
static STRICTINLINE int alpha_compare(int32_t comb_alpha);
static STRICTINLINE int finalize_spanalpha(uint32_t blend_en, uint32_t curpixel_cvg, uint32_t curpixel_memcvg);
static STRICTINLINE int32_t normalize_dzpix(int32_t sum);
static STRICTINLINE int32_t clamp(int32_t value,int32_t min,int32_t max);
static STRICTINLINE void get_texel1_1cycle(int32_t* s1, int32_t* t1, int32_t s, int32_t t, int32_t w, int32_t dsinc, int32_t dtinc, int32_t dwinc, int32_t scanline, struct spansigs* sigs);
static STRICTINLINE void get_nexttexel0_2cycle(int32_t* s1, int32_t* t1, int32_t s, int32_t t, int32_t w, int32_t dsinc, int32_t dtinc, int32_t dwinc);
static INLINE void calculate_clamp_diffs(uint32_t tile);
static INLINE void calculate_tile_derivs(uint32_t tile);
static void deduce_derivatives(void);

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

#include "rdp/cmd.c"
#include "rdp/dither.c"
#include "rdp/blender.c"
#include "rdp/combiner.c"
#include "rdp/coverage.c"
#include "rdp/zbuffer.c"
#include "rdp/fbuffer.c"
#include "rdp/tmem.c"
#include "rdp/tcoord.c"
#include "rdp/rasterizer.c"

static STRICTINLINE void tcmask(int32_t* S, int32_t* T, int32_t num)
{
    int32_t wrap;



    if (tile[num].mask_s)
    {
        if (tile[num].ms)
        {
            wrap = *S >> tile[num].f.masksclamped;
            wrap &= 1;
            *S ^= (-wrap);
        }
        *S &= maskbits_table[tile[num].mask_s];
    }

    if (tile[num].mask_t)
    {
        if (tile[num].mt)
        {
            wrap = *T >> tile[num].f.masktclamped;
            wrap &= 1;
            *T ^= (-wrap);
        }

        *T &= maskbits_table[tile[num].mask_t];
    }
}


static STRICTINLINE void tcmask_coupled(int32_t* S, int32_t* sdiff, int32_t* T, int32_t* tdiff, int32_t num)
{
    int32_t wrap;
    int32_t maskbits;
    int32_t wrapthreshold;


    if (tile[num].mask_s)
    {
        maskbits = maskbits_table[tile[num].mask_s];

        if (tile[num].ms)
        {
            wrapthreshold = tile[num].f.masksclamped;

            wrap = (*S >> wrapthreshold) & 1;
            *S ^= (-wrap);
            *S &= maskbits;


            if (((*S - wrap) & maskbits) == maskbits)
                *sdiff = 0;
            else
                *sdiff = 1 - (wrap << 1);
        }
        else
        {
            *S &= maskbits;
            if (*S == maskbits)
                *sdiff = -(*S);
            else
                *sdiff = 1;
        }
    }
    else
        *sdiff = 1;

    if (tile[num].mask_t)
    {
        maskbits = maskbits_table[tile[num].mask_t];

        if (tile[num].mt)
        {
            wrapthreshold = tile[num].f.masktclamped;

            wrap = (*T >> wrapthreshold) & 1;
            *T ^= (-wrap);
            *T &= maskbits;

            if (((*T - wrap) & maskbits) == maskbits)
                *tdiff = 0;
            else
                *tdiff = 1 - (wrap << 1);
        }
        else
        {
            *T &= maskbits;
            if (*T == maskbits)
                *tdiff = -(*T & 0xff);
            else
                *tdiff = 1;
        }
    }
    else
        *tdiff = 1;
}

int rdp_init(struct core_config* _config)
{
    config = _config;

    tcdiv_ptr = tcdiv_func[0];

    uint32_t tmp[2] = {0};
    rdp_set_other_modes(tmp);
    other_modes.f.stalederivs = 1;

    memset(tile, 0, sizeof(tile));

    for (int i = 0; i < 8; i++)
    {
        calculate_tile_derivs(i);
        calculate_clamp_diffs(i);
    }

    memset(&combined_color, 0, sizeof(struct color));
    memset(&prim_color, 0, sizeof(struct color));
    memset(&env_color, 0, sizeof(struct color));
    memset(&key_scale, 0, sizeof(struct color));
    memset(&key_center, 0, sizeof(struct color));

    rdp_pipeline_crashed = 0;
    memset(&onetimewarnings, 0, sizeof(onetimewarnings));

    precalculate_everything();

    return 0;
}


static INLINE void precalculate_everything(void)
{
    precalc_cvmask_derivatives();
    z_init();
    dither_init();
    fb_init();
    blender_init();
    combiner_init();
    tmem_init();
    tcoord_init();
    rasterizer_init();
}

static STRICTINLINE void texture_pipeline_cycle(struct color* TEX, struct color* prev, int32_t SSS, int32_t SST, uint32_t tilenum, uint32_t cycle)
{
    int32_t maxs, maxt, invt3r, invt3g, invt3b, invt3a;
    int32_t sfrac, tfrac, invsf, invtf, sfracrg, invsfrg;
    int upper, upperrg, center, centerrg;


    int bilerp = cycle ? other_modes.bi_lerp1 : other_modes.bi_lerp0;
    int convert = other_modes.convert_one && cycle;
    struct color t0, t1, t2, t3;
    int sss1, sst1, sdiff, tdiff;

    sss1 = SSS;
    sst1 = SST;

    tcshift_cycle(&sss1, &sst1, &maxs, &maxt, tilenum);

    sss1 = TRELATIVE(sss1, tile[tilenum].sl);
    sst1 = TRELATIVE(sst1, tile[tilenum].tl);

    if (other_modes.sample_type || other_modes.en_tlut)
    {
        sfrac = sss1 & 0x1f;
        tfrac = sst1 & 0x1f;




        tcclamp_cycle(&sss1, &sst1, &sfrac, &tfrac, maxs, maxt, tilenum);






        tcmask_coupled(&sss1, &sdiff, &sst1, &tdiff, tilenum);







        upper = (sfrac + tfrac) & 0x20;




        if (tile[tilenum].format == FORMAT_YUV)
        {
            sfracrg = (sfrac >> 1) | ((sss1 & 1) << 4);



            upperrg = (sfracrg + tfrac) & 0x20;
        }
        else
        {
            upperrg = upper;
            sfracrg = sfrac;
        }





        if (!other_modes.sample_type)
            fetch_texel_entlut_quadro_nearest(&t0, &t1, &t2, &t3, sss1, sst1, tilenum, upper, upperrg);
        else if (other_modes.en_tlut)
            fetch_texel_entlut_quadro(&t0, &t1, &t2, &t3, sss1, sdiff, sst1, tdiff, tilenum, upper, upperrg);
        else
            fetch_texel_quadro(&t0, &t1, &t2, &t3, sss1, sdiff, sst1, tdiff, tilenum, upper - upperrg);
















        if (bilerp)
        {
            if (!other_modes.mid_texel)
                center = centerrg = 0;
            else
            {

                center = (sfrac == 0x10 && tfrac == 0x10);
                centerrg = (sfracrg == 0x10 && tfrac == 0x10);
            }

            invtf = 0x20 - tfrac;

            if (!centerrg)
            {
                if (!convert)
                {


                    if (upperrg)
                    {

                        invsfrg = 0x20 - sfracrg;

                        TEX->r = t3.r + ((invsfrg * (t2.r - t3.r) + invtf * (t1.r - t3.r) + 0x10) >> 5);
                        TEX->g = t3.g + ((invsfrg * (t2.g - t3.g) + invtf * (t1.g - t3.g) + 0x10) >> 5);
                    }
                    else
                    {
                        TEX->r = t0.r + ((sfracrg * (t1.r - t0.r) + tfrac * (t2.r - t0.r) + 0x10) >> 5);
                        TEX->g = t0.g + ((sfracrg * (t1.g - t0.g) + tfrac * (t2.g - t0.g) + 0x10) >> 5);
                    }
                }
                else
                {
                    if (upperrg)
                    {
                        TEX->r = prev->b + ((prev->r * (t2.r - t3.r) + prev->g * (t1.r - t3.r) + 0x80) >> 8);
                        TEX->g = prev->b + ((prev->r * (t2.g - t3.g) + prev->g * (t1.g - t3.g) + 0x80) >> 8);
                    }
                    else
                    {
                        TEX->r = prev->b + ((prev->r * (t1.r - t0.r) + prev->g * (t2.r - t0.r) + 0x80) >> 8);
                        TEX->g = prev->b + ((prev->r * (t1.g - t0.g) + prev->g * (t2.g - t0.g) + 0x80) >> 8);
                    }
                }
            }
            else
            {
                invt3r  = ~t3.r;
                invt3g = ~t3.g;

                if (!convert)
                {

                    TEX->r = t3.r + ((((t1.r + t2.r) << 6) - (t3.r << 7) + ((invt3r + t0.r) << 6) + 0xc0) >> 8);
                    TEX->g = t3.g + ((((t1.g + t2.g) << 6) - (t3.g << 7) + ((invt3g + t0.g) << 6) + 0xc0) >> 8);
                }
                else
                {
                    TEX->r = prev->b + ((prev->r * (t2.r - t3.r) + prev->g * (t1.r - t3.r) + ((invt3r + t0.r) << 6) + 0xc0) >> 8);
                    TEX->g = prev->b + ((prev->r * (t2.g - t3.g) + prev->g * (t1.g - t3.g) + ((invt3g + t0.g) << 6) + 0xc0) >> 8);
                }
            }

            if (!center)
            {
                if (!convert)
                {
                    if (upper)
                    {
                        invsf = 0x20 - sfrac;

                        TEX->b = t3.b + ((invsf * (t2.b - t3.b) + invtf * (t1.b - t3.b) + 0x10) >> 5);
                        TEX->a = t3.a + ((invsf * (t2.a - t3.a) + invtf * (t1.a - t3.a) + 0x10) >> 5);
                    }
                    else
                    {
                        TEX->b = t0.b + ((sfrac * (t1.b - t0.b) + tfrac * (t2.b - t0.b) + 0x10) >> 5);
                        TEX->a = t0.a + ((sfrac * (t1.a - t0.a) + tfrac * (t2.a - t0.a) + 0x10) >> 5);
                    }
                }
                else
                {
                    if (upper)
                    {
                        TEX->b = prev->b + ((prev->r * (t2.b - t3.b) + prev->g * (t1.b - t3.b) + 0x80) >> 8);
                        TEX->a = prev->b + ((prev->r * (t2.a - t3.a) + prev->g * (t1.a - t3.a) + 0x80) >> 8);
                    }
                    else
                    {
                        TEX->b = prev->b + ((prev->r * (t1.b - t0.b) + prev->g * (t2.b - t0.b) + 0x80) >> 8);
                        TEX->a = prev->b + ((prev->r * (t1.a - t0.a) + prev->g * (t2.a - t0.a) + 0x80) >> 8);
                    }
                }
            }
            else
            {
                invt3b = ~t3.b;
                invt3a = ~t3.a;

                if (!convert)
                {
                    TEX->b = t3.b + ((((t1.b + t2.b) << 6) - (t3.b << 7) + ((invt3b + t0.b) << 6) + 0xc0) >> 8);
                    TEX->a = t3.a + ((((t1.a + t2.a) << 6) - (t3.a << 7) + ((invt3a + t0.a) << 6) + 0xc0) >> 8);
                }
                else
                {
                    TEX->b = prev->b + ((prev->r * (t2.b - t3.b) + prev->g * (t1.b - t3.b) + ((invt3b + t0.b) << 6) + 0xc0) >> 8);
                    TEX->a = prev->b + ((prev->r * (t2.a - t3.a) + prev->g * (t1.a - t3.a) + ((invt3a + t0.a) << 6) + 0xc0) >> 8);
                }
            }
        }
        else
        {

            if (convert)
                t0 = t3 = *prev;


            if (upperrg)
            {
                if (upper)
                {
                    TEX->r = t3.b + ((k0_tf * t3.g + 0x80) >> 8);
                    TEX->g = t3.b + ((k1_tf * t3.r + k2_tf * t3.g + 0x80) >> 8);
                    TEX->b = t3.b + ((k3_tf * t3.r + 0x80) >> 8);
                    TEX->a = t3.b;
                }
                else
                {
                    TEX->r = t0.b + ((k0_tf * t3.g + 0x80) >> 8);
                    TEX->g = t0.b + ((k1_tf * t3.r + k2_tf * t3.g + 0x80) >> 8);
                    TEX->b = t0.b + ((k3_tf * t3.r + 0x80) >> 8);
                    TEX->a = t0.b;
                }
            }
            else
            {
                if (upper)
                {
                    TEX->r = t3.b + ((k0_tf * t0.g + 0x80) >> 8);
                    TEX->g = t3.b + ((k1_tf * t0.r + k2_tf * t0.g + 0x80) >> 8);
                    TEX->b = t3.b + ((k3_tf * t0.r + 0x80) >> 8);
                    TEX->a = t3.b;
                }
                else
                {
                    TEX->r = t0.b + ((k0_tf * t0.g + 0x80) >> 8);
                    TEX->g = t0.b + ((k1_tf * t0.r + k2_tf * t0.g + 0x80) >> 8);
                    TEX->b = t0.b + ((k3_tf * t0.r + 0x80) >> 8);
                    TEX->a = t0.b;
                }
            }
        }

        TEX->r &= 0x1ff;
        TEX->g &= 0x1ff;
        TEX->b &= 0x1ff;
        TEX->a &= 0x1ff;


    }
    else
    {




        tcclamp_cycle_light(&sss1, &sst1, maxs, maxt, tilenum);

        tcmask(&sss1, &sst1, tilenum);


        fetch_texel(&t0, sss1, sst1, tilenum);

        if (bilerp)
        {
            if (!convert)
            {
                TEX->r = t0.r & 0x1ff;
                TEX->g = t0.g & 0x1ff;
                TEX->b = t0.b;
                TEX->a = t0.a;
            }
            else
                TEX->r = TEX->g = TEX->b = TEX->a = prev->b;
        }
        else
        {
            if (convert)
                t0 = *prev;

            TEX->r = t0.b + ((k0_tf * t0.g + 0x80) >> 8);
            TEX->g = t0.b + ((k1_tf * t0.r + k2_tf * t0.g + 0x80) >> 8);
            TEX->b = t0.b + ((k3_tf * t0.r + 0x80) >> 8);
            TEX->a = t0.b;
            TEX->r &= 0x1ff;
            TEX->g &= 0x1ff;
            TEX->b &= 0x1ff;
        }
    }

}

static void loading_pipeline(int start, int end, int tilenum, int coord_quad, int ltlut)
{


    int localdebugmode = 0, cnt = 0;
    int i, j;

    int dsinc, dtinc;
    dsinc = spans.ds;
    dtinc = spans.dt;

    int s, t;
    int ss, st;
    int xstart, xend, xendsc;
    int sss = 0, sst = 0;
    int ti_index, length;

    uint32_t tmemidx0 = 0, tmemidx1 = 0, tmemidx2 = 0, tmemidx3 = 0;
    int dswap = 0;
    uint16_t* tmem16 = (uint16_t*)tmem;
    uint32_t readval0, readval1, readval2, readval3;
    uint32_t readidx32;
    uint64_t loadqword;
    uint16_t tempshort;
    int tmem_formatting = 0;
    uint32_t bit3fl = 0, hibit = 0;

    if (end > start && ltlut)
    {
        rdp_pipeline_crashed = 1;
        return;
    }

    if (tile[tilenum].format == FORMAT_YUV)
        tmem_formatting = 0;
    else if (tile[tilenum].format == FORMAT_RGBA && tile[tilenum].size == PIXEL_SIZE_32BIT)
        tmem_formatting = 1;
    else
        tmem_formatting = 2;

    int tiadvance = 0, spanadvance = 0;
    int tiptr = 0;
    switch (ti_size)
    {
    case PIXEL_SIZE_4BIT:
        rdp_pipeline_crashed = 1;
        return;
        break;
    case PIXEL_SIZE_8BIT:
        tiadvance = 8;
        spanadvance = 8;
        break;
    case PIXEL_SIZE_16BIT:
        if (!ltlut)
        {
            tiadvance = 8;
            spanadvance = 4;
        }
        else
        {
            tiadvance = 2;
            spanadvance = 1;
        }
        break;
    case PIXEL_SIZE_32BIT:
        tiadvance = 8;
        spanadvance = 2;
        break;
    }

    for (i = start; i <= end; i++)
    {
        xstart = span[i].lx;
        xend = span[i].unscrx;
        xendsc = span[i].rx;
        s = span[i].s;
        t = span[i].t;

        ti_index = ti_width * i + xend;
        tiptr = ti_address + PIXELS_TO_BYTES(ti_index, ti_size);

        length = (xstart - xend + 1) & 0xfff;

        if (trace_write_is_open()) {
            trace_write_rdram((tiptr >> 2), length);
        }

        for (j = 0; j < length; j+= spanadvance)
        {
            ss = s >> 16;
            st = t >> 16;







            sss = ss & 0xffff;
            sst = st & 0xffff;

            tc_pipeline_load(&sss, &sst, tilenum, coord_quad);

            dswap = sst & 1;


            get_tmem_idx(sss, sst, tilenum, &tmemidx0, &tmemidx1, &tmemidx2, &tmemidx3, &bit3fl, &hibit);

            readidx32 = (tiptr >> 2) & ~1;
            RREADIDX32(readval0, readidx32);
            readidx32++;
            RREADIDX32(readval1, readidx32);
            readidx32++;
            RREADIDX32(readval2, readidx32);
            readidx32++;
            RREADIDX32(readval3, readidx32);


            switch(tiptr & 7)
            {
            case 0:
                if (!ltlut)
                    loadqword = ((uint64_t)readval0 << 32) | readval1;
                else
                {
                    tempshort = readval0 >> 16;
                    loadqword = ((uint64_t)tempshort << 48) | ((uint64_t) tempshort << 32) | ((uint64_t) tempshort << 16) | tempshort;
                }
                break;
            case 1:
                loadqword = ((uint64_t)readval0 << 40) | ((uint64_t)readval1 << 8) | (readval2 >> 24);
                break;
            case 2:
                if (!ltlut)
                    loadqword = ((uint64_t)readval0 << 48) | ((uint64_t)readval1 << 16) | (readval2 >> 16);
                else
                {
                    tempshort = readval0 & 0xffff;
                    loadqword = ((uint64_t)tempshort << 48) | ((uint64_t) tempshort << 32) | ((uint64_t) tempshort << 16) | tempshort;
                }
                break;
            case 3:
                loadqword = ((uint64_t)readval0 << 56) | ((uint64_t)readval1 << 24) | (readval2 >> 8);
                break;
            case 4:
                if (!ltlut)
                    loadqword = ((uint64_t)readval1 << 32) | readval2;
                else
                {
                    tempshort = readval1 >> 16;
                    loadqword = ((uint64_t)tempshort << 48) | ((uint64_t) tempshort << 32) | ((uint64_t) tempshort << 16) | tempshort;
                }
                break;
            case 5:
                loadqword = ((uint64_t)readval1 << 40) | ((uint64_t)readval2 << 8) | (readval3 >> 24);
                break;
            case 6:
                if (!ltlut)
                    loadqword = ((uint64_t)readval1 << 48) | ((uint64_t)readval2 << 16) | (readval3 >> 16);
                else
                {
                    tempshort = readval1 & 0xffff;
                    loadqword = ((uint64_t)tempshort << 48) | ((uint64_t) tempshort << 32) | ((uint64_t) tempshort << 16) | tempshort;
                }
                break;
            case 7:
                loadqword = ((uint64_t)readval1 << 56) | ((uint64_t)readval2 << 24) | (readval3 >> 8);
                break;
            }


            switch(tmem_formatting)
            {
            case 0:
                readval0 = (uint32_t)((((loadqword >> 56) & 0xff) << 24) | (((loadqword >> 40) & 0xff) << 16) | (((loadqword >> 24) & 0xff) << 8) | (((loadqword >> 8) & 0xff) << 0));
                readval1 = (uint32_t)((((loadqword >> 48) & 0xff) << 24) | (((loadqword >> 32) & 0xff) << 16) | (((loadqword >> 16) & 0xff) << 8) | (((loadqword >> 0) & 0xff) << 0));
                if (bit3fl)
                {
                    tmem16[tmemidx2 ^ WORD_ADDR_XOR] = (uint16_t)(readval0 >> 16);
                    tmem16[tmemidx3 ^ WORD_ADDR_XOR] = (uint16_t)(readval0 & 0xffff);
                    tmem16[(tmemidx2 | 0x400) ^ WORD_ADDR_XOR] = (uint16_t)(readval1 >> 16);
                    tmem16[(tmemidx3 | 0x400) ^ WORD_ADDR_XOR] = (uint16_t)(readval1 & 0xffff);
                }
                else
                {
                    tmem16[tmemidx0 ^ WORD_ADDR_XOR] = (uint16_t)(readval0 >> 16);
                    tmem16[tmemidx1 ^ WORD_ADDR_XOR] = (uint16_t)(readval0 & 0xffff);
                    tmem16[(tmemidx0 | 0x400) ^ WORD_ADDR_XOR] = (uint16_t)(readval1 >> 16);
                    tmem16[(tmemidx1 | 0x400) ^ WORD_ADDR_XOR] = (uint16_t)(readval1 & 0xffff);
                }
                break;
            case 1:
                readval0 = (uint32_t)(((loadqword >> 48) << 16) | ((loadqword >> 16) & 0xffff));
                readval1 = (uint32_t)((((loadqword >> 32) & 0xffff) << 16) | (loadqword & 0xffff));

                if (bit3fl)
                {
                    tmem16[tmemidx2 ^ WORD_ADDR_XOR] = (uint16_t)(readval0 >> 16);
                    tmem16[tmemidx3 ^ WORD_ADDR_XOR] = (uint16_t)(readval0 & 0xffff);
                    tmem16[(tmemidx2 | 0x400) ^ WORD_ADDR_XOR] = (uint16_t)(readval1 >> 16);
                    tmem16[(tmemidx3 | 0x400) ^ WORD_ADDR_XOR] = (uint16_t)(readval1 & 0xffff);
                }
                else
                {
                    tmem16[tmemidx0 ^ WORD_ADDR_XOR] = (uint16_t)(readval0 >> 16);
                    tmem16[tmemidx1 ^ WORD_ADDR_XOR] = (uint16_t)(readval0 & 0xffff);
                    tmem16[(tmemidx0 | 0x400) ^ WORD_ADDR_XOR] = (uint16_t)(readval1 >> 16);
                    tmem16[(tmemidx1 | 0x400) ^ WORD_ADDR_XOR] = (uint16_t)(readval1 & 0xffff);
                }
                break;
            case 2:
                if (!dswap)
                {
                    if (!hibit)
                    {
                        tmem16[tmemidx0 ^ WORD_ADDR_XOR] = (uint16_t)(loadqword >> 48);
                        tmem16[tmemidx1 ^ WORD_ADDR_XOR] = (uint16_t)(loadqword >> 32);
                        tmem16[tmemidx2 ^ WORD_ADDR_XOR] = (uint16_t)(loadqword >> 16);
                        tmem16[tmemidx3 ^ WORD_ADDR_XOR] = (uint16_t)(loadqword & 0xffff);
                    }
                    else
                    {
                        tmem16[(tmemidx0 | 0x400) ^ WORD_ADDR_XOR] = (uint16_t)(loadqword >> 48);
                        tmem16[(tmemidx1 | 0x400) ^ WORD_ADDR_XOR] = (uint16_t)(loadqword >> 32);
                        tmem16[(tmemidx2 | 0x400) ^ WORD_ADDR_XOR] = (uint16_t)(loadqword >> 16);
                        tmem16[(tmemidx3 | 0x400) ^ WORD_ADDR_XOR] = (uint16_t)(loadqword & 0xffff);
                    }
                }
                else
                {
                    if (!hibit)
                    {
                        tmem16[tmemidx0 ^ WORD_ADDR_XOR] = (uint16_t)(loadqword >> 16);
                        tmem16[tmemidx1 ^ WORD_ADDR_XOR] = (uint16_t)(loadqword & 0xffff);
                        tmem16[tmemidx2 ^ WORD_ADDR_XOR] = (uint16_t)(loadqword >> 48);
                        tmem16[tmemidx3 ^ WORD_ADDR_XOR] = (uint16_t)(loadqword >> 32);
                    }
                    else
                    {
                        tmem16[(tmemidx0 | 0x400) ^ WORD_ADDR_XOR] = (uint16_t)(loadqword >> 16);
                        tmem16[(tmemidx1 | 0x400) ^ WORD_ADDR_XOR] = (uint16_t)(loadqword & 0xffff);
                        tmem16[(tmemidx2 | 0x400) ^ WORD_ADDR_XOR] = (uint16_t)(loadqword >> 48);
                        tmem16[(tmemidx3 | 0x400) ^ WORD_ADDR_XOR] = (uint16_t)(loadqword >> 32);
                    }
                }
            break;
            }


            s = (s + dsinc) & ~0x1f;
            t = (t + dtinc) & ~0x1f;
            tiptr += tiadvance;
        }
    }
}

static void edgewalker_for_loads(int32_t* lewdata)
{
    int j = 0;
    int xleft = 0, xright = 0;
    int xstart = 0, xend = 0;
    int s = 0, t = 0, w = 0;
    int dsdx = 0, dtdx = 0;
    int dsdy = 0, dtdy = 0;
    int dsde = 0, dtde = 0;
    int tilenum = 0, flip = 0;
    int32_t yl = 0, ym = 0, yh = 0;
    int32_t xl = 0, xm = 0, xh = 0;
    int32_t dxldy = 0, dxhdy = 0, dxmdy = 0;

    int cmd_id = CMD_ID(lewdata);
    int ltlut = (cmd_id == CMD_ID_LOAD_TLUT);
    int coord_quad = ltlut || (cmd_id == CMD_ID_LOAD_BLOCK);
    flip = 1;
    max_level = 0;
    tilenum = (lewdata[0] >> 16) & 7;


    yl = SIGN(lewdata[0], 14);
    ym = lewdata[1] >> 16;
    ym = SIGN(ym, 14);
    yh = SIGN(lewdata[1], 14);

    xl = SIGN(lewdata[2], 28);
    xh = SIGN(lewdata[3], 28);
    xm = SIGN(lewdata[4], 28);

    dxldy = 0;
    dxhdy = 0;
    dxmdy = 0;


    s    = lewdata[5] & 0xffff0000;
    t    = (lewdata[5] & 0xffff) << 16;
    w    = 0;
    dsdx = (lewdata[7] & 0xffff0000) | ((lewdata[6] >> 16) & 0xffff);
    dtdx = ((lewdata[7] << 16) & 0xffff0000)    | (lewdata[6] & 0xffff);
    dsde = 0;
    dtde = (lewdata[9] & 0xffff) << 16;
    dsdy = 0;
    dtdy = (lewdata[8] & 0xffff) << 16;

    spans.ds = dsdx & ~0x1f;
    spans.dt = dtdx & ~0x1f;
    spans.dw = 0;






    xright = xh & ~0x1;
    xleft = xm & ~0x1;

    int k = 0;

    int sign_dxhdy = 0;

    int do_offset = 0;

    int xfrac = 0;






#define ADJUST_ATTR_LOAD()                                      \
{                                                               \
    span[j].s = s & ~0x3ff;                                     \
    span[j].t = t & ~0x3ff;                                     \
}


#define ADDVALUES_LOAD() {  \
            t += dtde;      \
}

    int32_t maxxmx, minxhx;

    int spix = 0;
    int ycur =  yh & ~3;
    int ylfar = yl | 3;

    int valid_y = 1;
    int length = 0;
    int32_t xrsc = 0, xlsc = 0, stickybit = 0;
    int32_t yllimit = yl;
    int32_t yhlimit = yh;

    xfrac = 0;
    xend = xright >> 16;


    for (k = ycur; k <= ylfar; k++)
    {
        if (k == ym)
            xleft = xl & ~1;

        spix = k & 3;

        if (!(k & ~0xfff))
        {
            j = k >> 2;
            valid_y = !(k < yhlimit || k >= yllimit);

            if (spix == 0)
            {
                maxxmx = 0;
                minxhx = 0xfff;
            }

            xrsc = (xright >> 13) & 0x7ffe;



            xlsc = (xleft >> 13) & 0x7ffe;

            if (valid_y)
            {
                maxxmx = (((xlsc >> 3) & 0xfff) > maxxmx) ? (xlsc >> 3) & 0xfff : maxxmx;
                minxhx = (((xrsc >> 3) & 0xfff) < minxhx) ? (xrsc >> 3) & 0xfff : minxhx;
            }

            if (spix == 0)
            {
                span[j].unscrx = xend;
                ADJUST_ATTR_LOAD();
            }

            if (spix == 3)
            {
                span[j].lx = maxxmx;
                span[j].rx = minxhx;


            }


        }

        if (spix == 3)
        {
            ADDVALUES_LOAD();
        }



    }

    loading_pipeline(yhlimit >> 2, yllimit >> 2, tilenum, coord_quad, ltlut);
}


static const char *const image_format[] = { "RGBA", "YUV", "CI", "IA", "I", "???", "???", "???" };
static const char *const image_size[] = { "4-bit", "8-bit", "16-bit", "32-bit" };


static void rdp_invalid(const uint32_t* args)
{
}

static void rdp_noop(const uint32_t* args)
{
}


static void rdp_sync_load(const uint32_t* args)
{

}

static void rdp_sync_pipe(const uint32_t* args)
{


}

static void rdp_sync_tile(const uint32_t* args)
{

}

static void rdp_sync_full(const uint32_t* args)
{
    core_sync_dp();
}

static void rdp_set_convert(const uint32_t* args)
{
    int32_t k0 = (args[0] >> 13) & 0x1ff;
    int32_t k1 = (args[0] >> 4) & 0x1ff;
    int32_t k2 = ((args[0] & 0xf) << 5) | ((args[1] >> 27) & 0x1f);
    int32_t k3 = (args[1] >> 18) & 0x1ff;
    k0_tf = (SIGN(k0, 9) << 1) + 1;
    k1_tf = (SIGN(k1, 9) << 1) + 1;
    k2_tf = (SIGN(k2, 9) << 1) + 1;
    k3_tf = (SIGN(k3, 9) << 1) + 1;
    k4 = (args[1] >> 9) & 0x1ff;
    k5 = args[1] & 0x1ff;
}

static void rdp_set_prim_depth(const uint32_t* args)
{
    primitive_z = args[1] & (0x7fff << 16);


    primitive_delta_z = (uint16_t)(args[1]);
}

static void rdp_set_other_modes(const uint32_t* args)
{
    other_modes.cycle_type          = (args[0] >> 20) & 0x3;
    other_modes.persp_tex_en        = (args[0] & 0x80000) ? 1 : 0;
    other_modes.detail_tex_en       = (args[0] & 0x40000) ? 1 : 0;
    other_modes.sharpen_tex_en      = (args[0] & 0x20000) ? 1 : 0;
    other_modes.tex_lod_en          = (args[0] & 0x10000) ? 1 : 0;
    other_modes.en_tlut             = (args[0] & 0x08000) ? 1 : 0;
    other_modes.tlut_type           = (args[0] & 0x04000) ? 1 : 0;
    other_modes.sample_type         = (args[0] & 0x02000) ? 1 : 0;
    other_modes.mid_texel           = (args[0] & 0x01000) ? 1 : 0;
    other_modes.bi_lerp0            = (args[0] & 0x00800) ? 1 : 0;
    other_modes.bi_lerp1            = (args[0] & 0x00400) ? 1 : 0;
    other_modes.convert_one         = (args[0] & 0x00200) ? 1 : 0;
    other_modes.key_en              = (args[0] & 0x00100) ? 1 : 0;
    other_modes.rgb_dither_sel      = (args[0] >> 6) & 0x3;
    other_modes.alpha_dither_sel    = (args[0] >> 4) & 0x3;
    other_modes.blend_m1a_0         = (args[1] >> 30) & 0x3;
    other_modes.blend_m1a_1         = (args[1] >> 28) & 0x3;
    other_modes.blend_m1b_0         = (args[1] >> 26) & 0x3;
    other_modes.blend_m1b_1         = (args[1] >> 24) & 0x3;
    other_modes.blend_m2a_0         = (args[1] >> 22) & 0x3;
    other_modes.blend_m2a_1         = (args[1] >> 20) & 0x3;
    other_modes.blend_m2b_0         = (args[1] >> 18) & 0x3;
    other_modes.blend_m2b_1         = (args[1] >> 16) & 0x3;
    other_modes.force_blend         = (args[1] >> 14) & 1;
    other_modes.alpha_cvg_select    = (args[1] >> 13) & 1;
    other_modes.cvg_times_alpha     = (args[1] >> 12) & 1;
    other_modes.z_mode              = (args[1] >> 10) & 0x3;
    other_modes.cvg_dest            = (args[1] >> 8) & 0x3;
    other_modes.color_on_cvg        = (args[1] >> 7) & 1;
    other_modes.image_read_en       = (args[1] >> 6) & 1;
    other_modes.z_update_en         = (args[1] >> 5) & 1;
    other_modes.z_compare_en        = (args[1] >> 4) & 1;
    other_modes.antialias_en        = (args[1] >> 3) & 1;
    other_modes.z_source_sel        = (args[1] >> 2) & 1;
    other_modes.dither_alpha_en     = (args[1] >> 1) & 1;
    other_modes.alpha_compare_en    = (args[1]) & 1;

    set_blender_input(0, 0, &blender.i1a_r[0], &blender.i1a_g[0], &blender.i1a_b[0], &blender.i1b_a[0],
                      other_modes.blend_m1a_0, other_modes.blend_m1b_0);
    set_blender_input(0, 1, &blender.i2a_r[0], &blender.i2a_g[0], &blender.i2a_b[0], &blender.i2b_a[0],
                      other_modes.blend_m2a_0, other_modes.blend_m2b_0);
    set_blender_input(1, 0, &blender.i1a_r[1], &blender.i1a_g[1], &blender.i1a_b[1], &blender.i1b_a[1],
                      other_modes.blend_m1a_1, other_modes.blend_m1b_1);
    set_blender_input(1, 1, &blender.i2a_r[1], &blender.i2a_g[1], &blender.i2a_b[1], &blender.i2b_a[1],
                      other_modes.blend_m2a_1, other_modes.blend_m2b_1);

    other_modes.f.stalederivs = 1;
}

static void deduce_derivatives()
{

    other_modes.f.partialreject_1cycle = (blender.i2b_a[0] == &inv_pixel_color.a && blender.i1b_a[0] == &pixel_color.a);
    other_modes.f.partialreject_2cycle = (blender.i2b_a[1] == &inv_pixel_color.a && blender.i1b_a[1] == &pixel_color.a);


    other_modes.f.special_bsel0 = (blender.i2b_a[0] == &memory_color.a);
    other_modes.f.special_bsel1 = (blender.i2b_a[1] == &memory_color.a);


    other_modes.f.realblendershiftersneeded = (other_modes.f.special_bsel0 && other_modes.cycle_type == CYCLE_TYPE_1) || (other_modes.f.special_bsel1 && other_modes.cycle_type == CYCLE_TYPE_2);
    other_modes.f.interpixelblendershiftersneeded = (other_modes.f.special_bsel0 && other_modes.cycle_type == CYCLE_TYPE_2);

    other_modes.f.rgb_alpha_dither = (other_modes.rgb_dither_sel << 2) | other_modes.alpha_dither_sel;

    if (other_modes.rgb_dither_sel == 3)
        rgb_dither_ptr = rgb_dither_func[1];
    else
        rgb_dither_ptr = rgb_dither_func[0];

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
        render_spans_1cycle_ptr = render_spans_1cycle_func[2];
    else if (texel0_used_in_cc1 || lod_frac_used_in_cc1)
        render_spans_1cycle_ptr = render_spans_1cycle_func[1];
    else
        render_spans_1cycle_ptr = render_spans_1cycle_func[0];

    if (texel1_used_in_cc1)
        render_spans_2cycle_ptr = render_spans_2cycle_func[3];
    else if (texel1_used_in_cc0 || texel0_used_in_cc1)
        render_spans_2cycle_ptr = render_spans_2cycle_func[2];
    else if (texel0_used_in_cc0 || lod_frac_used_in_cc0 || lod_frac_used_in_cc1)
        render_spans_2cycle_ptr = render_spans_2cycle_func[1];
    else
        render_spans_2cycle_ptr = render_spans_2cycle_func[0];


    int lodfracused = 0;

    if ((other_modes.cycle_type == CYCLE_TYPE_2 && (lod_frac_used_in_cc0 || lod_frac_used_in_cc1)) || \
        (other_modes.cycle_type == CYCLE_TYPE_1 && lod_frac_used_in_cc1))
        lodfracused = 1;

    if ((other_modes.cycle_type == CYCLE_TYPE_1 && combiner.rgbsub_a_r[1] == &noise) || \
        (other_modes.cycle_type == CYCLE_TYPE_2 && (combiner.rgbsub_a_r[0] == &noise || combiner.rgbsub_a_r[1] == &noise)) || \
        other_modes.alpha_dither_sel == 2)
        get_dither_noise_ptr = get_dither_noise_func[0];
    else if (other_modes.f.rgb_alpha_dither != 0xf)
        get_dither_noise_ptr = get_dither_noise_func[1];
    else
        get_dither_noise_ptr = get_dither_noise_func[2];

    other_modes.f.dolod = other_modes.tex_lod_en || lodfracused;
}

static void rdp_set_tile_size(const uint32_t* args)
{
    int tilenum = (args[1] >> 24) & 0x7;
    tile[tilenum].sl = (args[0] >> 12) & 0xfff;
    tile[tilenum].tl = (args[0] >>  0) & 0xfff;
    tile[tilenum].sh = (args[1] >> 12) & 0xfff;
    tile[tilenum].th = (args[1] >>  0) & 0xfff;

    calculate_clamp_diffs(tilenum);
}

static void rdp_load_block(const uint32_t* args)
{
    int tilenum = (args[1] >> 24) & 0x7;
    int sl, sh, tl, dxt;


    tile[tilenum].sl = sl = ((args[0] >> 12) & 0xfff);
    tile[tilenum].tl = tl = ((args[0] >>  0) & 0xfff);
    tile[tilenum].sh = sh = ((args[1] >> 12) & 0xfff);
    tile[tilenum].th = dxt  = ((args[1] >>  0) & 0xfff);

    calculate_clamp_diffs(tilenum);

    int tlclamped = tl & 0x3ff;

    int32_t lewdata[10];

    lewdata[0] = (args[0] & 0xff000000) | (0x10 << 19) | (tilenum << 16) | ((tlclamped << 2) | 3);
    lewdata[1] = (((tlclamped << 2) | 3) << 16) | (tlclamped << 2);
    lewdata[2] = sh << 16;
    lewdata[3] = sl << 16;
    lewdata[4] = sh << 16;
    lewdata[5] = ((sl << 3) << 16) | (tl << 3);
    lewdata[6] = (dxt & 0xff) << 8;
    lewdata[7] = ((0x80 >> ti_size) << 16) | (dxt >> 8);
    lewdata[8] = 0x20;
    lewdata[9] = 0x20;

    edgewalker_for_loads(lewdata);

}

static void rdp_load_tlut(const uint32_t* args)
{


    tile_tlut_common_cs_decoder(args);
}

static void rdp_load_tile(const uint32_t* args)
{
    tile_tlut_common_cs_decoder(args);
}

static void tile_tlut_common_cs_decoder(const uint32_t* args)
{
    int tilenum = (args[1] >> 24) & 0x7;
    int sl, tl, sh, th;


    tile[tilenum].sl = sl = ((args[0] >> 12) & 0xfff);
    tile[tilenum].tl = tl = ((args[0] >>  0) & 0xfff);
    tile[tilenum].sh = sh = ((args[1] >> 12) & 0xfff);
    tile[tilenum].th = th = ((args[1] >>  0) & 0xfff);

    calculate_clamp_diffs(tilenum);


    int32_t lewdata[10];

    lewdata[0] = (args[0] & 0xff000000) | (0x10 << 19) | (tilenum << 16) | (th | 3);
    lewdata[1] = ((th | 3) << 16) | (tl);
    lewdata[2] = ((sh >> 2) << 16) | ((sh & 3) << 14);
    lewdata[3] = ((sl >> 2) << 16) | ((sl & 3) << 14);
    lewdata[4] = ((sh >> 2) << 16) | ((sh & 3) << 14);
    lewdata[5] = ((sl << 3) << 16) | (tl << 3);
    lewdata[6] = 0;
    lewdata[7] = (0x200 >> ti_size) << 16;
    lewdata[8] = 0x20;
    lewdata[9] = 0x20;

    edgewalker_for_loads(lewdata);
}

static void rdp_set_tile(const uint32_t* args)
{
    int tilenum = (args[1] >> 24) & 0x7;

    tile[tilenum].format    = (args[0] >> 21) & 0x7;
    tile[tilenum].size      = (args[0] >> 19) & 0x3;
    tile[tilenum].line      = (args[0] >>  9) & 0x1ff;
    tile[tilenum].tmem      = (args[0] >>  0) & 0x1ff;
    tile[tilenum].palette   = (args[1] >> 20) & 0xf;
    tile[tilenum].ct        = (args[1] >> 19) & 0x1;
    tile[tilenum].mt        = (args[1] >> 18) & 0x1;
    tile[tilenum].mask_t    = (args[1] >> 14) & 0xf;
    tile[tilenum].shift_t   = (args[1] >> 10) & 0xf;
    tile[tilenum].cs        = (args[1] >>  9) & 0x1;
    tile[tilenum].ms        = (args[1] >>  8) & 0x1;
    tile[tilenum].mask_s    = (args[1] >>  4) & 0xf;
    tile[tilenum].shift_s   = (args[1] >>  0) & 0xf;

    calculate_tile_derivs(tilenum);
}

static void rdp_fill_rect(const uint32_t* args)
{
    uint32_t xl = (args[0] >> 12) & 0xfff;
    uint32_t yl = (args[0] >>  0) & 0xfff;
    uint32_t xh = (args[1] >> 12) & 0xfff;
    uint32_t yh = (args[1] >>  0) & 0xfff;

    if (other_modes.cycle_type == CYCLE_TYPE_FILL || other_modes.cycle_type == CYCLE_TYPE_COPY)
        yl |= 3;

    uint32_t xlint = (xl >> 2) & 0x3ff;
    uint32_t xhint = (xh >> 2) & 0x3ff;

    int32_t ewdata[CMD_MAX_INTS];
    ewdata[0] = (0x3680 << 16) | yl;
    ewdata[1] = (yl << 16) | yh;
    ewdata[2] = (xlint << 16) | ((xl & 3) << 14);
    ewdata[3] = 0;
    ewdata[4] = (xhint << 16) | ((xh & 3) << 14);
    ewdata[5] = 0;
    ewdata[6] = (xlint << 16) | ((xl & 3) << 14);
    ewdata[7] = 0;
    memset(&ewdata[8], 0, 36 * sizeof(int32_t));

    edgewalker_for_prims(ewdata);
}

static void rdp_set_texture_image(const uint32_t* args)
{
    ti_format   = (args[0] >> 21) & 0x7;
    ti_size     = (args[0] >> 19) & 0x3;
    ti_width    = (args[0] & 0x3ff) + 1;
    ti_address  = args[1] & 0x0ffffff;



}

static STRICTINLINE int32_t normalize_dzpix(int32_t sum)
{
    if (sum & 0xc000)
        return 0x8000;
    if (!(sum & 0xffff))
        return 1;

    if (sum == 1)
        return 3;

    for(int count = 0x2000; count > 0; count >>= 1)
    {
        if (sum & count)
            return(count << 1);
    }
    msg_error("normalize_dzpix: invalid codepath taken");
    return 0;
}

static STRICTINLINE int32_t clamp(int32_t value,int32_t min,int32_t max)
{
    if (value < min)
        return min;
    else if (value > max)
        return max;
    else
        return value;
}

static INLINE void calculate_clamp_diffs(uint32_t i)
{
    tile[i].f.clampdiffs = ((tile[i].sh >> 2) - (tile[i].sl >> 2)) & 0x3ff;
    tile[i].f.clampdifft = ((tile[i].th >> 2) - (tile[i].tl >> 2)) & 0x3ff;
}


static INLINE void calculate_tile_derivs(uint32_t i)
{
    tile[i].f.clampens = tile[i].cs || !tile[i].mask_s;
    tile[i].f.clampent = tile[i].ct || !tile[i].mask_t;
    tile[i].f.masksclamped = tile[i].mask_s <= 10 ? tile[i].mask_s : 10;
    tile[i].f.masktclamped = tile[i].mask_t <= 10 ? tile[i].mask_t : 10;
    tile[i].f.notlutswitch = (tile[i].format << 2) | tile[i].size;
    tile[i].f.tlutswitch = (tile[i].size << 2) | ((tile[i].format + 2) & 3);

    if (tile[i].format < 5)
    {
        tile[i].f.notlutswitch = (tile[i].format << 2) | tile[i].size;
        tile[i].f.tlutswitch = (tile[i].size << 2) | ((tile[i].format + 2) & 3);
    }
    else
    {
        tile[i].f.notlutswitch = 0x10 | tile[i].size;
        tile[i].f.tlutswitch = (tile[i].size << 2) | 2;
    }
}

static STRICTINLINE void get_texel1_1cycle(int32_t* s1, int32_t* t1, int32_t s, int32_t t, int32_t w, int32_t dsinc, int32_t dtinc, int32_t dwinc, int32_t scanline, struct spansigs* sigs)
{
    int32_t nexts, nextt, nextsw;

    if (!sigs->endspan || !sigs->longspan || !span[scanline + 1].validline)
    {


        nextsw = (w + dwinc) >> 16;
        nexts = (s + dsinc) >> 16;
        nextt = (t + dtinc) >> 16;
    }
    else
    {







        int32_t nextscan = scanline + 1;
        nextt = span[nextscan].t >> 16;
        nexts = span[nextscan].s >> 16;
        nextsw = span[nextscan].w >> 16;
    }

    tcdiv_ptr(nexts, nextt, nextsw, s1, t1);
}

static STRICTINLINE void get_nexttexel0_2cycle(int32_t* s1, int32_t* t1, int32_t s, int32_t t, int32_t w, int32_t dsinc, int32_t dtinc, int32_t dwinc)
{


    int32_t nexts, nextt, nextsw;
    nextsw = (w + dwinc) >> 16;
    nexts = (s + dsinc) >> 16;
    nextt = (t + dtinc) >> 16;

    tcdiv_ptr(nexts, nextt, nextsw, s1, t1);
}
