/*
 * music.c - the game's MIDI music (MUSICn.HMP) on its FM instrument banks.
 *
 * The original hands the song and the two banks to HMI's FM driver
 * (HMIMDRV.386), which plays them on the sound card's OPL chip.  The driver
 * is not kept; this file is a small stand-in for both it and the chip.
 *
 * From the game's files:
 *   MUSICn.HMP   "HMIMIDIP013195": +0x30 number of tracks, +0x38 ticks per
 *                second (120); the tracks start at 0x388, each
 *                [number, length including these 12 bytes, channel] + events.
 *                An event is a delay then a MIDI message; the delay is 7 bits
 *                a byte, LOW part first, and the LAST byte has its top bit
 *                set (the reverse of a MIDI file).  FF 2F ends a track.
 *   MELODIC.BNK  AdLib instrument bank, 128 instruments = the MIDI programs:
 *   DRUM.BNK     the same, 128 = the drum notes of MIDI channel 10; the third
 *                byte of a drum's name record is the pitch it is played at.
 *                Bank: +0x0c offset of the name records (12 bytes: index,
 *                flag/pitch, name), +0x10 offset of the instruments (30
 *                bytes: 2, then 13 for each of the two operators -- key
 *                scaling, multiplier, feedback, attack, sustain level,
 *                sustaining, decay, release, level, tremolo, vibrato, rate
 *                scaling, connection -- then the two wave shapes).
 *
 * The port's own (the driver's choices are not read): how loudness and MIDI
 * volume become operator levels, 18 voices, smooth panning, the note stolen
 * when all voices are busy.  The connection byte is taken as the chip's own
 * bit (0 = the first operator modulates the second), which is what makes 112
 * of the 128 instruments FM; OS_MUSIC_CON=1 reads it the other way round.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port.h"

#define RATE      22050.0f
#define OVER      2                 /* the synthesiser runs at twice the output rate */
#define NVOICE    18
#define MAXTRACK  32
#define SINE_N    4096

typedef struct {
    int ksl, mult, fb, ar, sl, sustaining, dr, rr, tl, am, vib, ksr, con, wave;
} op_patch;
typedef struct { op_patch op[2]; int pitch; } patch;

enum { ENV_OFF, ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE };
typedef struct {
    const op_patch *p;
    float phase, step;              /* in turns */
    float att;                      /* envelope, dB down */
    float base;                     /* level + key scaling + loudness, dB down */
    float d_attack, d_decay, d_release;
    int   state;
    float out1, out2;               /* last two outputs, for feedback */
} oper;
typedef struct {
    oper  op[2];
    int   on, chan, note, held;
    long  age;
    float freq;                     /* of the note, before the bend */
} fm_voice;

typedef struct {
    const uint8_t *p, *end, *start;
    long wait;
    int  done;
} track;

static patch    g_melodic[128], g_drum[128];
static int      g_banks;
static uint8_t *g_song;
static long     g_song_len;
static track    g_track[MAXTRACK];
static int      g_ntrack, g_playing, g_flip_con;
static float    g_tick_rate = 120.0f, g_tick_acc;
static fm_voice g_voice[NVOICE];
static long     g_age;
static float    g_sine[SINE_N];
static float    g_amp[96 * 8 + 1];          /* dB down (eighths) -> amplitude */
static float    g_fb[8];                    /* feedback setting -> turns of phase */
static float    g_lfo_am, g_lfo_vib;        /* phases, turns */
static float    g_volume = 0.6f;            /* under the engine; the game takes its own from SCREAMER.CFG */
static struct { int program, volume, expression, pan; float bend; } g_chan[16];

static const float g_mult[16] = { 0.5f, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 12, 12, 15, 15 };

/* ------------------------------------------------------------------ banks */

