#include "vi.h"
#include "common.h"
#include "plugin.h"
#include "rdram.h"
#include "msg.h"
#include "screen.h"
#include "irand.h"
#include "parallel_c.hpp"

#include <memory.h>

struct ccvg
{
    uint8_t r, g, b, cvg;
};

// config
static struct core_config* cfg;

// states
static uint32_t prevvicurrent;
static int emucontrolsvicurrent;
static int prevserrate;
static int oldlowerfield;
static int32_t oldvstart;
static uint32_t prevwasblank;
static uint32_t tvfadeoutstate[625];
static int pitchindwords;
static int ispal;
static int lineshifter;
static int minhpass;
static int maxhpass;
static uint32_t x_add;
static uint32_t x_start_init;
static uint32_t y_add;
static uint32_t y_start;

// prescale buffer
static int32_t* PreScale;
static uint32_t prescale_ptr;
static int linecount;

// parsed VI registers
static int dither_filter;
static int fsaa;
static int divot;
static int gamma;
static int gamma_dither;
static int lerp_en;
static int extralines;
static int vitype;
static int serration_pulses;
static int gamma_and_dither;
static int32_t hres, vres;
static int32_t v_start;
static int32_t h_start;

static struct
{
    int nolerp, vbusclock;
} onetimewarnings;

// function pointers
STRICTINLINE void vi_fetch_filter16(struct ccvg* res, uint32_t fboffset, uint32_t cur_x, uint32_t fsaa, uint32_t dither_filter, uint32_t vres, uint32_t fetchstate);
STRICTINLINE void vi_fetch_filter32(struct ccvg* res, uint32_t fboffset, uint32_t cur_x, uint32_t fsaa, uint32_t dither_filter, uint32_t vres, uint32_t fetchstate);

static void (*vi_fetch_filter_func[2])(struct ccvg*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) =
{
    vi_fetch_filter16, vi_fetch_filter32
};

