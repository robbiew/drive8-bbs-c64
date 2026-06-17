/* DIAGNOSTIC spike door (round 2 fix) — self-contained $9700 image.
 * All runtime helpers (bcexec) live inside the $9700-$C000 region.
 * Entry saves host SP, sets door-local SP, calls door_main, restores host SP.
 * Host saves/restores ZP $02-$8F around the JSR $9700. */

#pragma overlay( DOOR, 1 )
#pragma section( door_code, 0 )
#pragma section( door_data, 0 )
#pragma section( door_bss,  0, , , bss )
#pragma region( door, 0x9700, 0xC000, , 1, {door_code, door_data, door_bss} )

#pragma data(door_data)
static const char MSG[] = "DOOR HELLO VIA CALLBACK";
#pragma data(data)

#pragma code(door_code)

#define SCR ((volatile unsigned char *)0x0400)
#define COL ((volatile unsigned char *)0xD800)
#define DCB ((volatile unsigned char *)0x033C)

/* oscar64 ZP software stack pointer at $23/$24 */
#define ZP_SP_LO  (*((volatile unsigned char *)0x23))
#define ZP_SP_HI  (*((volatile unsigned char *)0x24))

/* Door-local oscar64 software stack top: top of door region minus guard */
/* Door region is $9700-$C000; stack top = $BFFE (2-byte aligned, below $C000) */
#define DOOR_STACK_LO 0xFE
#define DOOR_STACK_HI 0xBF

struct api { unsigned char version; void (*print)(const char *); };

/* Override bcexec inside the door region so no cross-region calls are needed.
 * In native mode (-n), bcexec is simply JMP (ACCU) = JMP ($001b). */
__asm door_bcexec
{
    jmp (accu)
}
#pragma runtime(bcexec, door_bcexec)

/* host_sp_lo / host_sp_hi: scratch in door_bss (must be inside $9700 region) */
#pragma bss(door_bss)
static unsigned char host_sp_lo;
static unsigned char host_sp_hi;
#pragma bss(bss)

void door_main(void) {
  struct api *a;
  SCR[40] = 0x01; COL[40] = 1;                       /* 'A' door entered */
  a = (struct api *)(DCB[3] | (DCB[4] << 8));
  SCR[41] = 0x02; COL[41] = 1;                       /* 'B' DCB pointer read */
  if (DCB[0] != 'D' || DCB[1] != '6') return;
  SCR[42] = 0x03; COL[42] = 1;                       /* 'C' magic ok */
  a->print(MSG);                                      /* cross-build callback */
  SCR[43] = 0x04; COL[43] = 1;                       /* 'D' callback returned */
}

/* door_entry: JSR-safe entry at $9700.
 * Saves host's oscar64 SP, installs door-local SP, calls door_main, restores. */
void door_entry(void) {
  /* save host SP */
  host_sp_lo = ZP_SP_LO;
  host_sp_hi = ZP_SP_HI;
  /* install door-local software stack top */
  ZP_SP_LO = DOOR_STACK_LO;
  ZP_SP_HI = DOOR_STACK_HI;
  /* run door */
  door_main();
  /* restore host SP so host resumes cleanly */
  ZP_SP_LO = host_sp_lo;
  ZP_SP_HI = host_sp_hi;
}

#pragma code(code)

/* Prevent dead-code elimination; the stub calls door_entry which chains to door_main. */
void (*door_entry_ptr)(void) = door_entry;

int main(void) {
  door_entry_ptr();
  return 0;
}
