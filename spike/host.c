/* spike host — fills DCB at $033C, loads door, calls it, checks ZP survived */
#include <c64/kernalio.h>
#include <conio.h>

#define DCB ((volatile unsigned char *)0x033C)

static void host_print(const char *s) { while (*s) putch(*s++); }

struct api { unsigned char version; void (*print)(const char *); };
static struct api g_api;

/* Oscar64 crashes on (void(*)(void))0x9700 cast; use inline asm instead */
static void call_door(void)
{
    __asm {
        jsr $9700
    }
}

int main(void) {
  /* poke a known zero-page sentinel we expect to survive the door call */
  *((volatile unsigned char *)0x02) = 0x5A;

  g_api.version = 1;
  g_api.print   = host_print;

  DCB[0] = 'D'; DCB[1] = '6';            /* magic */
  DCB[2] = 1;                            /* abi version */
  DCB[3] = (unsigned)&g_api & 0xFF;      /* api ptr lo */
  DCB[4] = ((unsigned)&g_api >> 8);      /* api ptr hi */

  host_print("HOST: LOADING DOOR\r");
  krnio_setnam("DOOR");
  if (!krnio_load(1, 8, 1)) { host_print("LOAD FAILED\r"); return 1; }

  call_door();                            /* enter door at $9700 */

  host_print("HOST: RETURNED\r");
  if (*((volatile unsigned char *)0x02) == 0x5A) host_print("ZP OK\r");
  else host_print("ZP CORRUPT\r");
  return 0;
}
