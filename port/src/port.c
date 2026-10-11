#define OS_PORT_IMPL
#define _GNU_SOURCE
#include "os_version.h"
#include "port.h"
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <strings.h>

/* a block the port allocated: on a list, so that all of them can be let go at once */
typedef struct block { struct block *prev, *next; size_t size; uint64_t magic; } block;
#define MAGIC 0x4f53626c6f636b21ull

#define PACKS 6          /* containers kept in memory at once */

/* everything that must outlive os_state_restore() lives in this one block */
typedef struct {
    char   data[768], home[768];
    int    dev, argc, has_next, next_argc;
    char  *argv[16];
    char   next_store[3][768];
    char  *next_argv[4];
    block *blocks;
    unsigned char *data_copy;           /* the program's initialised statics as they were */
    size_t data_len;
    size_t *bss_at; uintptr_t *bss_val; size_t bss_n;   /* the few non-zero words of the rest */
    unsigned once[4];
    void  *slot[8];
    /* the game's own containers, read as they are (see packed_open) */
    int    packed;                      /* the data directory is a copy of the GAME, with its DATA and GFX folders */
    char   dir_data[800], dir_gfx[800];
    struct pack { char name[16]; unsigned char *bytes; long len, dir; int nent; unsigned stamp; } pack[PACKS];
    unsigned pack_clock;
} keep;
static keep *K;

/* the program's own writable statics: GNU ld gives their ends these names */
#ifdef OS_OWN_SECTIONS
/* the build has put the port's own statics into sections of their own
   (CMakeLists.txt): what is saved and put back is those and nothing of the
   libraries linked in */
extern char __start_osdata[], __stop_osdata[], __start_osbss[], __stop_osbss[];
#define __data_start __start_osdata
#define _edata       __stop_osdata
#define __bss_start  __start_osbss
#define _end         __stop_osbss
#else
extern char __data_start[], _edata[], __bss_start[], _end[];
#endif

