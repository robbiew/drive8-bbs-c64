#!/usr/bin/env python3
"""Convert a T/64 .d81 into the flat SEQ folder tree the SoftIEC build reads.

Non-destructive: reads the image, writes a new directory. The source .d81 is
never modified, so a failed run loses nothing.

Produces the layout verified on hardware:
    <outdir>/         CONFIG, ovl_boot.prg, BOOT-<ver>-SIEC.prg
    <outdir>/SYSTEM/  the other six overlays, USR LOG, USR PROF, ACCESS,
                      CALLERS, T64.SIEC, all gfiles/menus/prompts
    <outdir>/MSGS/    T64.SIEC (+ USR.PTR, BOARDS, B<n>.IDX, B<n>.TXT)
    <outdir>/FILES/   T64.SIEC (+ UDS, UD<n>)
    <outdir>/DOORS/   T64.SIEC (+ DOORS)
CONFIG and ovl_boot.prg MUST be at the root: main() loads OVL_BOOT and
cfg_init() reads CONFIG before any section path is registered, using
whatever directory the KERNAL cursor is already sitting in.
"""
import argparse
import os
import re
import shutil
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

# Per-board / per-area REL sets that only exist once boards/areas have been
# created (src/data/messages.c: "B%u.IDX"; src/data/file_entries.c: "UD%u").
# Record sizes: include/bbs/records.h RECORD_SIZE_MSG_IDX / RECORD_SIZE_FILE_ENTRY.
RECORD_SIZE_MSG_IDX = 63
RECORD_SIZE_FILE_ENTRY = 100
BOARD_IDX_RE = re.compile(r"^b(\d+)\.idx$")
BOARD_TXT_RE = re.compile(r"^b(\d+)\.txt$")   # message bodies (SEQ, not REL)
UD_AREA_RE = re.compile(r"^ud(\d+)$")

# SEQ files RECORD_SETS cannot see (it only knows REL sets). Canonical names
# are uppercase to match the literal strings the firmware SETNAMs
# (include/bbs/access.h ACCESS_FILE, include/bbs/callers.h CALLERS_FILE).
SEQ_FIXED = {
    "access":  ("ACCESS",  "SYSTEM"),
    "callers": ("CALLERS", "SYSTEM"),
}

# SEQ entries that are expected on a seeded image but deliberately not
# migrated (a different, non-SIEC format) — not to be confused with a
# genuinely unrecognized entry the SysOp should look at.
EXPECTED_SEQ_SKIP = {"config"}

# The SIEC binaries this tree cannot boot without. Root: ovl_boot.prg plus
# the BOOT-<ver>-SIEC.prg binary itself (main() loads OVL_BOOT before cfg_init
# runs, at the tree root). SYSTEM/: the other six overlays, loaded later once
# disk_select_partition() has positioned the cursor there.
ROOT_SIEC_ARTIFACTS = ["ovl_boot.prg"]   # + BOOT-<ver>-SIEC.prg, added by name
SYSTEM_SIEC_ARTIFACTS = [
    "ovl_wfc.prg", "ovl_msgs.prg", "ovl_doors.prg",
    "ovl_files.prg", "ovl_zmodem.prg", "ovl_auth.prg",
]


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


def classify_entry(name, kind):
    """Map one c1541 -list entry (name, "rel"/"seq") to (canonical_name,
    section, record_size_or_None). record_size is None for a byte-exact SEQ
    copy, an int for a REL set that must be trimmed to whole records.
    Returns None for anything this tool does not migrate (PRG binaries come
    from the build tree, not the image; unrecognised names are reported,
    never silently dropped).
    """
    lname = name.strip().lower()
    if kind == "rel":
        for canon, (section, size) in RECORD_SETS.items():
            if canon.lower() == lname:
                return (canon, section, size)
        m = BOARD_IDX_RE.match(lname)
        if m:
            return (f"B{int(m.group(1))}.IDX", "MSGS", RECORD_SIZE_MSG_IDX)
        m = UD_AREA_RE.match(lname)
        if m:
            return (f"UD{int(m.group(1))}", "FILES", RECORD_SIZE_FILE_ENTRY)
        return None
    if kind == "seq":
        if lname in SEQ_FIXED:
            canon, section = SEQ_FIXED[lname]
            return (canon, section, None)
        if lname in EXPECTED_SEQ_SKIP:
            return None   # legacy REL-build data file; not the SIEC CONFIG
        m = BOARD_TXT_RE.match(lname)
        if m:
            return (f"B{int(m.group(1))}.TXT", "MSGS", None)
        if lname[:2] in ("g.", "m.", "p."):
            # gfiles/menus/prompts: session_display_file() builds the SETNAM
            # from a lowercase prefix + lowercase base, so the on-disk
            # spelling (already lowercase in data/gfiles/) is canonical.
            return (name.strip(), "SYSTEM", None)
        return None
    return None


