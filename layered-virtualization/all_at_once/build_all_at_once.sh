#!/usr/bin/env bash
#
# build_all_at_once.sh - Tigress Virtualize -> Windows x64 PE -> Wine pipeline
#                        (SINGLE-PASS variant)
# Usage: ./build_all_at_once.sh [source.c]   (default: client_single.c)
#
# Contrast with the parent project's build_and_test.sh:
#
#   build_and_test.sh   virtualizes the 15 functions ONE AT A TIME, feeding
#                       each output back in as the next input.  Because `main`
#                       calls everything and is done LAST, its virtualized VM
#                       absorbs all 14 earlier layers: `main`'s bytecode array
#                       becomes 2,490,051 bytes and the final C file ~185 MB.
#
#   this script         virtualizes ALL 15 functions in ONE `tigress` pass on
#                       the ORIGINAL source.  No layer ever sees another
#                       layer's output, so `main`'s bytecode stays 684 bytes
#                       and the final C file is ~10 MB (18x smaller).
#
# Pipeline:
#   1. Run Tigress once: --Functions=<all 15>
#   2. Validate + patch with patch_pointer_width.py, once per function
#      (the patcher is idempotent; it also applies the file-global Bug 5 fix)
#   3. Compile with the REAL MinGW toolchain
#   4. Sanity-check the PE (arch + KERNEL32/WS2_32 imports)
#   5. Start a test HTTP server, run under Wine, verify output markers
#
# All artifacts go under ./build/<YYYYMMDD_HHMMSS>/ so runs never overwrite
# each other.

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR"

step() { printf '\n=== %s ===\n' "$1"; }
die()  { printf 'FAIL: %s\n' "$1" >&2; exit 1; }
info() { printf 'INFO: %s\n' "$1"; }

# ---------- argument / prereq checks ----------
SOURCE_C="${1:-client_single.c}"
[ -f "$SOURCE_C" ] || die "missing $SOURCE_C - run from this directory."

[ -x "$PROJECT_DIR/ccwrap/x86_64-w64-mingw32-gcc" ] || die "missing ccwrap/x86_64-w64-mingw32-gcc"
[ -f "$PROJECT_DIR/compat/x86intrin.h" ]            || die "missing compat/x86intrin.h"
[ -f "$PROJECT_DIR/compat/emmintrin.h" ]            || die "missing compat/emmintrin.h"
[ -f "$PROJECT_DIR/compat/winnt.h" ]                || die "missing compat/winnt.h"
command -v tigress                 >/dev/null || die "'tigress' not found on PATH."
command -v x86_64-w64-mingw32-gcc  >/dev/null || die "'x86_64-w64-mingw32-gcc' not found on PATH."
command -v wine                    >/dev/null || die "'wine' not found on PATH."
command -v python3                 >/dev/null || die "'python3' not found on PATH."

# ---------- unique build directory ----------
RUN_ID="$(date +%Y%m%d_%H%M%S)"
BUILD_DIR="$PROJECT_DIR/build/$RUN_ID"
mkdir -p "$BUILD_DIR"
info "Build directory: $BUILD_DIR"
info "Source file:     $SOURCE_C"

# ---------- the single-pass function list ----------
FUNCTIONS=(
    initialize_network
    cleanup_network
    close_connection
    test_payload
    free_test_memory
    inspect_memory
    allocate_test_memory
    copy_test_data
    send_request
    receive_response
    connect_server
    find_header_end
    parse_response
    process_response
    main
)
FUNCTION_LIST="$(IFS=,; echo "${FUNCTIONS[*]}")"

# Compiler flags — -lws2_32 must come AFTER the source file so the linker
# sees unresolved references before it discards the import library.
MINGW_OPT=-O0
MINGW_LIBS=(-lws2_32)

# ---------- 1/5 Tigress (one pass, all functions) ----------
step "1/5 Run Tigress (single pass, ${#FUNCTIONS[@]} functions)"
RAW_C="$BUILD_DIR/all_at_once.raw.c"
PATCHED_C="$BUILD_DIR/all_at_once.patched.c"
TIGRESS_LOG="$BUILD_DIR/tigress.log"

