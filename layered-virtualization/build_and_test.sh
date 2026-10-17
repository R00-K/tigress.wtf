#!/usr/bin/env bash
#
# build_and_test.sh - Tigress Virtualize -> Windows x64 PE -> Wine pipeline
# Usage: ./build_and_test.sh [source.c]   (default: client_single.c)
#
# Virtualizes every function in the source one-by-one:
#   For each function:
#     1. Run Tigress --Transform=Virtualize --Functions=<fn> on the current
#        (already partially virtualized) source.
#     2. Apply patch_pointer_width.py; if it reports a shape mismatch, fall
#        back to the raw Tigress output.
#     3. Try to compile with MinGW.  If it fails, report clearly and skip
#        that function (keeping the previous working source).
#   After all functions have been attempted:
#     4. Compile the final accumulated source.
#     5. Sanity-check the PE (arch + KERNEL32/WS2_32 imports).
#     6. Start a minimal Python test server on 127.0.0.1:8000, run the
#        executable under Wine, and verify expected output markers.
#
# Each invocation writes ALL artifacts under a new timestamped directory:
#   ./build/<YYYYMMDD_HHMMSS>/
# so earlier runs are never touched.

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR"

step() { printf '\n=== %s ===\n' "$1"; }
die()  { printf 'FAIL: %s\n' "$1" >&2; exit 1; }
info() { printf 'INFO: %s\n' "$1"; }

# ---------- argument / prereq checks ----------
SOURCE_C="${1:-client_single.c}"
[ -f "$SOURCE_C" ] || die "missing $SOURCE_C - run from the project directory."

[ -x "$PROJECT_DIR/ccwrap/x86_64-w64-mingw32-gcc" ] || die "missing ccwrap/x86_64-w64-mingw32-gcc"
[ -f "$PROJECT_DIR/compat/x86intrin.h" ]             || die "missing compat/x86intrin.h"
[ -f "$PROJECT_DIR/compat/emmintrin.h" ]             || die "missing compat/emmintrin.h"
command -v tigress                 >/dev/null || die "'tigress' not found on PATH."
command -v x86_64-w64-mingw32-gcc >/dev/null || die "'x86_64-w64-mingw32-gcc' not found on PATH."
command -v wine                   >/dev/null || die "'wine' not found on PATH."
command -v python3                >/dev/null || die "'python3' not found on PATH."

# ---------- unique build directory ----------
RUN_ID="$(date +%Y%m%d_%H%M%S)"
BUILD_DIR="$PROJECT_DIR/build/$RUN_ID"
mkdir -p "$BUILD_DIR"
info "Build directory: $BUILD_DIR"
info "Source file:     $SOURCE_C"

# ---------- functions to virtualize (ordered simple → complex) ----------
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

# Compiler flags — -lws2_32 must come AFTER the source file so the linker
# sees unresolved references before it discards the import library.
MINGW_OPT=-O0
MINGW_LIBS=(-lws2_32)

# ---------- helper functions ----------

run_tigress() {
    # run_tigress <in.c> <fn> <out.c> <log>
    PATH="$PROJECT_DIR/ccwrap:$PATH" tigress \
        --Environment=x86_64:Windows:Gcc \
        --Compiler=x86_64-w64-mingw32-gcc \
        --Transform=Virtualize \
        --Functions="$2" \
        --out="$3" \
        "$1" >"$4" 2>&1
}

run_patch() {
    # run_patch <raw.c> <patched.c> <fn> <log>  -> exits 0 on success/skip
    python3 "$PROJECT_DIR/patch_pointer_width.py" "$1" "$2" "$3" >"$4" 2>&1
}

run_compile() {
    # run_compile <src.c> <out.exe> <log>  -> exits 0 on clean compile
    # Source file MUST come before -l flags so the linker sees references first.
    x86_64-w64-mingw32-gcc "$MINGW_OPT" -o "$2" "$1" "${MINGW_LIBS[@]}" >"$3" 2>&1
}

# ---------- function-by-function virtualization loop ----------
current_src="$SOURCE_C"
virtualized=()
skipped=()
fn_index=0

