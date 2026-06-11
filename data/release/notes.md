TURBO/64 BBS v__VERSION__ — PRE-RELEASE
========================================

This is an early pre-release for testing and feedback. It is incomplete — many features are stubbed out and there are known rough edges. Expect bugs.

If you run into problems or have suggestions, please open an
issue on GitHub:

  https://github.com/robbiew/turbo64/issues

What works in v__VERSION__:

- Login and new-user registration
- Terminal auto-detection (PETSCII, ANSI/CP437, ASCII)
- Bulletin boards (read, post, maintenance)
- SysOp Configure editor (init files, user mgmt, board mgmt,
  config, access levels)
- 80-column SysOp spy mode (ANSI callers, REU required)
- SwiftLink/ACIA modem support, VICE tcpser bridge

What is NOT yet working:

- Private mail
- File transfers (upload/download)
- SysOp chat / page
- Polls and votes
- Door games
- Several smaller features (see README.txt)

Installation
------------
1. Mount TURBO64-__VERSION__.d81 on device 8
2. LOAD "CONFIGURE-__VERSION__",8
3. RUN — choose I (INIT FILES) for first-time setup
4. LOAD "BOOT-__VERSION__",8
5. RUN

For full instructions see README.txt inside the archive.