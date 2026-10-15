/*
 * SCREAMER 2: the port's side of the game's own routines.
 *
 * The race and menu routines are the game's, written by hand in src/s2r_car.h (docs/NOTES2.md
 * S2-8) after the game's machine code; they work on the program's own memory layout (s2_mem.h).
 * S2_REFERENCE (the developer's alone): the routines converted to C from the game's program
 * instead (src/s2_gen.inc, tools/s2c.py, never published), the reference the hand-written ones
 * are checked against.  This file is what stands around them:
 *   - the block, its allocator and the routines' stack;
 *   - the game's data tables read into the block where the game's loaders put them
 *     (CAR_PARAMS, SKILLS, DAMAGES, a track's TRACK_DATA and road kinds, the cars' line);
 *   - the HOOKS: what the routines call outside themselves -- the track under a wheel,
 *     the road's direction, the engine's lists of objects, sound, bytes copied -- answered
 *     from the port's own scene.
 * Every address written here is the game's (flat image of STARTL.EXE); docs/NOTES2.md has
 * what each is.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#include "s2_mem.h"
#ifdef S2_REFERENCE
#include "s2_exe.h"                     /* (the player's STARTL.EXE: the reference build's alone) */
#endif
#include "s2_port.h"
#include "port.h"
#include "sound.h"

uint8_t  s2_mem[S2_SIZE];
static struct { uint32_t page; uint8_t bytes[0x1000]; } s2_code[32];
uint32_t s2_sp;
static uint32_t s2_heap;

#define HEAP_AT  0x140000u          /* past the data object (it ends at 0x139c90) and S2_TMP */
#define STACK_AT (S2_BASE + S2_SIZE - 0x80000u)   /* the routines' stack: the block's last 512 KB */

#define I32(a) (*(int32_t *)MP(a))
#define U32(a) (*(uint32_t *)MP(a))
#define F32(a) (*(float *)MP(a))
#define U8(a)  (*(uint8_t *)MP(a))

void s2_bad_address(uint32_t a)
{
    static int said;
    if (said++ < 8) fprintf(stderr, "screamer 2: a routine reached outside its memory (0x%x)\n", a);
}

uint8_t *s2_code_var(uint32_t a)
{
    if (a < 0x1000u) {                            /* (the null page: the game reads through an empty list's head, 0x1257f -- harmless under DOS) */
        static uint8_t zero_page[0x1000 + 16];
        memset(zero_page, 0, sizeof zero_page);
        return zero_page + a;
    }
    if (a >= 0x10000u && a < S2_BASE - 16)        /* (the engine's variables in its code; the keys' table at 0x100002) */
        for (int i = 0; i < 32; i++) {
            if (!s2_code[i].page) {                 /* (a new page: as the program's image has it, when it is loaded -- the menu) */
                s2_code[i].page = a & ~0xfffu;
#ifdef S2_REFERENCE
                memcpy(s2_code[i].bytes, s2_flat + s2_code[i].page, 0x1000);
#else
                memset(s2_code[i].bytes, 0, 0x1000);       /* (what the menu reads there, s2r_menu_tables writes) */
#endif
            }
            if (s2_code[i].page == (a & ~0xfffu) && (a & 0xfffu) < 0x1000u - 16) return s2_code[i].bytes + (a & 0xfffu);
        }
    s2_bad_address(a);
    return s2_mem;
}

static int s2_in_menu;
static uint32_t s2m_heap_lo, s2m_alloc(uint32_t n);
uint32_t s2_alloc(uint32_t size)
{
#ifdef S2_CONVERTED
    if (s2_in_menu && s2m_heap_lo) return s2m_alloc(size);     /* (the menu's blocks come and go: s2_menu_hooks.h) */
#endif
    uint32_t a = s2_heap;
    size = (size + 15u) & ~15u;
    if (a + size > STACK_AT) { fprintf(stderr, "screamer 2: out of the block's memory (%u more)\n", size); return 0; }
    s2_heap += size;
    memset(MP(a), 0, size);
    return a;
}

void s2_str(uint32_t a, const char *text)
{
    memcpy(MP(a), text, strlen(text) + 1);
}

#ifdef S2_CONVERTED
void s2_gen_strings(void);
#endif
static void s2_hooks_set(void);

#ifdef S2_CONVERTED
int s2_available(void) { return 1; }
#else
int s2_available(void) { return 0; }
#endif

/* the block as a race finds it; 0 if the game's program cannot be had */
int s2_reset(void)
{
#ifdef S2_CONVERTED
    memset(s2_mem, 0, sizeof s2_mem);
    memset(s2_code, 0, sizeof s2_code);
    s2_gen_strings();
    s2_hooks_set();
#else
    static int said;
    if (!said++) fprintf(stderr, "screamer 2: this build has not the game's routines (src/s2_gen.inc, the developer's alone): Screamer 2 cannot run\n");
    return 0;
#endif
    s2_heap = HEAP_AT;
    s2_sp = STACK_AT;
    return 1;
}

/*
 * A TABLE OUT OF A .DTR FILE, as the game reads one (0x31235): the lines after the one
 * that is the section's name, up to END_LIST; a word beginning d is a whole number, f a
 * float, x a hexadecimal number, s a name (12 bytes); `*` begins a comment that runs to the
 * end of the line.  The values fill rows of `row` bytes, `stride` apart, from `dest`.
 * Returns the number of rows.
 */
int s2_dtr(const char *file, const char *section, uint32_t dest, int stride, int row)
{
    FILE *f = os_fopen(file, "rb");
    char line[1024];
    int in = 0, rows = 0, at = 0;
    if (!f) { fprintf(stderr, "screamer 2: no %s\n", file); return 0; }
    while (fgets(line, sizeof line, f)) {
        char *p = line;
        if (!in) {
            while (*p == ' ' || *p == '\t') p++;
            char *e = p + strlen(p);
            while (e > p && (e[-1] == '\r' || e[-1] == '\n' || e[-1] == ' ' || e[-1] == '\t')) *--e = 0;
            if (!strcmp(p, section)) in = 1;
            continue;
        }
        for (;;) {
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
            if (!*p || *p == '*') break;
            char *w = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') p++;
            if (*p) *p++ = 0;
            if (!strcmp(w, "END_LIST")) { fclose(f); return rows; }
            const uint32_t to = dest + (uint32_t)(stride * rows + at);
            if (*w == 'd') { I32(to) = (int32_t)strtol(w + 1, NULL, 10); at += 4; }
            else if (*w == 'f') { F32(to) = (float)strtod(w + 1, NULL); at += 4; }
            else if (*w == 'x') { U32(to) = (uint32_t)strtoul(w + 1, NULL, 16); at += 4; }
            else if (*w == 's') { strncpy((char *)MP(to), w + 1, 11); ((char *)MP(to))[11] = 0; at += 12; }
            if (at == row) { at = 0; rows++; }
        }
    }
    fclose(f);
    return rows;
}

/* ------------------------------------------------------------------ the hooks */

#ifdef S2_CONVERTED
/* nothing to do in the port (yet): the engine's lists of what is drawn, its material
   libraries and files, text made with sprintf, and sound */
#if 0
#define HOOK_22d35
#define FUN_00022d35(...) 0
#endif
#endif
/*
 * SOUND.  The game's race routines call six routines of its sound manager (0xae..0xaf..,
 * over HMI's digital driver); here they drive the port's mixer.  The manager has 64 "slots"
 * (the engine of car n, its tyres, a crash ...): a slot plays one sample at a time.
 *   0xaea54(m, sample, slot, rate, volume, replace)  start the sample in the slot (rate 0 =
 *            its own; volume 0 = its own, 1 = silent; `replace`: if the slot is playing
 *            this very sample, stop that first)
 *   0xaebf0(m, sample, slot, rate, volume)  the same, unless the slot is already playing
 *            this sample (what keeps a looping sound going); another sample is stopped
 *   0xaee18(m, slot, volume)   0xaee98(m, slot) stop   0xaf45c(m) forget the distances
 *   0xaf168(m, record)  a sound somewhere on the track, heard from the listener: record =
 *            slot, source x, y, the listener's heading, listener x, y, how far it carries
 *            (whole numbers), and a pitch (float).  With d the distance: beyond the range
 *            silent; within it  rate = the sample's x pitch^2, + rate x (last d - d) /
 *            range x 5 (it rises as it comes nearer);  volume = the sample's x ((range -
 *            d) / range)^2;  pan = 32768 - 32767 x cos((1 - across / range) x 1.87), across
 *            = the source's place along the listener's (cos, sin) of its heading.
 * Samples: SOUND/GAME.DTR (numbers 0 .. 0x20: engines M_*, skids, hits, crashes, tyres,
 * turbos T_*) and AMBIENT<track - 1>.DTR (0x40 ..): name, number, loop length, loop count
 * (-1 = round and round), volume (of 0x7fff); then rate, bits, channels, format.
 */
#define S2_SAMPLES 128
#define S2_SLOTS 64
static struct { int id, rate, volume, loops; } s2_sample[S2_SAMPLES];
static struct { int handle, sample, volume; float last; } s2_slot[S2_SLOTS];
static int s2_sound_on;
/* the manager's own volume over every slot (its +0x18, 0..0x7fff, handed to HMI's driver): full
   as the program starts (0xae910), set by 0xaede0 (the race's load and its end, 0x2e1f3 /
   0x31495: the settings' effects volume, 0x130d58), eased towards a value by 0xaec90 (the
   fades as a page or the race is left) */
static int s2_master = 0x7fff;
/* (how loud the whole of it is against the mixer's scale, which was set for Screamer's
   samples: four engines and a skid at once clipped at 1.  The port's own; OS_S2SNDGAIN) */
static float s2_sound_gain = 0.5f;

static void s2_sound_table(const char *file)
{
    FILE *f = os_fopen(file, "rb");
    char line[256], name[32];
    int in = 0, at = -1;
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        char *p = line;
        unsigned num = 0; int rate = 0;
        while (*p == ' ' || *p == '\t') p++;
        if (!in) { if (!strncmp(p, "SOUND_DATA", 10)) in = 1; continue; }
        if (!strncmp(p, "END_LIST", 8)) break;
        if (*p == 's' && sscanf(p, "s%31s x%x", name, &num) == 2) {
            /* (the numbers after the name: number, loop length, loop count, volume) */
            const char *q = strrchr(p, 'x');
            const unsigned vol = q ? (unsigned)strtoul(q + 1, NULL, 16) : 0x7fffu;
            at = num < S2_SAMPLES ? (int)num : -1;
            if (at < 0) continue;
            { char *dot = strrchr(name, '.'); if (dot) *dot = 0; }        /* (the mixer adds .RAW) */
            s2_sample[at].loops = strstr(p, "x-1") != NULL;
            s2_sample[at].volume = (int)(vol && vol <= 0x7fff ? vol : 0x7fff);
            s2_sample[at].rate = 11025;
            s2_sample[at].id = snd_add(name, 1.0f, s2_sample[at].loops ? -1 : 0, 0, 0);   /* (the mixer's count: -1 = for ever) */
        } else if (*p == 'd' && at >= 0 && sscanf(p, "d%d", &rate) == 1) { if (rate > 0) s2_sample[at].rate = rate; at = -1; }
    }
    fclose(f);
}

/* the samples of a race on `track`, the mixer started (main.c says when) */
void s2_sound_start(int track)
{
    char file[96];
    for (int i = 0; i < S2_SAMPLES; i++) s2_sample[i].id = -1;
    for (int i = 0; i < S2_SLOTS; i++) { s2_slot[i].handle = -1; s2_slot[i].sample = -1; s2_slot[i].last = -1.0f; }
    snd_init("build/dec/SOUND", "build/dec/SOUND/none");
    s2_sound_table("build/dec/SOUND/GAME.DTR");
    snprintf(file, sizeof file, "build/dec/SOUND/AMBIENT%d.DTR", track - 1);
    s2_sound_table(file);
    s2_sound_on = !getenv("OS_NOSOUND");
    s2_master = 0x7fff;
    if (getenv("OS_S2SNDGAIN")) s2_sound_gain = (float)atof(getenv("OS_S2SNDGAIN"));
}

static int s2_slot_live(int slot) { return slot >= 0 && slot < S2_SLOTS && s2_slot[slot].handle >= 0 && snd_playing(s2_slot[slot].handle); }
static void s2_slot_rate(int slot, int rate) { if (s2_slot_live(slot)) snd_pitch(s2_slot[slot].handle, (int)((double)rate * 65536.0 / 11025.0)); }
static void s2_slot_volume(int slot, int volume)
{
    if (!s2_slot_live(slot)) return;
    s2_slot[slot].volume = volume < 0 ? 0 : volume > 0x7fff ? 0x7fff : volume;
    snd_volume(s2_slot[slot].handle, s2_sound_gain * (float)s2_slot[slot].volume / 32767.0f * (float)s2_master / 32767.0f);
}
static void s2_master_set(int v)
{
    s2_master = v < 0 ? 0 : v > 0x7fff ? 0x7fff : v;
    for (int i = 0; i < S2_SLOTS; i++) if (s2_slot_live(i)) s2_slot_volume(i, s2_slot[i].volume);
}

static int FUN_000aea54(uint32_t m, int sample, int slot, int rate, int volume, int replace)
{
    (void)m;
    if (!s2_sound_on || slot < 0 || slot >= S2_SLOTS || sample < 0 || sample >= S2_SAMPLES || s2_sample[sample].id < 0) return 0;
    if (getenv("OS_SNDLOG")) printf("sound: slot %d starts sample 0x%x rate %d volume %d\n", slot, sample, rate, volume);
    if (replace && s2_slot_live(slot) && s2_slot[slot].sample == sample) snd_stop(s2_slot[slot].handle);
    s2_slot[slot].handle = snd_play(s2_sample[sample].id);
    s2_slot[slot].sample = sample;
    s2_slot[slot].last = -1.0f;
    s2_slot_rate(slot, rate ? rate : s2_sample[sample].rate);
    s2_slot_volume(slot, volume == 1 ? 0 : volume ? volume : s2_sample[sample].volume);
    return 0;
}
static int FUN_000aebf0(uint32_t m, int sample, int slot, int rate, int volume)
{
    if (!s2_sound_on || slot < 0 || slot >= S2_SLOTS) return 0;
    if (s2_slot_live(slot)) {
        if (s2_slot[slot].sample == sample) return 0;
        snd_stop(s2_slot[slot].handle); s2_slot[slot].handle = -1;
    }
    return FUN_000aea54(m, sample, slot, rate, volume, 1);
}
static int FUN_000aee18(uint32_t m, int slot, int volume) { (void)m; s2_slot_volume(slot, (int16_t)volume); return 0; }
static int FUN_000aee98(uint32_t m, int slot)
{
    (void)m;
    if (slot >= 0 && slot < S2_SLOTS && s2_slot[slot].handle >= 0) { snd_stop(s2_slot[slot].handle); s2_slot[slot].handle = -1; }
    return 0;
}
static int FUN_000af45c(uint32_t m) { (void)m; for (int i = 0; i < S2_SLOTS; i++) s2_slot[i].last = -1.0f; return 0; }
static int FUN_000af168(uint32_t m, uint32_t rec)
{
    (void)m;
    const int slot = I32(rec);
    if (!s2_sound_on || !s2_slot_live(slot)) return 0;
    const int sample = s2_slot[slot].sample;
    const double dx = (double)(I32(rec + 0x10) - I32(rec + 4)), dy = (double)(I32(rec + 0x14) - I32(rec + 8));
    const float d = (float)sqrt(dx * dx + dy * dy);
    const int range = I32(rec + 0x18);
    int rate = 11025, volume = 0, pan = 0x8000;
    if (s2_slot[slot].last == -1.0f) s2_slot[slot].last = d;
    if ((float)range > d) {
        const double pitch = (double)F32(rec + 0x1c);
        const int own = (int)((double)s2_sample[sample].rate * (pitch * pitch));
        rate = (int)((double)own + (double)own * (double)((s2_slot[slot].last - d) / (float)range) * 5.0);
        const double near_ = ((double)range - (double)d) / (double)range;
        volume = (int)((double)s2_sample[sample].volume * (near_ * near_));
        const float c = (float)cos((double)F32(rec + 0xc)), sn = (float)sin((double)F32(rec + 0xc));
        const float across = (float)(I32(rec + 8) - I32(rec + 0x14)) * sn + (float)(I32(rec + 4) - I32(rec + 0x10)) * c;
        pan = (int)(32768.0 - cos((1.0 - (double)across / (double)range) * 1.87) * 32767.0);
    }
    if (getenv("OS_SNDLOG")) { static int n_; if (n_++ % 71 == 0) printf("sound: slot %d (sample 0x%x) heard at distance %.0f of %d, pitch %.3f -> rate %d volume %d pan %d\n", slot, sample, (double)d, range, (double)F32(rec + 0x1c), rate, volume, pan); }
    /* (a pitch of 0 -- the track's ambient sounds -- gives a rate of 0 by this sum; what HMI
       makes of that is not read: taken as "the sample's own") */
    s2_slot_rate(slot, rate > 0 ? rate : s2_sample[sample].rate);
    s2_slot_volume(slot, volume);
    if (s2_slot_live(slot)) snd_pan(s2_slot[slot].handle, (float)(pan - 32768) / 32768.0f);
    s2_slot[slot].last = d;
    return 0;
}

/* 0x37e06: a time as text, "%02d:%02d:%02d" of minutes, seconds, hundredths (each cut off),
   or "--:--:--" for 3599.999 (no time yet).  Its sprintf's arguments are on the stack. */