static int is_dir(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static int is_file(const char *dir, const char *name)
{
    char p[1024];
    struct stat st;
    snprintf(p, sizeof p, "%s/%s", dir, name);
    return stat(p, &st) == 0;
}

/*
 * THE GAME'S CONTAINERS, READ DIRECTLY.
 *
 * The port's code asks for "build/dec/<CONTAINER>/<FILE>" -- the layout the
 * unpacking tool makes (tools/extract.sh).  When the data directory is a copy
 * of the game itself, that same name is answered straight out of
 * DATA/<CONTAINER>.DAT or GFX/<CONTAINER>.DAT, so nothing has to be unpacked
 * first.  A container (see tools/dat_container.py, and CLAUDE.md):
 *     [ file data ][ directory: 32-byte records ][ uint32 directory size ]
 *     record: 16-byte name XOR 0x3A (0x3A ends it), offset, stored size,
 *             real size, method (0 = as it is, else LZW)
 * and the LZW is the game's own (0x4d120...): codes of 9 to 12 bits, lowest
 * bit first, 256 = start again, 257 = end, first free code 258, one more bit
 * when the next free code reaches 512 / 1024 / 2048.
 */
static int ci_find(const char *dir, const char *name, char *out, size_t outsize)
{
    DIR *d = opendir(dir);
    if (!d) return 0;
    struct dirent *e;
    int ok = 0;
    while ((e = readdir(d)) != NULL)
        if (!strcasecmp(e->d_name, name)) { snprintf(out, outsize, "%s/%s", dir, e->d_name); ok = 1; break; }
    closedir(d);
    return ok;
}

static uint32_t le32(const unsigned char *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static long lzw_decode(const unsigned char *src, long n, unsigned char *out, long want)
{
    static const unsigned mask[13] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x1ff, 0x3ff, 0x7ff, 0xfff };
    short prefix[4096];
    unsigned char suffix[4096], stack[4100];
    long bit = 0, got = 0;
    int width = 9, threshold = 0x200, next = 0x102, prev = -1;
    while (got < want) {
        long at = bit >> 3;
        if (bit + width > n * 8) break;
        unsigned v = src[at] | (at + 1 < n ? (unsigned)src[at + 1] << 8 : 0) | (at + 2 < n ? (unsigned)src[at + 2] << 16 : 0);
        int code = (int)((v >> (bit & 7)) & mask[width]);
        bit += width;
        if (code == 0x101) break;
        if (code == 0x100) { width = 9; threshold = 0x200; next = 0x102; prev = -1; continue; }
        int sp = 0, c = code, first;
        if (code >= next) {                         /* the code about to be made: the last string + its own first byte */
            if (prev < 0 || code > next) return -1;
            c = prev;
        }
        while (c > 0xff) { if (sp >= 4096) return -1; stack[sp++] = suffix[c]; c = prefix[c]; }
        first = c;
        stack[sp++] = (unsigned char)c;
        while (sp > 0 && got < want) out[got++] = stack[--sp];
        if (code >= next && got < want) out[got++] = (unsigned char)first;
        if (prev >= 0 && next < 4096) {
            prefix[next] = (short)prev; suffix[next] = (unsigned char)first;
            next++;
            if (next >= threshold && width != 12) { width++; threshold *= 2; }
        }
        prev = code;
    }
    return got;
}

static struct pack *pack_get(const char *name)
{
    struct pack *use = NULL;
    for (int i = 0; i < PACKS; i++) {
        struct pack *p = &K->pack[i];
        if (p->bytes && !strcasecmp(p->name, name)) { p->stamp = ++K->pack_clock; return p; }
    }
    for (int i = 0; i < PACKS; i++) {               /* an empty place, else the one longest unused */
        struct pack *p = &K->pack[i];
        if (!p->bytes) { use = p; break; }
        if (!use || p->stamp < use->stamp) use = p;
    }
    char file[32], path[1024];
    snprintf(file, sizeof file, "%.15s.DAT", name);
    if (!ci_find(K->dir_data, file, path, sizeof path) && !ci_find(K->dir_gfx, file, path, sizeof path)) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *b = len >= 36 ? malloc((size_t)len) : NULL;
    if (!b || fread(b, 1, (size_t)len, f) != (size_t)len) { free(b); fclose(f); return NULL; }
    fclose(f);
    long dsize = (long)le32(b + len - 4);
    if (dsize <= 0 || dsize % 32 || dsize > len - 4) { free(b); return NULL; }
    free(use->bytes);
    use->bytes = NULL;
    snprintf(use->name, sizeof use->name, "%.15s", name);
    use->bytes = b; use->len = len; use->dir = len - 4 - dsize; use->nent = (int)(dsize / 32);
    use->stamp = ++K->pack_clock;
    return use;
}

/* a file out of a container, held in memory and read like any other file */
typedef struct { unsigned char *bytes; long len, at; } membuf;

static ssize_t mem_read(void *c, char *out, size_t n)
{
    membuf *m = c;
    long left = m->len - m->at;
    if (left <= 0) return 0;
    if ((long)n > left) n = (size_t)left;
    memcpy(out, m->bytes + m->at, n);
    m->at += (long)n;
    return (ssize_t)n;
}

static int mem_seek(void *c, off64_t *pos, int whence)
{
    membuf *m = c;
    long to = whence == SEEK_SET ? (long)*pos : whence == SEEK_CUR ? m->at + (long)*pos : m->len + (long)*pos;
    if (to < 0) return -1;
    m->at = to;
    *pos = to;
    return 0;
}

static int mem_close(void *c)
{
    membuf *m = c;
    free(m->bytes);
    free(m);
    return 0;
}

/* "<CONTAINER>/<FILE>" out of the game's containers, as a stream to read; NULL if it is not there */
static FILE *packed_open(const char *rel)
{
    char cname[16];
    const char *slash = strchr(rel, '/');
    if (!slash || slash == rel || slash - rel >= (long)sizeof cname || !slash[1]) return NULL;
    memcpy(cname, rel, (size_t)(slash - rel));
    cname[slash - rel] = 0;
    struct pack *p = pack_get(cname);
    if (!p) return NULL;
    for (int i = 0; i < p->nent; i++) {
        const unsigned char *r = p->bytes + p->dir + 32L * i;
        char nm[17];
        int k = 0;
        while (k < 16 && r[k] != 0x3a) { nm[k] = (char)(r[k] ^ 0x3a); k++; }
        nm[k] = 0;
        if (!k || strcasecmp(nm, slash + 1)) continue;
        long off = (long)le32(r + 16), stored = (long)le32(r + 20), real = (long)le32(r + 24);
        uint32_t method = le32(r + 28);
        if (off < 0 || stored < 0 || off + stored > p->dir || real < 0 || real > (64L << 20)) return NULL;
        unsigned char *buf = malloc((size_t)real + 1);
        if (!buf) return NULL;
        long got = method ? lzw_decode(p->bytes + off, stored, buf, real)
                          : (memcpy(buf, p->bytes + off, (size_t)(stored < real ? stored : real)), stored < real ? stored : real);
        if (got != real) {
            fprintf(stderr, "openscreamer: %s is damaged in the game's files (%ld of %ld bytes)\n", rel, got, real);
            free(buf);
            return NULL;
        }
        if (getenv("OS_PACKDUMP")) {             /* testing: write what was read out of the container */
            char dn[900], fn[1000];
            snprintf(dn, sizeof dn, "%s/%s", getenv("OS_PACKDUMP"), cname);
            mkdir(dn, 0755);
            snprintf(fn, sizeof fn, "%s/%s", dn, nm);
            FILE *o = fopen(fn, "wb");
            if (o) { fwrite(buf, 1, (size_t)real, o); fclose(o); }
        }
        /* a stream over the bytes, which go when it is closed (mem_close) */
        membuf *m = malloc(sizeof *m);
        if (!m) { free(buf); return NULL; }
        m->bytes = buf; m->len = real; m->at = 0;
        static const cookie_io_functions_t io = { mem_read, NULL, mem_seek, mem_close };
        FILE *f = fopencookie(m, "rb", io);
        if (!f) { free(buf); free(m); }
        return f;
    }
    return NULL;
}

/*
 * IS THE GAME ALL THERE?  Asked once, when the program starts.
 *
 * Needed: the sixteen containers of DATA (GEN, LOCAL, MUSIC, SOUND, the six
 * tracks and the six tracks driven the other way) and nine of GFX (GAME,
 * GRAF, MENU and the six tracks' low-resolution pictures).  The six
 * high-resolution containers are wanted too but the game runs without them
 * (it stays in low resolution).  A container counts when it is there, its
 * list of contents is sound, and EVERY file in it unpacks to the size the
 * list gives -- so a copy cut short or damaged is caught here, not half way
 * through a race.
 */
static int pack_whole(const char *path, int *files)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *b = len >= 36 ? malloc((size_t)len) : NULL;
    int ok = b && fread(b, 1, (size_t)len, f) == (size_t)len;
    fclose(f);
    long dsize = ok ? (long)le32(b + len - 4) : 0;
    if (ok && (dsize <= 0 || dsize % 32 || dsize > len - 4)) ok = 0;
    long dir = len - 4 - dsize;
    unsigned char *out = NULL;
    long room = 0;
    for (long i = 0; ok && i < dsize / 32; i++) {
        const unsigned char *r = b + dir + 32 * i;
        if (r[0] == 0x3a) continue;                 /* the empty record that ends the list */
        long off = (long)le32(r + 16), stored = (long)le32(r + 20), real = (long)le32(r + 24);
        if (off < 0 || stored < 0 || off + stored > dir || real < 0 || real > (64L << 20)) { ok = 0; break; }
        if (le32(r + 28)) {
            if (real > room) { free(out); out = malloc((size_t)real + 1); room = real; if (!out) { ok = 0; break; } }
            if (lzw_decode(b + off, stored, out, real) != real) ok = 0;
        } else if (stored != real) ok = 0;
        (*files)++;
    }
    free(out);
    free(b);
    return ok;
}

