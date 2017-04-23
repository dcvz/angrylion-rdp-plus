#include "plugin.h"
#include "gfx_1.3.h"
#include "memutil.h"

#define SP_INTERRUPT    0x1
#define SI_INTERRUPT    0x2
#define AI_INTERRUPT    0x4
#define VI_INTERRUPT    0x8
#define PI_INTERRUPT    0x10
#define DP_INTERRUPT    0x20

extern GFX_INFO gfx;

static size_t rdram_size;

static struct {
    DP_REGISTER reg;
    PDWORD* ptr;
} dp_reg[] = {
    {DP_START,          &gfx.DPC_START_REG},
    {DP_END,            &gfx.DPC_END_REG},
    {DP_CURRENT,        &gfx.DPC_CURRENT_REG},
    {DP_STATUS,         &gfx.DPC_STATUS_REG},
    {DP_CLOCK,          &gfx.DPC_CLOCK_REG},
    {DP_BUFBUSY,        &gfx.DPC_BUFBUSY_REG},
    {DP_PIPEBUSY,       &gfx.DPC_PIPEBUSY_REG},
    {DP_TMEM,           &gfx.DPC_TMEM_REG},
};

static struct {
    VI_REGISTER reg;
    PDWORD* ptr;
} vi_reg[] = {
    {VI_STATUS,         &gfx.VI_STATUS_REG},
    {VI_ORIGIN,         &gfx.VI_ORIGIN_REG},
    {VI_WIDTH,          &gfx.VI_WIDTH_REG},
    {VI_INTR,           &gfx.VI_INTR_REG},
    {VI_V_CURRENT_LINE, &gfx.VI_V_CURRENT_LINE_REG},
    {VI_TIMING,         &gfx.VI_TIMING_REG},
    {VI_V_SYNC,         &gfx.VI_V_SYNC_REG},
    {VI_H_SYNC,         &gfx.VI_H_SYNC_REG},
    {VI_LEAP,           &gfx.VI_LEAP_REG},
    {VI_H_START,        &gfx.VI_H_START_REG},
    {VI_V_START,        &gfx.VI_V_START_REG},
    {VI_V_BURST,        &gfx.VI_V_BURST_REG},
    {VI_X_SCALE,        &gfx.VI_X_SCALE_REG},
    {VI_Y_SCALE,        &gfx.VI_Y_SCALE_REG},
};

void plugin_interrupt(void)
{
    *gfx.MI_INTR_REG |= DP_INTERRUPT;
    gfx.CheckInterrupts();
}

uint32_t* plugin_dp_register(DP_REGISTER reg)
{
    return (uint32_t*)*dp_reg[reg].ptr;
}

uint32_t* plugin_vi_register(VI_REGISTER reg)
{
    return (uint32_t*)*vi_reg[reg].ptr;
}

uint8_t* plugin_rdram(void)
{
    return gfx.RDRAM;
}

size_t plugin_rdram_size(void)
{
    if (!rdram_size) {
        // Any current plugin specifications have never told the graphics plugin
        // how much RDRAM is allocated, so use a Win32 hack to detect it.
        rdram_size = IsValidPtrW32(&gfx.RDRAM[0x7f0000], 16) ? 0x800000 : 0x400000;
    }
    return rdram_size;
}

uint8_t* plugin_dmem(void)
{
    return gfx.DMEM;
}
