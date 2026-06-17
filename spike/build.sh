#!/usr/bin/env bash
# spike/build.sh — build HOST.prg + DOOR.prg and assemble spike.d81
#
# Findings from the ZP/ABI spike:
#   * Oscar64 segfaults on ((void(*)(void))0x9700)() casts; use inline __asm instead.
#   * A standalone door PRG at $9700 is produced via oscar64's overlay mechanism:
#       #pragma overlay( DOOR, 1 )  +  #pragma region( door, 0x9700, 0xC000, , 1, ... )
#     This causes oscar64 to emit build/c64/DOOR.prg (load addr $9700) as a side-effect
#     of compiling the door stub.  The door_main symbol lands at exactly $9700.
#   * -n (native code) is required for the door; default bytecode mode + overlay crashes.
#   * The combined c1541 -format ... -write ... chained form does not work with the
#     homebrew c1541 build; format and write must be separate invocations.
#
# Round 2 fix — self-contained door image (no sub-$9700 bcexec calls):
#   * Root cause: oscar64 emits bcexec (indirect-call trampoline) in the stub MAIN
#     region at $088a; the door overlay called JSR $088a → wild jump at runtime.
#   * Fix: override bcexec with an in-region clone via:
#       __asm door_bcexec { jmp (accu) }
#       #pragma runtime(bcexec, door_bcexec)
#     door_bcexec lands at $9788; all door JSR/JMP targets are now >= $9700.
#   * door_entry() at $9700 saves host's oscar64 SP ($23/$24), installs door-local
#     SP top ($BFFE), calls door_main, then restores host SP before RTS.
#   * host_sp_lo/hi scratch variables placed in door_bss (in-region at $97a3/$97a4)
#     via #pragma bss(door_bss) / #pragma bss(bss) guards.
#   * Host saves/restores ZP $02-$8F around JSR $9700 to protect all oscar64 runtime
#     state (highest observed ZP var in HOST.asm: $54; $8F is generous headroom).
#
# Round 3 — BBS build mode:
#   * HOST now built with -Os -Oo (no -n) matching the real BBS build configuration.
#   * host_print is #pragma native: door indirect-calls it; it calls bytecode host_emit
#     (proving native door → native wrapper → bytecode body chain).
#   * call_door is #pragma native with hand __asm X-indexed ZP save/restore (no ZP
#     scratch used in the saved $02-$8F range; uses only A and X registers).
#   * ZP save range widened evidence: BOOT-0.1.0.asm highest named ZP var is T12=$64;
#     $8F upper bound remains generous headroom.

set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OSCAR="$ROOT/vendor/oscar64/bin/oscar64"
INC="-i=$ROOT/vendor/oscar64/include"
OUT="$ROOT/build/c64"

mkdir -p "$OUT"

echo "--- Building HOST.prg ---"
# Round 3: HOST built with BBS build flags (-Os -Oo, no -n) to match the real BBS.
# host_print and call_door are #pragma native to ensure:
#   - host_print: native entry point so the door's indirect call (JMP ACCU) lands correctly,
#     and it can internally call the bytecode host_emit (native→bytecode intra-host chain).
#   - call_door: native so the inline __asm ZP save/restore uses no ZP scratch (self-corruption
#     safe) and the JSR $9700 works in native context.
"$OSCAR" $INC -Os -Oo -o="$OUT/HOST.prg" "$ROOT/spike/host.c"

echo "--- Building DOOR.prg (overlay at \$9700) ---"
# door: oscar64 overlay mechanism pins door_main at exactly $9700.
# door.c declares #pragma overlay( DOOR, 1 ) + #pragma region( door, 0x9700, ... )
# and places door_main in the door_code section.  The compiler emits DOOR.prg
# (load addr $9700) alongside the stub binary (DOOR_STUB.prg, discarded).
"$OSCAR" $INC -n -O2 -o="$OUT/DOOR_STUB.prg" "$ROOT/spike/door.c"
# DOOR.prg is produced in the same directory as the -o output

echo "--- Verifying DOOR.prg load address ---"
DOOR_LO=$(xxd -l 2 "$OUT/DOOR.prg" | awk '{print $2}')
if [ "$DOOR_LO" = "0097" ]; then
    echo "OK: DOOR.prg load address is \$9700"
else
    echo "ERROR: DOOR.prg load address is not \$9700 (got: $DOOR_LO)" >&2
    exit 1
fi

echo "--- Assembling spike.d81 ---"
C1541="${C1541:-c1541}"
# Format then write (combined form not supported by all c1541 builds)
"$C1541" -format "spike,01" d81 "$OUT/spike.d81" >/dev/null
"$C1541" "$OUT/spike.d81" -write "$OUT/HOST.prg" host >/dev/null
"$C1541" "$OUT/spike.d81" -write "$OUT/DOOR.prg" door >/dev/null

echo ""
echo "=== Spike build complete ==="
echo "  HOST.prg  : $OUT/HOST.prg"
echo "  DOOR.prg  : $OUT/DOOR.prg  (load addr \$9700, door_main at \$9700)"
echo "  spike.d81 : $OUT/spike.d81"
echo ""
echo "To run in VICE (human gate — ZP-survival unverified by this script):"
echo "  x64sc \"$OUT/spike.d81\" 2>/dev/null &"
echo "  # In VICE: LOAD\"HOST\",8  then RUN"
echo "  # Expected output:"
echo "  #   HOST: LOADING DOOR"
echo "  #   DOOR: HELLO VIA CALLBACK"
echo "  #   HOST: RETURNED"
echo "  #   ZP OK"