#define HOOK_37e06
static int FUN_00037e06(uint32_t to, uint32_t r1, uint32_t r2, uint32_t r3, float t)
{
    (void)r1; (void)r2; (void)r3;
    if (BITS(t) == 0x4560fffc) strcpy((char *)MP(to), "--:--:--");
    else {
        const int whole = (int)t;
        sprintf((char *)MP(to), "%02d:%02d:%02d", whole / 60, whole % 60, (int)((t - (float)whole) * 100.0f));
    }
    return 0;
}
#ifdef S2_CONVERTED
#define HOOK_132b4
#define FUN_000132b4(...) 0          /* the view keys: the port's own (s2_view) */
#define HOOK_4f6c4
#define FUN_0004f6c4(...) 0          /* the picture's size and eye line (320, 200 rows, 65 % down) */
#define HOOK_af45c
#define HOOK_31b00
#define FUN_00031b00(...) 0          /* the network */
#define HOOK_31c8c
#define FUN_00031c8c(...) 0
#define HOOK_4a455                   /* the replay's recorder (0x4a455): written down, or read back */
#define FUN_0004a455(...) ((U8(0x11f0a8) & 4) ? (I32(0x1330b8) ? FUN_0004a488() : FUN_0004a59a()) : (void)0)
#define HOOK_4c0cf
#define HOOK_4c15f
#define HOOK_aea54
#define HOOK_aebf0
#define HOOK_aee18
#define HOOK_aee98
#define HOOK_af168
#define HOOK_4ea98
#define FUN_0004ea98(...) 0          /* (arc cosine: its calls are rewritten, tools/s2c_fix.txt) */
#define HOOK_afed2
#define FUN_000afed2(...) 0          /* (power: the same) */
#define HOOK_b04a8

/* THE SOLVER OF TOUCHING CARS (0x245a3) is not carried over yet: "the whole step can be done" */
#if 0
#define HOOK_245a3
#define FUN_000245a3(...) 2.0
#endif

/* 0x10d9e: the angle of a vector (x, y), 0 .. 2 pi */
#define HOOK_10d9e
static double FUN_00010d9e(float x, float y)
{
    float a = (float)s2_acos((double)x / sqrt((double)(x * x + y * y)));
    return 0.0f > y ? 6.2831854820251465f - a : a;
}

/* 0x338ed: the angles at which a camera at `at` looks at the thing at `from` -- the heading
   from the vector's angle, and up / down from arc cosine(length on the ground / length), whose
   argument is on the FPU stack */
#define HOOK_338ed
static int FUN_000338ed(uint32_t out, uint32_t from, uint32_t at)
{
    const int dx = *(int32_t *)MP(at) - *(int32_t *)MP(from), dy = *(int32_t *)MP(at + 4) - *(int32_t *)MP(from + 4),
              dz = *(int32_t *)MP(at + 8) - *(int32_t *)MP(from + 8);
    const float all = (float)sqrt((double)(dx * dx + dy * dy + dz * dz)), flat = (float)sqrt((double)(dx * dx + dz * dz));
    const float a = (float)fabs(s2_acos((double)(flat / all)));
    *(float *)MP(out + 4) = (float)FUN_00010d9e((float)dx, (float)dz) + -1.5707964f;
    *(float *)MP(out) = dy < 0 ? a : (float)(6.2831854820251465 - (double)a);
    *(float *)MP(out + 8) = 0.0f;
    return 0;
}

#endif
/* bytes copied and filled, whole numbers made positive, names copied, chance */
#define HOOK_ae6e3
static uint32_t FUN_000ae6e3(uint32_t to, uint32_t from, uint32_t n) { memmove(MP(to), MP(from), n); return to; }
#define HOOK_ae8b0
static uint32_t FUN_000ae8b0(uint32_t to, uint32_t value, uint32_t n) { memset(MP(to), (int)value, n); return to; }
#ifdef S2_CONVERTED
#define HOOK_afeca
static int s2_abs(int v) { return v < 0 ? -v : v; }
#define FUN_000afeca(v, ...) s2_abs((int)(v))
#endif
#define HOOK_b3be4
static uint32_t FUN_000b3be4(uint32_t to, uint32_t from) { strcpy((char *)MP(to), (const char *)MP(from)); return to; }
static uint32_t s2_seed = 1;
#define HOOK_b3c2d
static uint32_t FUN_000b3c2d(uint32_t seed) { s2_seed = seed; return 0; }
#define HOOK_b3c09
static int s2_rand(void) { s2_seed = s2_seed * 1103515245u + 12345u; return (int)(s2_seed >> 16 & 0x7fff); }
#define FUN_000b3c09(...) s2_rand()

/*
 * THE GROUND UNDER A POINT (0x28ce6; docs/NOTES2.md S2-5g).  The point is at 0x11cdcc /
 * 0x11cdd4; what is found goes where the engine leaves it, in variables of its code
 * object: 0x4cb98 the height, 0x4cba0.. the surface's normal, 0x4cbec the road kind.
 */
/* a test (tools/s2emu.py --hooks): every answer the port gives the routines about the track,
   in order, so that the program's own code can be given the same */
static FILE *s2_hooklog(void)
{
    static FILE *f; static int tried;
    if (!tried) { tried = 1; if (getenv("OS_S2HOOKLOG")) f = fopen(getenv("OS_S2HOOKLOG"), "wb"); }
    return f;
}
static void s2_hookput(const int32_t *w, int n) { FILE *f = s2_hooklog(); if (f) { fwrite(w, 4, (size_t)n, f); fflush(f); } }

#define HOOK_28ce6
static uint64_t FUN_00028ce6(int mode, uint32_t edx)
{
    int h = 0, n[3] = { 0, 0, 0 }, kind = 0;
    I32(0x4cb98) = 0;
    const int found = s2p_ground(mode == 1, I32(0x11cdcc), I32(0x11cdd4), &h, n, &kind);
    if (found) {
        I32(0x4cbec) = kind;
        if (mode != 1) { I32(0x4cb98) = h; I32(0x4cba0) = n[0]; I32(0x4cba4) = n[1]; I32(0x4cba8) = n[2]; }
    }
    { const int32_t rec[9] = { 1, I32(0x11cdcc), I32(0x11cdd4), found | (mode == 1 ? 2 : 0), h, n[0], n[1], n[2], kind }; s2_hookput(rec, 9); }
    if (getenv("OS_S2ASKLOG")) printf("   ground(%d) at %d %d: %s, height %d, kind %d\n", mode, I32(0x11cdcc), I32(0x11cdd4), found ? "yes" : "NO", h, kind);
    return (uint64_t)edx << 32 | (found ? 1u : 0u);
}

/*
 * THE NEAREST POINT OF THE ROAD'S CENTRE LINE (argument 0) OR WALL SEGMENT (1) to the
 * point at 0x11cdcc / 0x11cdd4 (0x2a371).  The game leaves the address of the record it
 * found in the track's own data (0x11cde8 / 0x11cdd8); here a copy of the record is kept
 * in the block.  For a centre point also its place (0x11cde0, 0x11cde4) and the road's
 * direction there (0x11cddc), turned round when the track is driven the other way
 * (0x130d30 & 8).
 */
#define HOOK_2a371
static uint32_t s2_found[2];
#define FUN_0002a371(walls, ...) s2_nearest(walls)
static uint64_t s2_nearest(int walls)
{
    const uint32_t edx = 0;
    float rec[7];
    int32_t log[13] = { 2, walls, I32(0x11cdcc), I32(0x11cdd4), 0, 0, 0, 0, 0, 0, 0, 0 };
    if (!s2_found[0]) { s2_found[0] = s2_alloc(16); s2_found[1] = s2_alloc(32); }
    /* (the game goes on to read the place and direction of the road point it last found even
       when it finds none: 0x2a371's end) */
    int found = !getenv("OS_S2NONEAR") && s2p_nearest(walls, I32(0x11cdcc), I32(0x11cdd4), rec);
    const uint32_t at = s2_found[walls ? 1 : 0];
    if (found) {
        F32(at) = rec[0];
        for (int i = 1; i < (walls ? 7 : 3); i++) I32(at + 4u * (uint32_t)i) = (int32_t)rec[i];
        if (walls) U32(0x11cdd8) = at; else U32(0x11cde8) = at;
    }
    if (!walls && U32(0x11cde8)) {
        const uint32_t was = U32(0x11cde8);
        I32(0x11cde0) = I32(was + 4); I32(0x11cde4) = I32(was + 8);
        float a = F32(was);
        if (U32(0x130d30) & 8) a += 3.14159274f;
        if (a > 6.28318548f) a += -6.28318548f;
        F32(0x11cddc) = a;
    }
    log[4] = found; log[5] = (int32_t)at;
    if (getenv("OS_S2ASKLOG")) printf("   nearest(%d) at %d %d: %s %d %d\n", walls, I32(0x11cdcc), I32(0x11cdd4), found ? "yes" : "NO", found ? I32(at + 4) : 0, found ? I32(at + 8) : 0);
    if (found) memcpy(log + 6, MP(at), walls ? 28 : 12);
    s2_hookput(log, 13 - 0);
    return (uint64_t)edx << 32 | (found ? 1u : 0u);
}

/*
 * THE ENGINE'S LIST OF OBJECTS, as much of it as the car's set-up reads (0x2f519): an
 * object has a number, and at 0x11da24 + number * 0x1c the address of its blocks' records,
 * 0x20 bytes each with the block's place in the object at +0x14 (0x4f9e2).  0x4bc94 gives
 * a car's model its number, 0x4bc03 finds a block of it by name.
 */
#define S2_OBJECTS 200            /* (the engine's own list holds 200: 0x4bd1b) */
#define S2_BLOCKS  16
static struct { int nblock; char name[S2_BLOCKS][16]; } s2_object[S2_OBJECTS];
static int s2_nobject;

static int s2_object_load(const char *file)
{
    FILE *f = os_fopen(file, "rb");
    static uint8_t b[1 << 17];
    if (!f || s2_nobject >= S2_OBJECTS) { if (f) fclose(f); fprintf(stderr, "screamer 2: no %s\n", file); return -1; }
    const long len = (long)fread(b, 1, sizeof b, f);
    fclose(f);
    const int k = s2_nobject++, n = len > 0x1c ? *(int32_t *)(b + 0x14) : 0;
    /* the object as the engine has it listed: 0x40 bytes (where it is and how it is turned:
       the car's routine 0x16878 writes them there every frame), then its blocks' records */
    const uint32_t object = s2_alloc(0x40 + S2_BLOCKS * 0x20), blocks = object + 0x40;
    U32(0x11da20 + (uint32_t)k * 0x1c) = object;
    U32(0x11da24 + (uint32_t)k * 0x1c) = blocks;
    s2_object[k].nblock = 0;
    for (int i = 0; i < n && i < S2_BLOCKS; i++) {
        const long at = *(int32_t *)(b + 0x18 + 4 * i);
        if (at <= 0 || at + 0x78 > len) break;
        const long rec = *(int32_t *)(b + at + 0x0c);
        memcpy(s2_object[k].name[i], b + at + 0x68, 15);
        if (rec > 0 && rec + 0x1c <= len) memcpy(MP(blocks + (uint32_t)i * 0x20 + 0x14), b + rec + 0x10, 12);
        s2_object[k].nblock = i + 1;
    }
    return k;
}

/* the menu (s2_menu_hooks.h): it is running (s2_in_menu, above); the container loaded from;
   each object's name and container, which the port's drawing of the engine's list goes by */
static char s2m_cont[16], s2m_obj_name[S2_OBJECTS][16], s2m_obj_cont[S2_OBJECTS][16];
static char s2m_lib[32], s2m_obj_lib[S2_OBJECTS][32];   /* the material library current (0x4feb4), each object's */

#define HOOK_4b843                     /* an object by its file's name: the shadow -- none yet */
static int FUN_0004b843(uint32_t list, uint32_t name, uint32_t to)
{
    (void)list;
    int k = -1;
    if (s2_in_menu)                                       /* (the menu lets objects go, 0x4ba39: a free place first) */
        for (int j = 0; j < s2_nobject; j++) if (!U32(0x11da20 + (uint32_t)j * 0x1c) && !I32(0x11da28 + (uint32_t)j * 0x1c)) { k = j; break; }
    if (k < 0) {
        if (s2_nobject >= S2_OBJECTS) return 3;
        k = s2_nobject++;
    }
    const uint32_t object = s2_alloc(0x40 + 0x20);
    U32(0x11da20 + (uint32_t)k * 0x1c) = object;
    U32(0x11da24 + (uint32_t)k * 0x1c) = object + 0x40;
    I32(0x11da28 + (uint32_t)k * 0x1c) = 1;               /* (it has frames: what 0x4c0cf / 0x4c15f ask) */
    I32(0x11da2c + (uint32_t)k * 0x1c) = -1;              /* (not in the draw list: 0x4be04 puts it there) */
    s2_object[k].nblock = 0;
    snprintf(s2_object[k].name[0], sizeof s2_object[k].name[0], "%.15s", (const char *)MP(name));
    snprintf(s2m_obj_name[k], sizeof s2m_obj_name[k], "%.15s", (const char *)MP(name));
    snprintf(s2m_obj_cont[k], sizeof s2m_obj_cont[k], "%s", s2m_cont);
    snprintf(s2m_obj_lib[k], sizeof s2m_obj_lib[k], "%s", s2m_lib);
    if (s2_in_menu && getenv("OS_S2MLOG")) printf("menu: object %d %s of %s\n", k, s2m_obj_name[k], s2m_cont);
    I32(to) = k;
    return 0;
}

/*
 * AN OBJECT'S ANIMATION (the smoke's): the object's record +0x20 is how far through it is,
 * 0 .. 65535.  0x4c15f(list, record, .., .., part) sets it; 0x4c0cf(list, record, .., ..,
 * step) moves it on by step x 65535 and gives -1 when it has run out (and takes one whole
 * run off), 0 otherwise, 0x11 for an object without frames.
 */
static int FUN_0004c15f(uint32_t list, uint32_t rec, uint32_t r2, uint32_t r3, float part)
{
    (void)r2; (void)r3;
    const uint32_t k = U32(rec);
    if (!I32(list + k * 0x1c + 0x14)) return 0x11;
    I32(U32(list + k * 0x1c + 0xc) + 0x20) = (int32_t)(part * 65535.0f);
    return 0;
}
static int FUN_0004c0cf(uint32_t list, uint32_t rec, uint32_t r2, uint32_t r3, float step)
{
    (void)r2; (void)r3;
    const uint32_t k = U32(rec);
    if (!I32(list + k * 0x1c + 0x14)) return 0x11;
    const uint32_t at = U32(list + k * 0x1c + 0xc) + 0x20;
    I32(at) = (int32_t)(float)(step * 65535.0f + (float)I32(at));
    if (I32(at) < 0x10000) return 0;
    I32(at) -= 0x10000;
    return -1;
}

#define HOOK_4bc94                     /* the car's model: AUTO<model> of CARS<team><condition & 1>1 */
static int FUN_0004bc94(uint32_t list, uint32_t name, uint32_t to)
{
    if (s2_in_menu) return FUN_0004b843(list, name, to);   /* (the menu's: any object by name, of the container) */
    char file[96];
    const uint32_t car = to - 0xbf0;
    const int i = I32(car);
    (void)list; (void)name;
    snprintf(file, sizeof file, "build/dec/CARS%d%d1/AUTO%d.3DX", I32(0x130c58 + (uint32_t)i * 0x2c) + 1,
             (int)(U32(0x130c50) & 1), I32(0x130c5c + (uint32_t)i * 0x2c) + 1);
    const int k = s2_object_load(file);
    I32(to) = k < 0 ? 0 : k;
    return 0;
}

#define HOOK_4bc03                     /* a block of an object, by name: its number to `to + 4` */
static int FUN_0004bc03(uint32_t name, uint32_t to, uint32_t list)
{
    const int k = I32(to);
    (void)list;
    I32(to + 4) = 0;
    if (k < 0 || k >= s2_nobject) return 0;
    for (int i = 0; i < s2_object[k].nblock; i++)
        if (!strcasecmp(s2_object[k].name[i], (const char *)MP(name))) { I32(to + 4) = i; return 0; }
    fprintf(stderr, "screamer 2: the car's model has no block \"%s\"\n", (const char *)MP(name));
    return 0;
}

#ifdef S2_CONVERTED
#include "s2_menu_hooks.h"
#include "s2r_car.h"                   /* (the game's routines, written by hand: docs/NOTES2.md S2-8) */
#ifdef S2_REFERENCE
#include "s2_gen.inc"                  /* (the developer's reference: the routines converted from the game's program) */
#else
void s2_gen_strings(void) { }
void s2_gen_tables(void) { s2r_tables(); }
#endif
static void s2_hooks_set(void) { s2r_strings(); }
static void s2m_list_clear(void) { FUN_0004b016(); }

/*
 * THE GAME'S OWN MENU (the default; docs/NOTES2.md S2-5zq): its loop 0x38540, run
 * from a fresh block holding the player's STARTL.EXE's data (the menu reads a great many of
 * its tables -- the race's routines start from the few they need, s2_tables.txt).  The game's
 * directories are empty names, so that its files are "\gfx\graf1.dat" and the like.  Ends
 * when the menu would leave the game or start a race (not yet), or the window is closed.
 */
static void s2_ease_tables(void);

/* what the menu keeps while its race (the port's other part) runs: its block, the engine's
   pages, and its own tables (os_slot 9; os_keep_calloc: the parts' memory goes between them) */
