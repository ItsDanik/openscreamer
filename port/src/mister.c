/* mister.c - see mister.h */
#define _GNU_SOURCE                 /* (REG_RIP for OS_PROF on the PC) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <mister_hybrid.h>
#include "mister.h"
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <ucontext.h>
#include <unistd.h>
#include <sys/syscall.h>

/*
 * A SAMPLING PROFILE (OS_PROF=<seconds>, for development): 1000 times a second of the program's
 * CPU time the place each thread is at, counted by address; after so many seconds the counts go
 * to /tmp/openscreamer_prof.txt ("address count", the threads together) and the counting stops.
 * tools: addr2line -f -i -e build/mister/openscreamer (the unstripped binary) for each address.
 */
#define PROF_SLOTS 65536
static uint32_t g_prof_pc[PROF_SLOTS], g_prof_n[PROF_SLOTS], g_prof_tid[PROF_SLOTS];
static volatile int g_prof_on;
static void prof_tick(int sig, siginfo_t *si, void *ctx)
{
    (void)sig; (void)si;
    if (!g_prof_on) return;
#if defined(__arm__)
    const uint32_t pc = (uint32_t)((ucontext_t *)ctx)->uc_mcontext.arm_pc;
#elif defined(__x86_64__)
    const uint32_t pc = (uint32_t)((ucontext_t *)ctx)->uc_mcontext.gregs[REG_RIP];
#else
    const uint32_t pc = 0; (void)ctx;
#endif
    const uint32_t tid = (uint32_t)syscall(SYS_gettid);
    uint32_t h = ((pc >> 1) ^ tid * 40503u) * 2654435761u >> 16;
    for (int k = 0; k < 64; k++, h = (h + 1) & (PROF_SLOTS - 1)) {
        if (g_prof_pc[h] == pc && g_prof_tid[h] == tid) { __atomic_add_fetch(&g_prof_n[h], 1, __ATOMIC_RELAXED); return; }
        if (!g_prof_pc[h]) { g_prof_pc[h] = pc; g_prof_tid[h] = tid; __atomic_add_fetch(&g_prof_n[h], 1, __ATOMIC_RELAXED); return; }
    }
}
static void prof_dump(void)
{
    FILE *f = fopen("/tmp/openscreamer_prof.txt", "w");
    if (!f) return;
    for (int i = 0; i < PROF_SLOTS; i++) if (g_prof_n[i]) fprintf(f, "%08x %u %u\n", g_prof_pc[i], g_prof_n[i], g_prof_tid[i]);
    fclose(f);
    {   /* (and where the libraries are, for the addresses in them) */
        FILE *m = fopen("/proc/self/maps", "r"), *o = fopen("/tmp/openscreamer_prof_maps.txt", "w");
        char line[512];
        while (m && o && fgets(line, sizeof line, m)) fputs(line, o);
        if (m) fclose(m);
        if (o) fclose(o);
    }
    printf("profile: written to /tmp/openscreamer_prof.txt\n");
}
/* once a picture (mister_frame_done) */
static void prof_follow(void)
{
    static Uint32 began;
    static int state = -1;
    if (state < 0) state = SDL_getenv("OS_PROF") ? 0 : 2;
    if (state == 2) return;
    if (state == 0) {
        struct sigaction sa;
        memset(&sa, 0, sizeof sa);
        sa.sa_sigaction = prof_tick; sa.sa_flags = SA_SIGINFO | SA_RESTART;
        sigaction(SIGPROF, &sa, NULL);
        struct itimerval it = { { 0, 1000 }, { 0, 1000 } };
        setitimer(ITIMER_PROF, &it, NULL);
        began = SDL_GetTicks(); g_prof_on = 1; state = 1;
        return;
    }
    if (SDL_GetTicks() - began >= (Uint32)SDL_atoi(SDL_getenv("OS_PROF")) * 1000u) {
        g_prof_on = 0;
        struct itimerval off = { { 0, 0 }, { 0, 0 } };
        setitimer(ITIMER_PROF, &off, NULL);
        prof_dump();
        state = 2;
    }
}

