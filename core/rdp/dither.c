static void rgb_dither_complete(int* r, int* g, int* b, int dith);
static void rgb_dither_nothing(int* r, int* g, int* b, int dith);
static void get_dither_noise_complete(int x, int y, int* cdith, int* adith);
static void get_dither_only(int x, int y, int* cdith, int* adith);
static void get_dither_nothing(int x, int y, int* cdith, int* adith);

static void (*get_dither_noise_func[3])(int, int, int*, int*) =
{
    get_dither_noise_complete, get_dither_only, get_dither_nothing
};

static void (*rgb_dither_func[2])(int*, int*, int*, int) =
{
    rgb_dither_complete, rgb_dither_nothing
};

static const uint8_t bayer_matrix[16] =
{
     0,  4,  1, 5,
     4,  0,  5, 1,
     3,  7,  2, 6,
     7,  3,  6, 2
};


static const uint8_t magic_matrix[16] =
{
     0,  6,  1, 7,
     4,  2,  5, 3,
     3,  5,  2, 4,
     7,  1,  6, 0
};

static TLS void (*get_dither_noise_ptr)(int, int, int*, int*);
static TLS void (*rgb_dither_ptr)(int*, int*, int*, int);

static void rgb_dither_complete(int* r, int* g, int* b, int dith)
{

    int32_t newr = *r, newg = *g, newb = *b;
    int32_t rcomp, gcomp, bcomp;


    if (newr > 247)
        newr = 255;
    else
        newr = (newr & 0xf8) + 8;
    if (newg > 247)
        newg = 255;
    else
        newg = (newg & 0xf8) + 8;
    if (newb > 247)
        newb = 255;
    else
        newb = (newb & 0xf8) + 8;

    if (other_modes.rgb_dither_sel != 2)
        rcomp = gcomp = bcomp = dith;
    else
    {
        rcomp = dith & 7;
        gcomp = (dith >> 3) & 7;
        bcomp = (dith >> 6) & 7;
    }





    int32_t replacesign = (rcomp - (*r & 7)) >> 31;

    int32_t ditherdiff = newr - *r;
    *r = *r + (ditherdiff & replacesign);

    replacesign = (gcomp - (*g & 7)) >> 31;
    ditherdiff = newg - *g;
    *g = *g + (ditherdiff & replacesign);

    replacesign = (bcomp - (*b & 7)) >> 31;
    ditherdiff = newb - *b;
    *b = *b + (ditherdiff & replacesign);

}

static void rgb_dither_nothing(int* r, int* g, int* b, int dith)
{
}


static void get_dither_noise_complete(int x, int y, int* cdith, int* adith)
{


    noise = ((irand() & 7) << 6) | 0x20;


    int dithindex;
    switch(other_modes.f.rgb_alpha_dither)
    {
    case 0:
        dithindex = ((y & 3) << 2) | (x & 3);
        *adith = *cdith = magic_matrix[dithindex];
        break;
    case 1:
        dithindex = ((y & 3) << 2) | (x & 3);
        *cdith = magic_matrix[dithindex];
        *adith = (~(*cdith)) & 7;
        break;
    case 2:
        dithindex = ((y & 3) << 2) | (x & 3);
        *cdith = magic_matrix[dithindex];
        *adith = (noise >> 6) & 7;
        break;
    case 3:
        dithindex = ((y & 3) << 2) | (x & 3);
        *cdith = magic_matrix[dithindex];
        *adith = 0;
        break;
    case 4:
        dithindex = ((y & 3) << 2) | (x & 3);
        *adith = *cdith = bayer_matrix[dithindex];
        break;
    case 5:
        dithindex = ((y & 3) << 2) | (x & 3);
        *cdith = bayer_matrix[dithindex];
        *adith = (~(*cdith)) & 7;
        break;
    case 6:
        dithindex = ((y & 3) << 2) | (x & 3);
        *cdith = bayer_matrix[dithindex];
        *adith = (noise >> 6) & 7;
        break;
    case 7:
        dithindex = ((y & 3) << 2) | (x & 3);
        *cdith = bayer_matrix[dithindex];
        *adith = 0;
        break;
    case 8:
        dithindex = ((y & 3) << 2) | (x & 3);
        *cdith = irand();
        *adith = magic_matrix[dithindex];
        break;
    case 9:
        dithindex = ((y & 3) << 2) | (x & 3);
        *cdith = irand();
        *adith = (~magic_matrix[dithindex]) & 7;
        break;
    case 10:
        *cdith = irand();
        *adith = (noise >> 6) & 7;
        break;
    case 11:
        *cdith = irand();
        *adith = 0;
        break;
    case 12:
        dithindex = ((y & 3) << 2) | (x & 3);
        *cdith = 7;
        *adith = bayer_matrix[dithindex];
        break;
    case 13:
        dithindex = ((y & 3) << 2) | (x & 3);
        *cdith = 7;
        *adith = (~bayer_matrix[dithindex]) & 7;
        break;
    case 14:
        *cdith = 7;
        *adith = (noise >> 6) & 7;
        break;
    case 15:
        *cdith = 7;
        *adith = 0;
        break;
    }
}