typedef struct {
    int resume, nobject, exit_code;
    uint32_t heap, sp, heap_lo, heap_hi, ease_row, last_block, camera;
    char cont[16], lib[32], name[S2_OBJECTS][16], ocont[S2_OBJECTS][16], olib[S2_OBJECTS][32];
    struct { uint32_t page; uint8_t bytes[0x1000]; } code[32];
    uint8_t mem[S2_SIZE];
} s2_menu_kept_t;
static void s2m_keep(void)
{
    s2_menu_kept_t *k = os_slot(9);
    if (!k) { k = os_keep_calloc(1, sizeof *k); if (!k) return; os_slot_set(9, k); }
    memcpy(k->mem, s2_mem, S2_SIZE);
    memcpy(k->code, s2_code, sizeof k->code);
    k->heap = s2_heap; k->sp = s2_sp; k->nobject = s2_nobject;
    k->heap_lo = s2m_heap_lo; k->heap_hi = s2m_heap_hi; k->ease_row = s2m_ease_row; k->last_block = s2m_last_block; k->camera = s2m_camera;
    memcpy(k->cont, s2m_cont, sizeof k->cont); memcpy(k->lib, s2m_lib, sizeof k->lib);
    memcpy(k->name, s2m_obj_name, sizeof k->name); memcpy(k->ocont, s2m_obj_cont, sizeof k->ocont); memcpy(k->olib, s2m_obj_lib, sizeof k->olib);
    k->resume = 1;
}
/* WHAT A RACE LEAVES FOR THE GAME'S MENU (the race part's block is its own; in the game it is
   one): the race's record, the cars, the places (0x132384: filled by the end's ENTER, 0x47e75,
   when not a replay -- per place the team, the model, the car, the time) and the best times.
   Kept by the race part as it is left (s2_race_results_keep), put into the menu's block as it
   comes back (os_slot 10). */
#define S2K_CAR(i)   (0x119300u + (uint32_t)(i) * 0xcc0u)       /* (CAR, ENTRY, S2_REC_AT / SIZE: defined further on) */
#define S2K_ENTRY(i) (0x130c58u + (uint32_t)(i) * 0x2cu)
#define S2K_REC_AT   0x1290bcu
#define S2K_REC_SIZE (3u * 0x20d0u + 3u * 0x834u)
typedef struct { int ok; uint8_t record[0x124], cars[4 * 0xcc0], places[0x44], records[S2K_REC_SIZE]; } s2_race_kept_t;
void s2_race_results_keep(void)
{
    s2_race_kept_t *k = os_slot(10);
    if (!k) { k = os_keep_calloc(1, sizeof *k); if (!k) return; os_slot_set(10, k); }
    /* taken as the race's own end screen is left with ENTER (0x47e75, not in a replay: main.c),
       and again as the part is left -- but not from a replay over it: the cars are then where
       the replay left them (ESC in it: a few seconds in, the player "done" but not finished,
       which 0x43914 counts as last -- Danik's league, won twice, had 10 points) */
    if ((U32(0x130d10) & 0x100) && k->ok) return;
    if (!(U32(0x130d10) & 0x100)) {                     /* (0x47e75: not from a replay) */
        U32(0x132384) = U32(0x130c48);
        for (int i = 0; i < I32(0x130c48) && i < 4; i++) {
            const int place = I32(S2K_CAR(i) + 0xc30);
            if (place < 1 || place > 4) continue;
            U32(0x132384 + 4u * (uint32_t)place) = U32(S2K_ENTRY(i));
            U32(0x132394 + 4u * (uint32_t)place) = U32(S2K_ENTRY(i) + 4);
            U32(0x1323b4 + 4u * (uint32_t)place) = U32(S2K_CAR(i) + 0xc38);
            I32(0x1323a4 + 4u * (uint32_t)place) = i;
        }
    }
    if (getenv("OS_S2MLOG")) {
        printf("race: the result kept%s: places (car):", (U32(0x130d10) & 0x100) ? " (in a replay)" : "");
        for (int i = 0; i < I32(0x130c48) && i < 4; i++) printf(" %d=car %d%s", I32(S2K_CAR(i) + 0xc30), i, (U8(S2K_CAR(i) + 0x4c) & 2) ? "" : " (not finished)");
        printf("\n");
    }
    memcpy(k->record, MP(0x130c48), sizeof k->record);
    memcpy(k->cars, MP(0x119300), sizeof k->cars);
    memcpy(k->places, MP(0x132384), sizeof k->places);
    memcpy(k->records, MP(S2K_REC_AT), sizeof k->records);
    k->ok = 1;
}
static void s2_race_results_put(void)
{
    s2_race_kept_t *k = os_slot(10);
    if (!k || !k->ok) return;
    k->ok = 0;
    memcpy(MP(0x130c48), k->record, sizeof k->record);
    memcpy(MP(0x119300), k->cars, sizeof k->cars);
    memcpy(MP(0x132384), k->places, sizeof k->places);
    memcpy(MP(S2K_REC_AT), k->records, sizeof k->records);
}

/* THE RACE'S RECORD FROM THE GAME'S MENU: in the game one block, made by the race's set-up
   0x339e7 from the settings and the pages -- the cars and their entries (team, model, name,
   gearbox, the garage's set-up, +0x14), the detail (0x130c54), the damage (0x130d54, GAME
   SETTINGS' DAMAGES), 0x130d64 (MUSIC), 0x130c64, the volumes and the view (0x130d58..0x130d68,
   which 0x38540 copies back into the settings after the race; the race keeps the view in use
   as the player finishes, 0x201ee).  The race part takes it as the menu kept it, all but the
   mode and the times (0x130d10..0x130d2f: the race part's own set-up makes them, as 0x339e7
   leaves them) and the laps (0x130d48: OS_LAPS, which is the record's, or a test's). */
static void s2m_race_record(void)
{
    const s2_menu_kept_t *k = os_slot(9);
    if (!k || !getenv("OS_FROM_MENU")) return;
    const uint8_t *m = k->mem - S2_BASE;
    memcpy(MP(0x130c48), m + 0x130c48, 0x130d10 - 0x130c48);
    memcpy(MP(0x130d30), m + 0x130d30, 0x130d48 - 0x130d30);
    memcpy(MP(0x130d4c), m + 0x130d4c, 0x130d6c - 0x130d4c);
    if (getenv("OS_S2MLOG")) printf("race: the menu's record: %d cars, damage %d, detail %d, set-up %d %d %d %d %d %d, view %d\n", I32(0x130c48), I32(0x130d54), I32(0x130c54),
                                    I32(0x130c58 + 0x14), I32(0x130c58 + 0x18), I32(0x130c58 + 0x1c), I32(0x130c58 + 0x20), I32(0x130c58 + 0x24), I32(0x130c58 + 0x28), I32(0x130d68));
    s2_master_set(I32(0x130d58));                        /* (the effects' volume, as the race's load sets it: 0x2e1f3) */
}

/* and the best times: in the game one set of lists, in its settings file (0x1290bc..); a race
   from the game's menu starts with the menu's (the port's own file, openscreamer.s2records,
   follows them: it is what the port's plain menu reads) */
static void s2m_race_records(uint8_t *r, uint32_t at, uint32_t size)
{
    const s2_menu_kept_t *k = os_slot(9);
    if (!k || !getenv("OS_FROM_MENU")) return;
    memcpy(MP(at), k->mem + (at - S2_BASE), size);
    if (r) memcpy(r, MP(at), size);
}

static int s2m_unkeep(void)
{
    s2_menu_kept_t *k = os_slot(9);
    if (!k || !k->resume) return 0;
    k->resume = 0;
    memcpy(s2_mem, k->mem, S2_SIZE);
    memcpy(s2_code, k->code, sizeof k->code);
    s2_heap = k->heap; s2_sp = k->sp; s2_nobject = k->nobject;
    s2m_heap_lo = k->heap_lo; s2m_heap_hi = k->heap_hi; s2m_ease_row = k->ease_row; s2m_last_block = k->last_block; s2m_camera = k->camera;
    memcpy(s2m_cont, k->cont, sizeof k->cont); memcpy(s2m_lib, k->lib, sizeof k->lib);
    memcpy(s2m_obj_name, k->name, sizeof k->name); memcpy(s2m_obj_cont, k->ocont, sizeof k->ocont); memcpy(s2m_obj_lib, k->olib, sizeof k->olib);
    s2_race_results_put();
    return 1;
}

/* the menu left by closing the window: the game's own save of the settings file (0x331dc), which
   its loop does as it ends, so that what was won is kept as it is when the game is quit */
static int s2_menu_left(void)
{
    s2_in_menu = 0;
    snd_close();                                         /* (the menu's sounds: the next part opens its own) */
    if (s2m_window_gone) {
        s2m_window_gone = 0;
        s2_sp = STACK_AT;
        S2_FPU_IN; FUN_000331dc(); S2_FPU_OUT;
    }
    return s2_menu_exit_code;
}

int s2_menu_run(void)
{
    char path[512];
    if (s2m_unkeep()) {                                  /* (back from the menu's race: its loop goes on after it) */
        s2_sp = STACK_AT;
        s2_in_menu = 1; s2_menu_exit_code = 0; s2m_resume = (U32(0x130d10) & 0x80) ? 2 : I32(0x1324dc) < 0 ? 3 : 1;   /* (2: after the attract race, 3: after a replay) */
        s2m_objects_clear();
        s2_master = I32(0x130d58);                       /* (the sounds' volume as the race's end leaves it: 0x31495) */
        if (!setjmp(s2_menu_out)) { S2_FPU_IN; FUN_00038540(); S2_FPU_OUT; }
        return s2_menu_left();
    }
    if (!s2_reset()) return 0;
#ifdef S2_REFERENCE
    snprintf(path, sizeof path, "%s/STARTL.EXE", os_data_dir());
    if (!s2_exe_load(os_path(path))) {
        snprintf(path, sizeof path, "%s/startl.exe", os_data_dir());
        if (!s2_exe_load(os_path(path))) { fprintf(stderr, "screamer 2: the menu needs STARTL.EXE\n"); return 0; }
    }
    {   /* (the converted routines read the program's data object: copied into the block) */
        memcpy(s2_mem, s2_flat + S2_BASE, HEAP_AT - S2_BASE);
    }
#else
    (void)path;
    s2r_strings(); s2r_tables(); s2r_menu_tables();      /* (what the menu's routines read of the game's program) */
#endif
    s2_heap = HEAP_AT; s2_sp = STACK_AT;
    s2_nobject = 0; s2m_cont[0] = 0;
    U8(0x11eff8) = 0; U8(0x11f060) = 0;
    U32(0x134794) = s2_alloc(s2_screen_hi() ? 256000u : 64000u);   /* the game's screen (the port draws into its own picture; STARTH's is 640 x 400) */
    s2_ease_tables();                                    /* (what the program makes as it starts, 0x2ddea) */
    s2_dtr("build/dec/GAME/CARS.DTR", "CAR_PARAMS", 0x11c600, 0x58, 0x58);     /* (and the tables it reads then) */
    s2_dtr("build/dec/GAME/CRUSC.DTR", "CRUSCOTTI", 0x1242b4, 0x58, 0x58);
    s2_dtr("build/dec/GAME/MISC.DTR", "SKILLS", 0x11f0f4, 0x1c, 0x1c);
    s2_dtr("build/dec/GAME/MISC.DTR", "CD_TRACKS", 0x11f0d8, 4, 4);
    s2_dtr("build/dec/GAME/MISC.DTR", "DAMAGES", 0x11cce0, 4, 4);
    s2m_heap_start(); s2m_ease_row = 0;
    s2_in_menu = 1; s2_menu_exit_code = 0;
    if (!setjmp(s2_menu_out)) {
        S2_FPU_IN;
        FUN_00038540();
        S2_FPU_OUT;
    }
    return s2_menu_left();
}
#else
int s2_menu_run(void) { fprintf(stderr, "screamer 2: the game's own menu needs the converted build\n"); return 0; }
static void s2m_race_record(void) {}
void s2_race_results_keep(void) {}
static void s2m_race_records(uint8_t *r, uint32_t at, uint32_t size) { (void)r; (void)at; (void)size; }   /* (no routines in this build) */
/* WITHOUT THE CONVERTED ROUTINES (src/s2_gen.inc is the developer's alone): the race part
   still compiles, with the routines it calls as nothing -- s2_reset() refuses, so none of them
   is ever reached */
static void s2_hooks_set(void) { }
#define FUN_000309b0(a)          ((void)(a))
#define FUN_0004b016()           ((void)0)
#define FUN_0002f519(a, b, c, d) ((void)(a), (void)(b), (void)(c), (void)(d), 0)
#define FUN_00012d31(a, b)       ((void)(a), (void)(b))
#define FUN_0001d9fc(a)          ((void)(a))
#define FUN_0002baa4()           ((void)0)
#define FUN_0002c132()           ((void)0)
#define FUN_0002cc74()           ((void)0)
#define FUN_0002c475()           ((void)0)
#define FUN_0002a639(a)          ((void)(a))
#define FUN_00030b29(a, b)       ((void)(a), (void)(b), 0)
#define FUN_00012dff(a, b)       ((void)(a), (void)(b))
#define FUN_00012d00(a, b)       ((void)(a), (void)(b))
static void s2_gen_tables(void) { }
#undef S2_FPU_IN
#undef S2_FPU_OUT
#define S2_FPU_IN  (void)0
#define S2_FPU_OUT (void)0
#endif

/* ------------------------------------------------------------------ a race */

#define CAR(i)   (0x119300u + (uint32_t)(i) * 0xcc0u)
#define ENTRY(i) (0x130c58u + (uint32_t)(i) * 0x2cu)        /* a car of the race record */
#define KEYS(i)  (0x127708u + (uint32_t)(i) * 0xc0u)        /* a player's controls */
#define CAMERA   U32(0x1192f4)                               /* the player's view: its camera record */

/*
 * THE TRACK'S ANIMATIONS (0x30ec2 loads the paths, 0x31060 sets the rows up, 0x2cee4 runs
 * them every frame of the main loop; only at the top detail).  Written out here from those
 * routines rather than run: the paths are built by a module of the game's own (0x11c0c,
 * 0x12512) out of MAPn.PTH, and what is needed of it is small.
 *
 * MAPn.PTH: a count, then for each path 100 bytes (name at +8, "MV_<c>...": the character
 * c is its number, '1'..'9' = 0..8, 'a'.. = 9..) and three channels -- x, y, height -- each
 * 0x30 bytes (+0x1c how many keys) and its keys of 0x2c bytes: +8 time (0 .. 1), +0xc value,
 * +0x10 and +0x14 slopes.  Between two keys p and q (0x126ad) the value is the cubic through
 * p.value, p.value + p.slope_out h, q.value - q.slope_in h, q.value with h = (q.time -
 * p.time) / 3.
 *
 * MAPnDAT.DTR's ANIMATIONS, a row = 0x4c bytes at 0x126d5c: name (12; `@...` = something of
 * the track itself, which only has a sound), +0xc how long one run takes, +0x10 when it
 * started, +0x14 the distance at which it starts (-1: 52000), +0x18 flags (1 it starts when
 * the player comes that near, 2 once only, 4 it faces the screen and is not turned along its
 * way), +0x1c which path, +0x20 on which lap (0 any), +0x24 its sample, +0x28 its object,
 * +0x30 where it is (x, height, y), +0x48 its sound is on.  The sound: slot 7 + row, started
 * silent, then heard from the camera with a range of 80000.
 */
#define S2_PATHS 40
#define S2_PATH_KEYS 24
typedef struct { int n; float t[S2_PATH_KEYS], v[S2_PATH_KEYS], out[S2_PATH_KEYS], in[S2_PATH_KEYS]; } s2_channel;
static struct { int ok; s2_channel c[3]; } s2_path[S2_PATHS];
static int s2_nanim;

static void s2_paths_load(const char *file)
{
    FILE *f = os_fopen(file, "rb");
    uint8_t head[100], ch[0x30], key[0x2c];
    int32_t count;
    memset(s2_path, 0, sizeof s2_path);
    if (!f) return;
    if (fread(&count, 4, 1, f) == 1)
        while (fread(head, 1, 100, f) == 100) {
            const int c = head[11];
            const int at = c >= '0' && c <= '9' ? c - '1' : c >= 'a' && c <= 'z' ? c - 0x58 : c - 0x38;
            int bad = 0;
            for (int k = 0; k < 3 && !bad; k++) {
                s2_channel tmp;
                if (fread(ch, 1, 0x30, f) != 0x30) { bad = 1; break; }
                int32_t n; memcpy(&n, ch + 0x1c, 4);
                tmp.n = 0;
                for (int i = 0; i < n && !bad; i++) {
                    if (fread(key, 1, 0x2c, f) != 0x2c) { bad = 1; break; }
                    if (tmp.n < S2_PATH_KEYS) {
                        memcpy(&tmp.t[tmp.n], key + 8, 4); memcpy(&tmp.v[tmp.n], key + 0xc, 4);
                        memcpy(&tmp.out[tmp.n], key + 0x10, 4); memcpy(&tmp.in[tmp.n], key + 0x14, 4);
                        tmp.n++;
                    }
                }
                if (!bad && at >= 0 && at < S2_PATHS) s2_path[at].c[k] = tmp;
            }
            if (bad) break;
            if (at >= 0 && at < S2_PATHS) s2_path[at].ok = 1;
        }
    fclose(f);
}

