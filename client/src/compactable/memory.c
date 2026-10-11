#include "../memory.h"
#include "tigress_windows_memory.h"
#include <stdio.h>
#include <string.h>

int allocate_test_memory(TestMemory *memory, size_t size)
{
    if (memory == NULL || size == 0) {
        return 0;
    }
    memory->address = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (memory->address == NULL) {
        fprintf(stderr, "[-] VirtualAlloc failed: %lu\n", (unsigned long)GetLastError());
        memory->size = 0;
        return 0;
    }
    memory->size = size;
    printf("    address: %p\n    size: %zu bytes\n", memory->address, memory->size);
    return 1;
}

int copy_test_data(TestMemory *memory, const char *data, size_t length)
{
    SIZE_T bytes_written = 0;
    if (memory == NULL || memory->address == NULL || data == NULL || length > memory->size) {
        fprintf(stderr, "[-] Invalid memory copy request\n");
        return 0;
    }
    if (!WriteProcessMemory(GetCurrentProcess(), memory->address, data, length, &bytes_written) ||
        bytes_written != length) {
        fprintf(stderr, "[-] WriteProcessMemory failed: %lu (wrote %llu of %llu bytes)\n",
                (unsigned long)GetLastError(),
                (unsigned long long)bytes_written,
                (unsigned long long)length);
        return 0;
    }
    return 1;
}

void inspect_memory(const TestMemory *memory, size_t bytes_to_show)
{
    size_t i;
    const unsigned char *bytes;
    if (memory == NULL || memory->address == NULL) {
        return;
    }
    if (bytes_to_show > memory->size) {
        bytes_to_show = memory->size;
    }
    bytes = (const unsigned char *)memory->address;
    printf("    memory bytes:");
    for (i = 0; i < bytes_to_show; ++i) {
        printf(" %02X", bytes[i]);
    }
    printf("\n");
}

void free_test_memory(TestMemory *memory)
{
    if (memory != NULL && memory->address != NULL) {
        if (!VirtualFree(memory->address, 0, MEM_RELEASE)) {
            fprintf(stderr, "[-] VirtualFree failed: %lu\n", (unsigned long)GetLastError());
        }
        memory->address = NULL;
        memory->size = 0;
    }
}

int main(void) { return 0; }