static void game_check(void)
{
    static const char *const data[] = { "GEN", "LOCAL", "MUSIC", "SOUND", "MAP1", "MAP2", "MAP3", "MAP4", "MAP5", "MAP6",
                                        "MAP1I", "MAP2I", "MAP3I", "MAP4I", "MAP5I", "MAP6I" };
    static const char *const gfx[]  = { "GAME", "GRAF", "MENU", "MAP1L", "MAP2L", "MAP3L", "MAP4L", "MAP5L", "MAP6L" };
    static const char *const high[] = { "MAP1H", "MAP2H", "MAP3H", "MAP4H", "MAP5H", "MAP6H" };
    char bad[1200] = "", path[1024], file[32];
    int files = 0, nbad = 0, hi = 1;
    printf("OpenScreamer %s\n", OS_VERSION);
    printf("Checking game data in %s ... ", K->data);
    fflush(stdout);
    for (int set = 0; set < 3; set++) {
        const char *const *list = set == 0 ? data : set == 1 ? gfx : high;
        int n = set == 0 ? 16 : set == 1 ? 9 : 6;
        for (int i = 0; i < n; i++) {
            int ok;
            const char *what = "missing";
            if (K->packed) {
                snprintf(file, sizeof file, "%s.DAT", list[i]);
                ok = ci_find(set == 0 ? K->dir_data : K->dir_gfx, file, path, sizeof path);
                if (ok && !pack_whole(path, &files)) { ok = 0; what = "damaged"; }
            } else {                                /* files unpacked by tools/extract.sh: the folder must be there */
                snprintf(path, sizeof path, "%s/%s", K->data, list[i]);
                ok = is_dir(path);
            }
            if (ok) continue;
            if (set == 2) { hi = 0; continue; }
            size_t at = strlen(bad);
            snprintf(bad + at, sizeof bad - at, "\n  %s/%s%s  %s", set == 0 ? "DATA" : "GFX", list[i],
                     K->packed ? ".DAT" : " (unpacked folder)", what);
            nbad++;
        }
    }
    if (nbad) {
        printf("FAILED\n");
        fflush(stdout);
        fprintf(stderr, "openscreamer: %d of the game's files cannot be used:%s\n"
                        "Say where a complete copy of Screamer is with -data DIR (the folder that holds its\n"
                        "DATA and GFX folders), or put it next to the program as \"screamer\".\n", nbad, bad);
        exit(1);
    }
    if (K->packed) printf("OK (%d containers, %d files", hi ? 31 : 25, files);
    else           printf("OK (unpacked files");
    printf("%s)\n", hi ? "" : "; no high-resolution files: low resolution only");
    fflush(stdout);
}