void (*vi_fetch_filter_ptr)(struct ccvg*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

// lookup tables
static uint32_t gamma_table[0x100];
static uint32_t gamma_dither_table[0x4000];
static int vi_restore_table[0x400];

#define PRESCALE_WIDTH 640
#define PRESCALE_HEIGHT 625

STRICTINLINE void restore_filter16(int* r, int* g, int* b, uint32_t fboffset, uint32_t num, uint32_t hres, uint32_t fetchbugstate)
{


    uint32_t idx = (fboffset >> 1) + num;

    uint32_t toleftpix = idx - 1;

    uint32_t leftuppix, leftdownpix, maxpix;

    leftuppix = idx - hres - 1;

    if (fetchbugstate != 1)
    {
        leftdownpix = idx + hres - 1;
        maxpix = idx + hres + 1;
    }
    else
    {
        leftdownpix = toleftpix;
        maxpix = toleftpix + 2;
    }

    int rend = *r;
    int gend = *g;
    int bend = *b;
    const int* redptr = &vi_restore_table[(rend << 2) & 0x3e0];
    const int* greenptr = &vi_restore_table[(gend << 2) & 0x3e0];
    const int* blueptr = &vi_restore_table[(bend << 2) & 0x3e0];

    uint32_t tempr, tempg, tempb;
    uint16_t pix;
    uint32_t addr;


#define VI_COMPARE(x)                                               \
{                                                                   \
    addr = (x);                                                     \
    RREADIDX16(pix, addr);                                          \
    tempr = (pix >> 11) & 0x1f;                                     \
    tempg = (pix >> 6) & 0x1f;                                      \
    tempb = (pix >> 1) & 0x1f;                                      \
    rend += redptr[tempr];                                          \
    gend += greenptr[tempg];                                        \
    bend += blueptr[tempb];                                         \
}

#define VI_COMPARE_OPT(x)                                           \
{                                                                   \
    addr = (x);                                                     \
    pix = rdram16[addr ^ WORD_ADDR_XOR];                            \
    tempr = (pix >> 11) & 0x1f;                                     \
    tempg = (pix >> 6) & 0x1f;                                      \
    tempb = (pix >> 1) & 0x1f;                                      \
    rend += redptr[tempr];                                          \
    gend += greenptr[tempg];                                        \
    bend += blueptr[tempb];                                         \
}

    if (rdram_valid_idx16(maxpix) && rdram_valid_idx16(leftuppix))
    {
        VI_COMPARE_OPT(leftuppix);
        VI_COMPARE_OPT(leftuppix + 1);
        VI_COMPARE_OPT(leftuppix + 2);
        VI_COMPARE_OPT(leftdownpix);
        VI_COMPARE_OPT(leftdownpix + 1);
        VI_COMPARE_OPT(maxpix);
        VI_COMPARE_OPT(toleftpix);
        VI_COMPARE_OPT(toleftpix + 2);
    }
    else
    {
        VI_COMPARE(leftuppix);
        VI_COMPARE(leftuppix + 1);
        VI_COMPARE(leftuppix + 2);
        VI_COMPARE(leftdownpix);
        VI_COMPARE(leftdownpix + 1);
        VI_COMPARE(maxpix);
        VI_COMPARE(toleftpix);
        VI_COMPARE(toleftpix + 2);
    }


    *r = rend;
    *g = gend;
    *b = bend;
}

STRICTINLINE void restore_filter32(int* r, int* g, int* b, uint32_t fboffset, uint32_t num, uint32_t hres, uint32_t fetchbugstate)
{
    uint32_t idx = (fboffset >> 2) + num;

    uint32_t toleftpix = idx - 1;

    uint32_t leftuppix, leftdownpix, maxpix;

    leftuppix = idx - hres - 1;

    if (fetchbugstate != 1)
    {
        leftdownpix = idx + hres - 1;
        maxpix = idx +hres + 1;
    }
    else
    {
        leftdownpix = toleftpix;
        maxpix = toleftpix + 2;
    }

    int rend = *r;
    int gend = *g;
    int bend = *b;
    const int* redptr = &vi_restore_table[(rend << 2) & 0x3e0];
    const int* greenptr = &vi_restore_table[(gend << 2) & 0x3e0];
    const int* blueptr = &vi_restore_table[(bend << 2) & 0x3e0];

    uint32_t tempr, tempg, tempb;
    uint32_t pix, addr;

#define VI_COMPARE32(x)                                                 \
{                                                                       \
    addr = (x);                                                         \
    RREADIDX32(pix, addr);                                              \
    tempr = (pix >> 27) & 0x1f;                                         \
    tempg = (pix >> 19) & 0x1f;                                         \
    tempb = (pix >> 11) & 0x1f;                                         \
    rend += redptr[tempr];                                              \
    gend += greenptr[tempg];                                            \
    bend += blueptr[tempb];                                             \
}

#define VI_COMPARE32_OPT(x)                                                 \
{                                                                       \
    addr = (x);                                                         \
    pix = rdram32[addr];                                                \
    tempr = (pix >> 27) & 0x1f;                                         \
    tempg = (pix >> 19) & 0x1f;                                         \
    tempb = (pix >> 11) & 0x1f;                                         \
    rend += redptr[tempr];                                              \
    gend += greenptr[tempg];                                            \
    bend += blueptr[tempb];                                             \
}

    if (rdram_valid_idx32(maxpix) && rdram_valid_idx32(leftuppix))
    {
        VI_COMPARE32_OPT(leftuppix);
        VI_COMPARE32_OPT(leftuppix + 1);
        VI_COMPARE32_OPT(leftuppix + 2);
        VI_COMPARE32_OPT(leftdownpix);
        VI_COMPARE32_OPT(leftdownpix + 1);
        VI_COMPARE32_OPT(maxpix);
        VI_COMPARE32_OPT(toleftpix);
        VI_COMPARE32_OPT(toleftpix + 2);
    }
    else
    {
        VI_COMPARE32(leftuppix);
        VI_COMPARE32(leftuppix + 1);
        VI_COMPARE32(leftuppix + 2);
        VI_COMPARE32(leftdownpix);
        VI_COMPARE32(leftdownpix + 1);
        VI_COMPARE32(maxpix);
        VI_COMPARE32(toleftpix);
        VI_COMPARE32(toleftpix + 2);
    }

    *r = rend;
    *g = gend;
    *b = bend;
}

STRICTINLINE void video_max_optimized(uint32_t* pixels, uint32_t* penumin, uint32_t* penumax, int numofels)
{



    int i;
    int posmax = 0, posmin = 0;
    uint32_t curpenmax = pixels[0], curpenmin = pixels[0];
    uint32_t max, min;

    for (i = 1; i < numofels; i++)
    {
        if (pixels[i] > pixels[posmax])
        {
            curpenmax = pixels[posmax];
            posmax = i;
        }
        else if (pixels[i] < pixels[posmin])
        {
            curpenmin = pixels[posmin];
            posmin = i;
        }
    }
    max = pixels[posmax];
    min = pixels[posmin];
    if (curpenmax != max)
    {
        for (i = posmax + 1; i < numofels; i++)
        {
            if (pixels[i] > curpenmax)
                curpenmax = pixels[i];
        }
    }
    if (curpenmin != min)
    {
        for (i = posmin + 1; i < numofels; i++)
        {
            if (pixels[i] < curpenmin)
                curpenmin = pixels[i];
        }
    }
    *penumax = curpenmax;
    *penumin = curpenmin;
}


STRICTINLINE void video_filter16(int* endr, int* endg, int* endb, uint32_t fboffset, uint32_t num, uint32_t hres, uint32_t centercvg, uint32_t fetchbugstate)
{






    uint32_t penumaxr, penumaxg, penumaxb, penuminr, penuming, penuminb;
    uint16_t pix;
    uint32_t numoffull = 1;
    uint8_t hidval;
    uint32_t r, g, b;
    uint32_t backr[7], backg[7], backb[7];

    r = *endr;
    g = *endg;
    b = *endb;

    backr[0] = r;
    backg[0] = g;
    backb[0] = b;





    uint32_t idx = (fboffset >> 1) + num;

    uint32_t toleft = idx - 2;
    uint32_t toright = idx + 2;

    uint32_t leftup, rightup, leftdown, rightdown;

    leftup = idx - hres - 1;
    rightup = idx - hres + 1;










    if (fetchbugstate != 1)
    {
        leftdown = idx + hres - 1;
        rightdown = idx + hres + 1;
    }
    else
    {
        leftdown = toleft;
        rightdown = toright;
    }


#define VI_ANDER(x) {                                                   \
            PAIRREAD16(pix, hidval, (x));                                   \
            if (hidval == 3 && (pix & 1))                               \
            {                                                           \
                backr[numoffull] = GET_HI(pix);                         \
                backg[numoffull] = GET_MED(pix);                        \
                backb[numoffull] = GET_LOW(pix);                        \
                numoffull++;                                            \
            }                                                           \
}

    VI_ANDER(leftup);
    VI_ANDER(rightup);
    VI_ANDER(toleft);
    VI_ANDER(toright);
    VI_ANDER(leftdown);
    VI_ANDER(rightdown);

    uint32_t colr, colg, colb;




    video_max_optimized(backr, &penuminr, &penumaxr, numoffull);
    video_max_optimized(backg, &penuming, &penumaxg, numoffull);
    video_max_optimized(backb, &penuminb, &penumaxb, numoffull);





    uint32_t coeff = 7 - centercvg;
    colr = penuminr + penumaxr - (r << 1);
    colg = penuming + penumaxg - (g << 1);
    colb = penuminb + penumaxb - (b << 1);

    colr = (((colr * coeff) + 4) >> 3) + r;
    colg = (((colg * coeff) + 4) >> 3) + g;
    colb = (((colb * coeff) + 4) >> 3) + b;

    *endr = colr & 0xff;
    *endg = colg & 0xff;
    *endb = colb & 0xff;



}

