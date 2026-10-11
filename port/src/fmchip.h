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
/* for a chip that is not in software (MISTER): where in the block of audio
   being made the next writes belong, in 44.1 kHz frames, and how loud the chip
   is to be, 1 = as it comes.  Nothing to the software chip. */
void fmchip_at(int frame);
void fmchip_gain(float gain);

#endif
