/*
 * THE CD'S SONGS, AS FILES.  Screamer 2 plays its music from the disc's audio tracks (the CD
 * driver's "play track n", 0x4d5b4; the menu song 2, a race the song MISC.DTR's CD_TRACKS has
 * for its track, both round and round).  The port has no disc: track n is a file
 *
 *     track_NN.ogg / track_NN.mp3 / track_NN.wav     (NN = 02 .. 99, two digits)
 *
 * looked for in the game's directory's "music" folder, then in the game's directory, then in
 * the port's home's "music" folder (the first found, in that order of folders, then of kinds).
 * The player makes them from the disc's own image once (they are the game's: never published).
 *
 * The song is read as it plays, in the mixer's thread (sound.c, under its lock): decoded a few
 * thousand frames at a time, each output frame the average of the song's frames it spans (the
 * mixer runs at 22050 a second, a CD at 44100), mono doubled to both sides.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "port.h"
#include "sound.h"

#define STB_VORBIS_HEADER_ONLY
#include "third_party/stb_vorbis.c"
#include "third_party/dr_mp3.h"
#include "third_party/dr_wav.h"

enum { CD_NONE, CD_OGG, CD_MP3, CD_WAV };
static struct {
    int kind, loop, track;
    stb_vorbis *ogg;
    drmp3 mp3;
    drwav wav;
    int channels, rate;
    int16_t buf[4096 * 2];          /* decoded, as two sides */
    int have, at;                   /* frames in buf, the next one */
    float frac, step;               /* how far into frame `at`; song frames an output frame */
    float gain;
    int paused;                     /* (the race's CD player: PLAY again goes on from here) */
} g_cd;
static float g_cd_volume = 1.0f;    /* the CD's own volume (0x4d7c0: 0..255 of each side, here 0..1) */

static int cd_open_file(const char *path)
{
    const char *dot = strrchr(path, '.');
    if (!dot) return 0;
    if (!strcmp(dot, ".ogg")) {
        int err = 0;
        g_cd.ogg = stb_vorbis_open_filename(path, &err, NULL);
        if (!g_cd.ogg) return 0;
        const stb_vorbis_info in = stb_vorbis_get_info(g_cd.ogg);
        g_cd.kind = CD_OGG; g_cd.channels = in.channels; g_cd.rate = (int)in.sample_rate;
    } else if (!strcmp(dot, ".mp3")) {
        if (!drmp3_init_file(&g_cd.mp3, path, NULL)) return 0;
        g_cd.kind = CD_MP3; g_cd.channels = (int)g_cd.mp3.channels; g_cd.rate = (int)g_cd.mp3.sampleRate;
    } else if (!strcmp(dot, ".wav")) {
        if (!drwav_init_file(&g_cd.wav, path, NULL)) return 0;
        g_cd.kind = CD_WAV; g_cd.channels = (int)g_cd.wav.channels; g_cd.rate = (int)g_cd.wav.sampleRate;
    } else return 0;
    if (g_cd.channels < 1 || g_cd.rate < 1000) { cd_audio_stop(); return 0; }
    return 1;
}

void cd_audio_stop(void)
{
    if (g_cd.kind == CD_OGG && g_cd.ogg) stb_vorbis_close(g_cd.ogg);
    if (g_cd.kind == CD_MP3) drmp3_uninit(&g_cd.mp3);
    if (g_cd.kind == CD_WAV) drwav_uninit(&g_cd.wav);
    memset(&g_cd, 0, sizeof g_cd);
}

/* the file of track n, or 0 */
int cd_audio_find(int track, char *out, size_t n)
{
    static const char *const kinds[] = { "ogg", "mp3", "wav" };
    char dirs[3][512];
    snprintf(dirs[0], sizeof dirs[0], "%s/music", os_data_dir());
    snprintf(dirs[1], sizeof dirs[1], "%s", os_data_dir());
    snprintf(dirs[2], sizeof dirs[2], "%s/music", os_home_dir());
    for (int d = 0; d < 3; d++)
        for (int k = 0; k < 3; k++) {
            snprintf(out, n, "%s/track_%02d.%s", dirs[d], track, kinds[k]);
            FILE *f = fopen(os_path(out), "rb");
            if (f) { char real[600]; fclose(f); snprintf(real, sizeof real, "%s", os_path(out)); snprintf(out, n, "%s", real); return 1; }
        }
    return 0;
}

