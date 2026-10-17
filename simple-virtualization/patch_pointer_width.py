#!/usr/bin/env python3
"""
patch_pointer_width.py <generated.c> <patched.c> <function_name>

Fixes 32-bit pointer-truncation bugs in Tigress 4.2.0 Virtualize output for a
Windows x64 (LLP64) target, where sizeof(unsigned long) == 4 but a pointer is
8 bytes.

This is the ONE-PASS variant, used when every function is virtualized in a
single `tigress --Functions=fn1,fn2,...,main` invocation.  It differs from the
layered-build patcher in two ways:

  * Declaration lookup is scoped to the function.  In a single pass Tigress
    numbers its round-trip temporaries globally and can reuse the same name
    (e.g. `formalGlobalInit54`) in several functions, so a file-global search
    for the declaration is ambiguous.  The declaration that belongs to this
    function is the last one appearing before this function's first store to
    that temporary.

  * A missing interpreter read-back site is not fatal.  Some functions (notably
    `main`) read their $locals slots directly with an already-64-bit cast and
    have no `formal$` opcode read-back, so only the store side needs fixing.

Bug classes:

  Bug 1 - formalGlobalInit* parameter / callee-state stores:
    Original:  *formalGlobalInitN = (unsigned long )(& ident);
    Patched:   *formalGlobalInitN = (unsigned long long )(& ident);

  Bug 2 - $array and $strings pointer stores (same shape, bare identifier RHS):
    Original:  *formalGlobalInitN = (unsigned long )(_TIG_VZ_*_$array);
    Patched:   *formalGlobalInitN = (unsigned long long )(_TIG_VZ_*_$array);

  Bug 3 - read-back sites (patched only when present):
    Original:  (char *)*((unsigned long *)(locals + *((int *)pc[N])));
    Patched:   (char *)*((unsigned long long *)(locals + *((int *)pc[N])));

  Bug 4 - PlusPI_char_star_unsigned_long2char_star sign extension:
    Original:  ... ->_char_star + ($sp[0] + -1)->_unsigned_long;
    Patched:   ... ->_char_star + (ptrdiff_t)(long)($sp[0] + -1)->_unsigned_long;
    (unsigned long is 32-bit on MinGW-64; -1 is encoded 0xFFFFFFFF and would be
    zero-extended to +4294967295 instead of sign-extended to -1.)

It validates that each temporary's slots are distinct and 8-byte aligned and
that they do not overlap the operand-stack node array, and refuses (nonzero
exit) on a shape it does not recognize rather than patching blindly.
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

    n_pluspi = 0
    n_reads = 0

    if skip_msg is None:
        # --- validate offsets ---
        offsets = [int(m.group("offset")) for m in ul_pairs]
        if len(set(offsets)) != len(offsets):
            fail(f"store offsets for '{fn}' are not distinct: {offsets}.")
        for off in offsets:
            if off % 8 != 0:
                fail(f"store offset {off} for '{fn}' is not 8-byte aligned.")

        # --- for each ul var, locate its declaration SCOPED to this function ---
        edits = []
        seen_vars = set()
        for m in ul_pairs:
            var = m.group("var")
            if var not in seen_vars:
                seen_vars.add(var)
                var_decl_re = re.compile(
                    r"(?P<indent>[ \t]*)unsigned long \*" + re.escape(var) + r" ;\n"
                )
                first_use = min(
                    mm.start() for mm in ul_pairs if mm.group("var") == var
                )
                decls = [
                    d for d in var_decl_re.finditer(src) if d.start() < first_use
                ]
                if len(decls) == 0:
                    fail(
                        f"no 'unsigned long *{var} ;' declaration found before its "
                        f"first store in '{fn}'."
                    )
                decl = max(decls, key=lambda d: d.start())
                edits.append((decl.start(), decl.end(),
                              decl.group(0).replace("unsigned long *",
                                                    "unsigned long long *", 1)))

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

        # --- locate all read-back sites for this function (optional) ---
        read_re = re.compile(
            r"\(char \*\)\*\(\(unsigned long \*\)\("
            + re.escape(locals_name)
            + r" \+ \*\(\(int \*\)"
            + re.escape(pc_name)
            + r"\[\d+\]\)\)\);\n"
        )
        reads = list(read_re.finditer(src))
        n_reads = len(reads)
        if n_reads == 0:
            print(
                f"PATCH-WARN: no interpreter read-back site found for '{fn}' - it "
                f"reads its $locals slots directly (already 64-bit); patching the "
                f"store side only.",
                file=sys.stderr,
            )
        for r in reads:
            edits.append((r.start(), r.end(),
                          r.group(0).replace("unsigned long *",
                                             "unsigned long long *", 1)))

        # --- apply all edits back-to-front ---
        edits.sort(key=lambda e: e[0], reverse=True)
        for start, end, text in edits:
            out = out[:start] + text + out[end:]

    # --- Bug 4: PlusPI_char_star_unsigned_long2char_star sign-extension fix ---
    pluspi_old = f"->_char_star + ({sp_name}[0] + -1)->_unsigned_long;\n"
    pluspi_new = f"->_char_star + (ptrdiff_t)(long)({sp_name}[0] + -1)->_unsigned_long;\n"
    n_pluspi = out.count(pluspi_old)
    if n_pluspi > 0:
        out = out.replace(pluspi_old, pluspi_new)

    # --- Bug 5 (file-global, idempotent): the VM OPERAND STACK's 'unsigned long'
    #     representation is 4 bytes on Windows x64.  Several generated opcodes
    #     (convert_char_star2unsigned_long, store/load_unsigned_long, ...) route
    #     POINTER values through that member, so the pointer is truncated exactly
    #     like the formalGlobalInit bug.  Widen the operand-stack member and every
    #     cast on the same line that produces/consumes it, to 64 bits.
    #
    #     SEMANTICS NOTE: this makes the virtualized program treat C 'unsigned long'
    #     as 64-bit (as it is on Linux LP64) instead of Windows' 32-bit.  For a
    #     program whose unsigned-long values never rely on 32-bit wraparound (as is
    #     the case for client_single.c: only strtoul()'d sizes and %lu error text)
    #     this is safe; for code that genuinely depends on 32-bit unsigned long
    #     arithmetic it is a behaviour change and should be reviewed.
    n_b5_member = 0
    n_b5_cast = 0
    lines = out.split("\n")
    for i, ln in enumerate(lines):
        if "unsigned long _unsigned_long ;" in ln:
            ln = ln.replace("unsigned long _unsigned_long ;",
                            "unsigned long long _unsigned_long ;")
            n_b5_member += 1
        if "_unsigned_long" in ln:
            if "(unsigned long )" in ln:
                n_b5_cast += ln.count("(unsigned long )")
                ln = ln.replace("(unsigned long )", "(unsigned long long )")
            if "(unsigned long *)" in ln:
                n_b5_cast += ln.count("(unsigned long *)")
                ln = ln.replace("(unsigned long *)", "(unsigned long long *)")
        lines[i] = ln
    out = "\n".join(lines)

    with open(out_path, "w", encoding="utf-8", errors="surrogateescape") as f:
        f.write(out)

    if skip_msg is not None:
        suffix = f" + PlusPI fix ({n_pluspi})" if n_pluspi > 0 else ""
        print(f"{skip_msg}{suffix}")
    else:
        print(
            f"PATCH-OK: {len(ul_pairs)} store pair(s) for '{fn}' "
            f"(offsets {sorted(offsets)}) and {n_reads} read-back(s) "
            f"rewritten to 'unsigned long long'; "
            f"PlusPI sign-extension: {n_pluspi} occurrence(s)."
        )
    if n_b5_member or n_b5_cast:
        print(
            f"PATCH-BUG5: operand-stack 'unsigned long' widened to 64-bit "
            f"({n_b5_member} union member(s), {n_b5_cast} cast site(s)) - "
            f"see the Bug 5 semantics note in this script."
        )


if __name__ == "__main__":
    main()
