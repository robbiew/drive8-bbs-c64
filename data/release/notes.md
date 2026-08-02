This is an early pre-release for testing and feedback. It is incomplete and
file transfers in particular need more real-world testing before they can be
considered stable. Expect bugs — please report them on GitHub.

  https://github.com/robbiew/turbo64/issues

v__VERSION__ is a bug-fix release. It fixes nine defects present in v0.3.0 —
one that made the clock run fast on every NTSC machine, and one that could
lose data silently.


Fixed in v__VERSION__
---------------------

**The clock ran 20% fast on 60 Hz machines.** The CIA has to be told what
frequency its time-of-day pin is fed at, and the BBS always claimed 50 Hz.
On a machine fed 60 Hz — which includes any NTSC Commodore 64 Ultimate — the
clock gained a fifth: about 12 minutes an hour, half an hour over 2.5 hours.

That rate cannot be worked out from the video standard, so the BBS now
measures it: at boot it times the clock against a hardware timer for about
half a second and configures itself from the result. The boot screen reports
what it found, as `TOD CLOCK: 50HZ` or `60HZ`. If your clock has been
drifting, this was why, and there is nothing to configure.

Measured after the fix on a Commodore 64 Ultimate: 420.4 seconds elapsed in
420.4 seconds of real time. Before it, that same interval read 504.

**The date on the WFC screen could gain a digit,** showing 07/30/266. The
header row is redrawn without clearing, and it is one column narrower when
the hour has one digit, so at the 12:59 -> 1:00 rollover the last digit of
the year was left stranded on screen. Only the display was affected — the
stored date was always correct, so nothing written to disk was wrong.

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

**Partitions were ignored when creating and saving data.** CONFIGURE's INIT
built the user database on whatever partition the drive happened to be on
rather than on SYSDEV's, and saving DEVICES paired the previous device with
the newly-entered partition — which on a Commodore 64 Ultimate meant sending
CP to its emulated 1581, which rejects the command, and reporting
"SAVE FAILED".

**The BBS could only find its config on the device it was compiled for.**
CONFIG was read from the compile-time default device (normally 8) rather
than from the device the BBS was loaded from, so a BBS run from any other
device read a different config and then failed to find its own USR LOG. It
now reads CONFIG from the boot device, which is what makes running the whole
BBS from a single sd2iec card work.

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

Copying files onto an sd2iec device: copy them **from the C64** with a file-copy
utility rather than from a PC. Files copied onto the card from a PC keep their
FAT extension as part of the name, so the C64 sees "BOOT-0.3.1.PRG" and
LOAD"BOOT-0.3.1" fails. Files created through the C64 get proper CBM names.
(Alternatively enable extension hiding on the drive with XE+ then XW.) The
BBS's own data files are never affected — CONFIGURE creates them through the
C64, so they are always named correctly.

If storage behaves unexpectedly, the source tree carries a set of standalone
diagnostics that run the same disk code the BBS does — PTEST, RELTEST,
CPTEST, DIR, EXISTS, CLEAN, WIPE and COPYALL. They are not included in this
archive, since two of them delete files; build them with `make diag` and
start with PTEST. Note that pointing any of them at a device that is not
present will hang the C64 and require a reset — that is KERNAL serial-bus
behaviour, not a fault in the tools.

See the README for full details.


Running the BBS entirely from one device
---------------------------------------
Verified this release: BOOT, CONFIGURE, the overlays, menus, config, user
database and message base all on a single uIEC/SD card, with system files on
partition 1 and the message base on partition 2. No disk image mounted.

Two things to know if you try it:

- Copy the program files onto the card FROM THE C64, not from a PC. See the
  README. A PC-side copy leaves the FAT extension in the filename.
- CONFIGURE is larger than BASIC's ~39 KB program space, so after it exits
  BASIC reports OUT OF MEMORY on the next LOAD. Type NEW first - you do not
  need to reboot.

Known gaps
----------
- Reading the current date and time automatically (as opposed to the tick
  rate above) still only works through an Ultimate's RTC on the cartridge
  port. It does not read the clock from an sd2iec or CMD drive even when one
  is fitted, so on a plain C64 you are prompted for the time at every boot.
- The new tick-rate detection was confirmed on 60 Hz hardware. The 50 Hz
  result has only been reproduced under emulation — if you run a PAL machine,
  the boot screen should say `TOD CLOCK: 50HZ`, and it is worth telling us
  if it does not.
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