/* 0x126ad: a channel's value and its rate of change at s */
static void s2_channel_at(const s2_channel *c, float s, float *value, float *rate)
{
    *value = 0.0f; *rate = 0.0f;
    if (c->n < 1) return;
    if (c->n < 2) { *value = c->v[0]; return; }
    if (c->t[0] <= s) { if (c->t[c->n - 1] <= s) s = c->t[c->n - 1] + -0.001f; }
    else s = c->t[0];
    int q = 1;
    while (q < c->n - 1 && c->t[q] <= s) q++;
    const int p = q - 1;
    const float dt = c->t[q] - c->t[p], u = (s - c->t[p]) / dt, h = (float)((double)dt * 0.333333333);
    const float b1 = c->out[p] * h + c->v[p], b2 = c->v[q] - c->in[q] * h;
    const float a3 = (b1 * 3.0f + (c->v[q] - b2 * 3.0f)) - c->v[p], a2 = c->v[p] * 3.0f + (b2 * 3.0f - b1 * 6.0f), a1 = b1 * 3.0f - c->v[p] * 3.0f;
    *value = ((a3 * u + a2) * u + a1) * u + c->v[p];
    *rate = ((a2 * 2.0f + a3 * 3.0f * u) * u + a1) / dt;
}

/* 0x129d5: where a path is at `part` (0 .. 1) and which way it is going */
static void s2_path_at(int n, float part, float place[3], float way[3])
{
    place[0] = place[1] = place[2] = 0.0f; way[0] = way[1] = way[2] = 0.0f;
    if (n < 0 || n >= S2_PATHS || !s2_path[n].ok) return;
    part = part < 0.0f ? 0.0f : part > 1.0f ? 1.0f : part;
    for (int k = 0; k < 3; k++) s2_channel_at(&s2_path[n].c[k], part, &place[k], &way[k]);
    const float len = (float)sqrt((double)(way[2] * way[2] + way[0] * way[0] + way[1] * way[1]));
    if (len != 0.0f) for (int k = 0; k < 3; k++) way[k] *= 1.0f / len;
}

static void s2_anims_start(int track)
{
    char file[96];
    s2_nanim = 0;
    I32(0x124208) = I32(0x130c54) == 2 && !getenv("OS_NOANIMS");
    I32(0x126d54) = I32(0x126d58) = 0;
    if (!I32(0x124208)) return;
    snprintf(file, sizeof file, "build/dec/MAP%d/MAP%d.PTH", track, track);
    s2_paths_load(file);
    snprintf(file, sizeof file, "build/dec/MAP%d/MAP%dDAT.DTR", track, track);
    s2_nanim = s2_dtr(file, "ANIMATIONS", 0x126d5c, 0x4c, 0x28);
    if (s2_nanim > 40) s2_nanim = 40;
    I32(0x126d54) = I32(0x126d58) = s2_nanim;
    for (uint32_t i = 0; i < (uint32_t)s2_nanim; i++) {              /* 0x31060 */
        const uint32_t r = 0x126d5c + i * 0x4c;
        float place[3], way[3];
        if (I32(r + 0x18) & 1) F32(r + 0x10) = 0.0f;
        if (U8(r) == '@') {
            const int n = I32(r + 0x1c);
            if (n >= 0 && n < S2_PATHS && s2_path[n].ok) { I32(r + 0x30) = (int32_t)s2_path[n].c[0].v[0]; I32(r + 0x38) = (int32_t)s2_path[n].c[1].v[0]; I32(r + 0x34) = (int32_t)s2_path[n].c[2].v[0]; }
        } else {
            s2_path_at(I32(r + 0x1c), 0.0f, place, way);
            I32(r + 0x30) = (int32_t)place[0]; I32(r + 0x34) = (int32_t)place[2]; I32(r + 0x38) = (int32_t)place[1];
        }
        I32(r + 0x40) = 0;                 /* (the port's: shown) */
        I32(r + 0x48) = 0;
    }
}

/* 0x2cee4, once a frame of the game's */
static void s2_anims_step(void)
{
    if (!I32(0x124208)) return;
    const uint32_t me = CAR(I32(0x11f0c8));
    const float clock = F32(0x13335c);
    for (uint32_t i = 0; i < (uint32_t)s2_nanim; i++) {
        const uint32_t r = 0x126d5c + i * 0x4c;
        const int flags = I32(r + 0x18), own = U8(r) != '@';
        int dx = abs(I32(me + 0x4d8) - I32(r + 0x30)), dy = abs(I32(me + 0x4e0) - I32(r + 0x38));
        if (getenv("OS_S2ANIMALL")) dx = dy = 0;      /* a test: every object as if the car were by it */
        int shown = 0;
        if (((dx < 52000 && dy < 52000) || ((flags & 3) == 0 && own)) && (I32(me + 0x4a8) + 1 == I32(r + 0x20) || I32(r + 0x20) == 0)) {
            if (!own) shown = 1;
            else {
                float range = F32(r + 0x14), part;
                if (range == -1.0f) range = 52000.0f;
                if ((float)dx < range && (float)dy < range && (flags & 1) && (U32(r + 0x10) & 0x7fffffff) == 0) F32(r + 0x10) = clock;
                const float f = (clock - F32(r + 0x10)) / F32(r + 0xc);
                if (f < 0.0f || ((flags & 1) && (U32(r + 0x10) & 0x7fffffff) == 0)) part = -1.0f;
                else if (s2_bits(f) < 0x3f800001) part = f - (float)(int)f;
                else if (!(flags & 2)) {
                    if (!(flags & 1)) part = f - (float)(int)f;
                    else { F32(r + 0x10) = 0.0f; part = 0.0f; }
                } else part = -1.0f;
                if (part != -1.0f) {
                    float place[3], way[3];
                    s2_path_at(I32(r + 0x1c), part, place, way);
                    I32(r + 0x30) = (int32_t)place[0]; I32(r + 0x34) = (int32_t)place[2]; I32(r + 0x38) = (int32_t)place[1];
                    /* which way it faces (the game puts these in the object, +0x18 and +0x14, in
                       2048ths of a turn): round, from the path's direction on the ground; then up
                       or down, from its climb against what is left along the ground */
                    {
                        float turn = fabsf(way[1]) == 0.0f ? 1.5707964f : (float)atan(fabs((double)way[0] / (double)way[1]));
                        if (0.0f < way[1]) turn = 3.14159274f - turn;
                        if (way[0] < 0.0f) turn = 6.28318548f - turn;
                        const float sn = (float)sin(-(double)turn), cs = (float)cos(-(double)turn);
                        const float along = way[0] * sn + way[1] * cs;
                        float climb = fabsf(along) == 0.0f ? 1.5707964f : (float)atan(fabs((double)way[2] / (double)along));
                        if (0.0f < along) climb = 3.14159274f - climb;
                        if (way[2] < 0.0f) climb = 6.28318548f - climb;
                        F32(r + 0x3c) = (float)((int)((turn * 1024.0f) / 3.14159274f) & 0x7ff) * (3.14159274f / 1024.0f);    /* (the port keeps them in the row) */
                        F32(r + 0x44) = (float)((int)((climb * 1024.0f) / 3.14159274f) & 0x7ff) * (3.14159274f / 1024.0f);
                    }
                    shown = 1;
                }
            }
        } else if (own && (flags & 1) && !(flags & 2) && (U32(r + 0x10) & 0x7fffffff) != 0) F32(r + 0x10) = 0.0f;
        I32(r + 0x40) = shown && own;
        if (I32(r + 0x24) != 0 && !(U32(0x11f14c) & 1)) {
            const int slot = 7 + (int)i;
            if (!shown) { if (I32(r + 0x48)) FUN_000aee98(0, slot); }
            else if (!I32(r + 0x48)) FUN_000aea54(0, I32(r + 0x24), slot, 0, 1, 1);
            else {
                I32(0x124280) = slot;
                I32(0x124284) = I32(CAMERA + 0x48); I32(0x124288) = I32(CAMERA + 0x50);
                F32(0x12428c) = F32(CAMERA + 0x58) + -3.14159274f;
                I32(0x124290) = I32(r + 0x30); I32(0x124294) = I32(r + 0x38);
                I32(0x124298) = 80000; F32(0x12429c) = 0.0f;
                FUN_000af168(0, 0x124280);
            }
            I32(r + 0x48) = shown;
        }
    }
}

/* the animations that are to be seen: the object's name, where, which way, whether it faces the screen */
int s2_anims(s2_anim *out, int max)
{
    int n = 0;
    for (uint32_t i = 0; i < (uint32_t)s2_nanim && n < max; i++) {
        const uint32_t r = 0x126d5c + i * 0x4c;
        if (!I32(r + 0x40)) continue;
        out[n].id = (int)i;
        out[n].name = (const char *)MP(r);
        out[n].x = (float)I32(r + 0x30); out[n].height = (float)I32(r + 0x34); out[n].y = (float)I32(r + 0x38);
        out[n].heading = F32(r + 0x3c); out[n].climb = F32(r + 0x44);
        out[n].faces_screen = (I32(r + 0x18) & 4) != 0;
        n++;
    }
    return n;
}

/* the cars' line: MAPnOPP.SPL into the block as the game keeps it (0x321a4) -- a table of
   0x24-byte entries (+0 the nodes, +0x1c how many, +0x20 the first) at the address in
   0x127350, their number in 0x127354 -- and each walked once (0x309b0) */
static void s2_line_load(const char *file)
{
    FILE *f = os_fopen(file, "rb");
    int32_t hdr[2];
    const uint32_t table = s2_alloc(16 * 0x24);
    int n = 0;
    U32(0x127350) = table;
    while (f && n < 16 && fread(hdr, 4, 2, f) == 2 && hdr[0] > 0 && hdr[0] < 1024) {
        const uint32_t nodes = s2_alloc((uint32_t)hdr[0] * 0x38);
        if (!nodes || fread(MP(nodes), 0x38, (size_t)hdr[0], f) != (size_t)hdr[0]) break;
        U32(table + (uint32_t)n * 0x24) = nodes;
        I32(table + (uint32_t)n * 0x24 + 0x1c) = hdr[0];
        I32(table + (uint32_t)n * 0x24 + 0x20) = hdr[1];
        n++;
    }
    if (f) fclose(f);
    I32(0x127354) = n;
    for (int i = 0; i < n; i++) FUN_000309b0((int)(table + (uint32_t)i * 0x24));
}

/*
 * TIME ATTACK (the mode word 0x130d10 = 4; 0x339e7's third kind of game): two cars, three
 * laps, the second car the GHOST -- the player's best race so far on this track, driven
 * again by the game's own routines from what was written down of it: for every frame its
 * two times (0x133350, 0x133354) and the six controls (0x4a59a writes a record of 0x20
 * bytes, 0x4a488 reads one).  The frame is 0x2c132: the player's car as ever, and the
 * ghost's car run in ITS OWN frames until its clock (0x133364) has reached the race's,
 * then shown part-way through its last (0x16878).  Nothing touches the ghost and it
 * touches nothing; only the player's car knocks obstacles (0x2a639).
 *   0x1330ac the ghost's records, 0x1330c8 how many, 0x1330c0 the next; 0x1330b8 it runs
 *   0x1330b0 this race's records (the second half of the same block), 0x1330cc how many
 *   0x1330bc room in the block (records), 0x1330b4 there is a block, 0x1330d0 writing
 *   0x11f0cc the ghost's car, 0x11f0c8 / 0x11f0d0 the player's, 0x11f0a4 the ghost is shown
 * The race is kept as the next ghost when it ends with the player's car FIRST (0x2b2a8:
 * car +0xc30 == 1 and done) -- so the first race always, later ones only if the ghost was
 * beaten -- into the game's replays folder as attack<track>.rpl (0x326e2; read by 0x327ed):
 * how many frames, WHICH CAR the player's was, the race's record, the player's controls
 * record (how its six controls are to be read: the routines look at it), the frames.  The
 * two cars start side by side (0x2f519: 1000 to either side by the car's number, no row
 * behind), so the ghost has to be the car it was: with a ghost of car n the player gets car
 * 1 - n, and the two change sides every time the ghost is beaten.  The port keeps the same
 * things in a file of its own, openscreamer.ghosts/s2attack<track>.rpl.
 */
int s2_mode = 1;                           /* the kind of game: 1 arcade, 2 championship, 4 time attack, 8 free run */
int s2_ghost_on = 1;                       /* TIME ATTACK GHOST CAR */
#define S2_GHOST_MAX 7000                  /* frames of a race: six minutes and a half */
#define S2_REPLAY_MAX (2 * S2_GHOST_MAX)   /* a replay's: thirteen minutes */
typedef struct { int play, count, me; uint8_t rec[S2_REPLAY_MAX][0x20]; } s2_replay_t;
static struct { int ok, count, track, car; int32_t entry[11]; uint8_t keys[0xa8], rec[S2_GHOST_MAX][0x20]; } s2_ghost;
static int s2_me;                          /* the player's car: 0 but against a ghost that was car 0 */

static void s2_ghost_name(char *out, size_t size, int track)
{
    snprintf(out, size, "REPLAYS/s2attack%d.rpl", track);          /* (os_fopen: the port's openscreamer.ghosts) */
}

/* before the cars are loaded: is there a ghost for this track, and which car is it */
int s2_attack_plan(int track, int *team, int *model)
{
    char name[64];
    int32_t head[5];
    FILE *f;
    s2_ghost.ok = 0; s2_ghost.count = 0; s2_ghost.track = track;
    s2_me = 0;
    if (s2_mode != 4) return 0;
    const int my_team = team[0], my_model = model[0];
    team[1] = team[0]; model[1] = model[0];
    s2_ghost_name(name, sizeof name, track);
    f = os_fopen(name, "rb");
    if (!f) return 0;
    if (fread(head, 4, 5, f) == 5 && head[0] == 0x4732534f && head[1] == 2 && head[2] == track && head[3] > 1 && head[3] < S2_GHOST_MAX
        && (head[4] == 0 || head[4] == 1) && fread(s2_ghost.entry, 4, 11, f) == 11 && fread(s2_ghost.keys, 1, 0xa8, f) == 0xa8
        && fread(s2_ghost.rec, 0x20, (size_t)head[3], f) == (size_t)head[3]
        && s2_ghost.entry[0] >= 0 && s2_ghost.entry[0] < 4 && s2_ghost.entry[1] >= 0 && s2_ghost.entry[1] < 5) {
        s2_ghost.ok = 1; s2_ghost.count = head[3]; s2_ghost.car = head[4];
        s2_me = 1 - s2_ghost.car;
        team[s2_ghost.car] = s2_ghost.entry[0]; model[s2_ghost.car] = s2_ghost.entry[1];
        team[s2_me] = my_team; model[s2_me] = my_model;
    }
    fclose(f);
    return s2_ghost.ok;
}

/* as the race is left (0x2b2a8): the player's race becomes the ghost if it ended first */
void s2_race_leave(void)
{
    char name[64];
    FILE *f;
    if (!(U32(0x130d10) & 4) || !I32(0x1330d0)) return;
    const uint32_t me = CAR(I32(0x11f0d0));
    I32(0x1330d0) = 0;                                       /* (once) */
    if (getenv("OS_S2GHOSTLOG"))                             /* a test: where the two cars are, and OS_S2GHOSTLOG=2 keeps the race whatever its end */
        for (int i = 0; i < I32(0x130c48); i++)
            printf("ghost: car %d%s at %d %d, speed %.2f, clock %.3f (the ghost's %.3f), frames %d written, %d of %d read\n", i, i == I32(0x11f0cc) ? " (the ghost)" : "",
                   I32(CAR(i) + 0x4d8), I32(CAR(i) + 0x4e0), (double)F32(CAR(i) + 0x4bc), (double)F32(0x13335c), (double)F32(0x133364), I32(0x1330cc), I32(0x1330c0), I32(0x1330c8));
    if ((I32(me + 0xc30) != 1 || !(U8(me + 0x4c) & 1) || I32(0x1330cc) < 2) && !(getenv("OS_S2GHOSTLOG") && atoi(getenv("OS_S2GHOSTLOG")) == 2)) return;
    s2_ghost_name(name, sizeof name, s2_ghost.track);
    f = os_fopen(name, "wb");
    if (!f) return;
    const int32_t head[5] = { 0x4732534f, 2, s2_ghost.track, I32(0x1330cc), I32(0x11f0d0) };
    fwrite(head, 4, 5, f);
    fwrite(MP(ENTRY(I32(0x11f0d0))), 4, 11, f);
    fwrite(MP(KEYS(I32(0x11f0d0))), 1, 0xa8, f);
    fwrite(MP(U32(0x1330b0)), 0x20, (size_t)head[3], f);
    fclose(f);
    if (getenv("OS_S2GHOSTLOG")) printf("ghost: kept %d frames, %.2f s\n", head[3], (double)F32(me + 0xc38));
}

/*
 * THE REPLAY (an arcade or a league's race; 0x2b2a8, 0x2e1f3, 0x2ecb0, 0x4746a, 0x47e75).
 * As for the ghost, nothing but the frames is written down: each frame's two times and the
 * player's six controls (0x4a455 at the start of the race's frame 0x2baa4 -> 0x4a59a), into
 * a block from 0x1330ac.  As the end's screen comes up the race is kept (0x326e2: the
 * frames so far, the player's car, the race's record, into the replays folder as "temp");
 * REPLAY leaves the race with that name kept, and the race is set up again from it
 * (0x2e1f3(0) -> 0x327ed): the mode's 0x100 set, 0x4a407 turns the reading on (0x1330b8),
 * and the same frame 0x2baa4 now reads each frame back (0x4a488: the times, and the controls
 * into car 0x11f0cc's record) -- everything else is worked out again, the same way, from
 * the same start (the random numbers' seed is the race's own).  When the frames run out
 * the player's car is marked done and its controls let go.  In a replay ESC ends it at once
 * (0x2d7c0: 0x11f0a8 |= 8, which stops the race's frames, 0x2b9b1; the car done), F9 is the
 * trackside cameras and F11 follows the next car (0x132b4, 0x2d7c0).  Neither the best times
 * nor a league's places are taken from a replay (0x201ee, 0x4746a, 0x47e75).
 * The port keeps the frames in memory that outlives its parts (os_slot 8), not in a file,
 * and starts the replay as RACE AGAIN starts a race.
 */
