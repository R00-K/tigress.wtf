/* Project-local wrapper for <winnt.h>, used ONLY while Tigress parses source
 * (via the ccwrap/ compiler wrapper that injects -I compat/).
 *
 * Root cause: winnt.h defines C_ASSERT without a #ifndef guard as
 *
 *   #define C_ASSERT(e) extern void __C_ASSERT__(int [(e)?1:-1])
 *
 * Tigress's CIL-based front end cannot evaluate sizeof() for Windows structs
 * at parse time, so every C_ASSERT(sizeof(X)==N) expands to an array of
 * negative length and CIL aborts with "Length of array is negative".
 *
 * Fix: pull in the complete real winnt.h via #include_next (so all types,
 * constants and intrinsic stubs are correct), then undefine and replace
 * C_ASSERT with a no-op for all subsequently included Windows headers.
 *
 * NOT used for the final Windows build: ccwrap/ is only placed first on PATH
 * around the `tigress` invocation; the real MinGW compile gets the genuine
 * winnt.h with a working C_ASSERT.
 */
#include_next <winnt.h>

#undef  C_ASSERT
#define C_ASSERT(e)  /* no-op for CIL */