#define EXIT_TO_MENU  0
#define EXIT_RESTART  42    /* danik_hybrid_launch.sh starts the game again: the core's list of games */
#define EXIT_RELOADED 43    /* danik_hybrid_launch.sh starts the game again once the core has settled */

int mister_on(void)
{
    return MH_Open();
}

/* "O[27:26],Rumble,Off,On,Strong" in core/OpenScreamer.sv: 0, 1 or 2 */
int mister_rumble_on(void)
{
    int level = MH_Open() ? MH_OSD_GAME_BITS(MH_OSDStatus(), 26, 2) : 1;
    return level > 2 ? 2 : level;
}

/* "O[25:24],Resolution,320x200,320x240,640x200,640x240": bit 0 = 240 rows,
   bit 1 = 640 columns */
int mister_res_mode(void)
{
    return MH_Open() ? MH_OSD_GAME_BITS(MH_OSDStatus(), 24, 2) : 0;
}

void mister_hud_margins(int *top, int *bottom)
{
    *top = *bottom = MH_HUD_DEFAULT;
    if (MH_Open()) MH_HudMargins(top, bottom);
}

/* the menu mask's bits, as core/OpenScreamer.sv has them: 0 grey the HUD lines,
   1 hide Screamer's cheats, 2 hide Screamer 2's */
void mister_menu_mask(int tall, int game)
{
    static int was = -1;
    const int mask = (tall ? 0 : 1) | (game == 2 ? 2 : 4);
    if (mask == was || !MH_Open()) return;
    MH_SetMenuMask(mask);
    was = mask;
}

/* "O[29],Frozen Countdown", "O[30],Mirrored", "O[31],Upside Down", "O[33:32],Props,Normal,Set 2,Set 3",
   "O[34],Afterburner Difficulty", "T[56]".."T[58]" */
int mister_cheats(void)
{
    if (!MH_Open()) return 0;
    const uint64_t st = MH_OSDStatus();
    const int props = (int)MH_OSD_GAME_BITS(st, 32, 2);
    return (props > 2 ? 2 : props) | (MH_OSD_GAME_BITS(st, 29, 1) ? 4 : 0)
         | (MH_OSD_GAME_BITS(st, 30, 1) ? 0x10 : 0) | (MH_OSD_GAME_BITS(st, 31, 1) ? 0x20 : 0);
}

int mister_cheat_afterburner(void)
{
    return MH_Open() && MH_OSD_GAME_BITS(MH_OSDStatus(), 34, 1);
}

int mister_triggers(void)
{
    return MH_Open() ? MH_OSDTriggers() : 0;
}

unsigned mister_ticks(void)
{
    static uint32_t field0;
    static unsigned base;
    static int      counting;
    if (!MH_IsOpen()) return SDL_GetTicks();
    if (!counting) { field0 = MH_FieldCounter(); base = SDL_GetTicks(); counting = 1; }
    /* a field is 400 x 262 pixels at 6.25 MHz: 16.768 ms */
    return base + (unsigned)((uint64_t)(MH_FieldCounter() - field0) * 16768 / 1000);
}

static int g_fm_at;

int mister_fm_start(void)
{
    if (!MH_Open() || !MH_FMAvailable()) return 0;
    MH_FMEnable(0);
    MH_FMEnable(1);
    return 1;
}

void mister_fm_write(int reg, int value)
{
    MH_FMWrite(reg, value, g_fm_at);
}

void mister_fm_at(int frame)
{
    g_fm_at = frame;
}

void mister_fm_gain(float gain)
{
    MH_FMVolume((int)(gain * 32.0f + 0.5f));
}

/*
 * THE TWO GAMES (package/games.txt), each in a folder of its own in the
 * game's (games/dnk/OpenScreamer/): screamer/ and screamer2/.  The first
 * release had Screamer's folders in the game's folder itself, which is still
 * taken when screamer/ has not got them.  The files are the ones the
 * program's own check asks for (game_check in port.c).
 */