static s2_replay_t *s2_replay_kept(void)
{
    s2_replay_t *k = os_slot(8);
    if (!k) { k = os_keep_calloc(1, sizeof *k); if (k) os_slot_set(8, k); }
    return k;
}

/*
 * THE REPLAYS AS FILES (the game's folder REPLAYS; the port's openscreamer.s2replays, the files
 * byte for byte the game's).  0x326e2(name) writes <name>.rpl: 0x27c bytes from 0x1330d4 -- the
 * frames' count (0x1330cc), the player's car (0x11f0d0), both players' controls (0x127708,
 * 0x1277c8: 0xa8 each), the race's record as it was at the start (0x130d6c, copied from
 * 0x130c48 by the race's load 0x2e1f3: 0x124 bytes) -- then the frames, 0x20 bytes each, from
 * 0x1330ac (time attack: 0x1330b0).  Every race's is written as "temp" as its end screen comes
 * up; 0x353b3, as a car's total goes into its best times (not in time attack), renames it
 * REP<laps 0-3><kind 0 championship, 1 arcade, 2 time attack><track 0-6><letter>.RPL, the
 * letter 'A' + the first number from 1 that no row of that list holds at +0x78, and puts the
 * number there (and the name in 0x130d34).  VIEW REPLAY (the best times' page, 0x4254f, on a
 * row whose number is not 0) puts that name in 0x130d34 and starts the race through the
 * menu's loop with 0x1324e0 = -1 (no set-up: 0x339e7 would clear the name); the race's load
 * reads the file (0x327ed(name, 0): the record over 0x130c48 -- but for the volumes, the
 * detail and the view, which stay the player's -- and the controls; then (name, 1) the frames)
 * and runs it as a replay; the menu goes back to the best times' page.  The races are the same
 * again from their frames: nothing in a race draws random numbers but the sparks (0x2aeb9).
 */
static void s2_rpl_file(char *out, size_t n, const char *name)
{
    char up[32];
    size_t i = 0;
    for (; name[i] && i < sizeof up - 1; i++) up[i] = (char)toupper((unsigned char)name[i]);
    up[i] = 0;
    snprintf(out, n, "S2REPLAYS/%s.RPL", up);
}
static void s2_rpl_write(const char *name)            /* 0x326e2 */
{
    char file[64];
    s2_rpl_file(file, sizeof file, name);
    FILE *f = os_fopen(file, "wb");
    if (!f) return;
    I32(0x1330d4) = I32(0x1330cc);
    I32(0x1330d8) = I32(0x11f0d0);
    memmove(MP(0x1330dc), MP(0x127708), 0xa8);
    memmove(MP(0x133184), MP(0x1277c8), 0xa8);
    memmove(MP(0x13322c), MP(0x130d6c), 0x124);
    fwrite(MP(0x1330d4), 0x27c, 1, f);
    const int n = I32(0x1330cc) < S2_REPLAY_MAX ? I32(0x1330cc) : S2_REPLAY_MAX;
    if (n > 0) fwrite(MP(U32((U32(0x130d10) & 4) ? 0x1330b0 : 0x1330ac)), 0x20, (size_t)n, f);
    fclose(f);
    if (getenv("OS_S2REPLAYLOG")) printf("replay: %s written, %d frames\n", file, n);
}
/* 0x353b3's part: the race just kept as "temp" becomes the replay of the row `at` of `list` */
static void s2_rpl_name(uint32_t list, int at, int kind)
{
    char temp[64], file[64], name[16];
    FILE *f;
    s2_rpl_file(temp, sizeof temp, "temp");
    if (!(f = os_fopen(temp, "rb"))) return;
    fclose(f);
    int n = 1;
    for (int r = 0; r < 15; r++) if (I32(list + 0x78 + 4u * (uint32_t)r) == n) { n++; r = -1; }
    I32(list + 0x78 + 4u * (uint32_t)at) = n;
    snprintf(name, sizeof name, "REP%c%c%c%c", '0' + I32(0x128a30), '0' + kind, '0' + I32(0x130c4c), 'A' + n);
    s2_rpl_file(file, sizeof file, name);
    remove(os_path(file));
    rename(os_path(temp), os_path(file));
    s2_str(0x130d34, name);
    if (getenv("OS_S2REPLAYLOG")) printf("replay: kept as %s (row %d)\n", name, at + 1);
}
/* 0x327ed for the menu's VIEW REPLAY: the record into the block and the frames to be run; 0 if
   there is no such file */
int s2_rpl_load(const char *name)
{
    char file[64];
    uint8_t head[0x27c];
    s2_replay_t *k = s2_replay_kept();
    s2_rpl_file(file, sizeof file, name);
    FILE *f = os_fopen(file, "rb");
    if (!f || !k) { if (f) fclose(f); fprintf(stderr, "screamer 2: no replay %s\n", file); return 0; }
    int ok = fread(head, 1, sizeof head, f) == sizeof head;
    int32_t count, me;
    memcpy(&count, head, 4); memcpy(&me, head + 4, 4);
    ok = ok && count > 1 && me >= 0 && me < 4;
    if (ok) {
        const uint32_t keep[4] = { 0x130d5c, 0x130d58, 0x130c54, 0x130d68 };      /* (the player's volumes, detail and view) */
        int32_t was[4];
        for (int i = 0; i < 4; i++) was[i] = I32(keep[i]);
        memcpy(MP(0x130c48), head + 0x158, 0x124);
        for (int i = 0; i < 4; i++) I32(keep[i]) = was[i];
        memcpy(MP(0x127708), head + 8, 0xa8);
        memcpy(MP(0x1277c8), head + 0xb0, 0xa8);
        k->count = count < S2_REPLAY_MAX ? count : S2_REPLAY_MAX;
        k->me = me;
        ok = fread(k->rec, 0x20, (size_t)k->count, f) == (size_t)k->count;
        k->play = ok;
    }
    fclose(f);
    if (getenv("OS_S2REPLAYLOG")) printf("replay: %s %s, %d frames, track %d, mode 0x%x\n", file, ok ? "read" : "NOT read", k->count, I32(0x130c4c) + 1, U32(0x130d10));
    return ok;
}

/* a test (OS_S2REPLAYTEST with OS_S2REPLAYFILE): the frames kept in memory forgotten and the
   replay read back from the file just written */
void s2_replay_from_file(void)
{
    s2_replay_t *k = s2_replay_kept();
    if (!k) return;
    memset(k->rec, 0, sizeof k->rec); k->count = 0; k->play = 0;
    s2_rpl_load("temp");
}

/* as the end's screen comes up (0x4746a / 0x47e75, 0x326e2): the race's frames kept */
void s2_replay_keep(void)
{
    s2_replay_t *k = s2_replay_kept();
    if (!k || (U32(0x130d10) & 0x100) || !I32(0x1330d0) || !I32(0x1330b4)) return;
    k->count = I32(0x1330cc) < S2_REPLAY_MAX ? I32(0x1330cc) : S2_REPLAY_MAX;
    k->me = I32(0x11f0d0);
    memcpy(k->rec, MP(U32(0x1330ac)), (size_t)k->count * 0x20u);
    s2_rpl_write("temp");
}

/* the end's screen offers REPLAY (0x4746a: the block there, and this race written down or a replay) */
int s2_replay_offered(void)
{
    return I32(0x1330b4) && (I32(0x1330d0) || (U32(0x130d10) & 0x100)) && !(U32(0x130d10) & 0xfc & ~0x100u);
}
/* the choice: the next race is the replay (1) or a new one (0) */
void s2_replay_take(int on) { s2_replay_t *k = s2_replay_kept(); if (k) k->play = on; }
int s2_replaying(void) { return (U32(0x130d10) & 0x100) != 0; }

/* the replay's band (0x45882 -> 0x463c0, while the player's car runs): the team of the car
   the cameras follow (0x11f0c8) and the view; 0: not shown */
int s2_replay_band(int *team, int *view)
{
    if (!I32(0x1330b4) || !I32(0x1330b8) || (U32(0x130d10) & 4) || (U8(CAR(I32(0x11f0d0)) + 0x4c) & 1)) return 0;
    *team = I32(ENTRY(I32(0x11f0c8)));
    *view = I32(CAMERA + 0xae4);
    return 1;
}

/* ESC in a replay (0x2d7c0): the rest of it is not run, the player's car is done */
void s2_replay_stop(void)
{
    U32(0x11f0a8) |= 8;
    U8(CAR(I32(0x11f0d0)) + 0x4c) |= 1;
    I32(CAR(I32(0x11f0d0)) + 4) = 0;
}

/* F11 in a replay (0x2d7c0): the cameras follow the next car (while the player's runs) */
void s2_replay_next_car(void)
{
    if (!(U32(0x130d10) & 0x100) || (U8(CAR(I32(0x11f0d0)) + 0x4c) & 1)) return;
    I32(0x11f0c8) = (I32(U32(CAMERA + 0xadc)) + 1) % I32(0x130c48);
    S2_FPU_IN; FUN_00012d00((int)CAMERA, (int)CAR(I32(0x11f0c8))); S2_FPU_OUT;
}

/*
 * THE CHAMPIONSHIP (mode 2; the menu's 0x431ca LEAGUE SELECTION, 0x339e7 case 0 a race's
 * set-up, 0x47e75 the race's end, 0x43914 LEAGUE REPORT).
 *   A league (table 0x1130b0, seven numbers each): how many races; what winning it opens --
 *   the tracks' level (0x128a14), the winning team's fifth car (0x128aa0), the next league
 *   (0x128a18); the others' level (0x130d4c); and two numbers for the weather.
 *       league 1:  4 races, level 2 (rookie)      league 3:  6 races, level 0 (pro)
 *       league 2:  5 races, level 1 (normal)      league 4:  6 races, level 1, all on car 5
 *   The races are the tracks in the menu's order (0x113224: 0 2 3 4 5 1 6), from the first.
 *   Four cars: the player's team and car, the others the next teams and the next cars round
 *   (in league 4 every other car is the fifth, 0x339e7).
 *   THE WEATHER: a league has 0, 1, 2, 2 races not in plain daylight (0x128a24, set as the
 *   league is chosen).  Before a race the random numbers are seeded with (race + 1) x a
 *   number taken as the league was chosen (0x1324cc); then, while such races are left: if a
 *   draw's low six bits are under 32, or no more races are left than have to be -- (races -
 *   race - the league's sixth number) <= left -- this is one: by the track's entry of
 *   0x11320c condition 1, condition 2, or one of the two by another draw.
 *   AFTER A RACE the cars get 10, 6, 3, 0 points by their places (0x113180; a player who
 *   left the race is put last, 0x43914), and the report lists the four by points.  When
 *   the league's races are done the player has won it if first in that list.
 */
const int s2_league[4][7] = { { 4, 1, 0, 1, 2, 1, 3 }, { 5, 2, 0, 2, 1, 1, 2 }, { 6, 3, 0, 3, 0, 1, 1 }, { 6, 0, 1, 0, 1, 0, 1 } };
const int s2_track_order[7] = { 0, 2, 3, 4, 5, 1, 6 };
static const int s2_weather_of[7] = { -1, 0, 0, 0, 1, 0, 0 };      /* 0x11320c (six read; the seventh track's is taken as 0) */
unsigned s2_race_seed;                     /* not 0: the random numbers' seed as the race starts */

/* the condition of a league's race, as 0x339e7 draws it; *left is the league's races not
   in daylight still to come; the seed as it is afterwards goes on into the race */
int s2_champ_weather(int league, int race, int *left, unsigned number)
{
    int cond = 0;
    const int track = s2_track_order[race % 7];
    s2_seed = (unsigned)(race + 1) * number;
    if (*left != 0 && ((s2_rand() & 0x3f) < 0x20 || (s2_league[league][0] - race) - s2_league[league][5] <= *left)) {
        *left -= 1;
        cond = s2_weather_of[track] == 0 ? 1 : s2_weather_of[track] == 1 ? 2 : (s2_rand() & 1) + 1;
    }
    s2_race_seed = s2_seed;
    return cond;
}

/* the cars by their places as the race is left (0x47e75's ENTER; 0x43914 for a player who
   left before the end: last, the fourth car taking the player's place) */
int s2_results(s2_result *r)
{
    const int n = I32(0x130c48), me = I32(0x11f0d0);
    for (int i = 0; i < n && i < 4; i++) {
        const int at = I32(CAR(i) + 0xc30) - 1;
        if (at < 0 || at >= 4) continue;
        r[at].car = i; r[at].team = I32(ENTRY(i)); r[at].model = I32(ENTRY(i) + 4);
        r[at].time = F32(CAR(i) + 0xc38); r[at].done = I32(CAR(i) + 0x4a8) >= I32(0x130d48);
    }
    if (!(U8(CAR(me) + 0x4c) & 2) && n == 4) {
        int at = 0;
        while (at < 3 && r[at].car != me) at++;
        if (at != 3) { r[at] = r[3]; r[3].car = me; r[3].team = I32(ENTRY(me)); r[3].model = I32(ENTRY(me) + 4); r[3].time = 3599.999f; r[3].done = 0; }
    }
    return n < 4 ? n : 4;
}

/*
 * THE BEST TIMES.  A list is 300 bytes: fifteen times (floats, the quickest first), +0x3c
 * fifteen names of four bytes, +0x78 fifteen replay numbers, +0xb4 fifteen teams, +0xf0
 * fifteen car models.  From 0x1290bc the lists of TOTAL times: [kind 0 championship, 1
 * arcade, 2 time attack][track 0..6][laps 1, 3, 5, 10] (0x20d0 a kind, 0x4b0 a track); from
 * 0x12f32c the lists of LAP times: [kind][track] (0x834 a kind).  0x128a30 is which length
 * of race the menu has.  A race starts with the lists' first as BEST TOTAL and BEST LAP
 * (0x130d14, 0x130d18: 0x339e7); at its end 0x353b3(kind, 0x13305c) puts every human car
 * that finished into both (time attack: only the player's, and always the 3-lap list) and
 * leaves the car's row, or -1, at 0x13305c[car] -- the end's screen shows the list with
 * that row lit.  The game keeps them in its settings file; the port in a file of its own,
 * openscreamer.s2records (the lists as they are, after a mark).  Before anybody has driven
 * (0x32d53, no settings file) every row is 3599.999 by "MIL", replay 0, +0xb4 = the row & 3,
 * +0xf0 = 0 -- and both players' names (0x1289f4, 0x1289f8) are "MIL" too.
 */
