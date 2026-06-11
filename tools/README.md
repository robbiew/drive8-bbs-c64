# TURBO/64 BBS — Development Tools

Build, test, and deploy scripts for TURBO/64 BBS. All tools live in `tools/` and can be run from the project root.

---

## Quick Start

### Test in VICE

```bash
tools/build.sh vice           # build + assemble disk + launch VICE
tools/build.sh vice -f        # fullscreen
```

### Deploy to Ultimate64

```bash
tools/build.sh u64            # build + assemble fresh disk + deploy
tools/build.sh u64 --seed-users   # preserve existing user data
tools/build.sh u64 -l sd     # deploy to SD card
```

---

## Tool Reference

### `build.sh` — Master Launcher

Orchestrates build, disk assembly, and deployment in one command.

```bash
tools/build.sh <command> [options]
```

| Command | Description |
|---------|-------------|
| `vice` | Build binaries, assemble disk, launch VICE |
| `u64` | Build binaries, assemble disk, deploy to U64 |
| `disk` | Assemble disk image only (no binary rebuild) |
| `build` | Compile BOOT and CONFIGURE PRGs only |
| `clean` | Remove all build artifacts |
| `help` | Show usage |

**User database options** (for `u64` and `disk` commands):

| Option | Effect |
|--------|--------|
| `--fetch-users` | Fetch live USR LOG/PROF from U64 → `data/users-seed.d81`, then assemble |
| `--seed-users` | Assemble using existing `data/users-seed.d81` |

Neither flag → fresh disk (prompts for confirmation before wiping user data on U64).

**Examples:**

```bash
tools/build.sh vice                   # build and launch VICE
tools/build.sh vice -f                # fullscreen
tools/build.sh u64                    # fresh disk (prompts)
tools/build.sh u64 -l sd             # deploy to SD card
tools/build.sh u64 --fetch-users     # preserve live user DB
tools/build.sh u64 --seed-users      # restore from data/users-seed.d81
tools/build.sh disk --seed-users     # assemble only, with user data
tools/build.sh build                 # compile BOOT-<ver>.prg + CONFIGURE-<ver>.prg
tools/build.sh clean                 # wipe build/
```

**Environment:**
```
VICE_CMD=x64sc     # override VICE binary (default: x64sc)
T64_SD_PATH=...     # override U64 deployment path
```

---

### `deploy-vice.sh` — VICE Launcher

Launches VICE with the BBS disk image, a tcpser modem bridge, and a DS12C887 RTC cartridge at `$D700` (avoids conflict with the ACIA at `$DE00`).

```bash
tools/deploy-vice.sh [options]
```

| Option | Description |
|--------|-------------|
| `-f, --fullscreen` | Launch fullscreen |
| `-p, --paused` | Pause on startup |
| `-d, --debugger` | Enable VICE debugger window |
| `--no-autostart` | Don't auto-load the disk |
| `-c, --config <path>` | Use custom VICE config file |
| `--no-tcpser` | Skip modem bridge |
| `--tcpser-port <port>` | Telnet port for tcpser (default: 6400) |

tcpser bridges VICE's virtual SwiftLink/ACIA to a telnet port. Connect from any terminal:
```bash
telnet localhost 6400
```

**Environment:**
```
VICE_CMD=x64sc    # override VICE binary
TCPSER_CMD=tcpser # override tcpser binary
```

---

### `assemble-d81.sh` — Disk Image Assembly

Assembles `build/c64/TURBO64-<ver>.d81` from compiled PRGs, `data/config`, and `data/gfiles/`.

```bash
tools/assemble-d81.sh [options] [version]
```

| Option | Description |
|--------|-------------|
| `--seed-users <path>` | Use existing `.d81` as base, preserving USR LOG and USR PROF |
| `--fetch-users` | Fetch live disk from U64 via `fetch-u64.sh`, use as seed |
| `[version]` | Override version string (default: read from `include/bbs/version.h`) |

If `data/users-seed.d81` is present it is used as the seed automatically (no flag required).

**Output** — `build/c64/`:
```
TURBO64-<ver>.d81          bootable BBS disk image
BOOT-<ver>.prg        main BBS runtime
CONFIGURE-<ver>.prg   SysOp editor
ovl_msgs.prg          message module overlay
ovl_wfc.prg           WFC display overlay
```

---

### `deploy-u64.sh` — Ultimate64 Deployment

Uploads `TURBO64-<ver>.d81` to U64 storage via `c64u`. Optionally also restores the boards seed disk.

```bash
tools/deploy-u64.sh [options]
```

| Option | Description |
|--------|-------------|
| `-l, --location <loc>` | `usb0` (default), `usb1`, `sd`, `bbs` (→ `/BBS`), or a full path |
| `--boards` | Also upload `data/boards-seed.d81` as `BOARDS-<ver>.D81` |
| `-h, --help` | Show help |

```bash
tools/deploy-u64.sh                  # deploy to /USB0/BBS/
tools/deploy-u64.sh -l sd           # deploy to /SD/BBS/
tools/deploy-u64.sh -l bbs          # deploy to /BBS/
tools/deploy-u64.sh --boards -l bbs # also restore boards disk
```

On U64 after deploying:
```
@0 "TURBO64-0.1.0.D81"
LOAD "BOOT-0.1.0",8
RUN
```

**Requires:** `vendor/c64u/bin/c64u` — run `tools/install-c64u.sh` if missing.

**Environment:** `T64_SD_PATH` overrides default path.

---

### `fetch-u64.sh` — Download Live BBS Disk

Downloads the live `TURBO64-<ver>.D81` from U64 to the project root (or a specified path). Useful for inspecting the live disk without modifying it.

```bash
tools/fetch-u64.sh [options]
```

