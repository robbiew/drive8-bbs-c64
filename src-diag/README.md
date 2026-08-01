# Storage diagnostics

Standalone C64 programs that exercise **the real T/64 HAL** against a device, for
questions only hardware can answer. Build with `make diag`; the PRGs land in
`build/c64/`. Each asks for a device number (8–11) and prints results on screen.

| Program | Answers |
|---|---|
| `PTEST` | Does `CP<n>` + `0:` reach the right partition, and are partitions isolated? |
| `RELTEST` | Can this device create, position and read a REL file? |
| `CPTEST` | What status code does `CP<n>` actually return, per device? |
| `DIR` | What is on a given device/partition? (non-destructive) |

They link `src/hal/disk.c` and `src/hal/rel.c` directly, so a pass here is evidence
about the code the BBS actually runs — not a reimplementation that might diverge.

## Measured results (2026-08-01)

Commodore 64 Ultimate, firmware 1.1.0. Every result paired with a control run against
a known-good device.

| | Ultimate emulated 1581 (dev 8) | SoftIEC (dev 11) | uIEC/sd2iec (dev 10) |
|---|---|---|---|
| REL files | works | **fails** (DOS 61) | works |
| `CP<n>` partition select | **rejected** | n/a | accepted |
| Partition isolation | n/a | n/a | **verified** |
| SEQ files | works | works | works |

Two consequences worth remembering:

- The Ultimate's **emulated 1581 does not implement `CP`**, so partitions are unusable
  there. Real 1581 hardware and sd2iec both support it.
- **SoftIEC has no REL support**, which is why the record database cannot live there
  without the REU/SEQ work (Phase D). sd2iec *does* support REL, so T/64 runs on
  sd2iec hardware today.

## Two traps these programs exist to avoid

Both were hit for real while writing them, and both produced confident wrong answers
until a control run exposed them.

**1. Never read the status channel while a REL file is open.** `disk_status()` closes
and reopens logical file 15 — the command channel `rel_position()` seeks through.
Calling it between `rel_open()` and `rel_close()` makes a perfectly good 1581 report
"REL BROKEN". Collect results, print after closing. Same defect exists in `store_cd()`'s
history and is documented in `src/hal/rel.c`.

**2. `disk_open()` returning `BBS_OK` does not mean the file exists.** KERNAL OPEN on
the IEC bus succeeds on any device that answers, whatever DOS thought of the request.
Only the DOS status code tells the truth — 62 is FILE NOT FOUND. Any test that keys on
the open's return value rather than the status code is measuring nothing.

## Hazard

**Pointing these at a device number that is not present will hang the C64** and require
a reset. Reading the status channel of an absent device never returns. A
`KRNIO_NODEVICE` guard was tried and does **not** prevent this — it was measured
ineffective on both JiffyDOS and stock kernals. Confirm the device number first.

Separately, on the test machine a **JiffyDOS kernal hung on a uIEC at device 10** with
the card both inserted and removed; the stock kernal worked. Cause not established.
If a device seems absent, try a stock kernal before concluding anything.