#define S1_FILES(at) \
    at "DATA/GEN.DAT", at "DATA/LOCAL.DAT", at "DATA/MUSIC.DAT", at "DATA/SOUND.DAT", \
    at "DATA/MAP1.DAT", at "DATA/MAP2.DAT", at "DATA/MAP3.DAT", at "DATA/MAP4.DAT", at "DATA/MAP5.DAT", at "DATA/MAP6.DAT", \
    at "DATA/MAP1I.DAT", at "DATA/MAP2I.DAT", at "DATA/MAP3I.DAT", at "DATA/MAP4I.DAT", at "DATA/MAP5I.DAT", at "DATA/MAP6I.DAT", \
    at "GFX/GAME.DAT", at "GFX/GRAF.DAT", at "GFX/MENU.DAT", at "GFX/MAP1L.DAT", at "GFX/MAP2L.DAT", at "GFX/MAP3L.DAT", \
    at "GFX/MAP4L.DAT", at "GFX/MAP5L.DAT", at "GFX/MAP6L.DAT", NULL
static const char *const s1_files[] = { S1_FILES("screamer/") };
static const char *const s1_old_files[] = { S1_FILES("") };
static const char *const s2_files[] = {
    "screamer2/DATA/GEN.DAT", "screamer2/DATA/GAME.DAT", "screamer2/DATA/LOCAL.DAT", "screamer2/DATA/SOUND.DAT",
    "screamer2/DATA/MAP1.DAT", "screamer2/DATA/MAP2.DAT", "screamer2/DATA/MAP3.DAT", "screamer2/DATA/MAP4.DAT",
    "screamer2/DATA/MAP5.DAT", "screamer2/DATA/MAP6.DAT", "screamer2/DATA/MAP7.DAT",
    "screamer2/GFX/GAME1.DAT", "screamer2/GFX/GRAF1.DAT", "screamer2/GFX/MAP1L01.DAT", "screamer2/GFX/MAP2L01.DAT",
    "screamer2/GFX/MAP3L01.DAT", "screamer2/GFX/MAP4L01.DAT", "screamer2/GFX/MAP5L01.DAT", "screamer2/GFX/MAP6L01.DAT",
    "screamer2/GFX/MAP7L01.DAT", "screamer2/GFX/MAP1L11.DAT", "screamer2/GFX/MAP2L11.DAT", "screamer2/GFX/MAP3L11.DAT",
    "screamer2/GFX/MAP4L11.DAT", "screamer2/GFX/MAP6L11.DAT", "screamer2/GFX/CARS101.DAT", "screamer2/GFX/CARS201.DAT",
    "screamer2/GFX/CARS301.DAT", "screamer2/GFX/CARS401.DAT", NULL };
#define NO_GAME "No game data found. In %s/ on the SD card, put the DATA, GFX and CONFIG " \
    "folders of Screamer in screamer/, and those of Screamer 2 (with ANIMS and music) in screamer2/."

static const char *g_listed = "OS_MISTER_LISTED";     /* (the environment outlives the port's parts) */

int mister_pick_game(char *dir, int size)
{
    mh_game games[2] = {
        { "S1", "Screamer", s1_files, NULL,
          "Screamer was not found. Copy the DATA, GFX and CONFIG folders of Screamer (the 1995 game) "
          "to %s/screamer/ on the SD card." },
        { "S2", "Screamer 2", s2_files, NULL,
          "Screamer 2 was not found. Copy the DATA, GFX, ANIMS and CONFIG folders of Screamer 2, "
          "and its music folder, to %s/screamer2/ on the SD card." },
    };
    int old = !MH_GameFound(&games[0], NULL) && (games[0].files = s1_old_files, MH_GameFound(&games[0], NULL));
    if (!old) games[0].files = s1_files;
    int listed = 0;
    const int pick = MH_PickGame(games, 2, NULL, NO_GAME, &listed);
    if (pick < 0) return 0;
    if (listed) setenv(g_listed, "1", 1);
    snprintf(dir, (size_t)size, "%s", pick == 1 ? "screamer2" : old ? "." : "screamer");
    printf("mister: %s (%s), in %s\n", games[pick].name, listed ? "picked from the core's list" : "its menu entry, or the only one there", dir);
    fflush(stdout);
    {   /* on the core's picture until the game's first frame, as every hybrid port has it */
        char text[MH_MSG_COLS + 1];
        snprintf(text, sizeof text, "Loading %s...", games[pick].name);
        MH_UI_Message("OpenScreamer", text, 0);
    }
    return pick + 1;
}

