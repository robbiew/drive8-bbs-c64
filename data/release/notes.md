This is an early pre-release for testing and feedback. It is incomplete — many features are stubbed out and there are known rough edges. Expect bugs.

If you run into problems or have suggestions, please open an issue on GitHub:

  https://github.com/robbiew/turbo64/issues

New in v__VERSION__
-------------------
- Door programs: the BBS now loads and runs external Oscar64 plug-ins
  ("doors") during a call. Register them in CONFIGURE (D — DOOR PROGRAMS):
  title, filename, device/drive, command key, min access level, and an
  optional run-at-login flag.
- Door author dev kit: write a door as a single C file against a small
  versioned SDK (caller info + core I/O), build with `make door`. See
  devkit/README.md. The example FORTUNE door ships on the disk image.
- CONFIGURE message-area editor reworked to a paged list + interactive
  field editor (the manual compact/prune screen was retired; pruning
  still runs automatically at runtime).

What works in v__VERSION__:

- Login and new-user registration
- Terminal auto-detection (PETSCII, ANSI/CP437, ASCII)
- Bulletin boards (read, post)
- Door programs (run external plug-ins; dev kit for authors)
- SysOp Configure editor (init files, user mgmt, board mgmt,
  door mgmt, config, access levels)
- 80-column SysOp spy mode (ANSI callers, REU required)
- SwiftLink/ACIA modem support, VICE tcpser bridge

What is NOT yet working:

- Private mail
- File transfers (upload/download)
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