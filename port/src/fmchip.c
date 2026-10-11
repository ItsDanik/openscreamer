/*
 * fmchip.c - the FM chip in software: Nuked OPL3 (nuked/, by Nuke.YKT,
 * LGPL 2.1 or later), a chip-exact OPL3 that is an OPL2 until it is told
 * otherwise, which music.c never does.
 *
 * The chip makes 49716 samples a second.  They are taken at 44100 and two
 * are averaged for one frame of the mixer's 22050.  A register write is
 * queued two chip samples (40 microseconds) behind the one before, about the
 * time the driver's own write routine took on the card: it wrote the
 * register's number, read the status port six times, wrote the value and
 * read the port 28 times more.
 */
#include <stdint.h>
#include "fmchip.h"
#include "nuked/opl3.h"
#ifdef MISTER_HYBRID
#include "mister.h"
#endif

static opl3_chip g_chip;
static int       g_ready;
static int       g_core;      /* MISTER: the chip is the core's */

void fmchip_reset(void)
{
#ifdef MISTER_HYBRID
    g_core = mister_fm_start();
#endif
    OPL3_Reset(&g_chip, 44100);
    g_ready = 1;
}

void fmchip_write(int reg, int value)
{
    if (!g_ready) fmchip_reset();
#ifdef MISTER_HYBRID
    if (g_core) { mister_fm_write(reg, value); return; }
#endif
    OPL3_WriteRegBuffered(&g_chip, (uint16_t)(reg & 0xff), (uint8_t)value);
}

float fmchip_frame(void)
{
    int16_t a[2], b[2];
    if (!g_ready || g_core) return 0.0f;
    OPL3_GenerateResampled(&g_chip, a);
    OPL3_GenerateResampled(&g_chip, b);
    return (float)(a[0] + b[0]) * 0.5f;
}

void fmchip_at(int frame)
{
#ifdef MISTER_HYBRID
    mister_fm_at(frame);
#else
    (void)frame;
#endif
}

void fmchip_gain(float gain)
{
#ifdef MISTER_HYBRID
    mister_fm_gain(gain);
#else
    (void)gain;
#endif
}
