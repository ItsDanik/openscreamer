/*
 * SCREAMER 2: the port's side of the game's own routines.
 *
 * s2_gen.c holds the game's race routines as tools/s2c.py carries them over; they work on
 * a block of bytes laid out as the program's data object (s2_mem.h).  This file is what
 * stands around them:
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

uint8_t  s2_mem[S2_SIZE];
static struct { uint32_t page; uint8_t bytes[0x1000]; } s2_code[8];
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

void s2_reset(void)
{
    memset(s2_mem, 0, sizeof s2_mem);
    memset(s2_code, 0, sizeof s2_code);
    s2_heap = HEAP_AT;
    s2_sp = STACK_AT;
    s2_gen_strings();
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

/* nothing to do in the port (yet): the engine's lists of what is drawn, its material
   libraries and files, text made with sprintf, and sound */
#define HOOK_204af
#define FUN_000204af(...) 0          /* a wheel's smoke */
#if 0
#define HOOK_22d35
#define FUN_00022d35(...) 0
#endif
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
#define HOOK_132b4
#define FUN_000132b4(...) 0          /* the view keys: the port's own (s2_view) */
#define HOOK_4f6c4
#define FUN_0004f6c4(...) 0          /* the picture's size and eye line (320, 200 rows, 65 % down) */
#define HOOK_af45c
#define FUN_000af45c(...) 0
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
#define FUN_0004c0cf(...) 0
#define HOOK_4eaed
#define FUN_0004eaed(...) 0
#define HOOK_4feb4
#define FUN_0004feb4(...) 0
#define HOOK_aea54
#define FUN_000aea54(...) 0
#define HOOK_aebf0
#define FUN_000aebf0(...) 0
#define HOOK_aee18
#define FUN_000aee18(...) 0
#define HOOK_aee98
#define FUN_000aee98(...) 0
#define HOOK_af168
#define FUN_000af168(...) 0
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

/* bytes copied and filled, whole numbers made positive, names copied, chance */
#define HOOK_ae6e3
static uint32_t FUN_000ae6e3(uint32_t to, uint32_t from, uint32_t n) { memmove(MP(to), MP(from), n); return to; }
#define HOOK_ae8b0
static uint32_t FUN_000ae8b0(uint32_t to, uint32_t value, uint32_t n) { memset(MP(to), (int)value, n); return to; }
#define HOOK_afeca
static int s2_abs(int v) { return v < 0 ? -v : v; }
#define FUN_000afeca(v, ...) s2_abs((int)(v))
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
#define HOOK_28ce6
static uint64_t FUN_00028ce6(int mode, uint32_t edx)
{
    int h, n[3], kind;
    (void)mode;
    I32(0x4cb98) = 0;
    if (!s2p_ground(I32(0x11cdcc), I32(0x11cdd4), &h, n, &kind)) return (uint64_t)edx << 32;
    I32(0x4cb98) = h;
    I32(0x4cba0) = n[0]; I32(0x4cba4) = n[1]; I32(0x4cba8) = n[2];
    I32(0x4cbec) = kind;
    return (uint64_t)edx << 32 | 1u;
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
    if (!s2_found[0]) { s2_found[0] = s2_alloc(16); s2_found[1] = s2_alloc(32); }
    if (getenv("OS_S2NONEAR")) return (uint64_t)edx << 32;      /* a test: no road, no walls (tools/s2emu.py) */
    if (!s2p_nearest(walls, I32(0x11cdcc), I32(0x11cdd4), rec)) return (uint64_t)edx << 32;
    const uint32_t at = s2_found[walls ? 1 : 0];
    F32(at) = rec[0];
    for (int i = 1; i < (walls ? 7 : 3); i++) I32(at + 4u * (uint32_t)i) = (int32_t)rec[i];
    if (walls) { U32(0x11cdd8) = at; return (uint64_t)edx << 32 | 1u; }
    U32(0x11cde8) = at;
    I32(0x11cde0) = I32(at + 4); I32(0x11cde4) = I32(at + 8);
    float a = rec[0];
    if (U32(0x130d30) & 8) a += 3.14159274f;
    if (a > 6.28318548f) a += -6.28318548f;
    F32(0x11cddc) = a;
    return (uint64_t)edx << 32 | 1u;
}

