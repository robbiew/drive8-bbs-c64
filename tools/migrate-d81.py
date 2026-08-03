#!/usr/bin/env python3
"""Convert a T/64 .d81 into the flat SEQ folder tree the SoftIEC build reads.

Non-destructive: reads the image, writes a new directory. The source .d81 is
never modified, so a failed run loses nothing.
"""
import argparse
import os
import subprocess
import sys

MAX_PATH = 23   # cfg_t.init_* is char[24] in T64_STORE_SEQ builds

# name -> (section, record size). Record sizes are include/bbs/records.h.
# Sections verified against the real rel_open() call sites, not assumed:
#   USR LOG/USR PROF (users.c), USR.DAY (usrday.c), VOTE1 (votes.c)
#     -> bbs_cfg.drive_system                                    -> SYSTEM
#   USR.PTR (usrptr.c), BOARDS (boards.c)
#     -> bbs_cfg.drive_msgs                                      -> MSGS
#   UDS (file_areas.c)         -> bbs_cfg.drive_files            -> FILES
#   DOORS (doors.c)            -> bbs_cfg.drive_doors            -> DOORS
RECORD_SETS = {
    "USR LOG":  ("SYSTEM", 30),
    "USR PROF": ("SYSTEM", 86),
    "USR.PTR":  ("MSGS",   40),
    "USR.DAY":  ("SYSTEM",  8),
    "BOARDS":   ("MSGS",   44),
    "UDS":      ("FILES",  40),
    "VOTE1":    ("SYSTEM", 40),
    "DOORS":    ("DOORS",  40),
}
SECTIONS = ("SYSTEM", "MSGS", "FILES", "DOORS")


def trim_records(data, record_size):
    """Drop trailing all-zero records; pad a ragged tail to a whole record."""
    if record_size == 0:
        return b""
    if len(data) % record_size:
        data = data + bytes(record_size - (len(data) % record_size))
    last = 0
    for i in range(0, len(data), record_size):
        if any(data[i:i + record_size]):
            last = i + record_size
    return data[:last]


def device_spec(device, base, section):
    path = base.rstrip("/") + "/" + section
    if len(path) > MAX_PATH:
        raise ValueError(
            f"folder path {path!r} is {len(path)} chars; the SoftIEC build "
            f"holds {MAX_PATH}. Use a shorter --base."
        )
    return f"{device};{path}"


def extract(c1541, image, name, dest):
    """Pull one file out of the image. Returns False if it is not present.

    MEASURED: c1541 -read matches on the same lower/uppercase-PETSCII
    convention -write uses (see assemble-d81.sh's cbm_name() comment) but
    inverted — the file renders as this project's ALL-CAPS name on disk, yet
    -read must be asked for the lowercase spelling, and a bare name with no
    type suffix returns a plain FILE NOT FOUND for a REL file even when it
    is present. Both the lowercasing and the explicit ",l" are required;
    dropping either reproduces the false-negative silently (verified
    against a real seeded image, where every RECORD_SETS entry came back
    "missing" until both were added).
    """
    cbm_name = name.lower() + ",l"
    r = subprocess.run([c1541, image, "-read", cbm_name, dest],
                       capture_output=True, text=True)
    return r.returncode == 0 and os.path.exists(dest)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("outdir")
    ap.add_argument("--base", default="/USB1/TURBO64",
                    help="absolute SoftIEC path the tree will live at")
    ap.add_argument("--device", type=int, default=11)
    # tools/assemble-d81.sh resolves the same tool via a bare "c1541" on
    # PATH (overridable with $C1541); mirror that instead of a hardcoded
    # vendor/ path, since this repo does not vendor VICE.
    ap.add_argument("--c1541", default=os.environ.get("C1541", "c1541"),
                    help="path to the VICE c1541 tool")
    args = ap.parse_args()

    if os.path.exists(args.outdir) and os.listdir(args.outdir):
        sys.exit(f"{args.outdir} exists and is not empty; refusing to overwrite")

    for section in SECTIONS:
        os.makedirs(os.path.join(args.outdir, section), exist_ok=True)

    # Fail before doing any work if the paths will not fit.
    specs = {s: device_spec(args.device, args.base, s) for s in SECTIONS}

    tmp = os.path.join(args.outdir, ".extract")
    converted, missing = [], []
    for name, (section, size) in RECORD_SETS.items():
        if not extract(args.c1541, args.image, name, tmp):
            missing.append(name)
            continue
        with open(tmp, "rb") as f:
            data = f.read()
        out = trim_records(data, size)
        with open(os.path.join(args.outdir, section, name), "wb") as f:
            f.write(out)
        converted.append(f"{name}: {len(data)} -> {len(out)} bytes "
                         f"({len(out) // size} records)")
    if os.path.exists(tmp):
        os.remove(tmp)

    # The marker goes in ALL FOUR section folders, not just SYSTEM. Task 5's
    # arrival check opens it per section to detect the measured trap where CD:
    # to a wrong folder returns DOS status 0 and silently does not move.
    for section in SECTIONS:
        with open(os.path.join(args.outdir, section, "T64.SIEC"), "w") as f:
            f.write("T64SEQ1\r")

    # CONFIG lives at the ROOT of the tree, NOT in SYSTEM/. Measured on
    # hardware: cfg_init() reads it before any section path is registered, so
    # no CD: has happened yet and the read lands in the SoftIEC default path.
    # Putting it in SYSTEM/ means the BBS never finds it and silently falls
    # back to compile-time defaults — it boots fine and reads the wrong device.
    with open(os.path.join(args.outdir, "CONFIG"), "w") as f:
        f.write(f"DEV_SYSTEM={specs['SYSTEM']}\r")
        f.write(f"DEV_MSGS={specs['MSGS']}\r")
        f.write(f"DEV_FILES={specs['FILES']}\r")
        f.write(f"DEV_DOORS={specs['DOORS']}\r")
        f.write(f"DEV_GFILES={specs['SYSTEM']}\r")

    for line in converted:
        print(line)
    if missing:
        print(f"not present in the image (skipped): {', '.join(missing)}")
    print(f"\nwrote {args.outdir}; copy its contents to {args.base} on the stick")
    print("the source image was not modified")


if __name__ == "__main__":
    main()
