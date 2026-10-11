/*
 * music.c - the game's MIDI music (MUSICn.HMP), played as the game played it:
 * by writing the registers of the sound card's FM chip.
 *
 * The original has three layers.  The game hands a song to HMI's MIDI player,
 * which is linked into STARTL.EXE (0xd2000..0xe4000); that steps the song's
 * tracks on a timer and passes each MIDI message to a driver out of
 * HMIMDRV.386; the driver for an AdLib / Sound Blaster is `fmmidi.com`
 * ("HMI MIDI Driver For OPL2"), which turns the messages into register writes
 * with the two instrument banks.  The first two are below, read from the
 * code; the chip is behind fmchip.h.
 *
 * From the game's files:
 *   MUSICn.HMP   "HMIMIDIP013195": +0x30 number of tracks, +0x38 ticks per
 *                second (120); the tracks start at 0x388, each
 *                [number, length including these 12 bytes, a channel] + events.
 *                An event is a delay then a MIDI message; the delay is 7 bits
 *                a byte, LOW part first, and the LAST byte has its top bit
 *                set (the reverse of a MIDI file).  FF 2F ends a track.
 *   MELODIC.BNK  AdLib instrument bank, 128 instruments = the MIDI programs:
 *   DRUM.BNK     the same, 128 = the drum notes of MIDI channel 10; the third
 *                byte of a drum's name record is the pitch it is played at.
 *                Bank: +8 instruments in use, +0x0c offset of the name
 *                records (12 bytes), +0x10 offset of the instruments (30
 *                bytes: 2, then 13 for each of the two operators -- key
 *                scaling, multiplier, feedback, attack, sustain level,
 *                sustaining, decay, release, level, tremolo, vibrato, rate
 *                scaling, connection -- then the two wave shapes).
 *
 * THE PLAYER (0xe3d51 the timer's routine, 0xe2ca9 a message on its way to the
 * driver, 0xe37eb a song's end, 0xe29d9 / 0xd3a54 the volumes):
 *   - every tick each track's count goes up, and its messages are sent while
 *     the delay before them has run out; track 0's messages are not sent;
 *   - "channel volume" (controller 7) is the only message it changes: the
 *     driver gets  master x (song x value >> 7) >> 7,  song = 127 and master =
 *     the game's music volume / 256 (96 as the game starts it), and the value
 *     is remembered per channel; when the master changes, each track still
 *     running gets its remembered value again for the channel in its header;
 *   - at a song's end every track but the first gets, for the channel in its
 *     header: all notes off (123), reset controllers (121), pitch bend to the
 *     middle, channel volume 0 (as it is, not through the master).  The game
 *     then starts the song again.
 *
 * THE DRIVER (fmmidi.com; addresses in that file):
 *   - nine voices, one chip channel each, no rhythm mode; register 0xBD is
 *     0xC0, the deep tremolo and vibrato;
 *   - the bank's instruments are packed into register bytes once (0xd37) --
 *     all but the last two in use, which stay as they are in the file;
 *   - note on (0xf6b): the first free voice, or else the first voice of the
 *     lowest channel that has never had a pitch bend (0x1ed7); that voice's
 *     note off; five times over, both operators' release to the fastest;
 *     the instrument's registers (0x201e); the second operator's level (and
 *     the first one's when the instrument adds its two operators) worked out
 *     from channel volume x velocity (below); frequency and note on (0x2227);
 *     the channel's pitch bend if it has ever had one.
 *     A drum (channel 9) takes its instrument from the drum bank by the note
 *     and plays at the pitch in that instrument's name record;
 *   - level: v = (volume x 128 / 127) x velocity >> 7, t = table[v >> 1] (64
 *     steps, 63 at v = 0 and 0 at the top), and the register gets
 *     64 - (64 - t) x (64 - the instrument's own level) / 64;
 *   - controller 7 sets the levels of the channel's playing notes again
 *     (0x237f), 0x66 is the pitch bend's range in semitones (2), 0x79 and 0x7b
 *     as usual; sustain (0x40) is in the driver and in no song, and is not
 *     here.  Program change, and a pitch bend's high byte only (0x19bb,
 *     0x1a7d: between the note's own frequency number and that of the note
 *     `range` away, by straight line).  Modulation, pan, reverb and channel
 *     pressure, which the songs are full of, are ignored: the chip is mono.
 *   Three slips of the driver's are kept: whether the FIRST operator gets a
 *   level is asked of the melodic instrument of the channel's program, also
 *   for a drum; controller 0x79 resets the velocity of the voice with the
 *   channel's NUMBER; and a note under 12 reads its frequency out of the
 *   level table.  One is not: with no voice to take, the driver uses the
 *   note's number as a voice and writes all over itself.  No song gets there
 *   (checked: none of the seven), and here the note is dropped.
 *
 * The port's own: how loud the chip is against the effects (the card's analog
 * mixer; g_volume) -- set so that the music is as loud as the port's first
 * synthesiser made it.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port.h"
#include "fmchip.h"

#define RATE      22050.0f
#define MAXTRACK  32

/* ------------------------------------------------------------- the driver */

