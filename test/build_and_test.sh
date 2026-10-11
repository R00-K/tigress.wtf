#!/usr/bin/env bash
#
# build_and_test.sh - repeatable Tigress Virtualize -> Windows x64 PE ->
# Wine verification pipeline for calculate() in windows_api_test.c.
#
# Never touches windows_api_test.c, windows_api_test_obf.c(.orig),
# windows_api_test_obf.exe, or anything else already in the project
# root - all generated/patched/built artifacts for this script go under
# ./build/, so previously validated evidence is preserved untouched.
#
# Steps (exits nonzero and stops at the first failure):
#   1. Run Tigress (via ccwrap/ + compat/) to regenerate the obfuscated
#      source for `calculate`.
#   2. Validate the generated source has EXACTLY the known pointer-width
#      defect shape (see compat/README.md, Part 2) and apply the
#      narrowly-scoped patch - or fail with a clear error if the shape
#      differs (patch_pointer_width.py never guesses).
#   3. Compile the patched source with the real, unmodified MinGW
#      toolchain into a Windows x64 PE.
#   4. Sanity-check the PE (architecture + a real KERNEL32.dll import).
#   5. Run it under Wine and assert it prints "Result: 30" and exits 0.
#
# This script only exercises Wine. It does NOT establish behavior on
# real Windows - see the final printed caveat.

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR"

SOURCE_C="windows_api_test.c"
FUNCTION="calculate"
BUILD_DIR="$PROJECT_DIR/build"
RAW_C="$BUILD_DIR/windows_api_test_obf.generated.c"
PATCHED_C="$BUILD_DIR/windows_api_test_obf.patched.c"
TEST_EXE="$BUILD_DIR/windows_api_test_obf.test.exe"
TIGRESS_LOG="$BUILD_DIR/tigress.log"
COMPILE_LOG="$BUILD_DIR/mingw-compile.log"
WINE_LOG="$BUILD_DIR/wine-run.log"

step() { printf '\n=== %s ===\n' "$1"; }
die() { printf 'FAIL: %s\n' "$1" >&2; exit 1; }

[ -f "$SOURCE_C" ] || die "missing $SOURCE_C - run this from the project directory."
[ -x "$PROJECT_DIR/ccwrap/x86_64-w64-mingw32-gcc" ] || die "missing ccwrap/x86_64-w64-mingw32-gcc wrapper."
[ -f "$PROJECT_DIR/compat/x86intrin.h" ] || die "missing compat/x86intrin.h stub."
[ -f "$PROJECT_DIR/compat/emmintrin.h" ] || die "missing compat/emmintrin.h stub."
command -v tigress >/dev/null || die "'tigress' not found on PATH."
command -v x86_64-w64-mingw32-gcc >/dev/null || die "'x86_64-w64-mingw32-gcc' not found on PATH."
command -v wine >/dev/null || die "'wine' not found on PATH (install wine64 first)."

mkdir -p "$BUILD_DIR"
rm -f "$RAW_C" "$PATCHED_C" "$TEST_EXE"

step "1/5 Run Tigress (--Transform=Virtualize --Functions=$FUNCTION)"
if ! PATH="$PROJECT_DIR/ccwrap:$PATH" tigress \
      --Environment=x86_64:Windows:Gcc \
      --Compiler=x86_64-w64-mingw32-gcc \
      --Transform=Virtualize \
      --Functions="$FUNCTION" \
      --out="$RAW_C" \
      "$SOURCE_C" > "$TIGRESS_LOG" 2>&1; then
  cat "$TIGRESS_LOG" >&2
  die "tigress exited nonzero - see $TIGRESS_LOG"
fi
[ -s "$RAW_C" ] || { cat "$TIGRESS_LOG" >&2; die "tigress reported success but $RAW_C is missing/empty."; }
grep -q "_TIG_VZ_[A-Za-z0-9]*_1_${FUNCTION}_" "$RAW_C" \
  || die "$RAW_C has no _TIG_VZ_*_1_${FUNCTION}_* symbols - '$FUNCTION' was not virtualized."
echo "OK: generated $(wc -l < "$RAW_C") lines, '$FUNCTION' virtualized."

step "2/5 Validate + patch the pointer-width defect"
python3 "$PROJECT_DIR/patch_pointer_width.py" "$RAW_C" "$PATCHED_C" "$FUNCTION"
[ -s "$PATCHED_C" ] || die "patch step reported success but $PATCHED_C is missing/empty."

step "3/5 Compile the Windows x64 executable (real MinGW toolchain, no wrapper)"
if ! x86_64-w64-mingw32-gcc -O0 -o "$TEST_EXE" "$PATCHED_C" > "$COMPILE_LOG" 2>&1; then
  tail -40 "$COMPILE_LOG" >&2
  die "x86_64-w64-mingw32-gcc exited nonzero - see $COMPILE_LOG"
fi
grep -qi '\berror\b' "$COMPILE_LOG" && { cat "$COMPILE_LOG" >&2; die "compiler reported an error despite exit 0."; }
[ -x "$TEST_EXE" ] || die "$TEST_EXE was not produced."
echo "OK: compiled with no errors ($(grep -ci warning "$COMPILE_LOG" || true) warnings, pre-existing mingw-header noise expected)."

step "4/5 Sanity-check the PE"
FILE_OUT="$(file "$TEST_EXE")"
echo "$FILE_OUT"
case "$FILE_OUT" in
  *"PE32+ executable"*"x86-64"*"MS Windows"*) : ;;
  *) die "unexpected file type for $TEST_EXE: $FILE_OUT" ;;
esac
# Pure bash substring test, deliberately not piped into `grep -q`: with
# `pipefail` set, grep -q exits the instant it finds a match and can
# SIGPIPE whichever producer (objdump, or even `echo` of an already-
# captured variable) is still writing, making a successful match look
# like a pipeline failure.
OBJDUMP_OUT="$(x86_64-w64-mingw32-objdump -p "$TEST_EXE")"
[[ "$OBJDUMP_OUT" == *"KERNEL32.dll"* ]] \
  || die "$TEST_EXE has no KERNEL32.dll import - Windows API linkage looks wrong."
echo "OK: genuine PE32+ x86-64 Windows executable with real KERNEL32.dll imports."

step "5/5 Run under Wine and verify calculate(10, 20) == 30"
set +e
WINE_OUT="$(WINEDEBUG=-all timeout 20 wine "$TEST_EXE" </dev/null 2>"$WINE_LOG")"
WINE_STATUS=$?
set -e
WINE_OUT="${WINE_OUT%$'\r'}"  # the Windows CRT writes CRLF; $(...) only strips the trailing \n
echo "$WINE_OUT"
[ "$WINE_STATUS" -eq 0 ] || { cat "$WINE_LOG" >&2; die "wine exited $WINE_STATUS (expected 0) - see $WINE_LOG"; }
[ "$WINE_OUT" = "Result: 30" ] || die "unexpected program output: '$WINE_OUT' (expected 'Result: 30')"

echo
echo "PASS: calculate(10, 20) = 30"
echo
echo "NOTE: this only proves correctness under Wine 9.0. It has not been"
echo "tested on real Windows in this run - do not report native Windows"
echo "compatibility as confirmed without testing there."