int os_exists(const char *path)
{
    FILE *f = os_fopen(path, "rb");
    if (f) fclose(f);
    return f != NULL;
}

static void os_help(const char *me)
{
    printf(
"OpenScreamer " OS_VERSION " -- Screamer (1995) for Linux.  It needs a copy of the original game:\n"
"put the game's folder next to the program as \"screamer\" (or name it with -data).\n"
"\n"
"USAGE\n"
"  %s [options]                     the game: intro, menu, races\n"
"  %s [options] build/dec/MAPn n    straight into a race on track n (1-6)\n"
"  %s [options] build/dec/MAPnI n   the same track driven the other way\n"
"  %s ... --shot FILE [t]           save one picture (PPM) from sample t of the lap and leave\n"
"\n"
"OPTIONS\n"
"  -h, --help     this text\n""  --version      the version, and nothing else\n"
"  -data DIR      your copy of the game: the folder that holds its DATA and GFX folders\n"
"                 default: $OPENSCREAMER_DATA, ./screamer, <program>/screamer\n"
"                 (a folder of files already unpacked with tools/extract.sh works too)\n"
"  -home DIR      where settings and saves are kept\n"
"                 default: $OPENSCREAMER_HOME, this directory if it already holds them,\n"
"                 else ~/.config/openscreamer\n"
"  -d             developer mode: walk / fly round the track, inspection keys\n"
"\n"
"\"build/dec/MAPn\" is only a name for track n: it is found in the game's folder, wherever that is.\n"
"Nothing has to be unpacked: the game's own files are read as they are.\n"
"\n"
"IN THE MENU\n"
"  UP / DOWN      choose a line           LEFT / RIGHT  change an option, the car, the track\n"
"  ENTER          go on                   ESC           back (on the first page: leave)\n"
"  OPTIONS has: DETAIL (LOW, MID, HI as the original; ULTRA draws much further),\n"
"  RESOLUTION (320x200 or 640x400), difficulty, laps, speech, music, records.\n"
"\n"
"IN A RACE\n"
"  UP / DOWN      accelerate / brake      LEFT / RIGHT  steer\n"
"  ALT / CTRL     gear up / down (cars with manual transmission); A / Z do the same\n"
"                 (these six are the game's own CONFIG/CONTROLS.CFG; to change one, a line\n"
"                 in openscreamer.cfg: key_accelerate= key_brake= key_left= key_right=\n"
"                 key_gear_up= key_gear_down=  with a key's name, such as  key_brake=Space)\n"
"  C              next view              F1 bumper cam  F2 cockpit  F3 near chase  F4 far chase\n"
"  M              radar zoom             P   the CD player (music and volumes)\n"
"Game controller (in a race):\n"
"  left stick     steer                   right / left trigger   accelerate / brake\n"
"  d-pad          steer                   A / B                  accelerate / brake\n"
"  shoulders      gear up (right) / down (left)      Y  change view\n"
"  START = ENTER, BACK = ESC; in the menus the d-pad or stick moves, A chooses, B goes back\n"
"  BACKSPACE      back onto the road     ESC leave the race (it asks first)\n"
"  F5             FPS counter            F11 full screen    F12 screenshot (shot_NNN.ppm)\n"
"\n"
"FILES IT KEEPS (in the home directory)\n"
"  openscreamer.cfg       settings        openscreamer.records   best times, name, what is unlocked\n"
"  openscreamer.champ     championship    openscreamer.replays/  replays of record races\n"
"  openscreamer.ghosts/   time-attack ghosts\n"
"\n"
"SWITCHES FOR ONE RUN (environment variables)\n"
"  OS_RES=low|high    OS_DETAIL=0..3    OS_LAPS=n    OS_DIFF=0..3    OS_CAR=n\n"
"  OS_MODE=cones|slalom|attack    OS_NOINTRO=1    OS_NOMUSIC=1    OS_NOSOUND=1\n",
        me, me, me, me);
}

