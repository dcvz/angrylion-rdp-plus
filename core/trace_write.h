#pragma once

#include "trace.h"

#include <stdint.h>
#include <stdbool.h>

bool trace_write_open(const char* path);
bool trace_write_is_open(void);
void trace_write_header(size_t rdram_size);
void trace_write_cmd(uint32_t* cmd, size_t length);
void trace_write_rdram(size_t offset, size_t length);
void trace_write_vi(void);
void trace_write_reset(void);
void trace_write_close(void);
