/*
 * SCREAMER 2: the player's STARTL.EXE read into the flat layout its addresses mean (s2_exe.c).
 * The converted routines take from it what the program holds as it starts: its data object
 * (the menu's tables and texts, copied into the block) and the few variables the engine keeps
 * in its code (s2_code_var).  Nothing of the program is in the port; this goes too once every
 * routine is written again by hand (docs/NOTES2.md S2-7).
 */
#ifndef OS_S2_EXE_H
#define OS_S2_EXE_H

#include <stdint.h>

#define S2_FLAT 0x210000u            /* code 0x10000.., data 0x110000..0x139c90, and room after */
extern uint8_t s2_flat[S2_FLAT];

/* the player's program: 1 if it is the one these addresses are of */
int  s2_exe_load(const char *path);
/* the image as it was read (s2_flat is given back to it) */
void s2_exe_fresh(void);

#endif
