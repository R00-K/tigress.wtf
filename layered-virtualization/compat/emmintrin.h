/* Project-local stub for GCC 13 mingw-w64's <emmintrin.h> (SSE2),
 * used ONLY while Tigress parses source (via the ccwrap/ wrapper).
 * See compat/x86intrin.h and compat/README.md for the full rationale
 * and limitations. windows.h's winnt.h pulls this in directly (not
 * just via x86intrin.h) purely to back the YieldProcessor/
 * MemoryBarrier macros with _mm_pause/_mm_mfence; nothing in this
 * project calls those macros.
 */
#ifndef _EMMINTRIN_H_INCLUDED
#define _EMMINTRIN_H_INCLUDED
#endif