def list_image(c1541, image):
    """Return [(name, type), ...] for every directory entry in the image."""
    r = subprocess.run([c1541, image, "-list"], capture_output=True, text=True)
    entries = []
    for line in r.stdout.splitlines():
        m = re.match(r'^\s*\d+\s+"([^"]*)"\s+(\S+)', line)
        if m:
            entries.append((m.group(1).rstrip(), m.group(2).lower()))
    return entries


def extract(c1541, image, name, dest, suffix):
    """Pull one file out of the image. Returns False if it is not present.

    MEASURED: c1541 -read matches on the same lower/uppercase-PETSCII
    convention -write uses (see assemble-d81.sh's cbm_name() comment) but
    inverted — the file renders as this project's ALL-CAPS name on disk, yet
    -read must be asked for the lowercase spelling, and a bare name with no
    type suffix returns a plain FILE NOT FOUND even when the file is present.
    Both the lowercasing and the explicit type suffix are required, and the
    suffix itself differs by file type: ",l" for REL, ",s" for SEQ — using
    the REL suffix (or none) on a SEQ file reproduces the same silent
    false-negative (verified against a real seeded image: ACCESS/CALLERS
    came back "missing" until ",s" was added).
    """
    cbm_name = name.lower() + f",{suffix}"
    r = subprocess.run([c1541, image, "-read", cbm_name, dest],
                       capture_output=True, text=True)
    return r.returncode == 0 and os.path.exists(dest)


def read_version(version_header):
    with open(version_header) as f:
        content = f.read()
    m = re.search(r'BBS_RELEASE_VERSION_COMPACT\s+"([^"]+)"', content)
    if not m:
        sys.exit(f"could not find BBS_RELEASE_VERSION_COMPACT in {version_header}")
    return m.group(1)


def find_missing_siec_artifacts(siec_dir, version):
    """Names (relative to siec_dir) of any required SIEC binary not present."""
    boot_name = f"BOOT-{version}-SIEC.prg"
    wanted = ROOT_SIEC_ARTIFACTS + [boot_name] + SYSTEM_SIEC_ARTIFACTS
    return [n for n in wanted if not os.path.isfile(os.path.join(siec_dir, n))]


def copy_siec_artifacts(siec_dir, version, outdir):
    """Copy whichever of the required SIEC binaries are present in siec_dir
    into outdir (root vs SYSTEM/ per the verified layout). Caller is
    responsible for the fail-loud/allow-incomplete decision beforehand.
    """
    boot_name = f"BOOT-{version}-SIEC.prg"
    copied = []
    for n in ROOT_SIEC_ARTIFACTS + [boot_name]:
        src = os.path.join(siec_dir, n)
        if os.path.isfile(src):
            shutil.copy2(src, os.path.join(outdir, n))
            copied.append(n)
    for n in SYSTEM_SIEC_ARTIFACTS:
        src = os.path.join(siec_dir, n)
        if os.path.isfile(src):
            shutil.copy2(src, os.path.join(outdir, "SYSTEM", n))
            copied.append(n)
    return copied


