/* Project-local stub for GCC 13 mingw-w64's <x86intrin.h>, used ONLY while
 * Tigress parses source (via the ccwrap/ compiler wrapper that injects
 * -I compat/).
 *
 * Root cause: Tigress 4.2.0's C front end (CIL-based) cannot parse GCC's
 * vector-extension type system (__attribute__((vector_size(N)))) that
 * essentially every x86 SIMD intrinsic header relies on. windows.h's winnt.h
 * unconditionally pulls in the whole tree via `# include <x86intrin.h>`
 * purely to back a handful of macros (CacheLineFlush, MemoryBarrier,
 * LoadFence/StoreFence, YieldProcessor, FastFence). Nothing in this project
 * calls those macros, so this header is made a no-op for the Tigress parse
 * pass only.
 *
 * NOT used for the final Windows build: ccwrap/ is only placed first on PATH
 * around the `tigress` invocation, so the real MinGW compile gets the genuine,
 * fully-defined x86intrin.h / immintrin.h / AVX-512 tree.
 */
#ifndef _X86INTRIN_H_INCLUDED
#define _X86INTRIN_H_INCLUDED
#endif
