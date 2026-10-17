# Tigress / MinGW-w64 GCC 13 compatibility workaround

## Root cause

Tigress 4.2.0's C front end is CIL-based (OCaml `cil`), and CIL does not
support GCC's vector-extension type system
(`__attribute__((vector_size(N)))`), which underlies essentially every
x86 SIMD intrinsic header GCC ships (MMX/SSE*/AVX*/AVX-512*). The first
symptom you hit was narrower — a hard parse error on the GNU `_Float16`
literal suffix (`0.0f16`) in `avx512fp16intrin.h`/`avx512bf16intrin.h` —
but stubbing only those leaf headers is **not sufficient**: once that
error is gone, CIL immediately fails on ordinary `vector_size`-attributed
code in `avx512fintrin.h`, `f16cintrin.h`, `mm3dnow.h`, etc. with errors
like `Expecting a pointer type in index` and
`doInit: unexpected NEXT_INIT for ... __attribute__((vector_size(...)))`.

This whole tree gets pulled in by `<windows.h>` even though your code
never touches SIMD: `winnt.h` unconditionally does

```c
#if defined(__x86_64) && !defined(RC_INVOKED)
...
# include <x86intrin.h>     // pulls in the entire AVX-512 tree
...
#include <emmintrin.h>      // SSE2, pulled in a second time, separately
...
#endif
```

purely to back a handful of macros (`CacheLineFlush`, `MemoryBarrier`,
`LoadFence`/`StoreFence`, `YieldProcessor`, `FastFence`) with
`_mm_clflush`/`_mm_mfence`/`_mm_lfence`/`_mm_sfence`/`_mm_pause`/
`__faststorefence`. Nothing in `windows_api_test.c` calls those macros.

Ruled out: defining `RC_INVOKED` to skip that block (the documented
"resource compiler" escape hatch) also strips real declarations used by
normal Windows API code (`HWND`, `DWORD`, `BYTE`, `GetLastError`,
`MessageBoxA`, `GENERIC_READ`, ...) everywhere else in `windows.h`'s
header tree — confirmed by a standalone compile test. It fails
requirement "retain correct Windows API integration" and was discarded.

Also ruled out: the `CC_OPTIONS` string that Tigress prints in its
`Settings:` banner is **not** a user-settable env var or CLI flag — it
is an internal, model-derived preprocessing flag string
(`host_os_and_cc_options` in the Tigress binary), always empty for this
target. Setting `CC_OPTIONS=-I...` in the environment has no effect
(confirmed: Tigress always prints `CC_OPTIONS=` empty regardless).
Tigress 4.2.0 has no documented flag for extra `-I`/CFLAGS into its own
parse pass.

## Workaround

Tigress resolves `--Compiler=x86_64-w64-mingw32-gcc` by name through
`$PATH` (like `execvp`), so a project-local wrapper placed first on
`PATH` controls exactly what Tigress's parse pass sees, without
touching the real compiler or any system header:

- `ccwrap/x86_64-w64-mingw32-gcc` — shell wrapper that `exec`s the real
  `/usr/bin/x86_64-w64-mingw32-gcc` with `-I<project>/compat` prepended.
- `compat/x86intrin.h`, `compat/emmintrin.h` — empty, guard-only stubs
  (same include-guard macro names as the real headers) that make the
  two entry points `winnt.h` uses a no-op for Tigress's parse pass only.

Because `-I` directories are searched before the compiler's built-in
system include dirs, these two stub files shadow the real ones **only**
for the `tigress` invocation (with `ccwrap/` prepended to `PATH`). The
final MinGW build of the Tigress-generated output does not use the
wrapper or `-Icompat`, so it gets the genuine, fully correct
`x86intrin.h`/`emmintrin.h`/`immintrin.h`/AVX-512 tree and genuine
`windows.h` declarations — confirmed by inspecting the produced PE's
import table (`KERNEL32.dll`: `GetLastError`, `InitializeCriticalSection`,
etc. - real Windows API symbols, not stubs).

## Commands

