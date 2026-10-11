/*
 * sound.c - the game's sound effects, played through SDL.
 *
 * The original goes through the HMI Sound Operating System; all of that is
 * replaced here.  What is kept is what the game asks of it (0x1f708 start a
 * sample, 0x1f870 stop it, 0x1f7d4 set its pitch, 0x1f754 swap the sample a
 * voice is playing, 0x1f79c start it unless it is already playing):
 *
 *   - the samples are the .RAW files of SOUND.DAT: 8-bit unsigned, mono,
 *     11025 Hz (0x1f1eb sets the driver up with 0x2b11);
 *   - SFXSBASE.DTR in GEN.DAT lists the effects, and a sample's NUMBER in the
 *     game is its row there: 0 skidfx, 1 crash, 2 oppmotor, 3 automoto,
 *     4 wallh1, 5 hit_0, 6 click, 7 autogall, 8 dentone, 9 door, 10 taz3,
 *     11 taz4.  Each row: name, volume (low 16 bits, 0x7fff = full), loop
 *     (-1 = for ever, 0 = once, n = n times round the loop), loop start and
 *     length in samples, pitch, pan;
 *   - a pitch is 16.16: 0x10000 plays the sample at its own rate.
 */
#include "sound.h"

#include <SDL.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port.h"

#define RATE_SAMPLE 11025
#define RATE_OUT    22050
#define VOICES      32

/* music.c */
void music_frame(float *left, float *right);   /* one output frame at RATE_OUT */
int  music_play(const char *dir, int number);
void music_stop(void);
void fmchip_at(int frame);
void music_volume(float v);                    /* the chip in the mix: the port's own */
void music_master(float m);                    /* the game's music volume, 1 = as it starts */

typedef struct {
    char     name[16];
    uint8_t *data;
    int      len;
    float    volume;
    int      loop, loop_start, loop_len;
} sample;

typedef struct {
    const sample *s;
    double pos, step;
    int    loops_left;      /* -1 = for ever */
    float  gain;            /* set by the game per voice: distance */
    float  pan;             /* -1 left .. 0 middle .. +1 right */
    int    serial;          /* identifies this start, so a stale handle does nothing */
} voice;

static sample g_sample[SND_MAX];
static int    g_nsample;
static voice  g_voice[VOICES];
static int    g_serial = 1;
static int    g_open;
static FILE  *g_wav;
static long   g_wav_frames;
static char   g_dir[512];

