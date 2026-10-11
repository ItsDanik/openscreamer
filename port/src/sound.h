#ifndef SCREAMER_SOUND_H
#define SCREAMER_SOUND_H

#define SND_MAX 96

/* the game's own numbers for the effects of SFXSBASE.DTR */
enum {
    SND_SKID = 0, SND_CRASH = 1, SND_OPP_ENGINE = 2, SND_ENGINE = 3, SND_WALL = 4,
    SND_HIT = 5, SND_CLICK = 6, SND_ENGINE_TUNNEL = 7, SND_DENT = 8, SND_DOOR = 9
};

/* `sound_dir` holds the .RAW samples, `list_file` is SFXSBASE.DTR.  Returns
   the number of samples loaded. */
int  snd_init(const char *sound_dir, const char *list_file);
void snd_close(void);
int  snd_add(const char *name, float volume, int loop, int loop_start, int loop_len);
int  snd_find(const char *name);

int  snd_play(int id);                 /* returns a handle, -1 = none */
int  snd_stop(int handle);             /* returns -1 */
int  snd_playing(int handle);
void snd_pitch(int handle, int pitch); /* 16.16: 0x10000 = as recorded */
void snd_swap(int handle, int id);
void snd_volume(int handle, float gain);
void snd_pan(int handle, float pan);   /* -1 left .. +1 right */
int  snd_music(const char *dir, int number);   /* MUSICn.HMP, looping; 0 = stop */
int  snd_add_list(const char *list_file);
int  snd_play_once(int id, int handle);

/* testing without a sound card: with OS_SNDWAV=<file> the mix is written to
   a WAV file, advanced by this much each call */
void snd_render(float seconds);
void snd_masters(float effects, float music);   /* 1 = the game's starting level */

#endif
