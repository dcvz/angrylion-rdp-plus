#include "plugin.h"
#include "gfx_1.3.h"

#define SP_INTERRUPT    0x1
#define SI_INTERRUPT    0x2
#define AI_INTERRUPT    0x4
#define VI_INTERRUPT    0x8
#define PI_INTERRUPT    0x10
#define DP_INTERRUPT    0x20

extern GFX_INFO gfx;

static size_t rdram_size;
static uint8_t* rdram_hidden_bits;

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

static bool is_valid_ptr(void *ptr, uint32_t bytes)
{
    SIZE_T dwSize;
    MEMORY_BASIC_INFORMATION meminfo;
    if (!ptr) {
        return false;
    }
    memset(&meminfo, 0x00, sizeof(meminfo));
    dwSize = VirtualQuery(ptr, &meminfo, sizeof(meminfo));
    if (!dwSize) {
        return false;
    }
    if (MEM_COMMIT != meminfo.State) {
        return false;
    }
    if (!(meminfo.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) {
        return false;
    }
    if (bytes > meminfo.RegionSize) {
        return false;
    }
    if ((uint64_t)((char*)ptr - (char*)meminfo.BaseAddress) > (uint64_t)(meminfo.RegionSize - bytes)) {
        return false;
    }
    return true;
}

void plugin_init(void)
{
    // Zilmar plugins can't know how much RDRAM is allocated, so use a Win32 hack
    // to detect it
    rdram_size = is_valid_ptr(&gfx.RDRAM[0x7f0000], 16) ? 0x800000 : 0x400000;

    // Zilmar plugins also can't access the hidden bits, so allocate it on our own
    rdram_hidden_bits = malloc(rdram_size);
    memset(rdram_hidden_bits, 3, rdram_size);
}

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

uint8_t* plugin_rdram_hidden(void)
{
    return rdram_hidden_bits;
}

size_t plugin_rdram_size(void)
{
    return rdram_size;
}

uint8_t* plugin_dmem(void)
{
    return gfx.DMEM;
}

void plugin_close(void)
{
    if (rdram_hidden_bits) {
        free(rdram_hidden_bits);
        rdram_hidden_bits = NULL;
    }
}