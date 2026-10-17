# Tigress + MinGW-w64 Virtualization: Root Causes and Fixes

Virtualizing `calculate(int a, int b)` with Tigress 4.2.0 for a Windows
x64 target failed in two completely different ways, one after the
other. The first was a **build-time failure**: Tigress's own C parser
could not read GCC 13's MinGW-w64 system headers, because
`<windows.h>` transitively pulls in AVX-512 FP16/BF16 SIMD intrinsic
headers that use syntax (`_Float16` literals, `vector_size`
attributes) Tigress's parser does not support at all. The second,
found only after the first was fixed and the executable was actually
run, was a **runtime crash**: the compiled executable raised a
Windows access violation (`0xC0000005`) the moment `calculate` was
first called, from a Tigress code-generation bug where a 64-bit
pointer gets silently truncated to 32 bits.

Both were fixed with small, targeted, project-local changes that never
touch Tigress itself, never touch system compiler headers, and never
touch the original `windows_api_test.c`. The final result: a genuine
Windows x64 PE executable, with `calculate` virtualized by Tigress,
correctly returning `calculate(10, 20) == 30`, verified end-to-end
under Wine 9.0.

## 1. Environment and Goal

| | |
| --- | --- |
| Host OS | Ubuntu 24.04 Linux |
| Tigress | 4.2.0, at `/usr/local/bin/tigress` |
| Cross-compiler | `x86_64-w64-mingw32-gcc`, GCC 13.2 (also tested: `-posix` variant) |
| Target | genuine Windows x64 PE executable |
| Project directory | `~/Desktop/tigress.wtf/test` |
| Source file | `windows_api_test.c` — includes `<windows.h>` and `<stdio.h>`, defines `calculate(int a, int b) { return a + b; }` called from `main()` |

The command being run throughout:

```bash
tigress \
  --Environment=x86_64:Windows:Gcc \
  --Compiler=x86_64-w64-mingw32-gcc \
  --Transform=Virtualize \
  --Functions=calculate \
  --out=windows_api_test_obf.c \
  windows_api_test.c
```

The goal: get this command to succeed, compile its output into a real
Windows x64 executable with the real MinGW-w64 toolchain, and confirm
the virtualized `calculate(10, 20)` still returns `30` — without
patching system compiler headers, without uninstalling or downgrading
any toolchain, and without replacing the real application with a toy
stand-in.

## 2. Problem 1 — Tigress's Parser Crashes on GCC 13 Headers

Running the command above failed immediately, before any
transformation happened:

```
/usr/lib/gcc/x86_64-w64-mingw32/13-win32/include/avx512fp16intrin.h[186:23-27] : syntax error
Parsing error
```

The offending line, inside GCC's own header:

```c
return _mm_set1_ph (0.0f16);
```

`0.0f16` is a GNU C `_Float16`-suffixed floating literal — valid GCC
13 syntax, but not something Tigress's parser recognizes. The puzzle:
`windows_api_test.c` never uses AVX-512, FP16, or any SIMD intrinsic.
Tracing the actual include chain
(`x86_64-w64-mingw32-gcc -H -E windows_api_test.c -o /dev/null 2> include-tree.txt`)
showed why it shows up anyway:

```
windows.h → windef.h → winnt.h → psdk_inc/intrin-impl.h
                              → x86intrin.h → immintrin.h → avx512fp16intrin.h
                              → emmintrin.h   (included a second time, directly)
```

`winnt.h` unconditionally pulls in the entire x86 SIMD intrinsic tree
— MMX, SSE, AVX, AVX-512, everything — purely to back a handful of
macros it defines (`CacheLineFlush`, `MemoryBarrier`,
`LoadFence`/`StoreFence`, `YieldProcessor`, `FastFence`) with the
corresponding `_mm_*` intrinsic functions. `windows_api_test.c` never
calls any of those macros, but the `#include` happens regardless of
whether anything downstream actually uses it.

### Root cause

