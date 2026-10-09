#ifndef MEMORY_WIN_COMPAT_H
#define MEMORY_WIN_COMPAT_H

/*
 * Minimal LOCAL replacement for <windows.h>, containing ONLY the
 * declarations actually used by memory.c.
 *
 * This header intentionally does NOT include <windows.h> or any other
 * MinGW/Windows system header, so TIGRESS never sees the GCC intrinsic
 * headers (e.g. avx512fp16intrin.h) that those files pull in.
 */

/* ---- Types ---- */

#if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__) || defined(_M_X64)
typedef unsigned long long SIZE_T;   /* Windows: ULONG_PTR on 64-bit */
#else
typedef unsigned long SIZE_T;        /* Windows: ULONG_PTR on 32-bit */
#endif

typedef unsigned long DWORD;         /* Windows: 32-bit unsigned */
typedef int BOOL;                    /* Windows: 32-bit signed */
typedef void *HANDLE;                /* Windows: opaque handle */

#ifndef WINAPI
#define WINAPI __stdcall             /* Windows default calling convention */
#endif

/* ---- Constants ---- */

#define MEM_COMMIT   0x00001000UL
#define MEM_RESERVE  0x00002000UL
#define MEM_RELEASE  0x00008000UL
#define PAGE_READWRITE 0x00000004UL

/* ---- Functions ---- */

void *WINAPI VirtualAlloc(void *lp_address, SIZE_T dw_size,
                          DWORD fl_allocation_type, DWORD fl_protect);

BOOL WINAPI VirtualFree(void *lp_address, SIZE_T dw_size, DWORD dw_free_type);

DWORD WINAPI GetLastError(void);

BOOL WINAPI WriteProcessMemory(HANDLE h_process, void *lp_base_address,
                               const void *lp_buffer, SIZE_T n_size,
                               SIZE_T *lp_number_of_bytes_written);

HANDLE WINAPI GetCurrentProcess(void);

#endif /* MEMORY_WIN_COMPAT_H */