static int load_bank(const char *file, patch *out, int drums)
{
    FILE *f = fopen(file, "rb");
    if (!f) return 0;
    uint8_t b[8192];
    size_t n = fread(b, 1, sizeof b, f);
    fclose(f);
    if (n < 28 || memcmp(b + 2, "ADLIB-", 6)) return 0;
    uint32_t names = b[12] | b[13] << 8, data = b[16] | b[17] << 8;
    for (int i = 0; i < 128; i++) {
        const uint8_t *nr = b + names + 12 * i;
        if (names + 12 * (size_t)i + 12 > n) break;
        int idx = nr[0] | nr[1] << 8;
        const uint8_t *r = b + data + 30 * idx;
        if (data + 30 * (size_t)idx + 30 > n) continue;
        for (int o = 0; o < 2; o++) {
            const uint8_t *q = r + 2 + 13 * o;
            op_patch *p = &out[i].op[o];
            p->ksl = q[0] & 3;  p->mult = q[1] & 15; p->fb = q[2] & 7;
            p->ar = q[3] & 15;  p->sl = q[4] & 15;   p->sustaining = q[5] & 1;
            p->dr = q[6] & 15;  p->rr = q[7] & 15;   p->tl = q[8] & 63;
            p->am = q[9] & 1;   p->vib = q[10] & 1;  p->ksr = q[11] & 1;
            p->con = q[12] & 1; p->wave = r[28 + o] & 3;
        }
        out[i].pitch = drums ? nr[2] : -1;
    }
    return 1;
}

/* ------------------------------------------------------------- the voices */

static float wave(int shape, float phase)
{
    phase -= floorf(phase);
    float s = g_sine[(int)(phase * SINE_N) & (SINE_N - 1)];
    switch (shape) {
    case 1:  return s > 0.0f ? s : 0.0f;                       /* half sine */
    case 2:  return fabsf(s);                                  /* rectified */
    case 3:  return (phase < 0.25f || (phase >= 0.5f && phase < 0.75f)) ? fabsf(s) : 0.0f;
    default: return s;
    }
}

/* the chip's key scaling: dB down, growing with pitch (rows of 3 dB/octave) */
static float key_scale_db(int fnum, int block, int ksl)
{
    static const int rom[16] = { 0, 32, 40, 45, 48, 51, 53, 55, 56, 58, 59, 60, 61, 62, 63, 64 };
    static const int shift[4] = { 8, 1, 2, 0 };
    int k = (rom[fnum >> 6] << 2) - ((8 - block) << 5);
    if (k < 0) k = 0;
    return (float)(k >> shift[ksl]) * 0.1875f;
}

static void op_start(oper *o, const op_patch *p, float freq, float extra_db)
{
    /* the chip's own frequency number and octave, for its rate and key scaling */
    int block = 0;
    float fn = freq * 1048576.0f / 49716.0f;
    while (fn >= 1024.0f && block < 7) { fn *= 0.5f; block++; }
    int fnum = fn > 1023.0f ? 1023 : (int)fn;
    int keycode = block * 2 + (fnum >> 9);
    int ks = p->ksr ? keycode : keycode >> 2;

    o->p = p;
    o->phase = 0.0f;
    o->step = freq * g_mult[p->mult] / (RATE * OVER);
    o->base = (float)p->tl * 0.75f + key_scale_db(fnum, block, p->ksl) + extra_db;
    o->out1 = o->out2 = 0.0f;

    /* rates: 0..100% in 2.826 s (attack) / 39.28 s (decay, release) at rate
       1, halving with every step, a quarter step per unit of rate scaling */
    const float dt = 1.0f / (RATE * OVER);
    int ra = p->ar ? p->ar * 4 + ks : 0, rd = p->dr ? p->dr * 4 + ks : 0, rl = p->rr ? p->rr * 4 + ks : 0;
    if (ra > 63) ra = 63;
    if (rd > 63) rd = 63;
    if (rl > 63) rl = 63;
    o->d_attack  = ra ? (ra >= 60 ? 1.0f : 1.0f - expf(-4.56f * dt / (2.826f * powf(2.0f, 1.0f - ra * 0.25f)))) : 0.0f;
    o->d_decay   = rd ? 96.0f * dt / (39.28f * powf(2.0f, 1.0f - rd * 0.25f)) : 0.0f;
    o->d_release = rl ? 96.0f * dt / (39.28f * powf(2.0f, 1.0f - rl * 0.25f)) : 0.0f;
    o->att = 96.0f;
    o->state = ENV_ATTACK;
}