const char *mister_game_path(const char *folder)
{
    static char path[600];
    if (!folder || !strcmp(folder, ".") || !folder[0]) snprintf(path, sizeof path, "%s", MH_GamePath());
    else snprintf(path, sizeof path, "%s/%s", MH_GamePath(), folder[0] == '.' && folder[1] == '/' ? folder + 2 : folder);
    return path;
}

void mister_fatal(const char *text)
{
    if (MH_Open()) MH_UI_Message("OpenScreamer", text, MH_UI_WAIT | MH_UI_ERROR);
}

int mister_exit_code(void)
{
    if (MH_CoreReloaded()) return EXIT_RELOADED;
    /* a game picked from the core's list: the list again (not when another core was loaded) */
    if (getenv(g_listed) && MH_Open()) return EXIT_RESTART;
    return EXIT_TO_MENU;
}

void mister_pad_mapping(void)
{
    static int done;
    if (done || !MH_IsOpen()) return;
    done = 1;
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        char guid[40], map[400];
        SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(i), guid, sizeof guid);
        SDL_snprintf(map, sizeof map, "%s,%s,a:b0,b:b1,rightshoulder:b2,leftshoulder:b3,y:b4,x:b5,back:b6,start:b7,"
                     "leftstick:b28,rightstick:b29,dpup:h0.1,dpright:h0.2,dpdown:h0.4,dpleft:h0.8,leftx:a0,lefty:a1,"
                     /* the core's own Menu OK / Menu Back buttons ("J1": a spare button given them in
                        Define buttons), as two buttons SDL has no other use for here */
                     "misc1:b8,paddle1:b9,"
                     /* the analog triggers, which the hybrid library reads from Linux (MH_ReadTriggers) */
                     "lefttrigger:+a4,righttrigger:+a5,",
                     guid, SDL_JoystickNameForIndex(i) ? SDL_JoystickNameForIndex(i) : "MiSTer Joystick");
        SDL_GameControllerAddMapping(map);
    }
}

static Uint64 g_work_from, g_work_sum, g_mark_at, g_stage[6];

/* the time since the last mark belongs to `stage` */
void mister_mark(int stage)
{
    Uint64 now = SDL_GetPerformanceCounter();
    if (g_mark_at) g_stage[stage] += now - g_mark_at;
    g_mark_at = now;
}
long mister_pixels, mister_rows, mister_tris;
long mister_helper_cnt[3];
static Uint64 g_wait_sum, g_wait_max;
long mister_stat[4];
static float g_last_work;

float mister_last_work(void)
{
    return MH_IsOpen() ? g_last_work : 0.0f;
}

void mister_frame_done(void)
{
    static uint32_t field0;
    static int frames, on = -1;
    prof_follow();
    if (on < 0) on = SDL_getenv("OS_FPSLOG") != NULL;
    if (!on || !MH_IsOpen()) return;
    uint32_t field = MH_FieldCounter();
    if (!frames) { field0 = field; g_work_sum = 0; }
    frames++;
    if (field - field0 >= 600) {
        printf("speed: %.1f pictures a second (%d in %u fields), %.1f ms to make one; %ld triangles, %ld rows, %ld pixels a picture\n",
               (double)(frames - 1) * 59.64 / (double)(field - field0), frames - 1, field - field0,
               (double)g_work_sum * 1000.0 / (double)SDL_GetPerformanceFrequency() / (double)frames,
               mister_tris / frames, mister_rows / frames, mister_pixels / frames);
        printf("       the helper's: %ld triangles, %ld rows, %ld pixels a picture\n",
               mister_helper_cnt[0] / frames, mister_helper_cnt[1] / frames, mister_helper_cnt[2] / frames);
        mister_helper_cnt[0] = mister_helper_cnt[1] = mister_helper_cnt[2] = 0;
        {
            const double f = 1000.0 / (double)SDL_GetPerformanceFrequency() / (double)frames;
            printf("       of that: the game %.1f, sky and clearing %.1f, the scene worked out %.1f and its filling finished %.1f, the rest of the picture %.1f, to the screen %.1f ms\n",
                   (double)g_stage[0] * f, (double)g_stage[1] * f, (double)g_stage[2] * f, (double)g_stage[5] * f, (double)g_stage[3] * f, (double)g_stage[4] * f);
            printf("       and handing it over, with the wait for the field: %.1f ms (the longest %.1f)\n", (double)g_wait_sum * f, (double)g_wait_max * f * (double)frames);
            g_wait_sum = g_wait_max = 0;
            printf("       triangles listed %ld a picture; when the last was listed the helper had filled %ld%% of the lanes (backdrop still unpainted in %ld pictures); idle turns of the main thread %ld\n", mister_stat[0] / frames, mister_stat[0] ? mister_stat[1] * 100 / 8 / mister_stat[0] : 0, mister_stat[2], mister_stat[3] / frames);
            mister_stat[0] = mister_stat[1] = mister_stat[2] = mister_stat[3] = 0;
            { extern Uint64 g_helper_time[2]; printf("       the helper: backdrop and clearing %.1f ms, filling %.1f ms a picture\n", (double)g_helper_time[0] * f, (double)g_helper_time[1] * f); g_helper_time[0] = g_helper_time[1] = 0; }
            for (int k = 0; k < 6; k++) g_stage[k] = 0;
        }
        mister_tris = mister_rows = mister_pixels = 0;
        fflush(stdout);
        frames = 0;
    }
}