The `0.0f16` literal was only the first symptom, not the real
problem. Tigress's C front end is CIL-based (the OCaml `cil`
library), and **CIL does not support GCC's vector-extension type
system** — `__attribute__((vector_size(N)))` — which underlies
essentially every x86 SIMD intrinsic header GCC ships, not just the
FP16/BF16 ones.

This was proven directly, not assumed: after neutralizing the
`_Float16` headers, Tigress immediately hit a *different* class of
error one header deeper:

```
<<Tigress.CIL-ERR-ERROR>> avx512fintrin.h:16415: Expecting a pointer type in index:
 t1=TNamed(__m128d, TFloat(double, __attribute__((__vector_size__(16), __may_alias__))), )
 t2=TInt(int, )

<<Tigress.CIL-ERR-NOT-IMPLEMENTED>> f16cintrin.h:40: doInit: unexpected NEXT_INIT for short __attribute__((__vector_size__(16)))
```

`avx512fintrin.h` (plain AVX-512, no FP16 involved) and
`f16cintrin.h` both failed on ordinary `vector_size`-attributed code.
So the real defect is structural: Tigress 4.2.0 cannot parse the GCC
vector-intrinsic header family at all, on this GCC version. Removing
only the FP16/BF16 headers is necessary but not sufficient — the
whole SIMD tree had to be kept out of Tigress's parse pass, not just
the two headers that happened to produce the first error message.

### Dead ends ruled out

Two superficially reasonable fixes were tried and actually disproven,
not just dismissed:

- **`RC_INVOKED`.** `winnt.h`'s SIMD-pulling block is wrapped in
  `#if defined(__x86_64) && !defined(RC_INVOKED)` — `RC_INVOKED` is
  the documented escape hatch Microsoft's Resource Compiler uses to
  skip C-specific code when it only needs to process `.rc` resource
  scripts. Defining it was tested directly:

  ```c
  #define RC_INVOKED 1
  #include <windows.h>
  ```

  This does skip the SIMD block — but it also strips real,
  load-bearing declarations everywhere else in the `windows.h` tree.
  Compiling a file that used `HWND`, `DWORD`, `GetLastError`,
  `MessageBoxA`, and `GENERIC_READ` with `RC_INVOKED` defined failed
  with `unknown type name 'HWND'`, `implicit declaration of function
  'GetLastError'`, and similar errors throughout. This violates the
  hard requirement to retain correct Windows API integration, so it
  was discarded.

- **The `CC_OPTIONS` environment variable.** Tigress prints a
  `CC_OPTIONS=` line in its startup banner, and an earlier attempt
  tried `CC_OPTIONS="-I$PWD/compat"` to inject an include path. It had
  no effect — and inspecting the strings in the `tigress` binary
  itself explained why: `CC_OPTIONS` is not a user-facing hook at all.
  It is an internal, model-derived preprocessing-flag string computed
  per target (`host_os_and_cc_options` in the binary), and for this
  target it is always empty, regardless of what the environment
  variable is set to. Tigress 4.2.0 has no documented or discovered
  flag for injecting extra include paths or preprocessor arguments
  into its own parse pass.

### The fix

Tigress resolves `--Compiler=x86_64-w64-mingw32-gcc` by name through
`$PATH`, the same way `execvp` would. That meant a project-local
wrapper placed first on `PATH` could control exactly what Tigress's
parse pass sees, without touching the real compiler, Tigress itself,
or any system header:

**`ccwrap/x86_64-w64-mingw32-gcc`** — a shell wrapper that `exec`s the
real `/usr/bin/x86_64-w64-mingw32-gcc`, prepending `-I<project>/compat`:

```bash
#!/bin/bash
REAL=/usr/bin/x86_64-w64-mingw32-gcc
exec "$REAL" -I"/home/.../test/compat" "$@"
```

**`compat/x86intrin.h`** and **`compat/emmintrin.h`** — the two entry
points `winnt.h` actually uses to reach the SIMD tree, replaced with
empty, guard-only stubs that keep the same include-guard macro names
as the real headers:

```c
#ifndef _X86INTRIN_H_INCLUDED
#define _X86INTRIN_H_INCLUDED
#endif
```

