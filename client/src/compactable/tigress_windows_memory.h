#ifndef TIGRESS_WINDOWS_MEMORY_H
#define TIGRESS_WINDOWS_MEMORY_H

/* Minimal declarations used while TIGRESS parses memory.c. */
typedef void *HANDLE;
typedef unsigned long DWORD;
typedef int BOOL;
/* Windows x64 SIZE_T is always 64 bits. Keep this explicit so TIGRESS's
 * target model cannot infer a different width from its C library headers. */
typedef unsigned long long SIZE_T;

#ifndef NULL
#define NULL ((void *)0)
#endif

#define MEM_COMMIT     0x00001000UL
#define MEM_RESERVE    0x00002000UL
#define MEM_RELEASE    0x00008000UL
#define PAGE_READWRITE 0x04UL

extern void *VirtualAlloc(void *address, SIZE_T size, DWORD allocation_type,
                          DWORD protect);
extern BOOL VirtualFree(void *address, SIZE_T size, DWORD free_type);
extern BOOL WriteProcessMemory(HANDLE process, void *base_address,
                               const void *buffer, SIZE_T size,
                               SIZE_T *bytes_written);
extern HANDLE GetCurrentProcess(void);
extern DWORD GetLastError(void);

#endif