def write_config(outdir, specs):
    """Write CONFIG at the tree ROOT (never SYSTEM/ — see module docstring)."""
    with open(os.path.join(outdir, "CONFIG"), "w") as f:
        f.write(f"DEV_SYSTEM={specs['SYSTEM']}\r")
        f.write(f"DEV_MSGS={specs['MSGS']}\r")
        f.write(f"DEV_FILES={specs['FILES']}\r")
        f.write(f"DEV_DOORS={specs['DOORS']}\r")
        f.write(f"DEV_GFILES={specs['SYSTEM']}\r")


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

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
    ap.add_argument("--siec-build-dir",
                    default=os.path.join(root, "build", "c64", "siec"),
                    help="where ovl_*.prg / BOOT-<ver>-SIEC.prg were built "
                         "(default: build/c64/siec, i.e. 'make c64-siec')")
    ap.add_argument("--version-header",
                    default=os.path.join(root, "include", "bbs", "version.h"))
    ap.add_argument("--allow-incomplete", action="store_true",
                    help="produce the tree even if SIEC binaries are missing "
                         "(the result will NOT boot; for inspecting the data "
                         "layout only)")
    args = ap.parse_args()

    if os.path.exists(args.outdir) and os.listdir(args.outdir):
        sys.exit(f"{args.outdir} exists and is not empty; refusing to overwrite")

    # Fail before doing any work if the paths will not fit.
    specs = {s: device_spec(args.device, args.base, s) for s in SECTIONS}

    # Fail loudly, before touching the image or creating any output, if the
    # tree we are about to build cannot boot. A tree that "looks complete"
    # (data migrated, folders present) but is missing OVL_BOOT or the BOOT
    # binary fails silently at the C64 — the exact trap this tool exists to
    # remove. --allow-incomplete is an explicit opt-out for inspecting the
    # data layout only; it is not a way to produce a deployable tree.
    version = read_version(args.version_header)
    missing_bin = find_missing_siec_artifacts(args.siec_build_dir, version)
    if missing_bin and not args.allow_incomplete:
        sys.exit(
            "missing SIEC build artifact(s) in {}: {}\n"
            "This tree would not boot on hardware. Build them first:\n"
            "    make c64-siec\n"
            "(pass --allow-incomplete to write the tree anyway, for "
            "inspecting the data layout only)".format(
                args.siec_build_dir, ", ".join(missing_bin))
        )

    for section in SECTIONS:
        os.makedirs(os.path.join(args.outdir, section), exist_ok=True)

    entries = list_image(args.c1541, args.image)

    converted, data_missing, unrecognized = [], [], []
    tmp = os.path.join(args.outdir, ".extract")
    for name, kind in entries:
        if kind not in ("rel", "seq"):
            continue   # PRG binaries etc. come from --siec-build-dir, not the image
        job = classify_entry(name, kind)
        if job is None:
            if kind == "seq" and name.strip().lower() in EXPECTED_SEQ_SKIP:
                continue   # expected, not the SIEC CONFIG format; no warning
            unrecognized.append(f"{name} ({kind})")
            continue
        canon, section, size = job
        suffix = "l" if kind == "rel" else "s"
        if not extract(args.c1541, args.image, name, tmp, suffix):
            data_missing.append(canon)
            continue
        with open(tmp, "rb") as f:
            data = f.read()
        out = trim_records(data, size) if size else data
        with open(os.path.join(args.outdir, section, canon), "wb") as f:
            f.write(out)
        if size:
            converted.append(f"{canon}: {len(data)} -> {len(out)} bytes "
                             f"({len(out) // size} records)")
        else:
            converted.append(f"{canon}: {len(out)} bytes")
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
    write_config(args.outdir, specs)

    copied = copy_siec_artifacts(args.siec_build_dir, version, args.outdir)

    for line in converted:
        print(line)
    if data_missing:
        print(f"data not present in the image (skipped, not fatal): "
              f"{', '.join(data_missing)}")
    if unrecognized:
        print(f"NOT migrated (unrecognized on-disk entries, check manually): "
              f"{', '.join(unrecognized)}")
    print(f"\ncopied {len(copied)} SIEC binaries from {args.siec_build_dir}: "
          f"{', '.join(copied)}")
    if missing_bin:
        print(f"WARNING: tree is INCOMPLETE and will not boot — missing: "
              f"{', '.join(missing_bin)}")
    print(f"\nwrote {args.outdir}; copy its contents to {args.base} on the stick")
    print("the source image was not modified")


if __name__ == "__main__":
    main()