static float op_run(oper *o, float mod, float bend, float trem, float vib)
{
    const op_patch *p = o->p;
    switch (o->state) {
    case ENV_ATTACK:
        o->att -= o->att * o->d_attack;
        if (o->att < 0.2f) { o->att = 0.0f; o->state = ENV_DECAY; }
        break;
    case ENV_DECAY: {
        float sl = p->sl == 15 ? 93.0f : (float)p->sl * 3.0f;
        o->att += o->d_decay;
        if (o->att >= sl) { o->att = sl; o->state = p->sustaining ? ENV_SUSTAIN : ENV_RELEASE; }
        break;
    }
    case ENV_RELEASE:
        o->att += o->d_release;
        if (o->att >= 96.0f) { o->att = 96.0f; o->state = ENV_OFF; }
        break;
    default: break;
    }
    if (o->state == ENV_OFF) return 0.0f;
    float db = o->base + o->att + (p->am ? trem : 0.0f);
    float amp = db >= 96.0f ? 0.0f : g_amp[(int)(db * 8.0f)];
    o->phase += o->step * bend * (p->vib ? vib : 1.0f);
    if (o->phase >= 1.0f) o->phase -= 1.0f;
    return wave(p->wave, o->phase + mod) * amp;
}

static float chan_gain_db(int chan, int velocity)
{
    float g = (float)velocity / 127.0f * (float)g_chan[chan].volume / 127.0f
            * (float)g_chan[chan].expression / 127.0f;
    if (g < 0.001f) return 96.0f;
    return -20.0f * log10f(g);
}

static void note_off(int chan, int note)
{
    for (int v = 0; v < NVOICE; v++) {
        fm_voice *vo = &g_voice[v];
        if (vo->on && vo->held && vo->chan == chan && vo->note == note) {
            vo->held = 0;
            for (int o = 0; o < 2; o++)
                if (vo->op[o].state != ENV_OFF) vo->op[o].state = ENV_RELEASE;
        }
    }
}

static void note_on(int chan, int note, int velocity)
{
    const patch *p = chan == 9 ? &g_drum[note & 127] : &g_melodic[g_chan[chan].program & 127];
    int pitch = chan == 9 ? p->pitch : note;
    if (pitch < 0) return;
    note_off(chan, note);

    /* a free voice, or else the oldest one that has been let go, or the oldest */
    int pick = -1;
    long best = 0;
    for (int v = 0; v < NVOICE && pick < 0; v++) if (!g_voice[v].on) pick = v;
    if (pick < 0) {
        for (int v = 0; v < NVOICE; v++) {
            long score = (g_age - g_voice[v].age) + (g_voice[v].held ? 0 : 1000000);
            if (pick < 0 || score > best) { pick = v; best = score; }
        }
    }
    fm_voice *vo = &g_voice[pick];
    vo->on = 1; vo->held = 1; vo->chan = chan; vo->note = note; vo->age = g_age++;
    vo->freq = 440.0f * powf(2.0f, ((float)pitch - 69.0f) / 12.0f);
    float loud = chan_gain_db(chan, velocity);
    int additive = p->op[0].con ^ g_flip_con;
    op_start(&vo->op[0], &p->op[0], vo->freq, additive ? loud : 0.0f);
    op_start(&vo->op[1], &p->op[1], vo->freq, loud);
}

static void all_off(void)
{
    memset(g_voice, 0, sizeof g_voice);
    for (int c = 0; c < 16; c++) {
        g_chan[c].program = 0; g_chan[c].volume = 100; g_chan[c].expression = 127;
        g_chan[c].pan = 64; g_chan[c].bend = 1.0f;
    }
}

/* ------------------------------------------------------------- the song */

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

static void track_event(track *t)
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
    int chan = st & 15, a = t->p < t->end ? *t->p++ : 0, b = 0;
    int kind = st & 0xf0;
    if (kind != 0xc0 && kind != 0xd0) b = t->p < t->end ? *t->p++ : 0;
    switch (kind) {
    case 0x80: note_off(chan, a); break;
    case 0x90: if (b) note_on(chan, a, b); else note_off(chan, a); break;
    case 0xb0:
        if (a == 7) g_chan[chan].volume = b;
        else if (a == 11) g_chan[chan].expression = b;
        else if (a == 10) g_chan[chan].pan = b;
        else if (a == 123 || a == 120)
            for (int v = 0; v < NVOICE; v++) if (g_voice[v].chan == chan) note_off(chan, g_voice[v].note);
        break;
    case 0xc0: g_chan[chan].program = a; break;
    case 0xe0: g_chan[chan].bend = powf(2.0f, ((float)((b << 7 | a) - 8192) / 8192.0f) * 2.0f / 12.0f); break;
    default: break;
    }
}

static void song_tick(void)
{
    int alive = 0;
    for (int i = 0; i < g_ntrack; i++) {
        track *t = &g_track[i];
        while (!t->done && t->wait <= 0) {
            track_event(t);
            if (!t->done) t->wait += read_delay(t);
        }
        if (!t->done) { t->wait--; alive = 1; }
    }
    if (!alive) song_rewind();                        /* round again */
}