static uint8_t d_melodic[128][30], d_drum[128][30];     /* packed */
static uint8_t d_drum_pitch[128];
static int     d_program[16], d_bend[16], d_bent[16], d_range[16], d_volume[16];
static int     d_voice_chan[9], d_voice_vel[9];
static uint8_t d_voice_note[9];                         /* 0 = free */
static uint8_t d_level[0x16], d_release[0x16];          /* an operator's 0x40 and 0x80 as the instrument has them */
static uint8_t d_a0[9], d_b0[9];
static long    d_dropped;

static const uint8_t d_ops[9][2] = { {0, 3}, {1, 4}, {2, 5}, {8, 11}, {9, 12}, {10, 13}, {16, 19}, {17, 20}, {18, 21} };

/* 0x2f00: channel volume x velocity (halved) -> what comes off the level */
static const uint8_t d_soft[64] = {
    63, 58, 53, 48, 44, 41, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,
    15, 14, 14, 13, 13, 12, 12, 11, 11, 10, 10, 9, 9, 8, 8, 7, 7, 6, 6, 6, 5, 5, 5, 4, 4, 4, 4, 3, 3, 3, 2, 2, 2, 1, 1, 0
};
/* 0x2f4c: octave and frequency number of notes 12.. (eight octaves and seven
   notes), and straight after it, at 0x30e8, the numbers the pitch bend uses
   where a bend crosses into the next octave */
static const uint16_t d_freq[] = {
    0x157, 0x16b, 0x181, 0x198, 0x1b0, 0x1ca, 0x1e5, 0x202, 0x220, 0x241, 0x263, 0x287,
    0x557, 0x56b, 0x581, 0x598, 0x5b0, 0x5ca, 0x5e5, 0x602, 0x620, 0x641, 0x663, 0x687,
    0x957, 0x96b, 0x981, 0x998, 0x9b0, 0x9ca, 0x9e5, 0xa02, 0xa20, 0xa41, 0xa63, 0xa87,
    0xd57, 0xd6b, 0xd81, 0xd98, 0xdb0, 0xdca, 0xde5, 0xe02, 0xe20, 0xe41, 0xe63, 0xe87,
    0x1157, 0x116b, 0x1181, 0x1198, 0x11b0, 0x11ca, 0x11e5, 0x1202, 0x1220, 0x1241, 0x1263, 0x1287,
    0x1557, 0x156b, 0x1581, 0x1598, 0x15b0, 0x15ca, 0x15e5, 0x1602, 0x1620, 0x1641, 0x1663, 0x1687,
    0x1957, 0x196b, 0x1981, 0x1998, 0x19b0, 0x19ca, 0x19e5, 0x1a02, 0x1a20, 0x1a41, 0x1a63, 0x1a87,
    0x1d57, 0x1d6b, 0x1d81, 0x1d98, 0x1db0, 0x1dca, 0x1de5, 0x1e02, 0x1e20, 0x1e41, 0x1e63, 0x1e87,
    0x1eae, 0x1eb7, 0x1f02, 0x1f30, 0x1f60, 0x1f94, 0x1fca,
    0x144, 0x132, 0x121, 0x110, 0x101, 0xf8, 0xe5, 0xd8, 0xcc, 0xc1, 0xb6, 0xac,
    0xa1, 0x90, 0x99, 0x86, 0x81, 0x7c, 0x73, 0x6c, 0x66, 0x60, 0x5b, 0x56,
    0x2ae, 0x2d6, 0x302, 0x330, 0x360, 0x394, 0x3de, 0x404, 0x440, 0x482, 0x4c6, 0x50e
};
#define D_FREQS  ((int)(sizeof d_freq / sizeof d_freq[0]))
#define D_BEND   103                    /* where the pitch bend's numbers start in d_freq */