/* the palette as the core has it (MH_SetPalette): sent when it is another */
static uint32_t g_core_pal[256];
static int g_core_pal_ok;
static void pal_to_core(const uint32_t *pal)
{
    uint32_t rgb[256];
    for (int i = 0; i < 256; i++) rgb[i] = pal[i] & 0xffffffu;
    if (g_core_pal_ok && !memcmp(rgb, g_core_pal, sizeof rgb)) return;
    MH_SetPalette(rgb);
    memcpy(g_core_pal, rgb, sizeof rgb);
    g_core_pal_ok = 1;
}

/*
 * A picture can be handed over in two ways.  mister_show() does it all here
 * and now, through SDL (which also sets the core's video mode when the size
 * has changed).  mister_show_later() only waits for the field and leaves the
 * conversion and the copy to another thread -- the one that fills the scene,
 * idle at that moment -- which calls mister_show_job(): a millisecond of the
 * main thread a picture.  The picture must then not be drawn on until the job
 * is done (mister_show_wait()); the race does not draw on it before the next
 * scene is filled, by that same thread.
 */
static struct { const void *pixels; int w, h; uint32_t pal[256]; } g_later;
static SDL_atomic_t g_later_due;
static int g_shown_w, g_shown_h;                 /* the size mister_show() last showed (the core is in that mode) */
static uint32_t g_shown_field;

void mister_show_wait(void)
{
    while (SDL_AtomicGet(&g_later_due)) SDL_Delay(0);
}

static void work_done(Uint64 now)
{
    if (g_work_from) { g_work_sum += now - g_work_from; g_last_work = (float)((double)(now - g_work_from) * 1000.0 / (double)SDL_GetPerformanceFrequency()); }
}

static void wait_done(Uint64 since)
{
    g_work_from = SDL_GetPerformanceCounter();
    g_wait_sum += g_work_from - since;
    if (g_work_from - since > g_wait_max) g_wait_max = g_work_from - since;
    g_mark_at = g_work_from;
}

/* "O[28],Lock to 30 FPS,Off,On": a picture every second field, for the
   resolutions (640 wide) at which one field is not time enough -- an even
   30 a second instead of a pace that wanders between 30 and 40.
   OS_LOCK30=0|1 fixes it for a test. */
static int lock30(void)
{
    static int fixed = -2;
    if (fixed == -2) fixed = SDL_getenv("OS_LOCK30") ? SDL_atoi(SDL_getenv("OS_LOCK30")) != 0 : -1;
    if (fixed >= 0) return fixed;
    return MH_IsOpen() ? MH_OSD_GAME_BITS(MH_OSDStatus(), 28, 1) : 0;
}

