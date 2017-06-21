#pragma once

#include <stdbool.h>
#include <stdint.h>

bool file_exists(const char* path);
bool file_path_indexed(char* path, size_t path_size, const char* dir, const char* name, const char* ext, uint32_t* index);