/* track n from its start, round and round if `loop`; 0 if there is no file for it */
int cd_audio_play(int track, int loop, int out_rate)
{
    char path[600];
    cd_audio_stop();
    if (!cd_audio_find(track, path, sizeof path)) {
        if (getenv("OS_MUSICLOG")) printf("cd: no file for track %d\n", track);
        return 0;
    }
    if (!cd_open_file(path)) { fprintf(stderr, "cd: %s cannot be read\n", path); return 0; }
    g_cd.loop = loop; g_cd.track = track;
    g_cd.step = (float)g_cd.rate / (float)out_rate;
    if (getenv("OS_MUSICLOG")) printf("cd: track %d from %s (%d a second, %d side(s))%s\n", track, path, g_cd.rate, g_cd.channels, loop ? ", round and round" : "");
    return 1;
}

int cd_audio_track(void) { return g_cd.kind ? g_cd.track : 0; }
void cd_audio_pause(int on) { g_cd.paused = on != 0; }
int cd_audio_paused(void) { return g_cd.kind && g_cd.paused; }
void cd_audio_volume(float v) { g_cd_volume = v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }

/* more of the song into buf; 0 at its end (when not looping) */
static int cd_decode(void)
{
    static int16_t raw[4096 * 8];
    const int want = 4096, ch = g_cd.channels > 8 ? 8 : g_cd.channels;
    for (int tries = 0; tries < 2; tries++) {
        long got = 0;
        if (g_cd.kind == CD_OGG) {
            /* (stb_vorbis gives the sides it is asked for: one, or two) */
            got = stb_vorbis_get_samples_short_interleaved(g_cd.ogg, ch >= 2 ? 2 : 1, raw, want * (ch >= 2 ? 2 : 1));
            if (got > 0) {
                for (long i = 0; i < got; i++) {
                    g_cd.buf[2 * i] = raw[ch >= 2 ? 2 * i : i];
                    g_cd.buf[2 * i + 1] = raw[ch >= 2 ? 2 * i + 1 : i];
                }
            }
        } else {
            const long cap = (long)(sizeof raw / sizeof raw[0]) / ch, n = want < cap ? want : cap;
            got = g_cd.kind == CD_MP3 ? (long)drmp3_read_pcm_frames_s16(&g_cd.mp3, (drmp3_uint64)n, raw)
                                      : (long)drwav_read_pcm_frames_s16(&g_cd.wav, (drwav_uint64)n, raw);
            for (long i = 0; i < got; i++) {
                g_cd.buf[2 * i] = raw[i * ch];
                g_cd.buf[2 * i + 1] = raw[i * ch + (ch >= 2 ? 1 : 0)];
            }
        }
        if (got > 0) { g_cd.have = (int)got; g_cd.at = 0; return 1; }
        if (!g_cd.loop) return 0;
        /* the end: from the start again (0x4d630 plays it again as the drive stops) */
        if (g_cd.kind == CD_OGG) stb_vorbis_seek_start(g_cd.ogg);
        if (g_cd.kind == CD_MP3) drmp3_seek_to_pcm_frame(&g_cd.mp3, 0);
        if (g_cd.kind == CD_WAV) drwav_seek_to_pcm_frame(&g_cd.wav, 0);
    }
    return 0;
}

/* one output frame, added to *left / *right at `gain` (the mixer's thread) */
void cd_audio_frame(float *left, float *right, float gain)
{
    if (!g_cd.kind || g_cd.paused) return;
    float need = g_cd.step, l = 0.0f, r = 0.0f;
    while (need > 0.0f) {
        if (g_cd.at >= g_cd.have && !cd_decode()) { cd_audio_stop(); return; }
        float part = 1.0f - g_cd.frac;
        if (part > need) part = need;
        l += (float)g_cd.buf[2 * g_cd.at] * part;
        r += (float)g_cd.buf[2 * g_cd.at + 1] * part;
        need -= part;
        g_cd.frac += part;
        if (g_cd.frac >= 0.9999f) { g_cd.frac = 0.0f; g_cd.at++; }
    }
    const float k = gain * g_cd_volume / g_cd.step;
    *left += l * k;
    *right += r * k;
}
