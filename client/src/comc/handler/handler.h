#ifndef MEMORYLAB_HANDLER_H
#define MEMORYLAB_HANDLER_H

#include <stddef.h>
#include "memory.h"

int parse_response(const char *response, size_t *allocation_size,
                   const char **data, size_t *data_length);
int process_response(const char *response, TestMemory *memory);

#endif