STRICTINLINE void video_filter32(int* endr, int* endg, int* endb, uint32_t fboffset, uint32_t num, uint32_t hres, uint32_t centercvg, uint32_t fetchbugstate)
{

    uint32_t penumaxr, penumaxg, penumaxb, penuminr, penuming, penuminb;
    uint32_t numoffull = 1;
    uint32_t pix = 0, pixcvg = 0;
    uint32_t r, g, b;
    uint32_t backr[7], backg[7], backb[7];

    r = *endr;
    g = *endg;
    b = *endb;

    backr[0] = r;
    backg[0] = g;
    backb[0] = b;

    uint32_t idx = (fboffset >> 2) + num;

    uint32_t toleft = idx - 2;
    uint32_t toright = idx + 2;

    uint32_t leftup, rightup, leftdown, rightdown;

    leftup = idx - hres - 1;
    rightup = idx - hres + 1;

    if (fetchbugstate != 1)
    {
        leftdown = idx + hres - 1;
        rightdown = idx + hres + 1;
    }
    else
    {
        leftdown = toleft;
        rightdown = toright;
    }

#define VI_ANDER32(x) {                                                 \
            RREADIDX32(pix, (x));                                       \
            pixcvg = (pix >> 5) & 7;                                    \
            if (pixcvg == 7)                                            \
            {                                                           \
                backr[numoffull] = (pix >> 24) & 0xff;                  \
                backg[numoffull] = (pix >> 16) & 0xff;                  \
                backb[numoffull] = (pix >> 8) & 0xff;                   \
                numoffull++;                                            \
            }                                                           \
}

    VI_ANDER32(leftup);
    VI_ANDER32(rightup);
    VI_ANDER32(toleft);
    VI_ANDER32(toright);
    VI_ANDER32(leftdown);
    VI_ANDER32(rightdown);

    uint32_t colr, colg, colb;

    video_max_optimized(backr, &penuminr, &penumaxr, numoffull);
    video_max_optimized(backg, &penuming, &penumaxg, numoffull);
    video_max_optimized(backb, &penuminb, &penumaxb, numoffull);

    uint32_t coeff = 7 - centercvg;
    colr = penuminr + penumaxr - (r << 1);
    colg = penuming + penumaxg - (g << 1);
    colb = penuminb + penumaxb - (b << 1);

    colr = (((colr * coeff) + 4) >> 3) + r;
    colg = (((colg * coeff) + 4) >> 3) + g;
    colb = (((colb * coeff) + 4) >> 3) + b;

    *endr = colr & 0xff;
    *endg = colg & 0xff;
    *endb = colb & 0xff;
}

STRICTINLINE void vi_fetch_filter16(struct ccvg* res, uint32_t fboffset, uint32_t cur_x, uint32_t fsaa, uint32_t dither_filter, uint32_t vres, uint32_t fetchstate)
{
    int r, g, b;
    uint32_t idx = (fboffset >> 1) + cur_x;
    uint8_t hval;
    uint16_t pix;
    uint32_t cur_cvg;
    if (fsaa)
    {
        PAIRREAD16(pix, hval, idx);
        cur_cvg = ((pix & 1) << 2) | hval;
    }
    else
    {
        RREADIDX16(pix, idx);
        cur_cvg = 7;
    }
    r = GET_HI(pix);
    g = GET_MED(pix);
    b = GET_LOW(pix);

    uint32_t fbw = vi_width & 0xfff;

    if (cur_cvg == 7)
    {
        if (dither_filter)
            restore_filter16(&r, &g, &b, fboffset, cur_x, fbw, fetchstate);
    }
    else
    {
        video_filter16(&r, &g, &b, fboffset, cur_x, fbw, cur_cvg, fetchstate);
    }


    res->r = r;
    res->g = g;
    res->b = b;
    res->cvg = cur_cvg;
}

