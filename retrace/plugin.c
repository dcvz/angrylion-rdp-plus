#include "plugin.h"
#include "retrace.h"

#include <memory.h>

#define RDRAM_MAX_SIZE 0x800000

static uint8_t rdram[RDRAM_MAX_SIZE];
static uint8_t rdram_hidden_bits[RDRAM_MAX_SIZE];
static uint8_t dmem[0x1000];

static uint32_t dp_reg[DP_NUM_REG];
static uint32_t vi_reg[VI_NUM_REG];

static uint32_t rdram_size;

void plugin_init(void)
{
}

void plugin_interrupt(void)
{
}

uint32_t* plugin_dp_register(enum dp_register reg)
{
    return &dp_reg[reg];
}

uint32_t* plugin_vi_register(enum vi_register reg)
{
    return &vi_reg[reg];
}

uint8_t* plugin_rdram(void)
{
    return rdram;
}

uint8_t* plugin_rdram_hidden(void)
{
    return rdram_hidden_bits;
}

uint32_t plugin_rdram_size(void)
{
    return rdram_size;
}

uint8_t* plugin_dmem(void)
{
    return dmem;
}

void plugin_close(void)
{
}

uint32_t plugin_rom_name(char* name, uint32_t name_size)
{
    return 0;
}

void plugin_set_rdram_size(uint32_t size)
{
    rdram_size = size;
}
