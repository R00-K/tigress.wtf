/*
 * compat/ws2tcpip.h - Tigress parse-pass stub
 *
 * Problem: ws2tcpip.h lines 125 and 132 use C_ASSERT (a negative-array-size
 * compile-time assertion) to check sizeof(IN_PKTINFO)==8 and
 * sizeof(IN6_PKTINFO)==20.  CIL computes those struct sizes differently from
 * GCC, so the array length evaluates to -1 and CIL aborts with:
 *   "Length of array is negative"
 *
 * Root cause: C_ASSERT expands to 'typedef char __C_ASSERT__[(e)?1:-1]'.
 * CIL does not match GCC's x86_64 Windows LLP64 struct-layout model exactly
 * for these two structs, so (e) evaluates to 0 inside CIL.
 *
 * A simple -DC_ASSERT(e)= command-line flag does NOT fix this because
 * winnt.h (included earlier via winsock2.h -> windows.h -> winnt.h)
 * redefines C_ASSERT with a #define, overriding any -D command-line value.
 *
 * Fix: intercept <ws2tcpip.h> here (compat/ is first on the -I path),
 * redefine C_ASSERT as a no-op AFTER winnt.h has already run its #define,
 * then pull in the real ws2tcpip.h via #include_next so all real type and
 * function declarations are still visible to CIL.
 *
 * The real MinGW build (step 3, no ccwrap, no -Icompat) uses the genuine
 * ws2tcpip.h with the real C_ASSERT intact, so struct-size correctness is
 * still validated at final compile time.
 */

/* Suppress C_ASSERT for the remainder of this translation unit parse pass.
 * winnt.h has already been included (and defined C_ASSERT) before we get
 * here, so we must undef first. */
#ifdef C_ASSERT
#undef C_ASSERT
#endif
#define C_ASSERT(e) /* disabled for Tigress CIL parser - see compat/README.md */

/* Include the real ws2tcpip.h.  #include_next skips the compat/ directory
 * and picks up the next match in the include path (the system header). */
#include_next <ws2tcpip.h>
