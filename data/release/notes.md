This is an early pre-release for testing and feedback. It is incomplete and
file transfers in particular need more real-world testing before they can be
considered stable. Expect bugs — please report them on GitHub.

  https://github.com/robbiew/turbo64/issues

v__VERSION__ is a bug-fix release. It fixes five defects present in v0.3.0,
one of which could lose data silently.


Fixed in v__VERSION__
---------------------

**Partitions did not work, and failed silently.** The `8;1` style field in
CONFIG > DEVICES is documented as device;PARTITION, but the value was being
sent as a CBM *drive* number in the filename ("2:BOARDS"). A 1581 has only
drive 0, so the open failed — and because nothing checked the DOS error
channel, it failed with no message. Setting any device to a non-zero
partition therefore broke EVERY disk operation on that device, not just the
obvious one. The usual symptom was "NO BOARDS."

Partitions are now selected the documented way: a `CP<n>` command on the
command channel, with filenames using the drive-0 form. **Partition 0 sends
no command at all**, so existing setups are unaffected.

If you hit this: no data was lost. Nothing could open it.

**Door loading failed silently on a non-zero partition** — same root cause,
in the door loader.

**A file area with all-empty record slots could hang the file listing.** The
loop counters were `u8` compared against 255, which is always true, so
termination relied on a break the empty-slot path skipped.

**Zmodem upload could not overwrite an existing file.** The save-and-replace
prefix was missing, so re-uploading returned FILE EXISTS instead of replacing.

**A rejected REL file still reported success.** `rel_open()` only checked the
KERNAL, never the drive's status channel — so on a device that refused the
open, every subsequent record write also reported success while the data went
nowhere. This one could lose records with no warning.


Storage: what works on which device
-----------------------------------
Measured on hardware, each with a control run against a known-good drive:

  1541 / 1571 / 1581       REL yes,  partitions yes (real hardware)
  C64U emulated 1581       REL yes,  partitions NO — the emulation
                                     rejects CP; use partition 0
  sd2iec / uIEC / SD2IEC   REL yes,  partitions yes, isolation verified
  C64U SoftIEC             REL NO  — cannot host the record database

The C64 Ultimate's built-in emulated drives do not support partitions. Set
them to partition 0. Partitions need real hardware or an sd2iec-class device.

New in this release: `make diag` builds standalone storage diagnostics
(PTEST, RELTEST, CPTEST, DIR, EXISTS, CLEAN) that run the same disk code the
BBS does. Start with PTEST if storage behaves unexpectedly. Note that
pointing them at a device that is not present will hang the C64 and require
a reset — that is KERNAL serial-bus behaviour, not a fault in the tools.

See the README for full details.


Known gaps
----------
- The clock auto-detect only reads an Ultimate's RTC through the cartridge
  port. It does not read the clock from an sd2iec or CMD drive even when one
  is fitted, so on a plain C64 you are prompted for the time at every boot.
- Free space is not reported for devices that give no block count.


What works in v__VERSION__:

- Login and new-user registration
- Terminal auto-detection (PETSCII, ANSI/CP437, ASCII)
- Bulletin boards (read, post)
- Door programs (run external plug-ins; dev kit for authors)
- File transfers — upload and download via Punter and Zmodem (needs testing)
- SysOp Configure editor (users, boards, doors, file areas, config, access)
- 80-column SysOp spy mode (ANSI callers, REU required)
- SwiftLink/ACIA modem support, VICE tcpser bridge

What is NOT yet working:

- Private mail
- SysOp chat / page
- Polls and votes
- Several smaller features (see README.txt)

Installation
------------
1. Mount TURBO64-__VERSION__.d81 on device 8
2. LOAD "CONFIGURE-__VERSION__",8
3. RUN — choose I (INIT FILES) for first-time setup
4. LOAD "BOOT-__VERSION__",8
5. RUN

For full instructions see README.txt inside the archive.
