/* spike door — built to $9700; reads DCB, calls host print, returns.
 * Compiled as a standalone overlay PRG via the DOOR overlay region.
 * Build produces build/c64/DOOR.prg (load addr $9700) as a side-effect. */

/* Pin all door code into the DOOR overlay region at $9700-$BFFF */
#pragma overlay( DOOR, 1 )
#pragma section( door_code, 0 )
#pragma section( door_bss,  0, , , bss )
#pragma region( door, 0x9700, 0xC000, , 1, {door_code, door_bss} )

#pragma code(door_code)

#define DCB ((volatile unsigned char *)0x033C)
struct api { unsigned char version; void (*print)(const char *); };

void door_main(void) {
  struct api *a = (struct api *)(DCB[3] | (DCB[4] << 8));
  if (DCB[0] != 'D' || DCB[1] != '6') return;
  a->print("DOOR: HELLO VIA CALLBACK\r");
}

#pragma code(code)

/* door_entry_ptr forces door_main into the overlay region (prevents DCE).
 * The stub main() below is never run at $9700; the host JSRs there directly. */
void (*door_entry_ptr)(void) = door_main;

int main(void) {
  door_entry_ptr();
  return 0;
}