static uint8_t *load_raw(const char *name, int *len)
{
    char file[700], up[32];
    int i = 0;
    for (; name[i] && i < 31; i++) up[i] = (char)toupper((unsigned char)name[i]);
    up[i] = 0;
    snprintf(file, sizeof file, "%s/%s.RAW", g_dir, up);
    FILE *f = fopen(file, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc((size_t)(n > 0 ? n : 1));
    if (n <= 0 || fread(d, 1, (size_t)n, f) != (size_t)n) { free(d); fclose(f); return NULL; }
    fclose(f);
    *len = (int)n;
    return d;
}

int snd_add(const char *name, float volume, int loop, int loop_start, int loop_len)
{
    if (g_nsample >= SND_MAX) return -1;
    sample *s = &g_sample[g_nsample];
    memset(s, 0, sizeof *s);
    snprintf(s->name, sizeof s->name, "%s", name);
    s->data = load_raw(name, &s->len);
    s->volume = volume;
    s->loop = loop;
    s->loop_start = loop_start;
    s->loop_len = loop_len;
    if (!s->data) fprintf(stderr, "sound: no sample %s\n", name);
    return g_nsample++;
}

int snd_find(const char *name)
{
    for (int i = 0; i < g_nsample; i++)
        if (!strcasecmp(g_sample[i].name, name)) return i;
    return -1;
}

/*
 * Two channels.  The game gives HMI a pan of 0..0xffff per voice (0x8000 =
 * middle; 0xc6670); how HMI turns that into two volumes is inside the driver
 * that is not kept, so the law here is the port's own: the middle is full on
 * both sides and a voice fades out of the far side linearly.
 */
/*
 * THE EFFECTS ARE MIXED AS THE GAME'S DRIVER MIXED THEM (2026-10-09, after
 * Danik found the engines quiet and muffled beside the original's).
 *
 *  - The game opens its digital driver at 11025 Hz (0x1f1eb: 0x2b11) and the
 *    samples are 8-bit.  A voice played off its own pitch is stepped through
 *    by whole samples, nothing smoothed between them -- that roughness is the
 *    brightness of the original's engines.  The port used to mix at 22050 Hz
 *    and slide from each sample to the next, which is a low-pass filter: the
 *    muffling.  Now an effects sample is worked out 11025 times a second
 *    from the nearest stored sample and held for two output frames.
 *  - Loudness: a sample at full volume fills the driver's 8-bit range, and
 *    the game's master volume for effects starts at 0x6000 of 0x7fff (the
 *    same number as the music's and the speech's: 0x1bc2f).  So one sample
 *    step is 256 x 0.75 = 192 of the 16-bit output; the port had 80 "for
 *    headroom".  Several loud voices together saturate, as 8 bits did.
 *  OS_SFXVOL=x scales it (1 = as above).
 */
/*
 * THE CARD'S OUTPUT FILTER (2026-10-11, after Danik found every effect brighter
 * and thinner than the original's -- the aeroplane "like a phone speaker").
 * Holding each 11025 Hz value for two output frames, as the port did for two
 * days, is a converter with nothing after it: every sound comes with a copy
 * of itself mirrored above 5.5 kHz, almost as loud.  No PC sound card let
 * that out.  A Sound Blaster Pro has a low-pass filter at 3.2 kHz on its
 * output, on by default; later cards and the emulators cut at or under half
 * the playing rate as well.  So the effects (not the music: that was the
 * card's FM chip, another path) go through a two-pole low-pass at 3.2 kHz
 * here.  The roughness of a voice stepped through by whole samples is still
 * made at 11025 Hz, before the filter, as in the driver.
 * OS_SFXLP=<Hz> moves the cut (0 = no filter, the port of 2026-10-09).
 */
static float g_sfx_gain = 192.0f, g_sfx_base = 192.0f, g_music_base = 0.6f;
static float g_lp_b0 = 1.0f, g_lp_b1, g_lp_b2, g_lp_a1, g_lp_a2;      /* the filter: one stage, as it comes */

static void lowpass_set(float hz)
{
    if (hz <= 0.0f || hz >= RATE_OUT * 0.45f) { g_lp_b0 = 1.0f; g_lp_b1 = g_lp_b2 = g_lp_a1 = g_lp_a2 = 0.0f; return; }
    const float w = 2.0f * 3.14159265f * hz / (float)RATE_OUT, cs = cosf(w), al = sinf(w) / 1.41421356f;
    const float a0 = 1.0f + al;
    g_lp_b0 = (1.0f - cs) * 0.5f / a0; g_lp_b1 = (1.0f - cs) / a0; g_lp_b2 = g_lp_b0;
    g_lp_a1 = -2.0f * cs / a0; g_lp_a2 = (1.0f - al) / a0;
}

static void mix(int16_t *out, int frames)
{
    static float holdl, holdr;
    static int   phase;
    for (int i = 0; i < frames; i++) {
        if (!(phase++ & 1)) {
            float accl = 0.0f, accr = 0.0f;
            for (int v = 0; v < VOICES; v++) {
                voice *vo = &g_voice[v];
                const sample *s = vo->s;
                if (!s) continue;
                int end = (s->loop && s->loop_len > 0) ? s->loop_start + s->loop_len : s->len;
                if (end > s->len) end = s->len;
                if (vo->pos >= (double)end) {
                    if (vo->loops_left == 0) { vo->s = NULL; continue; }
                    if (vo->loops_left > 0) vo->loops_left--;
                    double start = (s->loop_len > 0) ? (double)s->loop_start : 0.0;
                    vo->pos = start + (vo->pos - (double)end);
                    if (vo->pos >= (double)end) vo->pos = start;
                }
                float x = ((float)s->data[(int)vo->pos] - 128.0f) * s->volume * vo->gain;
                accl += x * (vo->pan > 0.0f ? 1.0f - vo->pan : 1.0f);
                accr += x * (vo->pan < 0.0f ? 1.0f + vo->pan : 1.0f);
                vo->pos += vo->step * 2.0;       /* step is per output frame; this runs every second one */
            }
            holdl = accl * g_sfx_gain;
            holdr = accr * g_sfx_gain;
        }
        static float zl[2], zr[2];                  /* the output filter's memory, per side */
        float accl = g_lp_b0 * holdl + zl[0], accr = g_lp_b0 * holdr + zr[0];
        zl[0] = g_lp_b1 * holdl - g_lp_a1 * accl + zl[1]; zl[1] = g_lp_b2 * holdl - g_lp_a2 * accl;
        zr[0] = g_lp_b1 * holdr - g_lp_a1 * accr + zr[1]; zr[1] = g_lp_b2 * holdr - g_lp_a2 * accr;
        float ml = 0.0f, mr = 0.0f;
        fmchip_at(2 * i);
        music_frame(&ml, &mr);
        accl += ml; accr += mr;
        if (accl > 32767.0f) accl = 32767.0f;
        if (accl < -32768.0f) accl = -32768.0f;
        if (accr > 32767.0f) accr = 32767.0f;
        if (accr < -32768.0f) accr = -32768.0f;
        out[2 * i] = (int16_t)accl;
        out[2 * i + 1] = (int16_t)accr;
    }
}

static void callback(void *user, Uint8 *stream, int len)
{
    (void)user;
    mix((int16_t *)stream, len / 4);
}

static void lock(void)   { if (g_open) SDL_LockAudio(); }
static void unlock(void) { if (g_open) SDL_UnlockAudio(); }

int snd_init(const char *sound_dir, const char *list_file)
{
    snprintf(g_dir, sizeof g_dir, "%s", sound_dir);
    g_nsample = 0;
    if (getenv("OS_SFXVOL")) g_sfx_base = 192.0f * (float)atof(getenv("OS_SFXVOL"));
    if (getenv("OS_MUSICVOL")) g_music_base = (float)atof(getenv("OS_MUSICVOL"));
    g_sfx_gain = g_sfx_base;
    music_volume(g_music_base);
    memset(g_voice, 0, sizeof g_voice);
    lowpass_set(getenv("OS_SFXLP") ? (float)atof(getenv("OS_SFXLP")) : 3200.0f);

    FILE *f = fopen(list_file, "r");
    if (f) {
        char line[256];
        int in = 0;
        while (fgets(line, sizeof line, f)) {
            char *p = line;
            while (*p == ' ' || *p == '\t') p++;
            if (!in) { if (!strncmp(p, "SOUND_FX", 8)) in = 1; continue; }
            if (!strncmp(p, "END_LIST", 8)) break;
            char name[32];
            unsigned vol = 0;
            int loop = 0, ls = 0, ll = 0;
            if (sscanf(p, "s%31s x%x d%d d%d d%d", name, &vol, &loop, &ls, &ll) == 5)
                snd_add(name, (float)(vol & 0xffff) / 32767.0f, loop, ls, ll);
        }
        fclose(f);
    }

    if (getenv("OS_NOSOUND")) return g_nsample;
    if (getenv("OS_SNDWAV")) {               /* no device: the mix goes to a file, for testing */
        g_wav = fopen(getenv("OS_SNDWAV"), "wb");
        if (g_wav) { uint8_t hdr[44] = { 0 }; fwrite(hdr, 1, 44, g_wav); }
        return g_nsample;
    }
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "sound: %s\n", SDL_GetError());
        return g_nsample;
    }
    SDL_AudioSpec want;
    memset(&want, 0, sizeof want);
    want.freq = RATE_OUT;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = callback;
    if (SDL_OpenAudio(&want, NULL) != 0) {
        fprintf(stderr, "sound: %s\n", SDL_GetError());
        return g_nsample;
    }
    g_open = 1;
    SDL_PauseAudio(0);
    return g_nsample;
}

