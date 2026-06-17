# TURBO/64 Door Authoring Guide

A T/64 door is a native 6502 PRG that the BBS loads at $9700 and enters via
`JSR $9700`.  The dev kit handles all startup plumbing; you write one function.

---

## Quick start

```c
/* mydoor.c — one file: include door_crt.h, then write door_main. */
#include "door_crt.h"

void door_main(void) {
    const bbs_api_t *b = bbs();
    bbs_caller_t me;
    if (!b) return;          /* not running inside a T/64 BBS — bail */
    b->get_caller(&me);
    b->clear_screen();
    b->print("HELLO FROM MY DOOR\r\n");
    b->getkey();
}
```

Build:

```bash
make door DOOR=mydoor
# Output: build/c64/MYDOOR.prg  (load address $9700)
```

---

## Build command

```
make door DOOR=<name>                     # source: devkit/examples/<name>.c
make door DOOR=<name> SRC=path/to/foo.c  # source: any path
```

Flags: `-n -O2` (native; bytecode mode + the $9700 overlay region crashes oscar64).
The $9700 load address comes from `#pragma overlay/region` in `door_crt.h`, not a
CLI flag.  Oscar64 emits the door overlay as `DOOR.prg`; the Makefile renames it
to `<NAME>.prg`.

---

## API surface (`bbs_api_t`)

All functions are called through the pointer returned by `bbs()`:

| Function | Signature | Description |
|---|---|---|
| `print` | `void (*)(const char *cp437)` | Send CP437 text to caller |
| `print_n` | `void (*)(const char *buf, u8 len)` | Send `len` bytes of CP437 |
| `display_file` | `void (*)(u8 cat, const char *name)` | Display a gfile |
| `clear_screen` | `void (*)(void)` | Clear caller's screen |
| `getkey` | `u8 (*)(void)` | Read one keypress; returns 0 on carrier drop |
| `read_line` | `i8 (*)(char *buf, u8 max)` | Read a line; see below |
| `get_caller` | `void (*)(bbs_caller_t *out)` | Fill caller info struct |

### `bbs_caller_t` fields

| Field | Type | Notes |
|---|---|---|
| `handle` | `char[16]` | Caller's login handle (NUL-terminated) |
| `access_level` | `u8` | Access level (0-5: 0=deleted, 1=new, 2=user, 3=power, 4=co-sysop, 5=sysop) |
| `firstname` | `char[16]` | |
| `lastname` | `char[16]` | |
| `location` | `char[21]` | |
| `term_width` | `u8` | Terminal width in columns |
| `term_mode` | `u8` | Terminal mode (PETSCII / ANSI / ASCII) |

---

## Cooperative-blocking rule

**Block ONLY via `getkey` or `read_line`.  Treat their abort sentinels as "exit now."**

- `getkey` returns `0` if the carrier drops mid-wait.  Check and return immediately.
- `read_line` returns `-1` if carrier is absent at entry.
- `read_line` returns `0` if the carrier drops in the middle of the read. (The
  underlying line reader bails out leaving a partial buffer; the wrapper detects
  the dropped carrier, clears the buffer, and returns `0`.)

Treat both `0` and `-1` from `read_line` as "carrier lost, exit now."  Note a
genuine empty line (the caller just pressed RETURN with no text) also returns `0`,
and the wrapper does not distinguish it from a carrier drop — so a door that must
accept empty input cannot rely on `read_line` alone.

```c
/* Safe getkey pattern */
if (b->getkey() == 0) return;

/* Safe read_line pattern */
char buf[81];
i8 n = b->read_line(buf, 80);
if (n <= 0) return;   /* 0 = carrier lost mid-read; -1 = carrier absent */
```

---

## Size budget

The door region is `$9700-$C000` — approximately **10 KB**.  `door_crt.h` uses
roughly 50 bytes for the header, bcexec, door_entry, and scratch.  The rest is
yours.  If your door grows beyond the budget, oscar64 will error at link time.

---

## Runtime constraint — no sub-$9700 calls

The door is a self-contained native image.  All indirect function calls go through
`door_bcexec` (an in-region clone of oscar64's `bcexec` trampoline), so the door
makes no jumps below $9700.

**However**: certain C operators emit direct calls to oscar64 runtime helpers that
live in the stub (below $9700).  Known offenders:

| Operator | Runtime | Workaround |
|---|---|---|
| `%` (modulo) | `divmod` | Use if/else or bit masking |
| `/` (division) | `divmod` | Same |
| 32-bit arithmetic | various | Avoid `u32`/`i32` in doors |

Verification command (check the door ASM for sub-$9700 calls):

```bash
grep -E "01:[0-9a-f]+ :.*JSR \$[0-8]" build/c64/_door_stub.asm
# Should produce no output for a well-formed door
```

To keep the door asm files for inspection: build manually then inspect before the
Makefile cleanup removes the `_door_stub.*` files.

---

## ABI versioning — append-only

`BBS_ABI_VERSION` (currently 1) increments when new fields are added to
`bbs_api_t` or `bbs_caller_t`.  New fields are always appended; existing offsets
never change.  A door compiled against ABI 1 continues to work with a BBS
advertising ABI 1.  `bbs()` returns NULL if the DCB magic or version does not
match — always check the return value.

---

## Installing a door

1. Build: `make door DOOR=mydoor` → `build/c64/MYDOOR.prg`
2. Copy to the door device/drive (same device as the system drive by default):
   ```
   c1541 bbs.d81 -write build/c64/MYDOOR.prg mydoor
   ```
   Or use the deploy script: `bash tools/deploy-u64.sh --prg-only`
3. Run `CONFIGURE-<ver>.prg` on the C64, navigate to **DOOR PROGRAMS**, and add
   an entry:
   - **TITLE**: display name shown in the door menu
   - **FILENAME**: the CBM filename on disk (e.g. `mydoor`)
   - **DRIVE/DEVICE**: drive number and device number where the PRG lives
   - **CMD KEY**: single letter that selects this door in the menu
   - **MIN ACCESS**: minimum access level required
   - **FLAGS**: `L` to run at login, or leave blank for menu-only

---

## Door header layout (reference)

The first 6 bytes of the door PRG (payload, starting at $9700) must be:

```
$9700: 4C lo hi   ; JMP door_entry  — BBS enters via JSR $9700
$9703: 44 36      ; 'D','6'          — BBS_DOOR_MAGIC0/1
$9705: 01         ; BBS_ABI_VERSION  — checked before entry
```

`door_crt.h` produces this layout automatically.  `door_run` validates offsets
3-5 before calling `enter_door()`.

---

## Files

| File | Purpose |
|---|---|
| `door_crt.h` | **Include this in your door.** Pulls in the SDK + all `$9700` startup (header, bcexec, door_entry) and makes the door sections active so your code+data land in the image |
| `door_sdk.h` | API types (`bbs_api_t`/`bbs_caller_t`) + the `bbs()` accessor — included by `door_crt.h` |
| `examples/fortune.c` | Worked example (the door bundled on the release disk) |
