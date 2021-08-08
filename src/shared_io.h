#pragma once

#ifndef SHARED_IO_H_
#define SHARED_IO_H_

//-----------------------------------------------------------------------------

#include "shared_types.h"
#include <vector>

//-----------------------------------------------------------------------------

int file_seek(FILE *file, int64_t offset, int orgin);

int64_t file_tell(FILE *file);

int file_remove(const char *filename);

std::vector<uint8_t> file_tovector(const char *filename);

//-----------------------------------------------------------------------------

#endif
