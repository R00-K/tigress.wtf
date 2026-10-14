#!/usr/bin/env python3
"""
patch_pointer_width.py <generated.c> <patched.c> <fn1[,fn2,...]>

For each named function (comma-separated list), validates that Tigress's
Virtualize output has the known pointer-width defect shape (see
compat/README.md Part 2) and applies the narrowly-scoped
unsigned long -> unsigned long long fix for the formalGlobalInit*
variables used to round-trip formal-parameter addresses through the
VM's $locals buffer.

Handles multiple functions in a single pass.  Refuses (nonzero exit) if
a store-cast pair exists but no matching declaration is found, or if a
read-back site count is not exactly 1 per function that has store-casts.
Functions with no by-ref parameter slots (void, or not virtualized) are
skipped silently.  The script is a no-op if everything is already
unsigned long long.
"""
import re
import sys


def fail(msg):
    print(f"PATCH-FAIL: {msg}", file=sys.stderr)
    sys.exit(1)


def main():
    if len(sys.argv) != 4:
        fail("usage: patch_pointer_width.py <in.c> <out.c> <fn1[,fn2,...]>")
    in_path, out_path, fn_arg = sys.argv[1], sys.argv[2], sys.argv[3]
    functions = [f.strip() for f in fn_arg.split(",") if f.strip()]

    with open(in_path, "r", encoding="utf-8", errors="surrogateescape") as f:
        src = f.read()

    # Build map: var_name -> decl_match for every formalGlobalInitN declaration
    decl_re = re.compile(
        r"(?P<indent>[ \t]*)(?P<type>unsigned long(?: long)?) \*"
        r"(?P<var>formalGlobalInit\d+) ;\n"
    )
    all_decls = {m.group("var"): m for m in decl_re.finditer(src)}

    # Accumulate edits across all functions
    vars_to_patch = {}       # var_name -> decl_match
    cast_pairs_to_patch = [] # store-cast pair match objects
    reads_to_patch = []      # read-back match objects

    functions_found = []
    functions_skipped = []

    for fn in functions:
        locals_re = re.compile(
            r"_TIG_VZ_([A-Za-z0-9]+)_1_" + re.escape(fn) + r"_\$locals\b"
        )
        tags = set(locals_re.findall(src))
        if not tags:
            functions_skipped.append(fn)
            continue
        if len(tags) > 1:
            fail(
                f"found {len(tags)} distinct _TIG_VZ_<tag>_1_{fn}_$locals "
                f"identifiers ({sorted(tags)}) - expected exactly 1. "
                "Refusing to guess which to patch."
            )
        tag = next(iter(tags))
        locals_name = f"_TIG_VZ_{tag}_1_{fn}_$locals"
        pc_name    = f"_TIG_VZ_{tag}_1_{fn}_$pc"
        functions_found.append((fn, locals_name, pc_name))

        # Find store-cast pairs for this function's $locals
        cast_re = re.compile(
            r"(?P<var>formalGlobalInit\d+) = \(unsigned long \*\)\(\(char \*\)\(\(void \*\)\("
            + re.escape(locals_name)
            + r"\)\) \+ (?P<offset>\d+)\);\n"
            r"[ \t]*\*(?P=var) = \(unsigned long \)\(& (?P<ident>\w+)\);\n"
        )
        pairs = list(cast_re.finditer(src))
        if not pairs:
            # Virtualized but no by-ref parameter slots - nothing to patch here
            continue

        # Validate offsets are distinct and 8-byte aligned
        offsets = [int(m.group("offset")) for m in pairs]
        if len(set(offsets)) != len(offsets):
            fail(f"{fn}: store offsets are not distinct: {offsets}. Not patching.")
        for off in offsets:
            if off % 8 != 0:
                fail(f"{fn}: store offset {off} is not 8-byte aligned. Not patching.")

        # Check for overlap with the operand-stack node-array region
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
                    f"{fn}: parameter slots end at byte {highest_slot_end} but "
                    f"node-array region starts at {region_start} - OVERLAP. "
                    "Manual review required."
                )
        else:
            print(
                f"PATCH-WARN: {fn}: could not locate node-array region start "
                "(non-fatal; proceeding since store/read shape matched).",
                file=sys.stderr,
            )

        for m in pairs:
            var = m.group("var")
            if var not in all_decls:
                fail(
                    f"{fn}: store-cast pair references '{var}' but no "
                    f"'unsigned long [long] *{var} ;' declaration found."
                )
            if "unsigned long long" in m.group(0):
                continue  # already patched
            cast_pairs_to_patch.append(m)
            decl = all_decls[var]
            if decl.group("type") == "unsigned long":
                vars_to_patch[var] = decl

        # Read-back site(s) - at most 1 expected per function with store-cast
        # pairs.  Functions with no formal parameters (e.g. main(void)) produce
        # store-cast pairs to stash local variable addresses but emit 0
        # read-back sites, so 0 is valid; > 1 is unexpected.
        read_re = re.compile(
            r"\(char \*\)\*\(\(unsigned long \*\)\("
            + re.escape(locals_name)
            + r" \+ \*\(\(int \*\)"
            + re.escape(pc_name)
            + r"\[0\]\)\)\);\n"
        )
        reads = list(read_re.finditer(src))
        if len(reads) > 1:
            fail(
                f"{fn}: expected at most 1 read-back site, found {len(reads)}. "
                "Not patching."
            )
        for r in reads:
            if "unsigned long long" not in r.group(0):
                reads_to_patch.append(r)

    if not functions_found:
        fail(
            "none of the specified functions had _TIG_VZ_*_1_fn_$locals "
            "symbols in the generated file. Was --Functions= correct?"
        )

    if not vars_to_patch and not cast_pairs_to_patch and not reads_to_patch:
        print(
            "PATCH-SKIP: all formalGlobalInit* sites are already "
            "'unsigned long long'. Copying input unchanged."
        )
        with open(out_path, "w", encoding="utf-8", errors="surrogateescape") as f:
            f.write(src)
        return

    # Build edit list and apply back-to-front so earlier offsets stay valid
    edits = []

    for decl in vars_to_patch.values():
        edits.append((
            decl.start(), decl.end(),
            decl.group(0).replace("unsigned long *", "unsigned long long *", 1),
        ))

    for m in cast_pairs_to_patch:
        new_text = (
            m.group(0)
            .replace("unsigned long *", "unsigned long long *", 1)
            .replace("unsigned long )(& ", "unsigned long long )(& ", 1)
        )
        edits.append((m.start(), m.end(), new_text))

    for m in reads_to_patch:
        new_text = m.group(0).replace(
            "unsigned long *", "unsigned long long *", 1
        )
        edits.append((m.start(), m.end(), new_text))

    edits.sort(key=lambda e: e[0], reverse=True)
    out = src
    for start, end, text in edits:
        out = out[:start] + text + out[end:]

    with open(out_path, "w", encoding="utf-8", errors="surrogateescape") as f:
        f.write(out)

    fn_names = [f for f, _, _ in functions_found]
    print(
        f"PATCH-OK: {len(vars_to_patch)} declaration(s), "
        f"{len(cast_pairs_to_patch)} store-cast pair(s), "
        f"{len(reads_to_patch)} read-back site(s) rewritten to "
        f"'unsigned long long' across {len(fn_names)} function(s) "
        f"({', '.join(fn_names)}). "
        f"Skipped (not virtualized / no params): "
        f"{', '.join(functions_skipped) if functions_skipped else 'none'}."
    )


if __name__ == "__main__":
    main()