```bash
cd ~/Desktop/tigress.wtf/layered-virtualization

# 1. Run Tigress with the wrapper compiler shadowing the real one on PATH.
PATH="$PWD/ccwrap:$PATH" tigress \
  --Environment=x86_64:Windows:Gcc \
  --Compiler=x86_64-w64-mingw32-gcc \
  --Transform=Virtualize \
  --Functions=calculate \
  --out=windows_api_test_obf.c \
  windows_api_test.c

# 2. Compile the obfuscated output with the REAL, unmodified toolchain
#    (no wrapper, no -Icompat) to get a genuine Windows x64 PE.
x86_64-w64-mingw32-gcc -O0 -o windows_api_test_obf.exe windows_api_test_obf.c
```

## Verification done

- `tigress ... --Transform=Virtualize --Functions=calculate` exits 0 and
  produces a 9 MB `windows_api_test_obf.c` containing `_TIG_VZ_*`
  virtualized interpreter code for `calculate` (confirmed by grepping
  the output for `calculate`/`VKEY`).
- `x86_64-w64-mingw32-gcc -O0 -o windows_api_test_obf.exe windows_api_test_obf.c`
  exits 0 (only pre-existing, unrelated `#pragma pack`/pointer-cast
  warnings from mingw headers themselves - not from the Tigress output).
- `file windows_api_test_obf.exe` → `PE32+ executable (console) x86-64,
  for MS Windows` - a genuine Windows x64 PE, not a toy stand-in.
- `objdump -p` on the PE shows real imports from `KERNEL32.dll` and
  `msvcrt.dll` (`GetLastError`, `InitializeCriticalSection`, etc.),
  confirming Windows API linkage is intact and correct.
- **Not yet verified**: native execution (`calculate(10,20) == 30`) on
  Windows or under Wine - no Windows machine or Wine was available in
  this environment at the time of testing. Wine 9.0 is available via
  `apt-get install wine64` but was not installed (needs explicit
  approval - ask first).

## Limitation

If a function you virtualize (not just the surrounding file) calls
SSE/AVX/AVX-512 intrinsics **directly**, Tigress 4.2.0's CIL parser
cannot parse that code at all, with or without this workaround - that
is an upstream CIL limitation (no `vector_size` attribute support), not
something a header stub can paper over. This workaround only removes
*incidental* exposure to that intrinsic tree via `<windows.h>`'s own
internals; it does not add vector-intrinsic support to Tigress.

## Fallback if this stops working on a future GCC/mingw-w64 update

If a future mingw-w64 header release pulls in more `vector_size` code
through some other path, re-run the "Approach B" include-tree diagnostic
to find the new entry point, and extend `compat/` with one more
guard-only stub for that specific header:

```bash
x86_64-w64-mingw32-gcc -H -E windows_api_test.c -o /dev/null 2> include-tree.txt
```

Never stub a header that real WinAPI code (outside SIMD macros) depends
on for type definitions - check cross-references first, as done here
for `__m128h`/`__m256h`/`__m512h`/`__bf16`.

---

## Part 2: runtime access violation (0xC0000005) under the real Windows API test

After the header workaround above, Tigress generated source and MinGW
compiled a valid PE, but running it on real Windows PowerShell crashed
with exit code `-1073741819` (`0xC0000005`, access violation), no output.

### Reproduction (Wine)

Installed `wine64` 9.0 (`wine-9.0 (Ubuntu 9.0~repack-4build3)`) via
`sudo apt install -y wine64` at the user's explicit request.

- Baseline (`windows_api_test_baseline.exe`, built from the untouched
  `windows_api_test.c`): runs under Wine, prints `Result: 30`, exit 0.
- Obfuscated (`windows_api_test_obf.exe`, pre-fix): crashes under Wine
  with the *same* `0xC0000005` pattern as on real Windows -
  `Unhandled page fault on read access to 0x00000000fe2ffeb8`.
- Control test: a second, older virtualized file in this directory
  (`virtualization_test_obf.c`, generated earlier from a plain
  `#include <stdio.h>` source with **no** `windows.h` and **no**
  compat/ccwrap workaround involved) crashes identically. This rules
  out the Part 1 header workaround as the cause - the bug is in
  Tigress's `Virtualize` transform output itself for this target.

### Root cause (confirmed from the generated C source, not just disassembly)