/* ------------------------------------------------------------ interface */

void music_stop(void)
{
    g_playing = 0;
    all_off();
}

void music_volume(float v) { g_volume = v; }

/* `dir` holds MELODIC.BNK, DRUM.BNK and the songs; number = the n of MUSICn.HMP */
int music_play(const char *dir, int number)
{
    char file[700];
    music_stop();
    if (!g_sine[SINE_N / 4]) {
        for (int i = 0; i < SINE_N; i++) g_sine[i] = sinf((float)i * 6.2831853f / SINE_N);
        for (int i = 0; i <= 96 * 8; i++) g_amp[i] = powf(10.0f, -(float)i / 160.0f);
        for (int i = 1; i < 8; i++) g_fb[i] = 3.99f * powf(2.0f, (float)i - 9.0f);
    }
    g_flip_con = getenv("OS_MUSIC_CON") ? 1 : 0;
    if (!g_banks) {
        snprintf(file, sizeof file, "%s/MELODIC.BNK", dir);
        int a = load_bank(file, g_melodic, 0);
        snprintf(file, sizeof file, "%s/DRUM.BNK", dir);
        int b = load_bank(file, g_drum, 1);
        if (!a || !b) { fprintf(stderr, "music: no instrument banks in %s\n", dir); return 0; }
        g_banks = 1;
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
        o += len;
    }
    all_off();
    song_rewind();
    g_tick_acc = 0.0f;
    g_playing = g_ntrack > 0;
    return g_playing;
}

/* one output frame at 22050 Hz, on the mixer's 16-bit scale */
void music_frame(float *left, float *right)
{
    *left = *right = 0.0f;
    if (!g_playing) return;

    g_tick_acc += g_tick_rate / RATE;
    while (g_tick_acc >= 1.0f) { g_tick_acc -= 1.0f; song_tick(); }

    /* the chip's two slow wobbles: tremolo 3.7 Hz, 1 dB; vibrato 6.1 Hz, 7 cents */
    g_lfo_am += 3.7f / RATE;   if (g_lfo_am >= 1.0f) g_lfo_am -= 1.0f;
    g_lfo_vib += 6.1f / RATE;  if (g_lfo_vib >= 1.0f) g_lfo_vib -= 1.0f;
    float tri = g_lfo_am < 0.5f ? g_lfo_am * 2.0f : 2.0f - g_lfo_am * 2.0f;
    float trem = tri * 1.0f;
    float vib = 1.0f + g_sine[(int)(g_lfo_vib * SINE_N) & (SINE_N - 1)] * 0.004f;

    float l = 0.0f, r = 0.0f;
    for (int v = 0; v < NVOICE; v++) {
        fm_voice *vo = &g_voice[v];
        if (!vo->on) continue;
        oper *m = &vo->op[0], *c = &vo->op[1];
        float bend = g_chan[vo->chan].bend;
        int additive = m->p->con ^ g_flip_con;
        float acc = 0.0f;
        for (int k = 0; k < OVER; k++) {
            /* feedback: the average of the operator's last two outputs, up to
               +-2 turns of phase at the top setting; a full-scale modulator
               swings the second operator's phase by +-4 turns */
            float fb = (m->out1 + m->out2) * g_fb[m->p->fb];
            float mo = op_run(m, fb, bend, trem, vib);
            m->out2 = m->out1; m->out1 = mo;
            if (additive) acc += mo + op_run(c, 0.0f, bend, trem, vib);
            else          acc += op_run(c, mo * 3.99f, bend, trem, vib);
        }
        acc /= OVER;
        if (c->state == ENV_OFF && (!additive || m->state == ENV_OFF)) { vo->on = 0; continue; }
        float pan = ((float)g_chan[vo->chan].pan - 64.0f) / 64.0f;
        l += acc * (pan > 0.0f ? 1.0f - pan : 1.0f);
        r += acc * (pan < 0.0f ? 1.0f + pan : 1.0f);
    }
    /* the half-wave shapes are lopsided: take the standing offset out */
    static float dl, dr;
    dl += (l - dl) * 0.002f;
    dr += (r - dr) * 0.002f;
    *left = (l - dl) * 5000.0f * g_volume;
    *right = (r - dr) * 5000.0f * g_volume;
}
