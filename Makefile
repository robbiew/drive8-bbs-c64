# TURBO/64 BBS — Makefile

ROOT        := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
OSCAR64     := $(ROOT)vendor/oscar64/bin/oscar64
OUTDIR      := $(ROOT)build/c64

VERSION     := $(shell grep 'BBS_RELEASE_VERSION_COMPACT' $(ROOT)include/bbs/version.h | cut -d'"' -f2)

BOOT_PRG        := $(OUTDIR)/BOOT-$(VERSION).prg
CONFIGURE_PRG   := $(OUTDIR)/CONFIGURE-$(VERSION).prg
OVERLAYS_D64 := $(OUTDIR)/overlays.d64
MSGS_OVL_PRG := $(OUTDIR)/ovl_msgs.prg
BOOT_OVL_PRG := $(OUTDIR)/ovl_boot.prg

INCLUDES    := -i=$(ROOT)include -i=$(ROOT)vendor/oscar64/include -i=$(ROOT)src -i=$(ROOT)src/data
OFLAGS      := -Os -Oo

# BOOT-only defines: strip oscar64's dead printf float path (no %f in the
# codebase). Scoped to BOOT; the editor build is untouched.
# NOTE: -dNOLONG also applies (no %l in source) but currently crashes oscar64's
# code generator (assertion in NativeCodeGenerator.cpp CopyCode), so it's left
# off; NOFLOAT alone frees ~2.4KB, ample for current needs.
BOOT_DEFS   := -dNOFLOAT -dT64_BOOT_OVERLAY

DRVFLAGS    :=
ifdef T64_DRIVE_SYSTEM
DRVFLAGS    += -D=T64_DRIVE_SYSTEM=$(T64_DRIVE_SYSTEM)
endif
ifdef T64_DRIVE_MSGS
DRVFLAGS    += -D=T64_DRIVE_MSGS=$(T64_DRIVE_MSGS)
endif
ifdef T64_DRIVE_FILES
DRVFLAGS    += -D=T64_DRIVE_FILES=$(T64_DRIVE_FILES)
endif
ifdef T64_DRIVE_DOORS
DRVFLAGS    += -D=T64_DRIVE_DOORS=$(T64_DRIVE_DOORS)
endif

CFLAGS      := $(INCLUDES) $(OFLAGS) $(DRVFLAGS)

HAL_SRCS := src/err.c src/hal/clock.c src/hal/disk.c src/hal/rel.c src/hal/reu.c src/hal/term.c src/io/io.c src/net/at_response.c src/net/telnet_iac.c src/platform/cbmdos/net.c src/term/term.c src/term/cp437_ascii.c src/term/cp437_petscii.c src/term/ansi.c
DATA_SRCS := src/data/cfg.c src/data/users.c src/data/boards.c src/data/file_areas.c src/data/votes.c src/data/messages.c src/data/usrptr.c src/data/sstatus.c src/data/syscnt.c src/data/access.c src/data/usrday.c src/data/user_hash.c src/data/doors.c src/data/door_visible.c
SESSION_SRCS := src/session/session.c src/session/spy80.c src/session/spy80_ansi.c src/session/spy80_font.c src/session/prompt_cursor.c src/session/prompt_cursor_frame.c
FEATURE_SRCS := src/features/menu.c src/features/menu_tables.c src/features/menu_actions.c src/features/auth.c src/features/newuser.c src/features/bulletin.c src/features/editor.c src/features/mail.c src/features/files.c src/features/chat.c src/features/vote.c src/features/callers.c src/features/sysop.c src/features/doors.c
PUB_HDRS := include/bbs/version.h include/bbs/config.h include/bbs/types.h include/bbs/err.h include/bbs/drives.h include/bbs/net.h include/bbs/io.h include/bbs/term.h include/bbs/rel.h include/bbs/records.h include/bbs/cfg.h include/bbs/users.h include/bbs/boards.h include/bbs/file_areas.h include/bbs/votes.h include/bbs/messages.h include/bbs/usrptr.h include/bbs/sstatus.h include/bbs/syscnt.h include/bbs/usrday.h include/bbs/editor.h include/bbs/menu.h include/bbs/session.h include/bbs/prompt_cursor.h include/bbs/auth.h include/bbs/bulletin.h include/bbs/mail.h include/bbs/files.h include/bbs/chat.h include/bbs/vote.h include/bbs/callers.h include/bbs/sysop.h include/bbs/hal/clock.h include/bbs/hal/disk.h include/bbs/hal/reu.h include/bbs/hal/term.h include/bbs/menu_actions.h include/bbs/doors.h