`windedbg` backtrace put the fault inside `calculate` itself, first
call, instruction `mov (%rdx),%edx` dereferencing `rdx =
0x00000000fe2ffeb8` - the low 32 bits of a real, valid stack address
(`...fe2ffeb8`) with the high 32 bits zeroed. That is the signature of
a 64-bit pointer truncated to 32 bits and then zero-extended back.

The generated source (`windows_api_test_obf.c`, function `calculate`)
confirmed exactly where:

```c
unsigned long *formalGlobalInit10 ;
...
formalGlobalInit10 = (unsigned long *)(... _TIG_VZ_..._$locals ... + 8);
*formalGlobalInit10 = (unsigned long )(& b);   /* &b is a real 8-byte pointer */
```

Tigress's Virtualize transform round-trips the address of each
by-reference formal parameter through its interpreter's `$locals`
buffer using C type `unsigned long`. On Linux (LP64) `sizeof(unsigned
long) == 8`, so this is correct there. On Windows x64 (LLP64, which is
what MinGW-w64 implements), `sizeof(unsigned long) == 4` - so
`*formalGlobalInit10 = (unsigned long)(&b)` silently truncates the
64-bit address to its low 32 bits before storing it into the 8-byte
slot (the stale/zeroed upper 4 bytes of that slot stay zero). The
matching read-back site later does `*((unsigned long *)(..._$locals +
offset))` - also only 4 bytes on Windows - and the truncated,
zero-extended value is then dereferenced as a pointer, landing in
unmapped memory.

This reproduced identically across `--VirtualizeOperands=stack`,
`registers`, and `mixed` (tested all three), so it is not specific to
one operand-representation mode - it is the `x86_64:Windows:Gcc`
target model in Tigress 4.2.0 mapping its "pointer-sized integer" to
C `unsigned long` instead of a type that is actually 8 bytes on
Windows (`unsigned long long`, `uintptr_t`, or `void *`). This is an
upstream Tigress code-generation bug for Windows x64 (LLP64) targets,
not something caused by the compat/ccwrap header workaround, and not
fixable by changing Tigress's command-line options - it is baked into
the C it emits.

### Fix applied (project-local, to the generated file only)

Patched `windows_api_test_obf.c` (backup kept at
`windows_api_test_obf.c.orig`) in the 4 lines this bug touches for
`calculate`, changing `unsigned long` to `unsigned long long` (8 bytes
on both Windows LLP64 and Linux LP64) for the pointer round-trip only:

- the `formalGlobalInit10` declaration and its two cast sites (storing
  `&a` / `&b`),
- the matching read-back cast in the `formal` opcode case.

Nothing else in the 9000+-line file was touched; the two unrelated
pre-existing `unsigned long` casts inside `winnt.h`'s inlined
`NtCurrentTeb`/`GetCurrentFiber` (lines ~142741/143951) were left
alone - they are real Windows header code, not Tigress's VM state, and
are not part of this bug.

### Rebuild and verification

```bash
cd ~/Desktop/tigress.wtf/layered-virtualization
x86_64-w64-mingw32-gcc -O0 -o windows_api_test_obf.exe windows_api_test_obf.c
WINEDEBUG=-all wine windows_api_test_obf.exe
```

Result: `gcc` exit 0, no errors. `file` confirms `PE32+ ... for MS
Windows`. `objdump -p` still shows real `KERNEL32.dll` imports
(`GetLastError`, `InitializeCriticalSection`, ...). Under Wine:

```
Result: 30
exit=0
```

`calculate(10, 20) == 30`, confirmed for the actual virtualized
implementation (the `_TIG_VZ_rYXd_1_calculate_*` interpreter state
machine), not a stand-in.

### Remaining limitation

This fix is applied to the generated `.c` file, not to Tigress itself,
so it must be reapplied (or scripted) after every fresh
`--Transform=Virtualize` run against an `x86_64:Windows:*` target as
long as this is an open bug in Tigress 4.2.0. It generalizes to any
function with by-reference-passed formal parameters under Virtualize
on Windows; functions with no such parameters (no address-of a local
is round-tripped through `$locals`) would not hit this particular
pattern. Not verified on real Windows hardware (only Wine 9.0) - Wine
reproduced the exact same exception code and address pattern reported
from real Windows PowerShell, which is strong (not absolute) evidence
the fix holds there too.
