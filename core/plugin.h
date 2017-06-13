#pragma once

#include <stdint.h>
#include <stdbool.h>

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

enum dp_register
{
    DP_START,
    DP_END,
    DP_CURRENT,
    DP_STATUS,
    DP_CLOCK,
    DP_BUFBUSY,
    DP_PIPEBUSY,
    DP_TMEM,
    DP_NUM_REG
};

enum vi_register
{
    VI_STATUS,
    VI_ORIGIN,
    VI_WIDTH,
    VI_INTR,
    VI_V_CURRENT_LINE,
    VI_TIMING,
    VI_V_SYNC,
    VI_H_SYNC,
    VI_LEAP,
    VI_H_START,
    VI_V_START,
    VI_V_BURST,
    VI_X_SCALE,
    VI_Y_SCALE,
    VI_NUM_REG
};

// RDRAM pointers
#define rdram32                 ((uint32_t*)plugin_rdram())
#define rdram16                 ((uint16_t*)plugin_rdram())
#define rdram8                  (plugin_rdram())
#define rdram_hidden            (plugin_rdram_hidden())

// SP memory pointers
#define sp_dmem32               ((uint32_t*)plugin_dmem())
#define sp_dmem16               ((uint16_t*)plugin_dmem())
#define sp_dmem8                (plugin_dmem())

// DP registers
#define dp_start                (*plugin_dp_register(DP_START))
#define dp_end                  (*plugin_dp_register(DP_END))
#define dp_current              (*plugin_dp_register(DP_CURRENT))
#define dp_status               (*plugin_dp_register(DP_STATUS))
#define dp_clock                (*plugin_dp_register(DP_CLOCK))
#define dp_bufbusy              (*plugin_dp_register(DP_BUFBUSY))
#define dp_pipebusy             (*plugin_dp_register(DP_PIPEBUSY)
#define dp_tmem                 (*plugin_dp_register(DP_TMEM))

// VI registers
#define vi_control              (*plugin_vi_register(VI_STATUS))
#define vi_origin               (*plugin_vi_register(VI_ORIGIN))
#define vi_width                (*plugin_vi_register(VI_WIDTH))
#define vi_v_intr               (*plugin_vi_register(VI_V_INTR))
#define vi_v_current_line       (*plugin_vi_register(VI_V_CURRENT_LINE))
#define vi_timing               (*plugin_vi_register(VI_TIMING))
#define vi_v_sync               (*plugin_vi_register(VI_V_SYNC))
#define vi_h_sync               (*plugin_vi_register(VI_H_SYNC))
#define vi_leap                 (*plugin_vi_register(VI_LEAP))
#define vi_h_start              (*plugin_vi_register(VI_H_START))
#define vi_v_start              (*plugin_vi_register(VI_V_START))
#define vi_v_burst              (*plugin_vi_register(VI_V_BURST))
#define vi_x_scale              (*plugin_vi_register(VI_X_SCALE))
#define vi_y_scale              (*plugin_vi_register(VI_Y_SCALE))

void plugin_init(void);
void plugin_interrupt(void);
uint32_t* plugin_dp_register(enum dp_register reg);
uint32_t* plugin_vi_register(enum vi_register reg);
uint8_t* plugin_rdram(void);
uint8_t* plugin_rdram_hidden(void);
size_t plugin_rdram_size(void);
uint8_t* plugin_dmem(void);
void plugin_close(void);
