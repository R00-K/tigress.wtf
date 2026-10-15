#!/usr/bin/env python3
"""
patch_pointer_width.py <generated.c> <patched.c> <function_name>

Fixes 32-bit truncation bugs in Tigress Virtualize output for <function_name>
on Windows x64 where sizeof(unsigned long)==4.

Bug 1 – formalGlobalInit* parameter stores (_TIG_IZ_* globals):
  Original:  *formalGlobalInitN = (unsigned long)(& _TIG_IZ_*_formal_x);
  Patched:   *formalGlobalInitN = (unsigned long long)(& _TIG_IZ_*_formal_x);

Bug 2 – $array and $strings pointer stores (same pattern, broader RHS):
  Original:  *formalGlobalInitN = (unsigned long)(_TIG_VZ_*_$array);
  Original:  *formalGlobalInitN = (unsigned long)(& _TIG_VZ_*_$strings);
  Patched:   *formalGlobalInitN = (unsigned long long)(_TIG_VZ_*_$array);
  Patched:   *formalGlobalInitN = (unsigned long long)(& _TIG_VZ_*_$strings);

Bug 3 – read-back sites:
  Original:  (char *)*((unsigned long *)(locals + *((int *)pc[N])));
  Patched:   (char *)*((unsigned long long *)(locals + *((int *)pc[N])));

Bug 4 – PlusPI_char_star_unsigned_long2char_star sign-extension:
  Original:  ... + ($sp[0] + -1)->_unsigned_long;
  Patched:   ... + (ptrdiff_t)(long)($sp[0] + -1)->_unsigned_long;
  (unsigned long is 32-bit on MinGW-64; negative offsets like -1=0xFFFFFFFF must be
  sign-extended to 64-bit ptrdiff_t, not zero-extended, to avoid +4GB pointer wraps)

This version handles ALL formalGlobalInit* store pairs in the target function's
$locals block regardless of RHS form (with/without &, with/without $ in ident).
"""
import re
import sys


def fail(msg):
    print(f"PATCH-FAIL: {msg}", file=sys.stderr)
    sys.exit(1)