info "Functions: $FUNCTION_LIST"
PATH="$PROJECT_DIR/ccwrap:$PATH" tigress \
    --Environment=x86_64:Windows:Gcc \
    --Compiler=x86_64-w64-mingw32-gcc \
    --Transform=Virtualize \
    --Functions="$FUNCTION_LIST" \
    --out="$RAW_C" \
    "$SOURCE_C" >"$TIGRESS_LOG" 2>&1

[ -s "$RAW_C" ] || die "Tigress produced empty output - see $TIGRESS_LOG"

# Every requested function must actually have been virtualized.
missing=()
for fn in "${FUNCTIONS[@]}"; do
    grep -q "_TIG_VZ_[A-Za-z0-9]*_1_${fn}_" "$RAW_C" || missing+=("$fn")
done
[ ${#missing[@]} -eq 0 ] || die "not virtualized: ${missing[*]} - see $TIGRESS_LOG"
info "OK: $(wc -l < "$RAW_C") lines, all ${#FUNCTIONS[@]} functions virtualized."

# ---------- 2/5 Validate + patch ----------
step "2/5 Validate + patch (patch_pointer_width.py)"
cp "$RAW_C" "$PATCHED_C"
patch_fails=0
for fn in "${FUNCTIONS[@]}"; do
    if python3 "$PROJECT_DIR/patch_pointer_width.py" "$PATCHED_C" "$PATCHED_C.tmp" "$fn" \
            >"$BUILD_DIR/patch_$fn.log" 2>&1; then
        mv "$PATCHED_C.tmp" "$PATCHED_C"
        printf '  %-22s %s\n' "$fn" "$(head -1 "$BUILD_DIR/patch_$fn.log")"
    else
        printf '  %-22s FAIL: %s\n' "$fn" "$(head -1 "$BUILD_DIR/patch_$fn.log")"
        patch_fails=$((patch_fails + 1))
        rm -f "$PATCHED_C.tmp"
    fi
done
[ "$patch_fails" -eq 0 ] || die "$patch_fails function(s) failed to patch - every function must patch cleanly or the build will crash at runtime (this is exactly the Tigress LLP64 bug)."
grep -h "PATCH-BUG5" "$BUILD_DIR"/patch_*.log | head -1 | sed 's/^/  /' || true
info "OK: all ${#FUNCTIONS[@]} functions patched."

# ---------- 3/5 Compile ----------
step "3/5 Compile"
FINAL_EXE="$BUILD_DIR/client_single_vz.exe"
COMPILE_LOG="$BUILD_DIR/compile.log"
x86_64-w64-mingw32-gcc "$MINGW_OPT" -o "$FINAL_EXE" "$PATCHED_C" "${MINGW_LIBS[@]}" >"$COMPILE_LOG" 2>&1 \
    || { tail -40 "$COMPILE_LOG" >&2; die "compile failed - see $COMPILE_LOG"; }
# Match only real GCC diagnostics, never the word 'error' inside a printed
# header macro line (e.g. '#define WSAMAKEASYNCREPLY(buflen,error) ...').
grep -qE ': (fatal )?error:' "$COMPILE_LOG" \
    && { cat "$COMPILE_LOG" >&2; die "compiler reported errors despite exit 0."; }
[ -x "$FINAL_EXE" ] || die "$FINAL_EXE was not produced."
info "OK: compiled ($(grep -ci warning "$COMPILE_LOG" || true) warnings, MinGW header noise expected)."

# ---------- 4/5 PE sanity check ----------
step "4/5 PE Sanity Check"
FILE_OUT="$(file "$FINAL_EXE")"
echo "$FILE_OUT"
case "$FILE_OUT" in
    *"PE32+"*"x86-64"*"MS Windows"*) : ;;
    *) die "unexpected file type: $FILE_OUT" ;;