Because `-I` directories are searched before a compiler's built-in
system include directories, these two files shadow the real ones
**only** when `ccwrap/` is first on `PATH` — i.e., only during the
`tigress` invocation itself:

```bash
PATH="$PWD/ccwrap:$PATH" tigress \
  --Environment=x86_64:Windows:Gcc \
  --Compiler=x86_64-w64-mingw32-gcc \
  --Transform=Virtualize --Functions=calculate \
  --out=windows_api_test_obf.c windows_api_test.c
```

The final MinGW build of the generated output does **not** use the
wrapper or `-Icompat`, so it gets the genuine, fully-defined
`x86intrin.h`/`emmintrin.h`/`immintrin.h`/AVX-512 tree and genuine
`windows.h` declarations. Safe to stub precisely because `winnt.h`
only needs those two headers for macros (`CacheLineFlush`,
`MemoryBarrier`, etc.) that nothing in this project calls.

### Verification

| Check | Result |
| --- | --- |
| `tigress --Transform=Virtualize --Functions=calculate` | exit 0, no parse errors |
| Generated `windows_api_test_obf.c` | 9,056,905 bytes, contains `_TIG_VZ_*_calculate_*` virtualization symbols |
| `x86_64-w64-mingw32-gcc -O0 -o windows_api_test_obf.exe windows_api_test_obf.c` (real toolchain, no wrapper) | exit 0, only pre-existing, unrelated mingw-header warnings |
| `file windows_api_test_obf.exe` | `PE32+ executable (console) x86-64, for MS Windows, 19 sections` |
| `objdump -p` import table | real `KERNEL32.dll` symbols (`GetLastError`, `InitializeCriticalSection`, …) — confirms Windows API linkage is genuine, not stubbed |

At this point the build problem was solved: Tigress virtualized
`calculate` and produced a real Windows x64 PE. But running that PE
was the next test — and it failed.

## 3. Problem 2 — Runtime Access Violation (0xC0000005)

Running `windows_api_test_obf.exe` on real Windows PowerShell:

```powershell
.\windows_api_test_obf.exe
$LASTEXITCODE
```

produced no output at all, and:

```
-1073741819
```

That is `0xC0000005` reinterpreted as a signed 32-bit integer — a
Windows **access violation**. To investigate locally, Wine 9.0
(`wine64`) was installed (`sudo apt install -y wine64`, at explicit
request) and the same executable run under it:

```
wine: Unhandled page fault on read access to 00000000FE2FFEB8 at address 00000001400019E5
```

Same exception class, and — critically for ruling out causes later —
the baseline (non-virtualized) build of the exact same source ran
perfectly under Wine, printing `Result: 30` and exiting 0. The bug was
specific to the virtualized build.

### Diagnosis

`winedbg` was used to catch the crash live and pull a register dump
and backtrace:

```bash
printf 'cont\nbt\ninfo reg\ndisas\nquit\n' | WINEDEBUG=-all winedbg ./windows_api_test_obf.exe
```

Key evidence:

```
Unhandled exception: page fault on read access to 0x00000000fe2ffeb8
rip:00000001400019e5 ... rdx:00000000fe2ffeb8 ...
Backtrace:
=>0 0x1400019e5 in windows_api_test_obf (+0x19e5)
  1 0x140001739 in windows_api_test_obf (+0x1739)   [inside main]
  2 ... BaseThreadInitThunk ... [normal CRT startup chain]
0x1400019e5: mov (%rdx), %edx
```

The fault is on the **very first call** to `calculate` from `main`,
inside `calculate` itself, dereferencing `rdx = 0x00000000fe2ffeb8`.
That value is the tell: it is the *low 32 bits* of a perfectly
ordinary stack address (compare to `rsp = 0x00007ffffe2ffc70` in the
same dump — the pattern `...fe2ffeb8` matches), with the *high 32
bits zeroed*. That is the exact signature of a 64-bit pointer that
got truncated to 32 bits somewhere and then zero-extended back to 64
on read.