static void get_dither_only(int x, int y, int* cdith, int* adith)
{
    int dithindex;
    switch(other_modes.f.rgb_alpha_dither)
    {
    case 0:
        dithindex = ((y & 3) << 2) | (x & 3);
        *adith = *cdith = magic_matrix[dithindex];
        break;
    case 1:
        dithindex = ((y & 3) << 2) | (x & 3);
        *cdith = magic_matrix[dithindex];
        *adith = (~(*cdith)) & 7;
        break;
    case 2:
        dithindex = ((y & 3) << 2) | (x & 3);
        *cdith = magic_matrix[dithindex];
        *adith = (noise >> 6) & 7;
        break;
    case 3:
        dithindex = ((y & 3) << 2) | (x & 3);
        *cdith = magic_matrix[dithindex];
        *adith = 0;
        break;
    case 4:
        dithindex = ((y & 3) << 2) | (x & 3);
        *adith = *cdith = bayer_matrix[dithindex];
        break;
    case 5:
        dithindex = ((y & 3) << 2) | (x & 3);
        *cdith = bayer_matrix[dithindex];
        *adith = (~(*cdith)) & 7;
        break;
    case 6:
        dithindex = ((y & 3) << 2) | (x & 3);
        *cdith = bayer_matrix[dithindex];
        *adith = (noise >> 6) & 7;
        break;
    case 7:
        dithindex = ((y & 3) << 2) | (x & 3);
        *cdith = bayer_matrix[dithindex];
        *adith = 0;
        break;
    case 8:
        dithindex = ((y & 3) << 2) | (x & 3);
        *cdith = irand();
        *adith = magic_matrix[dithindex];
        break;
    case 9:
        dithindex = ((y & 3) << 2) | (x & 3);
        *cdith = irand();
        *adith = (~magic_matrix[dithindex]) & 7;
        break;
    case 10:
        *cdith = irand();
        *adith = (noise >> 6) & 7;
        break;
    case 11:
        *cdith = irand();
        *adith = 0;
        break;
    case 12:
        dithindex = ((y & 3) << 2) | (x & 3);
        *cdith = 7;
        *adith = bayer_matrix[dithindex];
        break;
    case 13:
        dithindex = ((y & 3) << 2) | (x & 3);
        *cdith = 7;
        *adith = (~bayer_matrix[dithindex]) & 7;
        break;
    case 14:
        *cdith = 7;
        *adith = (noise >> 6) & 7;
        break;
    case 15:
        *cdith = 7;
        *adith = 0;
        break;
    }
}

static void get_dither_nothing(int x, int y, int* cdith, int* adith)
{
}

static void dither_init(void)
{
    get_dither_noise_ptr = get_dither_noise_func[0];
    rgb_dither_ptr = rgb_dither_func[0];
}