#define S2_REC_AT   0x1290bcu
#define S2_REC_SIZE (3u * 0x20d0u + 3u * 0x834u)
char s2_name[4] = "MIL";                   /* the player's name (0x130c60: three letters; 0x32d53 starts it as "MIL") */
static uint8_t *s2_records(void)
{
    uint8_t *r = os_slot(6);
    if (!r) {
        FILE *f;
        char mark[8] = { 0 };
        r = os_keep_calloc(1, S2_REC_SIZE);              /* (not malloc: a change of part frees that) */
        if (!r) return NULL;
        for (uint32_t l = 0; l < S2_REC_SIZE / 300u; l++)
            for (uint32_t k = 0; k < 15; k++) {
                const float none = 3599.999f;
                const int32_t row = (int32_t)(k & 3);
                memcpy(r + l * 300u + k * 4u, &none, 4);
                memcpy(r + l * 300u + 0x3c + k * 4u, "MIL", 4);
                memcpy(r + l * 300u + 0xb4 + k * 4u, &row, 4);
            }
        f = os_fopen("openscreamer.s2records", "rb");
        if (f) {
            uint8_t *in = malloc(S2_REC_SIZE);
            if (in && fread(mark, 1, 8, f) == 8 && !memcmp(mark, "OS2REC1", 8) && fread(in, 1, S2_REC_SIZE, f) == S2_REC_SIZE) memcpy(r, in, S2_REC_SIZE);
            free(in);
            fclose(f);
        }
        os_slot_set(6, r);
    }
    return r;
}
static int s2_kind(void) { return (U32(0x130d10) & 4) ? 2 : (U32(0x130d10) & 2) ? 0 : (U32(0x130d10) & 1) ? 1 : -1; }
static uint32_t s2_list_total(int kind)
{
    if (kind == 2) return 0x12d388u + (uint32_t)I32(0x130c4c) * 0x4b0u;
    return S2_REC_AT + (uint32_t)I32(0x128a30) * 300u + (uint32_t)I32(0x130c4c) * 0x4b0u + (uint32_t)kind * 0x20d0u;
}
static void s2_list_put(uint32_t list, float time, int car)
{
    int at = 0;
    while (at < 15 && F32(list + (uint32_t)at * 4) < time) at++;
    if (at >= 15) return;
    for (int k = 13; k >= at; k--) {
        const uint32_t e = list + (uint32_t)k * 4;
        U32(e + 4) = U32(e); U32(e + 0x7c) = U32(e + 0x78); U32(e + 0xb8) = U32(e + 0xb4); U32(e + 0xf4) = U32(e + 0xf0); U32(e + 0x40) = U32(e + 0x3c);
    }
    F32(list + (uint32_t)at * 4) = time;
    U32(list + (uint32_t)at * 4 + 0x78) = 0;
    I32(list + (uint32_t)at * 4 + 0xb4) = I32(ENTRY(car));
    I32(list + (uint32_t)at * 4 + 0xf0) = I32(ENTRY(car) + 4);
    memcpy(MP(list + (uint32_t)at * 4 + 0x3c), MP(ENTRY(car) + 8), 4);
}
/* 0x353b3, as the end's screen comes up (once); the player's row in the list of totals, or -1 */
void s2_replay_keep(void);
int s2_records_enter(void)
{
    const int kind = s2_kind();
    uint8_t *r = s2_records();
    if (!I32(0x133054)) s2_replay_keep();
    if (U32(0x130d10) & 0x100) {                               /* (a replay's end puts nothing in: 0x4746a, 0x47e75) */
        if (!I32(0x133054)) for (int i = 0; i < 4; i++) I32(0x13305c + 4 * (uint32_t)i) = -1;
        I32(0x133054) = 1;
        return -1;
    }
    if (kind < 0 || !r || I32(0x133054)) return I32(0x13305c + 4 * (uint32_t)I32(0x11f0d0));
    I32(0x133054) = 1;
    if (getenv("OS_S2ENDTEST")) U8(CAR(I32(0x11f0d0)) + 0x4c) |= 3;      /* (a test of the end's screen: the race as it stands counts) */
    for (int i = 0; i < 4; i++) I32(0x13305c + 4 * (uint32_t)i) = -1;
    for (int i = 0; i < I32(0x130c48); i++) {
        if (((U32(0x130d10) & 4) && i != I32(0x11f0d0)) || !U32(CAR(i) + 0xb4) || !(U8(CAR(i) + 0x4c) & 2)) continue;
        const uint32_t list = s2_list_total(kind);
        int at = 0;
        while (at < 15 && F32(list + (uint32_t)at * 4) < F32(CAR(i) + 0xc38)) at++;
        s2_list_put(list, F32(CAR(i) + 0xc38), i);
        I32(0x13305c + 4 * (uint32_t)i) = at < 15 ? at : -1;
        if (at < 15 && U32(0x130d10) != 4) s2_rpl_name(list, at, kind);
        s2_list_put(0x12f32cu + (uint32_t)I32(0x130c4c) * 300u + (uint32_t)kind * 0x834u, F32(CAR(i) + 0xc40), i);
    }
    memcpy(r, MP(S2_REC_AT), S2_REC_SIZE);
    {
        FILE *f = os_fopen("openscreamer.s2records", "wb");
        if (f) { fwrite("OS2REC1", 1, 8, f); fwrite(r, 1, S2_REC_SIZE, f); fclose(f); }
    }
    return I32(0x13305c + 4 * (uint32_t)I32(0x11f0d0));
}
/* the list the end's screen shows: fifteen times, +0x3c names, +0xb4 teams, +0xf0 models */
const void *s2_records_list(void) { const int kind = s2_kind(); return kind < 0 ? NULL : MP(s2_list_total(kind)); }

/*
 * A FREE RUN (mode 8: TIME ATTACK's other choice on its car screen, 0x3d234 -- "3 LAPS" or
 * "FREE RUN"): the player alone, and the race does not end on the laps (0x201ee).  A lap is
 * timed at five places, A to E (0x1ffcc: every marker passed, car +0x4b0, counted in fives):
 * car +0xc44[5] this lap's five times, +0xc58[5] those of the car's best lap, 0x130d1c[5]
 * those of the best lap of all (0x201ee copies them when a lap ends quicker: +0xc54 against
 * +0xc68 and 0x130d2c -- the fifth time is the lap's).  The race screen shows the five
 * (text at 0x132fc8 + 11 n) and next to each how far from the best lap's (0x133000 + 11 n:
 * a '+' or '-' and the difference, or the best lap's time itself for the one coming); four
 * seconds after a lap ends (0x13304c) the list is cleared.  The best lap's five stay as long
 * as the game is not left for the menu (0x3f7a3 clears them): kept here in a slot that
 * outlives the port's parts.
 */
typedef struct { int track; float split[5]; float best_lap; } s2_kept;
static s2_kept *s2_keep(void)
{
    s2_kept *k = os_slot(5);
    if (!k) { k = os_keep_calloc(1, sizeof *k); if (k) { k->track = -1; os_slot_set(5, k); } }
    return k;
}
void s2_splits_forget(void) { s2_kept *k = s2_keep(); if (k) k->track = -1; }

/* the race screen's part of it (0x45882, mode 8), once a frame of the game's */
static void s2_free_step(void)
{
    const uint32_t me = CAR(I32(0x11f0c8));
    int k = I32(me + 0x4b4) % 5;
    if (0.0f < F32(0x13304c)) {
        F32(0x13304c) = F32(0x13304c) - F32(0x133354);
        if (!(F32(0x13304c) < 0.0f)) return;
        for (uint32_t i = 0; i < 5; i++) { s2_str(0x132fc9 + i * 0xb, "--:--:--"); s2_str(0x133001 + i * 0xb, " --:--:--"); }
        FUN_00037e06(0x133002, 0, 0, 0, F32(0x130d1c));
    }
    F32(me + 0xc44 + 4 * (uint32_t)k) = F32(me + 0xc3c);
    FUN_00037e06(0x132fc9 + (uint32_t)k * 0xb, 0, 0, 0, F32(me + 0xc44 + 4 * (uint32_t)k));
    if (getenv("OS_S2FREELOG")) {                            /* a test */
        static int was = -1;
        if (was != I32(me + 0x4b4)) {
            was = I32(me + 0x4b4);
            printf("free: markers %d (%d), lap %d, clock %.2f, this lap", was, I32(me + 0x4b0), I32(me + 0x4a8), (double)F32(me + 0xc3c));
            for (uint32_t i = 0; i < 5; i++) printf(" %.2f", (double)F32(me + 0xc44 + 4 * i));
            printf(" | best");
            for (uint32_t i = 0; i < 5; i++) printf(" %.2f", (double)F32(0x130d1c + 4 * i));
            printf(" | %s\n", (const char *)MP(0x133000 + (uint32_t)((was + 4) % 5) * 0xb) + 1); fflush(stdout);
        }
    }
}

/* the tables that ease a value towards another for every frame rate 1..50 (0x315a4, as the
   program starts: (0.5 + i * 0.005) to the power 50 / rate); 0x4b016 points 0x1240c0 at the
   frame's row, and the menu's movements (0x33635, 0x11212) and the race's go by it */
static void s2_ease_tables(void)
{
    for (int rate = 0; rate < 50; rate++)
        for (int i = 0; i < 100; i++)
            F32(0x11f2a0 + (uint32_t)(rate * 400 + i * 4)) = (float)pow(0.5 + (double)((float)i * 0.005f), 50.0 / (double)(rate + 1));
}

/* the player's car has done its laps (car +0x4c bit 0: 0x2d7c0 then takes ESC as nothing) */
int s2_player_done(void) { return U8(CAR(s2_me) + 0x4c) & 1; }
/* the effects while the race's pause screen is up (0x49f8e): eased to nothing at 32767 a second,
   and back at once to the effects' volume (0x130d58) as it is left */
void s2_sound_pause(float dt, int on)
{
    if (!on) { s2_master_set(I32(0x130d58)); return; }
    s2_master_set(s2_master - (int)(dt * 32767.0f) < 0 ? 0 : s2_master - (int)(dt * 32767.0f));
}

/* the race record's two volumes, which the race's CD player sets: 0 the CD's (0x130d5c), 1 the
   effects' (0x130d58); 0..0x7fff */
int s2_volume(int which) { return I32(which ? 0x130d58 : 0x130d5c); }
void s2_volume_set(int which, int v) { I32(which ? 0x130d58 : 0x130d5c) = v < 0 ? 0 : v > 0x7fff ? 0x7fff : v; }

/* the race's CD song, as the race's set-up chose it (0 none) */
static int s2_song;
void s2_race_music(void) { if (s2_song > 0 && snd_cd_track() != s2_song) snd_cd(s2_song, 1); else if (s2_song <= 0) snd_cd(0, 0); }

/* THE ATTRACT RACE's frame (mode 0x80, 0x3c9ac): every car the game's driver's (the player's car
   has no controls there); and every whole ten seconds of the race's clock the camera is told to
   go on to the next car at its next trackside camera (0x2d7c0 sets the camera record's +0xae8;
   0x14acf follows (car + 1) % cars when the nearest camera changes) */
void s2_attract_step(void)
{
    s2_robot(1);
    const float c = F32(0x13335c), step = F32(0x133354);
    if ((int)c % 10 == 0 && (int)(c - step) % 10 != 0) I32(CAMERA + 0xae8) = 1;
}