void os_init(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--version")) { printf("OpenScreamer %s\n", OS_VERSION); exit(0); }
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h") || !strcmp(argv[i], "-help")) {
            const char *me = strrchr(argv[0], '/');
            os_help(me ? me + 1 : argv[0]);
            exit(0);
        }
    K = calloc(1, sizeof *K);
    if (!K) exit(1);
    const char *data = getenv("OPENSCREAMER_DATA"), *home = getenv("OPENSCREAMER_HOME");
    K->argv[K->argc++] = argv[0];
    for (int i = 1; i < argc && K->argc < 15; i++) {
        if (!strcmp(argv[i], "-d")) K->dev = 1;
        else if (!strcmp(argv[i], "-data") && i + 1 < argc) data = argv[++i];
        else if (!strcmp(argv[i], "-home") && i + 1 < argc) home = argv[++i];
        else K->argv[K->argc++] = argv[i];
    }
    char exe[600] = "";
    ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n > 0) { exe[n] = 0; char *s = strrchr(exe, '/'); if (s) *s = 0; } else strcpy(exe, ".");
    if (data) snprintf(K->data, sizeof K->data, "%s", data);
    else {
        /* a copy of the game first, then files already unpacked */
        char cand[7][700];
        snprintf(cand[0], 700, "screamer");
        snprintf(cand[1], 700, "%s/screamer", exe);
        snprintf(cand[2], 700, "%s/../../screamer", exe);
        snprintf(cand[3], 700, "build/dec");
        snprintf(cand[4], 700, "%s/data", exe);
        snprintf(cand[5], 700, "%s/../../build/dec", exe);
        snprintf(cand[6], 700, "data");
        snprintf(K->data, sizeof K->data, "%.700s", cand[0]);
        for (int i = 0; i < 7; i++)
            if (is_dir(cand[i])) { snprintf(K->data, sizeof K->data, "%.700s", cand[i]); break; }
    }
    /* the game itself (its DATA and GFX folders, whatever their letters' case),
       or files unpacked from it? */
    K->packed = ci_find(K->data, "DATA", K->dir_data, sizeof K->dir_data)
             && ci_find(K->data, "GFX", K->dir_gfx, sizeof K->dir_gfx)
             && is_dir(K->dir_data) && is_dir(K->dir_gfx);
    if (home) snprintf(K->home, sizeof K->home, "%s", home);
    else if (is_file(".", "openscreamer.cfg") || is_file(".", "openscreamer.records") || is_file(".", "openscreamer.champ"))
        strcpy(K->home, ".");
    else {
        const char *xdg = getenv("XDG_CONFIG_HOME"), *h = getenv("HOME");
        if (xdg && *xdg) snprintf(K->home, sizeof K->home, "%s/openscreamer", xdg);
        else if (h && *h) snprintf(K->home, sizeof K->home, "%s/.config/openscreamer", h);
        else strcpy(K->home, ".");
    }
    if (strcmp(K->home, ".")) {
        char up[768];
        snprintf(up, sizeof up, "%s", K->home);
        char *s = strrchr(up, '/');
        if (s && s != up) { *s = 0; mkdir(up, 0755); }
        mkdir(K->home, 0755);
    }
    if (!is_dir(K->data)) {
        printf("Checking game data ... FAILED\n");
        fflush(stdout);
        fprintf(stderr, "openscreamer: no game at \"%s\".\n"
                        "Say where your copy of Screamer is with -data DIR (the folder that holds its DATA and\n"
                        "GFX folders), or put it next to the program as \"screamer\".\n", K->data);
        exit(1);
    }
    game_check();
}

