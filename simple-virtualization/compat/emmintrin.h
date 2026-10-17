/* Project-local stub for GCC 13 mingw-w64's <emmintrin.h> (SSE2), used ONLY
 * while Tigress parses source (via the ccwrap/ compiler wrapper).
 * winnt.h pulls this in directly (not just via x86intrin.h) purely to back
 * YieldProcessor/MemoryBarrier with _mm_pause/_mm_mfence; nothing in this
 * project calls those macros.
 *
 * NOT used for the final Windows build (see ccwrap/ and compat/x86intrin.h).
 */
#ifndef _EMMINTRIN_H_INCLUDED
#define _EMMINTRIN_H_INCLUDED
#endif