int s2_race_start(int track, int condition, int ncars, const int *team, const int *model)
{
    char file[96];
    if (!s2_reset()) return 0;
    s2_gen_tables();
    s2_nobject = 0;
    s2_found[0] = s2_found[1] = 0;
    /* what the program sets up as it starts (0x2ddea): the steering's range, the tables
       that ease a value towards another for every frame rate 1..50 (0x315a4:
       (0.5 + i * 0.005) to the power 50 / rate), the game's data tables */
    F32(0x112e20) = 0.8f;
    s2_ease_tables();
    s2_dtr("build/dec/GAME/CARS.DTR", "CAR_PARAMS", 0x11c600, 0x58, 0x58);
    s2_dtr("build/dec/GAME/MISC.DTR", "SKILLS", 0x11f0f4, 0x1c, 0x1c);
    s2_dtr("build/dec/GAME/CRUSC.DTR", "CRUSCOTTI", 0x1242b4, 0x58, 0x58);        /* the dashboards (0x2ddea) */
    s2_dtr("build/dec/GAME/MISC.DTR", "DAMAGES", 0x11cce0, 4, 4);
    /* the track's (0x2ef4e): TRACK_DATA, and its six road kinds filed under their numbers */
    snprintf(file, sizeof file, "build/dec/MAP%d/MAP%dDAT.DTR", track, track);
    if (!s2_dtr(file, "TRACK_DATA", 0x1240f8, 0x110, 0x110)) return 0;
    for (uint32_t k = 0; k < 6; k++)
        if (I32(0x124130 + k * 0x24) != -1)
            memcpy(MP(0x11cdec + (uint32_t)I32(0x124130 + k * 0x24) * 0x24), MP(0x124130 + k * 0x24), 0x24);
    snprintf(file, sizeof file, "build/dec/MAP%d/MAP%dOPP.SPL", track, track);
    s2_line_load(file);
    /* the bends' arrows: MAPnDIR.SPL, one list (0x30b10 -> 0x12734c: the address of its
       0x24-byte entry); the next one to come is 0x133080, from 0 (0x2e2c3) */
    {
        int32_t hdr[2];
        FILE *f;
        snprintf(file, sizeof file, "build/dec/MAP%d/MAP%dDIR.SPL", track, track);
        f = os_fopen(file, "rb");
        U32(0x12734c) = 0; I32(0x133080) = 0; F32(0x133090) = 0.0f;
        if (f && fread(hdr, 4, 2, f) == 2 && hdr[0] > 0 && hdr[0] < 1024) {
            const uint32_t entry = s2_alloc(0x24), nodes = s2_alloc((uint32_t)hdr[0] * 0x38);
            if (entry && nodes && fread(MP(nodes), 0x38, (size_t)hdr[0], f) == (size_t)hdr[0]) {
                U32(entry) = nodes; I32(entry + 0x1c) = hdr[0]; I32(entry + 0x20) = hdr[1];
                U32(0x12734c) = entry;
            }
        }
        if (f) fclose(f);
    }
    /* the race record (0x130c48): cars, track, condition, detail; a car: team, model, ...,
       +0x14 its six settings as the attract race has them (0x3c9ac) */
    /* the level (0x130d4c): 0 pro, 1 normal, 2 rookie -- the row of SKILLS (MISC.DTR) from
       which 0x2da06 sets every opponent's handicap (car +0x48) eight times a second, by how
       far ahead of or behind the player it is; and the row of DAMAGES */
    I32(0x130d4c) = s2_level;
    /* the race's length (0x130d48), no time yet for the best lap and the best race (0x130d18,
       0x130d14: 3599.999) and their texts, as 0x2e1f3 leaves them */
    I32(0x130d48) = s2_laps;
    F32(0x130d18) = F32(0x130d14) = 3599.999f;
    U8(0x132fa4) = U8(0x132fb0) = 1;
    FUN_00037e06(0x132fa5, 0, 0, 0, F32(0x130d18)); FUN_00037e06(0x132fb1, 0, 0, 0, F32(0x130d14));
    F32(0x133078) = F32(0x13307c) = 0.0f;
    I32(0x130c48) = ncars; I32(0x130c4c) = track - 1; I32(0x130c50) = condition; I32(0x130c54) = 2;
    /* AN ARCADE RACE, AS THE ORIGINAL HAS IT IN ITS MEMORY (read out of the game running in
       DOSBox from its default menu choices, NOTES2 S2-5w -- not from the menu's code, whose
       branch for another kind of game had been taken for this one): the mode word 0x130d10 =
       1; every car's six settings 2 2 -1 2 2 2 (-1 = the track's own tyres, 0x14e20; with
       the mode's bit 1 that routine leaves the computer's cars' settings alone too); the
       gearbox by itself for the player (+0xc = 1) and 0 in the others' entries; damage on
       (0x130d54), 0x130d64 = 1, 0x130d6c = 4 */
    U32(0x130d10) = 1;
    if (getenv("OS_S2MODE")) U32(0x130d10) = (uint32_t)strtoul(getenv("OS_S2MODE"), NULL, 0);   /* a test: another kind of game (2 championship, 4 time attack, 8 free run) */
    I32(0x130d54) = 1; I32(0x130d64) = 1; I32(0x130d6c) = 4;
    for (int i = 0; i < ncars; i++) {
        static const int settings[6] = { 2, 2, -1, 2, 2, 2 };
        I32(ENTRY(i)) = team[i]; I32(ENTRY(i) + 4) = model[i];
        I32(ENTRY(i) + 0x0c) = i == s2_me && (!getenv("OS_S2AUTO") || atoi(getenv("OS_S2AUTO")));   /* (the game's menu: AUTOMATIC / MANUAL) */
        for (uint32_t k = 0; k < 6; k++) I32(ENTRY(i) + 0x14 + 4 * k) = settings[k];
    }
    /* the player's own set-up (0x339e7 cases 0, 2, 4: the car screen's Custom 1-4, 0x1324e8;
       arcade always the standard one): the six of 0x128aa4[team][car][set-up], into the
       player's entry (0x327ed carries it to the player's car when there is a ghost) */
    if ((s2_mode & (2 | 4 | 8)) && getenv("OS_S2SETUP")) {
        int six[6];
        if (sscanf(getenv("OS_S2SETUP"), "%d,%d,%d,%d,%d,%d", &six[0], &six[1], &six[2], &six[3], &six[4], &six[5]) == 6)
            for (uint32_t k = 0; k < 6; k++) I32(ENTRY(s2_me) + 0x14 + 4 * k) = six[k];
    }
    s2m_race_record();
    if (!getenv("OS_S2MODE")) U32(0x130d10) = (uint32_t)s2_mode;
    /* the CD's song (0x2e1f3): the one MISC.DTR's CD_TRACKS has for the track, round and round,
       when MUSIC is on (0x130d64), at the settings' music volume (0x130d5c >> 7 of 255); not in
       the attract race.  A race of the port's own menu has it on, at full volume.  (A replay
       starts the race again: the song goes on.) */
    {
        const int from_menu = getenv("OS_FROM_MENU") && os_slot(9);
        if (!from_menu) I32(0x130d58) = I32(0x130d5c) = 0x7fff;     /* (no menu: both volumes full -- the pause screen and the
                                                                   CD player work with these two) */
        s2_dtr("build/dec/GAME/MISC.DTR", "CD_TRACKS", 0x11f0d8, 4, 4);
        snd_cd_volume((float)(I32(0x130d5c) >> 7) / 255.0f);
        const int t = I32(0x130c4c), song = getenv("OS_CDTRACK") ? atoi(getenv("OS_CDTRACK"))   /* (a test: another song) */
                                          : t >= 0 && t < 7 ? I32(0x11f0d8 + 4 * (uint32_t)t) : 0;
        s2_song = (from_menu ? I32(0x130d64) != 0 : 1) && !getenv("OS_NOMUSIC") ? song : 0;
        if (!(U32(0x130d10) & 0x80)) s2_race_music();    /* (the attract race: as its picture comes, 0x37ea9 -> 0x4d758) */
        else snd_cd(0, 0);
    }
    if (U32(0x130d10) & 0x80) I32(0x130d48) = 0;         /* (the attract race's record has no laps: 0x3c9ac leaves them as they
                                                            are, nought in the original there -- no car ever finishes) */
    if ((U32(0x130d10) & 2) && getenv("OS_S2SEED")) s2_seed = (uint32_t)strtoul(getenv("OS_S2SEED"), NULL, 10);   /* (a league's race: the draw of its weather came first) */
    /* the best times (0x339e7): the lists, which length of race, and this race's two to beat */
    {
        uint8_t *r = s2_records();
        static const int lap_of[4] = { 1, 3, 5, 10 };
        const int kind = s2_kind();
        if (r) memcpy(MP(S2_REC_AT), r, S2_REC_SIZE);
        s2m_race_records(r, S2_REC_AT, S2_REC_SIZE);
        if (U32(0x130d10) & 4) I32(0x130d48) = 3;
        I32(0x128a30) = 1;
        for (int k = 0; k < 4; k++) if (lap_of[k] == I32(0x130d48)) I32(0x128a30) = k;
        if (kind >= 0 && r) {
            F32(0x130d14) = F32(s2_list_total(kind));
            F32(0x130d18) = F32(0x12f32cu + (uint32_t)I32(0x130c4c) * 300u + (uint32_t)kind * 0x834u);
            FUN_00037e06(0x132fa5, 0, 0, 0, F32(0x130d18)); FUN_00037e06(0x132fb1, 0, 0, 0, F32(0x130d14));
        }
        if (getenv("OS_S2NAME") && strlen(getenv("OS_S2NAME")) == 3) memcpy(s2_name, getenv("OS_S2NAME"), 4);   /* (the game's menu: the name entered) */
        for (int i = 0; i < ncars; i++) memcpy(MP(ENTRY(i) + 8), i == s2_me ? s2_name : "CPU", 4);   /* (0x339e7: the others "CPU", 0x110fd2) */
        I32(0x133054) = 0;
    }
    /* the five places' texts and times (0x2e1f3; the menu's 0x3f7a3 has no best lap yet) */
    {
        s2_kept *k = s2_keep();
        const int mine = k && k->track == track && (U32(0x130d10) & 8);
        for (uint32_t i = 0; i < 5; i++) {
            F32(0x130d1c + 4 * i) = mine ? k->split[i] : 3599.999f;
            s2_str(0x132fc8 + i * 0xb, "\001--:--:--"); s2_str(0x133000 + i * 0xb, "\001 --:--:--");
        }
        if (mine && k->best_lap > 0.0f) { F32(0x130d18) = k->best_lap; FUN_00037e06(0x132fa5, 0, 0, 0, F32(0x130d18)); }
        FUN_00037e06(0x133002, 0, 0, 0, F32(0x130d1c));
        F32(0x13304c) = 0.0f;
        if (k) k->track = track;
    }
    if (U32(0x130d10) & 8) I32(0x130d48) = 1;
    memmove(MP(0x130d6c), MP(0x130c48), 0x124);          /* (0x2e1f3: the record as the race starts -- what its replay's file keeps) */
    if (U32(0x130d10) & 4) {
        /* time attack (0x339e7 case 2, 0x2e1f3, 0x327ed, 0x2ecb0): three laps, the ghost's
           car as its race had it, the block for the two races' records */
        const int have = s2_ghost.ok && s2_ghost_on && ncars > 1;
        const uint32_t block = s2_alloc(2u * S2_GHOST_MAX * 0x20u);
        I32(0x130d48) = 3; I32(0x130d50) = s2_ghost_on;
        const int ghost = 1 - s2_me;
        if (s2_ghost.ok && ncars > 1) { memcpy(MP(ENTRY(ghost)), s2_ghost.entry, 0x2c); memcpy(MP(KEYS(ghost)), s2_ghost.keys, 0xa8); }
        I32(0x11f0cc) = ghost; I32(0x11f0c8) = I32(0x11f0d0) = s2_me; I32(0x11f0a4) = have;
        U32(0x1330ac) = block; U32(0x1330b0) = block + S2_GHOST_MAX * 0x20u;
        I32(0x1330bc) = block ? 2 * S2_GHOST_MAX : 0; I32(0x1330b4) = block != 0;
        I32(0x1330c8) = have && block ? s2_ghost.count : 0;
        if (I32(0x1330c8)) memcpy(MP(block), s2_ghost.rec, (size_t)s2_ghost.count * 0x20u);
        I32(0x1330b8) = I32(0x1330c8) != 0; I32(0x1330c0) = 0;                 /* 0x4a407: the ghost runs */
        I32(0x1330d0) = block != 0; I32(0x1330cc) = I32(0x1330c4) = 0;         /* 0x4a3c4: this race is written down */
        U32(0x11f0a8) |= 4; U32(0x11f0ac) = U32(0x11f0a8);
        F32(0x133364) = F32(0x133370) = F32(0x13336c) = 0.0f;
    }
    if (!(U32(0x130d10) & 0xfc)) {
        /* an arcade or a league's race (0x2ecb0: modes with no bit over 4) is written down
           (0x4a2d4 the block, 0x4a3c4 writing on) -- or, when REPLAY was taken, run again
           from what was (0x327ed, 0x4a407: the mode's 0x100, the frames read back) */
        s2_replay_t *k = s2_replay_kept();
        const uint32_t block = k ? s2_alloc(S2_REPLAY_MAX * 0x20u) : 0;
        U32(0x1330ac) = block; U32(0x1330b0) = block + S2_REPLAY_MAX / 2 * 0x20u;
        I32(0x1330bc) = block ? S2_REPLAY_MAX : 0; I32(0x1330b4) = block != 0;
        I32(0x1330b8) = I32(0x1330d0) = I32(0x1330c0) = I32(0x1330c8) = I32(0x1330cc) = I32(0x1330c4) = 0;
        if (block && k->play && k->count > 1 && k->me == s2_me) {
            memcpy(MP(block), k->rec, (size_t)k->count * 0x20u);
            I32(0x1330c8) = k->count; I32(0x1330b8) = 1;
            I32(0x11f0cc) = I32(0x11f0c8) = I32(0x11f0d0) = k->me;
            U32(0x130d10) |= 0x100;
        } else if (block) I32(0x1330d0) = 1;
        if (k) k->play = 0;
        if (block) { U32(0x11f0a8) |= 4; U32(0x11f0ac) = U32(0x11f0a8); }
    }
    /* where each view's camera is (0x1192f4[view]: +0x48, +0x50 its place, +0x58 its
       heading): the engine sound is placed by it (0x1f756) */
    for (uint32_t k = 0; k < 4; k++) U32(0x1192f4 + 4 * k) = s2_alloc(0xaec);
    /* the first is the player's, as 0x12bd8 and 0x2e1f3 leave it: nothing shown yet, the
       car it follows (+0xadc), its flags (+0xae0 = 9), the view (+0xae4) */
    I32(CAMERA + 0x2c) = I32(CAMERA + 0x20) = I32(CAMERA + 0x28) = I32(CAMERA + 0x24) = -1;
    U32(CAMERA + 0xadc) = CAR(s2_me); I32(CAMERA + 0xae0) = 9;
    /* the third is the mirror's (0x2b873 draws [0x1192fc] when 0x11f0c4 is set): the same
       car, view 5 */
    {
        const uint32_t m = U32(0x1192fc);
        I32(m + 0x2c) = I32(m + 0x20) = I32(m + 0x28) = I32(m + 0x24) = -1;
        U32(m + 0xadc) = CAR(s2_me); I32(m + 0xae0) = 1; I32(m + 0xae4) = 5;
    }
    I32(0x11f0c8) = I32(0x11f0d0) = s2_me;                  /* the car that is the player's */
    /* the player's controls (0x127708): six entries of 0x1c bytes as CONFIG/CONTROLS.CFG has
       them for the keyboard (0x323be reads them; DEFCTRL.CFG is the same) -- in use, device 0,
       kind 1 = a key, 0, 1 for the first two and 0 for the rest, 1, the key's scan code --
       then the six values the port sets (+0xa8) */
    for (uint32_t k = 0; k < 6; k++) {
        static const int scan[6] = { 72, 80, 75, 77, 54, 29 };
        const int entry[7] = { 1, 0, 1, 0, k < 2, 1, scan[k] };
        for (uint32_t j = 0; j < 7; j++) I32(KEYS(s2_me) + k * 0x1c + j * 4) = entry[j];
    }
    F32(0x133350) = F32(0x133354) = 0.02f;
    {
        S2_FPU_IN;
        int bad = 0;
        FUN_0004b016();
        for (int i = 0; i < ncars && !bad; i++) bad = FUN_0002f519((int)CAR(i), S2_TMP, 1, i) != 0;
        S2_FPU_OUT;
        if (bad) return 0;
        if (getenv("OS_S2GHOSTLOG")) for (int i = 0; i < ncars; i++) printf("ghost: set up, mode %u: car %d at %d %d, heading %.3f, controls 0x%x, slot %d\n", U32(0x130d10), i, I32(CAR(i) + 0x4d8), I32(CAR(i) + 0x4e0), (double)F32(CAR(i) + 0x4e8), U32(CAR(i) + 0xb4), I32(ENTRY(i) + 0x10));
    }
    /* the smoke's objects (0x2ef4e, at the top detail): for every car eight "asmoke" (car
       +0x228, 0x28 bytes each) and eight "a2smoke" (+0x368) -- the puffs of 0x204af */
    if (I32(0x130c54) == 2) {
        s2_str(S2_TMP + 0x100, "asmoke"); s2_str(S2_TMP + 0x110, "a2smoke");
        for (int i = 0; i < ncars; i++)
            for (uint32_t k = 0; k < 8; k++) {
                FUN_0004b843(0x11da14, S2_TMP + 0x100, CAR(i) + 0x228 + k * 0x28);
                FUN_0004b843(0x11da14, S2_TMP + 0x110, CAR(i) + 0x368 + k * 0x28);
            }
    }
    /* the sparks' objects (0x30db4): sixty records of 0x3c bytes from 0x125dd4, the object at
       +0x28 -- thirty "aztri" (cars touching, 0x245a3 -> 0x2aeb9) and thirty "az1tri" */
    s2_str(S2_TMP + 0x120, "aztri"); s2_str(S2_TMP + 0x130, "az1tri");
    for (uint32_t k = 0; k < 60; k++) {
        if (FUN_0004b843(0x11da14, S2_TMP + (k < 30 ? 0x120u : 0x130u), 0x125dfc + k * 0x3c)) { I32(0x125e08 + k * 0x3c) = 0; I32(0x125e04 + k * 0x3c) = 0; }
    }
    /* THE OBSTACLES (0x30a4e, 0x30b29): the names of MAPnDAT.DTR's OSTACLES (rows of three, 12
       bytes each, at 0x126be4; how many rows: 0x124660) and, if there are any, the places of
       MAPnOST.SPL (0x124658: its entry; 0x12465c set) -- a node's +4, +8 the place and +0xc
       which row.  0x30b29 makes a record of each (0x3c bytes from 0x124664, as a spark's)
       and stands it on the ground along the road. */
    {
        int32_t hdr[2];
        FILE *f;
        snprintf(file, sizeof file, "build/dec/MAP%d/MAP%dDAT.DTR", track, track);
        I32(0x124660) = s2_dtr(file, "OSTACLES", 0x126be4, 0x24, 0x24);
        I32(0x12465c) = 0; U32(0x124658) = 0; I32(0x126d4c) = 0;
        snprintf(file, sizeof file, "build/dec/MAP%d/MAP%dOST.SPL", track, track);
        f = I32(0x124660) > 0 && !getenv("OS_NOPROPS") ? os_fopen(file, "rb") : NULL;
        if (f && fread(hdr, 4, 2, f) == 2 && hdr[0] > 0 && hdr[0] <= 64) {
            const uint32_t entry = s2_alloc(0x24), nodes = s2_alloc((uint32_t)hdr[0] * 0x38);
            if (entry && nodes && fread(MP(nodes), 0x38, (size_t)hdr[0], f) == (size_t)hdr[0]) {
                U32(entry) = nodes; I32(entry + 0x1c) = hdr[0]; I32(entry + 0x20) = hdr[1];
                U32(0x124658) = entry; I32(0x12465c) = 1;
            }
        }
        if (f) fclose(f);
        S2_FPU_IN;
        if (FUN_00030b29(1, 0)) { fprintf(stderr, "screamer 2: the obstacles could not all be made\n"); I32(0x126d4c) = 0; }
        S2_FPU_OUT;
    }
    s2_anims_start(track);
    /* the trackside cameras (0x14d00): CAMERASn.MCM is their number, then 0x20 bytes each --
       place, three angles, the kind 0..4 (+0x18) and a figure for it (+0x1c) -- to the
       camera record's +0xa60 and +0x60 (80 at most) */
    {
        snprintf(file, sizeof file, "build/dec/MAP%d/CAMERAS%d.MCM", track, track);
        FILE *f = fopen(file, "rb");
        int32_t n = 0;
        if (f && fread(&n, 4, 1, f) == 1 && n > 0 && n <= 80 && fread(MP(CAMERA + 0x60), 0x20, (size_t)n, f) == (size_t)n)
            I32(CAMERA + 0xa60) = n;
        if (f) fclose(f);
    }
    /* the view a race starts in is the options' (0x128a68 -> 0x130d68; 0x2e1f3 takes 2, 3,
       4 or 6 and makes anything else 2), and the options start with 2 (0x32d53); a replay
       starts with the trackside cameras (0x2e1f3: view 1) */
    {
        const int v = I32(0x130d68);
        s2_view((U32(0x130d10) & 0x180) ? 1 : v == 3 || v == 4 || v == 6 ? v : 2);   /* (and the attract race's: view 1) */
    }
    return 1;
}

/* THE GAME'S VIEWS (0x12dff, one routine each): 2 from the car, 400 above its place
   (0x13bab); 6 the same 300 above (0x13cbd); 4 at the car's own "POSIZIONE TELECAMERA" of
   CARS.DTR, turning and leaning half with the car (0x14200); 3 from 1300 behind, looking
   0.274 down (0x13f38); 5 backwards (the mirror); 1 the trackside cameras.  The keys' order
   in 0x132b4 is 2, 6, 4, 3. */
void s2_view(int view)
{
    /* 0x12dff takes the view keys only while the camera's flag 2 is clear (0x201ee sets it,
       0xb, when the player's car has done its laps) and the game's flag 0x10 is */
    if (I32(CAMERA + 0x2c) != -1 && ((U8(CAMERA + 0xae0) & 2) || (U32(0x11f0a8) & 0x10))) return;
    FUN_00012d31((int)CAMERA, view);
    I32(CAMERA + 0x24) = -1; I32(CAMERA + 0x44) = 0;
}

void s2_camera(s2_camera_state *c)
{
    c->x = I32(CAMERA + 0x48); c->height = I32(CAMERA + 0x4c); c->y = I32(CAMERA + 0x50);
    for (uint32_t k = 0; k < 3; k++) c->angle[k] = F32(CAMERA + 0x54 + 4 * k);
    c->view = I32(CAMERA + 0xae4);
    c->shot = I32(CAMERA + 0x24);
    c->rows = I32(CAMERA + 0x1c);
    c->fade_near = -I32(0x11373c); c->fade_far = -I32(0x113738);
}

int s2_level = 1;                           /* (an arcade race of the original's with its options as they start: 1) */
int s2_laps = 3;

void s2_hud(s2_hud_state *h)
{
    const uint32_t me = CAR(I32(0x11f0c8));
    memset(h, 0, sizeof *h);
    h->ncars = I32(0x130c48);
    h->position = I32(me + 0xc30);
    h->laps = I32(0x130d48);
    h->lap = I32(me + 0x4ac) + 1 < h->laps ? I32(me + 0x4ac) + 1 : h->laps;        /* (0x459bf) */
    h->gear = I32(me + 0xc7c);
    h->finished = U8(CAR(I32(0x11f0d0)) + 0x4c) & 1;       /* (the player's car, 0x45882 / 0x47040: in a replay the cameras may follow another) */
    h->wrong_way = F32(me + 0x1f0) == -1.0f;
    h->clock = F32(0x13335c);
    h->this_lap = F32(me + 0xc3c); h->total = F32(me + 0xc38);
    h->best_lap = (const char *)MP(0x132fa4); h->best_total = (const char *)MP(0x132fb0);
    h->message = (const char *)MP(0x132f98); h->message_left = (float *)MP(0x133078);
    h->best_lap_left = (float *)MP(0x13307c);
    h->view = I32(U32(0x1192f4) + 0xae4);
    h->mode = U32(0x130d10);
    h->ghost = I32(0x11f0a4);
    h->lap_best = F32(me + 0xc40);
    h->split_now = (const char *)MP(0x132fc8); h->split_best = (const char *)MP(0x133000);
    h->leader = -1;
    if (!(U32(0x130d10) & 0x30) && (U32(0x130d10) & 1))
        for (int i = 0; i < I32(0x130c48); i++)
            if (i != I32(0x11f0c8) && I32(CAR(i) + 0xc30) == 1) h->leader = i;
    h->bend = F32(0x133090) > 0.0f && !(U8(me + 0x4c) & 1) && h->view != 1 ? I32(0x133088) + 0x1b : -1;
    /* the dashboard is the car's model's; a model 4's is its team's of the second four (0x2e5d0) */
    h->dash = I32(ENTRY(I32(0x11f0c8)) + 4);
    if (h->dash == 4) h->dash = I32(ENTRY(I32(0x11f0c8))) + 4;
    h->dash_rec = MP(0x1242b4 + (uint32_t)(h->dash & 7) * 0x58);
    h->speed = F32(me + 0x500);
    h->revs = F32(me + 0x60) != 0.0f ? F32(me + 0xc78) / F32(me + 0x60) : 0.0f;
    for (int i = 0; i < h->ncars && i < 4; i++) {
        float d = (F32(me + 0x4bc) - F32(CAR(i) + 0x4bc)) / 200000.0f;
        h->team[i] = I32(ENTRY(I32(CAR(i))));
        h->ahead[i] = d > 1.0f ? 1.0f : d < -1.0f ? -1.0f : d;
    }
}