for fn in "${FUNCTIONS[@]}"; do
    fn_index=$((fn_index + 1))
    step "$fn_index/${#FUNCTIONS[@]} Virtualizing: $fn"

    fn_dir="$BUILD_DIR/fn_${fn_index}_${fn}"
    mkdir -p "$fn_dir"

    raw_c="$fn_dir/${fn}.raw.c"
    patched_c="$fn_dir/${fn}.patched.c"
    tigress_log="$fn_dir/tigress.log"
    patch_log="$fn_dir/patch.log"
    compile_log="$fn_dir/compile.log"
    test_exe="$fn_dir/test_${fn}.exe"

    # --- A: Tigress ---
    info "  [A] Running Tigress on '$fn'..."
    if ! run_tigress "$current_src" "$fn" "$raw_c" "$tigress_log"; then
        info "  SKIP: Tigress failed for '$fn'"
        tail -5 "$tigress_log" | sed 's/^/      /' || true
        skipped+=("$fn:tigress_failed")
        continue
    fi
    if [ ! -s "$raw_c" ]; then
        info "  SKIP: Tigress produced empty output for '$fn'"
        skipped+=("$fn:empty_output")
        continue
    fi
    if ! grep -q "_TIG_VZ_[A-Za-z0-9]*_1_${fn}_" "$raw_c"; then
        info "  SKIP: no VZ symbols for '$fn' in Tigress output - function not virtualized"
        skipped+=("$fn:not_virtualized")
        continue
    fi
    info "  [A] OK: $(wc -l < "$raw_c") lines generated, '$fn' virtualized."

    # --- B: patch ---
    info "  [B] Applying patch_pointer_width.py..."
    working_c="$raw_c"
    if run_patch "$raw_c" "$patched_c" "$fn" "$patch_log"; then
        patch_msg="$(head -1 "$patch_log" 2>/dev/null || true)"
        info "  [B] $patch_msg"
        [ -s "$patched_c" ] && working_c="$patched_c"
    else
        patch_msg="$(head -1 "$patch_log" 2>/dev/null || true)"
        info "  [B] Patch step failed ('$patch_msg') - will try compiling raw output."
        working_c="$raw_c"
    fi

    # --- C: compile ---
    info "  [C] Compiling..."
    compile_ok=0
    if run_compile "$working_c" "$test_exe" "$compile_log"; then
        compile_ok=1
    elif [ "$working_c" = "$patched_c" ]; then
        info "  [C] Patched compile failed; retrying with raw output..."
        if run_compile "$raw_c" "$test_exe" "$compile_log"; then
            working_c="$raw_c"
            compile_ok=1
        fi
    fi

    if [ "$compile_ok" -eq 1 ]; then
        info "  [C] OK: compiled successfully."
        current_src="$working_c"
        virtualized+=("$fn")
    else
        info "  SKIP: compile failed for '$fn'."
        tail -20 "$compile_log" | sed 's/^/      /' || true
        skipped+=("$fn:compile_failed")
    fi
done

# ---------- summary ----------
step "Virtualization Summary"
echo "Virtualized (${#virtualized[@]}): ${virtualized[*]:-none}"
echo "Skipped     (${#skipped[@]}):     ${skipped[*]:-none}"

[ ${#virtualized[@]} -gt 0 ] || die "No functions were virtualized. Cannot continue."

# ---------- final compile ----------
step "Final Compile"
FINAL_C="$BUILD_DIR/client_single_vz_final.c"
FINAL_EXE="$BUILD_DIR/client_single_vz.exe"
FINAL_COMPILE_LOG="$BUILD_DIR/final-compile.log"

cp "$current_src" "$FINAL_C"
info "Final source: $FINAL_C ($(wc -l < "$FINAL_C") lines)"

if ! x86_64-w64-mingw32-gcc "$MINGW_OPT" -o "$FINAL_EXE" "$FINAL_C" "${MINGW_LIBS[@]}" >"$FINAL_COMPILE_LOG" 2>&1; then
    tail -40 "$FINAL_COMPILE_LOG" >&2
    die "Final compile failed - see $FINAL_COMPILE_LOG"
fi
# Match only real GCC diagnostics ("path:line:col: error: ..." / "fatal error:"),
# not arbitrary occurrences of the word 'error' — GCC warnings print macro
# source lines like '#define WSAMAKEASYNCREPLY(buflen,error) ...' which a
# naive \berror\b grep falsely treats as a compile error.
grep -qE ': (fatal )?error:' "$FINAL_COMPILE_LOG" \
    && { cat "$FINAL_COMPILE_LOG" >&2; die "Compiler reported errors despite exit 0."; }
[ -x "$FINAL_EXE" ] || die "$FINAL_EXE was not produced."
info "OK: compiled ($(grep -ci warning "$FINAL_COMPILE_LOG" || true) warnings, MinGW header noise expected)."

# ---------- PE sanity check ----------
step "PE Sanity Check"
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

# ---------- Wine test ----------
step "Wine Test (with test server)"

TEST_SERVER_PORT=8000
TEST_SERVER_SCRIPT="$BUILD_DIR/test_server.py"
TEST_SERVER_LOG="$BUILD_DIR/test-server.log"
WINE_LOG="$BUILD_DIR/wine-run.log"

# Write a minimal HTTP server that serves one request with the expected body
# format: STATUS=READY / TYPE=TEST / SIZE=N / DATA=<bytes>
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

# Wait until the server is actually listening before launching Wine
for _i in 1 2 3 4 5; do
    grep -q "ready" "$TEST_SERVER_LOG" 2>/dev/null && break
    sleep 0.5
done

set +e
WINE_OUT="$(WINEDEBUG=+seh,-all timeout 60 wine "$FINAL_EXE" </dev/null 2>"$WINE_LOG")"
WINE_STATUS=$?
set -e

wait "$SERVER_PID" 2>/dev/null || true

# Strip Windows CRLF so string comparisons work
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
echo "PASS: client_single.c - ${#virtualized[@]} of ${#FUNCTIONS[@]} functions virtualized."
echo "Virtualized: ${virtualized[*]}"
[ ${#skipped[@]} -gt 0 ] && echo "Skipped:     ${skipped[*]}"
echo
echo "Build directory: $BUILD_DIR"
echo
echo "NOTE: tested under Wine only. Native Windows compatibility not verified."
