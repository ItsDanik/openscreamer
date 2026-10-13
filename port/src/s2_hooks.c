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
#define HOOK_2da06
#define FUN_0002da06(...) 0
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
    float a = (float)acos((double)x / sqrt((double)(x * x + y * y)));
    return 0.0f > y ? 6.2831854820251465f - a : a;
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
    I32(0x130c48) = ncars; I32(0x130c4c) = track - 1; I32(0x130c50) = condition; I32(0x130c54) = 2;
    for (int i = 0; i < ncars; i++) {
        static const int settings[6] = { 2, 2, -1, 2, 2, 2 };
        I32(ENTRY(i)) = team[i]; I32(ENTRY(i) + 4) = model[i];
        I32(ENTRY(i) + 0x0c) = 1;                          /* the gearbox by itself */
        for (uint32_t k = 0; k < 6; k++) I32(ENTRY(i) + 0x14 + 4 * k) = settings[k];
    }
    /* where each view's camera is (0x1192f4[view]: +0x48, +0x50 its place, +0x58 its
       heading): the engine sound is placed by it (0x1f756) */
    for (uint32_t k = 0; k < 4; k++) U32(0x1192f4 + 4 * k) = s2_alloc(0x100);
    I32(0x11f0c8) = 0;                                      /* the car that is the player's */
    /* the player's controls (0x127708): the sign each one counts with, and "keys" */
    I32(KEYS(0) + 0x14) = 1; I32(KEYS(0) + 0x30) = 1; I32(KEYS(0) + 0x84) = 1; I32(KEYS(0) + 0xa0) = 1;
    I32(KEYS(0) + 0x40) = 1;
    F32(0x133350) = F32(0x133354) = 0.02f;
    FUN_0004b016();
    for (int i = 0; i < ncars; i++)
        if (FUN_0002f519((int)CAR(i), S2_TMP, 1, i) != 0) return 0;
    return 1;
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
}

void s2_car(int i, s2_car_state *c)
{
    c->x = I32(CAR(i) + 0x4d8); c->height = I32(CAR(i) + 0x4dc); c->y = I32(CAR(i) + 0x4e0);
    c->angle[0] = F32(CAR(i) + 0x4e4); c->angle[1] = F32(CAR(i) + 0x4e8); c->angle[2] = F32(CAR(i) + 0x4ec);
    c->speed = F32(CAR(i) + 0x500);
    c->steer = F32(CAR(i) + 0xbc);
    c->rpm = F32(CAR(i) + 0xc78);
    c->gear = I32(CAR(i) + 0xc7c);
    for (uint32_t w = 0; w < 4; w++) {
        c->wheel_down[w] = I32(CAR(i) + 0x57c + w * 0x70);
        c->wheel_ground[w] = I32(CAR(i) + 0x5b8 + w * 0x70);
        c->wheel_kind[w] = I32(CAR(i) + 0x5d0 + w * 0x70);
    }
}