void s2_controls(int accelerate, int brake, int left, int right, int gear_up, int gear_down)
{
    I32(KEYS(s2_me) + 0xa8) = accelerate ? 1023 : 0;
    I32(KEYS(s2_me) + 0xac) = brake ? 1023 : 0;
    I32(KEYS(s2_me) + 0xb0) = left ? 1023 : 0;
    I32(KEYS(s2_me) + 0xb4) = right ? 1023 : 0;
    I32(KEYS(s2_me) + 0xb8) = gear_up ? 1023 : 0;
    I32(KEYS(s2_me) + 0xbc) = gear_down ? 1023 : 0;
}

/* a test: car i put at (x, y), height 0, facing `heading`, as 0x2f519 leaves one on the grid */
void s2_put(int i, int x, int y, float heading)
{
    I32(CAR(i) + 0x4d8) = x; I32(CAR(i) + 0x4e0) = y; I32(CAR(i) + 0x4dc) = 0;
    F32(CAR(i) + 0x4e8) = heading;
    { S2_FPU_IN; FUN_0001d9fc((int)CAR(i)); S2_FPU_OUT; }
    memcpy(MP(CAR(i) + 0x734), MP(CAR(i) + 0x4d8), 0x25c);   /* (the frame before: 0x2f519) */
}

/* a test: the player's car driven by the game's own driver (its controls record, car +0xb4,
   taken away) or given back -- given back when it has done its laps (the game drives a car that
   has, 0x201ee; and a human car's times go into the best times, 0x353b3) */
void s2_robot(int on)
{
    static uint32_t kept;
    if (U8(CAR(s2_me) + 0x4c) & 2) on = 0;
    if (on && U32(CAR(s2_me) + 0xb4)) { kept = U32(CAR(s2_me) + 0xb4); U32(CAR(s2_me) + 0xb4) = 0; }
    else if (!on && kept) { U32(CAR(s2_me) + 0xb4) = kept; kept = 0; }
}
/* the smoke: every puff that is on -- which of the two (1 asmoke, 2 a2smoke: by the road's
   kind), where the engine has it (the object's place, 0x2073c) and how far through, 0 .. 65535 */
int s2_puffs(s2_puff *out, int max)
{
    int n = 0;
    for (int i = 0; i < I32(0x130c48); i++)
        for (uint32_t k = 0; k < 16 && n < max; k++) {
            const uint32_t rec = CAR(i) + 0x228 + k * 0x28;
            if (!I32(rec + 8)) continue;
            const uint32_t o = U32(0x11da20 + U32(rec) * 0x1c);
            out[n].kind = k < 8 ? 1 : 2;
            out[n].car = i;
            out[n].x = I32(o + 8); out[n].height = I32(o + 0xc); out[n].y = I32(o + 0x10);
            out[n].phase = I32(o + 0x20);
            /* a puff of the car the chase view follows is carried with the car (0x2073c: its
               place is the car's + the record's +0xc..0x14, turned with the car) */
            out[n].carried = I32(CAMERA + 0xae4) == 3 && i == I32(U32(CAMERA + 0xadc));
            if (out[n].carried) { out[n].x = I32(rec + 0xc); out[n].height = I32(rec + 0x10); out[n].y = I32(rec + 0x14); }
            n++;
        }
    return n;
}
/* the sparks that are flying (0x2cc74): where, how turned (three angles), which of the two */
int s2_sparks(s2_spark *out, int max)
{
    int n = 0;
    for (uint32_t k = 0; k < 60 && n < max; k++) {
        const uint32_t b = 0x125dd4 + k * 0x3c;
        if (!I32(b + 0x30)) continue;
        if (getenv("OS_S2SPARKLOG")) printf("  spark %u: at %.0f %.0f %.0f going %.0f %.0f %.0f, left %.2f s, object %d, flags %d %d\n", k, (double)F32(b), (double)F32(b + 4), (double)F32(b + 8), (double)F32(b + 0x18), (double)F32(b + 0x1c), (double)F32(b + 0x20), (double)F32(b + 0x24), I32(b + 0x28), I32(b + 0x30), I32(b + 0x34));   /* a test */
        out[n].id = (int)k;
        out[n].kind = k < 30 ? 0 : 1;
        out[n].x = F32(b); out[n].height = F32(b + 4); out[n].y = F32(b + 8);
        for (uint32_t j = 0; j < 3; j++) out[n].angle[j] = F32(b + 0xc + 4 * j);
        n++;
    }
    return n;
}
/* the obstacles (0x2c475, 0x2a639): its object's name, where it is, its three angles -- as
   0x30b29 stands one up they are (pi, the road's direction, pi) on level ground */
int s2_obstacles(s2_obstacle *out, int max)
{
    int n = 0;
    if (getenv("OS_S2SPARKLOG")) printf("  obstacles in the game's list: %d; the first: object %d shown %d flying %d at %.0f %.0f %.0f (objects made: %d)\n", I32(0x126d4c), I32(0x124664 + 0x28), I32(0x124664 + 0x30), I32(0x124664 + 0x34), (double)F32(0x124664), (double)F32(0x124668), (double)F32(0x12466c), s2_nobject);   /* a test */
    for (uint32_t k = 0; k < (uint32_t)I32(0x126d4c) && n < max; k++) {
        const uint32_t b = 0x124664 + k * 0x3c;
        const int o = I32(b + 0x28);
        if (o < 0 || o >= s2_nobject) continue;          /* (+0x30 is "to be placed again", cleared once done: 0x2c475) */
        out[n].id = (int)k;
        out[n].name = s2_object[o].name[0];
        out[n].x = F32(b); out[n].height = F32(b + 4); out[n].y = F32(b + 8);
        for (uint32_t j = 0; j < 3; j++) out[n].angle[j] = F32(b + 0xc + 4 * j);
        out[n].flying = I32(b + 0x34);
        if (getenv("OS_S2SPARKLOG") && atoi(getenv("OS_S2SPARKLOG")) == 2)   /* a test: every obstacle */
            printf("  obstacle %u %s at %.0f %.0f %.0f angles %.3f %.3f %.3f flying %d\n", k, out[n].name, (double)out[n].x, (double)out[n].height, (double)out[n].y,
                   (double)out[n].angle[0], (double)out[n].angle[1], (double)out[n].angle[2], out[n].flying);
        n++;
    }
    return n;
}
/* THE MIRROR (0x2b873, 0x13e1a, 0x754e0): with 0x11f0c4 set and the view not the trackside
   one, the game draws the scene once more from its third camera -- view 5: the car's place,
   400 above its body, looking backwards, 35 rows -- 320 wide, and copies it to the screen at
   (80, 14) every other column and right to left: 160 by 35.  MIRROR_L.PKC goes over it. */
void s2_mirror_set(int on) { I32(0x11f0c4) = on != 0; }
int s2_mirror(s2_camera_state *c)
{
    const uint32_t m = U32(0x1192fc);
    if (!I32(0x11f0c4) || !m || I32(CAMERA + 0xae4) == 1) return 0;
    c->x = I32(m + 0x48); c->height = I32(m + 0x4c); c->y = I32(m + 0x50);
    for (uint32_t k = 0; k < 3; k++) c->angle[k] = F32(m + 0x54 + 4 * k);
    c->view = 5; c->shot = -1; c->rows = I32(m + 0x1c);
    c->fade_near = -I32(0x11373c); c->fade_far = -I32(0x113738);
    return c->rows > 0 && c->rows <= 100;
}
int s2_laps_done(int i) { return I32(CAR(i) + 0x4ac); }

/* a test: how many times the last frame's step was cut short because cars touched (0x11cdb4) */
int s2_touched(void) { return I32(0x11cdb4); }
/* a test: the race's clock, its mode bits, the level's row of SKILLS */
void s2_state(void)
{
    printf("clock %.2f (0x13335c)  mode 0x%x (0x130d10)  flags 0x%x (0x11f0a8)  level %d  skills", (double)F32(0x13335c), U32(0x130d10), U32(0x11f0a8), I32(0x130d4c));
    for (uint32_t k = 0; k < 7; k++) printf(" %.1f", (double)F32(0x11f0f4 + (uint32_t)I32(0x130d4c) * 0x1c + 4 * k));
    printf("\n");
}

/* a test: car i lifted `height` above where it stands and given `speed` units a second along
   its heading (its last frame's corners and place moved back by a step's worth), for cars
   that meet in the air */
void s2_throw(int i, int height, float speed, float step)
{
    const float h = F32(CAR(i) + 0x4e8), dx = sinf(h) * speed * step, dz = -cosf(h) * speed * step;
    I32(CAR(i) + 0x4dc) += height;
    for (uint32_t w = 0; w < 4; w++) {
        F32(CAR(i) + 0x518 + w * 0xc) += (float)height;
        I32(CAR(i) + 0x5a4 + w * 0x70) += height;            /* the wheel's own height (wheel +0x30) */
    }
    memcpy(MP(CAR(i) + 0x734), MP(CAR(i) + 0x4d8), 0x25c);
    I32(CAR(i) + 0x734) -= (int)dx; I32(CAR(i) + 0x73c) -= (int)dz;
    for (uint32_t w = 0; w < 4; w++) { F32(CAR(i) + 0x770 + w * 0xc) -= dx; F32(CAR(i) + 0x778 + w * 0xc) -= dz; }
    for (uint32_t w = 0; w < 4; w++) { F32(CAR(i) + 0x544 + w * 0xc) = dx / step; F32(CAR(i) + 0x54c + w * 0xc) = dz / step; }
}

/* a test's look at a car's record: the words from `from`, as floats and whole numbers */
void s2_peek(int i, uint32_t from, int n)
{
    for (int k = 0; k < n; k++)
        printf("  car +0x%03x  %12.4f  %d\n", from + 4u * (uint32_t)k, (double)F32(CAR(i) + from + 4u * (uint32_t)k), I32(CAR(i) + from + 4u * (uint32_t)k));
}

void s2_go(void) { U32(0x11f0a8) |= 0x20; }               /* the lights are out */

/* (the game has two: the step its routines work with, 0x133350, eased from the time the last
   pictures took, and that time itself, 0x133354, by which the race's clock goes on) */
static float s2_raw_step = -1.0f;
void s2_frame_times(float raw) { s2_raw_step = raw; }
void s2_clock_set(float clock) { F32(0x13335c) = clock; }

/*
 * THE BENDS' ARROWS (0x454fc, from the race screen 0x45882 while the player's car has not
 * finished and the view is not the trackside one; one player).  The list's nodes: +4 x,
 * +8 y, +0xc which arrow.  When the car is within 80 of the next node -- distances in
 * hundredths, so 8000 units -- its arrow is shown for a second (0x133090, counted down by
 * the frame's raw time) and the node after it becomes the next, round the list.  The arrow
 * is sprite 0x1b + its number, centred, at row 0x32.
 */
static void s2_bend(void)
{
    const uint32_t me = CAR(I32(0x11f0d0)), list = U32(0x12734c);
    if (!list || (U32(0x130d10) & 0x1c0) || (U8(me + 0x4c) & 1) || I32(CAMERA + 0xae4) == 1) return;
    const uint32_t node = U32(list) + (uint32_t)I32(0x133080) * 0x38;
    const double dy = (double)(I32(me + 0x4e0) - I32(node + 8)) * 0.01, dx = (double)(I32(me + 0x4d8) - I32(node + 4)) * 0.01;
    if (sqrt(dy * dy + dx * dx) < 80.0) {
        F32(0x133090) = 1.0f;
        I32(0x133088) = I32(node + 0xc);
        if (getenv("OS_S2BENDLOG")) printf("bend: node %d of %d, arrow %d, clock %.1f\n", I32(0x133080), I32(list + 0x1c), I32(node + 0xc), (double)F32(0x13335c));   /* a test */
        if (++I32(0x133080) >= I32(list + 0x1c)) I32(0x133080) = 0;
    }
    F32(0x133090) = F32(0x133090) - F32(0x133354);
    if (F32(0x133090) <= 0.0f) F32(0x133090) = 0.0f;
}

void s2_frame(float step)
{
    S2_FPU_IN;
    F32(0x133350) = step; F32(0x133354) = s2_raw_step >= 0.0f ? s2_raw_step : step;
    /* the frame of the game's main loop (0x2b9b1): the race, the obstacles that are flying,
       the sparks, [the track's animations 0x2cee4: not yet], and every car near the
       player's against the obstacles */
    if (!(U8(0x11f0a8) & 8)) {                            /* (8: a replay ended by ESC, 0x2d7c0 -- the race no longer run) */
        if (U32(0x130d10) & 4) FUN_0002c132(); else FUN_0002baa4();
        FUN_0002c475();
        FUN_0002cc74();
        s2_anims_step();
    }
    for (int i = 0; i < I32(0x130c48); i++) {
        const uint32_t me = CAR(I32(0x11f0d0));
        if (fabs((double)(I32(CAR(i) + 0x4dc) - I32(me + 0x4dc))) + fabs((double)(I32(CAR(i) + 0x4d8) - I32(me + 0x4d8))) < 20000.0) FUN_0002a639((int)CAR(i));
    }
    FUN_00012dff((int)CAMERA, 1);                         /* (0x2b873: before each picture) */
    if (I32(0x11f0c4)) { I32(U32(0x1192fc) + 0xae4) = 5; FUN_00012dff((int)U32(0x1192fc), 1); }   /* (and the mirror's) */
    S2_FPU_OUT;
    s2_bend();
    if (U32(0x130d10) & 8) {
        s2_kept *k = s2_keep();
        s2_free_step();
        if (k) { memcpy(k->split, MP(0x130d1c), 20); k->best_lap = F32(0x130d18); }
    }
    if (getenv("OS_S2TRACEFILE")) {                       /* a test (tools/s2emu.py): the player's car's record after every frame */
        static FILE *f;
        if (!f) f = fopen(getenv("OS_S2TRACEFILE"), "wb");
        if (f) { fwrite(MP(CAR(0)), 1, 0xcc0u * (uint32_t)I32(0x130c48), f); fflush(f); }      /* (every car's) */
    }
}

/* a test (tools/s2emu.py): the whole block as it stands, then the random numbers' seed and
   where the player's camera record is */
void s2_dump(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    const uint32_t more[3] = { s2_seed, CAMERA, s2_hooklog() ? (uint32_t)ftell(s2_hooklog()) : 0u };   /* (and how far the log of answers had got) */
    fwrite(s2_mem, 1, S2_SIZE, f);
    fwrite(more, 4, 3, f);
    fclose(f);
}

void s2_car(int i, s2_car_state *c)
{
    c->x = I32(CAR(i) + 0x4d8); c->height = I32(CAR(i) + 0x4dc); c->y = I32(CAR(i) + 0x4e0);
    c->angle[0] = F32(CAR(i) + 0x4e4); c->angle[1] = F32(CAR(i) + 0x4e8); c->angle[2] = F32(CAR(i) + 0x4ec);
    c->speed = F32(CAR(i) + 0x500);
    c->steer = F32(CAR(i) + 0xbc);
    c->rpm = F32(CAR(i) + 0xc78);
    c->gear = I32(CAR(i) + 0xc7c);
    c->steer_shown = F32(CAR(i) + 0x13c);
    /* time attack's frame (0x2c132), every frame: with no ghost to show (0x11f0a4 = 0: none
       kept for this track, or GHOST CAR off) the ghost's car's model and shadow (car +0xbf0,
       +0xbf8) are taken out of the engine's list (0x4c01c), else put back (0x4bfbe) */
    c->shown = !((U32(0x130d10) & 4) && !I32(0x11f0a4) && i == I32(0x11f0cc));
    {
        const int obj = I32(CAR(i) + 0xbf0);
        c->look = obj >= 0 && obj < S2_OBJECTS ? I32(U32(0x11da20 + (uint32_t)obj * 0x1c) + 0x20) & 0xfe00 : 0;
        c->env = obj >= 0 && obj < S2_OBJECTS ? I32(U32(0x11da20 + (uint32_t)obj * 0x1c) + 0x3c) : 0;
    }
    c->body_block[0] = I32(CAR(i) + 0x1f8); c->body_block[1] = I32(CAR(i) + 0x200);
    c->body_block[2] = I32(CAR(i) + 0x208); c->body_block[3] = I32(CAR(i) + 0xbf4);
    for (uint32_t w = 0; w < 4; w++) {
        c->wheel_block[w][0] = I32(CAR(i) + 0x5c4 + w * 0x70); c->wheel_block[w][1] = I32(CAR(i) + 0x5cc + w * 0x70);
    }
    c->ground = I32(CAR(i) + 0x4f8); c->ground_pitch = F32(CAR(i) + 0xc0c); c->ground_roll = F32(CAR(i) + 0xc14);
    c->body_pitch = F32(CAR(i) + 0xc18); c->body_roll = F32(CAR(i) + 0xc20); c->body_lift = I32(CAR(i) + 0x4f4);
    for (uint32_t w = 0; w < 4; w++) {
        c->wheel_spin[w] = F32(CAR(i) + 0x584 + w * 0x70); c->wheel_in[w] = I32(CAR(i) + 0x5bc + w * 0x70);
        c->wheel_down[w] = I32(CAR(i) + 0x57c + w * 0x70);
        c->wheel_ground[w] = I32(CAR(i) + 0x5b8 + w * 0x70);
        c->wheel_kind[w] = I32(CAR(i) + 0x5d0 + w * 0x70);
    }
}

int s2_player(void) { return I32(0x11f0d0); }
/* the engine's constant in its code (0xb04a8; 0.008 in the program): 0x1f1ef writes 0.002, or 0.03
   in a tunnel, for the car the camera follows */
float s2_env_rate(void) { return F32(0xb04a8); }