.PHONY: all c64 editor clean release test lint

SETUP_DATA  := $(OUTDIR)/config
SETUP_SRC   := $(ROOT)data/config

all: c64 editor

c64: $(BOOT_PRG) $(SETUP_DATA)

editor: $(CONFIGURE_PRG)

disk: all
	bash tools/assemble-d81.sh

disk-with-users: all
	bash tools/assemble-d81.sh --fetch-users

release:
	bash tools/release.sh

$(SETUP_DATA): $(SETUP_SRC)
	@mkdir -p $(OUTDIR)
	@cp $< $@

$(BOOT_PRG): src/main.c $(HAL_SRCS) $(DATA_SRCS) $(SESSION_SRCS) $(FEATURE_SRCS) $(PUB_HDRS)
	@mkdir -p $(OUTDIR)
	$(OSCAR64) $(CFLAGS) $(BOOT_DEFS) -o=$@ -d64=$(OVERLAYS_D64) $< $(HAL_SRCS) $(DATA_SRCS) $(SESSION_SRCS) $(FEATURE_SRCS)
	@echo "Built: $@"

# ovl_msgs.prg / ovl_boot.prg are produced as side-effects of the BOOT_PRG -d64
# build. These targets just verify they exist after the build.
$(MSGS_OVL_PRG): $(BOOT_PRG)
	@test -f "$@" || { echo "ERROR: overlay file not produced by oscar64"; exit 1; }
	@echo "Overlay: $@"

$(BOOT_OVL_PRG): $(BOOT_PRG)
	@test -f "$@" || { echo "ERROR: overlay file not produced by oscar64"; exit 1; }
	@echo "Overlay: $@"

EDITOR_HAL_SRCS := src/err.c src/hal/disk.c src/hal/rel.c
EDITOR_SRCS := src-editor/setup.c src-editor/reu_stubs.c src-editor/ui/util.c src-editor/ui/menu.c src-editor/ui/list.c src-editor/ui/edit.c src-editor/ui/dialog.c src-editor/admin/users.c src-editor/admin/messages.c src-editor/admin/config.c src-editor/admin/files.c src-editor/admin/votes.c

$(CONFIGURE_PRG): src-editor/main.c $(EDITOR_SRCS) $(EDITOR_HAL_SRCS) $(DATA_SRCS) $(PUB_HDRS)
	@mkdir -p $(OUTDIR)
	$(OSCAR64) $(CFLAGS) -i=$(ROOT)src-editor $(CFLAGS) -o=$@ $< $(EDITOR_SRCS) $(EDITOR_HAL_SRCS) $(DATA_SRCS)
	@echo "Built: $@"

test:
	bash tools/test.sh

lint:
	bash tools/lint.sh

clean:
	$(RM) $(OUTDIR)/*.prg $(OUTDIR)/*.asm $(OUTDIR)/*.int $(OUTDIR)/*.lbl $(OUTDIR)/*.map $(OUTDIR)/*.bcs $(OUTDIR)/config $(OVERLAYS_D64) $(MSGS_OVL_PRG) $(BOOT_OVL_PRG)
	$(RM) -r $(ROOT)build/host
	@echo "Clean."