/* the word the driver reads at 0x2f1c + 4 x note */
static unsigned d_note_word(int note)
{
    if (note >= 12) return note - 12 < D_FREQS ? d_freq[note - 12] : 0;
    if (note < 0 || note > 8) return 0;
    const uint8_t *b = d_soft + 28 + 4 * note;          /* the end of the level table */
    return b[0] | b[1] << 8 | b[2] << 16 | (unsigned)b[3] << 24;
}

/* 0xd37: a bank's instrument, one value a byte, into register bytes */
static void d_pack(uint8_t *r)
{
    r[0x0b] = (uint8_t)(r[0x0b] << 7 | r[0x0c] << 6 | r[0x07] << 5 | r[0x0d] << 4 | r[0x03]);
    r[0x02] = (uint8_t)(r[0x02] << 6 | r[0x0a]);
    r[0x05] = (uint8_t)(r[0x05] << 4 | r[0x08]);
    r[0x06] = (uint8_t)(r[0x06] << 4 | r[0x09]);
    r[0x0e] = (uint8_t)(r[0x04] * 2 | r[0x0e]);
    r[0x18] = (uint8_t)(r[0x18] << 7 | r[0x19] << 6 | r[0x14] << 5 | r[0x1a] << 4 | r[0x10]);
    r[0x0f] = (uint8_t)(r[0x0f] << 6 | r[0x17]);
    r[0x12] = (uint8_t)(r[0x12] << 4 | r[0x15]);
    r[0x13] = (uint8_t)(r[0x13] << 4 | r[0x16]);
}

static int d_load_bank(const char *file, uint8_t out[128][30], uint8_t *pitch)
{
    FILE *f = fopen(file, "rb");
    if (!f) return 0;
    uint8_t b[8192];
    size_t n = fread(b, 1, sizeof b, f);
    fclose(f);
    if (n < 28 || memcmp(b + 2, "ADLIB-", 6)) return 0;
    int used = (int16_t)(b[8] | b[9] << 8);
    uint32_t names = b[12] | b[13] << 8, data = b[16] | b[17] << 8;
    for (int i = 0; i < 128; i++) {
        if (data + 30 * (size_t)i + 30 > n) break;
        memcpy(out[i], b + data + 30 * i, 30);
        if (i < used - 2) d_pack(out[i]);
        if (pitch && names + 12 * (size_t)i + 12 <= n) pitch[i] = b[names + 12 * i + 2];
    }
    return 1;
}

/* 0x1c35, 0x1cb4, and the values the driver's own memory starts with */
static void d_init(void)
{
    fmchip_reset();
    memset(d_voice_note, 0, sizeof d_voice_note);
    memset(d_level, 0, sizeof d_level);
    memset(d_release, 0, sizeof d_release);
    memset(d_a0, 0, sizeof d_a0);
    memset(d_b0, 0, sizeof d_b0);
    for (int v = 0; v < 9; v++) { d_voice_chan[v] = 0; d_voice_vel[v] = 0x7f; }
    for (int c = 0; c < 16; c++) {
        d_program[c] = 0; d_bend[c] = 0x40; d_bent[c] = 0; d_range[c] = 2; d_volume[c] = 0x7f;
    }
    fmchip_write(0x01, 0x20);
    for (int v = 0; v < 9; v++) fmchip_write(0xb0 + v, 0);
    fmchip_write(0xbd, 0xc0);
}

/* 0x1f9e */
static void d_key_off(int v)
{
    if (!d_voice_note[v]) return;
    d_b0[v] &= 0xdf;
    fmchip_write(0xb0 + v, d_b0[v]);
    d_voice_note[v] = 0;
}

/* the level register of operator `op` for a voice's loudness (0xf6b, 0x237f) */
static void d_write_level(int op, unsigned loud)
{
    unsigned t = d_soft[(loud & 0xff) >> 1];
    unsigned level = (0x2000 - (0x40 - t) * 2 * (0x40 - (d_level[op] & 0x3fu))) >> 7;
    fmchip_write(0x40 + op, (int)((level & 0xff) | (d_level[op] & 0xc0u)));
}