/*
 * THE ENGINE'S LIST OF OBJECTS, as much of it as the car's set-up reads (0x2f519): an
 * object has a number, and at 0x11da24 + number * 0x1c the address of its blocks' records,
 * 0x20 bytes each with the block's place in the object at +0x14 (0x4f9e2).  0x4bc94 gives
 * a car's model its number, 0x4bc03 finds a block of it by name.
 */
#define S2_OBJECTS 16
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
    (void)list; (void)name;
    if (s2_nobject >= S2_OBJECTS) return 3;
    const int k = s2_nobject++;
    const uint32_t object = s2_alloc(0x40 + 0x20);
    U32(0x11da20 + (uint32_t)k * 0x1c) = object;
    U32(0x11da24 + (uint32_t)k * 0x1c) = object + 0x40;
    s2_object[k].nblock = 0;
    I32(to) = k;
    return 0;
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

#include "s2_gen.inc"

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
    s2_reset();
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
    s2_dtr("build/dec/GAME/MISC.DTR", "DAMAGES", 0x11cce0, 4, 4);
    /* the track's (0x2ef4e): TRACK_DATA, and its six road kinds filed under their numbers */
    snprintf(file, sizeof file, "build/dec/MAP%d/MAP%dDAT.DTR", track, track);
    if (!s2_dtr(file, "TRACK_DATA", 0x1240f8, 0x110, 0x110)) return 0;
    for (uint32_t k = 0; k < 6; k++)
        if (I32(0x124130 + k * 0x24) != -1)
            memcpy(MP(0x11cdec + (uint32_t)I32(0x124130 + k * 0x24) * 0x24), MP(0x124130 + k * 0x24), 0x24);
    snprintf(file, sizeof file, "build/dec/MAP%d/MAP%dOPP.SPL", track, track);
    s2_line_load(file);
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
    /* as the menu fills an ordinary race in (0x339e7, its case 0): the mode word 2; the
       player's six settings from the options' first set-up, which starts as 2 2 0 2 2 2
       (0x32d53: steering, gearing, TYRES 0 = slicks, front, rear, brakes); the other cars
       keep what the attract race gave all four, 2 2 -1 2 2 2 (0x3c9ac: -1 = the track's own
       tyres, 0x14e20); the gearbox by itself (+0xc, the options' 0x128a28 = 0) */
    U32(0x130d10) = 2;
    for (int i = 0; i < ncars; i++) {
        static const int mine[6] = { 2, 2, 0, 2, 2, 2 }, theirs[6] = { 2, 2, -1, 2, 2, 2 };
        I32(ENTRY(i)) = team[i]; I32(ENTRY(i) + 4) = model[i];
        I32(ENTRY(i) + 0x0c) = 1;
        for (uint32_t k = 0; k < 6; k++) I32(ENTRY(i) + 0x14 + 4 * k) = (i == 0 ? mine : theirs)[k];
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
    FUN_0004b016();
    for (int i = 0; i < ncars; i++)
        if (FUN_0002f519((int)CAR(i), S2_TMP, 1, i) != 0) return 0;
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
    c->fade_near = -I32(0x11373c); c->fade_far = -I32(0x113738);
}

int s2_level = 2;
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
    FUN_0001d9fc((int)CAR(i));
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

/* a test's look at a car's record: the words from `from`, as floats and whole numbers */
void s2_peek(int i, uint32_t from, int n)
{
    for (int k = 0; k < n; k++)
        printf("  car +0x%03x  %12.4f  %d\n", from + 4u * (uint32_t)k, (double)F32(CAR(i) + from + 4u * (uint32_t)k), I32(CAR(i) + from + 4u * (uint32_t)k));
}

void s2_go(void) { U32(0x11f0a8) |= 0x20; }               /* the lights are out */

void s2_frame(float step)
{
    F32(0x133350) = F32(0x133354) = step;
    FUN_0002baa4();
    FUN_00012dff((int)CAMERA, 1);                         /* (0x2b873: before each picture) */
    if (getenv("OS_S2TRACEFILE")) {                       /* a test (tools/s2emu.py): the player's car's record after every frame */
        static FILE *f;
        if (!f) f = fopen(getenv("OS_S2TRACEFILE"), "wb");
        if (f) { fwrite(MP(CAR(0)), 1, 0xcc0, f); fflush(f); }
    }
}

/* a test (tools/s2emu.py): the whole block as it stands, then the random numbers' seed and
   where the player's camera record is */
void s2_dump(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    const uint32_t more[2] = { s2_seed, CAMERA };
    fwrite(s2_mem, 1, S2_SIZE, f);
    fwrite(more, 4, 2, f);
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