def main():
    if len(sys.argv) != 4:
        fail("usage: patch_pointer_width.py <in.c> <out.c> <function_name>")
    in_path, out_path, fn = sys.argv[1], sys.argv[2], sys.argv[3]

    with open(in_path, "r", encoding="utf-8", errors="surrogateescape") as f:
        src = f.read()

    # --- discover the Tigress random tag for this function ---
    locals_re = re.compile(
        r"_TIG_VZ_([A-Za-z0-9]+)_1_" + re.escape(fn) + r"_\$locals\b"
    )
    tags = set(locals_re.findall(src))
    if len(tags) == 0:
        fail(
            f"no '_TIG_VZ_<tag>_1_{fn}_$locals' identifier found - "
            f"was --Functions={fn} actually virtualized?"
        )
    if len(tags) > 1:
        fail(
            f"found {len(tags)} distinct _TIG_VZ_<tag>_1_{fn}_$locals "
            f"identifiers ({sorted(tags)}) - expected exactly 1."
        )
    tag = next(iter(tags))
    locals_name = f"_TIG_VZ_{tag}_1_{fn}_$locals"
    pc_name     = f"_TIG_VZ_{tag}_1_{fn}_$pc"
    sp_name     = f"_TIG_VZ_{tag}_1_{fn}_$sp"

    # --- find ALL store pairs scoped to this function's locals ---
    # Handles both _TIG_IZ_* (formal params) and _TIG_VZ_*_$array/$strings
    # (inlined callee VM bytecode/string pointers).  The RHS may be:
    #   (& identifier)   or   (identifier)    where identifier may contain $.
    # [^)]+ matches any expression without a closing paren (none of the actual
    # RHS values have nested parens).
    ul_pairs_re = re.compile(
        r"(?P<var>formalGlobalInit\d+) = \(unsigned long \*\)\(\(char \*\)\(\(void \*\)\("
        + re.escape(locals_name)
        + r"\)\) \+ (?P<offset>\d+)\);\n"
        + r"[ \t]*\*(?P=var) = \(unsigned long \)\([^)]+\);\n"
    )
    ull_pairs_re = re.compile(
        r"(?P<var>formalGlobalInit\d+) = \(unsigned long long \*\)\(\(char \*\)\(\(void \*\)\("
        + re.escape(locals_name)
        + r"\)\) \+ (?P<offset>\d+)\);\n"
        + r"[ \t]*\*(?P=var) = \(unsigned long long \)\([^)]+\);\n"
    )

    ul_pairs  = list(ul_pairs_re.finditer(src))
    ull_pairs = list(ull_pairs_re.finditer(src))

    out = src  # start with original; edits applied below

    if not ul_pairs and not ull_pairs:
        skip_msg = (
            f"PATCH-SKIP: no formalGlobalInit* store pairs found for '{fn}' in "
            f"{locals_name} - function has no pointer parameters."
        )
    elif not ul_pairs:
        skip_msg = (
            f"PATCH-SKIP: all {len(ull_pairs)} store pair(s) for '{fn}' already "
            f"use 'unsigned long long' - already fixed."
        )
    else:
        skip_msg = None

    if skip_msg is None:
        # --- validate offsets ---
        offsets = [int(m.group("offset")) for m in ul_pairs]
        if len(set(offsets)) != len(offsets):
            fail(f"store offsets for '{fn}' are not distinct: {offsets}.")
        for off in offsets:
            if off % 8 != 0:
                fail(f"store offset {off} for '{fn}' is not 8-byte aligned.")

        # --- for each ul var, locate its declaration (once per unique var name) ---
        edits = []
        seen_vars = set()
        for m in ul_pairs:
            var = m.group("var")
            if var not in seen_vars:
                seen_vars.add(var)
                var_decl_re = re.compile(
                    r"(?P<indent>[ \t]*)unsigned long \*" + re.escape(var) + r" ;\n"
                )
                var_decls = list(var_decl_re.finditer(src))
                if len(var_decls) == 0:
                    fail(f"no 'unsigned long *{var} ;' declaration found.")
                if len(var_decls) > 1:
                    fail(f"found {len(var_decls)} declarations of '{var}' - ambiguous.")
                decl = var_decls[0]
                edits.append((decl.start(), decl.end(),
                              decl.group(0).replace("unsigned long *", "unsigned long long *", 1)))

            # store-pair edit: change (unsigned long *) in pointer assign and
            # (unsigned long ) in the dereference value - both become long long.
            new_store = m.group(0).replace(
                "(unsigned long *)", "(unsigned long long *)", 1
            ).replace(
                "(unsigned long )", "(unsigned long long )", 1
            )
            edits.append((m.start(), m.end(), new_store))

        # --- optional overlap check ---
        region_re = re.compile(
            r"\(\*\(\(union [A-Za-z0-9_$]+ \(\*\)\[1\]\[32\]\)\("
            + re.escape(locals_name)
            + r" \+ (?P<start>\d+)\)\)\)\[0\];"
        )
        region_matches = list(region_re.finditer(src))
        if region_matches:
            all_offsets = offsets + [int(m.group("offset")) for m in ull_pairs]
            region_start = int(region_matches[0].group("start"))
            highest_slot_end = max(all_offsets) + 8
            if highest_slot_end > region_start:
                fail(
                    f"parameter pointer slots end at byte {highest_slot_end} but the "
                    f"operand-stack node array starts at byte {region_start} - OVERLAP. "
                    f"Not patching; needs manual review."
                )
        else:
            print(
                "PATCH-WARN: could not locate the node-array region start to "
                "double-check for overlap (non-fatal; proceeding).",
                file=sys.stderr,
            )

        # --- locate all read-back sites for this function ---
        # Use \d+ for the pc index to handle functions with multiple pointer locals.
        read_re = re.compile(
            r"\(char \*\)\*\(\(unsigned long \*\)\("
            + re.escape(locals_name)
            + r" \+ \*\(\(int \*\)"
            + re.escape(pc_name)
            + r"\[\d+\]\)\)\);\n"
        )
        reads = list(read_re.finditer(src))
        if len(reads) == 0:
            fail(
                f"no read-back site of the form "
                f"'(char *)*((unsigned long *)({locals_name} + *((int *)"
                f"{pc_name}[N])));' found. Not patching."
            )
        for r in reads:
            edits.append((r.start(), r.end(),
                          r.group(0).replace("unsigned long *", "unsigned long long *", 1)))

        # --- apply all formalGlobalInit edits back-to-front ---
        edits.sort(key=lambda e: e[0], reverse=True)
        for start, end, text in edits:
            out = out[:start] + text + out[end:]

    # --- Bug 4: PlusPI_char_star_unsigned_long2char_star sign-extension fix ---
    # On Windows MinGW-64, unsigned long is 32-bit. When a negative offset is
    # encoded as (unsigned long)(-1) = 0xFFFFFFFF, PlusPI zero-extends it to
    # +4294967295 instead of sign-extending to -1, corrupting the pointer.
    # Fix: cast through (ptrdiff_t)(long) to sign-extend from 32 to 64 bits.
    pluspi_old = f"->_char_star + ({sp_name}[0] + -1)->_unsigned_long;\n"
    pluspi_new = f"->_char_star + (ptrdiff_t)(long)({sp_name}[0] + -1)->_unsigned_long;\n"
    n_pluspi = out.count(pluspi_old)
    if n_pluspi > 0:
        out = out.replace(pluspi_old, pluspi_new)

    with open(out_path, "w", encoding="utf-8", errors="surrogateescape") as f:
        f.write(out)

    if skip_msg is not None:
        suffix = f" + PlusPI fix ({n_pluspi})" if n_pluspi > 0 else ""
        print(f"{skip_msg}{suffix}")
    else:
        print(
            f"PATCH-OK: {len(ul_pairs)} store pair(s) for '{fn}' "
            f"(offsets {sorted(offsets)}) and {len(reads)} read-back(s) "
            f"rewritten to 'unsigned long long'; "
            f"PlusPI sign-extension: {n_pluspi} occurrence(s) in {out_path}"
        )


if __name__ == "__main__":
    main()