STRICTINLINE void vi_fetch_filter32(struct ccvg* res, uint32_t fboffset, uint32_t cur_x, uint32_t fsaa, uint32_t dither_filter, uint32_t vres, uint32_t fetchstate)
{
    int r, g, b;
    uint32_t pix, addr = (fboffset >> 2) + cur_x;
    RREADIDX32(pix, addr);
    uint32_t cur_cvg;
    if (fsaa)
        cur_cvg = (pix >> 5) & 7;
    else
        cur_cvg = 7;
    r = (pix >> 24) & 0xff;
    g = (pix >> 16) & 0xff;
    b = (pix >> 8) & 0xff;

    uint32_t fbw = vi_width & 0xfff;

    if (cur_cvg == 7)
    {
        if (dither_filter)
            restore_filter32(&r, &g, &b, fboffset, cur_x, fbw, fetchstate);
    }
    else
    {
        video_filter32(&r, &g, &b, fboffset, cur_x, fbw, cur_cvg, fetchstate);
    }

    res->r = r;
    res->g = g;
    res->b = b;
    res->cvg = cur_cvg;
}

STRICTINLINE void divot_filter(struct ccvg* final, struct ccvg centercolor, struct ccvg leftcolor, struct ccvg rightcolor)
{







    uint32_t leftr, leftg, leftb, rightr, rightg, rightb, centerr, centerg, centerb;

    *final = centercolor;

    if ((centercolor.cvg & leftcolor.cvg & rightcolor.cvg) == 7)



    {
        return;
    }

    leftr = leftcolor.r;
    leftg = leftcolor.g;
    leftb = leftcolor.b;
    rightr = rightcolor.r;
    rightg = rightcolor.g;
    rightb = rightcolor.b;
    centerr = centercolor.r;
    centerg = centercolor.g;
    centerb = centercolor.b;


    if ((leftr >= centerr && rightr >= leftr) || (leftr >= rightr && centerr >= leftr))
        final->r = leftr;
    else if ((rightr >= centerr && leftr >= rightr) || (rightr >= leftr && centerr >= rightr))
        final->r = rightr;

    if ((leftg >= centerg && rightg >= leftg) || (leftg >= rightg && centerg >= leftg))
        final->g = leftg;
    else if ((rightg >= centerg && leftg >= rightg) || (rightg >= leftg && centerg >= rightg))
        final->g = rightg;

    if ((leftb >= centerb && rightb >= leftb) || (leftb >= rightb && centerb >= leftb))
        final->b = leftb;
    else if ((rightb >= centerb && leftb >= rightb) || (rightb >= leftb && centerb >= rightb))
        final->b = rightb;
}

STRICTINLINE void gamma_filters(int* r, int* g, int* b, int gamma_and_dither)
{
    int cdith, dith;



    switch(gamma_and_dither)
    {
    case 0:
        return;
        break;
    case 1:
        cdith = irand();
        dith = cdith & 1;
        if (*r < 255)
            *r += dith;
        dith = (cdith >> 1) & 1;
        if (*g < 255)
            *g += dith;
        dith = (cdith >> 2) & 1;
        if (*b < 255)
            *b += dith;
        break;
    case 2:
        *r = gamma_table[*r];
        *g = gamma_table[*g];
        *b = gamma_table[*b];
        break;
    case 3:
        cdith = irand();
        dith = cdith & 0x3f;
        *r = gamma_dither_table[((*r) << 6)|dith];
        dith = (cdith >> 6) & 0x3f;
        *g = gamma_dither_table[((*g) << 6)|dith];
        dith = ((cdith >> 9) & 0x38) | (cdith & 7);
        *b = gamma_dither_table[((*b) << 6)|dith];
        break;
    }
}

STRICTINLINE void vi_vl_lerp(struct ccvg* up, struct ccvg down, uint32_t frac)
{
    uint32_t r0, g0, b0;
    if (!frac)
        return;

    r0 = up->r;
    g0 = up->g;
    b0 = up->b;

    up->r = ((((down.r - r0) * frac + 16) >> 5) + r0) & 0xff;
    up->g = ((((down.g - g0) * frac + 16) >> 5) + g0) & 0xff;
    up->b = ((((down.b - b0) * frac + 16) >> 5) + b0) & 0xff;

}

uint32_t vi_integer_sqrt(uint32_t a)
{
    unsigned long op = a, res = 0, one = 1 << 30;

    while (one > op)
        one >>= 2;

    while (one != 0)
    {
        if (op >= res + one)
        {
            op -= res + one;
            res += one << 1;
        }
        res >>= 1;
        one >>= 2;
    }
    return res;
}

