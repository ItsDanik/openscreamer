/*
 * SCREAMER 2: the port's side of the game's own routines.
 *
 * The race routines are the game's, run out of the player's STARTL.EXE by the interpreter
 * of s2_cpu.c; they work on the program's own memory layout (s2_mem.h).  (The developer's
 * build, S2_CONVERTED, has them converted to C by tools/s2c.py instead: the measure the
 * interpreter is checked against, not in the repository.)  This file is what stands around
 * them:
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
#include <string.h>
#include <strings.h>
#include <ctype.h>

#include "s2_mem.h"
#include "s2_port.h"
#include "port.h"
#include "sound.h"

#ifdef S2_CONVERTED
uint8_t  s2_mem[S2_SIZE];
static struct { uint32_t page; uint8_t bytes[0x1000]; } s2_code[8];
#endif
uint32_t s2_sp;
static uint32_t s2_heap;

#define HEAP_AT  0x140000u          /* past the data object (it ends at 0x139c90) and S2_TMP */
#define STACK_AT 0x1e0000u

#define I32(a) (*(int32_t *)MP(a))
#define U32(a) (*(uint32_t *)MP(a))
#define F32(a) (*(float *)MP(a))
#define U8(a)  (*(uint8_t *)MP(a))

void s2_bad_address(uint32_t a)
{
    static int said;
    if (said++ < 8) fprintf(stderr, "screamer 2: a routine reached outside its memory (0x%x)\n", a);
}

#ifdef S2_CONVERTED
uint8_t *s2_code_var(uint32_t a)
{
    if (a >= 0x10000u && a < 0xe5000u - 16)
        for (int i = 0; i < 8; i++) {
            if (!s2_code[i].page) s2_code[i].page = a & ~0xfffu;
            if (s2_code[i].page == (a & ~0xfffu) && (a & 0xfffu) < 0x1000u - 16) return s2_code[i].bytes + (a & 0xfffu);
        }
    s2_bad_address(a);
    return s2_mem;
}
#endif

