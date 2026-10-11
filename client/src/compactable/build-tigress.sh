#!/usr/bin/env bash
set -euo pipefail

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
COMPACTABLE="$ROOT/compactable"
INPUT="$COMPACTABLE/memory.c"
OUTPUT="$COMPACTABLE/memory.virtualized.c"
EXE="$ROOT/client-obfuscated.exe"
VOBJ="$COMPACTABLE/memory.virtualized.o"
OOBJ="$COMPACTABLE/memory.original.o"
WOBJ="$COMPACTABLE/memory.virtualized.wrappers.o"
WRAPPERS="$COMPACTABLE/memory.virtualized.wrappers.c"
TIGRESS_BIN=${TIGRESS_BIN:-tigress}
MINGW_CC=${MINGW_CC:-x86_64-w64-mingw32-gcc}
# The Windows API-heavy functions are selectable, but TIGRESS 4.2.0's
# x86_64:Windows:Gcc virtualizer currently does not return reliably from
# VirtualAlloc/WriteProcessMemory in this project. Use the pure C inspector
# as the working default; callers can override this with --functions.
FUNCTIONS=${FUNCTIONS:-inspect_memory}

usage() {
    cat <<EOF
Usage: $0 [--functions function[,function...]] [--no-link]

Environment overrides:
  TIGRESS_BIN  TIGRESS 4.2.0 executable (default: tigress)
  MINGW_CC     MinGW-w64 compiler (default: x86_64-w64-mingw32-gcc)
  FUNCTIONS    default TIGRESS function list
EOF
}

while (($#)); do
    case "$1" in
        --functions)
            (($# >= 2)) || { echo "--functions requires a value" >&2; exit 2; }
            FUNCTIONS=$2
            shift 2
            ;;
        --functions=*)
            FUNCTIONS=${1#*=}
            shift
            ;;
        --no-link)
            NO_LINK=1
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

NO_LINK=${NO_LINK:-0}

IFS=, read -r -a SELECTED_FUNCTIONS <<< "$FUNCTIONS"
VRENAME_FLAGS=(-Dmain=tigress_input_main)
ORENAME_FLAGS=()
for function in allocate_test_memory copy_test_data inspect_memory free_test_memory; do
    VRENAME_FLAGS+=("-D${function}=tigress_virtualized_${function}")
done
for function in "${SELECTED_FUNCTIONS[@]}"; do
    case "$function" in
        allocate_test_memory|copy_test_data|inspect_memory|free_test_memory)
            ORENAME_FLAGS+=("-D${function}=tigress_original_${function}")
            ;;
        *)
            echo "unsupported memory function: $function" >&2
            exit 2
            ;;
    esac
done

# Keep the application source authoritative. This is the only source-level
# substitution: replace the problematic SDK include with the TIGRESS shim.
sed -e 's|^#include "memory.h"$|#include "../memory.h"|' \
    -e 's|^[[:space:]]*#include[[:space:]]*<windows\.h>[[:space:]]*$|#include "tigress_windows_memory.h"|' \
    "$ROOT/memory.c" > "$INPUT"

# TIGRESS is whole-program oriented and requires main(). This generated stub
# is renamed at compile time and never replaces the application's main().
printf '\nint main(void) { return 0; }\n' >> "$INPUT"

"$TIGRESS_BIN" \
    --Environment=x86_64:Windows:Gcc \
    --Compiler="$MINGW_CC" \
    --Transform=Virtualize \
    --Functions="$FUNCTIONS" \
    --out="$OUTPUT" \
    "$INPUT"

if [[ "$NO_LINK" == 1 ]]; then
    echo "TIGRESS output: $OUTPUT"
    exit 0
fi

"$MINGW_CC" \
    -std=c11 -Wall -Wextra -O2 \
    -I"$ROOT" -I"$COMPACTABLE" \
    "${VRENAME_FLAGS[@]}" -c "$OUTPUT" -o "$VOBJ"

"$MINGW_CC" \
    -std=c11 -Wall -Wextra -O2 \
    -I"$ROOT" -I"$COMPACTABLE" \
    "${ORENAME_FLAGS[@]}" -c "$ROOT/memory.c" -o "$OOBJ"

{
    printf '%s\n' '#include "../memory.h"'
    printf '%s\n' 'int tigress_virtualized_allocate_test_memory(TestMemory *, size_t);'
    printf '%s\n' 'int tigress_virtualized_copy_test_data(TestMemory *, const char *, size_t);'
    printf '%s\n' 'void tigress_virtualized_inspect_memory(const TestMemory *, size_t);'
    printf '%s\n' 'void tigress_virtualized_free_test_memory(TestMemory *);'
    for function in "${SELECTED_FUNCTIONS[@]}"; do
        case "$function" in
            allocate_test_memory) printf '%s\n' 'int allocate_test_memory(TestMemory *m, size_t s) { return tigress_virtualized_allocate_test_memory(m, s); }' ;;
            copy_test_data) printf '%s\n' 'int copy_test_data(TestMemory *m, const char *d, size_t n) { return tigress_virtualized_copy_test_data(m, d, n); }' ;;
            inspect_memory) printf '%s\n' 'void inspect_memory(const TestMemory *m, size_t n) { tigress_virtualized_inspect_memory(m, n); }' ;;
            free_test_memory) printf '%s\n' 'void free_test_memory(TestMemory *m) { tigress_virtualized_free_test_memory(m); }' ;;
        esac
    done
} > "$WRAPPERS"

"$MINGW_CC" -std=c11 -Wall -Wextra -O2 -I"$ROOT" -I"$COMPACTABLE" \
    -c "$WRAPPERS" -o "$WOBJ"

"$MINGW_CC" \
    -std=c11 -Wall -Wextra -O2 \
    -I"$ROOT" -I"$COMPACTABLE" \
    "$OOBJ" "$VOBJ" "$WOBJ" \
    "$ROOT/network.c" "$ROOT/handler.c" "$ROOT/payload.c" "$ROOT/main.c" \
    -o "$EXE" \
    -lws2_32 -lkernel32

echo "TIGRESS output: $OUTPUT"
echo "Windows executable: $EXE"