void vi_init(struct core_config* config)
{
    cfg = config;

    for (int i = 0; i < 256; i++)
    {
        gamma_table[i] = vi_integer_sqrt(i << 6);
        gamma_table[i] <<= 1;
    }

    for (int i = 0; i < 0x4000; i++)
    {
        gamma_dither_table[i] = vi_integer_sqrt(i);
        gamma_dither_table[i] <<= 1;
    }

    for (int i = 0; i < 0x400; i++)
    {
        if (((i >> 5) & 0x1f) < (i & 0x1f))
            vi_restore_table[i] = 1;
        else if (((i >> 5) & 0x1f) > (i & 0x1f))
            vi_restore_table[i] = -1;
        else
            vi_restore_table[i] = 0;
    }

    prevvicurrent = 0;
    emucontrolsvicurrent = -1;
    prevserrate = 0;
    oldlowerfield = 0;
    oldvstart = 1337;
    prevwasblank = 0;
}

int vi_process_init(void)
{





    uint32_t final = 0;

















    hres = (vi_h_start & 0x3ff) - ((vi_h_start >> 16) & 0x3ff);

    vres = (vi_v_start & 0x3ff) - ((vi_v_start >> 16) & 0x3ff);
    vres >>= 1;





    dither_filter = (vi_control >> 16) & 1;
    fsaa = !((vi_control >> 9) & 1);
    divot = (vi_control >> 4) & 1;
    gamma = (vi_control >> 3) & 1;
    gamma_dither = (vi_control >> 2) & 1;
    lerp_en = (((vi_control >> 8) & 3) != 3);
    extralines = !((vi_control >> 8) & 1);

    vitype = vi_control & 3;
    serration_pulses = (vi_control >> 6) & 1;
    gamma_and_dither = (gamma << 1) | gamma_dither;
    if (((vi_control >> 5) & 1) && !onetimewarnings.vbusclock)
    {
        msg_warning("rdp_update: vbus_clock_enable bit set in VI_CONTROL_REG register. Never run this code on your N64! It's rumored that turning this bit on\
                    will result in permanent damage to the hardware! Emulation will now continue.\n");
        onetimewarnings.vbusclock = 1;
    }
















    vi_fetch_filter_ptr = vi_fetch_filter_func[vitype & 1];

    ispal = (vi_v_sync & 0x3ff) > 550;










    v_start = (vi_v_start >> 16) & 0x3ff;
    h_start = (vi_h_start >> 16) & 0x3ff;

    x_add = vi_x_scale & 0xfff;



















    if (!lerp_en && vitype == 2 && !onetimewarnings.nolerp && h_start < 0x80 && x_add <= 0x200)
    {
        msg_warning("Disabling VI interpolation in 16-bit color modes causes glitches on hardware if h_start is less than 128 pixels and x_scale is less or equal to 0x200.");
        onetimewarnings.nolerp = 1;
    }

    h_start -= (ispal ? 128 : 108);

    x_start_init = (vi_x_scale >> 16) & 0xfff;

    int h_start_clamped = 0;

    if (h_start < 0)
    {
        x_start_init += (x_add * (-h_start));
        hres += h_start;

        h_start = 0;
        h_start_clamped = 1;
    }





    int32_t v_end = vi_v_start & 0x3ff;
    int32_t v_sync = vi_v_sync & 0x3ff;
























    int validinterlace = (vitype & 2) && serration_pulses;
    if (validinterlace && prevserrate && emucontrolsvicurrent < 0)
        emucontrolsvicurrent = (vi_v_current_line & 1) != prevvicurrent ? 1 : 0;

    int lowerfield = 0;
    if (validinterlace)
    {
        if (emucontrolsvicurrent == 1)
            lowerfield = (vi_v_current_line & 1) ^ 1;
        else if (!emucontrolsvicurrent)
        {
            if (v_start == oldvstart)
                lowerfield = oldlowerfield ^ 1;
            else
                lowerfield = v_start < oldvstart ? 1 : 0;
        }
    }

    oldlowerfield = lowerfield;


    if (validinterlace)
    {
        prevserrate = 1;
        prevvicurrent = vi_v_current_line & 1;
        oldvstart = v_start;
    }
    else
        prevserrate = 0;













    lineshifter = serration_pulses ? 0 : 1;
    int twolines = serration_pulses ? 1 : 0;

    int32_t vstartoffset = ispal ? 44 : 34;
    v_start = (v_start - vstartoffset) / 2;






    y_start = (vi_y_scale >> 16) & 0xfff;
    y_add = vi_y_scale & 0xfff;

    if (v_start < 0)
    {
        y_start += (y_add * (uint32_t)(-v_start));
        v_start = 0;
    }

    int hres_clamped = 0;

    if ((hres + h_start) > PRESCALE_WIDTH)
    {
        hres = PRESCALE_WIDTH - h_start;
        hres_clamped = 1;
    }




    if ((vres + v_start) > PRESCALE_HEIGHT)
    {
        vres = PRESCALE_HEIGHT - v_start;
        msg_warning("vres = %d v_start = %d v_video_start = %d", vres, v_start, (vi_v_start >> 16) & 0x3ff);
    }

    int32_t h_end = hres + h_start;
    int32_t hrightblank = PRESCALE_WIDTH - h_end;

    int vactivelines = (vi_v_sync & 0x3ff) - vstartoffset;
    if (vactivelines > PRESCALE_HEIGHT)
        msg_error("VI_V_SYNC_REG too big");
    if (vactivelines < 0)
        return 0;
    //vactivelines >>= lineshifter;
    vactivelines = (ispal ? 576 : 480) >> lineshifter;

    int validh = (hres > 0 && h_start < PRESCALE_WIDTH);



    uint32_t pix = 0;
    uint8_t cur_cvg = 0;


    int32_t *d = 0;




















    minhpass = h_start_clamped ? 0 : 8;
    maxhpass =  hres_clamped ? hres : (hres - 7);

    if (!(vitype & 2) && prevwasblank)
    {
        return 0;
    }

    screen_get_buffer(PRESCALE_WIDTH, vactivelines, 480, &PreScale, &pitchindwords);

    pitchindwords >>= 2;
    linecount = serration_pulses ? (pitchindwords << 1) : pitchindwords;
    prescale_ptr = v_start * linecount + h_start + (lowerfield ? pitchindwords : 0);










    int i;
    if (!(vitype & 2))
    {
        if (cfg->tv_fading) {
            memset(tvfadeoutstate, 0, PRESCALE_HEIGHT * sizeof(uint32_t));
        }
        //for (i = 0; i < PRESCALE_HEIGHT; i++)
        for (i = 0; i < vactivelines; i++)
            memset(&PreScale[i * pitchindwords], 0, PRESCALE_WIDTH * sizeof(int32_t));
        prevwasblank = 1;
    }
    else
    {
        prevwasblank = 0;

        if (!cfg->tv_fading) {
            return 1;
        }

        int j;
        if (h_start > 0 && h_start < PRESCALE_WIDTH)
        {
            for (i = 0; i < vactivelines; i++)
                memset(&PreScale[i * pitchindwords], 0, h_start * sizeof(uint32_t));
        }
        if (h_end >= 0 && h_end < PRESCALE_WIDTH)
        {
            for (i = 0; i < vactivelines; i++)
                memset(&PreScale[i * pitchindwords + h_end], 0, hrightblank * sizeof(uint32_t));
        }

        for (i = 0; i < ((v_start << twolines) + (lowerfield ? 1 : 0)); i++)
        {
            if (tvfadeoutstate[i])
            {
                tvfadeoutstate[i]--;
                if (!tvfadeoutstate[i])
                {
                    if (validh)
                        memset(&PreScale[i * pitchindwords + h_start], 0, hres * sizeof(uint32_t));
                    else
                        memset(&PreScale[i * pitchindwords], 0, PRESCALE_WIDTH * sizeof(uint32_t));
                }
            }
        }
        if (!serration_pulses)
        {
            for(j = 0; j < vres; j++)
            {
                if (validh)
                    tvfadeoutstate[i] = 2;
                else if (tvfadeoutstate[i])
                {
                    tvfadeoutstate[i]--;
                    if (!tvfadeoutstate[i])
                    {
                        memset(&PreScale[i * pitchindwords], 0, PRESCALE_WIDTH * sizeof(uint32_t));
                    }
                }

                i++;
            }
        }
        else
        {
            for(j = 0; j < vres; j++)
            {
                if (validh)
                    tvfadeoutstate[i] = 2;
                else if (tvfadeoutstate[i])
                {
                    tvfadeoutstate[i]--;
                    if (!tvfadeoutstate[i])
                        memset(&PreScale[i * pitchindwords], 0, PRESCALE_WIDTH * sizeof(uint32_t));
                }

                if (tvfadeoutstate[i + 1])
                {
                    tvfadeoutstate[i + 1]--;
                    if (!tvfadeoutstate[i + 1])
                        if (validh)
                            memset(&PreScale[(i + 1) * pitchindwords + h_start], 0, hres * sizeof(uint32_t));
                        else
                            memset(&PreScale[(i + 1) * pitchindwords], 0, PRESCALE_WIDTH * sizeof(uint32_t));
                }

                i += 2;
            }
        }
        for (; i < vactivelines; i++)
        {
            if (tvfadeoutstate[i])
                tvfadeoutstate[i]--;
            if (!tvfadeoutstate[i])
                if (validh)
                    memset(&PreScale[i * pitchindwords + h_start], 0, hres * sizeof(uint32_t));
                else
                    memset(&PreScale[i * pitchindwords], 0, PRESCALE_WIDTH * sizeof(uint32_t));
        }
    }

    return 1;
}