void mister_show(void *window, const void *pixels, const uint32_t *pal, int w, int h)
{
    mister_show_wait();
    SDL_Surface *s = SDL_GetWindowSurface(window);
    if (!s) return;
    mister_mark(3);
    const uint8_t *const px = pixels;
    if (s->format->format == SDL_PIXELFORMAT_INDEX8 && s->format->palette && s->w >= w && s->h >= h) {
        /* the numbers as they are, and the surface's palette the picture's (SDL's driver sends it
           to the core when it changes; so does pal_to_core, for the pictures handed over later) */
        for (int y = 0; y < h; y++) memcpy((uint8_t *)s->pixels + y * s->pitch, px + y * w, (size_t)w);
        SDL_Palette *sp = s->format->palette;
        int same = sp->ncolors >= 256;
        for (int i = 0; same && i < 256; i++)
            same = sp->colors[i].r == (pal[i] >> 16 & 255) && sp->colors[i].g == (pal[i] >> 8 & 255) && sp->colors[i].b == (pal[i] & 255);
        if (!same) {
            SDL_Color c[256];
            for (int i = 0; i < 256; i++) { c[i].r = pal[i] >> 16 & 255; c[i].g = pal[i] >> 8 & 255; c[i].b = pal[i] & 255; c[i].a = 255; }
            SDL_SetPaletteColors(sp, c, 0, 256);
        }
        g_shown_w = w; g_shown_h = h;
    } else {
        /* (not the 8-bit surface it asks for: each pixel through the palette, in its colours) */
        for (int y = 0; y < h && y < s->h; y++)
            for (int x = 0; x < w && x < s->w; x++) {
                const uint32_t c = pal[px[y * w + x]];
                const Uint32 v = SDL_MapRGB(s->format, c >> 16 & 255, c >> 8 & 255, c & 255);
                uint8_t *const at = (uint8_t *)s->pixels + y * s->pitch + x * s->format->BytesPerPixel;
                if (s->format->BytesPerPixel == 2) *(uint16_t *)at = (uint16_t)v; else if (s->format->BytesPerPixel == 4) *(uint32_t *)at = v; else *at = (uint8_t)v;
            }
        g_shown_w = g_shown_h = 0;
    }
    /* what went before this is the making of the picture; the rest is the wait for the field */
    Uint64 now = SDL_GetPerformanceCounter();
    work_done(now);
    mister_mark(4);
    if (lock30() && MH_IsOpen()) MH_WaitField(g_shown_field + 1);      /* not before the second field after the last picture */
    SDL_UpdateWindowSurface(window);
    if (MH_IsOpen()) pal_to_core(pal);              /* (the core's, if a picture handed over later left another) */
    g_shown_field = MH_FieldCounter();
    wait_done(now);
}

int mister_show_later(void *window, const void *pixels, const uint32_t *pal, int w, int h)
{
    static int never = -1;
    if (never < 0) never = SDL_getenv("OS_SHOWNOW") != NULL;
    if (never || !MH_IsOpen() || w != g_shown_w || h != g_shown_h || w * h > 640 * 400) {
        mister_show(window, pixels, pal, w, h);
        return 0;
    }
    mister_show_wait();
    mister_mark(3);
    Uint64 now = SDL_GetPerformanceCounter();
    work_done(now);
    mister_mark(4);
    MH_WaitFrame(g_shown_field);                 /* one picture a field (two with an interlaced mode) */
    if (lock30()) MH_WaitField(g_shown_field + 1);
    g_shown_field = MH_FieldCounter();
    wait_done(now);
    g_later.pixels = pixels; g_later.w = w; g_later.h = h;
    memcpy(g_later.pal, pal, sizeof g_later.pal);
    SDL_AtomicSet(&g_later_due, 1);
    return 1;
}

void mister_show_job(void)
{
    if (!SDL_AtomicGet(&g_later_due)) return;
    pal_to_core(g_later.pal);
    MH_Present(g_later.pixels, g_later.w);           /* (the numbers as they are: the core has the palette) */
    SDL_AtomicSet(&g_later_due, 0);
}

void mister_second_cpu(void)
{
    sigset_t prof;                                 /* (OS_PROF: SDL's threads start with every signal blocked) */
    sigemptyset(&prof); sigaddset(&prof, SIGPROF);
    pthread_sigmask(SIG_UNBLOCK, &prof, NULL);
    MH_PinThreadToCPU1();
}
