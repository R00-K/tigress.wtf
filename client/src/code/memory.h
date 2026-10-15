#ifndef MEMORYLAB_MEMORY_H
#define MEMORYLAB_MEMORY_H

#include <stddef.h>

typedef struct TestMemory {
    void *address;
    size_t size;
} TestMemory;

int allocate_test_memory(TestMemory *memory, size_t size);
int copy_test_data(TestMemory *memory, const char *data, size_t length);
void inspect_memory(const TestMemory *memory, size_t bytes_to_show);
void free_test_memory(TestMemory *memory);

#endif
