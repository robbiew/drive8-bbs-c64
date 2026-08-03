"""Unit tests for the .d81 -> SoftIEC SEQ migration."""
import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))

import importlib
mig = importlib.import_module("migrate-d81")

fails = 0

def check(name, got, want):
    global fails
    if got != want:
        print(f"FAIL {name}: got {got!r} want {want!r}")
        fails += 1

# A REL file is preallocated to its maximum; the SEQ file must stop at the
# last record with any non-zero byte.
check("trim.empty",  mig.trim_records(bytes(90), 30), b"")
check("trim.one",    mig.trim_records(b"\x01" + bytes(89), 30), b"\x01" + bytes(29))
check("trim.sparse",
      mig.trim_records(bytes(30) + b"\x05" + bytes(29) + bytes(30), 30),
      bytes(30) + b"\x05" + bytes(29))
check("trim.full",   mig.trim_records(b"\x01" * 60, 30), b"\x01" * 60)

# A partial trailing record is padded out, never truncated mid-record.
check("trim.ragged", mig.trim_records(b"\x01" * 45, 30), b"\x01" * 45 + bytes(15))

check("trim.zero_size", mig.trim_records(b"\x01" * 30, 0), b"")

check("spec.system", mig.device_spec(11, "/USB1/TURBO64", "SYSTEM"),
      "11;/USB1/TURBO64/SYSTEM")
check("spec.trailing_slash", mig.device_spec(11, "/USB1/TURBO64/", "MSGS"),
      "11;/USB1/TURBO64/MSGS")

# 23 chars is what cfg_t holds; anything longer is a migration-time error,
# not a silent truncation the SysOp discovers at boot.
try:
    mig.device_spec(11, "/USB1/AAAAAAAAAAAAAAAAAAAA", "SYSTEM")
    check("spec.toolong", "no raise", "ValueError")
except ValueError:
    pass

print(f"migrate_d81: {fails} failed")
sys.exit(1 if fails else 0)