/* testing: advance the mix by `seconds` into the WAV file */
void snd_render(float seconds)
{
    if (!g_wav) return;
    static int16_t buf[8192];
    int frames = (int)(seconds * RATE_OUT + 0.5f);
    while (frames > 0) {
        int n = frames > 4096 ? 4096 : frames;
        mix(buf, n);
        fwrite(buf, 4, (size_t)n, g_wav);
        g_wav_frames += n;
        frames -= n;
    }
}

void snd_close(void)
{
    if (g_wav) {
        uint32_t bytes = (uint32_t)g_wav_frames * 4, v;
        uint16_t w;
        fseek(g_wav, 0, SEEK_SET);
        fwrite("RIFF", 1, 4, g_wav); v = 36 + bytes; fwrite(&v, 4, 1, g_wav);
        fwrite("WAVEfmt ", 1, 8, g_wav); v = 16; fwrite(&v, 4, 1, g_wav);
        w = 1; fwrite(&w, 2, 1, g_wav); w = 2; fwrite(&w, 2, 1, g_wav);
        v = RATE_OUT; fwrite(&v, 4, 1, g_wav); v = RATE_OUT * 4; fwrite(&v, 4, 1, g_wav);
        w = 4; fwrite(&w, 2, 1, g_wav); w = 16; fwrite(&w, 2, 1, g_wav);
        fwrite("data", 1, 4, g_wav); fwrite(&bytes, 4, 1, g_wav);
        fclose(g_wav);
        g_wav = NULL;
    }
    if (g_open) { SDL_CloseAudio(); g_open = 0; }
}

static voice *voice_of(int handle)
{
    if (handle <= 0) return NULL;
    voice *vo = &g_voice[handle % VOICES];
    return (vo->s && vo->serial == handle) ? vo : NULL;
}

