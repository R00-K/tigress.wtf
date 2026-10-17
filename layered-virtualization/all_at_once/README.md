# Single-pass Virtualize pipeline (all functions at once)

This folder is an alternative to the parent project's `build_and_test.sh`.
Run it with:

```bash
./build_all_at_once.sh            # default source: ./client_single.c
./build_all_at_once.sh other.c    # or any other source
```

Artifacts land in a fresh `./build/<YYYYMMDD_HHMMSS>/` per run.

## Why this exists — layered vs. one pass

The parent pipeline virtualizes the 15 functions **one at a time**, feeding each
Tigress output back in as the input to the next pass. That is "layered"
virtualization, and it compounds badly because `main` calls every other function
and is processed **last**:

| | Layered (`../build_and_test.sh`) | Single pass (this folder) |
|---|---|---|
| Tigress invocations | 15 | **1** |
| `main`'s bytecode array | `[1][2,490,051]` | `[1][684]` |
| Final generated `.c` | **185 MB** | **9.8 MB** |
| Final `.exe` | 18 MB | **500 KB** |
| `"inline assembly"` warnings | 14 (one per earlier layer) | **0** |

The generated C contains one statement per bytecode byte:

```c
_TIG_VZ_<tag>_1_main_$array[0][N] = _TIG_VZ_<tag>_1_main_<opcode>... ;
```

so a 2,490,051-byte bytecode array prints as ~175 MB of C. `main`'s array blows
up only in the layered run: by the time `main` is virtualized, the source already
contains 14 virtualized function bodies, and `main`'s VM absorbs them. Isolated
proof:

- `main` on the clean source → `[1][684]`, ~9.4 MB output, 0 warnings.
- `main` on the 14-layer accumulated source → `[1][2,490,051]`, ~185 MB, 14 warnings.
- Reversed order (`initialize_network` on top of an already-virtualized `main`) → `[1][174]`, no compounding.

So the blow-up is specifically "virtualize a caller on top of its already-virtualized
callees". Doing everything in one pass avoids it entirely.

## The pointer-width patch (`patch_pointer_width.py`)

Tigress 4.2.0 emits `unsigned long` for pointer-sized values, which is **8 bytes
on Linux (LP64) but 4 bytes on Windows x64 (LLP64)** — so pointers get silently
truncated. This patcher fixes the defect in the generated C (never in Tigress,
never in system headers). It is a generalization of the parent folder's patcher,
because the single-pass output has a different shape:

* **Bug 1–3** (same as parent): `formalGlobalInit*` parameter/callee-state
  stores, `$array`/`$strings` pointer stores, and interpreter read-back sites,
  rewritten from `unsigned long` to `unsigned long long`.
* **Scoped declarations.** In a single pass Tigress numbers its temporaries
  globally and can reuse a name (e.g. `formalGlobalInit54` in both
  `connect_server` and `receive_response`). The declaration looked up is the last
  one preceding that function's first store, so there is no cross-function
  ambiguity. (The parent patcher fails with "ambiguous" on this shape.)
* **Optional read-back site.** Some functions (e.g. `main`) read their `$locals`
  slots directly with an already-64-bit cast and have no `formal$` opcode
  read-back; that is now a warning, not a failure.
* **Bug 4**: `PlusPI` sign-extension (`(ptrdiff_t)(long)`), unchanged.
* **Bug 5 (new, needed for the single-pass build).** The VM's **operand stack**
  union has an `unsigned long _unsigned_long` member, and opcodes such as
  `convert_char_star2unsigned_long` / `store_unsigned_long` route *pointer*
  values through it — truncating them on Windows exactly like Bug 1. Bug 5 widens
  that union member and every cast on the same line that produces/consumes it to
  64-bit.

  **Semantics caveat:** Bug 5 makes the virtualized program treat C
  `unsigned long` as 64-bit (as on Linux) rather than Windows' 32-bit. That is
  safe for this program (`client_single.c` uses `unsigned long` only for
  `strtoul`'d sizes and `%lu` error text, never relying on 32-bit wraparound),
  but code that depends on 32-bit `unsigned long` arithmetic should be reviewed
  before applying it. The layered build never executes the truncating opcodes for
  this program, which is why it works without Bug 5.

The patcher refuses (nonzero exit) on a shape it does not recognize rather than
patching blindly; the build script then fails loudly.

## Files

| File | Purpose |
|---|---|
| `build_all_at_once.sh` | The 5-stage single-pass pipeline (Tigress → patch → compile → PE check → Wine test) |
| `patch_pointer_width.py` | Generalized pointer-width patcher (Bugs 1–5, single-pass shape) |
| `client_single.c` | Source snapshot (copy of the parent folder's file), so this folder runs standalone |
| `ccwrap/x86_64-w64-mingw32-gcc` | Self-locating PATH-shadowing wrapper that injects `-Icompat` for Tigress's parse pass only |
| `compat/x86intrin.h`, `compat/emmintrin.h`, `compat/winnt.h` | Parser-time-only stubs (SIMD tree stubbed out; `C_ASSERT` neutralized) |
| `build/<timestamp>/` | Per-run artifacts: raw + patched C, exe, per-function patch logs, compile log, Wine log |

The `ccwrap/` + `compat/` workaround is unchanged from the parent folder — see
`../compat/README.md` for the full root-cause writeup (Tigress's CIL parser
cannot handle GCC's `vector_size` SIMD headers or Windows' `C_ASSERT`).

## Verified result

```
=== 1/5 Run Tigress (single pass, 15 functions) ===  OK: 161579 lines, all 15 virtualized.
=== 2/5 Validate + patch ===                        OK: all 15 functions patched (PATCH-BUG5 applied).
=== 3/5 Compile ===                                  OK: compiled.
=== 4/5 PE Sanity Check ===                          OK: PE32+ x86-64, KERNEL32.dll + WS2_32.dll.
=== 5/5 Wine Test ===                                all 8 markers, exit 0.

PASS: client_single.c - single-pass, 15 of 15 functions virtualized.
```

## Caveats

- Tested under Wine 9.0 only; not on native Windows hardware.
- Bug 5 changes `unsigned long` semantics as described above — the single main
  behavioural compromise in this pipeline.
- The pointer-width fix lives in the generated C, not in Tigress, so
  `patch_pointer_width.py` must run after every fresh `--Transform=Virtualize`
  run against an `x86_64:Windows:*` target — which the build script does.