int   os_dev(void)  { return K && K->dev; }
int   os_argc(void) { return K->has_next ? K->next_argc : K->argc; }
char **os_argv(void) { return K->has_next ? K->next_argv : K->argv; }
const char *os_data_dir(void) { return K ? K->data : "build/dec"; }
const char *os_home_dir(void) { return K ? K->home : "."; }
int   os_has_next(void) { return K->has_next; }
void  os_next_done(void) { K->has_next = 0; }
void *os_slot(int id) { return K->slot[id & 7]; }
void  os_slot_set(int id, void *p) { K->slot[id & 7] = p; }

int os_once(int id)
{
    unsigned bit = 1u << (id & 31), *w = &K->once[(id >> 5) & 3];
    if (*w & bit) return 0;
    *w |= bit;
    return 1;
}

void os_next(int argc, const char *a1, const char *a2)
{
    K->next_argv[0] = K->argv[0];
    if (a1) { snprintf(K->next_store[0], 768, "%s", a1); K->next_argv[1] = K->next_store[0]; }
    if (a2) { snprintf(K->next_store[1], 768, "%s", a2); K->next_argv[2] = K->next_store[1]; }
    K->next_argv[argc] = NULL;
    K->next_argc = argc;
    K->has_next = argc > 0;
}

const char *os_path(const char *path)
{
    static char buf[8][1024];
    static int at;
    if (!K || !path) return path;
    char *out = buf[at++ & 7];
    if (!strncmp(path, "build/dec/", 10)) snprintf(out, 1024, "%s/%s", K->data, path + 10);
    else if (!strcmp(path, "build/dec")) snprintf(out, 1024, "%s", K->data);
    else if (!strncmp(path, "openscreamer.", 13)) snprintf(out, 1024, "%s/%s", K->home, path);
    else if (!strncmp(path, "REPLAYS/", 8)) {            /* time-attack ghosts: the port's own, not the game's directory */
        snprintf(out, 1024, "%s/openscreamer.ghosts", K->home);
        mkdir(out, 0755);
        snprintf(out, 1024, "%s/openscreamer.ghosts/%s", K->home, path + 8);
    }
    else return path;
    return out;
}

