/* Project-local wrapper for <winnt.h>, used ONLY while Tigress parses
 * source (via the ccwrap/ compiler wrapper that injects -I compat/).
 *
 * Root cause: winnt.h defines C_ASSERT without a #ifndef guard (lines
 * 144-148) as:
 *
 *   #define C_ASSERT(e) extern void __C_ASSERT__(int [(e)?1:-1])
 *
 * Tigress's C front end (CIL-based) cannot evaluate sizeof() for Windows
 * structs at parse time, so every C_ASSERT(sizeof(X)==N) call expands to
 * an array of negative length and CIL aborts with:
 *
 *   "Length of array is negative"
 *
 * Fix: pull in the complete real winnt.h via #include_next (so all types,
 * constants, and intrinsic stubs are defined correctly), then immediately
 * undefine and replace C_ASSERT with a no-op.  Every subsequent use of
 * C_ASSERT in ws2tcpip.h (and any other Windows header included after
 * winnt.h) expands to nothing, which CIL happily accepts.
 *
 * NOT used for the final Windows build: ccwrap/ is only placed first on
 * PATH around the `tigress` invocation.  The real MinGW compile of the
 * Tigress-generated output uses the unmodified system toolchain and gets
 * the genuine winnt.h with a working C_ASSERT.
 */
#include_next <winnt.h>

/* Neutralise C_ASSERT for all headers included after winnt.h. */
#undef  C_ASSERT
#define C_ASSERT(e)  /* no-op for CIL */
