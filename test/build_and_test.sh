#!/usr/bin/env bash
#
# build_and_test.sh - repeatable Tigress Virtualize -> Windows x64 PE ->
# Wine verification pipeline for the client/ sources.
#
# Source entry point: client/tigressinput.c (combined translation unit that
# #include-s main.c, handler.c, memory.c, network.c, payload.c).
#
# All generated/patched/built artifacts go under ./build/ so previously
# validated evidence is preserved untouched.
#
# Steps (exits nonzero and stops at the first failure):
#   1. Run Tigress (via ccwrap/ + compat/) to generate the obfuscated source.
#   2. Validate + patch the pointer-width defect for each virtualized function
#      (unsigned long -> unsigned long long for formalGlobalInit* vars).
#   3. Compile the patched source with the real MinGW toolchain into a
#      Windows x64 PE (links -lws2_32 for Winsock).
#   4. Sanity-check the PE (architecture + KERNEL32.dll import).
#   5. Run under Wine: verify the program starts, initialises Winsock, and
#      exits cleanly without an access violation (0xC0000005).  A clean
#      "[-] Could not connect" exit (code 1) is expected since no server
#      is running on port 8000 during the test.

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR"

SOURCE_C="client/tigressinput.c"
FUNCTION="main,find_header_end,parse_response,process_response,allocate_test_memory,copy_test_data,inspect_memory,free_test_memory,initialize_network,connect_server,send_request,receive_response,close_connection,cleanup_network,test_payload"
BUILD_DIR="$PROJECT_DIR/build"
RAW_C="$BUILD_DIR/tigressinput_test_obf.generated.c"
PATCHED_C="$BUILD_DIR/tigressinput_test_obf.patched.c"
TEST_EXE="$BUILD_DIR/tigressinput_test_obf.test.exe"
TIGRESS_LOG="$BUILD_DIR/tigress.log"
COMPILE_LOG="$BUILD_DIR/mingw-compile.log"
WINE_LOG="$BUILD_DIR/wine-run.log"

step() { printf '\n=== %s ===\n' "$1"; }
die()  { printf 'FAIL: %s\n' "$1" >&2; exit 1; }

[ -f "$SOURCE_C" ] || die "missing $SOURCE_C - run this script from the project root (test/)."
[ -x "$PROJECT_DIR/ccwrap/x86_64-w64-mingw32-gcc" ] || die "missing ccwrap/x86_64-w64-mingw32-gcc wrapper."
[ -f "$PROJECT_DIR/compat/x86intrin.h" ]  || die "missing compat/x86intrin.h stub."
[ -f "$PROJECT_DIR/compat/emmintrin.h" ]  || die "missing compat/emmintrin.h stub."
command -v tigress             >/dev/null || die "'tigress' not found on PATH."
command -v x86_64-w64-mingw32-gcc >/dev/null || die "'x86_64-w64-mingw32-gcc' not found on PATH."
command -v wine                >/dev/null || die "'wine' not found on PATH (install wine64 first)."

mkdir -p "$BUILD_DIR"
rm -f "$RAW_C" "$PATCHED_C" "$TEST_EXE"

step "1/5 Run Tigress (--Transform=Virtualize --Functions=<all client fns>)"
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
# Check at least one virtualized symbol is present
if ! grep -q '_TIG_VZ_' "$RAW_C"; then
  cat "$TIGRESS_LOG" >&2
  die "$RAW_C has no _TIG_VZ_* symbols - virtualization silently produced nothing."
fi
echo "OK: generated $(wc -l < "$RAW_C") lines with _TIG_VZ_ symbols."

step "2/5 Validate + patch the pointer-width defect"
python3 "$PROJECT_DIR/patch_pointer_width.py" "$RAW_C" "$PATCHED_C" "$FUNCTION"
[ -s "$PATCHED_C" ] || die "patch step reported success but $PATCHED_C is missing/empty."

step "3/5 Compile the Windows x64 executable (real MinGW toolchain, no wrapper)"
if ! x86_64-w64-mingw32-gcc -O0 -o "$TEST_EXE" "$PATCHED_C" -lws2_32 > "$COMPILE_LOG" 2>&1; then
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
# Pure bash substring test - avoids SIGPIPE race with grep -q under pipefail
OBJDUMP_OUT="$(x86_64-w64-mingw32-objdump -p "$TEST_EXE")"
[[ "$OBJDUMP_OUT" == *"KERNEL32.dll"* ]] \
  || die "$TEST_EXE has no KERNEL32.dll import - Windows API linkage looks wrong."
echo "OK: genuine PE32+ x86-64 Windows executable with real KERNEL32.dll imports."

step "5/5 Run under Wine - verify no access violation and clean startup"
set +e
WINE_OUT="$(WINEDEBUG=-all timeout 30 wine "$TEST_EXE" </dev/null 2>"$WINE_LOG")"
WINE_STATUS=$?
set -e
# Strip trailing CR that Windows CRT emits before each LF; $(...) only strips the final \n
WINE_OUT="${WINE_OUT%$'\r'}"
echo "Wine exit status: $WINE_STATUS"
echo "$WINE_OUT"

# -1073741819 == 0xC0000005 == STATUS_ACCESS_VIOLATION
if [ "$WINE_STATUS" -eq -1073741819 ]; then
  cat "$WINE_LOG" >&2
  die "access violation (0xC0000005) under Wine - pointer-width patch may be incomplete."
fi
# Any other negative exit code is also a Windows crash (NTSTATUS)
if [ "$WINE_STATUS" -lt 0 ]; then
  cat "$WINE_LOG" >&2
  die "Wine exited with negative status $WINE_STATUS (Windows crash) - see $WINE_LOG"
fi
# The program should have printed its startup banner before any network op
[[ "$WINE_OUT" == *"[+] Client started"* ]] \
  || die "expected '[+] Client started' in Wine output - program may not have run. Got: '$WINE_OUT'"

echo
echo "PASS: virtualized client started cleanly under Wine (exit $WINE_STATUS)."
echo "      A non-zero exit is expected when no server is listening on port 8000."
echo
echo "NOTE: this only proves the virtualized binary runs without crashing under"
echo "Wine 9.0. Real-server and native-Windows execution are not verified here."
