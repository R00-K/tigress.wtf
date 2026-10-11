/* Project-local stub for GCC 13 mingw-w64's <x86intrin.h>, used ONLY
 * while Tigress parses source (via the ccwrap/ compiler wrapper).
 *
 * Root cause: Tigress 4.2.0's C front end (CIL-based) cannot parse
 * GCC's vector-extension type system (__attribute__((vector_size(N))))
 * that essentially every x86 SIMD intrinsic header relies on - MMX,
 * SSE*, AVX*, AVX-512* (incl. the _Float16 "0.0f16" literals in the
 * FP16/BF16 headers that surface as the first symptom). windows.h's
 * winnt.h unconditionally pulls in the whole tree via:
 *
 *     # include <x86intrin.h>
 *
 * purely to get a handful of intrinsic-backed macros (CacheLineFlush,
 * MemoryBarrier, LoadFence/StoreFence, YieldProcessor, FastFence).
 * Nothing in this project calls those macros, so it is safe to make
 * this header a no-op for the Tigress parse pass only.
 *
 * NOT used for the final Windows build: ccwrap/ is only placed first
 * on PATH around the `tigress` invocation. The real MinGW compile of
 * the Tigress-generated output uses the unmodified system toolchain
 * and gets the genuine, fully-defined x86intrin.h / immintrin.h tree.
 *
 * Limitation: if your real source calls SSE/AVX/AVX-512 intrinsics
 * directly inside the function(s) being virtualized, Tigress 4.2.0
 * cannot parse that code at all (with or without this stub) - that is
 * an upstream CIL parser limitation, not something this workaround
 * can fix. See compat/README.md.
 */
#ifndef _X86INTRIN_H_INCLUDED
#define _X86INTRIN_H_INCLUDED
#endif