static void d_voice_levels(int v, int chan_volume)
{
    unsigned loud = (unsigned)((chan_volume << 7) / 0x7f) * (unsigned)d_voice_vel[v] >> 7;
    if (d_melodic[d_program[d_voice_chan[v]] & 127][0x0e] & 1) d_write_level(d_ops[v][0], loud);
    d_write_level(d_ops[v][1], loud);
}

/* 0x201e */
static void d_instrument(const uint8_t *r, int v)
{
    int m = d_ops[v][0], c = d_ops[v][1];
    d_release[c] = r[0x13];
    d_release[m] = r[0x06];
    fmchip_write(0x20 + m, r[0x0b]);
    fmchip_write(0x40 + m, r[0x02]);
    d_level[m] = r[0x02];
    fmchip_write(0x60 + m, r[0x05]);
    fmchip_write(0x80 + m, r[0x06]);
    fmchip_write(0xc0 + v, r[0x0e]);
    fmchip_write(0xe0 + m, r[0x1c]);
    fmchip_write(0x20 + c, r[0x18]);
    d_level[c] = r[0x0f];
    fmchip_write(0x60 + c, r[0x12]);
    fmchip_write(0x80 + c, r[0x13]);
    fmchip_write(0xe0 + c, r[0x1d]);
}

/* 0x1a7d: the frequency word of a voice's note under a pitch bend */
static unsigned d_bend_word(unsigned bend, int note, int v)
{
    int range = d_range[d_voice_chan[v]];
    if (note < 12 || note - range < 0 || note + range - 12 >= D_FREQS || range < 0 || range > 12)
        return d_note_word(note);
    unsigned f = d_note_word(note), d;
    if (bend < 0x40) {
        d = f - d_note_word(note - range);
        if (d > 0x2cf) d = ((f & 0x3ff) - d_freq[D_BEND - 1 + range]) & 0x3ff;
        f -= d * ((0x3f - bend) * 1000 >> 6) / 1000;
    } else {
        d = d_note_word(note + range) - f;
        if (d > 0x2cf) {
            f = ((f & 0x1c00) + 0x400) | d_freq[D_BEND + 11 - (note - 12) % 12];
            d = d_note_word(note + range) - f;
        }
        f += d * ((bend - 0x40) * 1000 >> 6) / 1000;
    }
    return f;
}

/* 0x22e9 */
static void d_frequency(int v, unsigned word)
{
    d_a0[v] = (uint8_t)word;
    d_b0[v] = (uint8_t)(word >> 8) | 0x20;
    fmchip_write(0xa0 + v, d_a0[v]);
    fmchip_write(0xb0 + v, d_b0[v]);
}

/* 0xf6b */
static void d_note_on(int chan, int note, int velocity)
{
    /* 0x1ed7 */
    int v = -1;
    for (int i = 0; i < 9 && v < 0; i++) if (!d_voice_note[i]) v = i;
    for (int c = 0; c < 16 && v < 0; c++) {
        if (d_bent[c]) continue;
        for (int i = 0; i < 9 && v < 0; i++) if (d_voice_chan[i] == c) v = i;
    }
    if (v < 0) { d_dropped++; return; }

    d_key_off(v);
    d_voice_chan[v] = chan;
    for (int i = 0; i < 5; i++) {
        fmchip_write(0x80 + d_ops[v][1], d_release[d_ops[v][1]] | 0x0f);
        fmchip_write(0x80 + d_ops[v][0], d_release[d_ops[v][0]] | 0x0f);
    }
    d_instrument(chan == 9 ? d_drum[note] : d_melodic[d_program[chan] & 127], v);
    d_voice_vel[v] = velocity;
    d_voice_levels(v, d_volume[chan]);

    /* 0x2227 */
    unsigned word = d_note_word(chan == 9 ? d_drum_pitch[note] : note);
    d_a0[v] = (uint8_t)word;
    d_b0[v] = (uint8_t)(word >> 8) | 0x20;
    fmchip_write(0xa0 + v, d_a0[v]);
    fmchip_write(0xb0 + v, d_b0[v] & 0xdf);
    fmchip_write(0xb0 + v, d_b0[v]);
    d_voice_note[v] = (uint8_t)note;

    if (chan != 9 && d_bent[chan]) d_frequency(v, d_bend_word((unsigned)d_bend[chan], note, v));
}

