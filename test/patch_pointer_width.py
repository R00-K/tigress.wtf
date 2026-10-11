#!/usr/bin/env python3
"""
patch_pointer_width.py <generated.c> <patched.c> <function_name>

Validates that a Tigress-generated Virtualize output for <function_name>
has EXACTLY the known pointer-width defect shape (see compat/README.md,
"Part 2"), then applies the narrowly-scoped unsigned-long -> unsigned-long-
long fix for that function only. Refuses (nonzero exit, clear message) if
the generated code's shape differs from what was audited, rather than
guessing or patching blindly.

This never does a global `unsigned long` -> `unsigned long long` replace.
It only rewrites the exact matched declaration/cast/read-back snippets for
the formal-parameter pointer round-trip of <function_name>.
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

    # The Tigress-generated identifiers embed a random tag, e.g.
    # _TIG_VZ_rYXd_1_calculate_$locals -- discover that tag for this
    # function rather than assuming it.
    locals_re = re.compile(
        r"_TIG_VZ_([A-Za-z0-9]+)_1_" + re.escape(fn) + r"_\$locals\b"
    )
    tags = set(locals_re.findall(src))
    if len(tags) == 0:
        fail(
            f"no '_TIG_VZ_<tag>_1_{fn}_$locals' identifier found - "
            f"was --Functions={fn} actually virtualized? Generated file "
            f"may be from a different Tigress run/version."
        )
    if len(tags) > 1:
        fail(
            f"found {len(tags)} distinct _TIG_VZ_<tag>_1_{fn}_$locals "
            f"identifiers ({sorted(tags)}) - expected exactly 1. Refusing "
            f"to guess which one to patch."
        )
    tag = next(iter(tags))
    locals_name = f"_TIG_VZ_{tag}_1_{fn}_$locals"
    pc_name = f"_TIG_VZ_{tag}_1_{fn}_$pc"

    # --- locate the declaration of the round-trip temp (any width) ---
    decl_re = re.compile(
        r"(?P<indent>[ \t]*)(?P<type>unsigned long(?: long)?) \*(?P<var>formalGlobalInit\d+) ;\n"
    )
    decls = list(decl_re.finditer(src))
    if len(decls) == 0:
        fail(
            "no 'unsigned long [long] *formalGlobalInitN ;' declaration "
            "found anywhere in the file - generated code shape has "
            "changed; nothing to validate against. Not patching."
        )
    if len(decls) > 1:
        fail(
            f"found {len(decls)} formalGlobalInit* declarations - "
            f"expected exactly 1 for a single-parameter-pair function "
            f"like {fn}. Refusing to guess; patch this script first."
        )
    decl = decls[0]
    var = decl.group("var")
    cur_type = decl.group("type")

    if cur_type == "unsigned long long":
        print(
            f"PATCH-SKIP: '{var}' is already declared 'unsigned long long' "
            f"(already fixed, or a newer Tigress build no longer has the "
            f"bug). Copying input unchanged."
        )
        with open(out_path, "w", encoding="utf-8", errors="surrogateescape") as f:
            f.write(src)
        return
    if cur_type != "unsigned long":
        fail(f"unexpected declared type '{cur_type}' for '{var}'.")

    # --- locate the store-cast + store-value pairs for this var ---
    cast_re = re.compile(
        re.escape(var)
        + r" = \(unsigned long \*\)\(\(char \*\)\(\(void \*\)\("
        + re.escape(locals_name)
        + r"\)\) \+ (?P<offset>\d+)\);\n"
        + r"[ \t]*\*"
        + re.escape(var)
        + r" = \(unsigned long \)\(& (?P<ident>\w+)\);\n"
    )
    pairs = list(cast_re.finditer(src))
    if len(pairs) == 0:
        fail(
            f"declaration for '{var}' found, but no matching "
            f"'{var} = (unsigned long *)(... {locals_name} ... + N); "
            f"*{var} = (unsigned long)(&X);' store pair found. Generated "
            f"shape differs from the audited defect. Not patching."
        )

    offsets = [int(m.group("offset")) for m in pairs]
    idents = [m.group("ident") for m in pairs]
    if len(set(offsets)) != len(offsets):
        fail(f"store offsets are not distinct: {offsets}. Refusing to patch.")
    for off in offsets:
        if off % 8 != 0:
            fail(f"store offset {off} for '{var}' is not 8-byte aligned.")

    # --- locate the operand-stack / node-array region start, to prove
    #     the parameter slots and the stack array don't overlap ---
    region_re = re.compile(
        r"\(\*\(\(union [A-Za-z0-9_$]+ \(\*\)\[1\]\[32\]\)\("
        + re.escape(locals_name)
        + r" \+ (?P<start>\d+)\)\)\)\[0\];"
    )
    region_matches = list(region_re.finditer(src))
    if region_matches:
        region_start = int(region_matches[0].group("start"))
        highest_slot_end = max(offsets) + 8
        if highest_slot_end > region_start:
            fail(
                f"parameter pointer slots end at byte {highest_slot_end} "
                f"but the operand-stack node array starts at byte "
                f"{region_start} - OVERLAP detected. Not patching; this "
                f"needs manual review, not an automated fix."
            )
    else:
        print(
            "PATCH-WARN: could not locate the node-array region start to "
            "double check for overlap (non-fatal; proceeding, since the "
            "store/read shape itself still matched exactly).",
            file=sys.stderr,
        )

    # --- locate the single read-back site for this var's buffer ---
    read_re = re.compile(
        r"\(char \*\)\*\(\(unsigned long \*\)\("
        + re.escape(locals_name)
        + r" \+ \*\(\(int \*\)"
        + re.escape(pc_name)
        + r"\[0\]\)\)\);\n"
    )
    reads = list(read_re.finditer(src))
    if len(reads) != 1:
        fail(
            f"expected exactly 1 read-back site of the form "
            f"'(char *)*((unsigned long *)({locals_name} + *((int *)"
            f"{pc_name}[0])));', found {len(reads)}. Not patching."
        )

    # --- all checks passed: apply the narrowly scoped rewrite ---
    # Edit only the exact matched spans (declaration, each store-cast
    # pair, each read-back site), applied back-to-front so earlier
    # offsets stay valid as the string length changes.
    out = src
    edits = []
    edits.append((decl.start(), decl.end(), decl.group(0).replace(
        "unsigned long *", "unsigned long long *", 1)))
    for m in pairs:
        new_text = m.group(0).replace(
            "unsigned long *", "unsigned long long *", 1
        ).replace(
            "unsigned long )(&", "unsigned long long )(&", 1
        )
        edits.append((m.start(), m.end(), new_text))
    for m in reads:
        new_text = m.group(0).replace("unsigned long *", "unsigned long long *", 1)
        edits.append((m.start(), m.end(), new_text))

    edits.sort(key=lambda e: e[0], reverse=True)
    for start, end, text in edits:
        out = out[:start] + text + out[end:]

    with open(out_path, "w", encoding="utf-8", errors="surrogateescape") as f:
        f.write(out)

    print(
        f"PATCH-OK: '{var}' (offsets {sorted(offsets)} for "
        f"{idents}) and its 1 read-back site rewritten to "
        f"'unsigned long long' in {out_path}"
    )


if __name__ == "__main__":
    main()