uint32_t s2_alloc(uint32_t size)
{
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

/* the block as a race finds it; 0 if the game's program cannot be had */
int s2_reset(void)
{
#ifdef S2_CONVERTED
    memset(s2_mem, 0, sizeof s2_mem);
    memset(s2_code, 0, sizeof s2_code);
    s2_gen_strings();
#else
    /* the player's own program, read once: <the game's directory>/STARTL.EXE */
    static int have;
    if (!have) {
        char path[512];
        snprintf(path, sizeof path, "%s/STARTL.EXE", os_data_dir());
        if (!s2_exe_load(os_path(path))) {
            snprintf(path, sizeof path, "%s/startl.exe", os_data_dir());
            if (!s2_exe_load(os_path(path))) return 0;
        }
        have = 1;
    }
    s2_exe_fresh();
    s2_hooks_set();
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
static struct { int handle, sample; float last; } s2_slot[S2_SLOTS];
static int s2_sound_on;
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
    if (getenv("OS_S2SNDGAIN")) s2_sound_gain = (float)atof(getenv("OS_S2SNDGAIN"));
}

static int s2_slot_live(int slot) { return slot >= 0 && slot < S2_SLOTS && s2_slot[slot].handle >= 0 && snd_playing(s2_slot[slot].handle); }
static void s2_slot_rate(int slot, int rate) { if (s2_slot_live(slot)) snd_pitch(s2_slot[slot].handle, (int)((double)rate * 65536.0 / 11025.0)); }
static void s2_slot_volume(int slot, int volume) { if (s2_slot_live(slot)) snd_volume(s2_slot[slot].handle, s2_sound_gain * (float)(volume < 0 ? 0 : volume > 0x7fff ? 0x7fff : volume) / 32767.0f); }

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
    s2_slot_rate(slot, rate);
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
#define HOOK_4a455
#define FUN_0004a455(...) 0          /* the replay's recorder */
#define HOOK_4b49f
#define FUN_0004b49f(...) 0
#define HOOK_4b4d4
#define FUN_0004b4d4(...) 0
#define HOOK_4bfbe
#define FUN_0004bfbe(...) 0
#define HOOK_4c01c
#define FUN_0004c01c(...) 0
#define HOOK_4c0cf
#define HOOK_4c15f
#define HOOK_4eaed
#define FUN_0004eaed(...) 0
#define HOOK_4feb4
#define FUN_0004feb4(...) 0
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

#define HOOK_4b843                     /* an object by its file's name: the shadow -- none yet */
static int FUN_0004b843(uint32_t list, uint32_t name, uint32_t to)
{
    (void)list;
    if (s2_nobject >= S2_OBJECTS) return 3;
    const int k = s2_nobject++;
    const uint32_t object = s2_alloc(0x40 + 0x20);
    U32(0x11da20 + (uint32_t)k * 0x1c) = object;
    U32(0x11da24 + (uint32_t)k * 0x1c) = object + 0x40;
    I32(0x11da28 + (uint32_t)k * 0x1c) = 1;               /* (it has frames: what 0x4c0cf / 0x4c15f ask) */
    s2_object[k].nblock = 0;
    snprintf(s2_object[k].name[0], sizeof s2_object[k].name[0], "%.15s", (const char *)MP(name));
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
#include "s2_gen.inc"
static void s2_hooks_set(void) { }
#else
/*
 * THE HOOKS AS THE INTERPRETER TAKES THEM (s2_cpu.h): the routine's arguments are in eax,
 * edx, ebx, ecx and then on the stack; what it gives back goes in eax.  The second number
 * of s2_hook() is what the routine itself takes off the stack as it returns (its `ret n`).
 * What is NOT hooked now and runs as the game's own code: the angle of a vector (0x10d9e),
 * the camera's look-at (0x338ed), the power (0xafed2), abs (0xafeca).
 */
static int h_nothing(s2_cpu *c) { c->r[S2_EAX] = 0; return 1; }
static int h_37e06(s2_cpu *c) { const uint32_t w = s2_arg(c, 0); float t; memcpy(&t, &w, 4); FUN_00037e06(c->r[S2_EAX], 0, 0, 0, t); return 1; }
static int h_ae6e3(s2_cpu *c) { c->r[S2_EAX] = FUN_000ae6e3(c->r[S2_EAX], c->r[S2_EDX], c->r[S2_EBX]); return 1; }
static int h_ae8b0(s2_cpu *c) { c->r[S2_EAX] = FUN_000ae8b0(c->r[S2_EAX], c->r[S2_EDX], c->r[S2_EBX]); return 1; }
static int h_b3be4(s2_cpu *c) { c->r[S2_EAX] = FUN_000b3be4(c->r[S2_EAX], c->r[S2_EDX]); return 1; }
static int h_b3c2d(s2_cpu *c) { c->r[S2_EAX] = FUN_000b3c2d(c->r[S2_EAX]); return 1; }
static int h_b3c09(s2_cpu *c) { c->r[S2_EAX] = (uint32_t)s2_rand(); return 1; }
static int h_28ce6(s2_cpu *c) { c->r[S2_EAX] = (uint32_t)FUN_00028ce6((int)c->r[S2_EAX], c->r[S2_EDX]); return 1; }
static int h_2a371(s2_cpu *c) { c->r[S2_EAX] = (uint32_t)s2_nearest((int)c->r[S2_EAX]); return 1; }
static int h_4b843(s2_cpu *c) { c->r[S2_EAX] = (uint32_t)FUN_0004b843(c->r[S2_EAX], c->r[S2_EDX], c->r[S2_EBX]); return 1; }
static int h_4bc94(s2_cpu *c) { c->r[S2_EAX] = (uint32_t)FUN_0004bc94(c->r[S2_EAX], c->r[S2_EDX], c->r[S2_EBX]); return 1; }
static int h_4c15f(s2_cpu *c) { const uint32_t w = s2_arg(c, 0); float v; memcpy(&v, &w, 4); c->r[S2_EAX] = (uint32_t)FUN_0004c15f(c->r[S2_EAX], c->r[S2_EDX], 0, 0, v); return 1; }
static int h_4c0cf(s2_cpu *c) { const uint32_t w = s2_arg(c, 0); float v; memcpy(&v, &w, 4); c->r[S2_EAX] = (uint32_t)FUN_0004c0cf(c->r[S2_EAX], c->r[S2_EDX], 0, 0, v); return 1; }
static int h_aea54(s2_cpu *c) { FUN_000aea54(c->r[S2_EAX], (int)c->r[S2_EDX], (int)c->r[S2_EBX], (int)c->r[S2_ECX], (int)s2_arg(c, 0), (int)s2_arg(c, 1)); c->r[S2_EAX] = 0; return 1; }
static int h_aebf0(s2_cpu *c) { FUN_000aebf0(c->r[S2_EAX], (int)c->r[S2_EDX], (int)c->r[S2_EBX], (int)c->r[S2_ECX], (int)s2_arg(c, 0)); c->r[S2_EAX] = 0; return 1; }
static int h_aee18(s2_cpu *c) { FUN_000aee18(c->r[S2_EAX], (int)c->r[S2_EDX], (int)c->r[S2_EBX]); c->r[S2_EAX] = 0; return 1; }
static int h_aee98(s2_cpu *c) { FUN_000aee98(c->r[S2_EAX], (int)c->r[S2_EDX]); c->r[S2_EAX] = 0; return 1; }
static int h_af168(s2_cpu *c) { FUN_000af168(c->r[S2_EAX], c->r[S2_EDX]); c->r[S2_EAX] = 0; return 1; }
static int h_af45c(s2_cpu *c) { FUN_000af45c(c->r[S2_EAX]); c->r[S2_EAX] = 0; return 1; }
static int h_4bc03(s2_cpu *c) { c->r[S2_EAX] = (uint32_t)FUN_0004bc03(c->r[S2_EAX], c->r[S2_EDX], c->r[S2_EBX]); return 1; }
/* the arc cosine (its number comes and goes on the 387's stack): out of range it is taken
   as 0 -- what the program's own error routine (0xbb541) makes of it is not read; in range
   the game's own routine runs */
static int h_4ea98(s2_cpu *c) { const double x = s2_st0(); (void)c; if (x * x <= 1.0) return 0; s2_st0_set(0.0); return 1; }

static void s2_hooks_set(void)
{
    static const struct { uint32_t at; int pop; } nothing[] = {
        { 0x132b4, 0 }, { 0x4f6c4, 0 }, { 0x31b00, 0 }, { 0x31c8c, 0 }, { 0x4a455, 0 },
        { 0x4b49f, 0 }, { 0x4b4d4, 0 }, { 0x4bfbe, 0 }, { 0x4c01c, 0 }, { 0x4eaed, 0 }, { 0x4feb4, 0 },
        { 0xb04a8, 0 },
    };
    s2_hooks_clear();
    for (unsigned k = 0; k < sizeof nothing / sizeof nothing[0]; k++) s2_hook(nothing[k].at, nothing[k].pop, h_nothing);
    s2_hook(0x37e06, 4, h_37e06);
    s2_hook(0xae6e3, 0, h_ae6e3); s2_hook(0xae8b0, 0, h_ae8b0); s2_hook(0xb3be4, 0, h_b3be4);
    s2_hook(0xb3c2d, 0, h_b3c2d); s2_hook(0xb3c09, 0, h_b3c09);
    s2_hook(0x28ce6, 0, h_28ce6); s2_hook(0x2a371, 0, h_2a371);
    s2_hook(0x4b843, 0, h_4b843); s2_hook(0x4bc94, 0, h_4bc94); s2_hook(0x4bc03, 0, h_4bc03);
    s2_hook(0x4ea98, 0, h_4ea98);
    s2_hook(0x4c15f, 4, h_4c15f); s2_hook(0x4c0cf, 4, h_4c0cf);
    s2_hook(0xaea54, 8, h_aea54); s2_hook(0xaebf0, 4, h_aebf0); s2_hook(0xaee18, 0, h_aee18);
    s2_hook(0xaee98, 0, h_aee98); s2_hook(0xaf168, 0, h_af168); s2_hook(0xaf45c, 0, h_af45c);
}
#define FUN_000309b0(a)          ((void)s2_call(0x309b0, (uint32_t)(a), 0, 0, 0, 0, NULL))
#define FUN_0004b016()           ((void)s2_call(0x4b016, 0, 0, 0, 0, 0, NULL))
#define FUN_0002f519(a, b, c, d) ((int)s2_call(0x2f519, (uint32_t)(a), (uint32_t)(b), (uint32_t)(c), (uint32_t)(d), 0, NULL))
#define FUN_00012d31(a, b)       ((void)s2_call(0x12d31, (uint32_t)(a), (uint32_t)(b), 0, 0, 0, NULL))
#define FUN_0001d9fc(a)          ((void)s2_call(0x1d9fc, (uint32_t)(a), 0, 0, 0, 0, NULL))
#define FUN_0002baa4()           ((void)s2_call(0x2baa4, 0, 0, 0, 0, 0, NULL))
#define FUN_0002cc74()           ((void)s2_call(0x2cc74, 0, 0, 0, 0, 0, NULL))
#define FUN_0002c475()           ((void)s2_call(0x2c475, 0, 0, 0, 0, 0, NULL))
#define FUN_0002a639(a)          ((void)s2_call(0x2a639, (uint32_t)(a), 0, 0, 0, 0, NULL))
#define FUN_00030b29(a, b)       ((int)s2_call(0x30b29, (uint32_t)(a), (uint32_t)(b), 0, 0, 0, NULL))
#define FUN_00012dff(a, b)       ((void)s2_call(0x12dff, (uint32_t)(a), (uint32_t)(b), 0, 0, 0, NULL))
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
    for (int rate = 0; rate < 50; rate++)
        for (int i = 0; i < 100; i++)
            F32(0x11f2a0 + (uint32_t)(rate * 400 + i * 4)) = (float)pow(0.5 + (double)((float)i * 0.005f), 50.0 / (double)(rate + 1));
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
    I32(0x130d54) = 1; I32(0x130d64) = 1; I32(0x130d6c) = 4;
    for (int i = 0; i < ncars; i++) {
        static const int settings[6] = { 2, 2, -1, 2, 2, 2 };
        I32(ENTRY(i)) = team[i]; I32(ENTRY(i) + 4) = model[i];
        I32(ENTRY(i) + 0x0c) = i == 0;
        for (uint32_t k = 0; k < 6; k++) I32(ENTRY(i) + 0x14 + 4 * k) = settings[k];
    }
    /* where each view's camera is (0x1192f4[view]: +0x48, +0x50 its place, +0x58 its
       heading): the engine sound is placed by it (0x1f756) */
    for (uint32_t k = 0; k < 4; k++) U32(0x1192f4 + 4 * k) = s2_alloc(0xaec);
    /* the first is the player's, as 0x12bd8 and 0x2e1f3 leave it: nothing shown yet, the
       car it follows (+0xadc), its flags (+0xae0 = 9), the view (+0xae4) */
    I32(CAMERA + 0x2c) = I32(CAMERA + 0x20) = I32(CAMERA + 0x28) = I32(CAMERA + 0x24) = -1;
    U32(CAMERA + 0xadc) = CAR(0); I32(CAMERA + 0xae0) = 9;
    I32(0x11f0c8) = 0;                                      /* the car that is the player's */
    /* the player's controls (0x127708): six entries of 0x1c bytes as CONFIG/CONTROLS.CFG has
       them for the keyboard (0x323be reads them; DEFCTRL.CFG is the same) -- in use, device 0,
       kind 1 = a key, 0, 1 for the first two and 0 for the rest, 1, the key's scan code --
       then the six values the port sets (+0xa8) */
    for (uint32_t k = 0; k < 6; k++) {
        static const int scan[6] = { 72, 80, 75, 77, 54, 29 };
        const int entry[7] = { 1, 0, 1, 0, k < 2, 1, scan[k] };
        for (uint32_t j = 0; j < 7; j++) I32(KEYS(0) + k * 0x1c + j * 4) = entry[j];
    }
    F32(0x133350) = F32(0x133354) = 0.02f;
    {
        S2_FPU_IN;
        int bad = 0;
        FUN_0004b016();
        for (int i = 0; i < ncars && !bad; i++) bad = FUN_0002f519((int)CAR(i), S2_TMP, 1, i) != 0;
        S2_FPU_OUT;
        if (bad) return 0;
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
       4 or 6 and makes anything else 2), and the options start with 2 (0x32d53) */
    s2_view(2);
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
    h->finished = U8(me + 0x4c) & 1;
    h->wrong_way = F32(me + 0x1f0) == -1.0f;
    h->clock = F32(0x13335c);
    h->this_lap = F32(me + 0xc3c); h->total = F32(me + 0xc38);
    h->best_lap = (const char *)MP(0x132fa4); h->best_total = (const char *)MP(0x132fb0);
    h->message = (const char *)MP(0x132f98); h->message_left = (float *)MP(0x133078);
    h->best_lap_left = (float *)MP(0x13307c);
    h->view = I32(U32(0x1192f4) + 0xae4);
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
    I32(KEYS(0) + 0xa8) = accelerate ? 1023 : 0;
    I32(KEYS(0) + 0xac) = brake ? 1023 : 0;
    I32(KEYS(0) + 0xb0) = left ? 1023 : 0;
    I32(KEYS(0) + 0xb4) = right ? 1023 : 0;
    I32(KEYS(0) + 0xb8) = gear_up ? 1023 : 0;
    I32(KEYS(0) + 0xbc) = gear_down ? 1023 : 0;
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
   taken away) or given back */
void s2_robot(int on)
{
    static uint32_t kept;
    if (on && U32(CAR(0) + 0xb4)) { kept = U32(CAR(0) + 0xb4); U32(CAR(0) + 0xb4) = 0; }
    else if (!on && kept) { U32(CAR(0) + 0xb4) = kept; kept = 0; }
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
        n++;
    }
    return n;
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
    FUN_0002baa4();
    FUN_0002c475();
    FUN_0002cc74();
    for (int i = 0; i < I32(0x130c48); i++) {
        const uint32_t me = CAR(I32(0x11f0d0));
        if (fabs((double)(I32(CAR(i) + 0x4dc) - I32(me + 0x4dc))) + fabs((double)(I32(CAR(i) + 0x4d8) - I32(me + 0x4d8))) < 20000.0) FUN_0002a639((int)CAR(i));
    }
    FUN_00012dff((int)CAMERA, 1);                         /* (0x2b873: before each picture) */
    S2_FPU_OUT;
    s2_bend();
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