esac
OBJDUMP_OUT="$(x86_64-w64-mingw32-objdump -p "$FINAL_EXE")"
[[ "$OBJDUMP_OUT" == *"KERNEL32.dll"* ]] || die "No KERNEL32.dll import."
[[ "$OBJDUMP_OUT" == *"WS2_32.dll"* ]]   || die "No WS2_32.dll import (Winsock2 linkage missing)."
info "OK: PE32+ x86-64 with KERNEL32.dll and WS2_32.dll imports."

# ---------- 5/5 Wine test ----------
step "5/5 Wine Test (with test server)"
TEST_SERVER_SCRIPT="$BUILD_DIR/test_server.py"
TEST_SERVER_LOG="$BUILD_DIR/test-server.log"
WINE_LOG="$BUILD_DIR/wine-run.log"

cat > "$TEST_SERVER_SCRIPT" << 'PYEOF'
#!/usr/bin/env python3
import socket, sys

PORT   = 8000
BODY   = b"STATUS=READY\nTYPE=TEST\nSIZE=4\nDATA=AAAA\n"
HEADER = (
    b"HTTP/1.1 200 OK\r\n"
    b"Content-Type: text/plain\r\n"
    b"Connection: close\r\n"
    b"Content-Length: " + str(len(BODY)).encode() + b"\r\n"
    b"\r\n"
)
RESPONSE = HEADER + BODY

srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
try:
    srv.bind(("127.0.0.1", PORT))
except OSError as e:
    print(f"bind failed: {e}", flush=True)
    sys.exit(1)
srv.listen(1)
srv.settimeout(30)
print(f"test server ready on 127.0.0.1:{PORT}", flush=True)
sys.stdout.flush()
try:
    conn, _ = srv.accept()
    buf = b""
    while b"\r\n\r\n" not in buf:
        chunk = conn.recv(4096)
        if not chunk:
            break
        buf += chunk
    conn.sendall(RESPONSE)
    try:
        conn.shutdown(socket.SHUT_WR)
    except OSError:
        pass
    conn.close()
    print("test server: request served.", flush=True)
except socket.timeout:
    print("test server: timed out waiting for client.", flush=True)
    sys.exit(1)
finally:
    srv.close()
PYEOF

info "Starting test server..."
python3 "$TEST_SERVER_SCRIPT" >"$TEST_SERVER_LOG" 2>&1 &
SERVER_PID=$!

for _i in 1 2 3 4 5; do
    grep -q "ready" "$TEST_SERVER_LOG" 2>/dev/null && break
    sleep 0.5
done

set +e
WINE_OUT="$(WINEDEBUG=-all timeout 60 wine "$FINAL_EXE" </dev/null 2>"$WINE_LOG")"
WINE_STATUS=$?
set -e

wait "$SERVER_PID" 2>/dev/null || true

WINE_OUT="$(printf '%s' "$WINE_OUT" | tr -d '\r')"

echo "--- Wine output ---"
echo "$WINE_OUT"
echo "--- Wine exit: $WINE_STATUS ---"

[ "$WINE_STATUS" -eq 0 ] || { cat "$WINE_LOG" >&2; die "wine exited $WINE_STATUS (expected 0)."; }

check_marker() {
    [[ "$WINE_OUT" == *"$1"* ]] || die "expected output marker not found: '$1'"
}
check_marker "[+] Client started"
check_marker "[+] Network initialized"
check_marker "[+] Request sent"
check_marker "[+] Response received"
check_marker "[+] Memory allocated"
check_marker "Payload test completed"
check_marker "[+] Test payload executed"
check_marker "[+] Cleanup complete"

echo
echo "PASS: ${SOURCE_C} - single-pass, ${#FUNCTIONS[@]} of ${#FUNCTIONS[@]} functions virtualized."
echo
echo "Build directory: $BUILD_DIR"
echo
echo "NOTE: tested under Wine only. Native Windows compatibility not verified."
