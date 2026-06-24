This is an early pre-release for testing and feedback. It is incomplete and
file transfers in particular need more real-world testing before they can be
considered stable. Expect bugs — please report them on GitHub.

  https://github.com/robbiew/turbo64/issues

New in v__VERSION__
-------------------
- File transfers: Punter (C64-native) and Zmodem (modern terminal) protocols
  for both upload and download
- FILES overlay: browse file areas, list files, select by number, choose
  protocol, upload with automatic file-entry registration
- Per-area file entry data store (REL-backed CRUD, soft-delete, download
  counter tracking)
- Resident Zmodem shim safely swaps overlays mid-transfer without corrupting
  the FILES session
- Binary-safe raw net I/O for protocol transfers
- CBM-DOS filename sanitisation on upload; download counter updated for both
  Punter and Zmodem paths
- Door and file menu improvements

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