/* 0x80d: one MIDI message */
static void d_message(int status, int a, int b)
{
    int chan = status & 15;
    a &= 127; b &= 127;
    switch (status & 0xf0) {
    case 0x90:
        if (b) { d_note_on(chan, a, b); break; }
        /* fall through */
    case 0x80:                                              /* 0x157b */
        for (int v = 0; v < 9; v++)
            if (d_voice_note[v] == a && d_voice_chan[v] == chan) d_key_off(v);
        break;
    case 0xb0:                                              /* 0x1846 */
        if (a == 7) {                                       /* 0x237f */
            d_volume[chan] = b;
            for (int v = 0; v < 9; v++)
                if (d_voice_note[v] && d_voice_chan[v] == chan) d_voice_levels(v, b);
        } else if (a == 0x66) {
            d_range[chan] = b;
        } else if (a == 0x79) {                             /* 0x174f */
            d_volume[chan] = 0x7f;
            if (chan < 9) d_voice_vel[chan] = 0x7f;
            d_bend[chan] = 0x40;
            d_range[chan] = 2;
        } else if (a == 0x7b) {                             /* 0x16e9 */
            for (int v = 0; v < 9; v++)
                if (d_voice_chan[v] == chan) { d_key_off(v); d_voice_note[v] = 0; }
        }
        break;
    case 0xc0: d_program[chan] = a; break;                  /* 0x17f9 */
    case 0xe0:                                              /* 0x19bb: the high byte only */
        if (chan == 9) break;
        d_bend[chan] = b;
        d_bent[chan] = 1;
        for (int v = 0; v < 9; v++)
            if (d_voice_note[v] && d_voice_chan[v] == chan)
                d_frequency(v, d_bend_word((unsigned)b, d_voice_note[v], v));
        break;
    default: break;
    }
}

/* ------------------------------------------------------------- the player */

typedef struct {
    const uint8_t *p, *end, *start;
    long wait;
    int  done, chan;
} track;

static int      g_banks, g_driver;
static uint8_t *g_song;
static long     g_song_len;
static track    g_track[MAXTRACK];
static int      g_ntrack, g_playing;
static float    g_tick_rate = 120.0f, g_tick_acc;
static float    g_volume = 0.6f;
static int      g_master = 96;                      /* 0xd3a54: 0..127 */
static uint8_t  g_chan_volume[16];                  /* 0x4018: controller 7 as the song last gave it */
static long     g_rounds;

/* 0xe2ca9: a message on its way to the driver */
static void send(int status, int a, int b)
{
    if ((status & 0xf0) == 0xb0 && a == 7) {
        g_chan_volume[status & 15] = (uint8_t)b;
        b = (g_master * ((127 * b) >> 7)) >> 7;
    }
    d_message(status, a, b);
}

/* 0xe37eb: a song's end, or its being stopped */
static void song_reset(void)
{
    for (int i = 1; i < g_ntrack; i++) {
        int c = g_track[i].chan & 15;
        d_message(0xb0 | c, 0x7b, 0);
        d_message(0xb0 | c, 0x79, 0);
        d_message(0xe0 | c, 0x40, 0x40);
        d_message(0xb0 | c, 7, 0);
    }
}

static long read_delay(track *t)
{
    long v = 0;
    int shift = 0;
    while (t->p < t->end) {
        int c = *t->p++;
        v |= (long)(c & 0x7f) << shift;
        shift += 7;
        if (c & 0x80) break;
    }
    return v;
}

static void song_rewind(void)
{
    for (int i = 0; i < g_ntrack; i++) {
        track *t = &g_track[i];
        t->p = t->start;
        t->done = 0;
        t->wait = read_delay(t);
    }
}

static void track_event(track *t, int number)
{
    if (t->p >= t->end) { t->done = 1; return; }
    int st = *t->p++;
    if (st == 0xff) {
        int type = t->p < t->end ? *t->p++ : 0x2f;
        long len = 0;
        while (t->p < t->end) { int c = *t->p++; len = len << 7 | (c & 0x7f); if (!(c & 0x80)) break; }
        t->p += len;
        if (type == 0x2f) t->done = 1;
        return;
    }
    if (st < 0x80) { t->done = 1; return; }          /* not a message: give the track up */
    int a = t->p < t->end ? *t->p++ : 0, b = 0;
    int kind = st & 0xf0;
    if (kind != 0xc0 && kind != 0xd0) b = t->p < t->end ? *t->p++ : 0;
    if (number) send(st, a, b);
}