| Option | Description |
|--------|-------------|
| `-l, --location <loc>` | Source location: `usb1` (default), `sd`, or full path |
| `-o, --output <path>` | Local destination (default: `<root>/TURBO64-<ver>.D81`) |

**Environment:** `T64_SD_PATH` overrides default source path.

---

### `extract-users.sh` — Snapshot User Database from U64

Fetches the live BBS disk from U64 and extracts user database files. Saves a seed disk for future builds.

```bash
tools/extract-users.sh [options]
```

| Option | Description |
|--------|-------------|
| `-l, --location <loc>` | Source location: `usb1` (default), `sd`, or full path |
| `-d, --disk <path>` | Use a local `.d81` instead of fetching from U64 |

**Outputs to `data/`:**
- `users-seed.d81` — full disk image; auto-used by `assemble-d81.sh` when present
- `usr_log` — raw USR LOG bytes (flat, no CBM overhead)
- `usr_prof` — raw USR PROF bytes

---

### `install-c64u.sh` — Install c64u CLI

Downloads and installs the `c64u` Ultimate64 filesystem tool into `vendor/c64u/bin/c64u`. Idempotent — exits immediately if already installed. Auto-detects platform (Darwin/Linux, arm64/x86_64).

```bash
tools/install-c64u.sh
```

---

### `install-oscar64.sh` — Build Oscar64 Compiler

Clones Oscar64 and builds it into `vendor/oscar64/`. Idempotent.

```bash
tools/install-oscar64.sh
```

Requires: `gcc`, `make`, `git`. Clones from `https://github.com/drmortalwombat/oscar64.git` (shallow). Installs to `vendor/oscar64/bin/oscar64`.

---

### `lint.sh` — Static Analysis

Runs `cppcheck` on C source with C99, warnings, style, performance, and portability checks. Exits 1 on error.

```bash
tools/lint.sh
```

Install cppcheck: `brew install cppcheck` / `sudo apt install cppcheck`.

---

### `release.sh` — GitHub Release Pipeline

Builds all release artifacts, generates FILE_ID.DIZ and README.txt from templates, bundles them into a single ZIP, and publishes a GitHub release via `gh`.

```bash
tools/release.sh              # full release: build, tag, push, publish
tools/release.sh --dry-run    # build and stage artifacts only, no tag/push/publish
tools/release.sh --skip-tag   # publish to an existing tag (no new tag created)
```

| Option | Description |
|--------|-------------|
| `--dry-run` | Build artifacts, create ZIP, show planned steps — no tag, push, or publish |
| `--skip-tag` | Build and publish to an existing tag — no git tag created or pushed |
| `-h, --help` | Show usage |

**Requires:** `gh` CLI installed and authenticated (`gh auth login`), `zip`, `oscar64` compiler, and `c1541` available.

**Release asset:** A single `TURBO64-<ver>.zip` containing:

- `TURBO64-<ver>.d81` — bootable BBS system disk
- `BOOT-<ver>.prg` — standalone BBS runtime
- `CONFIGURE-<ver>.prg` — standalone SysOp config editor
- `BOARDS-<ver>.d81` — blank disk for message bases (Device 9)
- `FILE_ID.DIZ` — classic BBS description file (45 chars/line, 10 lines)
- `README.txt` — plain-text project README

**Release notes:** If `data/release/notes.md` exists (with optional `__VERSION__` tokens), it is used as the GitHub release body. Otherwise, the script falls back to `git log` since the previous tag.

---

## Environment Variables

| Variable | Default | Used by |
|----------|---------|---------|
| `VICE_CMD` | `x64sc` | `deploy-vice.sh`, `build.sh` |
| `TCPSER_CMD` | `tcpser` | `deploy-vice.sh` |
| `T64_SD_PATH` | (none) | `deploy-u64.sh`, `fetch-u64.sh`, `extract-users.sh`, `extract-boards.sh` |
| `C1541` | `c1541` | `assemble-d81.sh`, `extract-boards.sh` |

---

## Output Locations

```
build/c64/
├── TURBO64-<ver>.d81          assembled BBS disk image
├── BOOT-<ver>.prg        main BBS runtime
├── CONFIGURE-<ver>.prg   SysOp editor
├── ovl_msgs.prg          message module overlay (loaded on demand)
└── ovl_wfc.prg           WFC display overlay (loaded on demand)

data/
├── users-seed.d81        user database snapshot (from extract-users.sh)
├── boards-seed.d81       boards disk snapshot (from extract-boards.sh)
├── usr_log               raw USR LOG binary
├── usr_prof              raw USR PROF binary
└── release/
    ├── file_id.diz.tmpl  FILE_ID.DIZ template (__VERSION__ substitution)
    ├── readme.txt.tmpl    README.txt template (__VERSION__ substitution)
    └── notes.md           release notes template (used by release.sh)

build/release/
└── TURBO64-<ver>.zip     release archive (created by release.sh)
```

---

## Script Index

| Script | Purpose |
|--------|---------|
| `build.sh` | Master launcher: vice / u64 / disk / build / clean |
| `deploy-vice.sh` | Launch VICE with modem bridge and RTC cartridge |
| `assemble-d81.sh` | Assemble bootable D8 disk image |
| `deploy-u64.sh` | Upload BBS (and optionally boards) disk to U64 |
| `fetch-u64.sh` | Download live BBS disk from U64 |
| `extract-users.sh` | Snapshot user database from U64 |
| `extract-boards.sh` | Snapshot boards disk from U64 |
| `install-c64u.sh` | Install c64u filesystem tool |
| `install-oscar64.sh` | Build oscar64 compiler from source |
| `lint.sh` | Run cppcheck static analysis |
| `release.sh` | Build artifacts, tag, and publish GitHub release |
