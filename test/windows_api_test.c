
#include <windows.h>
#include <stdio.h>

int calculate(int a, int b) {
    SIZE_T size = 4096;

    void *memory = VirtualAlloc(
        NULL,
        size,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_READWRITE
    );

    if (memory == NULL) {
        printf("VirtualAlloc failed: %lu\n", GetLastError());
        return -1;
    }

    printf("Allocated address: %p\n", memory);

    int result = a + b;

    VirtualFree(memory, 0, MEM_RELEASE);

    return result;
}

void main(void) {

}