**Ruling out the Part 1 workaround as the cause** (rather than
assuming it was fine): an older virtualized file in the same project
— `virtualization_test_obf.c`, generated earlier from a plain
`#include <stdio.h>` source with *no* `windows.h` and *no*
`compat/ccwrap` involvement at all — was rebuilt and run under Wine.
It crashed with the **identical** fault pattern: same instruction
(`mov (%rdx), %edx`), same truncated-pointer shape. This proves the
bug lives in Tigress's `Virtualize` transform output itself,
independent of the header workaround.

### Root cause

Reading the actual generated C source for `calculate` (not just the
disassembly) found the defect in plain text:

```c
int calculate(int a, int b) {
  char _TIG_VZ_rYXd_1_calculate_$locals[532];
  unsigned char *_TIG_VZ_rYXd_1_calculate_$pc[1];
  union _TIG_VZ_rYXd_1_calculate_$node *_TIG_VZ_rYXd_1_calculate_$sp[1];
  unsigned long *formalGlobalInit10;        /* <- the bug */

  formalGlobalInit10 = (unsigned long *)(... $locals ... + 8);
  *formalGlobalInit10 = (unsigned long)(& b);   /* &b is a real 64-bit address */
  formalGlobalInit10 = (unsigned long *)(... $locals ... + 0);
  *formalGlobalInit10 = (unsigned long)(& a);
  ...
}
```

Tigress's `Virtualize` transform represents `calculate`'s body as a
tiny bytecode interpreter. To let that interpreter read/write the
*formal parameters* `a` and `b` by reference, it first stores each
parameter's **address** into its own private `$locals` buffer, to be
read back later whenever the bytecode needs to load or store through
it.

The type it chose for that round-trip is C's `unsigned long`. On
Linux, `sizeof(unsigned long) == 8` (the LP64 data model) — a pointer
fits exactly, no problem. **On Windows x64, which uses the LLP64 data
model, `sizeof(unsigned long) == 4`** — only half a pointer. So
`*formalGlobalInit10 = (unsigned long)(&b)` silently truncates the
64-bit address down to its low 32 bits before writing it into the
8-byte slot; the slot's upper 4 bytes stay whatever they were (zero,
from stack initialization). When the bytecode later reads that slot
back as a pointer, it gets a corrupted address — the real low bits,
zero-extended on top — and dereferencing it faults.

This is a genuine Tigress 4.2.0 code-generation bug for Windows x64
(LLP64) targets. It reproduced identically across all three
`--VirtualizeOperands` modes tested (`stack`, `registers`, `mixed`),
so it is not specific to one operand-representation strategy — it is
the `x86_64:Windows:Gcc` target model mapping "pointer-sized integer"
to the wrong C type.

### Audit before patching

Before writing any patch, the whole 9,057-line generated file was
audited to make sure this was the *only* instance of the pattern, and
that fixing it couldn't corrupt something else by overlapping with
adjacent memory:

```bash
grep -oE "[A-Za-z0-9_]*GlobalInit[0-9]+" windows_api_test_obf.c | sort -u
# -> formalGlobalInit10  (exactly one, whole file)
```

| Item | Value |
| --- | --- |
| Round-trip temp variables in the whole file | 1 (`formalGlobalInit10`) |
| Store sites (offset, parameter) | offset 0 → `a`; offset 8 → `b` |
| Read sites | 1 (the `formal$` bytecode opcode handler) |
| Slot width needed | 8 bytes (a pointer) |
| `$locals` buffer size | 532 bytes (`char _TIG_VZ_..._$locals[532]`) |
| Where the operand-stack/node array region starts | byte 16 |

The two parameter slots (0–7 for `a`, 8–15 for `b`) exactly fill bytes
0–15; the operand-stack array begins at byte 16 — contiguous, zero
overlap, zero gap. No other pointer-round-trip pattern exists anywhere
else in the file. This confirmed the fix could be scoped to exactly
four lines with no risk of touching unrelated state.

### The fix

The fix changes `unsigned long` to **`unsigned long long`** — 8 bytes
on *both* Windows (LLP64) and Linux (LP64) — at exactly the four spots
this bug touches, and nowhere else:

```c
/* before */                              /* after */
unsigned long *formalGlobalInit10;    →  unsigned long long *formalGlobalInit10;

formalGlobalInit10 =                      formalGlobalInit10 =
  (unsigned long *)(...+ 8);         →      (unsigned long long *)(...+ 8);
*formalGlobalInit10 =                     *formalGlobalInit10 =
  (unsigned long)(& b);              →      (unsigned long long)(& b);

/* same pattern repeated for offset 0 / &a, and for the read-back site */
```

This is applied to the **generated** `.c` file, never to Tigress and
never to any system header. Two unrelated, pre-existing
`unsigned long` casts inside `winnt.h`'s inlined
`NtCurrentTeb`/`GetCurrentFiber` functions were identified during the
audit and deliberately left untouched — they are genuine Windows
header code with a different, legitimate purpose, not part of
Tigress's VM state.

### Verification

```bash
x86_64-w64-mingw32-gcc -O0 -o windows_api_test_obf.exe windows_api_test_obf.c
WINEDEBUG=-all wine windows_api_test_obf.exe
```

```
Result: 30
```

`gcc` exited 0 with zero errors; `file` still reported a genuine
`PE32+ ... for MS Windows`; `objdump -p` still showed the real
`KERNEL32.dll` imports. Under Wine, the program ran to completion and
printed `Result: 30`, exit code 0 — `calculate(10, 20) == 30`,
produced by the actual `_TIG_VZ_rYXd_1_calculate_*` virtualized
interpreter, not a bypassed or stand-in implementation.

## 4. The Repeatable Build-and-Test Pipeline

Patching one generated file by hand once is not a process.
`build_and_test.sh` + `patch_pointer_width.py` turn the whole
investigation into one repeatable command, run as `./build_and_test.sh`,
that fails loudly at the first broken step:

| Step | What it does | Fails if |
| --- | --- | --- |
| 1. Run Tigress | `PATH="ccwrap:$PATH" tigress ... --Functions=calculate` into `build/windows_api_test_obf.generated.c` | nonzero exit, or no `_TIG_VZ_*_calculate_*` symbols in the output |
| 2. Validate + patch | `patch_pointer_width.py` checks the generated file has *exactly* the known defect shape, then rewrites only those spots | the shape differs from what was audited (see below) |
| 3. Compile | real, unwrapped `x86_64-w64-mingw32-gcc` builds the patched source | compiler errors, or no executable produced |
| 4. Sanity-check the PE | confirms `PE32+ ... x86-64 ... MS Windows` and a real `KERNEL32.dll` import | wrong architecture or missing import |
| 5. Run under Wine | asserts the output is exactly `Result: 30` and exit code is exactly 0 | any other output or exit code |

The important design choice is in step 2: **`patch_pointer_width.py`
never does a blind `unsigned long` → `unsigned long long` replace.**
It discovers the function's randomized Tigress identifier tag (e.g.
`rYXd`), then requires an exact match on: exactly one
`formalGlobalInit*` declaration, a store-cast/store-value pair at a
*distinct, 8-byte-aligned* offset for each formal parameter, exactly
one matching read-back site, and confirms the parameter slots don't
overlap the operand-stack region. If the generated code is already
fixed (declared `unsigned long long`), it's a no-op. If the shape
differs from any of this in any way — a future Tigress version, a
different function signature, anything unexpected — it refuses with a
specific, named reason and a nonzero exit, rather than guessing. This
was verified directly: feeding it a deliberately corrupted copy of the
generated source (`unsigned int` instead of `unsigned long`) produced
`PATCH-FAIL: no 'unsigned long [long] *formalGlobalInitN ;' declaration
found...` and exit code 1.

### Two bugs found while building the script itself

Writing the script surfaced two unrelated shell bugs of its own —
both caught by actually running it, not assumed away:

1. **`pipefail` + `grep -q` SIGPIPE race.**
   `objdump -p "$exe" | grep -q "KERNEL32.dll"` looked correct and
   matched when run by hand, but failed inside the script under
   `set -euo pipefail`. `grep -q` exits the instant it finds a match,
   closing the pipe early; if the producer (`objdump`, or even a plain
   `echo` of an already-captured variable) is still writing when that
   happens, it receives `SIGPIPE` and exits with a nonzero status —
   which `pipefail` then reports as the whole pipeline failing, even
   though the match genuinely succeeded. Fix: capture the output into
   a variable first and test it with a pure-bash substring check,
   `[[ "$OBJDUMP_OUT" == *"KERNEL32.dll"* ]]`, with no pipe and no
   second process to race against.

2. **CRLF breaking an exact string comparison.** The Wine-run output
   was captured as `WINE_OUT="$(wine "$exe")"` and compared with
   `[ "$WINE_OUT" = "Result: 30" ]` — which failed even though the
   printed text was visibly identical. The Windows CRT writes `\r\n`
   line endings; bash's `$(...)` strips the trailing `\n` but leaves
   the `\r`, so the captured string was actually `"Result: 30\r"`.
   Fix: `WINE_OUT="${WINE_OUT%$'\r'}"` before comparing.

## 5. Final Results

| File | Status | Purpose |
| --- | --- | --- |
| `windows_api_test.c` | **untouched** (verified byte-identical to its backup throughout) | original source |
| `ccwrap/x86_64-w64-mingw32-gcc` | new | PATH-shadowing wrapper injecting `-Icompat` for Tigress's parse pass only |
| `compat/x86intrin.h`, `compat/emmintrin.h` | new | guard-only stubs that keep Tigress from parsing the unsupported SIMD tree |
| `compat/README.md` | new | detailed root-cause appendix for both problems (evidence log) |
| `windows_api_test_obf.c` / `.c.orig` | patched / backup kept | first validated virtualized build + its pre-patch original |
| `windows_api_test_obf.exe` | rebuilt | first validated working executable |
| `patch_pointer_width.py` | new | validates the exact defect shape, patches narrowly, refuses with a clear error otherwise |
| `build_and_test.sh` | new | the repeatable 5-step pipeline described above |
| `build/` | new (generated artifacts) | fresh Tigress output, patched output, test executable, logs — isolated from the files above |

**Final pipeline run, in full:**

```
=== 1/5 Run Tigress ===            OK: 144453 lines, 'calculate' virtualized.
=== 2/5 Validate + patch ===       PATCH-OK: 'formalGlobalInit10' (offsets [0, 8]) rewritten.
=== 3/5 Compile ===                OK: compiled with no errors.
=== 4/5 Sanity-check the PE ===    OK: genuine PE32+ x86-64 Windows executable, real KERNEL32.dll imports.
=== 5/5 Run under Wine ===         Result: 30

PASS: calculate(10, 20) = 30
```

Exit status: **0**, end to end.

## 6. Limitations and Caveats

- **Not tested on real Windows hardware in this session** — only Wine
  9.0. Wine reproduced the exact same `0xC0000005` exception and
  fault-address pattern reported from real Windows PowerShell before
  the fix, which is strong corroborating evidence, but it is not the
  same as a native-Windows confirmation of the fix.
- **The Problem 2 fix lives in the generated output, not in Tigress.**
  Because Tigress 4.2.0's `Virtualize` transform itself emits the
  buggy `unsigned long`, this patch must be reapplied — which
  `build_and_test.sh` now does automatically — after every fresh
  `--Transform=Virtualize` run against an `x86_64:Windows:*` target,
  until this is fixed upstream.
- **Scope is limited to `calculate()`.** The pointer-width defect
  generalizes to any function with by-reference-passed formal
  parameters under Virtualize on Windows; functions with no such
  parameters wouldn't hit this exact pattern, and haven't been
  separately audited here.
- **The SIMD-header workaround (Problem 1) is a parser-time-only
  accommodation.** If a function actually being virtualized calls
  SSE/AVX/AVX-512 intrinsics directly, Tigress 4.2.0's CIL-based
  parser cannot parse that code at all, with or without `compat/`'s
  stubs — that is an upstream CIL limitation (no `vector_size`
  attribute support), not something a header stub can fix.
