#include "screen.h"
#include "gfx_1.3.h"
#include "msg.h"

#include <ddraw.h>

static LPDIRECTDRAW7 lpdd = 0;
static LPDIRECTDRAWSURFACE7 lpddsprimary;
static LPDIRECTDRAWSURFACE7 lpddsback;
static DDSURFACEDESC2 ddsd;
static HRESULT res;
static RECT dst, src;

extern GFX_INFO gfx;

void screen_init(int width, int height, int screen_width, int screen_height)
{
    RECT bigrect, smallrect, statusrect;

    GetWindowRect(gfx.hWnd,&bigrect);
    GetClientRect(gfx.hWnd,&smallrect);
    int rightdiff = screen_width - smallrect.right;
    int bottomdiff = screen_height - smallrect.bottom;
    if (gfx.hStatusBar)
    {
        GetClientRect(gfx.hStatusBar, &statusrect);
        bottomdiff += statusrect.bottom;
    }
    MoveWindow(gfx.hWnd, bigrect.left, bigrect.top, bigrect.right - bigrect.left + rightdiff, bigrect.bottom - bigrect.top + bottomdiff, TRUE);


    DDPIXELFORMAT ftpixel;
    LPDIRECTDRAWCLIPPER lpddcl;

    res = DirectDrawCreateEx(0, (LPVOID*)&lpdd, &IID_IDirectDraw7, 0);
    if(res != DD_OK)
        msg_error("Couldn't create a DirectDraw object");
    res = IDirectDraw_SetCooperativeLevel(lpdd, gfx.hWnd, DDSCL_NORMAL);
    if(res != DD_OK)
        msg_error("Couldn't set a cooperative level. Error code %x", res);

    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;


    res = IDirectDraw_CreateSurface(lpdd, &ddsd, &lpddsprimary, 0);
    if(res != DD_OK)
        msg_error("CreateSurface for a primary surface failed. Error code %x", res);


    ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
    ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
    ddsd.dwWidth = width;
    ddsd.dwHeight = height;
    memset(&ftpixel, 0, sizeof(ftpixel));
    ftpixel.dwSize = sizeof(ftpixel);
    ftpixel.dwFlags = DDPF_RGB;
    ftpixel.dwRGBBitCount = 32;
    ftpixel.dwRBitMask = 0xff0000;
    ftpixel.dwGBitMask = 0xff00;
    ftpixel.dwBBitMask = 0xff;
    ddsd.ddpfPixelFormat = ftpixel;
    res = IDirectDraw_CreateSurface(lpdd, &ddsd, &lpddsback, 0);
    if (res == DDERR_INVALIDPIXELFORMAT)
        msg_error("ARGB8888 is not supported. You can try changing desktop color depth to 32-bit, but most likely that won't help.");
    else if(res != DD_OK)
        msg_error("CreateSurface for a secondary surface failed. Error code %x", res);


    res = IDirectDrawSurface_GetSurfaceDesc(lpddsback, &ddsd);
    if (res != DD_OK)
        msg_error("GetSurfaceDesc failed.");
    if ((ddsd.lPitch & 3) || ddsd.lPitch < (width << 2))
        msg_error("Pitch of a secondary surface is either not 32 bit aligned or two small.");


    res = IDirectDraw_CreateClipper(lpdd, 0, &lpddcl, 0);
    if (res != DD_OK)
        msg_error("Couldn't create a clipper.");
    res = IDirectDrawClipper_SetHWnd(lpddcl, 0, gfx.hWnd);
    if (res != DD_OK)
        msg_error("Couldn't register a windows handle as a clipper.");
    res = IDirectDrawSurface_SetClipper(lpddsprimary, lpddcl);
    if (res != DD_OK)
        msg_error("Couldn't attach a clipper to a surface.");


    src.top = src.left = 0;
    src.bottom = 0;
    src.right = width;




    POINT p;
    p.x = p.y = 0;
    GetClientRect(gfx.hWnd, &dst);
    ClientToScreen(gfx.hWnd, &p);
    OffsetRect(&dst, p.x, p.y);
    GetClientRect(gfx.hStatusBar, &statusrect);
    dst.bottom -= statusrect.bottom;



    DDBLTFX ddbltfx;
    ddbltfx.dwSize = sizeof(DDBLTFX);
    ddbltfx.dwFillColor = 0;
    res = IDirectDrawSurface_Blt(lpddsprimary, &dst, 0, 0, DDBLT_COLORFILL | DDBLT_WAIT, &ddbltfx);
    src.bottom = height;
    res = IDirectDrawSurface_Blt(lpddsback, &src, 0, 0, DDBLT_COLORFILL | DDBLT_WAIT, &ddbltfx);
}

void screen_lock(int** prescale, int* pitch)
{
    res = IDirectDrawSurface_Lock(lpddsback, 0, &ddsd, DDLOCK_SURFACEMEMORYPTR | DDLOCK_NOSYSLOCK, 0);
    if (res == DDERR_SURFACELOST)
    {
        while(1){
        res = IDirectDrawSurface_Restore(lpddsback);
        if (res != DD_OK)
            msg_error("Restore failed with DirectDraw error %x", res);
        res = IDirectDrawSurface_Lock(lpddsback, 0, &ddsd, DDLOCK_SURFACEMEMORYPTR | DDLOCK_NOSYSLOCK, 0);
        if (res != DDERR_SURFACELOST)
            break;
        };
    }
    else if (res != DD_OK)
        msg_error("Lock failed with DirectDraw error %x", res);

    *prescale = (int*)ddsd.lpSurface;
    *pitch = ddsd.lPitch >> 2;
}

void screen_unlock()
{
    res = IDirectDrawSurface_Unlock(lpddsback, 0);
    if (res != DD_OK && res != DDERR_GENERIC && res != DDERR_SURFACELOST)
        msg_error("Couldn't unlock the offscreen surface with DirectDraw error %x", res);
}

void screen_swap(int visiblelines)
{
    src.bottom = visiblelines;

    if (dst.left < dst.right && dst.top < dst.bottom)
    {
        res = IDirectDrawSurface_Blt(lpddsprimary, &dst, lpddsback, &src, DDBLT_WAIT, 0);
        if (res == DDERR_SURFACELOST)
        {
            while(1){
            res = IDirectDraw4_RestoreAllSurfaces(lpdd);
            if (res != DD_OK)
                msg_error("RestoreAllSurfaces failed with DirectDraw error %x", res);
            res = IDirectDrawSurface_Blt(lpddsprimary, &dst, lpddsback, &src, DDBLT_WAIT, 0);
            if (res != DDERR_SURFACELOST)
                break;
            }
        }
        else if (res != DD_OK && res != DDERR_GENERIC && res != DDERR_OUTOFMEMORY)
            msg_error("Scaled blit failed with DirectDraw error %x", res);

    }
}

void screen_move()
{
    RECT statusrect;
    POINT p;
    p.x = p.y = 0;
    GetClientRect(gfx.hWnd, &dst);
    ClientToScreen(gfx.hWnd, &p);
    OffsetRect(&dst, p.x, p.y);
    GetClientRect(gfx.hStatusBar, &statusrect);
    dst.bottom -= statusrect.bottom;
}

void screen_close()
{
    if (lpddsback)
    {
        IDirectDrawSurface_Release(lpddsback);
        lpddsback = 0;
    }
    if (lpddsprimary)
    {
        IDirectDrawSurface_Release(lpddsprimary);
        lpddsprimary = 0;
    }
    if (lpdd)
    {
        IDirectDraw_Release(lpdd);
        lpdd = 0;
    }
}