/* 0x1f708 */
int snd_play(int id)
{
    if (id < 0 || id >= g_nsample || !g_sample[id].data) return -1;
    lock();
    int slot = -1;
    for (int v = 0; v < VOICES; v++)
        if (!g_voice[v].s) { slot = v; break; }
    int handle = -1;
    if (slot >= 0) {
        voice *vo = &g_voice[slot];
        /* a handle is a serial number whose remainder is the slot */
        g_serial += VOICES;
        handle = g_serial - (g_serial % VOICES) + slot;
        vo->s = &g_sample[id];
        vo->pos = 0.0;
        vo->step = (double)RATE_SAMPLE / RATE_OUT;
        vo->loops_left = g_sample[id].loop;
        vo->gain = 1.0f;
        vo->pan = 0.0f;
        vo->serial = handle;
    }
    unlock();
    return handle;
}

/* 0x1f870: returns -1, the "no voice" handle, like the game's */
int snd_stop(int handle)
{
    lock();
    voice *vo = voice_of(handle);
    if (vo) vo->s = NULL;
    unlock();
    return -1;
}

int snd_playing(int handle)
{
    lock();
    int on = voice_of(handle) != NULL;
    unlock();
    return on;
}

/*
 * 0x1f7d4: pitch is 16.16, 0x10000 = the sample as recorded.  READ OUT OF THE
 * DRIVER (2026-10-11): the game stores the number in the driver's voice
 * (+0x44) and the mixers in HMIDRV.386 (its tr8m / tr8st / tr16m / tr16st
 * modules, picked by the sound card's driver) step through the sample with
 *     sample = data[index];  fraction += low word;  index += high word + carry
 * -- the nearest stored sample, nothing smoothed, as here.  The same code
 * shows the rest of the mix is as here too: 8-bit data, times the volume,
 * added into the card's buffer and held at its limits; pan = one side full,
 * the other fading in a straight line.
 */
void snd_pitch(int handle, int pitch)
{
    if (pitch < 1) pitch = 1;
    lock();
    voice *vo = voice_of(handle);
    if (vo) vo->step = (double)pitch / 65536.0 * RATE_SAMPLE / RATE_OUT;
    unlock();
}

/* the voice's own loudness, 0..1 (the game sets 0..32767 through HMI) */
void snd_volume(int handle, float gain)
{
    lock();
    voice *vo = voice_of(handle);
    if (vo) vo->gain = gain < 0.0f ? 0.0f : gain > 1.0f ? 1.0f : gain;
    unlock();
}

/* the music: MUSIC<number>.HMP from `dir`, round and round; 0 stops it */
int snd_music(const char *dir, int number)
{
    if (!g_open && !g_wav) return 0;
    lock();
    int ok = 0;
    if (number > 0) {

            ok = music_play(dir, number);
    } else {
        music_stop();
    }
    unlock();
    return ok;
}

/* 0xc6670: where the voice sits between the speakers, -1 left .. +1 right */
void snd_pan(int handle, float pan)
{
    lock();
    voice *vo = voice_of(handle);
    if (vo) vo->pan = pan < -1.0f ? -1.0f : pan > 1.0f ? 1.0f : pan;
    unlock();
}

/* load every row of a SOUND_FX list; returns the number of its first sample */
int snd_add_list(const char *list_file)
{
    int first = g_nsample;
    FILE *f = fopen(list_file, "r");
    if (!f) return first;
    char line[256];
    int in = 0;
    while (fgets(line, sizeof line, f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (!in) { if (!strncmp(p, "SOUND_FX", 8)) in = 1; continue; }
        if (!strncmp(p, "END_LIST", 8)) break;
        char name[32];
        unsigned vol = 0;
        int loop = 0, ls = 0, ll = 0;
        if (sscanf(p, "s%31s x%x d%d d%d d%d", name, &vol, &loop, &ls, &ll) == 5)
            snd_add(name, (float)(vol & 0xffff) / 32767.0f, loop, ls, ll);
    }
    fclose(f);
    return first;
}

/* 0x1f754: the same voice carries on with another sample */
void snd_swap(int handle, int id)
{
    if (id < 0 || id >= g_nsample || !g_sample[id].data) return;
    lock();
    voice *vo = voice_of(handle);
    if (vo && vo->s != &g_sample[id]) {
        vo->s = &g_sample[id];
        vo->pos = 0.0;
        vo->loops_left = g_sample[id].loop;
    }
    unlock();
}

/* 0x1f79c: start it, unless this handle is already playing it */
int snd_play_once(int id, int handle)
{
    lock();
    voice *vo = voice_of(handle);
    int same = vo && id >= 0 && id < g_nsample && vo->s == &g_sample[id];
    unlock();
    return same ? handle : snd_play(id);
}

/* the game's master volumes (0x1f93c): 1 = where the game starts them, 0x6000 of 0x7fff */
void snd_masters(float effects, float music)
{
    lock();
    g_sfx_gain = g_sfx_base * effects;
    music_volume(g_music_base);
    music_master(music);
    unlock();
}