/* a file of the game's CONFIG folder (CONTROLS.CFG), any letter case; NULL when
   the data directory is not a copy of the game or has no such file */
FILE *os_config_open(const char *name)
{
    char dir[1024], file[1300];
    if (!K || !ci_find(K->data, "CONFIG", dir, sizeof dir) || !ci_find(dir, name, file, sizeof file)) return NULL;
    return fopen(file, "rb");
}

FILE *os_fopen(const char *path, const char *mode)
{
    if (K && K->packed && path && mode && mode[0] == 'r' && !strncmp(path, "build/dec/", 10))
        return packed_open(path + 10);
    return fopen(os_path(path), mode);
}

void *os_malloc(size_t n)
{
    block *b = malloc(sizeof *b + n);
    if (!b) return NULL;
    b->size = n; b->magic = MAGIC; b->prev = NULL;
    b->next = K ? K->blocks : NULL;
    if (b->next) b->next->prev = b;
    if (K) K->blocks = b;
    return b + 1;
}

void *os_calloc(size_t n, size_t each)
{
    void *p = os_malloc(n * each);
    if (p) memset(p, 0, n * each);
    return p;
}

static void unlink_block(block *b)
{
    if (b->prev) b->prev->next = b->next; else if (K) K->blocks = b->next;
    if (b->next) b->next->prev = b->prev;
}

void os_free(void *p)
{
    if (!p) return;
    block *b = (block *)p - 1;
    if (b->magic != MAGIC) { free(p); return; }          /* not one of ours */
    unlink_block(b);
    b->magic = 0;
    free(b);
}

void *os_realloc(void *p, size_t n)
{
    if (!p) return os_malloc(n);
    block *b = (block *)p - 1;
    if (b->magic != MAGIC) return realloc(p, n);
    void *q = os_malloc(n);
    if (!q) return NULL;
    memcpy(q, p, b->size < n ? b->size : n);
    os_free(p);
    return q;
}

void os_state_save(void)
{
    keep *k = K;
    k->data_len = (size_t)(_edata - __data_start);
    k->data_copy = malloc(k->data_len ? k->data_len : 1);
    memcpy(k->data_copy, __data_start, k->data_len);
    /* the zero-filled statics: a handful of words are not zero already (the
       C library's stdout / stderr, and this file's own K) -- note which */
    uintptr_t *w = (uintptr_t *)(((uintptr_t)__bss_start + sizeof(uintptr_t) - 1) & ~(sizeof(uintptr_t) - 1));
    uintptr_t *end = (uintptr_t *)((uintptr_t)_end & ~(sizeof(uintptr_t) - 1));
    size_t n = 0, cap = 64;
    k->bss_at = malloc(cap * sizeof *k->bss_at);
    k->bss_val = malloc(cap * sizeof *k->bss_val);
    for (uintptr_t *p = w; p < end; p++) {
        if (!*p) continue;
        if (n == cap) {
            cap *= 2;
            k->bss_at = realloc(k->bss_at, cap * sizeof *k->bss_at);
            k->bss_val = realloc(k->bss_val, cap * sizeof *k->bss_val);
        }
        k->bss_at[n] = (size_t)(p - w); k->bss_val[n] = *p; n++;
    }
    k->bss_n = n;
}

void os_state_restore(void)
{
    keep *k = K;
    while (k->blocks) {                                  /* everything a part allocated */
        block *b = k->blocks;
        k->blocks = b->next;
        b->magic = 0;
        free(b);
    }
    memcpy(__data_start, k->data_copy, k->data_len);
    memset(__bss_start, 0, (size_t)(_end - __bss_start));
    uintptr_t *w = (uintptr_t *)(((uintptr_t)__bss_start + sizeof(uintptr_t) - 1) & ~(sizeof(uintptr_t) - 1));
    for (size_t i = 0; i < k->bss_n; i++) w[k->bss_at[i]] = k->bss_val[i];
}
