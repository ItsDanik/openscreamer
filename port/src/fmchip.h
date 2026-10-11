/*
 * fmchip.h - the sound card's FM chip (YM3812, "OPL2"), as music.c sees it.
 *
 * music.c only ever writes the chip's registers, as the game's driver did.
 * What is behind this is a software chip here (fmchip.c); a port that has a
 * real one -- the MiSTer core's, in the FPGA -- replaces that one file.
 */
#ifndef OS_FMCHIP_H
#define OS_FMCHIP_H

/* a silent chip: every register as after power-on */
void fmchip_reset(void);
/* the writes of one moment follow each other as on the card, 40 microseconds apart */
void fmchip_write(int reg, int value);
/* one output frame at 22050 Hz of what the chip plays, on a 16-bit scale
   (one note at full level swings +-4085) */
float fmchip_frame(void);

#endif