void vi_process(void)
{
    struct ccvg viaa_array[0xa10 << 1];
    struct ccvg divot_array[0xa10 << 1];

    int cache_marker = 0, cache_next_marker = 0, divot_cache_marker = 0, divot_cache_next_marker = 0;
    int cache_marker_init = (x_start_init >> 10) - 1;

    struct ccvg *viaa_cache = &viaa_array[0];
    struct ccvg *viaa_cache_next = &viaa_array[0xa10];
    struct ccvg *divot_cache = &divot_array[0];
    struct ccvg *divot_cache_next = &divot_array[0xa10];

    struct ccvg color, nextcolor, scancolor, scannextcolor;

    uint32_t pixels = 0, nextpixels = 0, fetchbugstate = 0;

    int r = 0, g = 0, b = 0;
    int xfrac = 0, yfrac = 0;
    int vi_width_low = vi_width & 0xfff;
    int line_x = 0, next_line_x = 0, prev_line_x = 0, far_line_x = 0;
    int prev_scan_x = 0, scan_x = 0, next_scan_x = 0, far_scan_x = 0;
    int prev_x = 0, cur_x = 0, next_x = 0, far_x = 0;

    bool cache_init = false;

    switch (vitype)
    {
        case 0:
        case 1:
        {
            break;
        }

        case 2:
        case 3:
        {
            pixels = 0;
#undef RENDER_CVG_BITS16
#undef RENDER_CVG_BITS32
#undef RENDER_MIN_CVG_ONLY
#undef RENDER_MAX_CVG_ONLY

#undef MONITOR_Z
#undef BW_ZBUFFER
#undef ZBUFF_AS_16B_IATEXTURE

#ifdef MONITOR_Z
            uint32_t frame_buffer = zb_address;
#else
            uint32_t frame_buffer = vi_origin & 0xffffff;
#endif

            if (frame_buffer)
            {
                int32_t j_start = 0;
                int32_t j_end = vres;
                int32_t j_add = 1;

                if (cfg->parallel) {
                    j_start = parallel_worker_id();
                    j_add = parallel_worker_num();
                }

                for (int32_t j = j_start; j < j_end; j += j_add) {
                    uint32_t x_start = x_start_init;
                    uint32_t curry = y_start + j * y_add;
                    uint32_t nexty = y_start + (j + 1) * y_add;
                    uint32_t prevy = curry >> 10;

                    cache_marker = cache_next_marker = cache_marker_init;
                    if (divot)
                        divot_cache_marker = divot_cache_next_marker = cache_marker_init;

                    int* d = PreScale + prescale_ptr + linecount * j;

                    yfrac = (curry >> 5) & 0x1f;
                    pixels = vi_width_low * prevy;
                    nextpixels = vi_width_low + pixels;

                    if (prevy == (nexty >> 10))
                        fetchbugstate = 2;
                    else
                        fetchbugstate >>= 1;

                    for (int i = 0; i < hres; i++, x_start += x_add)
                    {
                        line_x = x_start >> 10;
                        prev_line_x = line_x - 1;
                        next_line_x = line_x + 1;
                        far_line_x = line_x + 2;

                        cur_x = pixels + line_x;
                        prev_x = pixels + prev_line_x;
                        next_x = pixels + next_line_x;
                        far_x = pixels + far_line_x;


                        scan_x = nextpixels + line_x;
                        prev_scan_x = nextpixels + prev_line_x;
                        next_scan_x = nextpixels + next_line_x;
                        far_scan_x = nextpixels + far_line_x;


                        line_x++;
                        prev_line_x++;
                        next_line_x++;
                        far_line_x++;

                        xfrac = (x_start >> 5) & 0x1f;

                        int lerping = lerp_en && (xfrac || yfrac);


                        if (prev_line_x > cache_marker)
                        {
                            vi_fetch_filter_ptr(&viaa_cache[prev_line_x], frame_buffer, prev_x, fsaa, dither_filter, vres, 0);
                            vi_fetch_filter_ptr(&viaa_cache[line_x], frame_buffer, cur_x, fsaa, dither_filter, vres, 0);
                            vi_fetch_filter_ptr(&viaa_cache[next_line_x], frame_buffer, next_x, fsaa, dither_filter, vres, 0);
                            cache_marker = next_line_x;
                        }
                        else if (line_x > cache_marker)
                        {
                            vi_fetch_filter_ptr(&viaa_cache[line_x], frame_buffer, cur_x, fsaa, dither_filter, vres, 0);
                            vi_fetch_filter_ptr(&viaa_cache[next_line_x], frame_buffer, next_x, fsaa, dither_filter, vres, 0);
                            cache_marker = next_line_x;
                        }
                        else if (next_line_x > cache_marker)
                        {
                            vi_fetch_filter_ptr(&viaa_cache[next_line_x], frame_buffer, next_x, fsaa, dither_filter, vres, 0);
                            cache_marker = next_line_x;
                        }

                        if (prev_line_x > cache_next_marker)
                        {
                            vi_fetch_filter_ptr(&viaa_cache_next[prev_line_x], frame_buffer, prev_scan_x, fsaa, dither_filter, vres, fetchbugstate);
                            vi_fetch_filter_ptr(&viaa_cache_next[line_x], frame_buffer, scan_x, fsaa, dither_filter, vres, fetchbugstate);
                            vi_fetch_filter_ptr(&viaa_cache_next[next_line_x], frame_buffer, next_scan_x, fsaa, dither_filter, vres, fetchbugstate);
                            cache_next_marker = next_line_x;
                        }
                        else if (line_x > cache_next_marker)
                        {
                            vi_fetch_filter_ptr(&viaa_cache_next[line_x], frame_buffer, scan_x, fsaa, dither_filter, vres, fetchbugstate);
                            vi_fetch_filter_ptr(&viaa_cache_next[next_line_x], frame_buffer, next_scan_x, fsaa, dither_filter, vres, fetchbugstate);
                            cache_next_marker = next_line_x;
                        }
                        else if (next_line_x > cache_next_marker)
                        {
                            vi_fetch_filter_ptr(&viaa_cache_next[next_line_x], frame_buffer, next_scan_x, fsaa, dither_filter, vres, fetchbugstate);
                            cache_next_marker = next_line_x;
                        }


                        if (divot)
                        {
                            if (far_line_x > cache_marker)
                            {
                                vi_fetch_filter_ptr(&viaa_cache[far_line_x], frame_buffer, far_x, fsaa, dither_filter, vres, 0);
                                cache_marker = far_line_x;
                            }

                            if (far_line_x > cache_next_marker)
                            {
                                vi_fetch_filter_ptr(&viaa_cache_next[far_line_x], frame_buffer, far_scan_x, fsaa, dither_filter, vres, fetchbugstate);
                                cache_next_marker = far_line_x;
                            }

                            if (line_x > divot_cache_marker)
                            {
                                divot_filter(&divot_cache[line_x], viaa_cache[line_x], viaa_cache[prev_line_x], viaa_cache[next_line_x]);
                                divot_filter(&divot_cache[next_line_x], viaa_cache[next_line_x], viaa_cache[line_x], viaa_cache[far_line_x]);
                                divot_cache_marker = next_line_x;
                            }
                            else if (next_line_x > divot_cache_marker)
                            {
                                divot_filter(&divot_cache[next_line_x], viaa_cache[next_line_x], viaa_cache[line_x], viaa_cache[far_line_x]);
                                divot_cache_marker = next_line_x;
                            }

                            if (line_x > divot_cache_next_marker)
                            {
                                divot_filter(&divot_cache_next[line_x], viaa_cache_next[line_x], viaa_cache_next[prev_line_x], viaa_cache_next[next_line_x]);
                                divot_filter(&divot_cache_next[next_line_x], viaa_cache_next[next_line_x], viaa_cache_next[line_x], viaa_cache_next[far_line_x]);
                                divot_cache_next_marker = next_line_x;
                            }
                            else if (next_line_x > divot_cache_next_marker)
                            {
                                divot_filter(&divot_cache_next[next_line_x], viaa_cache_next[next_line_x], viaa_cache_next[line_x], viaa_cache_next[far_line_x]);
                                divot_cache_next_marker = next_line_x;
                            }

                            color = divot_cache[line_x];

                        }
                        else
                        {
                            color = viaa_cache[line_x];
                        }

                        if (lerping)
                        {
                            if (divot)
                            {
                                nextcolor = divot_cache[next_line_x];
                                scancolor = divot_cache_next[line_x];
                                scannextcolor = divot_cache_next[next_line_x];
                            }
                            else
                            {
                                nextcolor = viaa_cache[next_line_x];
                                scancolor = viaa_cache_next[line_x];
                                scannextcolor = viaa_cache_next[next_line_x];
                            }



                            vi_vl_lerp(&color, scancolor, yfrac);
                            vi_vl_lerp(&nextcolor, scannextcolor, yfrac);
                            vi_vl_lerp(&color, nextcolor, xfrac);
                        }

                        r = color.r;
                        g = color.g;
                        b = color.b;

                        gamma_filters(&r, &g, &b, gamma_and_dither);


#ifdef BW_ZBUFFER
                        uint32_t tempz = RREADIDX16((frame_buffer >> 1) + cur_x);
                        pix = tempz;

                        r = g = b = pix >> 8;

#endif
#ifdef ZBUFF_AS_16B_IATEXTURE
                        r = g = b = (((pix >> 8) & 0xff) * (pix & 0xff)) >> 8;
#endif
#ifdef RENDER_CVG_BITS16

                        r = g = b = cur_cvg << 5;
#endif
#ifdef RENDER_CVG_BITS32

                        r = g = b = cur_cvg << 5;
#endif
#ifdef RENDER_MIN_CVG_ONLY
                        if (!cur_cvg)
                            r = g = b = 0;
                        else
                            r = g =  b = 0xff;
#endif
#ifdef RENDER_MAX_CVG_ONLY
                        if (cur_cvg != 7)
                            r = g = b = 0;
                        else
                            r = g = b = 0xff;
#endif

                        if (i >= minhpass && i < maxhpass)
                            d[i] = (r << 16) | (g << 8) | b;
                        else
                            d[i] = 0;
                    }

                    if (!cache_init && y_add == 0x400) {
                        cache_marker = cache_next_marker;
                        cache_next_marker = cache_marker_init;

                        struct ccvg* tempccvgptr = viaa_cache;
                        viaa_cache = viaa_cache_next;
                        viaa_cache_next = tempccvgptr;
                        if (divot)
                        {
                            divot_cache_marker = divot_cache_next_marker;
                            divot_cache_next_marker = cache_marker_init;
                            tempccvgptr = divot_cache;
                            divot_cache = divot_cache_next;
                            divot_cache_next = tempccvgptr;
                        }

                        cache_init = true;
                    }
                }
            }
            break;
        }
        default:    msg_warning("Unknown framebuffer format %d\n", vi_control & 0x3);
    }
}

void vi_update(void)
{
    // try to init VI frame, abort if there's nothing to display
    if (!vi_process_init()) {
        return;
    }

    // run filter update in parallel if enabled
    if (cfg->parallel) {
        parallel_run(vi_process);
    } else {
        vi_process();
    }

    // render frame to screen
    screen_swap();
}
