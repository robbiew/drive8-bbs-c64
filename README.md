# TURBO/64 BBS (T/64)

> **Note:** This project is under active development and is not yet feature-complete.

## What This Is: A Functional Anachronism

TURBO/64 BBS is a Commodore 64 BBS written in C for the Oscar64 compiler. It targets native `.prg` output for real hardware (including C64 Ultimate) and VICE emulation.

**Current status:** v0.1.0 — login/registration, terminal translation (PETSCII, ANSI/CP437, ASCII), bulletin boards (post/read/list), and the SysOp editor are working. Private mail, file transfers, SysOp chat, polls, and the callers log remain stubbed.

> **Not a developer?** Download `TURBO64-<ver>.d81` and `BOARDS-<ver>.d81` from the [latest GitHub release](../../releases/latest), copy them to your U64 storage, and jump straight to [First-Time Setup — U64, Step 2](#2-mount-and-boot-into-configure). You can skip Requirements and Building entirely.

---

## Requirements

**Hardware / emulator**
- Commodore 64 with SwiftLink/ACIA cartridge at $DE00, **or** C64 Ultimate (U64) with built-in ACIA, **or** VICE x64sc with tcpser modem bridge
- Two .D81 disk images (or real 1581/CMD/U64 storage) — one for the BBS, one for message boards (device 9)
- 16 MB REU required for the message boards. The C64 Ultimate has one built in, and VICE is auto-configured with one (`-reusize 16384`). The BBS boots and lets you log in without a REU, but reading and posting messages need it.

**Developer Build host (macOS / Linux)**
- `oscar64` compiler — `bash tools/install-oscar64.sh`
- `c1541` from VICE package (disk assembly)
- `c64u` deploy tool for U64 — `bash tools/install-c64u.sh`
- `tcpser` for VICE modem emulation — macOS: `brew install tcpser`

---

## Building

```bash
make all          # compile BOOT and CONFIGURE PRGs
make disk         # assemble TURBO64-<ver>.d81 from build output + data/
make disk-with-users  # same, but fetches live user database from U64 first
```

Output: `build/c64/BOOT-<ver>.prg`, `build/c64/CONFIGURE-<ver>.prg`, `build/c64/TURBO64-<ver>.d81`

See [`tools/README.md`](tools/README.md) for the full reference covering `build.sh`, `deploy-vice.sh`, `deploy-u64.sh`, `extract-users.sh`, `extract-boards.sh`, and the other development scripts.

---

## First-Time Setup — U64

### 1. Deploy the BBS disk

```bash
tools/deploy-u64.sh            # deploys TURBO64-<ver>.D81 to /BBS/
tools/deploy-u64.sh -l sd      # deploys to /SD/BBS/
```

### 2. Mount and boot into CONFIGURE

On the U64, mount `TURBO64-<ver>.D81` on device 8 and load CONFIGURE:

```
LOAD "CONFIGURE-0.1.0",8
RUN
```

### 3. Initialize the disk (first time only)

From the CONFIGURE main menu, choose **I — INIT FILES**.

Creates:
- `USR LOG` — user database (100 slots, 30 bytes/record)
- `USR PROF` — extended user profiles (100 slots, 86 bytes/record)
- `CALLERS` — callers log, seeded with one SYSOP entry so the WFC screen has something to display

Record 1 is always the SysOp account — handle `SYSOP`, password `PASS`, access level 5. Change it!


### 4. Configure the BBS

CONFIGURE → **C — CONFIG → 1 SETTINGS**:

| Key | Default | Notes |
|-----|---------|-------|
| `BBS_NAME` | `A NEW T/64 BBS` | BBS name shown at login |
| `BBS_CITY` | `SOMEWHERE, CA` | Location string |
| `SYSOP_NAME` | `JOE SYSOP` | Displayed on BBS |
| `NEW_USER_LEVEL` | `1` | Access level for new registrants (0–4) |
| `ALLOW_NEW_USERS` | `1` | Allow self-registration |
| `DEV_SYSTEM` | `8` | Device for boot PRG, config, and gfiles |
| `DEV_MSGS` | `9` | Device for message boards (separate drive recommended) |
| `DEV_FILES` | `8` | Device for upload/download areas |
| `DEV_DOORS` | `8` | Device for door programs |

CONFIGURE → **C → 2 BAUD RATE**: 300, 1200, 2400, 9600, 19200, or 38400.

Changes are saved to the `config` SEQ file immediately.

### 5. Set up the message boards disk (device 9)

The BBS reads message boards from a separate disk on `DEV_MSGS` (default device 9). Two options:

#### Option A — blank disk (start fresh)

Format a blank .D81 and mount it on U64 device 9. Then in CONFIGURE → **M — MSG AREAS → C — CREATE**, add boards. Each board needs a title, read level, and write level. The `BOARDS DIR` REL file and per-board index files are created automatically on first post.

#### Option B — example seed disk (3 pre-configured boards)

`data/boards-seed.d81` contains a snapshot of `BOARDS-<ver>.D81` with three ready-to-use boards:

```bash
tools/deploy-u64.sh --boards   # uploads BOARDS-<ver>.D81 to /BBS/
```

Then mount `BOARDS-<ver>.D81` on U64 device 9.

To refresh the seed from a running U64 (e.g. after adding boards via CONFIGURE):

```bash
tools/extract-boards.sh        # saves live BOARDS-<ver>.D81 → data/boards-seed.d81
```

> ⚠️ The message index format uses 48-byte REL records. 

### 6. Run the BBS

Outside CONFIGURE, on the C64:

```
LOAD "BOOT-0.1.0",8
RUN
```

Expected startup output:

```
TURBO/64 BBS V0.1.0

LOADING SETUP...
  BBS: <your bbs name>
  SYSOP: <your name>

INIT MODEM...
  ACIA STATUS: $10
  DSR: INACTIVE

CHECKING REU...
  REU: 16 MB

CHECKING USR LOG...
  USR LOG: OK

CHECKING FOR REAL TIME CLOCK...
```

If `USR LOG: NOT FOUND` or `USR LOG: EMPTY` appears, return to CONFIGURE and run Init Files.

---

## First-Time Setup — VICE

### 1. Build and assemble

```bash
make all && make disk
```

### 2. Launch VICE with modem bridge

```bash
bash tools/deploy-vice.sh
```

Starts `x64sc` with the disk image and launches `tcpser` as a virtual RS-232 modem bridge. VICE connects the emulated ACIA to tcpser on port 25232; tcpser listens for telnet on port 6400.

### 3. Initialize and configure

VICE autostarts BOOT. Press **RUN/STOP** to interrupt, then:

```
LOAD "CONFIGURE-0.1.0",8
RUN
```

Follow the same steps as U64: Init Files, then Config. For VICE testing a blank boards disk is usually sufficient — CONFIGURE → M — MSG AREAS to create a board, then mount a formatted .D81 on device 9 in VICE.

### 4. Connect as a caller

```bash
telnet localhost 6400
```

---

## Configuration Reference

`data/config` is the template bundled into every disk build:

```
BBS_NAME=A NEW T/64 BBS
BBS_CITY=SOMEWHERE, CA
SYSOP_NAME=JOE SYSOP
NEW_USER_LEVEL=1
MIN_CALL_TIME=5
MAX_CALL_TIME=60
DEV_SYSTEM=8
DEV_MSGS=9
DEV_FILES=8
DEV_DOORS=8
MODEM_INIT=ATZ
BAUD_RATE=38400
MODEM_TIMEOUT=255
ALLOW_NEW_USERS=1
ALLOW_UPLOADS=1
```

Device values can include a drive number and CBM DOS init string: `8`, `8;0:`, or `8;0:;CD:BBS`. The BBS sends the init string to the drive at startup.

---

## Access Levels

| Level | Name | Description |
|-------|------|-------------|
| 0 | Deleted | Banned / deleted account |
| 1 | New | Unvalidated new user (default for registrants) |
| 2 | User | Standard validated user |
| 3 | Power | Power user |
| 4 | Co-SysOp | Co-SysOp |
| 5 | SysOp | Full access; cannot be deleted |

The SysOp account (ID 1) is created by Init Files at level 5. Adjust `NEW_USER_LEVEL` to control what level new registrants receive.

### Per-level limits & flags

Each level also carries daily usage limits and capability flags, edited in **CONFIGURE → Access Levels**:

- **CALLS/DAY** — maximum logins allowed per day for the level.
- **MINS/DAY** — maximum online minutes per day. Enforced live: a time-left banner and a per-second countdown show during the session (see the `%TL` MCI code), and the caller is warned and disconnected when the limit is reached. Levels with the **T** flag are exempt.
- **FLAGS** — capability bits, shown in the editor as a row of letters (a `-` means that flag is off, e.g. `APSJU---`):

| Letter | Flag | Grants |
|--------|------|--------|
| `A` | POST_ANON | post messages anonymously |
| `P` | PAGE_SYSOP | page the SysOp for chat |
| `S` | SEND_MAIL | send private mail |
| `J` | JOIN_POLLS | vote in polls |
| `U` | UPLOAD | upload files |
| `T` | NO_TIME_LIMIT | exempt from MINS/DAY |
| `C` | NO_CALL_LIMIT | exempt from CALLS/DAY |
| `Y` | SYSOP | full SysOp / Co-SysOp access |

These rows live in the `ACCESS` SEQ file on the system disk, one comma-delimited line per level:

```
level,name,calls_per_day,mins_per_day,flags
```

`flags` is the decimal sum of the bit values (`A`=1, `P`=2, `S`=4, `J`=8, `U`=16, `T`=32, `C`=64, `Y`=128). Edit it through CONFIGURE rather than by hand. Overriding the `name` field there also changes the label shown for that level throughout the BBS.

---

## CONFIGURE Menu Reference

```
CONFIGURE MAIN MENU
  I — INIT FILES      Initialize USR LOG, USR PROF, and CALLERS
  U — USER MGMT       List, delete, reset passwords
  M — MSG AREAS       Create/edit/delete message boards; compact and prune
  F — FILE AREAS      Create/edit upload/download areas (stub)
  V — VOTE MGMT       Create/edit polls (stub)
  C — CONFIG          Edit BBS name, devices, baud rate
  S — STATISTICS      Show user/area counts
  Q — QUIT
```

---

## Message Boards

Manage boards in CONFIGURE → **M — MSG AREAS** (List, Create, Edit, Delete,
Maintenance).

**Creating a board.** Create asks only for a **title**, then drops you into the
board editor where you set everything else. (Cancelling the editor right after
creating discards the new board.)

**Per-board settings (board editor).** Press the reverse-video hotkey to change
a field:

| Key | Field | Meaning |
|-----|-------|---------|
| `T` | TITLE | Board name (max 16 chars) |
| `L` | LIST ORD | Display order in the BBS (lower sorts first; ties break by id) |
| `R` | READ | Min access level to read (0–5) |
| `W` | WRITE | Min access level to post (0–5) |
| `A` | ANON | Allow anonymous posts (Y/N) |
| `N` | NET | Networked area (Y/N); when on, set `G` NET TAG |
| `O`/`X` | SUBOP | Assign / clear a SubOp by handle |
| `M` | MAX MSG | Per-board message limit (`DEFAULT` = use the system default) |
| `D` | MAX DAYS | Age-prune limit in days (`OFF` = no age pruning) |

Press `S` to save, `Q`/`C` to cancel.

**Size and age limits.** Two independent limits keep a board from growing without
bound (compile-time defaults in `include/bbs/config.h`):

- **Message count.** A board's `MAX MSG` caps how many messages it keeps.
  `DEFAULT` (0) uses `CFG_MSG_LIMIT_DEFAULT` = **100**. When a board passes its
  limit, the oldest messages are auto-pruned (soft-deleted) in batches of
  `CFG_MSG_PRUNE_BATCH` = **10**. There is also a hard ceiling of
  `CFG_MSG_MAX_PER_BOARD` = **200** messages per board — posting beyond it fails.
- **Age.** `MAX DAYS` soft-deletes messages older than N days. The default is
  `CFG_MSG_AGE_DEFAULT` = **0 = OFF** (no age pruning) until a SysOp sets a day
  count on the board.

So out of the box a board keeps ~100 messages (auto-pruning the oldest above
that, hard stop at 200) with no age limit.

**Maintenance.** CONFIGURE → M → **MAINTENANCE** picks a board and shows its
total and soft-deleted message counts, with a waste estimate:

- `C` — **COMPACT**: reclaim space from soft-deleted messages (rewrites the
  board's index/text files; recommended when waste ≥ 25%).
- `P` — **PRUNE NOW**: immediately soft-delete the oldest messages over the
  board's limit (one batch).
- `Q` — **QUIT** back to the menu.

Soft-deleted messages still occupy slots until a COMPACT reclaims them.

---

## Disk File Layout

Files on `TURBO64-<ver>.d81` (device 8, system device):

| Filename | Type | Description |
|----------|------|-------------|
| `boot-<ver>` | PRG | BBS runtime |
| `configure-<ver>` | PRG | SysOp editor |
| `config` | SEQ | BBS configuration (key=value) |
| `usr log` | REL | User database (30 bytes/record, 100 slots) |
| `usr prof` | REL | Extended user profiles (86 bytes/record, 100 slots) |
| `callers` | SEQ | Callers log (fixed-width lines; created by Init Files) |
| `g.login` | SEQ | Login art — generic fallback |
| `g.login 0` | SEQ | Login art — PETSCII |
| `g.login 1` | SEQ | Login art — ANSI/CP437 |
| `g.login 2` | SEQ | Login art — ASCII |
| `g.newuser` | SEQ | New user screen — generic fallback |
| `g.newuser 0` | SEQ | New user screen — PETSCII |
| `g.term` | SEQ | Terminal selection menu |
| `m.main` | SEQ | Main menu body — generic fallback |
| `m.main 0` | SEQ | Main menu body — PETSCII |
| `p.main 0` | SEQ | Main menu prompt — PETSCII |

Files on `BOARDS-<ver>.d81` (device 9, message device):

| Filename | Type | Description |
|----------|------|-------------|
| `boards dir` | REL | Board directory (44 bytes/record, up to 20 boards) |
| `b<n>.idx` | REL | Message index for board N (48 bytes/record, up to 200 msgs) |
| `b<n>.txt` | SEQ | Message body text for board N |

### Gfile Naming Convention

Display files follow the pattern `<prefix>.<name> [<mode>] [<width>]`:

- **prefix** — `g` = gfile (display page), `m` = menu body, `p` = prompt
- **mode** — `0`=PETSCII, `1`=ANSI/CP437, `2`=ASCII, 
- **width** — `40` or `80`; omit to match any width

BOOT tries the most specific match first:
```
<prefix>.<name> <mode> <width>  →  <prefix>.<name> <mode>  →  <prefix>.<name> <width>  →  <prefix>.<name>
```

A `p.<name>` file replaces the built-in `COMMAND: ` prompt for that menu. If absent, the built-in text is used.

### Pipe Color Codes

Display files support inline color codes using `|XX` or `\XX` (two-digit decimal 00–15). Both forms are identical — `\` is provided as a C64-native alias because the standard C64 keyboard has no `|` key: the `£` key produces CHR$(92) = ASCII backslash. Codes are translated to the appropriate PETSCII or ANSI escape for each caller's terminal; ASCII callers receive no color output.

| Code | Color | Code | Color |
|------|-------|------|-------|
| `\|00` | Black | `\|08` | Orange |
| `\|01` | White | `\|09` | Brown |
| `\|02` | Red | `\|10` | Light Red |
| `\|03` | Cyan | `\|11` | Dark Gray |
| `\|04` | Purple | `\|12` | Medium Gray |
| `\|05` | Green | `\|13` | Light Green |
| `\|06` | Blue | `\|14` | Light Blue |
| `\|07` | Yellow | `\|15` | Light Gray |
| `\|16` | Reverse on | `\|17` | Reverse off |

Color resets to white-on-black at the end of each display file. `\|17` (reverse off) is emitted automatically at reset; you only need it mid-file to cancel an earlier `\|16`.

> **Authoring on a C64:** prefer the `\XX` form — the `£` key types it directly. The `|` form is handy when authoring on a PC. A more C64-keyboard-friendly color scheme is on the [roadmap](#future-roadmap).

### MCI Substitution Codes

| Token | Replaced with | Active in |
|-------|---------------|-----------|
| `%SN` | Configured BBS (system) name | `g.term` (connect screen) |
| `%SO` | Configured SysOp name | `g.term` (connect screen) |
| `%BN` | Current board name (trimmed) | `p.msgs`, `p.read` |
| `%MN` | Current message number | `p.read` only |
| `%TL` | Caller's time left today (e.g. `10m`; `unlim` if the level is time-exempt) | Any display/prompt file (`g.*`, `m.*`, `p.*`) |

Tokens not yet set expand to an empty string. The time-left value (`%TL`) is recomputed from the live MINS/DAY state each time a file is shown. On PETSCII it is uppercased (`10M` / `UNLIM`) so it stays readable in the graphics charset. Example `p.main`:
```
 |01(%TL LEFT) |03CMD|07?|04:
```

Example `p.read`:
```
 |05MSG#|03%MN |05IN |03%BN
 |03CMD|07?|04:
```

## SysOp "Spy" Mode

While a caller is online, the SysOp's local C64 screen mirrors what the caller
sees, with a status panel anchored to the bottom rows showing who they are and
offering quick actions. The view adapts to the caller's terminal: 40-column
callers get a native PETSCII spy, and 80-column callers get a software
80-column "soft" view.

### 40-column spy (default)

For PETSCII callers, the caller's screen is mirrored directly
into the C64's 40-column text screen using the C64 character ROM, so the full
character set — including PETSCII graphics — renders faithfully. A five-row
user-details panel (rows 20–24) shows handle, access level, real name, call
count, location, and time online, with quick-action key labels.

The 40-column spy is **locked to the uppercase/graphics charset** for the whole
session — it does **not** follow a caller who switches their terminal to the
lowercase/text charset. This keeps PETSCII menus and art (authored in the
uppercase/graphics charset) and the status panel rendering in correct case, and
keeps the panel from flickering between cases as menus load. The trade-off: the
C64 has only one global charset, so genuinely mixed-case text a lowercase-mode
caller is reading (e.g. a message body) shows in graphics glyphs on the spy.
ANSI/ASCII callers are unaffected — they use the 80-column soft view below,
which renders its own font and preserves mixed case.

### 80-column "soft" view (ANSI / ASCII callers)

The C64's VIC-II has no hardware 80-column mode (that's a C128/VDC feature), so
for 80-column callers TURBO/64 paints a **software** 80-column display: the spy
zone is rendered into a VIC-II hi-res bitmap using a custom 4×6-pixel font,
giving 80 characters across on a stock C64. This lets the SysOp watch an
ANSI/CP437 or plain-ASCII caller's 80-column session — menus,
message reading, file listings — in their true layout.

This mode is **automatic** and requires a **REU** (RAM Expansion Unit, e.g. the
C64 Ultimate's built-in REU): when an REU is detected at boot and the caller's
terminal is 80-column, the soft view is used; otherwise the spy falls back to
the 40-column path. 

**How it works (brief).** When an 80-column session starts, the VIC switches to
hi-res bitmap mode (bank 3): the bitmap lives at `$E000` (under KERNAL ROM,
where the VIC still reads it as RAM; the CPU banks KERNAL out for the brief
glyph writes), and the per-cell colour RAM at `$C000`. A small ANSI parser
interprets the byte stream the BBS sends to the caller — printable text, CR/LF,
cursor positioning (`ESC[H`, `ESC[r;cH`, `ESC[A`–`D`), screen clear (`ESC[2J`),
and SGR colours (`ESC[…m`, mapped to the VIC palette) — and renders it into the
bitmap. Caller content occupies rows 0–23; row 24 is a reverse-video status bar
(handle · access level · time online · action-key hints) that stays visible
wherever the caller navigates and ticks once per second. Content scrolls when
it passes the bottom row, mirroring the caller's terminal.

**SysOp action keys.** **F2** drops the current caller (works from anywhere in
the BBS — the menu, the message bases, etc.). **F1** (SysOp chat) and **F3**
(change access level) are scaffolded but not yet wired.

**Limitations and drawbacks.**

- **Requires an REU.** No REU → 80-column callers are shown on the 40-column
  spy (clipped to 40 columns).
- **ASCII byte-stream only.** Only ANSI/CP437 and ASCII callers use
  the soft view. PETSCII callers — even a C128 in 80-column mode — stay on the
  40-column spy, because the bitmap parser speaks the ANSI/ASCII byte stream,
  not PETSCII control codes.
- **Approximate font.** The 4×6 font covers ASCII `0x20`–`0x7E`. CP437
  box-drawing characters are approximated to `+ - | #`, so framed ANSI screens
  render as recognizable but simplified line art, and fine graphics are lost.
- **Two colours per cell.** Hi-res bitmap mode allows one foreground colour per
  8×8 cell, and each cell holds two characters — so two adjacent characters of
  different colours share the rightmost one's colour. Background is always
  black (ANSI background colours are ignored).
- **Scroll pacing.** Scrolling shifts ~7 KB of bitmap, so on a stock 1 MHz C64
  a fast full-screen scroll paces the BBS's output noticeably slower (the
  caller still receives everything; it just streams a bit slower during heavy
  scrolling). It is much snappier on an accelerated machine like the U64.
- **Bottom row.** The status bar occupies row 24, so the caller's 25th row is
  not shown in the spy.

**Note:** the 80-column soft view is functional but has **not been extensively
tested**, particularly with heavy ANSI art. Expect rough edges with complex
colour/graphics screens. Any help here would be appreciated!

---

## Troubleshooting

**`USR LOG: NOT FOUND` on boot**
Run CONFIGURE → I Init Files. The REL file was never created.

**`USR LOG: EMPTY` on boot**
Record 1 (SysOp) is missing. Run Init Files again.

**`ERROR: MODEM INIT FAILED`**
ACIA not detected at $DE00. On U64, verify ACIA/SwiftLink is enabled. In VICE, verify `tools/deploy-vice.sh` launched with `-acia1` flags.

**VICE: telnet connection refused**
`tcpser` may not have started. Check that `deploy-vice.sh` output shows `tcpser launched`.

**Double-echo characters in terminal**
The BBS sends `IAC WILL ECHO`; your terminal client should suppress local echo. Use a proper telnet client (not raw TCP).

**Garbled characters (PETSCII/ANSI)**
BOOT auto-detects terminal type via backspace probe. Use a terminal that responds to backspace (PETSCII: C64 terminal; ANSI: SyncTerm, PuTTY). If detection fails, BBS falls back to ASCII.

**`M` from main menu says NO BOARDS**
Device 9 is not mounted or has no boards configured. Mount `BOARDS-<ver>.D81` on device 9 and verify `DEV_MSGS=9` in config. If the disk is blank, use CONFIGURE → M — MSG AREAS to create at least one board.

**Messages post fails / index errors**
A `b<n>.idx` file may have been created with 32-byte records (old format). Scratch the affected `b<n>.idx` on device 9 — the BBS recreates it with 48-byte records on the next post.

---

## What Is Not Yet Implemented (v0.1.0)

The following features return a placeholder message:

- Private mail
- File transfers (upload/download)
- SysOp chat / page
- Polls and votes
- Last caller display
- Door games
- System info
- Grafitti wall 
- Preferences (can't edit)
- Help
- Local chat
- 80 column (ANSI) view message viewer
- WFC F Keys (local login, log viewer, etc.)
- Spy F Keys (Chat, Change Access Level)
- Adapt message area to 80 cols if user selects that mode

Data structures and CONFIGURE admin modules for all of the above are in place.

## Future Roadmap

Things I'd love for TURBO/64 to do:

- Message networking (like Image3 - should D8 be compatible? Not Fidonet)
- Embedded scriping / modding support - let sysops customize the hell out of it
- External strings to easy editing
- A more C64-keyboard-friendly markup scheme for gfiles. Colors currently use `|XX`/`\XX` (the `\` form is already C64-typeable via the `£` key), but the `|` prefix isn't on the C64 keyboard. A dedicated single-key or `%`-style color scheme would make on-C64 authoring easier and keep color markup consistent with the `%`-prefixed MCI codes.

---

## How to Contribute

TURBO/64 is an active project and contributions of all kinds are welcome — you do not need to be a C programmer to help.

### Developers

Can you wrangle a C codebase targeting a 1MHz 6502 while having genuine opinions about which C64 BBS software had the best message base? Do you understand why fitting a feature into a 10 KB overlay segment is a satisfying constraint rather than an annoying one? We could use you.

The stubbed features listed above are the priority. Each one has its data layer and CONFIGURE admin UI in place; what is missing is the caller-facing session logic inside `src/features/`. If you want to pick up a feature, open an issue first to avoid duplication.

Good entry points: `src/features/mail.c`, `src/features/vote.c`, `src/features/callers.c` — all are thin stubs waiting to be wired up on the same pattern as `src/features/bulletin.c`.


**Technical chops:**
- Solid C experience — not "I got it to compile with enough casts"
- Understands the 6502 memory model: zero page, bank switching, why you care about what goes where
- Knows what IEC bus timing means in practice and why CBM DOS REL files are both brilliant and painful
- Can read a linker map and understand why the overlay boundary moved four times in a week
- Bonus: familiarity with oscar64, PETSCII terminal emulation, or CBM DOS internals
- Bonus: experience with modem/ACIA programming or serial protocol implementation

**Cultural fit:**
- Either lived through the C64 scene OR has become genuinely and unironically obsessed with it
- Understands why PETSCII is not just "ASCII but weird" and has feelings about it
- Gets that the 40-column display is a design constraint worth respecting, not working around
- Has opinions about which 1581 partition scheme was least terrible
- Will not suggest porting it to a Raspberry Pi

### C64 / Oscar64 Specialists

The BBS runs on a constrained 6502 with a 37 KB core region and a 10.5 KB overlay zone for the message module. Contributions that reduce code size, tighten the overlay boundary, or improve IEC bus throughput are extremely valuable. Familiarity with oscar64 pragmas (`#pragma code`, `#pragma overlay`, `#pragma region`) and CBM DOS REL files is a plus.

### Artists and SysOps

The BBS ships with bare-bones gfiles. What it needs:

- **PETSCII art** (`g.login 0`, `g.newuser 0`, `m.main 0`, `m.msgs 0`, `p.msgs 0`, etc.) — 40-column PETSCII using pipe/backslash color codes (`|NN` or `\NN`)
- **ANSI/CP437 art** (mode `1`, optionally `1 80` for 80-column) — classic BBS ANSI with the same color code system
- **ASCII variants** (mode `3`) — plain text for terminals that strip color
- **Menu prompt files** (`p.*`) — the one-or-two-line prompts shown at each menu; small but high-impact

Files live in `data/gfiles/` and are assembled directly onto the disk image by `make disk`. No build tools required — edit, copy to the D81, and test live.

#### Period-Correct ANSI Artists

Are you an old-school ANSi artist? Younger and inexplicably drawn to this era and aesthetic? Do you need one more goddamn thing to do? Consider spending some of your valuable free time on this — compensated by nothing more than the unyielding appreciation of the people who enjoy this kind of thing. Gotta be a few of us around, right?

**Style guide:**
- Authentic C64/Compunet-era aesthetic — PETSCII character graphics, the C64's native 40-column canvas, and the distinct visual language that came out of the early European and North American C64 scene
- Work within the C64's 16-color palette; the classic cyan-on-blue, or white-on-black with colored highlights are right at home here — lean into the limitations, don't fight them
- Logo screens and headers built from PETSCII block and graphic characters — the kind of thing that looked like it took three days on a real machine, because it did
- ANSI/CP437 variants (for mode `1`) should feel like they belong on a 1990 warez dist site, not a modern retro tribute — if it could have shipped on a C64 BBS in 1991, you're in the right place
- Group shoutouts, greetz, and handle tags are not only acceptable, they are strongly encouraged

### Testers

Real-hardware and emulator coverage is critical. Specifically needed:

- **SD2IEC** — the BBS uses CBM DOS REL files; SD2IEC REL support has known edge cases. Reports of what works and what does not are very helpful.
- **VICE** — regression testing after each feature addition; `tools/deploy-vice.sh` makes this fast.
- **Real 1541/1571/1581** — timing and IEC bus behaviour differs from emulation and the U64's built-in drive.
- **Terminal clients** — SyncTerm, PuTTY, netcat, and real C64 terminals over a nullmodem or SwiftLink.

File issues on GitHub with hardware/firmware versions, a description of what you did, and the exact error or unexpected behaviour.
