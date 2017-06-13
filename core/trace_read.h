#pragma once

#include "trace.h"

#include <stdint.h>
#include <stdbool.h>

bool trace_read_open(const char* path);
bool trace_read_is_open(void);
void trace_read_header(size_t* rdram_size);
char trace_read_id(void);
void trace_read_cmd(uint32_t* cmd, size_t* length);
void trace_read_rdram(void);
void trace_read_vi(void);
void trace_read_close(void);