static void song_tick(void)
{
    int alive = 0;
    for (int i = 0; i < g_ntrack; i++) {
        track *t = &g_track[i];
        while (!t->done && t->wait <= 0) {
            track_event(t, i);
            if (!t->done) t->wait += read_delay(t);
        }
        if (!t->done) { t->wait--; alive = 1; }
    }
    if (!alive) {                                    /* round again */
        song_reset();
        song_rewind();
        g_rounds++;
        if (getenv("OS_MUSICLOG"))
            fprintf(stderr, "music: round the song %ld times, %ld notes without a voice\n", g_rounds, d_dropped);
    }
}

/* ------------------------------------------------------------ interface */

void music_stop(void)
{
    if (g_playing) song_reset();
    g_playing = 0;
}

/* how loud the chip is in the mix: the port's own */
void music_volume(float v) { g_volume = v; fmchip_gain(2.6f * v); }

/* the game's music volume, 1 = where the game starts it (0x6000 of 0x7fff) */
void music_master(float m)
{
    int master = (int)(m * 96.0f + 0.5f);
    if (master < 0) master = 0;
    if (master > 127) master = 127;
    if (master == g_master) return;
    g_master = master;
    if (!g_playing) return;
    for (int i = 0; i < g_ntrack; i++) {             /* 0xe29d9 */
        track *t = &g_track[i];
        if (!t->done) send(0xb0 | (t->chan & 15), 7, g_chan_volume[t->chan & 15]);
    }
}

/* `dir` holds MELODIC.BNK, DRUM.BNK and the songs; number = the n of MUSICn.HMP */
int music_play(const char *dir, int number)
{
    char file[700];
    music_stop();
    if (!g_banks) {
        snprintf(file, sizeof file, "%s/MELODIC.BNK", dir);
        int a = d_load_bank(file, d_melodic, NULL);
        snprintf(file, sizeof file, "%s/DRUM.BNK", dir);
        int b = d_load_bank(file, d_drum, d_drum_pitch);
        if (!a || !b) { fprintf(stderr, "music: no instrument banks in %s\n", dir); return 0; }
        g_banks = 1;
    }
    if (!g_driver) {
        d_init();
        memset(g_chan_volume, 0x7f, sizeof g_chan_volume);
        g_driver = 1;
    }
    snprintf(file, sizeof file, "%s/MUSIC%d.HMP", dir, number);
    FILE *f = fopen(file, "rb");
    if (!f) { fprintf(stderr, "music: no %s\n", file); return 0; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc((size_t)(n > 0 ? n : 1));
    if (n < 0x388 || fread(d, 1, (size_t)n, f) != (size_t)n || memcmp(d, "HMIMIDIP013195", 14)) {
        fprintf(stderr, "music: %s is not an HMP song\n", file);
        free(d); fclose(f);
        return 0;
    }
    fclose(f);
    free(g_song);
    g_song = d; g_song_len = n;

    int tracks = d[0x30] | d[0x31] << 8;
    int rate = d[0x38] | d[0x39] << 8;
    g_tick_rate = rate > 0 ? (float)rate : 120.0f;
    g_ntrack = 0;
    long o = 0x388;
    for (int i = 0; i < tracks && i < MAXTRACK && o + 12 <= n; i++) {
        long len = d[o + 4] | d[o + 5] << 8 | d[o + 6] << 16 | (long)d[o + 7] << 24;
        if (len < 12 || o + len > n) break;
        track *t = &g_track[g_ntrack++];
        t->start = d + o + 12;
        t->end = d + o + len;
        t->chan = d[o + 8];
        o += len;
    }
    song_rewind();
    g_tick_acc = 0.0f;
    g_rounds = 0;
    g_playing = g_ntrack > 0;
    return g_playing;
}

/* one output frame at 22050 Hz, on the mixer's 16-bit scale */
void music_frame(float *left, float *right)
{
    *left = *right = 0.0f;
    if (!g_driver) return;
    if (g_playing) {
        g_tick_acc += g_tick_rate / RATE;
        while (g_tick_acc >= 1.0f) { g_tick_acc -= 1.0f; song_tick(); }
    }
    /* the chip rings on after a song is stopped, as it did */
    *left = *right = fmchip_frame() * 2.6f * g_volume;
}
