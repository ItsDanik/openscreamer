/*
 * main.c - OpenScreamer, track viewer.
 *
 * Draws a track exactly as the game's own data describes it: every piece in
 * MAPn.MMP, with the mesh MAPn.UNR gives it, turned by the record's own
 * heading, textured with the per-vertex texture coordinates and the material
 * rectangle from MAPn.MLB.  Nothing on screen is synthesised or tuned by eye
 * any more -- the road, the ground, the kerbs, the lane lines, the walls and
 * the bridges are all the game's pieces.
 *
 * (An earlier version built the road from the .SPL splines and guessed the
 * texture mapping; it is kept in port/attic/ for reference.  The splines are
 * now used for one thing only: the path the camera drives along.)
 *
 * Software rasteriser on a 320x200 framebuffer -- the original's low-res mode,
 * and what a MiSTer hybrid core wants (ARM makes a framebuffer, FPGA scans it
 * out).  The window is 4:3 and the projection carries an aspect correction, so
 * geometry is undistorted on a modern display; see ASPECT below.
 */

#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>

#include "scene.h"
#include "drive.h"
#include "sound.h"
#include "port.h"
#include "s2_port.h"
#include "controls.h"
#include <setjmp.h>

/* every event loop gets the game controller's buttons as keys (controls.c), and F5 -- the
   frames a second, on every screen -- is taken here (poll_event) */
static int poll_event(SDL_Event *e);
#ifdef MISTER_HYBRID
static void tall_follow(void);          /* MISTER: once a picture, the core's Resolution option */
#define SDL_PollEvent(e) (tall_follow(), poll_event(e))
#else
#define SDL_PollEvent(e) poll_event(e)
#endif


/*
 * THE PICTURE'S SIZE.  Low: 320 x 200, the game's own.  High: 640 x 400 --
 * the size the game's second set of files is made for (every "H" picture is
 * 640 x 400, every "H" font and sprite exactly twice its "L" twin; the "H"
 * tracks have more scenery, the same textures).  Both are shown 4:3.
 * Everything flat keeps its 320 x 200 positions and is multiplied by g_s.
 */
#define FB_MAXW 640
#define FB_MAXH 400
static int FB_W = 320, FB_H = 200;
static int g_s = 1;                /* 1 low, 2 high */
/*
 * A WIDE PICTURE (the MiSTer core's 640x200 and 640x240; OS_WIDE=1 tries it
 * anywhere): twice the columns, the same rows.  The artwork is the game's
 * high set (g_s = 2, as for 640x400) and everything flat is still worked
 * out in that set's own pixels; g_yd = 2 says that the screen has only every
 * second ROW of it, and the few routines that put a pixel down (hud_put,
 * dash_line, pkc_draw, pic_put, the darkenings) leave the odd rows out.  No
 * picture is stretched or squeezed by anything but that.  The 3D view has
 * the lens of the high set across and of the low set down.
 */
static int g_yd = 1;
#define ART_W (320 * g_s)
#define ART_H (200 * g_s)
#define SY    (g_s / g_yd)         /* screen rows to one row of the 320 x 200 layout */
/*
 * A TALLER PICTURE (the MiSTer core's 320x240; OS_TALL=1 tries it anywhere).
 * The game's artwork is all made for 200 rows and none of it is stretched:
 * g_tall is the extra rows (0 or 40) and g_oy how far down the flat thing
 * being drawn is put -- the middle for whole-screen pictures and menus (the
 * default), LAY_TOP / LAY_BOT for what belongs at an edge of the race's
 * screen: 8 rows in from it, a television's edge being what it is (Danik).
 * The 3D view has all the rows.  The picture's memory is always 40 rows more
 * than it needs, so that the height can change while the program runs
 * (tall_follow).
 */
static int g_tall, g_oy;
#define TALL_ROWS 40
#define LAY_TOP() (g_oy = g_tall ? 8 : 0)
#define LAY_MID() (g_oy = g_tall / 2)
#define LAY_BOT() (g_oy = g_tall ? g_tall - 8 : 0)
static int g_clip[4] = { 0, 320, 0, 200 };     /* where flat things may be drawn: x0, x1, y0, y1, in pixels */

/*
 * 320x200 is 1.6:1 but was always displayed at 4:3, so its pixels are 1.2x
 * taller than wide.  We render at 4:3 (window 960x720) and multiply the
 * horizontal focal by ASPECT, so a circle in world space looks round on screen.
 * The backdrop sampling uses the same corrected focal.
 */
#define ASPECT 1.2f

#define MAX_SAMPLES 8192
#define NEAR_Z      24.0f     /* near clip plane, world units */

#define COL_SKY_FALLBACK 0xFF161320u
#define COL_GROUND       0xFF23282Eu
#define COL_FOG          0xFF2A2434u

typedef struct { int w, h; uint8_t *pix; } sky;   /* RGB, w*h*3 */
typedef struct { float x, y, z, yaw, pitch, roll; } camera;   /* roll: + = right side up */

/*
 * THE GAME'S LENS.  A point is projected as
 *     screen = centre + coordinate * 128 / depth          (0xd7493: `shl eax,7`
 *                                                          then a divide)
 * in 320x200 pixels, the same 128 for both axes -- about 103 degrees across.
 * And the centre is not the middle of the screen: the view is set up with
 * 0x4c610(320, height, height * 0.65), so the eye line sits 65% of the way
 * down (row 130 of 200), leaving more picture above the road than below.
 * The port once used a 50-degree lens centred on the screen; everything
 * looked closer than in the original (Danik), and that was why.
 */
#define GAME_FOCAL 128.0f
static float g_cy = 130.0f;   /* the projection's centre row */
/* how far down the picture the eye line is: 0.65 (0x4f6c4's third argument in both games);
   OS_EYE to try another -- Screamer 2's picture measured about 0.675 against the original's */
#define EYE_LINE (getenv("OS_EYE") ? (float)atof(getenv("OS_EYE")) : 0.65f)
static int   g_vh = 200;      /* rows of the screen the 3D view covers */

static uint32_t *fb;          /* colour                                        */
static float    *zb;          /* 1/depth per pixel; bigger is nearer           */
static int16_t  *idb;         /* which scene object owns the pixel, -1 = none  */
static int g_tie_rule = 1;   /* OS_NOTIE=1 switches the same-depth rule off (for comparing) */
#define MAX_OBJS 256                 /* the pool of cars, props, puffs, sparks ... (g_car) */
static unsigned char g_obj_ground[4096 + MAX_OBJS + 8];   /* this picture: the object is a piece of the drivable track (record run 0) */
static int g_ground_now, g_obj_sprite;               /* the object being drawn: is such a piece / faces the screen (smoke) */
static unsigned char g_obj_carpet[4096 + MAX_OBJS + 8];   /* this picture: the object is a ground carpet (mesh.carpet) */
static int   g_tiemark;                           /* OS_TIEMARK, a test (render_frame) */
static float g_obj_d2[4096 + MAX_OBJS + 8];   /* this picture: camera to each object's own position, squared (by object id) */
static int16_t  *pxi;         /* palette index of the pixel, -1 = not known (sky) */
static uint8_t   g_smg[112 * 256];  /* MAPn.SMG, see raster_tri; Screamer 2 makes its own, of 112 rows (s2_blend_make) */
static int       g_smg_rows = 64;
static int       g_dbg_quad;        /* OS_S2PUFFQUAD: a test, the dust's square shown whole */
static int       g_have_smg;
static const scene *g_scene;
static uint32_t g_pal[256];   /* MAPn.COL, scaled to 8 bits */
/*
 * SCREAMER 2: A TRACK'S CONDITION (0x130c50, the race record's third word) and what the
 * track loader (0x2ef4e) and the face routines make of it:
 *   0  textures and backdrop of look 0 (MAPnL0c), DEPTH1.DPT, no fade, the picture
 *      cleared to TRACK_DATA's "background 256" colour
 *   1  look 1 (MAPnL1c: the digit is condition & 1, 0x2e69a), DEPTH1.DPT, faces faded by
 *      distance (not on the fourth track), cleared to colour 0
 *   2  look 0, DEPTH2.DPT, faded, NO backdrop, cleared to colour 0x13
 * and the seventh track: no fade whatever the condition.
 * THE FADE (0xbd5e9, and the same in four more face routines) is per FACE, not per pixel:
 *     z = ((z0 + z1) / 2 + z2) / 2            the depths of its first three corners
 *     d = |z - near|, held below  range = far - near
 *     row = (d * 16384 / range & 0x3f80) >> 6          128 steps, the table's EVEN rows
 *     pixel = DEPTH[row * 256 + texel]
 * near / far (0x12d31, set when the view changes): condition 1 -- 0 / 27000 in view 1,
 * else 6000 / 35000; condition 2 -- 0 / 25000.
 */
static int      g_s2_cond;
static int      g_fade_on;
static float    g_fade_near, g_fade_far;
static uint8_t  g_dpt[65536];
static const uint8_t *g_fade_row;       /* the face being drawn: its row of the table, or NULL */
static int      g_ground_colour = -1;   /* TRACK_DATA `color`: the palette index the game
                                           fills the screen below the backdrop with (0x1337d) */

/*
 * HOW FAR THE GAME DRAWS -- its DETAIL option (OPTIONS' first line, 0x1d2b4).
 *
 * Two limits, both the engine's own:
 *   - a piece is kept only within 52000 units of the camera (FUN_00083164:
 *     dist^2 >> 2 < 676000000);
 *   - every polygon is then CUT OFF at a depth in front of the camera, the
 *     far plane.  0x8baa4 writes that depth into seven compare instructions
 *     of the face routine (0x839af...: a face wholly beyond it is dropped,
 *     one that straddles it goes to the clipper), and it comes from the table
 *     at 0x21d4, one number per detail level:
 *         LOW 27808      MID 27808      HI 43808
 *     LOW differs from MID in one more thing: the moving scenery (buses,
 *     trams, the jet) is not loaded at all (0x15374: `0 < detail`).
 *
 * ULTRA is the port's own fourth level: no far plane, and pieces kept out to
 * a fixed 140000 units.
 */
#define DRAWDIST_ORIGINAL 52000.0f
#define DRAWDIST_FAR      140000.0f
static float g_drawdist = DRAWDIST_FAR;
static float g_far_z;                 /* the far plane's depth; 0 = none */
static int   g_detail = 2;            /* 0 LOW, 1 MID, 2 HI (the game's default), 3 ULTRA */
static float g_fog      = 0.0f;   /* OS_FOG=<units>: fade to haze, off by default */
static int   g_cull     = 1;      /* back-face culling, as the engine does */
static int   g_layer    = 3;      /* 0 / 1 = only that record run, 3 = both, 2 = none */

/* ------------------------------------------------------------------ sky */

/* The backdrop is MAPn.BCK in the track's own container: a PCX picture, 512 x
   256, with a palette of its own at its end.  (Until 2026-10-12 the port read
   a copy made by a tool, build/sky/MAPn.sky, that nobody but this machine
   had: no sky anywhere else.) */
static int sky_load(sky *s, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc((size_t)(n > 0 ? n : 1));
    if (n < 128 + 769 || fread(d, 1, (size_t)n, f) != (size_t)n || d[0] != 0x0a) { free(d); fclose(f); return -1; }
    fclose(f);
    const int w = (d[8] | d[9] << 8) - (d[4] | d[5] << 8) + 1, h = (d[10] | d[11] << 8) - (d[6] | d[7] << 8) + 1;
    const int bpl = d[66] | d[67] << 8;
    if (w < 1 || h < 1 || w > 4096 || h > 4096) { free(d); return -1; }
    const uint8_t *pal = d + n - 768;
    s->w = w;
    s->h = h;
    s->pix = malloc((size_t)w * h * 3);
    memset(s->pix, 0, (size_t)w * h * 3);
    long p = 128;
    for (int y = 0; y < h; y++) {
        int x = 0;
        while (x < bpl && p < n - 769) {
            int c = d[p++], run = 1;
            if ((c & 0xc0) == 0xc0) { run = c & 0x3f; c = d[p++]; }
            for (; run > 0 && x < bpl; run--, x++)
                if (x < w) memcpy(s->pix + ((size_t)y * w + x) * 3, pal + 3 * c, 3);
        }
    }
    free(d);
    return 0;
}

/*
 * The backdrop, as the game draws it (0x83a68; 0x83b44 is the same thing
 * drawn leaning).  MAPn.BCK is a 512 x 256 picture and it is simply COPIED to
 * the screen, not wrapped round a cylinder:
 *
 *   across   the picture's column under the screen's left edge is
 *            ((2047 - heading) / 2) mod 512, heading in the engine's 2048
 *            units a turn -- ONE PIXEL PER TWO UNITS, so the picture goes
 *            round exactly TWICE in a full turn and never jumps;
 *   down     its top row is at  eye line + 22 - 256 + 128 * sin(pitch):
 *            the picture's bottom edge is 22 rows below the eye line;
 *   below it the screen is filled with one colour: palette index `color`
 *            of TRACK_DATA in MAPnA.DTR (0x1337d puts it in the view at +0x10).
 *
 * The game's heading is the port's yaw + a quarter turn (forward is
 * (sin a, -cos a) there, (cos yaw, sin yaw) here).
 *
 * The port's first backdrop (2026-10-07) wrapped the picture round a cylinder
 * at the lens's own scale -- 1.9 times round a turn, so it jumped wherever the
 * heading wrapped -- and drew it mirrored.
 */

/*
 * THE PORT'S SETTINGS FILE, openscreamer.cfg -- what the game keeps in
 * CHOICE.DAT beside the records: the options, here with the resolution in the
 * place of the game's unused detail line.
 *
 * HIGH THAT WILL NOT START FALLS BACK TO LOW.  Three ways: the hi-res files
 * are not there (checked before anything is loaded); something fails while
 * starting (res_fail); or the program died starting in high last time -- a
 * mark is written to the file before a start in high and taken out again when
 * the first picture is up or the program ends in an orderly way, so a mark
 * still there means it never got that far.  In each case the setting is put
 * back to low and the program started again.
 */
/*
 * THE WINDOW is made once and outlives the parts of the program (os_slot 0 /
 * 1 / 2: window, renderer, the picture's texture -- the last one made again
 * when the picture's size changes).
 */
static jmp_buf *g_next_part;           /* where os_go() jumps back to: the loop in main() */

#ifdef MISTER_HYBRID
/* MISTER: "update the texture, clear, copy, present" is one call that turns
   the picture into the screen's 16 bits and shows it at the next field */
#define SDL_UpdateTexture(t, r, p, pitch) mister_show(os_slot(0), (p), FB_W, FB_H)
#define SDL_RenderClear(r)                ((void)0)
#define SDL_RenderCopy(r, t, a, b)        ((void)0)
#define SDL_RenderPresent(r)              ((void)0)
#endif

/* MISTER: where a picture's time goes, for the log (mister.c).  0 the game's
   own work is done, 1 sky and clearing, 2 the scene worked out, 5 and filled;
   then the rest (3). */
#ifdef MISTER_HYBRID
#define MISTER_MARK(k) mister_mark(k)
#else
#define MISTER_MARK(k) ((void)0)
#endif

static SDL_Window *sys_window(SDL_Renderer **ren)
{
    SDL_Window *win = os_slot(0);
    if (!win) {
#ifdef MISTER_HYBRID
        SDL_SetHint("SDL_MISTER_VSYNC", "1");            /* MISTER: a picture is shown once a field */
#endif
        if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return NULL; }
        /* 4:3, as 320x200 was always meant to be shown; resizable, letterboxed */
#ifdef MISTER_HYBRID
        /* MISTER: the window is the core's screen, as big as the picture (its
           size picks the video mode) */
        win = SDL_CreateWindow("OpenScreamer", 0, 0, FB_W, FB_H, 0);
#else
        win = SDL_CreateWindow("OpenScreamer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               960, 720, SDL_WINDOW_RESIZABLE);
#endif
#ifdef MISTER_HYBRID
        /* MISTER: no renderer -- the picture goes straight to the window's
           surface (mister_show(), in the place of SDL_UpdateTexture below) */
        SDL_Renderer *r = (SDL_Renderer *)win;
        if (!win) fprintf(stderr, "no picture: %s\n", SDL_GetError());
#else
        SDL_Renderer *r = win ? SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC) : NULL;
        if (!r) fprintf(stderr, "no %s: %s\n", win ? "renderer" : "window", SDL_GetError());
        SDL_RendererInfo info;
        if (r && SDL_GetRendererInfo(r, &info) == 0)
            printf("display: %s, vsync %s\n", info.name,
                   (info.flags & SDL_RENDERER_PRESENTVSYNC) ? "on" : "NOT given by the driver");
        if (r) SDL_RenderSetLogicalSize(r, 960, 720);
#endif
        os_slot_set(0, win); os_slot_set(1, r);
    }
    if (ren) *ren = os_slot(1);
    return win;
}

static SDL_Texture *sys_texture(int w, int h)
{
#ifdef MISTER_HYBRID
    if (os_slot(0)) {                                   /* MISTER: the screen follows the picture's size */
        int ww = 0, wh = 0;
        SDL_GetWindowSize(os_slot(0), &ww, &wh);
        if (ww != w || wh != h) SDL_SetWindowSize(os_slot(0), w, h);
    }
    return os_slot(0);                                  /* (there is no texture) */
#endif
    SDL_Texture *tex = os_slot(2);
    int tw = 0, th = 0;
    if (tex) SDL_QueryTexture(tex, NULL, NULL, &tw, &th);
    if (tex && (tw != w || th != h)) { SDL_DestroyTexture(tex); tex = NULL; }
    if (!tex && os_slot(1))
        tex = SDL_CreateTexture(os_slot(1), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
    os_slot_set(2, tex);
    return tex;
}

/* run another part of the program next (a1 / a2: its arguments), from a clean
   slate; does not come back */
static void os_go(int argc, const char *a1, const char *a2)
{
    os_next(argc, a1, a2);
    longjmp(*g_next_part, 1);
}

static struct { int res, diff, laps, speech, music, starting, vol_cd, vol_fx, vol_mu, detail; } g_cfg = { 0, 1, 0, 1, 1, 0, 0x6000, 0x6000, 0x6000, 2 };
/* key_accelerate= ... lines: a key's name in the place of the one in the game's CONTROLS.CFG (controls.c) */
static char g_cfg_key[6][40];
static const char *const g_cfg_key_word[6] = { "key_accelerate", "key_brake", "key_left", "key_right", "key_gear_up", "key_gear_down" };
static const char *const g_detail_word[4] = { "low", "mid", "hi", "ultra" };
static char **g_argv;
static int    g_argc;

static void cfg_load(void)
{
    FILE *f = fopen("openscreamer.cfg", "r");
    if (!f) return;
    char line[128], val[64];
    int n;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "resolution=%63s", val) == 1) g_cfg.res = !strcmp(val, "high");
        if (sscanf(line, "difficulty=%d", &n) == 1) g_cfg.diff = n < 0 ? 0 : n > 3 ? 3 : n;
        if (sscanf(line, "laps=%d", &n) == 1) g_cfg.laps = n & 3;
        if (sscanf(line, "speech=%d", &n) == 1) g_cfg.speech = n != 0;
        if (sscanf(line, "music=%d", &n) == 1) g_cfg.music = n != 0;
        if (sscanf(line, "starting_high=%d", &n) == 1) g_cfg.starting = n != 0;
        if (sscanf(line, "detail=%63s", val) == 1)
            for (int k = 0; k < 4; k++) if (!strcmp(val, g_detail_word[k])) g_cfg.detail = k;
        if (sscanf(line, "volume_cd=%d", &n) == 1) g_cfg.vol_cd = n < 0 ? 0 : n > 0x7fff ? 0x7fff : n;
        if (sscanf(line, "volume_fx=%d", &n) == 1) g_cfg.vol_fx = n < 0 ? 0 : n > 0x7fff ? 0x7fff : n;
        if (sscanf(line, "volume_mu=%d", &n) == 1) g_cfg.vol_mu = n < 0 ? 0 : n > 0x7fff ? 0x7fff : n;
        for (int k = 0; k < 6; k++) {
            size_t w = strlen(g_cfg_key_word[k]);
            if (strncmp(line, g_cfg_key_word[k], w) || line[w] != '=') continue;
            snprintf(g_cfg_key[k], sizeof g_cfg_key[k], "%s", line + w + 1);
            g_cfg_key[k][strcspn(g_cfg_key[k], "\r\n")] = 0;
        }
    }
    fclose(f);
}

static void cfg_save(void)
{
    FILE *f = fopen("openscreamer.cfg", "w");
    if (!f) return;
    fprintf(f, "detail=%s\nresolution=%s\ndifficulty=%d\nlaps=%d\nspeech=%d\nmusic=%d\n", g_detail_word[g_cfg.detail & 3],
            g_cfg.res ? "high" : "low", g_cfg.diff, g_cfg.laps, g_cfg.speech, g_cfg.music);
    fprintf(f, "volume_cd=%d\nvolume_fx=%d\nvolume_mu=%d\n", g_cfg.vol_cd, g_cfg.vol_fx, g_cfg.vol_mu);
    for (int k = 0; k < 6; k++) if (g_cfg_key[k][0]) fprintf(f, "%s=%s\n", g_cfg_key_word[k], g_cfg_key[k]);
    if (g_cfg.starting) fprintf(f, "starting_high=1\n");
    fclose(f);
}

/* the name of a file of the low set, or of its twin in the high set: the L
   before the extension (MENU1L.PCX, GAMEL.FNT, IMPLET_L.PKC) becomes an H */
static const char *res_name(const char *low)
{
    static char buf[4][600];
    static int at;
    char *out = buf[at++ & 3];
    snprintf(out, 600, "%s", low);
    if (g_s == 2) {
        char *dot = strrchr(out, '.');
        if (dot && dot > out && dot[-1] == 'L') dot[-1] = 'H';
    }
    return out;
}

static void res_started(void)
{
    if (!g_cfg.starting) return;
    g_cfg.starting = 0;
    cfg_save();
}

/* high cannot be started: back to low, and the program again from the top */
static void res_fail(const char *why)
{
    fprintf(stderr, "high resolution: %s -- going back to low\n", why);
    g_cfg.res = 0; g_cfg.starting = 0;
    cfg_save();
    setenv("OS_RES", "low", 1);
    if (g_argv && g_next_part) {             /* the same part again, in low (its arguments copied first: they may be the
                                                very buffers os_next writes the next part's into) */
        char a1[768] = "", a2[768] = "";
        if (g_argc > 1 && g_argv[1]) snprintf(a1, sizeof a1, "%s", g_argv[1]);
        if (g_argc > 2 && g_argv[2]) snprintf(a2, sizeof a2, "%s", g_argv[2]);
        os_go(g_argc, g_argc > 1 ? a1 : NULL, g_argc > 2 ? a2 : NULL);
    }
    exit(1);
}

static void res_init(void)
{
    cfg_load();
    {
        const char *key[6];
        for (int k = 0; k < 6; k++) key[k] = g_cfg_key[k];
        controls_init(key);
    }
#ifdef MISTER_HYBRID
    /* MISTER: the resolution is the core's OSD option, not the settings file's */
    int wide = 0;
    if (mister_on()) { g_tall = (mister_res_mode() & 1) ? TALL_ROWS : 0; wide = mister_res_mode() >> 1; }
#else
    int wide = 0;
#endif
    if (getenv("OS_TALL")) g_tall = atoi(getenv("OS_TALL")) ? TALL_ROWS : 0;
    if (getenv("OS_WIDE")) wide = atoi(getenv("OS_WIDE")) != 0;
    int hi = getenv("OS_RES") ? !strcmp(getenv("OS_RES"), "high") : g_cfg.res;
#ifdef MISTER_HYBRID
    hi = 0;                                  /* MISTER: there is no high resolution -- the machine cannot fill it */
#endif
    if (hi) { g_tall = 0; wide = 0; }
    if (wide) {
        /* (the high set's flat artwork must be there) */
        static const char *need[] = {
            "build/dec/GAME/GAMEH.FNT", "build/dec/GAME/GAMEH.SPR", "build/dec/GAME/IMPLET_H.PKC",
            "build/dec/GRAF/MENUH.FNT", "build/dec/GRAF/MENUH.SPR", "build/dec/GRAF/MENU1H.PCX",
        };
        for (size_t i = 0; i < sizeof need / sizeof need[0]; i++)
            if (!os_exists(need[i])) { fprintf(stderr, "a wide picture needs %s: not there\n", need[i]); wide = 0; break; }
    }
    if (wide) { g_s = 2; g_yd = 2; FB_W = 640; FB_H = 200; }
    if (hi && g_cfg.starting && !getenv("OS_RES"))
        res_fail("the last start in it did not get as far as a picture");
    if (hi) {
        static const char *need[] = {
            "build/dec/GAME/GAMEH.FNT", "build/dec/GAME/GAMEH.SPR", "build/dec/GAME/IMPLET_H.PKC",
            "build/dec/GRAF/MENUH.FNT", "build/dec/GRAF/MENUH.SPR", "build/dec/GRAF/MENU1H.PCX",
            "build/dec/MAP1H/MAP1.UNR",
        };
        static const char *need2[] = { "build/dec/MAP1H01/MAP1.UNR" };       /* Screamer 2, as far as it goes */
        const char **list = os_game() == 2 ? need2 : need;
        size_t nlist = os_game() == 2 ? sizeof need2 / sizeof need2[0] : sizeof need / sizeof need[0];
        for (size_t i = 0; i < nlist; i++)
            if (!os_exists(list[i]) || getenv("OS_RES_TESTFAIL")) {      /* the second: a test of the fall-back */
                if (getenv("OS_RES")) unsetenv("OS_RES");
                res_fail("its files are not there");
            }
        g_s = 2; FB_W = 640; FB_H = 400;
        if (!getenv("OS_RES")) { g_cfg.starting = 1; cfg_save(); if (os_once(1)) atexit(res_started); }
    }
    FB_H += g_tall;
    LAY_MID();
    g_vh = FB_H; g_cy = EYE_LINE * (float)FB_H;
    g_clip[0] = 0; g_clip[1] = FB_W; g_clip[2] = 0; g_clip[3] = FB_H;
}

static float g_focal_x, g_focal_y;
#ifdef MISTER_HYBRID
static int g_in_race;                                /* a race (or the intro) is set up and running */
static void race_art_load(void);
/* MISTER: the core's Resolution option is followed at once, wherever the
   program is (called once a picture, before anything is drawn) */
static void tall_follow(void)
{
    static int quiet = -1;
    if (quiet < 0) quiet = getenv("OS_TALL") != NULL || !mister_on();
    if (quiet || !fb || (g_s == 2 && g_yd == 1)) return;
    const int tall = (mister_res_mode() & 1) ? TALL_ROWS : 0;
    const int wide = mister_res_mode() >> 1;
    if (tall == g_tall && wide == (g_yd == 2)) return;
    mister_show_wait();                              /* (the last picture may still be on its way out) */
    if (wide != (g_yd == 2)) {
        /* the width is another set of artwork: the menu begins again in it,
           a race loads its flat pictures again and goes on */
        if (!g_in_race) {
            if (g_argv && g_next_part)
                os_go(g_argc, g_argc > 1 ? g_argv[1] : NULL, g_argc > 2 ? g_argv[2] : NULL);
            return;
        }
        if (wide && !os_exists("build/dec/GAME/GAMEH.FNT")) return;
        g_s = wide ? 2 : 1; g_yd = wide ? 2 : 1;
        FB_W = 320 * g_s; FB_H = 200 + g_tall;
        g_clip[1] = FB_W;
        g_focal_x = GAME_FOCAL * (float)g_s; g_focal_y = GAME_FOCAL * (float)SY;
        race_art_load();
    }
    FB_H += tall - g_tall;
    g_tall = tall;
    LAY_MID();
    g_vh = FB_H; g_cy = EYE_LINE * (float)FB_H;
    g_clip[3] = FB_H;
    for (int i = 0; i < FB_W * FB_H; i++) { fb[i] = 0xff000000u; zb[i] = 0.0f; idb[i] = -1; pxi[i] = 0; }
    sys_texture(FB_W, FB_H);                         /* the window's size is the core's video mode */
}
#endif

/* a whole-screen picture (200 rows) put in the middle of the screen */
static void pic_put(const uint8_t *pic, int scene)
{
    const int top = g_tall / 2 * FB_W, rows = FB_H - g_tall, n = FB_W * rows;
    for (int i = 0; i < top; i++) fb[i] = fb[top + n + i] = 0xff000000u;
    if (scene) for (int i = 0; i < FB_W * FB_H; i++) { zb[i] = 0.0f; idb[i] = -1; pxi[i] = 0; }
    for (int y = 0; y < rows; y++) {                 /* (g_yd: every second row of the picture in a wide one) */
        const uint8_t *from = pic + (size_t)(y * g_yd) * ART_W;
        uint32_t *to = fb + top + y * FB_W;
        for (int x = 0; x < FB_W; x++) to[x] = g_pal[from[x]];
        if (scene) for (int x = 0; x < FB_W; x++) pxi[top + y * FB_W + x] = (int16_t)from[x];
    }
}

/* the backdrop as whole pixels, made once (and by the main thread: sky_paint
   may be another thread's work, and the port's memory is the main thread's) */
static const uint8_t *g_sky_packed_of;
static uint32_t *g_sky_packed;
static void sky_ready(const sky *s)
{
    if (!s->pix || g_sky_packed_of == s->pix) return;
    g_sky_packed_of = s->pix;
    g_sky_packed = malloc(sizeof(uint32_t) * (size_t)s->w * (size_t)s->h);
    for (long i = 0; i < (long)s->w * s->h; i++)
        g_sky_packed[i] = 0xFF000000u | ((uint32_t)s->pix[3 * i] << 16) | ((uint32_t)s->pix[3 * i + 1] << 8) | s->pix[3 * i + 2];
}

static void sky_paint(const float yaw, const float pitch, const float roll,
                      const sky *s, float focal_x, float focal_y)
{
    (void)focal_x;
    if (getenv("OS_SKYMARK")) {                 /* a test: no backdrop but magenta, to find holes in the scene */
        for (int i = 0; i < FB_W * FB_H; i++) fb[i] = 0xFFFF00FFu;
        return;
    }
    if (!s->pix) {
        /* no backdrop (Screamer 2: indoors, or the condition without one): the clear colour */
        const uint32_t clear = os_game() == 2 && g_ground_colour >= 0 ? g_pal[g_ground_colour] : COL_SKY_FALLBACK;
        for (int i = 0; i < FB_W * FB_H; i++) fb[i] = clear;
        return;
    }
    float cr = cosf(roll), sr = sinf(roll);
    float heading = (yaw + 1.5707964f) * (1024.0f / (float)M_PI);
    float off = (2047.0f - heading) * 0.5f;
    off -= floorf(off / (float)s->w) * (float)s->w;
    /* the backdrop is the same picture in both sets: doubled in high, and
       in a wide picture doubled across only -- so it is worked out in the
       backdrop's own pixels, sx screen columns and sy screen rows to one */
    const float sx = (float)g_s, sy = (float)SY;
    const float top = g_cy / sy + (22.0f - (float)s->h) + focal_y / sy * sinf(pitch);

    sky_ready(s);
    const uint32_t *const packed = g_sky_packed;
    /*
     * Where a pixel would be with the camera upright -- the backdrop's column
     * and row -- is a straight-line function of the screen's x and y: it is
     * worked out at the start of each row and stepped along it in whole
     * numbers (16 bits of fraction).  (It was floating point, two floors and
     * a remainder for every pixel: fifteen milliseconds a picture on the
     * MiSTer.)
     */
    const uint32_t ground = g_ground_colour >= 0 ? g_pal[g_ground_colour] : COL_GROUND;
    const int w = s->w, h = s->h, pow2 = (w & (w - 1)) == 0;
    const int du = (int)(cr / sx * 65536.0f), dv = (int)(-sr / sx * 65536.0f);
    for (int y = 0; y < g_vh; y++) {
        uint32_t *row = fb + (size_t)y * FB_W;
        const float ax = -FB_W * 0.5f / sx, ay = (g_cy - (float)y) / sy;
        const float ux = ax * cr - ay * sr, uy = ax * sr + ay * cr;
        float fu = off + FB_W * 0.5f / sx + ux;
        fu -= floorf(fu / (float)w) * (float)w;                 /* (a whole number of times round) */
        int ui = (int)(fu * 65536.0f), vi = (int)floorf((g_cy / sy - uy - top) * 65536.0f);
        if (dv == 0) {
            /* upright: the whole row is one row of the backdrop, or ground */
            const int v = vi >> 16;
            if (v >= h) { for (int x = 0; x < FB_W; x++) row[x] = ground; continue; }
            const uint32_t *src = packed + (size_t)(v < 0 ? 0 : v) * w;
            if (pow2) for (int x = 0; x < FB_W; x++, ui += du) row[x] = src[(ui >> 16) & (w - 1)];
            else for (int x = 0; x < FB_W; x++, ui += du) { int px = (ui >> 16) % w; if (px < 0) px += w; row[x] = src[px]; }
            continue;
        }
        for (int x = 0; x < FB_W; x++, ui += du, vi += dv) {
            int v = vi >> 16;
            if (v >= h) { row[x] = ground; continue; }
            if (v < 0) v = 0;
            int px = ui >> 16;
            if (pow2) px &= w - 1; else { px %= w; if (px < 0) px += w; }
            row[x] = packed[(size_t)v * w + px];
        }
    }
}

/* ------------------------------------------------------------ camera path */

/*
 * The path the camera drives along: MAPnOPP.SPL, the line the computer
 * opponents follow.
 *
 * THE .SPL FILES ARE CUBIC BEZIER CURVES.  Each 56-byte node is a point, the
 * index of the next node, and two handles:
 *
 *     +0x04  x, y, z      the point
 *     +0x10  next         index of the next node (the chain closes on node 0)
 *     +0x14  x, y         handle on the way IN to this point
 *     +0x20  x, y         handle on the way OUT of it
 *
 * and the piece of curve from node A to node B = A.next is the Bezier
 * (A.point, A.out, B.in, B.point).  Checked against the road itself: sampled
 * that way, 200 of 200 points of track 1's opponent line lie on a road tile.
 * With the handles swapped it is 140, and joining the points with straight
 * lines 157 -- the old port did roughly that, which is why its camera wandered
 * onto the grass in the bends.
 *
 * The curve is then resampled at an even PATH_STEP, so one sample is one
 * fixed distance.  The height is not in the spline; path_drape() takes it from
 * the surface of the track.
 */
#define PATH_STEP 400.0f
#define SPL_MAX_NODES 4096

typedef struct {
    float p[MAX_SAMPLES][3];
    int   count;
} path;

static int path_build(path *r, const char *file)
{
    FILE *f = fopen(file, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", file); return -1; }
    static int32_t node[SPL_MAX_NODES][14];
    int32_t hdr[2];
    if (fread(hdr, 4, 2, f) != 2 || hdr[0] < 2 || hdr[0] > SPL_MAX_NODES ||
        fread(node, 56, (size_t)hdr[0], f) != (size_t)hdr[0]) {
        fclose(f);
        fprintf(stderr, "bad spline %s\n", file);
        return -1;
    }
    fclose(f);
    int n = hdr[0];

    /* walk the chain finely, dropping a sample every PATH_STEP of distance */
    r->count = 0;
    float carry = 0.0f, px = 0.0f, py = 0.0f;
    int have = 0, k = 0;
    for (int seg = 0; seg < n; seg++) {
        const int32_t *a = node[k];
        int nx = a[4];
        if (nx < 0 || nx >= n) break;
        const int32_t *b = node[nx];
        float P0[2] = { (float)a[1], (float)a[2] }, C1[2] = { (float)a[8], (float)a[9] };
        float C2[2] = { (float)b[5], (float)b[6] }, P1[2] = { (float)b[1], (float)b[2] };
        for (int j = 0; j < 512; j++) {
            float t = (float)j / 512.0f, m = 1.0f - t;
            float x = m*m*m*P0[0] + 3*m*m*t*C1[0] + 3*m*t*t*C2[0] + t*t*t*P1[0];
            float y = m*m*m*P0[1] + 3*m*m*t*C1[1] + 3*m*t*t*C2[1] + t*t*t*P1[1];
            if (have) carry += sqrtf((x - px) * (x - px) + (y - py) * (y - py));
            if ((!have || carry >= PATH_STEP) && r->count < MAX_SAMPLES) {
                r->p[r->count][0] = x;
                r->p[r->count][1] = y;
                r->p[r->count][2] = 0.0f;
                r->count++;
                carry = 0.0f;
            }
            px = x; py = y; have = 1;
        }
        k = nx;
        if (k == 0) break;
    }
    return r->count >= 8 ? 0 : -1;
}

/*
 * SCREAMER 2's ride: along the middle of the road.  Its MAPnOPP.SPL is one line
 * for all the cars, the same curve as Screamer's (0x1099a, 0x4a831: a quadratic
 * B-spline over the handles) -- but the cars there steer for themselves and the
 * line is only what they aim at: it runs up to two road-widths wide of the road
 * in places.  So the ride is laid over the MMP's road-centre points instead,
 * taken in order: from the one nearest the grid, always the nearest not yet
 * used that lies ahead, then smoothed and set out at the even PATH_STEP.
 */
static void path_from_road(path *r, const scene *sc)
{
    static float px[SCENE_MAX_WALL + 1], py[SCENE_MAX_WALL + 1];
    static unsigned char used[SCENE_MAX_WALL];
    int n = 0, at = 0;
    if (sc->ndir < 16 || r->count < 8) return;
    float best = 1e30f, dx = r->p[4][0] - r->p[0][0], dy = r->p[4][1] - r->p[0][1];
    for (int i = 0; i < sc->ndir; i++) {
        float d = (sc->dir[i].x - r->p[0][0]) * (sc->dir[i].x - r->p[0][0]) + (sc->dir[i].y - r->p[0][1]) * (sc->dir[i].y - r->p[0][1]);
        used[i] = 0;
        if (d < best) { best = d; at = i; }
    }
    for (;;) {
        px[n] = sc->dir[at].x; py[n] = sc->dir[at].y; n++;
        used[at] = 1;
        int next = -1;
        best = 6000.0f * 6000.0f;                       /* further than that is not the next point: the lap is closed */
        for (int i = 0; i < sc->ndir; i++) {
            if (used[i]) continue;
            float ex = sc->dir[i].x - px[n - 1], ey = sc->dir[i].y - py[n - 1], d = ex * ex + ey * ey;
            if (d < best && ex * dx + ey * dy > 0.0f) { best = d; next = i; }
        }
        if (next < 0) break;
        dx = sc->dir[next].x - px[n - 1]; dy = sc->dir[next].y - py[n - 1];
        at = next;
    }
    if (n < sc->ndir * 3 / 4) {                         /* not one loop: keep the cars' line */
        fprintf(stderr, "ride: the road's centre points do not make one lap (%d of %d): along the cars' line\n", n, sc->ndir);
        return;
    }
    for (int pass = 0; pass < 4; pass++) {              /* the points are whole numbers on a grid: round the corners off */
        float fx = px[0], fy = py[0], lx = px[n - 1], ly = py[n - 1];
        for (int k = 0; k < n; k++) {
            float cx = px[k], cy = py[k];
            float nx = k + 1 < n ? px[k + 1] : fx, ny = k + 1 < n ? py[k + 1] : fy;
            px[k] = 0.25f * lx + 0.5f * cx + 0.25f * nx; py[k] = 0.25f * ly + 0.5f * cy + 0.25f * ny;
            lx = cx; ly = cy;
        }
    }
    px[n] = px[0]; py[n] = py[0];
    r->count = 0;
    float carry = PATH_STEP;
    for (int k = 0; k < n; k++) {
        float sx = px[k + 1] - px[k], sy = py[k + 1] - py[k], len = sqrtf(sx * sx + sy * sy);
        for (float d = PATH_STEP - carry; d < len && r->count < MAX_SAMPLES; d += PATH_STEP) {
            r->p[r->count][0] = px[k] + sx * d / len;
            r->p[r->count][1] = py[k] + sy * d / len;
            r->p[r->count][2] = 0.0f;
            r->count++;
            carry = 0.0f;
        }
        carry = fmodf(carry + len, PATH_STEP);
    }
}

/* ------------------------------------------------------- palette, materials */

/*
 * MAPn.MLB -- THE MATERIALS.  Read out of the game's polygon routines, which
 * leaves nothing to tune:
 *
 *   material record, 16 bytes, at the byte offset a face stores (face + 4):
 *     +0x00  ROUTINE SELECTOR -- not a size.  The renderer does
 *                call [record[0] + 0x722e0 + n]
 *            into a table of polygon fillers (it sits at 0x822e0 in the flat
 *            image).  16 = opaque texture, 64 = texture with palette index 0
 *            transparent, 224 = see-through (blended with what is behind it
 *            through a lookup table), 128 = no texture (table separators).
 *     +0x04  offset of the TEXTURE RECORD
 *     +0x08  U EXTENT   } the size, in texels, of this material's rectangle.
 *     +0x0C  V EXTENT   } A vertex's raw (u, v) is a 0..255 fraction of it:
 *                           texel_u = raw_u * U_EXTENT / 256
 *                           texel_v = raw_v * V_EXTENT / 256
 *
 *   texture record:
 *     +0x00  number of FRAMES
 *     +0x04  (unused here; looks like the texture's average colour)
 *     +0x08  data offset of frame 0, then of the further frames
 *
 *   WHICH FRAME (0x8ab47, 0xd5980):  frame = frames * phase >> 14, where phase
 *   is the drawn object's record +0x20 >> 2 -- or, when that is -1 as it is for
 *   every track piece, the low 14 bits of the ANIMATION CLOCK.  The clock
 *   (0x1fcc4) is a number that runs 0..65535 and round again every 3.33 s
 *   (it gains 0.3 a second and wraps at 1), so a track texture plays through
 *   four times in that: once every 0.83 s.  That is the START banner blowing
 *   in the wind (15 frames), track 1's giant TV screen (10) and its turning
 *   "Skulls" sign (20), and so on -- four to nine textures a track.
 *   A CAR's textures have 8 frames and its own phase picks one: frames 0-3 are
 *   the first paint job and 4-7 the second (CARS_DATA `animation` x8000 >> 2
 *   = 0x2000 -> frame 4).  Within a paint job (0x173a9): + 2 while NOT
 *   braking (the brake lights' other picture), + 1 at or under 4000 units/s
 *   (the wheel's still picture; the other is its blur).
 *   These were once taken for detail levels chosen by distance.  They are not.
 *
 *   pixels: 8-bit palette indices in a page that is ALWAYS 256 BYTES PER ROW,
 *   whatever the rectangle's width --
 *           pixel = data[texel_v * 256 + texel_u]
 *   which is the engine's `mov bh, v / mov bl, u / mov al, [ebx + data]`.
 *
 * So the road surface (extent 254x254) is a full 256x256 picture, a guardrail
 * is 254x32, the crowd 120x45, a lane line 12x200.  The two extent fields
 * were once taken for a pair of palette colours, and every texture for a
 * 256x16 strip; both readings are dead.
 */
#define MAT_FRAMES 128  /* the menu's letter cubes have 76; Screamer 2's cars 128, picked by the car's flags */
typedef struct {
    int type;               /* routine selector: 16, 64, 224, ... */
    int su, sv;             /* rectangle size in texels */
    int levels;             /* number of frames */
    const uint8_t *page;    /* frame 0, 256 bytes per row; NULL = untextured */
    long avail;             /* bytes readable from `page` */
    const uint8_t *frame[MAT_FRAMES];
    long frame_avail[MAT_FRAMES];
} material;
#define MAT_MAX 1024
static material g_mat[MAT_MAX];
static float    g_obj_zbias = 1.0f;
static int      g_obj_over;     /* the object being drawn is not hidden by the road it stands in (scene_object.over) */
static float    g_obj_ground_vy;   /* ...and the height in the view of the place it stands on (its middle) */
static float    g_obj_eye_h;       /* ...and the camera's height over that road (over_ground) */
static float    g_eye_h = 400.0f;  /* this picture: the camera's height over the road under it */
static int      g_wheel_bias = 1;     /* OS_NOWHEELBIAS=1 turns it off */
static int      g_flat_colour = -1;   /* >= 0: the face being drawn is one flat palette colour */
static int      g_env_phase;    /* 0..65535: which reflection picture (routine 160) */
/* SCREAMER 2: THE REFLECTIONS' MAP.  A library loaded with flag 4 (0x4feb4: the race's "env" of
   gfx\envp<track><look>1.dat, the menu's of envm<..>.dat) is the engine's 0xae4e8, and while
   there is one routine 160 (0xcc510 .. 0xcd443) takes its PIXELS from that library's first
   material -- picture (frames x the object's +0x3c) >> 16 -- and only the size of the face's own
   material (a car's: 8 x 8, a placeholder of blue squares); 0x50165 / 0x50247 let it go.
   The material number of that first material, or -1 (Screamer 1: always -1) */
static int      g_env_mat = -1;
static int      g_tex_phase;    /* 0..16383: which frame of each texture is being drawn */
static int      g_anim_clock;   /* 0..65535, once round every 3.33 s (0x21538) */
static float    g_anim_time;    /* the same as 0..1 (0x18e08) */
static int      g_nmat;
static uint8_t *g_mlb;

static int32_t rd32(const uint8_t *p)
{
    return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                     ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

static int mlb_read(const char *path, int base);
static void mlb_load(const char *path)
{
    memset(g_mat, 0, sizeof g_mat);
    g_nmat = 0;
    g_env_mat = -1;
    mlb_read(path, 0);
}

/* ANOTHER material library after the ones there are (Screamer 2: each car has its own;
   the engine keeps the library's address with every object loaded while it was the
   current one, object +0xb4, 0x4b4d4).  Gives the number its first material gets --
   mesh_rebase() moves an object's faces onto it -- or -1. */
static int mlb_add(const char *path)
{
    const int base = g_nmat;
    return mlb_read(path, base) ? base : -1;
}

static void mesh_rebase(mesh *m, int base)
{
    for (int f = 0; f < m->nface; f++) m->material[f] += base * 16;
}

static int mlb_read(const char *path, int base)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "note: no materials at %s\n", path); return 0; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc((size_t)len);
    if (!buf || fread(buf, 1, (size_t)len, f) != (size_t)len) {
        free(buf); fclose(f);
        fprintf(stderr, "note: cannot read %s\n", path);
        return 0;
    }
    fclose(f);
    if (base == 0) g_mlb = buf;

    /* the material table runs up to the first texture record */
    long table_end = len;
    int ntex = 0;
    for (int k = 0; base + k < MAT_MAX && (long)(k + 1) * 16 <= table_end; k++) {
        const int i = base + k;
        const uint8_t *p = buf + k * 16;
        int ro = rd32(p + 4);
        g_mat[i].type = rd32(p);
        /* SCREAMER 2's routine 208 (the shadow, the smoke: MISC.MLB): drawn as the port's 224,
           texels = strengths looked up through the SMG table -- by the original's picture
           (soft shadow, smoke by degrees), not yet from its code */
        if (g_mat[i].type == 208) g_mat[i].type = 224;
        g_mat[i].su   = rd32(p + 8);
        g_mat[i].sv   = rd32(p + 12);
        g_nmat = i + 1;
        if (ro <= 0 || (long)ro + 12 > len) continue;
        if (ro < table_end) table_end = ro;
        int doff = rd32(buf + ro + 8);
        if (doff <= 0 || doff >= len) continue;
        g_mat[i].levels = rd32(buf + ro);
        g_mat[i].page   = buf + doff;
        g_mat[i].avail  = len - doff;
        if (g_mat[i].levels < 1 || g_mat[i].levels > MAT_FRAMES) g_mat[i].levels = 1;
        for (int fr = 0; fr < g_mat[i].levels; fr++) {
            int fo = ((long)ro + 12 + 4 * fr <= len) ? rd32(buf + ro + 8 + 4 * fr) : 0;
            if (fo <= 0 || fo >= len) fo = doff;
            g_mat[i].frame[fr] = buf + fo;
            g_mat[i].frame_avail[fr] = len - fo;
        }
        ntex++;
    }
    if (base) return 1;
    fprintf(stderr, "materials: %d records, %d with a texture\n", g_nmat, ntex);
    return 1;
}

static void pal_load(const char *path)
{
    for (int i = 0; i < 256; i++) g_pal[i] = 0xFF000000u;
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "note: no palette at %s\n", path); return; }
    /* 6-bit VGA DAC values (0..63), so scale by 4 */
    unsigned char b[768];
    if (fread(b, 1, sizeof b, f) == sizeof b)
        for (int i = 0; i < 256; i++)
            g_pal[i] = 0xFF000000u
                     | ((uint32_t)(b[3*i]     * 4) << 16)
                     | ((uint32_t)(b[3*i + 1] * 4) << 8)
                     |  (uint32_t)(b[3*i + 2] * 4);
    fclose(f);
}

/* --------------------------------------------------------------- rasteriser */

/* a vertex in VIEW space: x right, y up, z forward; u, v in texels */
typedef struct { float x, y, z, u, v; } vtx;

static float g_vc, g_vs, g_vcp, g_vsp;     /* camera yaw / pitch, cached */
static float g_vcr = 1.0f, g_vsr = 0.0f;   /* camera roll */
static camera g_cam;

static void view_setup(const camera *cam)
{
    g_cam = *cam;
    g_vc = cosf(cam->yaw);   g_vs = sinf(cam->yaw);
    g_vcp = cosf(cam->pitch); g_vsp = sinf(cam->pitch);
    g_vcr = cosf(cam->roll);  g_vsr = sinf(cam->roll);
}

/*
 * World (X east, Y north, Z up) -> view.  yaw is measured from +X, so forward
 * is (cos yaw, sin yaw) and RIGHT = forward x up = (sin yaw, -cos yaw).
 */
static void to_view(const float w[3], vtx *o)
{
    float dx = w[0] - g_cam.x, dy = w[1] - g_cam.y, dz = w[2] - g_cam.z;
    float fx = g_vc * dx + g_vs * dy;
    float x = g_vs * dx - g_vc * dy;
    float y = g_vcp * dz - g_vsp * fx;
    o->x = x * g_vcr + y * g_vsr;          /* lean with the camera */
    o->y = y * g_vcr - x * g_vsr;
    o->z = g_vsp * dz + g_vcp * fx;
}

/*
 * SCREAMER 2'S TWO DARKENING TABLES (0x5105e, made when the palette is set): for each of the
 * 256 colours the palette's nearest to 0.67 of it (table 0) and to 0.6 of it (table 1) -- the
 * first of equals; where the nearest is the colour itself, the nearest to 0.7 of that again.
 * Table 0 is the race screen's boxes (0x51698).  Table 1 is used by a filler at 0xc9030
 * (`mov al,[edi]; mov al,[table + al]; mov [edi],al` down each column of a face): WHICH faces
 * is not read.  It is NOT the cars' shadow, tried and taken back: the original's shadow is
 * soft-edged and its smoke see-through by degrees (build/png/s2_original_road.png), which
 * is the texel-as-strength blend the port's routine 224 does through MAPn.SMG.
 */
static uint8_t g_s2_dark[2][256];
static int     g_s2_dark_ok;
static void s2_dark_make(void)
{
    static uint32_t made_for[256];
    static const float part[2] = { 0.67f, 0.6f };
    if (g_s2_dark_ok && !memcmp(made_for, g_pal, sizeof made_for)) return;
    memcpy(made_for, g_pal, sizeof made_for);
    for (int t = 0; t < 2; t++)
        for (int i = 0; i < 256; i++) {
            const float want[3] = { (float)((g_pal[i] >> 16) & 255) * part[t], (float)((g_pal[i] >> 8) & 255) * part[t], (float)(g_pal[i] & 255) * part[t] };
            int best = 0;
            for (int pass = 0; pass < 2; pass++) {
                float least = 2.21e7f;
                const float k = pass ? 0.7f : 1.0f;
                for (int q = 0; q < 256; q++) {
                    const float dr = (float)((g_pal[q] >> 16) & 255) - want[0] * k, dg = (float)((g_pal[q] >> 8) & 255) - want[1] * k,
                                db = (float)(g_pal[q] & 255) - want[2] * k;
                    const float d = db * db + dg * dg + dr * dr;
                    if (d < least) { least = d; best = q; }
                }
                if (best != i) break;
            }
            g_s2_dark[t][i] = (uint8_t)best;
        }
    g_s2_dark_ok = 1;
}

static uint32_t mix(uint32_t a, uint32_t b, float t)
{
    if (t <= 0) return a;
    if (t > 1) t = 1;
    int ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
    int br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
    return 0xFF000000u | ((uint32_t)(ar + (br - ar) * t) << 16)
                       | ((uint32_t)(ag + (bg - ag) * t) << 8)
                       |  (uint32_t)(ab + (bb - ab) * t);
}

/*
 * One textured triangle, perspective-correct, depth-tested.
 *
 * A depth buffer rather than a painter's sort: the track is made of large
 * tiles with walls, paint and kerbs standing on and beside them, and no
 * ordering of whole polygons gets all of those right from every angle (the
 * old port's "the marking clips through the road" and "pieces switch sides in
 * the bend" were all sort errors).  At 320x200 a depth buffer costs nothing.
 */
/* floor and ceiling of a number known to be within a whole number's range,
   without a call into the maths library (the ARM has no instruction for them,
   and raster_tri wanted them twice a row) */
static inline int ifloor(float x) { int i = (int)x; return i - (x < (float)i); }
static inline int iceil(float x)  { int i = (int)x; return i + (x > (float)i); }

/*
 * A PUFF OVER THE ROAD IT STANDS IN (raster_tri, "smoke is not cut off by the
 * road").  The road may cover the puff's lower half only where it is the
 * ground between the camera and the puff's foot: a point `below` units under
 * the middle (the foot) of a puff at depth d is seen through flat road at
 * depth d * h / (h + below), h being the camera's height over the road (on an
 * even slope too, measured from the slope; the port takes the smaller of its
 * height over the road under it and over the puff's foot).  Road nearer than that -- a
 * crest, a bump -- hides it, as does any road over its upper half, and walls,
 * scenery and cars are tested as for anything else.  0.85: room for a road
 * that is not quite flat.  (It was: the road hid no puff less than 2.2 times
 * nearer, and Screamer 2's puffs were tested as twice as near as they are --
 * so an opponent's smoke showed through bumps and walls (Danik).)  The rule
 * starts 64 units over the middle, not at it: a puff's middle is the wheel's
 * foot, a little under the road as drawn, and the row just over the middle fell
 * to the plain test and lost to the road -- a one-pixel line through each puff,
 * long straight streaks when a sliding car's puffs stand side by side (Danik).
 */
static inline int over_ground(float mine, float there, float py, float gvy, float h)
{
    const float below = gvy - (g_cy - py) / (g_focal_y * mine);
    return below > -64.0f && mine > there * 0.85f * h / (h + below);
}

/* what raster_tri's rules of depth (they are explained there) say about a
   pixel that is not clearly in front of what is there: 1 = draw it */
static inline int depth_keep(float mine, float there, int other, int id, int cm, int sprite, int tie, float py, float gvy, float eh)
{
    if (sprite && other >= 0 && g_obj_ground[other] && over_ground(mine, there, py, gvy, eh)) return 1;
    if (mine * 2.0f < there) return 0;
    const int co = other >= 0 ? g_obj_carpet[other] : 0;
    if (tie && other >= 0 && cm != co && there > 0.0f)
        return !(cm ? mine < there * 2.0f : mine * 2.0f < there);
    if (tie && other >= 0 && other != id && id >= 0 && there > 0.0f &&
        mine > there * 0.9999f && mine < there * 1.0001f && g_obj_d2[id] != g_obj_d2[other])
        return !(g_obj_d2[id] > g_obj_d2[other]);
    return !(mine < there);
}
typedef uint32_t __attribute__((may_alias)) depth_bits;       /* a depth read as a whole number: the order is the same */

/*
 * A TRIANGLE TO FILL, with everything the filling needs to know about the
 * object it came from.  While a scene is drawn (render_frame) the triangles
 * are only listed, in order, and filled by TWO threads, each with rows of
 * its own (see the list, below) -- the MiSTer has two processors and a
 * picture's time is mostly filling.  A row is filled by one thread only, in
 * the list's order, so the picture is the same as from one thread.
 */
typedef struct {
    float ax, ay, bx, by, cx, cy;       /* the corners on the screen */
    float ia, ib, ic;                   /* 1 / depth at each */
    float au, av, bu, bv, cu, cv;       /* texture coordinates at each */
    const material *m;
    int   id, flat, sprite, tex_phase, env_phase;
    const uint8_t *fade;
    float zbias;
    float ground_vy, eye_h;             /* sprite: g_obj_ground_vy, g_obj_eye_h */
} tri_job;

static void raster_job(const tri_job *j, int rows, int counted);
static void raster_list(const tri_job *j);

static void raster_tri(const vtx *a, const vtx *b, const vtx *c,
                       const material *m, int id)
{
    tri_job j;
    j.ia = 1.0f / a->z; j.ib = 1.0f / b->z; j.ic = 1.0f / c->z;
    j.ax = FB_W * 0.5f + a->x * g_focal_x * j.ia;
    j.ay = g_cy - a->y * g_focal_y * j.ia;
    j.bx = FB_W * 0.5f + b->x * g_focal_x * j.ib;
    j.by = g_cy - b->y * g_focal_y * j.ib;
    j.cx = FB_W * 0.5f + c->x * g_focal_x * j.ic;
    j.cy = g_cy - c->y * g_focal_y * j.ic;
    {
        /* nothing of it in the picture: not worth listing */
        float lo = j.ax < j.bx ? j.ax : j.bx, hi = j.ax > j.bx ? j.ax : j.bx;
        if (j.cx < lo) lo = j.cx;
        if (j.cx > hi) hi = j.cx;
        if (!(hi >= 0 && lo <= FB_W)) return;
        lo = j.ay < j.by ? j.ay : j.by; hi = j.ay > j.by ? j.ay : j.by;
        if (j.cy < lo) lo = j.cy;
        if (j.cy > hi) hi = j.cy;
        if (!(hi >= 0 && lo <= FB_H)) return;
    }
    j.au = a->u; j.av = a->v; j.bu = b->u; j.bv = b->v; j.cu = c->u; j.cv = c->v;
    j.m = m; j.id = id;
    j.flat = g_flat_colour; j.sprite = g_obj_over; j.zbias = g_obj_zbias; j.fade = g_fade_row; j.ground_vy = g_obj_ground_vy; j.eye_h = g_obj_eye_h;
    j.tex_phase = g_tex_phase; j.env_phase = g_env_phase;
    raster_list(&j);
}

static void raster_job(const tri_job *j, int rows, int counted)      /* rows: bit (y & 7) set = this caller fills row y */
{
    const material *const m = j->m;
    const int id = j->id;
    (void)counted;
    const float ax = j->ax, ay = j->ay, bx = j->bx, by = j->by, cx = j->cx, cy = j->cy;

    float area = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);
    if (fabsf(area) < 1e-6f) return;

    float fminx = ax < bx ? ax : bx, fmaxx = ax > bx ? ax : bx;
    float fminy = ay < by ? ay : by, fmaxy = ay > by ? ay : by;
    if (cx < fminx) fminx = cx;
    if (cx > fmaxx) fmaxx = cx;
    if (cy < fminy) fminy = cy;
    if (cy > fmaxy) fmaxy = cy;
    if (!(fmaxx >= 0 && fminx <= FB_W && fmaxy >= 0 && fminy <= FB_H)) return;
    int minx = fminx < 0 ? 0 : ifloor(fminx);
    int miny = fminy < 0 ? 0 : ifloor(fminy);
    int maxx = fmaxx > FB_W - 1 ? FB_W - 1 : iceil(fmaxx);
    int maxy = fmaxy > g_vh - 1 ? g_vh - 1 : iceil(fmaxy);

    /* no row of this thread's, or no column, has its centre in the triangle:
       most of what is far away is like that, and needs nothing more */
    {
        int r0 = fminy - 0.5f <= (float)miny ? miny : iceil(fminy - 0.5f);
        const int r1 = fmaxy - 0.5f >= (float)maxy ? maxy : ifloor(fmaxy - 0.5f);
        while (r0 <= r1 && !(rows >> (r0 & 7) & 1)) r0++;
        if (r0 > r1) return;
        const int c0 = fminx - 0.5f <= (float)minx ? minx : iceil(fminx - 0.5f);
        const int c1 = fmaxx - 0.5f >= (float)maxx ? maxx : ifloor(fmaxx - 0.5f);
        if (c0 > c1) return;
    }
#ifdef MISTER_HYBRID
    if (counted) mister_tris++;
#endif
    const float ia = j->ia, ib = j->ib, ic = j->ic;
    float inv = 1.0f / area;
    const int keyed = (m->type == 64);
    const int glass = (m->type == 224);
    const material *const pm = (m->type == 160 && g_env_mat >= 0) ? &g_mat[g_env_mat] : m;   /* (Screamer 2: the reflections' map) */
    int fr = (m->type == 160) ? (pm->levels * j->env_phase >> 16) : (m->levels * j->tex_phase >> 14);
    if (fr < 0 || fr >= pm->levels) fr = 0;
    const uint8_t *page = pm->frame[fr];
    const long avail = pm->frame_avail[fr];
    /* a decal is coplanar with what it is stuck on; let it win the tie */
    /* a reflection (routine 160) is the glass UNDER the bodywork's cut-out
       texture: it must lose every tie with the faces lying on it */
    const float zbias = (keyed ? 1.002f : m->type == 160 ? 0.998f : 1.0f) * j->zbias;

    /*
     * The loop.  (2026-10-11, for the MiSTer's ARM, where it took over half of
     * a picture's time: nothing is divided or tested per row any more.)
     *  - 1/z, u/z and v/z are straight-line functions of the screen's x and y:
     *    their two slopes are worked out once a triangle;
     *  - the corners are put in order of height, and a row's stretch runs
     *    between the long edge and whichever short edge is at that height.
     *    A pixel is drawn when its centre is in the triangle, edges included,
     *    as before;
     *  - what the loop reads from the program's globals is read once.
     */
    const float e1x = bx - ax, e1y = by - ay, e2x = cx - ax, e2y = cy - ay;
    const float ua = j->au * ia, ub = j->bu * ib, uc = j->cu * ic;
    const float va = j->av * ia, vb = j->bv * ib, vc = j->cv * ic;
#define PLANE_(qa_, qb_, qc_, dx_, dy_) \
    const float dx_ = (((qb_) - (qa_)) * e2y - ((qc_) - (qa_)) * e1y) * inv, \
                dy_ = (((qc_) - (qa_)) * e1x - ((qb_) - (qa_)) * e2x) * inv
    PLANE_(ia, ib, ic, diz, dizy);
    PLANE_(ua, ub, uc, duz, duzy);
    PLANE_(va, vb, vc, dvz, dvzy);
#undef PLANE_
    float x0 = ax, y0 = ay, x1 = bx, y1 = by, x2 = cx, y2 = cy, t_;
    if (y1 < y0) { t_ = x0; x0 = x1; x1 = t_; t_ = y0; y0 = y1; y1 = t_; }
    if (y2 < y0) { t_ = x0; x0 = x2; x2 = t_; t_ = y0; y0 = y2; y2 = t_; }
    if (y2 < y1) { t_ = x1; x1 = x2; x2 = t_; t_ = y1; y1 = y2; y2 = t_; }
    const float s_long = y2 > y0 ? (x2 - x0) / (y2 - y0) : 0.0f;
    const float s_up   = y1 > y0 ? (x1 - x0) / (y1 - y0) : 0.0f;
    const float s_low  = y2 > y1 ? (x2 - x1) / (y2 - y1) : 0.0f;
    int ys = y0 - 0.5f <= (float)miny ? miny : iceil(y0 - 0.5f);         /* (y0 is at most FB_H here) */
    int ye = y2 - 0.5f >= (float)maxy ? maxy : ifloor(y2 - 0.5f);         /* (and y2 at least 0) */
    const int   wraps = (m->type == 160), fbw = FB_W;
    const int   wrap_mask = pm != m ? 127 : 255;     /* (Screamer 2's reflections' map: 0x7f7f, 0xcd5a1) */
    const int   flat = j->flat, sprite = j->sprite, tie = g_tie_rule;
    const float gvy = j->ground_vy, eh = j->eye_h;
    const int   cm = id >= 0 ? g_obj_carpet[id] : 0;
    const float fog = g_fog;
    const int   fogged = fog > 0.0f;
    const int   plain = !glass && !fogged && flat < 0 && !wraps;
    const uint32_t *const pal = g_pal;
    const uint8_t *const fade = j->fade;
    for (int y = ys; y <= ye; y++) {
        if (!(rows >> (y & 7) & 1)) continue;               /* another lane's row */
        const float py = (float)y + 0.5f;
        float xa = x0 + (py - y0) * s_long;
        float xb = py < y1 ? x0 + (py - y0) * s_up : x1 + (py - y1) * s_low;
        if (xa > xb) { t_ = xa; xa = xb; xb = t_; }
        xa -= 0.5f; xb -= 0.5f;
        if (!(xa <= (float)maxx && xb >= (float)minx)) continue;
        const int xs = xa <= (float)minx ? minx : iceil(xa), xe = xb >= (float)maxx ? maxx : ifloor(xb);
        if (xs > xe) continue;
#ifdef MISTER_HYBRID
        if (counted) { mister_pixels += xe - xs + 1; mister_rows++; }    /* MISTER: for the speed line of the log */
#endif
        const float fx = (float)xs + 0.5f - ax, fy = py - ay;
        float iz = ia + fx * diz + fy * dizy;
        float uz = ua + fx * duz + fy * duzy, vz = va + fx * dvz + fy * dvzy;
        /* the texture's place is worked out exactly every eighth pixel and
           run in a straight line between (one division for eight pixels) */
        float rz_ = 1.0f / iz;
        float u_at = uz * rz_, v_at = vz * rz_;
        for (int x = xs, pi = y * fbw + xs; x <= xe; ) {
          int n_ = xe - x + 1;
          if (n_ > 8) n_ = 8;
          static const float part[9] = { 0.0f, 65536.0f, 65536.0f / 2, 65536.0f / 3, 65536.0f / 4, 65536.0f / 5, 65536.0f / 6, 65536.0f / 7, 65536.0f / 8 };
          const float fn_ = (float)n_;
          uz += duz * fn_; vz += dvz * fn_;
          rz_ = 1.0f / (iz + diz * fn_);
          const float u_to = uz * rz_, v_to = vz * rz_;
          int ui = (int)(u_at * 65536.0f), vi = (int)(v_at * 65536.0f);
          const int dui = (int)((u_to - u_at) * part[n_]), dvi = (int)((v_to - v_at) * part[n_]);
          u_at = u_to; v_at = v_to;
          if (plain) {
              /* The loop nearly every pixel goes through -- a texture, solid
                 or with holes, nothing else going on -- with as little in it
                 as can be: the depths are compared as whole numbers (twice a
                 depth is its bits + 0x00800000), and only a pixel within a
                 factor of two of what is there is put to the rules. */
              uint32_t *const cp = fb + pi;
              depth_bits *const zp = (depth_bits *)(zb + pi);
              int16_t *const ip = idb + pi, *const xp = pxi + pi;
              for (int k_ = 0; k_ < n_; k_++, iz += diz, ui += dui, vi += dvi) {
                  union { float f; uint32_t u; } mz, tz;
                  mz.f = iz * zbias;
                  tz.u = zp[k_];
                  if (mz.u <= tz.u + 0x00800000u && !depth_keep(mz.f, tz.f, ip[k_], id, cm, sprite, tie, py, gvy, eh)) continue;
                  int tu = ui >> 16, tv = vi >> 16;
                  if ((tu | tv) & ~255) {
                      tu = tu < 0 ? 0 : tu > 255 ? 255 : tu;
                      tv = tv < 0 ? 0 : tv > 255 ? 255 : tv;
                  }
                  const long o = (long)tv * 256 + tu;
                  if (o >= avail) continue;
                  unsigned idx = page[o];
                  if (keyed && idx == 0) continue;
                  if (fade) idx = fade[idx];
                  cp[k_] = pal[idx];
                  zp[k_] = mz.u;
                  ip[k_] = (int16_t)id;
                  xp[k_] = (int16_t)idx;
              }
              x += n_; pi += n_;
              continue;
          }
          for (int k_ = 0; k_ < n_; k_++, x++, pi++, iz += diz, ui += dui, vi += dvi) {
            int tiewon = 0;
            {
                /*
                 * TWO OBJECTS IN THE SAME PLACE.  The engine has no depth
                 * buffer: it lists the objects with the distance from the
                 * camera to each one's own position (0x83164) and paints them
                 * whole, in that order.  Where the track's makers let two
                 * objects share a wall -- Sandrock's hotel by the start is
                 * two pieces of the same outline, one 10591 units above the
                 * other and overlapping it for 4400 -- the nearer OBJECT is
                 * simply painted over the other.  A depth buffer makes the
                 * two walls fight pixel by pixel instead (Danik: flickering
                 * stripes).  So when a pixel's depth is the same as what is
                 * there to one part in ten thousand and it belongs to another
                 * object, the game's rule decides: the object whose own
                 * position is nearer the camera wins.
                 */
                /*
                 * THE LAWN UNDER THE ROAD.  Every track has flat ground tiles
                 * named akk..tap.. (tappeto, carpet): big squares of grass
                 * laid UNDER the road and its verges, here and there higher
                 * than a stretch of road that dips (Lake Valley's bend with
                 * the chevron board: tile at height 875 right across road
                 * piece V34).  Painted by the engine before the road on top
                 * of it, a carpet is never seen where there is road; with a
                 * depth buffer its high corner covers road and rail (Danik).
                 * So against anything that is not a carpet, a carpet counts
                 * as TWICE as far away as it is.  For a flat tile that means
                 * exactly: whatever lies less than the camera's own height
                 * above the tile BELOW it shows through -- the road, kerbs
                 * and rails laid on it -- and nothing further down does.
                 */
                /*
                 * SMOKE IS NOT CUT OFF BY THE ROAD.  A puff is a rectangle
                 * 568 units high whose MIDDLE is at the car's own height
                 * (0x19a0e: the car's place plus an offset with no height),
                 * so its lower half is under the road.  The engine paints
                 * the puff, a separate object, over the road it stands on and
                 * the whole rectangle shows; a depth buffer hid the lower
                 * half and left a straight edge along the ground (Danik).  So
                 * a puff's lower half is not hidden by the drivable track
                 * between it and the camera (over_ground: a crest further in
                 * front still hides it); walls, scenery and cars do.
                 */
                /* Most pixels are nowhere near what is there already -- more
                   than twice as near, or more than twice as far -- and for
                   those every rule above says the same as the plain test. */
                const float mine = iz * zbias, there = zb[pi];
                tiewon = 0;
                if (mine > there * 2.0f) { }
                else {
                    const int other = idb[pi];
                    const int co = other >= 0 ? g_obj_carpet[other] : 0;
                    if (sprite && other >= 0 && g_obj_ground[other] && over_ground(mine, there, py, gvy, eh)) { }
                    else if (mine * 2.0f < there) continue;
                    else if (tie && other >= 0 && cm != co && there > 0.0f) {
                        if (cm ? mine < there * 2.0f : mine * 2.0f < there) continue;
                    } else
                    if (tie && other >= 0 && other != id && id >= 0 && there > 0.0f &&
                        mine > there * 0.9999f && mine < there * 1.0001f &&
                        g_obj_d2[id] != g_obj_d2[other]) {
                        if (g_obj_d2[id] > g_obj_d2[other]) continue;
                        tiewon = g_tiemark;
                    } else if (mine < there) continue;
                }
            }
            int tu = ui >> 16, tv = vi >> 16;
            if (wraps) { tu &= wrap_mask; tv &= wrap_mask; }                           /* reflections wrap */
            else {
                if (tu < 0) tu = 0;
                if (tu > 255) tu = 255;
                if (tv < 0) tv = 0;
                if (tv > 255) tv = 255;
            }
            long o = (long)tv * 256 + tu;
            if (flat < 0 && o >= avail) continue;
            uint8_t idx = flat >= 0 ? (uint8_t)flat : page[o];
            if (keyed && idx == 0) continue;       /* routine 64: 0 = nothing */
            if (fade && !glass) idx = fade[idx];
            uint32_t col = g_pal[idx];
            if (glass) {
                /*
                 * ROUTINE 224: the texel is not a colour, it is a STRENGTH,
                 * and what is drawn is looked up from what is already there:
                 *
                 *     pixel = SMG[strength * 256 + pixel]
                 *
                 * (0xcd9dd: `mov ah,[texture]; mov al,[screen]; mov al,fs:[eax]`.)
                 * MAPn.SMG is that table: 64 rows of 256, row 0 leaves the
                 * pixel alone and higher rows tint it more.  Track 4's
                 * headlight beams are this -- a fan of strengths laid on the
                 * road in front of each car, brightening whatever is under
                 * it -- and so are window glass and shadows.
                 *
                 * It works on palette indices, so the port keeps the index of
                 * every pixel it draws (pxi).  Sky pixels come from an RGB
                 * picture and have none; the nearest palette colour is found
                 * when one is needed.
                 */
                if (!g_have_smg) { fb[pi] = mix(fb[pi], col, 0.5f); continue; }
                if (idx == 0) continue;
                int under = pxi[pi];
                if (under < 0) {
                    uint32_t c0 = fb[pi];
                    int br = (c0 >> 16) & 0xFF, bg = (c0 >> 8) & 0xFF, bb = c0 & 0xFF;
                    long best = 1L << 40;
                    for (int q = 0; q < 256; q++) {
                        uint32_t pc = g_pal[q];
                        long dr = (long)((pc >> 16) & 0xFF) - br;
                        long dg = (long)((pc >> 8) & 0xFF) - bg;
                        long db = (long)(pc & 0xFF) - bb;
                        long dd = dr * dr + dg * dg + db * db;
                        if (dd < best) { best = dd; under = q; }
                    }
                }
                int row = idx >= g_smg_rows ? g_smg_rows - 1 : idx;
                if (g_dbg_quad && sprite && row == 48) row = 64;      /* (a test: the dust's square shown whole) */
                int out = g_smg[row * 256 + under];
                fb[pi] = g_pal[out];
                pxi[pi] = (int16_t)out;
                continue;
            }
            if (fogged)
                col = mix(col, COL_FOG, 1.0f - expf(-1.0f / (iz * fog)));
            fb[pi]  = tiewon ? 0xFFFF00FFu : col;      /* (OS_TIEMARK: won on a tie of depth, by the nearer object) */
            /* the depth that is KEPT is the biased one too.  Keeping the raw
               depth let a later face that was really behind (a tyre's black
               outer wall after its hub disc, a window's reflection after the
               pillar painted over it) pass the test on a tie and overwrite. */
            zb[pi]  = iz * zbias;
            idb[pi] = (int16_t)id;
            pxi[pi] = fogged ? -1 : (int16_t)idx;
          }
        }
    }
}

/*
 * A polygon in world space: to view space, clipped against the near plane
 * (NOT thrown away when one corner is behind the camera -- the tile under the
 * car always has corners behind it), then fanned into triangles.
 */
/*
 * The list.  g_tri_listing is on from the start of a scene to its end
 * (render_frame); a triangle from anywhere else is filled at once.  The
 * helper thread and what it waits on outlive the parts of the program
 * (os_slot 3): everything else here is put back with the rest.
 */
#define TRI_LIST 12288
static tri_job g_tri[TRI_LIST];
static int     g_tri_n, g_tri_listing;
/*
 * LANES: four, every fourth row of the picture each.  A lane is the whole
 * list filled in order for its rows, by whichever thread has taken it for the
 * next eight triangles (lane_work).  The helper starts on a triangle as
 * soon as it is listed, while this thread still works the scene out; when
 * this thread has listed them all it takes lanes too.  So the work is shared
 * out by itself, however busy the helper's processor is with other things
 * (on the MiSTer it has the audio and the copying of pictures to the core).
 */
#define LANES    8
Uint64 g_helper_time[2];                         /* the helper's time painting the backdrop, and filling */
static int g_lanes = 4;                          /* how many are in use: OS_LANES (2, 3, 4 or 8) */
static const unsigned char g_lane_rows[9][8] = { {0}, {0xff}, {0x55, 0xaa}, {0x49, 0x92, 0x24}, {0x11, 0x22, 0x44, 0x88}, {0}, {0}, {0}, {1, 2, 4, 8, 16, 32, 64, 128} };
#define ROWS_ALL 0xff
typedef struct {
    SDL_sem *wake, *done;
    const tri_job *list;
    SDL_atomic_t listed, closed, busy, at[LANES];
    SDL_SpinLock lane[LANES];
    SDL_Thread *thread;
    /* before any filling, the helper paints the backdrop and clears the
       depths (it has nothing else to do while the first triangles are being
       worked out): 1 = asked for, 2 = done */
    SDL_atomic_t before;
    SDL_atomic_t show;                   /* MISTER: a finished picture to hand over first (mister_show_later) */
    float yaw, pitch, roll, focal_x, focal_y;
    const sky *backdrop;
} tri_helper;

static void frame_clear(float yaw, float pitch, float roll, const sky *s, float focal_x, float focal_y)
{
    sky_paint(yaw, pitch, roll, s, focal_x, focal_y);
    for (int i = 0; i < FB_W * FB_H; i++) { zb[i] = 0.0f; idb[i] = -1; pxi[i] = -1; }
}

/*
 * Some of the lanes, if there is any to do: 1 = did some.  The lanes that are
 * furthest behind AND at the same place in the list are taken together, up to
 * `most` of them, and filled in one go (one set-up of a triangle for all of
 * their rows): the helper alone takes all four and fills whole triangles;
 * once both threads are at it each takes two, and one that runs out takes
 * what the other lets go of between its turns.
 */
static int lane_work(tri_helper *h, int counted, int most)
{
    const int n = SDL_AtomicGet(&h->listed);
    int least = n;
    for (int k = 0; k < g_lanes; k++) {
        const int at = SDL_AtomicGet(&h->at[k]);
        if (at < least) least = at;
    }
    if (least >= n) return 0;
    for (int pass = 0; pass < 2; pass++) {               /* first the ones furthest behind, then any */
        int got[LANES], ngot = 0, from = -1;
        unsigned rows = 0;
        for (int k = 0; k < g_lanes && ngot < most; k++) {
            const int at = SDL_AtomicGet(&h->at[k]);
            if (at >= n || (pass == 0 ? at != least : (from >= 0 && at != from))) continue;
            if (!SDL_AtomicTryLock(&h->lane[k])) continue;
            const int now = SDL_AtomicGet(&h->at[k]);    /* (it may have moved before the lock was had) */
            if (now >= n || (from >= 0 ? now != from : (pass == 0 && now != least))) { SDL_AtomicUnlock(&h->lane[k]); continue; }
            from = now;
            got[ngot++] = k;
            rows |= g_lane_rows[g_lanes][k];
        }
        if (!ngot) continue;
        const int some = 8;                              /* (short turns: when the other thread comes for lanes it does not wait long) */
        const int to = from + some < n ? from + some : n;
        for (int i = from; i < to; i++) raster_job(&h->list[i], (int)rows, counted);
        for (int k = 0; k < ngot; k++) { SDL_AtomicSet(&h->at[got[k]], to); SDL_AtomicUnlock(&h->lane[got[k]]); }
        return 1;
    }
    return 0;
}

static int lanes_done(tri_helper *h)
{
    const int n = SDL_AtomicGet(&h->listed);
    if (!SDL_AtomicGet(&h->closed)) return 0;
    for (int k = 0; k < g_lanes; k++) if (SDL_AtomicGet(&h->at[k]) < n) return 0;
    return 1;
}

static int tri_helper_run(void *arg)
{
    tri_helper *h = arg;
#ifdef MISTER_HYBRID
    mister_second_cpu();
#endif
    for (;;) {
        SDL_SemWait(h->wake);
#ifdef MISTER_HYBRID
        if (SDL_AtomicGet(&h->show)) { mister_show_job(); SDL_AtomicSet(&h->show, 0); }
#endif
        if (!SDL_AtomicGet(&h->busy)) continue;
        Uint64 t0_ = SDL_GetPerformanceCounter();
        if (SDL_AtomicGet(&h->before) == 1) {
            frame_clear(h->yaw, h->pitch, h->roll, h->backdrop, h->focal_x, h->focal_y);
            SDL_AtomicSet(&h->before, 2);
        }
        g_helper_time[0] += SDL_GetPerformanceCounter() - t0_;
        for (;;) {
            t0_ = SDL_GetPerformanceCounter();
            const int did_ = lane_work(h, 0, SDL_AtomicGet(&h->closed) ? (g_lanes + 1) / 2 : g_lanes);
            if (did_) { g_helper_time[1] += SDL_GetPerformanceCounter() - t0_; continue; }
            if (0) continue;
            if (lanes_done(h)) break;
            if (SDL_AtomicGet(&h->closed)) SDL_CPUPauseInstruction();      /* the other thread is at the last of it */
            else SDL_SemWait(h->wake);                                     /* more will be listed */
        }
        SDL_AtomicSet(&h->busy, 0);
        SDL_SemPost(h->done);
    }
    return 0;
}

static tri_helper *tri_helper_get(void)
{
    static tri_helper *known;                            /* (put back with a part's statics; found again in the slot) */
    if (known) return known;
    tri_helper *h = known = os_slot(3);
    if (!h && !getenv("OS_ONETHREAD")) {
        h = SDL_calloc(1, sizeof *h);                    /* (SDL's: not the port's memory, which a change of part frees) */
        h->wake = SDL_CreateSemaphore(0);
        h->done = SDL_CreateSemaphore(0);
        h->thread = SDL_CreateThread(tri_helper_run, "filler", h);
        if (!h->wake || !h->done || !h->thread) { SDL_free(h); h = NULL; setenv("OS_ONETHREAD", "1", 1); }
        os_slot_set(3, h);
        known = h;
    }
    return h;
}

static void raster_flush(void)
{
    tri_helper *h = tri_helper_get();
    if (!h) {
        for (int i = 0; i < g_tri_n; i++) raster_job(&g_tri[i], ROWS_ALL, 1);
        g_tri_n = 0;
        return;
    }
    if (!SDL_AtomicGet(&h->busy)) return;                /* nothing was listed, nothing asked for */
#ifdef MISTER_HYBRID
    mister_stat[0] += g_tri_n; for (int k = 0; k < g_lanes; k++) mister_stat[1] += SDL_AtomicGet(&h->at[k]) * 8 / g_lanes;
    mister_stat[2] += SDL_AtomicGet(&h->before) == 1;
#endif
    SDL_AtomicSet(&h->listed, g_tri_n);
    SDL_AtomicSet(&h->closed, 1);
    SDL_SemPost(h->wake);
    while (SDL_AtomicGet(&h->before) == 1) SDL_CPUPauseInstruction();
    while (!lanes_done(h))
        if (!lane_work(h, 1, (g_lanes + 1) / 2)) { SDL_CPUPauseInstruction();
#ifdef MISTER_HYBRID
            mister_stat[3]++;
#endif
        }
    SDL_SemWait(h->done);
    g_tri_n = 0;
}

static void raster_list(const tri_job *j)
{
    if (!g_tri_listing) { raster_job(j, ROWS_ALL, 1); return; }
    if (g_tri_n == TRI_LIST) raster_flush();
    g_tri[g_tri_n++] = *j;
    tri_helper *h = tri_helper_get();
    if (!h) return;
    if (!SDL_AtomicGet(&h->busy)) {                      /* the first of a list: the helper starts */
        h->list = g_tri;
        SDL_AtomicSet(&h->listed, 0);
        SDL_AtomicSet(&h->closed, 0);
        for (int k = 0; k < LANES; k++) SDL_AtomicSet(&h->at[k], 0);
        SDL_AtomicSet(&h->before, 0);
        SDL_AtomicSet(&h->busy, 1);
        SDL_SemPost(h->wake);
    }
    if ((g_tri_n & 31) == 0) {                           /* and is told of more every 32 */
        SDL_AtomicSet(&h->listed, g_tri_n);
        SDL_SemPost(h->wake);
    }
}

static void draw_poly_view(const vtx *in, int n, const material *m, int id);

static void draw_poly(float w[][3], float uv[][2], int n,
                      const material *m, int id)
{
    vtx in[8];
    for (int k = 0; k < n; k++) {
        to_view(w[k], &in[k]);
        in[k].u = uv[k][0];
        in[k].v = uv[k][1];
    }
    draw_poly_view(in, n, m, id);
}

/* the same from corners that are in view space already */
/* Screamer 2: the face's row of the distance table, from the depths of its first three
   corners as they are, behind the eye or not (0xbcf1a, 0xbd5e9; see g_s2_cond) */
static inline void fade_for(const vtx *in, int n)
{
    g_fade_row = NULL;
    if (!g_fade_on || n < 3) return;
    const float range = g_fade_far - g_fade_near;
    float d = fabsf(((in[0].z + in[1].z) * 0.5f + in[2].z) * 0.5f - g_fade_near);
    if (d > range - 1.0f) d = range - 1.0f;
    if (range > 0.0f) g_fade_row = g_dpt + (((int)(d * 16384.0f / range) & 0x3f80) >> 6) * 256;
}

static void draw_poly_view(const vtx *in, int n, const material *m, int id)
{
    vtx out[10];
    int no = 0;
    fade_for(in, n);
    for (int k = 0; k < n; k++) {
        const vtx *p = &in[k], *q = &in[k + 1 < n ? k + 1 : 0];
        int pin = p->z >= NEAR_Z, qin = q->z >= NEAR_Z;
        if (pin) out[no++] = *p;
        if (pin != qin) {
            float t = (NEAR_Z - p->z) / (q->z - p->z);
            vtx *o = &out[no++];
            o->x = p->x + (q->x - p->x) * t;
            o->y = p->y + (q->y - p->y) * t;
            o->z = NEAR_Z;
            o->u = p->u + (q->u - p->u) * t;
            o->v = p->v + (q->v - p->v) * t;
        }
    }
    /* the far plane (0x839af...), when the detail level has one */
    if (g_far_z > 0.0f && no >= 3) {
        vtx cut[12];
        int nc = 0;
        for (int k = 0; k < no; k++) {
            const vtx *p = &out[k], *q = &out[k + 1 < no ? k + 1 : 0];
            int pin = p->z < g_far_z, qin = q->z < g_far_z;
            if (pin) cut[nc++] = *p;
            if (pin != qin) {
                float t = (g_far_z - p->z) / (q->z - p->z);
                vtx *o = &cut[nc++];
                o->x = p->x + (q->x - p->x) * t;
                o->y = p->y + (q->y - p->y) * t;
                o->z = g_far_z;
                o->u = p->u + (q->u - p->u) * t;
                o->v = p->v + (q->v - p->v) * t;
            }
        }
        for (int k = 1; k + 1 < nc; k++)
            raster_tri(&cut[0], &cut[k], &cut[k + 1], m, id);
        return;
    }
    for (int k = 1; k + 1 < no; k++)
        raster_tri(&out[0], &out[k], &out[k + 1], m, id);
}

/* ------------------------------------------------------------------- scene */

/* a mesh corner in world space; local Y is up, local X/Z are on the ground */
/* tilt a local vector: pitch lifts the nose (+Z), roll lifts the +X side */
static vec3 tilt(const scene_object *so, const vec3 *l)
{
    /* (the sines of the last tilt asked for are kept: a car's every corner asks for the same) */
    static float of_pitch, of_roll, cp = 1.0f, sp, cr = 1.0f, sr;
    vec3 r = *l;
    if (so->pitch != 0.0f) {
        if (so->pitch != of_pitch) { of_pitch = so->pitch; cp = cosf(of_pitch); sp = sinf(of_pitch); }
        float y = r.y * cp + r.z * sp, z = r.z * cp - r.y * sp;
        r.y = y; r.z = z;
    }
    if (so->roll != 0.0f) {
        if (so->roll != of_roll) { of_roll = so->roll; cr = cosf(of_roll); sr = sinf(of_roll); }
        float x = r.x * cr - r.y * sr, y = r.y * cr + r.x * sr;
        r.x = x; r.y = y;
    }
    return r;
}

static void place(const scene_object *so, float c, float s, const vec3 *l,
                  float out[3])
{
    vec3 t = tilt(so, l);
    out[0] = so->x + t.x * c - t.z * s;
    out[1] = so->y + t.x * s + t.z * c;
    out[2] = so->z + t.y;
}

/*
 * A face whose corners are in view space AND on the screen already (prj: x, y
 * and 1/depth, worked out once a corner however many faces share it).  One
 * that crosses the near or the far plane goes the long way, to be cut.  Of
 * the rest, those with no pixel's centre in them are dropped here -- at a
 * distance that is half of all there are -- and the others listed as they
 * are, with no more dividing.
 */
static void emit_face(const vtx *in, float prj[][3], int nc, unsigned any, const material *m, int id)
{
    if (any & 3) { draw_poly_view(in, nc, m, id); return; }
    float x0 = prj[0][0], x1 = x0, y0 = prj[0][1], y1 = y0;
    for (int k = 1; k < nc; k++) {
        if (prj[k][0] < x0) x0 = prj[k][0];
        if (prj[k][0] > x1) x1 = prj[k][0];
        if (prj[k][1] < y0) y0 = prj[k][1];
        if (prj[k][1] > y1) y1 = prj[k][1];
    }
    if (!(x1 >= 0 && x0 <= FB_W && y1 >= 0 && y0 <= FB_H)) return;
    if (x1 - x0 < 1.0f && iceil(x0 - 0.5f) > ifloor(x1 - 0.5f)) return;       /* between two columns of centres */
    if (y1 - y0 < 1.0f && iceil(y0 - 0.5f) > ifloor(y1 - 0.5f)) return;       /* between two rows */
    fade_for(in, nc);
    tri_job j;
    j.m = m; j.id = id;
    j.flat = g_flat_colour; j.sprite = g_obj_over; j.zbias = g_obj_zbias; j.fade = g_fade_row; j.ground_vy = g_obj_ground_vy; j.eye_h = g_obj_eye_h;
    j.tex_phase = g_tex_phase; j.env_phase = g_env_phase;
    j.ax = prj[0][0]; j.ay = prj[0][1]; j.ia = prj[0][2]; j.au = in[0].u; j.av = in[0].v;
    for (int k = 1; k + 1 < nc; k++) {
        j.bx = prj[k][0]; j.by = prj[k][1]; j.ib = prj[k][2]; j.bu = in[k].u; j.bv = in[k].v;
        j.cx = prj[k + 1][0]; j.cy = prj[k + 1][1]; j.ic = prj[k + 1][2]; j.cu = in[k + 1].u; j.cv = in[k + 1].v;
        raster_list(&j);
    }
}

static int layer_wanted(const scene_object *so)
{
    if (g_layer == 3) return 1;
    return so->layer == g_layer;
}

/*
 * CARS.  AUTOnn.3DX in the track's container: twelve blocks -- body, nose,
 * tail, four tyres, four hubs, and one untextured block -- each carrying its
 * own position (mesh.c applies it), so the loaded mesh is a whole car standing
 * on y = 0 with its nose towards local -Z (see player_sync).  Their materials are records in the
 * track's own MAPn.MLB, like everything else.
 */
#define MAX_CARS MAX_OBJS       /* cars, and the props and moving scenery too */
static scene_object g_car[MAX_CARS];
static int          g_ncar;
static const scene_object *g_hide;     /* not drawn: the player's car, seen from inside */

static void draw_object(const scene_object *so, int id, int pass,
                        const camera *cam)
{
    const mesh *mm = so->m;
    g_tex_phase = (so->phase < 0) ? (g_anim_clock & 0x3fff) : (so->phase >> 2) & 0x3fff;
    g_obj_zbias = (so->zbias > 0.0f) ? so->zbias : 1.0f;
    g_obj_sprite = so->sprite;
    g_obj_over = so->over;
    g_env_phase = so->pose ? so->pose->env : 0;
    /* the camera's right and up, for the reflections */
    float dx = so->x - cam->x, dy = so->y - cam->y, dz = so->z - cam->z;
    float lim = g_drawdist + so->radius;
    if (dx*dx + dy*dy + dz*dz > lim * lim) return;
    /* nothing of this object in this pass (the see-through faces are few):
       found out once for each object, the first time it is near enough */
    static unsigned char has[4096 + MAX_OBJS + 8];       /* bit 0 known, 1 solid faces, 2 see-through ones */
    const int piece = g_scene && so >= g_scene->obj && so < g_scene->obj + g_scene->count;
    const int sorted = piece && id >= 0 && id < (int)sizeof has && mm;     /* (only the track's pieces: a car's number can go to another car) */
    if (sorted) {
        if (!has[id]) {
            unsigned char h = 1;
            for (int fi = 0; fi < mm->nface; fi++) {
                int mi = mm->material[fi] / 16;
                if (mm->ncorner[fi] < 3 || mi < 0 || mi >= g_nmat) continue;
                if (!g_mat[mi].page && g_mat[mi].type != 128) continue;
                h |= g_mat[mi].type == 224 ? 4 : 2;
            }
            has[id] = h;
        }
        if (!(has[id] & (pass == 1 ? 4 : 2))) {
            if (pass == 1) return;
            /* (pass 0 still notes the object's distance, below) */
        }
    }
    const float crx = g_vs, cry = -g_vc;            /* the camera's right and up: view_setup() has the sines */
    const float cux = -g_vc * g_vsp, cuy = -g_vs * g_vsp, cuz = g_vcp;
    if (id >= 0 && id < (int)(sizeof g_obj_d2 / sizeof g_obj_d2[0])) {
        g_obj_d2[id] = dx*dx + dy*dy + dz*dz;
        g_obj_carpet[id] = (unsigned char)(mm && mm->carpet);
        g_obj_ground[id] = (unsigned char)g_ground_now;
    }
    /* wholly behind the camera? */
    float centre[3] = { so->x, so->y, so->z };
    vtx vc;
    to_view(centre, &vc);
    if (vc.z < -so->radius) return;
    if (g_far_z > 0.0f && vc.z - so->radius > g_far_z) return;      /* all of it beyond the far plane */
    g_obj_ground_vy = vc.y;
    if (so->over) {         /* (the smaller of the camera's height over its own road and over this one's foot) */
        g_obj_eye_h = cam->z - so->z < g_eye_h ? cam->z - so->z : g_eye_h;
        if (g_obj_eye_h < 60.0f) g_obj_eye_h = 60.0f;
    }
    /* ...or all of it off to the side of the picture: the picture's furthest
       corner is 1.61 times as far out as it is deep, whichever way it leans */
    {
        const float out = sqrtf(vc.x * vc.x + vc.y * vc.y) - so->radius, deep = vc.z + so->radius;
        if (out > deep * 1.7f) return;
    }
    if (sorted && pass == 0 && !(has[id] & 2)) return;

    /* the pieces of the track do not turn: their sines are worked out once */
    static const scene *known;
    static float *piece_cos, *piece_sin;
    if (known != g_scene && g_scene) {
        known = g_scene;
        piece_cos = malloc(sizeof(float) * (size_t)(g_scene->count + 1));
        piece_sin = malloc(sizeof(float) * (size_t)(g_scene->count + 1));
        for (int o = 0; o < g_scene->count; o++) { piece_cos[o] = cosf(g_scene->obj[o].yaw); piece_sin[o] = sinf(g_scene->obj[o].yaw); }
    }
    float c = piece ? piece_cos[so - g_scene->obj] : cosf(so->yaw), sn = piece ? piece_sin[so - g_scene->obj] : sinf(so->yaw);
    /*
     * THE TRACK'S PIECES, QUICKLY (2026-10-11, for the MiSTer: a triangle cost
     * four microseconds before it was even filled).  A piece has no wheels,
     * no suspension, no tilt and no reflections, so:
     *  - a face turned away is dropped FIRST, by its normal and the eye's
     *    place in the piece's own frame -- nothing of it is worked out;
     *  - a corner goes from the piece's frame to the view in one step (three
     *    rows of three numbers), once however many faces share it;
     *  - a face wholly behind the eye, beyond the far plane or off one side
     *    of the picture is dropped before it is cut and set up.
     * Faces are drawn in the same order as below, to the same picture.
     */
    int plain_piece = !so->pose && !so->sprite && ((so->pitch == 0.0f && so->roll == 0.0f) || so->mat) && mm->nvert <= MESH_MAX_VERT;
    for (int i = 0; plain_piece && i < mm->nblock && !so->mat; i++) if (mm->block[i].has_vnormal) plain_piece = 0;
    if (plain_piece) {
        static vtx seen[MESH_MAX_VERT];
        static unsigned seen_at[MESH_MAX_VERT], stamp;
        static unsigned char seen_out[MESH_MAX_VERT];    /* which sides of the picture the corner is outside of */
        static float seen_prj[MESH_MAX_VERT][3];         /* where on the screen it is, and 1 / its depth */
        const float half_w = FB_W * 0.5f;
        float ax_[3][3];                                 /* where the piece's x, y (up) and z point, in view space */
        float dirs[3][3] = { { c, sn, 0.0f }, { 0.0f, 0.0f, 1.0f }, { -sn, c, 0.0f } };
        if (so->mat) for (int a = 0; a < 3; a++) for (int b = 0; b < 3; b++) dirs[a][b] = so->mat[a * 3 + b];
        for (int a = 0; a < 3; a++) {
            const float fx = g_vc * dirs[a][0] + g_vs * dirs[a][1];
            const float x = g_vs * dirs[a][0] - g_vc * dirs[a][1];
            const float y = g_vcp * dirs[a][2] - g_vsp * fx;
            ax_[a][0] = x * g_vcr + y * g_vsr;
            ax_[a][1] = y * g_vcr - x * g_vsr;
            ax_[a][2] = g_vsp * dirs[a][2] + g_vcp * fx;
        }
        /* the eye, in the piece's frame (each of its axes' share of the way to the eye) */
        const float ex_ = -(dirs[0][0] * dx + dirs[0][1] * dy + dirs[0][2] * dz),
                    ey_ = -(dirs[1][0] * dx + dirs[1][1] * dy + dirs[1][2] * dz),
                    ez_ = -(dirs[2][0] * dx + dirs[2][1] * dy + dirs[2][2] * dz);
        const float side = FB_W * 0.5f / g_focal_x, top = g_cy / g_focal_y, bottom = ((float)g_vh - g_cy) / g_focal_y;
        const float far_z = g_far_z;
        const int   culling = g_cull;
        stamp++;
        for (int fi = 0; fi < mm->nface; fi++) {
            const int nc = mm->ncorner[fi];
            if (nc < 3) continue;
            const int mi = mm->material[fi] / 16;
            if (mi < 0 || mi >= g_nmat) continue;
            const material *mat = &g_mat[mi];
            g_flat_colour = -1;
            if (!mat->page) {
                if (mat->type != 128) continue;
                g_flat_colour = mm->colour[fi] & 255;
            }
            if ((mat->type == 224) != (pass == 1)) continue;
            const int v0 = mm->idx[fi][0];
            if (v0 < 0) continue;
            if (culling) {
                const vec3 *nr = &mm->normal[fi], *l0 = &mm->vert[v0];
                if ((nr->x != 0.0f || nr->y != 0.0f || nr->z != 0.0f) &&
                    (l0->x - ex_) * nr->x + (l0->y - ey_) * nr->y + (l0->z - ez_) * nr->z < 0.0f) continue;
            }
            vtx in[4];
            float prj[4][3];
            int ok = 1;
            unsigned all = 0x3f, any = 0;                /* bits: 1 behind the eye, 2 beyond the far plane, 4 8 16 32 off a side */
            const float su = (float)mat->su * (1.0f / 256.0f), sv = (float)mat->sv * (1.0f / 256.0f);
            for (int k = 0; k < nc; k++) {
                const int vi = mm->idx[fi][k];
                if (vi < 0) { ok = 0; break; }
                if (seen_at[vi] != stamp) {
                    const vec3 *l = &mm->vert[vi];
                    const float x = vc.x + l->x * ax_[0][0] + l->y * ax_[1][0] + l->z * ax_[2][0];
                    const float y = vc.y + l->x * ax_[0][1] + l->y * ax_[1][1] + l->z * ax_[2][1];
                    const float z = vc.z + l->x * ax_[0][2] + l->y * ax_[1][2] + l->z * ax_[2][2];
                    seen[vi].x = x; seen[vi].y = y; seen[vi].z = z;
                    seen_out[vi] = (unsigned char)((z < NEAR_Z) | (far_z > 0.0f && z >= far_z) << 1 |
                                                   (x > z * side) << 2 | (x < -z * side) << 3 |
                                                   (y > z * top) << 4 | (y < -z * bottom) << 5);
                    if (z >= NEAR_Z) {
                        const float iz = 1.0f / z;
                        seen_prj[vi][0] = half_w + x * g_focal_x * iz;
                        seen_prj[vi][1] = g_cy - y * g_focal_y * iz;
                        seen_prj[vi][2] = iz;
                    }
                    seen_at[vi] = stamp;
                }
                const vtx *v = &seen[vi];
                in[k].x = v->x; in[k].y = v->y; in[k].z = v->z;
                in[k].u = (float)mm->uv[vi][0] * su;
                in[k].v = (float)mm->uv[vi][1] * sv;
                if (mat->type == 160 && so->mat && mm->face_block[fi] >= 0 && mm->block[mm->face_block[fi]].has_vnormal) {
                    /* a reflecting face of an object the engine turns (Screamer 2's menu: the cars):
                       the same sum as below, the corner's normal turned as the object is; no slide */
                    const vec3 *nv = &mm->vnormal[vi];
                    const float nx = nv->x * ax_[0][0] + nv->y * ax_[1][0] + nv->z * ax_[2][0];
                    const float ny = nv->x * ax_[0][1] + nv->y * ax_[1][1] + nv->z * ax_[2][1];
                    const float nz = nv->x * ax_[0][2] + nv->y * ax_[1][2] + nv->z * ax_[2][2];
                    float el = sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
                    if (el < 1.0f) el = 1.0f;
                    const float ex = v->x / el, ey = v->y / el, ez = v->z / el;
                    const float d2 = 2.0f * (ex * nx + ey * ny + ez * nz);
                    const float rx = d2 * nx - ex, ry = d2 * ny - ey;
                    const float rk = g_env_mat >= 0 ? 16.0f : 1.0f;
                    in[k].u = (127.0f + 127.0f * (rx * g_vcr - ry * g_vsr)) * su * rk;
                    in[k].v = (127.0f + 127.0f * (ry * g_vcr + rx * g_vsr)) * sv * rk;
                }
                prj[k][0] = seen_prj[vi][0]; prj[k][1] = seen_prj[vi][1]; prj[k][2] = seen_prj[vi][2];
                all &= seen_out[vi]; any |= seen_out[vi];
            }
            /* every corner behind the eye, or beyond the far plane; or none behind and all off one side */
            if (!ok || (all & 3) || (!(any & 1) && (all & 0x3c))) { g_flat_colour = -1; continue; }
            emit_face(in, prj, nc, any, mat, id);
            g_flat_colour = -1;
        }
        return;
    }

    /*
     * WHEELS THE BODYWORK COVERS.  On AUTO06 (the Bullet) the body comes down
     * over the rear wheels, and its two rear tyre blocks carry flag 0x200 --
     * the bit every body block has and no other tyre does (0x201 against 0x1).
     * Those wheels, tyre and hub, get no depth nudge: they are behind the body
     * and must stay there (Danik saw them through it).
     */
    unsigned covered = 0;
    if (so->pose)
        for (int i = 0; i < mm->nblock; i++) {
            if (mm->block[i].ox == 0.0f || !(mm->block[i].flags & 0x200)) continue;
            for (int j = 0; j < mm->nblock; j++)
                if (mm->block[j].ox * mm->block[i].ox > 0.0f &&
                    fabsf(mm->block[j].oz - mm->block[i].oz) < 8.0f) covered |= 1u << j;
        }
    static vtx q_seen[MESH_MAX_VERT];                 /* the quick way's corners in view space, and each block's turn and shift */
    static unsigned q_seen_at[MESH_MAX_VERT], q_block_at[32], q_stamp;
    static unsigned char q_out[MESH_MAX_VERT];
    static float q_prj[MESH_MAX_VERT][3];
    static float q_ref[MESH_MAX_VERT][2];             /* a reflecting corner's place in the reflected picture */
    static unsigned q_ref_at[MESH_MAX_VERT];
    static float q_m[32][4][3], q_eye[32][3];
    const float q_side = FB_W * 0.5f / g_focal_x, q_top = g_cy / g_focal_y, q_bottom = ((float)g_vh - g_cy) / g_focal_y;
    q_stamp++;
    /* a car's sines, once for all its faces */
    float pose_bpc = 1.0f, pose_bps = 0.0f, pose_brc = 1.0f, pose_brs = 0.0f;
    float pose_wcs = 1.0f, pose_wsn = 0.0f, pose_scs = 1.0f, pose_ssn = 0.0f;
    if (so->pose) {
        pose_bpc = cosf(so->pose->body_pitch); pose_bps = sinf(so->pose->body_pitch);
        pose_brc = cosf(so->pose->body_roll);  pose_brs = sinf(so->pose->body_roll);
        pose_wcs = cosf(so->pose->spin);       pose_wsn = sinf(so->pose->spin);
        pose_scs = cosf(so->pose->steer);      pose_ssn = sinf(so->pose->steer);
    }
    for (int fi = 0; fi < mm->nface; fi++) {
        int nc = mm->ncorner[fi];
        if (nc < 3) continue;
        int mi = mm->material[fi] / 16;
        if (mi < 0 || mi >= g_nmat) continue;
        const material *mat = &g_mat[mi];
        /*
         * ROUTINE 128 IS A FLAT COLOUR, not a blank (0xd8844): the face is
         * filled with the palette index in its own record, +0x02.  The cars'
         * TYRES are made of it -- so is the underside -- and until this was
         * drawn a wheel was only its hub disc, a cardboard circle (Danik).
         */
        g_flat_colour = -1;
        if (!mat->page) {
            if (mat->type != 128) continue;
            g_flat_colour = mm->colour[fi] & 255;
        }
        if ((mat->type == 224) != (pass == 1)) continue;

        /* a car's wheel block: turned about its own axle, the front ones
           steered as well (0x15c0c writes these angles into the block records) */
        const struct mesh_block *blk = &mm->block[mm->face_block[fi]];
        const int role = (so->pose && so->pose->per_wheel && mm->face_block[fi] < 16) ? so->pose->role[mm->face_block[fi]] : -1;
        const int wheel = role >= 0 ? (role & 0x30) != 0 : (so->pose && blk->ox != 0.0f);
        const int body = (so->pose && !wheel && (role < 0 || role == 1) &&
                          (so->pose->body_pitch != 0.0f || so->pose->body_roll != 0.0f ||
                           so->pose->body_drop != 0.0f));
        const float bpc = body ? pose_bpc : 1.0f, bps = body ? pose_bps : 0.0f;
        const float brc = body ? pose_brc : 1.0f, brs = body ? pose_brs : 0.0f;
        /*
         * The game has no depth buffer: it draws an object block by block in
         * a fixed order, so a wheel is simply painted over the bodywork it
         * sits in.  Here the body's wheel arch can lie a few units nearer
         * than the wheel and cut pieces out of it (Danik: "the car body draws
         * over them"), so the wheel blocks are counted a little nearer -- the
         * hub disc more than the tyre it sits on.  The far side's wheels are
         * hundreds of units further off and stay hidden.
         */
        g_obj_zbias = (so->zbias > 0.0f) ? so->zbias : 1.0f;
        if (wheel && g_wheel_bias && !(covered & (1u << mm->face_block[fi])))
            g_obj_zbias = (blk->flags & 0x800) ? 1.045f : 1.03f;
        /* ...and the other way round for the black panels inside the wheel
           arches (a car's flat-coloured block that is not a wheel): the game
           paints them first and the bodywork over them; they lie within a few
           units of the body's side and must lose to it */
        if (so->pose && !wheel && mat->type == 128 && g_wheel_bias) g_obj_zbias = 0.96f;
        float wcs = 1.0f, wsn = 0.0f, scs = 1.0f, ssn = 0.0f, wup = 0.0f;
        if (wheel && so->pose->per_wheel) {
            /* SCREAMER 2 (0x16878): every wheel has its own turning and its own spring */
            const int w = role & 3;
            if (role & 0x10) { wcs = cosf(so->pose->wheel_spin[w]); wsn = sinf(so->pose->wheel_spin[w]); }
            if (w == 0 || w == 2) { scs = pose_scs; ssn = pose_ssn; }      /* (0x16878: wheels 0 and 2 steer) */
            wup = -so->pose->wheel_in[w];
        }
        else if (wheel) {
            /* Only the HUB DISC turns (block flags 0x800).  The tyre block is
               not a whole tyre: it is the lower half of the tread and the
               inner wall, the part that shows under the bodywork -- turn it
               and it swings up inside the body and the wheel is left a flat
               disc with nothing behind it (Danik saw exactly that). */
            if (blk->flags & 0x800) { wcs = pose_wcs; wsn = pose_wsn; }
            if (blk->oz < 0.0f) { scs = pose_scs; ssn = pose_ssn; }
        }

        /*
         * QUICKLY, for every face but a reflecting one: all that happens to a
         * corner on its way to the view -- the wheel's turn, the body's lean,
         * the object's tilt, heading and place, the camera -- is one turn and
         * one shift for a whole block, found by sending four points through
         * it; a corner is then nine multiplications, once however many faces
         * share it, and the face is tested as the track's pieces' are.
         */
        const int bi = mm->face_block[fi];
        const int mirror = (mat->type == 160 && blk->has_vnormal);
        if (!so->sprite && bi >= 0 && bi < 32) {
            if (q_block_at[bi] != q_stamp) {
                static const float unit[4][3] = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
                float img[4][3];
                for (int a = 0; a < 4; a++) {
                    vec3 lv = { unit[a][0], unit[a][1], unit[a][2] };
                    if (wheel) {
                        float x = lv.x - blk->ox, y = lv.y - blk->oy, z = lv.z - blk->oz;
                        float y2 = y * wcs - z * wsn, z2 = z * wcs + y * wsn;
                        lv.x = blk->ox + x * scs + z2 * ssn;
                        lv.y = blk->oy + y2 + wup;
                        lv.z = blk->oz + z2 * scs - x * ssn;
                    }
                    if (body) {
                        float y = lv.y * bpc - lv.z * bps, z = lv.z * bpc + lv.y * bps;
                        float x = lv.x * brc - y * brs;
                        lv.y = y * brc + lv.x * brs - so->pose->body_drop;
                        lv.x = x; lv.z = z;
                    }
                    float wp[3];
                    vtx o;
                    place(so, c, sn, &lv, wp);
                    to_view(wp, &o);
                    img[a][0] = o.x; img[a][1] = o.y; img[a][2] = o.z;
                }
                for (int a = 0; a < 3; a++) {
                    q_m[bi][a][0] = img[a + 1][0] - img[0][0];
                    q_m[bi][a][1] = img[a + 1][1] - img[0][1];
                    q_m[bi][a][2] = img[a + 1][2] - img[0][2];
                    q_m[bi][3][a] = img[0][a];
                }
                /* the eye's place in the block's own frame (the turn undone: its rows are at right angles) */
                for (int a = 0; a < 3; a++)
                    q_eye[bi][a] = -(q_m[bi][a][0] * q_m[bi][3][0] + q_m[bi][a][1] * q_m[bi][3][1] + q_m[bi][a][2] * q_m[bi][3][2]);
                q_block_at[bi] = q_stamp;
            }
            const float (*M)[3] = q_m[bi];
            if (g_cull) {
                /* turned away: dropped before anything of it is worked out (half of a car's faces) */
                const vec3 *nr = &mm->normal[fi];
                const int v0 = mm->idx[fi][0];
                if (v0 >= 0 && v0 < MESH_MAX_VERT && (nr->x != 0.0f || nr->y != 0.0f || nr->z != 0.0f)) {
                    const vec3 *l0 = &mm->vert[v0];
                    if ((l0->x - q_eye[bi][0]) * nr->x + (l0->y - q_eye[bi][1]) * nr->y + (l0->z - q_eye[bi][2]) * nr->z < 0.0f) {
                        g_flat_colour = -1;
                        continue;
                    }
                }
            }
            vtx in[4];
            float prj[4][3];
            int ok = 1;
            unsigned all = 0x3f, any = 0;
            const float su = (float)mat->su * (1.0f / 256.0f), sv = (float)mat->sv * (1.0f / 256.0f);
            const float rk = (mirror && g_env_mat >= 0) ? 16.0f : 1.0f;     /* (Screamer 2's reflections: see the long way below) */
            for (int k = 0; k < nc; k++) {
                const int vi = mm->idx[fi][k];
                if (vi < 0 || vi >= MESH_MAX_VERT) { ok = 0; break; }
                if (q_seen_at[vi] != q_stamp) {
                    const vec3 *l = &mm->vert[vi];
                    const float x = M[3][0] + l->x * M[0][0] + l->y * M[1][0] + l->z * M[2][0];
                    const float y = M[3][1] + l->x * M[0][1] + l->y * M[1][1] + l->z * M[2][1];
                    const float z = M[3][2] + l->x * M[0][2] + l->y * M[1][2] + l->z * M[2][2];
                    q_seen[vi].x = x; q_seen[vi].y = y; q_seen[vi].z = z;
                    q_out[vi] = (unsigned char)((z < NEAR_Z) | (g_far_z > 0.0f && z >= g_far_z) << 1 |
                                                (x > z * q_side) << 2 | (x < -z * q_side) << 3 |
                                                (y > z * q_top) << 4 | (y < -z * q_bottom) << 5);
                    if (z >= NEAR_Z) {
                        const float iz = 1.0f / z;
                        q_prj[vi][0] = FB_W * 0.5f + x * g_focal_x * iz;
                        q_prj[vi][1] = g_cy - y * g_focal_y * iz;
                        q_prj[vi][2] = iz;
                    }
                    q_seen_at[vi] = q_stamp;
                }
                const vtx *v = &q_seen[vi];
                in[k].x = v->x; in[k].y = v->y; in[k].z = v->z;
                in[k].u = (float)mm->uv[vi][0] * su;
                in[k].v = (float)mm->uv[vi][1] * sv;
                prj[k][0] = q_prj[vi][0]; prj[k][1] = q_prj[vi][1]; prj[k][2] = q_prj[vi][2];
                all &= q_out[vi]; any |= q_out[vi];
                if (mirror) {
                    /* A reflecting face (routine 160, see the long way below):
                       the same sum in view space, where the eye is at the
                       origin -- the corner's normal is turned as its block
                       is, and the reflected line read along the camera's
                       right and up before its lean.  Once a corner. */
                    if (q_ref_at[vi] != q_stamp) {
                        const vec3 *nv = &mm->vnormal[vi];
                        const float nx = nv->x * M[0][0] + nv->y * M[1][0] + nv->z * M[2][0];
                        const float ny = nv->x * M[0][1] + nv->y * M[1][1] + nv->z * M[2][1];
                        const float nz = nv->x * M[0][2] + nv->y * M[1][2] + nv->z * M[2][2];
                        float el = sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
                        if (el < 1.0f) el = 1.0f;
                        const float ex = v->x / el, ey = v->y / el, ez = v->z / el;
                        const float d2 = 2.0f * (ex * nx + ey * ny + ez * nz);
                        const float rx = d2 * nx - ex, ry = d2 * ny - ey;
                        q_ref[vi][0] = rx * g_vcr - ry * g_vsr;
                        q_ref[vi][1] = ry * g_vcr + rx * g_vsr;
                        q_ref_at[vi] = q_stamp;
                    }
                    in[k].u = (127.0f + 127.0f * q_ref[vi][0] - (so->pose ? so->pose->env_u : 0.0f)) * su * rk;
                    in[k].v = (127.0f + 127.0f * q_ref[vi][1] - (so->pose ? so->pose->env_scroll : 0.0f)) * sv * rk;
                }
            }
            if (!ok || (all & 3) || (!(any & 1) && (all & 0x3c))) { g_flat_colour = -1; continue; }
            emit_face(in, prj, nc, any, mat, id);
            g_flat_colour = -1;
            continue;
        }

        float p[4][3], uv[4][2];
        int ok = 1;
        for (int k = 0; k < nc; k++) {
            int vi = mm->idx[fi][k];
            if (vi < 0) { ok = 0; break; }
            vec3 lv = mm->vert[vi];
            if (wheel) {
                float x = lv.x - blk->ox, y = lv.y - blk->oy, z = lv.z - blk->oz;
                float y2 = y * wcs - z * wsn, z2 = z * wcs + y * wsn;
                lv.x = blk->ox + x * scs + z2 * ssn;
                lv.y = blk->oy + y2 + wup;
                lv.z = blk->oz + z2 * scs - x * ssn;
            }
            if (body) {
                float y = lv.y * bpc - lv.z * bps, z = lv.z * bpc + lv.y * bps;
                float x = lv.x * brc - y * brs;
                lv.y = y * brc + lv.x * brs - so->pose->body_drop;
                lv.x = x; lv.z = z;
            }
            if (so->sprite) {
                /* flat to the screen: the model's x along the picture's
                   right, its y along the picture's up */
                const float rx = g_vs, ry = -g_vc, ux = -g_vsp * g_vc, uy = -g_vsp * g_vs, uz = g_vcp;
                const float sx = g_vcr * rx + g_vsr * ux, sy = g_vcr * ry + g_vsr * uy, sz = g_vsr * uz;
                const float tx = -g_vsr * rx + g_vcr * ux, ty = -g_vsr * ry + g_vcr * uy, tz = g_vcr * uz;
                p[k][0] = so->x + lv.x * sx + lv.y * tx;
                p[k][1] = so->y + lv.x * sy + lv.y * ty;
                p[k][2] = so->z + lv.x * sz + lv.y * tz;
            } else
            place(so, c, sn, &lv, p[k]);
            uv[k][0] = (float)mm->uv[vi][0] * (float)mat->su / 256.0f;
            uv[k][1] = (float)mm->uv[vi][1] * (float)mat->sv / 256.0f;
            if (mat->type == 160 && blk->has_vnormal) {
                /*
                 * ROUTINE 160 IS A REFLECTION (0xc2230 makes its coordinates,
                 * 0xdb310 uses them in place of the file's).  For each corner:
                 * e = the direction from the eye to it, n = its own normal,
                 * r = 2 (e . n) n - e, and the texture is read at
                 *     127 + 127 x (r to the right, r upwards)  -  how far the car has gone
                 * so the picture slides over the bodywork as the car turns
                 * and as it moves (0.002 texels a unit, 0.03 in a tunnel).
                 */
                vec3 nv = mm->vnormal[vi];
                if (body) {
                    float y = nv.y * bpc - nv.z * bps, z = nv.z * bpc + nv.y * bps;
                    float x = nv.x * brc - y * brs;
                    nv.y = y * brc + nv.x * brs; nv.x = x; nv.z = z;
                }
                vec3 nt = tilt(so, &nv);
                float nx = nt.x * c - nt.z * sn, ny = nt.x * sn + nt.z * c, nz = nt.y;
                float ex = p[k][0] - cam->x, ey = p[k][1] - cam->y, ez = p[k][2] - cam->z;
                float el = sqrtf(ex * ex + ey * ey + ez * ez);
                if (el < 1.0f) el = 1.0f;
                ex /= el; ey /= el; ez /= el;
                float d2 = 2.0f * (ex * nx + ey * ny + ez * nz);
                float rx = d2 * nx - ex, ry = d2 * ny - ey, rz = d2 * nz - ez;
                float ru = rx * crx + ry * cry, rv = rx * cux + ry * cuy + rz * cuz;
                /* SCREAMER 2 (g_env_mat): the reflections' map wraps at 128 (0xcd5a1 masks the texel's
                   address with 0x7f7f) and a face's size reads 16 times as far: a car's glass is 8 x 8,
                   so the whole of r's range spans the map's 128 once -- as the original's windscreens
                   show the map's streaks from side to side (DOSBox, the arcade page) */
                const float rk = g_env_mat >= 0 ? 16.0f : 1.0f;
                uv[k][0] = (127.0f + 127.0f * ru - (so->pose ? so->pose->env_u : 0.0f)) * (float)mat->su * rk / 256.0f;
                uv[k][1] = (127.0f + 127.0f * rv - (so->pose ? so->pose->env_scroll : 0.0f)) * (float)mat->sv * rk / 256.0f;
            }
        }
        if (!ok) continue;

        /*
         * Back-face culling with the face's STORED normal, the way the
         * engine does it (FUN_00083468): a face is drawn when
         * (corner - eye) . normal >= 0, i.e. the stored normal points
         * AWAY from the side you can see -- a road tile's reads
         * straight down.
         */
        if (g_cull && !so->sprite) {
            vec3 raw = mm->normal[fi];
            if (wheel) {
                float y2 = raw.y * wcs - raw.z * wsn, z2 = raw.z * wcs + raw.y * wsn;
                float x2 = raw.x * scs + z2 * ssn;
                raw.z = z2 * scs - raw.x * ssn; raw.x = x2; raw.y = y2;
            }
            const vec3 tl = tilt(so, &raw), *nl = &tl;
            if (raw.x != 0.0f || raw.y != 0.0f || raw.z != 0.0f) {
                float nx = nl->x * c - nl->z * sn;
                float ny = nl->x * sn + nl->z * c;
                float nz = nl->y;
                float d = (p[0][0] - cam->x) * nx + (p[0][1] - cam->y) * ny
                        + (p[0][2] - cam->z) * nz;
                if (d < 0.0f) continue;
            }
        }
        draw_poly(p, uv, nc, mat, id);
        g_flat_colour = -1;
    }
}

/* debugging overlay: the walls the car is held between (G key, OS_SHOWEDGES) */
static int g_show_edges;
static int g_guide_n;
static float (*g_guide)[3];           /* points along the game's wall segments */

static void draw_guides(void)
{
    for (int i = 0; i < g_guide_n; i++) {
        vtx v;
        to_view(g_guide[i], &v);
        if (v.z < NEAR_Z) continue;
        int x = (int)(FB_W * 0.5f + v.x * g_focal_x / v.z);
        int y = (int)(g_cy - v.y * g_focal_y / v.z);
        if (x < 1 || y < 1 || x >= FB_W - 1 || y >= FB_H - 1) continue;
        uint32_t col = 0xFFFFFF00u;
        fb[y * FB_W + x] = col;
        fb[y * FB_W + x + 1] = col;
        fb[(y + 1) * FB_W + x] = col;
        fb[(y + 1) * FB_W + x + 1] = col;
    }
}

static int track_under(float x, float y, int want_height, float *out);
static void render_scene(const camera *cam, const sky *s)
{
    {   /* the camera's height over the road (for the smoke: over_ground) */
        float h;
        g_eye_h = track_under(cam->x, cam->y, 1, &h) ? cam->z - h : 400.0f;
        if (g_eye_h < 60.0f) g_eye_h = 60.0f;
    }
    g_wheel_bias = !getenv("OS_NOWHEELBIAS");
    g_tie_rule = !getenv("OS_NOTIE");
    if (getenv("OS_ANIMCLOCK"))            /* 0..1: fix the animation clock (for --shot) */
    {
        g_anim_clock = (int)(atof(getenv("OS_ANIMCLOCK")) * 65535.0);
        mesh_animate_all(g_anim_clock);
    }
    MISTER_MARK(0);
    {
        const int want = getenv("OS_LANES") ? atoi(getenv("OS_LANES")) : 4;
        g_lanes = (want == 2 || want == 3 || want == 8) ? want : 4;
        tri_helper *h = tri_helper_get();
        if (h && !SDL_AtomicGet(&h->busy)) {
            sky_ready(s);
            h->yaw = cam->yaw; h->pitch = cam->pitch; h->roll = cam->roll;
            h->focal_x = g_focal_x; h->focal_y = g_focal_y; h->backdrop = s;
            h->list = g_tri;
            SDL_AtomicSet(&h->listed, 0);
            SDL_AtomicSet(&h->closed, 0);
            for (int k = 0; k < LANES; k++) SDL_AtomicSet(&h->at[k], 0);
            SDL_AtomicSet(&h->before, 1);
            SDL_AtomicSet(&h->busy, 1);
            SDL_SemPost(h->wake);
        } else
            frame_clear(cam->yaw, cam->pitch, cam->roll, s, g_focal_x, g_focal_y);
    }
    MISTER_MARK(1);
    view_setup(cam);
    if (!g_scene) { raster_flush(); return; }
    g_tri_listing = 1;

    /* pass 0: everything solid; pass 1: the see-through materials on top */
    for (int pass = 0; pass < 2; pass++) {
        for (int o = 0; o < g_scene->count; o++) {
            const scene_object *so = &g_scene->obj[o];
            g_ground_now = (so->layer == 0);
            if (so->m && layer_wanted(so)) draw_object(so, o, pass, cam);
            g_ground_now = 0;
        }
        for (int k = 0; k < g_ncar; k++)
            if (&g_car[k] != g_hide)
                draw_object(&g_car[k], g_scene->count + k, pass, cam);
    }
    MISTER_MARK(2);
    raster_flush();
    g_tri_listing = 0;
    if (g_show_edges) draw_guides();
    MISTER_MARK(5);
}

static void s2_env_slide(const camera *cam);
static void render_frame(const camera *cam, const sky *s)
{
    g_tiemark = getenv("OS_TIEMARK") != NULL;           /* a test: pixels won on a tie of depth in magenta */
    if (os_game() == 2) { s2_dark_make(); s2_env_slide(cam); }
#ifdef MISTER_HYBRID
    /*
     * MISTER: HOW FAR IS DRAWN FOLLOWS THE TIME THERE IS.  A picture has one
     * field, 16.7 ms; one that takes longer is shown a whole field late.  So
     * when the last picture took nearly all of it the far plane (the game's
     * DETAIL setting, section 30 of CLAUDE.md) comes a little nearer, and
     * when there is time to spare it goes back out -- never nearer than the
     * game's own MID and LOW, 27808, and never further than what is set.
     * OFF since 2026-10-12 (Danik: things at the far plane came and went
     * while the speed was on the edge); OS_ADAPTFAR=1 tries it.
     */
    static float reach = 1.0f;
    const float set = g_far_z;
    if (set > 27808.0f && getenv("OS_ADAPTFAR")) {
        const float took = mister_last_work();
        if (took > 14.6f) reach -= 0.02f;
        else if (took > 0.0f && took < 13.4f) reach += 0.005f;
        if (reach > 1.0f) reach = 1.0f;
        if (reach * set < 27808.0f) reach = 27808.0f / set;
        g_far_z = set * reach;
    }
    render_scene(cam, s);
    g_far_z = set;
#else
    render_scene(cam, s);
#endif
}

/*
 * The height of the track surface under a world point.  Two ways to choose
 * when surfaces are stacked (a bridge over the road, the tunnel roof):
 *
 *   below == 0   the one whose height is closest to `ref` -- used to lay the
 *                camera path on the road it belongs to;
 *   below == 1   the HIGHEST one that is not above `ref` -- "what is under my
 *                feet", used for walking.
 *
 * Returns 0 if there is nothing there.
 */
static int g_pick_mat;      /* material record of the surface last picked */
static int g_pick_obj;      /* scene object that surface belongs to */


static int surface_pick(float x, float y, float ref, int below, float *out)
{
    int found = 0;
    float best = 0.0f;
    if (!g_scene) return 0;
    for (int o = 0; o < g_scene->count; o++) {
        const scene_object *so = &g_scene->obj[o];
        const mesh *mm = so->m;
        if (!mm) continue;
        float dx = so->x - x, dy = so->y - y;
        if (dx*dx + dy*dy > so->radius * so->radius) continue;
        float c = cosf(so->yaw), sn = sinf(so->yaw);
        for (int fi = 0; fi < mm->nface; fi++) {
            if (fabsf(mm->normal[fi].y) < 0.5f) continue;   /* not a floor */
            int nc = mm->ncorner[fi];
            float p[4][3];
            int ok = 1;
            for (int k = 0; k < nc; k++) {
                int vi = mm->idx[fi][k];
                if (vi < 0) { ok = 0; break; }
                place(so, c, sn, &mm->vert[vi], p[k]);
            }
            if (!ok) continue;
            /* inside the polygon, seen from above? */
            int pos = 0, neg = 0;
            for (int k = 0; k < nc; k++) {
                const float *a = p[k], *b = p[(k + 1) % nc];
                float cr = (b[0] - a[0]) * (y - a[1]) - (b[1] - a[1]) * (x - a[0]);
                if (cr > 0) pos++; else if (cr < 0) neg++;
            }
            if (pos && neg) continue;
            /* height on the plane through the first three corners */
            float ux = p[1][0]-p[0][0], uy = p[1][1]-p[0][1], uz = p[1][2]-p[0][2];
            float vx = p[2][0]-p[0][0], vy = p[2][1]-p[0][1], vz = p[2][2]-p[0][2];
            float nx = uy*vz - uz*vy, ny = uz*vx - ux*vz, nz = ux*vy - uy*vx;
            if (fabsf(nz) < 1e-3f) continue;
            float z = p[0][2] - (nx * (x - p[0][0]) + ny * (y - p[0][1])) / nz;
            if (below) {
                if (z > ref) continue;
                if (!found || z > best) {
                    best = z; found = 1; g_pick_mat = mm->material[fi] / 16; g_pick_obj = o;
                }
            } else if (!found || fabsf(z - ref) < fabsf(best - ref)) {
                best = z; found = 1; g_pick_mat = mm->material[fi] / 16; g_pick_obj = o;
            }
        }
    }
    if (found) *out = best;
    return found;
}

static int surface_z(float x, float y, float ref, float *out)
{
    return surface_pick(x, y, ref, 0, out);
}

/* lay the camera path onto the track's own surface */
static void path_drape(path *r)
{
    float ref = 0.0f;
    int hits = 0;
    /* start from the height of the piece nearest the first sample */
    if (g_scene) {
        float best = 1e30f;
        for (int o = 0; o < g_scene->count; o++) {
            const scene_object *so = &g_scene->obj[o];
            if (!so->m || so->layer != 0) continue;
            float dx = so->x - r->p[0][0], dy = so->y - r->p[0][1];
            if (dx*dx + dy*dy < best) { best = dx*dx + dy*dy; ref = so->z; }
        }
    }
    for (int i = 0; i < r->count; i++) {
        float z;
        if (surface_z(r->p[i][0], r->p[i][1], ref, &z)) { ref = z; hits++; }
        else if (getenv("OS_PATHLOG")) printf("path: sample %d (%.0f, %.0f) is over no track surface\n", i, r->p[i][0], r->p[i][1]);
        r->p[i][2] = ref;
    }
    /* the tiles meet at an angle where a slope starts; round that off so the
       camera does not jerk (the ROAD is not smoothed, only the camera) */
    for (int pass = 0; pass < 3; pass++) {
        float prev = r->p[r->count - 1][2], first = r->p[0][2];
        for (int k = 0; k < r->count; k++) {
            float cur = r->p[k][2];
            float next = (k + 1 < r->count) ? r->p[k + 1][2] : first;
            r->p[k][2] = 0.25f * prev + 0.5f * cur + 0.25f * next;
            prev = cur;
        }
    }
    printf("camera path: %d samples, %d of them over a track surface\n",
           r->count, hits);
}

/*
 * WHAT AM I LOOKING AT?  The renderer records which piece owns every pixel,
 * so this is exact: the piece under the centre of the screen.
 */
static void describe_under_crosshair(void)
{
    /* OS_INSPECT=x,y looks at that pixel of the picture instead of its middle */
    int px = FB_W / 2, py = (int)g_cy;
    if (getenv("OS_INSPECT") && sscanf(getenv("OS_INSPECT"), "%d,%d", &px, &py) == 2) {
        if (px < 0) px = 0;
        if (px >= FB_W) px = FB_W - 1;
        if (py < 0) py = 0;
        if (py >= FB_H) py = FB_H - 1;
    } else { px = FB_W / 2; py = (int)g_cy; }
    int id = idb[py * FB_W + px];
    if (id < 0 || !g_scene) {
        printf("nothing under the crosshair (sky)\n");
        fflush(stdout);
        return;
    }
    const scene_object *so = (id < g_scene->count) ? &g_scene->obj[id]
                                                   : &g_car[id - g_scene->count];
    const mesh *mm = so->m;
    if (id >= g_scene->count)
        printf("\n(this is car model AUTO%02d)", so->v);
    printf("\n--- under the crosshair ---\n"
           "mesh V%d   record run %d   pos (%.0f, %.0f)   height %.0f   "
           "heading %d/2048 (%.0f deg)   %d faces\n",
           so->v, so->layer, so->x, so->y, so->z, so->t,
           so->t * 360.0f / 2048.0f, mm->nface);
    int seen[MAT_MAX] = { 0 };
    for (int f = 0; f < mm->nface; f++) {
        int mi = mm->material[f] / 16;
        if (mi < 0 || mi >= MAT_MAX || seen[mi]) continue;
        seen[mi] = 1;
        printf("  material %d: routine %d (%s), rectangle %dx%d texels\n",
               mi, g_mat[mi].type,
               g_mat[mi].type == 16 ? "solid" :
               g_mat[mi].type == 64 ? "cut-out" :
               g_mat[mi].type == 224 ? "see-through" : "other",
               g_mat[mi].su, g_mat[mi].sv);
    }
    fflush(stdout);
}

static void write_ppm(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); return; }
    fprintf(f, "P6\n%d %d\n255\n", FB_W, FB_H);
    for (int i = 0; i < FB_W * FB_H; i++) {
        uint32_t c = fb[i];
        unsigned char rgb[3] = { (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

/* the camera's place on the path at parameter t (in samples) */
static void path_at(const path *r, float t, float cam_height, camera *cam)
{
    int i0 = ((int)floorf(t) % r->count + r->count) % r->count;
    int i1 = (i0 + 1) % r->count;
    int i2 = (i0 + 3) % r->count;
    float f = t - floorf(t);
    cam->x = r->p[i0][0] + (r->p[i1][0] - r->p[i0][0]) * f;
    cam->y = r->p[i0][1] + (r->p[i1][1] - r->p[i0][1]) * f;
    cam->z = r->p[i0][2] + (r->p[i1][2] - r->p[i0][2]) * f + cam_height;
    cam->yaw = atan2f(r->p[i2][1] - r->p[i0][1], r->p[i2][0] - r->p[i0][0]);
}

/*
 * How far the road reaches to the left and to the right of a point on it.
 *
 * The road is a chain of pieces exactly one road wide (seen from above, the
 * record-run-0 tiles of track 1 are a single file of them all the way round),
 * and each is turned to follow the road.  So the road's edges at a point are
 * simply the edges of the PIECE under it, measured across the direction of
 * travel -- no material numbers, no guessing where tarmac ends.  Only the
 * piece's corners near the road's own height count, so a barrier moulded
 * into the piece does not widen it.  `side` is the unit vector to the RIGHT.
 * Returns 0 if the point is not on a surface at all.
 */
/*
 * Is (x, y) over one of this piece's faces, seen from above?  EVERY face, as
 * the game does it (0x1c09c walks the block's whole face list, quads through
 * 0x1c12c and triangles through 0x1c37a).  An earlier version counted only
 * near-level "floor" faces; that left the sloping kerb at the road's edge out
 * of the track, so the drivable area ended a kerb's width short of the wall
 * line -- an invisible wall just inside the real one, and a car that reached
 * it was dragged to the wall line and held there (Danik, track 4).
 */
static int piece_covers(const scene_object *so, float x, float y)
{
    const mesh *mm = so->m;
    float c = cosf(so->yaw), sn = sinf(so->yaw);
    for (int f = 0; f < mm->nface; f++) {
        int nc = mm->ncorner[f], pos = 0, neg = 0, ok = 1;
        float p[4][3];
        for (int k = 0; k < nc; k++) {
            int vi = mm->idx[f][k];
            if (vi < 0) { ok = 0; break; }
            place(so, c, sn, &mm->vert[vi], p[k]);
        }
        if (!ok) continue;
        for (int k = 0; k < nc; k++) {
            const float *a = p[k], *b = p[(k + 1) % nc];
            float cr = (b[0] - a[0]) * (y - a[1]) - (b[1] - a[1]) * (x - a[0]);
            if (cr > 0) pos++; else if (cr < 0) neg++;
        }
        if (!(pos && neg)) return 1;
    }
    return 0;
}

/*
 * WHAT IS UNDER THIS POINT -- the game's own search (0x11c00 -> 0x11ccc ->
 * 0x1c09c / 0x1bfc4), in the game's own order, because it stops at the FIRST
 * answer:
 *
 *   - the nine grid cells round the point, in the order of the table at
 *     0x1cec: its own, +x, -x, +z, -z, then the four corners;
 *   - in a cell, the pieces of RECORD RUN 0 in the order they are stored
 *     (run 0 is everything you can drive on, run 1 everything else);
 *   - a piece is looked at only if the point is inside its first block's
 *     bounding circle (the circle's centre is NOT turned with the piece --
 *     the game's own shortcut);
 *   - the point is turned into the piece's frame and tried three times: as
 *     it is, then 10 and 20 units along both axes, so a seam between pieces
 *     does not count as a gap;
 *   - the faces of the piece's first block in the order of ITS list, quads
 *     and triangles alike (the sloping kerb is track too).
 *
 * want_height == 0 is "am I on the track at all" (0x1c09c): any face will do.
 * Otherwise (0x1bfc4) the face must not be vertical, and the height is the
 * face's plane through one of its corners, worked out in whole numbers with
 * the face's stored normal:  corner height - (dx * Nx + dz * Nz) / Ny.
 *
 * Until 2026-10-11 the port took, of every drivable face over the point, the
 * one nearest the car's present height.  The game does no such thing.
 */
static int g_under_obj;        /* the piece found */
/* a point that falls between two faces is tried again a little further on: three times,
   10 apart, in Screamer (0x11ccc); five times, 4 apart, in Screamer 2 (0x28db7) */
static int   g_under_tries = 3;
static float g_under_step = 10.0f;
static float g_under_n[3];     /* its face's normal in world axes (x, y, up), as stored: not unit, either sign */

/* the drivable pieces of each cell, in stored order, found once (the search used to go
   through every piece of the track nine times) */
static int   under_first[256 * 256 + 1];
static int  *under_obj;
static float *under_cos, *under_sin;
static void under_index(void)
{
    static const scene *known;
    int *const cell_first = under_first;
    int *cell_obj = under_obj;
    float *obj_cos = under_cos, *obj_sin = under_sin;
    if (known != g_scene) {
        known = g_scene;
        cell_obj = malloc(sizeof(int) * (size_t)(g_scene->count + 1));
        obj_cos = malloc(sizeof(float) * (size_t)(g_scene->count + 1));
        obj_sin = malloc(sizeof(float) * (size_t)(g_scene->count + 1));
        memset(under_first, 0, sizeof under_first);
        for (int pass = 0; pass < 2; pass++) {
            for (int o = 0; o < g_scene->count; o++) {
                const scene_object *so = &g_scene->obj[o];
                if (so->layer != 0 || !so->m) continue;
                int kx = ((int)so->x + 0x100000) >> 13, ky = ((int)so->y + 0x100000) >> 13;
                if (kx < 0 || kx > 255 || ky < 0 || ky > 255) continue;
                /* SCREAMER 2: the cell whose list the piece is IN, as the game walks them (0x28ce6) */
                if (os_game() == 2) { kx = so->cell & 255; ky = so->cell >> 8; }
                if (!pass) cell_first[ky * 256 + kx + 1]++;
                else cell_obj[cell_first[ky * 256 + kx]++] = o;
            }
            if (!pass) for (int k = 0; k < 256 * 256; k++) cell_first[k + 1] += cell_first[k];
            else { for (int k = 256 * 256; k > 0; k--) cell_first[k] = cell_first[k - 1]; cell_first[0] = 0; }
        }
        for (int o = 0; o < g_scene->count; o++) { obj_cos[o] = cosf(g_scene->obj[o].yaw); obj_sin[o] = sinf(g_scene->obj[o].yaw); }
    }
    under_obj = cell_obj; under_cos = obj_cos; under_sin = obj_sin;
}

static int track_under(float x, float y, int want_height, float *out)
{
    static const int cell[9][2] = { {0,0}, {1,0}, {-1,0}, {0,1}, {0,-1}, {1,1}, {-1,-1}, {1,-1}, {-1,1} };
    if (!g_scene) return 0;
    under_index();
    const int *const cell_first = under_first, *const cell_obj = under_obj;
    const float *const obj_cos = under_cos, *const obj_sin = under_sin;
    int cx = ((int)x + 0x100000) >> 13, cy = ((int)y + 0x100000) >> 13;
    for (int ci = 0; ci < 9; ci++) {
        int kx = cx + cell[ci][0], ky = cy + cell[ci][1];
        if (kx < 0 || kx > 255 || ky < 0 || ky > 255) continue;
        for (int at = cell_first[ky * 256 + kx]; at < cell_first[ky * 256 + kx + 1]; at++) {
            int o = cell_obj[at];
            const scene_object *so = &g_scene->obj[o];
            const mesh *mm = so->m;
            float dx = x - so->x, dy = y - so->y;
            float bx = dx - mm->bound_cx, by = dy - mm->bound_cz;
            if (bx * bx + by * by > mm->bound_r * mm->bound_r) continue;
            float c = obj_cos[o], sn = obj_sin[o];
            float lx = dx * c + dy * sn, lz = -dx * sn + dy * c;        /* into the piece's frame */
            for (int tries = 0; tries < g_under_tries; tries++, lx += g_under_step, lz += g_under_step)
                for (int k = 0; k < mm->ncoll; k++) {
                    int f = mm->coll[k];
                    if (f < 0) continue;
                    int nc = mm->ncorner[f], pos = 0, neg = 0, ok = 1;
                    for (int j = 0; j < nc; j++) {
                        int va = mm->idx[f][j], vb = mm->idx[f][(j + 1) % nc];
                        if (va < 0 || vb < 0) { ok = 0; break; }
                        const vec3 *a = &mm->vert[va], *b = &mm->vert[vb];
                        float cr = (b->x - a->x) * (lz - a->z) - (b->z - a->z) * (lx - a->x);
                        if (cr > 0) pos++; else if (cr < 0) neg++;
                    }
                    if (!ok || (pos && neg)) continue;
                    g_under_obj = o;
                    if (!want_height) return 1;
                    int nx = (int)lroundf(mm->normal[f].x * 16384.0f);
                    int ny = (int)lroundf(mm->normal[f].y * 16384.0f);
                    int nz = (int)lroundf(mm->normal[f].z * 16384.0f);
                    if (ny == 0) continue;
                    const vec3 *v0 = &mm->vert[mm->idx[f][0]];
                    int num = ((int)lx - (int)v0->x) * nx + ((int)lz - (int)v0->z) * nz;
                    *out = so->z + v0->y + (float)(-num / ny);
                    g_under_n[0] = (float)nx * c - (float)nz * sn;
                    g_under_n[1] = (float)nx * sn + (float)nz * c;
                    g_under_n[2] = (float)ny;
                    return 1;
                }
        }
    }
    return 0;
}

/* SCREAMER 2's routines ask what is under a wheel (the hook for 0x28ce6, s2_hooks.c): the
   same search, and besides the height the surface's normal -- turned to point UP, as the
   wheel routine wants it (0x19cac pushes the car along its level part) -- and the piece's
   road kind */
/*
 * SCREAMER 2'S OWN "WHAT IS UNDER THIS POINT" (0x28ce6 -> 0x28db7 -> 0x4c24c, with 0x4c504 a
 * four-cornered face and 0x4c7de a three-cornered one), in whole numbers as the game does it:
 *
 *  - the nine cells round the point in the order of the table at 0x112e84, each cell's
 *    drivable pieces in stored order; a piece is looked at when the point is inside its first
 *    block's circle (radius, centre: the block's first three words);
 *  - the point is turned into the piece's frame (0x10388: cut to whole numbers) and up to
 *    five times moved 4 along both axes until a face holds it;
 *  - the faces in the order of the block's list at +0x28; a face holds the point when it is
 *    inside the corners' box and on the inner side (>= 0) of every edge of a TRIANGLE of it --
 *    corners 0 1 2, or for a four-cornered face else 0 2 3 -- one winding only;
 *  - the plane is THAT TRIANGLE's, from its corners (not the face's stored normal): n = A x B
 *    with A = corner 0 - the second, B = corner 0 - the third; a triangle standing on edge
 *    (n's height 0) is passed over; height = second corner's - ((point - it) . n) / n.height,
 *    the division cut off;
 *  - asked with 1 (0x4c474) the answer is only whether a face holds the point: no plane, and
 *    a face standing on edge counts;
 *  - the normal is turned back into the world (0x10388 again) as it is, not made to point up;
 *    the road's kind is the piece record's last word.
 */
static int s2_edge(const int a[3], const int b[3], int px, int pz)
{
    return (a[0] - b[0]) * (pz - a[2]) + (b[2] - a[2]) * (px - a[0]);
}

int s2p_ground(int only_whether, int x, int y, int *height, int normal[3], int *kind)
{
    static const int cell[9][2] = { {0,0}, {1,0}, {-1,0}, {0,1}, {0,-1}, {1,1}, {-1,-1}, {1,-1}, {-1,1} };
    if (getenv("OS_S2FLAT")) { *height = 0; normal[0] = 0; normal[1] = 16384; normal[2] = 0; *kind = atoi(getenv("OS_S2FLAT")); return 1; }   /* a test: a level world of one road kind */
    if (!g_scene) return 0;
    under_index();
    const int cx = (x + 0x100000) >> 13, cy = (y + 0x100000) >> 13;
    for (int ci = 0; ci < 9; ci++) {
        const int kx = cx + cell[ci][0], ky = cy + cell[ci][1];
        if (kx < 0 || kx > 255 || ky < 0 || ky > 255) continue;
        for (int at = under_first[ky * 256 + kx]; at < under_first[ky * 256 + kx + 1]; at++) {
            const int o = under_obj[at];
            const scene_object *so = &g_scene->obj[o];
            const mesh *mm = so->m;
            const int ox = (int)so->x, oh = (int)so->z, oz = (int)so->y;
            const int dx = x - ox, dz = y - oz;
            const int bx = dx - (int)mm->bound_cx, bz = dz - (int)mm->bound_cz, br = (int)mm->bound_r;
            if (bz * bz + bx * bx > br * br) continue;
            const float turn = (float)so->t * 0.0009765625f * 3.14159274f;
            const float c = cosf(turn), sn = sinf(turn);
            int px = (int)((float)dx * c + (float)dz * sn) + ox;
            int pz = (int)((float)dz * c - (float)dx * sn) + oz;
            for (int tries = 0; tries < 5; tries++, px += 4, pz += 4)
                for (int k = 0; k < mm->ncoll; k++) {
                    const int f = mm->coll[k];
                    if (f < 0) continue;
                    const int nc = mm->ncorner[f];
                    int v[4][3], lo[2], hi[2], which = 0;
                    if (nc != 3 && nc != 4) continue;
                    for (int j = 0; j < nc; j++) {
                        const vec3 *p = &mm->vert[mm->idx[f][j]];
                        v[j][0] = (int)p->x + ox; v[j][1] = (int)p->y + oh; v[j][2] = (int)p->z + oz;
                        if (!j || v[j][0] < lo[0]) lo[0] = v[j][0];
                        if (!j || v[j][0] > hi[0]) hi[0] = v[j][0];
                        if (!j || v[j][2] < lo[1]) lo[1] = v[j][2];
                        if (!j || v[j][2] > hi[1]) hi[1] = v[j][2];
                    }
                    if (px > hi[0] || px < lo[0] || pz > hi[1] || pz < lo[1]) continue;
                    if (s2_edge(v[0], v[1], px, pz) >= 0 && s2_edge(v[1], v[2], px, pz) >= 0 && s2_edge(v[2], v[0], px, pz) >= 0) which = 1;
                    else if (nc == 4 && s2_edge(v[0], v[2], px, pz) >= 0 && s2_edge(v[2], v[3], px, pz) >= 0 &&
                             s2_edge(v[3], v[0], px, pz) >= 0) which = 2;
                    if (!which) continue;
                    /* asked only WHETHER there is ground (0x28ce6(1) -> 0x4c474: the wall test's
                       question about each corner of the car): any face that holds the point will
                       do, one standing on edge too -- no plane is made */
                    if (only_whether) { *kind = so->kind; g_under_obj = o; return 1; }
                    const int *p1 = v[which], *p2 = v[which + 1];
                    const int ax = v[0][0] - p1[0], ay = v[0][1] - p1[1], az = v[0][2] - p1[2];
                    const int qx = v[0][0] - p2[0], qy = v[0][1] - p2[1], qz = v[0][2] - p2[2];
                    const int nz = ax * qy - ay * qx, ny = qx * az - ax * qz, nx = ay * qz - az * qy;
                    if (ny == 0) continue;
                    *height = -((px - p1[0]) * nx + (pz - p1[2]) * nz) / ny + p1[1];
                    {
                        const float cb = cosf(-turn), sb = sinf(-turn);
                        normal[0] = (int)((float)nx * cb + (float)nz * sb);
                        normal[1] = ny;
                        normal[2] = (int)((float)nz * cb - (float)nx * sb);
                    }
                    *kind = so->kind;
                    g_under_obj = o;
                    return 1;
                }
        }
    }
    return 0;
}

static int on_track(float x, float y)
{
    if (!g_scene) return 1;
    return track_under(x, y, 0, NULL);
}

/*
 * The tilt that lays a thing heading `yaw` flat on the face track_under() has
 * just found -- how the game stands an opponent (0x1cb9c) and a prop
 * (0x15240, 0x20880) on a slope: the face's stored normal is turned into the
 * thing's own frame and each of its two level parts, x pi / 46080 of the
 * normal's 16384 units, is an angle (1.117 x the slope).
 *
 * The port's lean is + when the RIGHT side is higher.  (-fy, fx) is the left,
 * and a road rising to the right has its normal leaning left; with the sign
 * the other way round the opponents leaned AGAINST every banked bend, at
 * twice the banking (Danik's screenshot: on two wheels at 45 degrees).
 */
static void slope_tilt(float yaw, float *pitch, float *roll)
{
    float nx = g_under_n[0], ny = g_under_n[1], nz = g_under_n[2];
    float len = sqrtf(nx * nx + ny * ny + nz * nz);
    if (nz < 0.0f) len = -len;
    if (len != 0.0f) { nx /= len; ny /= len; }
    float fx = sinf(yaw), fy = -cosf(yaw);
    *pitch = -1.117f * (nx * fx + ny * fy);
    *roll  = 1.117f * (nx * -fy + ny * fx);
}

/*
 * Which side of each wall is the track on?  +1 = the side its normal points
 * away from... measured, not assumed: the track files have walls authored
 * both ways round.  Take the side that has track 200 units from the wall's
 * middle; if that does not decide it, the side the road's centre line is on.
 */
static float g_wall_in[SCENE_MAX_WALL];

static const scene_dir *nearest_dir(float x, float y);

static void walls_prepare(void)
{
    int undecided = 0;
    for (int i = 0; g_scene && i < g_scene->nwall; i++) {
        const scene_wall *w = &g_scene->wall[i];
        float nx = cosf(w->angle), ny = -sinf(w->angle);
        int neg = on_track(w->x - nx * 200.0f, w->y - ny * 200.0f);
        int pos = on_track(w->x + nx * 200.0f, w->y + ny * 200.0f);
        if (neg != pos) { g_wall_in[i] = pos ? 1.0f : -1.0f; continue; }
        const scene_dir *d = nearest_dir(w->x, w->y);
        float side = d ? (d->x - w->x) * nx + (d->y - w->y) * ny : -1.0f;
        g_wall_in[i] = side > 0.0f ? 1.0f : -1.0f;
        undecided++;
    }
    if (g_scene)
        printf("walls: %d segments, inside found from the track surface for %d, "
               "from the centre line for %d\n", g_scene->nwall,
               g_scene->nwall - undecided, undecided);
}

/* the nearest wall segment / road-direction point (0x120c0, modes 1 and 0) */
static const scene_wall *nearest_wall(float x, float y)
{
    const scene_wall *best = NULL;
    float bd = 1e30f;
    for (int i = 0; g_scene && i < g_scene->nwall; i++) {
        const scene_wall *w = &g_scene->wall[i];
        float d = (w->x - x) * (w->x - x) + (w->y - y) * (w->y - y);
        if (d < bd) { bd = d; best = w; }
    }
    return best;
}

static const scene_dir *nearest_dir(float x, float y)
{
    const scene_dir *best = NULL;
    float bd = 1e30f;
    for (int i = 0; g_scene && i < g_scene->ndir; i++) {
        const scene_dir *w = &g_scene->dir[i];
        float d = (w->x - x) * (w->x - x) + (w->y - y) * (w->y - y);
        if (d < bd) { bd = d; best = w; }
    }
    return best;
}

/* for SCREAMER 2's routines (the hook for 0x2a371): the record of the road-centre point or
   wall segment nearest a place, as the track's file has it */
/*
 * SCREAMER 2'S NEAREST ROAD POINT / WALL (0x2a371), as the game looks: only in the nine grid
 * cells round the point -- the rows from the one before to the one after, in each the
 * columns likewise, each cell's list in stored order -- by the squares of whole numbers,
 * the first of equals; none there = none.
 */
int s2p_nearest(int walls, int x, int y, float rec[7])
{
    if (!g_scene) return 0;
    const int cx = (x + 0x100000) >> 13, cy = (y + 0x100000) >> 13;
    int least = 0x7ffffff8, found = -1;
    for (int row = cy - 1; row <= cy + 1; row++)
        for (int col = cx - 1; col <= cx + 1; col++) {
            if (row < 0 || row > 255 || col < 0 || col > 255) continue;
            const int cell = row * 256 + col;
            if (walls) {
                for (int i = 0; i < g_scene->nwall; i++) {
                    const scene_wall *w = &g_scene->wall[i];
                    if (w->cell != cell) continue;
                    const int d = ((int)w->x - x) * ((int)w->x - x) + ((int)w->y - y) * ((int)w->y - y);
                    if (d < least) { least = d; found = i; }
                }
            } else {
                for (int i = 0; i < g_scene->ndir; i++) {
                    const scene_dir *w = &g_scene->dir[i];
                    if (w->cell != cell) continue;
                    const int d = ((int)w->x - x) * ((int)w->x - x) + ((int)w->y - y) * ((int)w->y - y);
                    if (d < least) { least = d; found = i; }
                }
            }
        }
    if (found < 0) return 0;
    if (walls) {
        const scene_wall *w = &g_scene->wall[found];
        rec[0] = w->angle; rec[1] = w->x; rec[2] = w->y; rec[3] = w->x1; rec[4] = w->y1; rec[5] = w->x2; rec[6] = w->y2;
    } else {
        const scene_dir *d = &g_scene->dir[found];
        rec[0] = d->angle; rec[1] = d->x; rec[2] = d->y;
    }
    return 1;
}

static mesh g_shadow_mesh;
static int  g_shadow_ok;
static mesh car_mesh[7];
static int  car_mesh_ok[7];

/*
 * Stand a car on the track: the height and tilt that put its four wheels on
 * the surface, using the port's own surface lookup.  NOT what runs in a
 * race -- that is player_contact() and the opponents' one-point rule, both
 * the game's.  This only sets the player's car down before the first step
 * (race set-up, BACKSPACE) and is the independent measure OS_TILTCHECK
 * compares the opponents' tilt with.
 *
 * `yaw` is the car's heading (forward = (sin yaw, -cos yaw)); `ref` is roughly
 * where the car is now, and each wheel takes the surface CLOSEST to that
 * height.  (An earlier version took "the highest surface not above the car":
 * on track 2's steep, banked start a car that had been shoved a little way
 * into the slope then found nothing above it and stayed stuck inside the
 * road -- Danik.)  Returns 0 if no wheel found a surface.
 */
static int car_ground(const mesh *mm, float x, float y, float ref, float yaw,
                      float *z, float *pitch, float *roll)
{
    float fx = sinf(yaw), fy = -cosf(yaw);          /* forward */
    float rx = -cosf(yaw), ry = -sinf(yaw);         /* right */
    float af = mm->axle_front, ar = mm->axle_rear, tr = 226.0f;
    /* front-left, front-right, rear-left, rear-right */
    const float wx[4] = { -tr, tr, -tr, tr }, wz[4] = { af, af, -ar, -ar };
    float h[4];
    int hits = 0;
    for (int k = 0; k < 4; k++) {
        h[k] = ref;
        if (surface_pick(x + rx * wx[k] + fx * wz[k], y + ry * wx[k] + fy * wz[k],
                         ref, 0, &h[k]) && fabsf(h[k] - ref) < 700.0f)
            hits++;
        else
            h[k] = ref;
    }
    if (!hits) return 0;
    /* a wheel that found nothing takes the height of the others */
    float sum = 0.0f;
    int n = 0;
    for (int k = 0; k < 4; k++) if (h[k] != ref || hits == 4) { sum += h[k]; n++; }
    float avg = n ? sum / (float)n : ref;
    for (int k = 0; k < 4; k++) if (h[k] == ref && hits < 4) h[k] = avg;
    *z     = 0.25f * (h[0] + h[1] + h[2] + h[3]);
    *pitch = atan2f(0.5f * (h[0] + h[1]) - 0.5f * (h[2] + h[3]), af + ar);
    *roll  = atan2f(0.5f * (h[1] + h[3]) - 0.5f * (h[0] + h[2]), 2.0f * tr);
    return 1;
}

/* load the seven car models; who drives which is decided later */
static void cars_load(const char *gfxdir)
{
    g_ncar = 0;
    for (int k = 0; k < 7; k++) {
        char file[600];
        snprintf(file, sizeof file, "%s/AUTO%02d.3DX", gfxdir, k);
        car_mesh_ok[k] = (mesh_load(&car_mesh[k], file) == 0);
    }
    char sfile[600];
    snprintf(sfile, sizeof sfile, "%s/SHADOW.3DX", gfxdir);
    g_shadow_ok = (mesh_load(&g_shadow_mesh, sfile) == 0);
}

/*
 * SCREAMER 2's CARS, as far as it goes: one of each team standing on the road.
 * The container is CARS<team 1-4><condition & 1><colours> (0x2f9dd), the car in it
 * AUTO<1-5> with a material library of its own (AUTOn.MLB; AUTOn.WLB is the same for the
 * program's 8 MB version, 0x11f14c).  Every palette of the game has the same first 120
 * colours and the cars' pictures use only those, so the track's palette serves.
 * The object is built like Screamer's: MV_mauto, fauto, rauto, four tyres, four hubs.
 */
#define S2_CARS 4
static mesh     g_s2_car[S2_CARS];
static car_pose g_s2_pose[S2_CARS];
static scene_object *g_s2_obj[S2_CARS];
static int      g_s2_on;                 /* the game's own race routines are running (s2_hooks.c) */
/*
 * HOW FAR A CAR'S REFLECTIONS HAVE SLID (0xb0040, every time the engine draws the object): its
 * move since it was last drawn, turned into the camera's axes, times 0xb04a8 (s2_env_rate) is
 * added to two sums kept with the object (+0x30, +0x38: the camera's right and up); each sum
 * to the nearest whole number, of 256, is taken from that axis's coordinate of the reflection
 * (127 + 127 r - it).  Screamer 1's engine does the same; the port's Screamer 1 slides by the
 * distance driven alone (env_scroll), as before.
 */
static struct { float sum[2], last[3]; int have; } g_s2_slide[S2_CARS];
static void s2_env_slide(const camera *cam)
{
    const float k = s2_env_rate();
    const float cyw = cosf(cam->yaw), syw = sinf(cam->yaw), cp = cosf(cam->pitch), sp = sinf(cam->pitch);
    const float rx = syw, ry = -cyw;                            /* the camera's right and up, as draw_object's */
    const float ux = -cyw * sp, uy = -syw * sp, uz = cp;
    for (int i = 0; i < g_s2_on && i < S2_CARS; i++) {
        const scene_object *o = g_s2_obj[i];
        if (!o) continue;
        const float at[3] = { o->x, o->y, o->z };
        if (g_s2_slide[i].have) {
            const float dx = at[0] - g_s2_slide[i].last[0], dy = at[1] - g_s2_slide[i].last[1], dz = at[2] - g_s2_slide[i].last[2];
            g_s2_slide[i].sum[0] += (dx * rx + dy * ry) * k;
            g_s2_slide[i].sum[1] += (dx * ux + dy * uy + dz * uz) * k;
        }
        memcpy(g_s2_slide[i].last, at, sizeof at);
        g_s2_slide[i].have = 1;
        g_s2_pose[i].env_u = (float)((int)lrintf(g_s2_slide[i].sum[0]) & 255);
        g_s2_pose[i].env_scroll = (float)((int)lrintf(g_s2_slide[i].sum[1]) & 255);
    }
}

static void s2_race_read(void);

/* the race: four cars, one of each team, the player's (car 0; OS_CAR=0..4 its model) last
   on the grid as the game puts it; the others drive themselves */
static scene_object *g_s2_shadow[S2_CARS];
static scene_object *shadow_new(void);
static int g_s2_mirror_on, g_s2_mirror_frame_ok = -1;      /* the mirror: on (R, OS_S2MIRROR); its frame's picture read */
/* (the mirror's camera after the game's last two frames, and how far between them the
   picture is: it is drawn in between like everything else, or what it shows would move
   17.7 times a second on a faster screen) */
static s2_camera_state g_s2_mir_was, g_s2_mir_is;
static int g_s2_mir_was_ok, g_s2_mir_is_ok;
static float g_s2_show_t = 1.0f;
static int pkc_load(const char *file, int16_t *out);
static void pkc_draw(const int16_t *pic);
static int g_pkc_loads;
#define S2_PUFFS 48
#define S2_SPARKS 60
static mesh g_s2_smoke[2], g_s2_tri[2];
static scene_object *g_s2_puff[S2_PUFFS], *g_s2_spark[S2_SPARKS];
static int g_s2_npuff, g_s2_nspark;
/* the obstacles (cones ...): a mesh for each name the track's list has, an object for each */
#define S2_OBST 64
#define S2_OBST_KINDS 8
static mesh g_s2_obst_mesh[S2_OBST_KINDS];
static char g_s2_obst_name[S2_OBST_KINDS][16];
static int  g_s2_obst_kinds, g_s2_obst_base = -1, g_s2_nobst;
static char g_s2_obst_dir[512];
static scene_object *g_s2_obst[S2_OBST];
/* The game moves the sparks and the obstacles once a frame of its own, 17.7 times a second;
   like the cars they are drawn between its last two frames (s2_race_read keeps both), or a
   flying cone would jump from place to place on a faster screen. */
static struct { int on; s2_spark s; } g_s2_spark_was[S2_SPARKS], g_s2_spark_is[S2_SPARKS];
static struct { int on; s2_obstacle s; } g_s2_obst_was[S2_OBST], g_s2_obst_is[S2_OBST];
/* the track's animations (birds ...): s2_hooks.c says which are to be seen and where */
#define S2_ANIMS 24
static scene_object *g_s2_anim[S2_ANIMS];
static int g_s2_nanimobj;
static struct { int on; s2_anim s; } g_s2_anim_was[40], g_s2_anim_is[40];
static float s2_between(float a, float b, float t);

static mesh *s2_obst_mesh(const char *name)
{
    for (int k = 0; k < g_s2_obst_kinds; k++) if (!strcmp(g_s2_obst_name[k], name)) return g_s2_obst_mesh[k].nface ? &g_s2_obst_mesh[k] : NULL;
    if (g_s2_obst_kinds >= S2_OBST_KINDS || g_s2_obst_base < 0) return NULL;
    const int k = g_s2_obst_kinds++;
    char file[600], up[16];
    snprintf(g_s2_obst_name[k], sizeof g_s2_obst_name[k], "%s", name);
    snprintf(up, sizeof up, "%s", name);
    for (char *c = up; *c; c++) *c = (char)toupper((unsigned char)*c);
    snprintf(file, sizeof file, "%s/%s.3DX", g_s2_obst_dir, up);
    memset(&g_s2_obst_mesh[k], 0, sizeof g_s2_obst_mesh[k]);
    if (mesh_load(&g_s2_obst_mesh[k], file) != 0) { fprintf(stderr, "note: no obstacle at %s\n", file); g_s2_obst_mesh[k].nface = 0; return NULL; }
    mesh_rebase(&g_s2_obst_mesh[k], g_s2_obst_base);
    return &g_s2_obst_mesh[k];
}

/* the puffs the game has on, as they stand after its last frame (they do not move, but for
   those of the car the chase view follows: 0x2073c) */
static void s2_puffs_show(float t)
{
    s2_puff p[S2_PUFFS];
    const int n = g_s2_npuff ? s2_puffs(p, g_s2_npuff) : 0;
    g_dbg_quad = getenv("OS_S2PUFFQUAD") != NULL;
    if (getenv("OS_S2PUFFLOG")) {                              /* a test: the puffs that are on, by car */
        int by[S2_CARS] = { 0 };
        for (int i = 0; i < n; i++) if (p[i].car >= 0 && p[i].car < S2_CARS) by[p[i].car]++;
        printf("puffs on: %d (by car: %d %d %d %d)", n, by[0], by[1], by[2], by[3]);
        for (int i = 0; i < n; i++) if (p[i].car == 0) { printf("; the player's first: kind %d at %d %d height %d, part %d; the car at %.0f %.0f", p[i].kind, p[i].x, p[i].y, p[i].height, p[i].phase, g_s2_obj[0] ? g_s2_obj[0]->x : 0.0f, g_s2_obj[0] ? g_s2_obj[0]->y : 0.0f); break; }
        printf("\n");
        for (int i = 0; i < n; i++) if (p[i].carried) printf("   carried puff of car %d, kind %d: from the car %d %d %d, part %d\n", p[i].car, p[i].kind, p[i].x, p[i].height, p[i].y, p[i].phase);
    }
    for (int i = 0; i < g_s2_npuff; i++) {
        scene_object *o = g_s2_puff[i];
        if (i >= n) { o->z = -1.0e6f; continue; }
        o->m = &g_s2_smoke[p[i].kind == 2];
        o->x = (float)p[i].x; o->y = (float)p[i].y; o->z = (float)p[i].height;
        /* (the car is drawn between two of the game's frames: a puff carried with it goes
           where the car is DRAWN, or it would run a frame ahead of it -- up to 900 units at
           100 mph, onto the car's tail and half the size) */
        if (p[i].carried && p[i].car >= 0 && p[i].car < S2_CARS && g_s2_obj[p[i].car]) {
            const scene_object *c = g_s2_obj[p[i].car];
            o->x += c->x; o->y += c->y; o->z += c->z;
        }
        o->phase = p[i].phase;
    }
    /* the sparks (0x2aeb9, 0x2cc74): small triangles thrown from where two cars touch, up to
       thirty at once by how hard, falling at 7848 a second squared and gone after two seconds */
    {
        int m = 0;
        for (int k = 0; k < S2_SPARKS && m < g_s2_nspark; k++) {
            if (!g_s2_spark_is[k].on) continue;
            const s2_spark *b = &g_s2_spark_is[k].s, *a = g_s2_spark_was[k].on ? &g_s2_spark_was[k].s : b;
            scene_object *o = g_s2_spark[m++];
            o->m = &g_s2_tri[b->kind];
            o->x = s2_between(a->x, b->x, t); o->y = s2_between(a->y, b->y, t); o->z = s2_between(a->height, b->height, t);
            o->pitch = s2_between(a->angle[0], b->angle[0], t); o->yaw = s2_between(a->angle[1], b->angle[1], t); o->roll = s2_between(a->angle[2], b->angle[2], t);
        }
        for (int i = m; i < g_s2_nspark; i++) g_s2_spark[i]->z = -1.0e6f;
        if (getenv("OS_S2PUFFLOG") && m) printf("sparks flying: %d\n", m);
    }
    /* the obstacles, where the game has them (0x30b29 stands them up, 0x2a639 knocks them,
       0x2c475 flies them): level they are (pi, the road's direction, pi) */
    {
        int m = 0;
        for (int k = 0; k < S2_OBST && m < g_s2_nobst; k++) {
            if (!g_s2_obst_is[k].on) continue;
            const s2_obstacle *b = &g_s2_obst_is[k].s, *a = g_s2_obst_was[k].on ? &g_s2_obst_was[k].s : b;
            mesh *mm = s2_obst_mesh(b->name);
            if (!mm) continue;
            scene_object *o = g_s2_obst[m++];
            o->m = mm;
            /* The game has an obstacle's MIDDLE at the ground and, drawing whole objects back
               to front, shows all of it.  With a depth test the lower half would be in the
               ground; letting the road not hide it made it show through crests and walls
               (Danik).  So it is stood ON the ground instead -- raised by what its model
               has below its middle -- and hidden by whatever is in front like anything else.
               The port's own. */
            float low = 0.0f;
            for (int vi = 0; vi < mm->nvert; vi++) if (mm->vert[vi].y < low) low = mm->vert[vi].y;
            o->x = s2_between(a->x, b->x, t); o->y = s2_between(a->y, b->y, t); o->z = s2_between(a->height, b->height, t) - low;
            o->yaw = s2_between(a->angle[1], b->angle[1], t);
            o->pitch = s2_between(a->angle[0], b->angle[0], t) - 3.14159274f; o->roll = s2_between(a->angle[2], b->angle[2], t) - 3.14159274f;
        }
        for (int i = m; i < g_s2_nobst; i++) g_s2_obst[i]->z = -1.0e6f;
        if (getenv("OS_S2PUFFLOG") && m) printf("obstacles: %d\n", m);
    }
    /* the track's animations: an object along its path (0x2cee4), drawn between the game's
       last two frames; one that has just appeared starts where it is */
    {
        int m = 0;
        for (int k = 0; k < 40 && m < g_s2_nanimobj; k++) {
            if (!g_s2_anim_is[k].on) continue;
            const s2_anim *b = &g_s2_anim_is[k].s, *a = g_s2_anim_was[k].on ? &g_s2_anim_was[k].s : b;
            mesh *mm = s2_obst_mesh(b->name[0] == 's' ? b->name + 1 : b->name);
            if (!mm) continue;
            scene_object *o = g_s2_anim[m++];
            o->m = mm;
            o->sprite = b->faces_screen;
            o->x = s2_between(a->x, b->x, t); o->y = s2_between(a->y, b->y, t); o->z = s2_between(a->height, b->height, t);
            o->yaw = b->heading; o->pitch = -b->climb; o->roll = 0.0f;     /* (the game's up / down, +0x14, is the port's pitch the other way round:
                                                                                 the cable cars on track 4's slopes leant against their rails -- Danik) */
            if (getenv("OS_S2ANIMTURN")) { float a0 = 0, a1 = 0, a2 = 0; sscanf(getenv("OS_S2ANIMTURN"), "%f,%f,%f", &a0, &a1, &a2); o->pitch += a0; o->yaw += a1; o->roll += a2; }   /* a test */
        }
        for (int i = m; i < g_s2_nanimobj; i++) g_s2_anim[i]->z = -1.0e6f;
        if (getenv("OS_S2PUFFLOG") && m) printf("animations seen: %d; the first at %.0f %.0f height %.0f\n", m, (double)g_s2_anim[0]->x, (double)g_s2_anim[0]->y, (double)g_s2_anim[0]->z);
    }
    if (getenv("OS_S2PUFFTEST") && g_s2_npuff && g_s2_obj[0]) {       /* a test: one puff over the player's car, part-way through */
        scene_object *o = g_s2_puff[g_s2_npuff - 1];
        o->m = &g_s2_smoke[atoi(getenv("OS_S2PUFFTEST")) == 2];
        o->x = g_s2_obj[0]->x; o->y = g_s2_obj[0]->y; o->z = g_s2_obj[0]->z + 400.0f; o->phase = 30000;
        const int c = getenv("OS_S2PUFFCAR") ? atoi(getenv("OS_S2PUFFCAR")) : -1;
        if (c >= 0 && c < S2_CARS && g_s2_obj[c] && g_s2_npuff >= 5)    /* ...or five where car n stands, as its own would */
            for (int k = 0; k < 5; k++) {
                o = g_s2_puff[g_s2_npuff - 1 - k];
                o->m = &g_s2_smoke[atoi(getenv("OS_S2PUFFTEST")) == 2];
                o->x = g_s2_obj[c]->x + (float)(k % 3 - 1) * 300.0f; o->y = g_s2_obj[c]->y + (float)(k / 3) * 300.0f;
                o->z = g_s2_obj[c]->z; o->phase = 20000;
            }
    }
}

static void s2_hud_load(void);
static float g_s2_end_t;
static int   g_s2_end_sel, g_s2_track;

static void volumes_apply(float fade);
static void s2_attract_start(void);
static void s2_race_begin(int track, const char *gfxdir)
{
    if (getenv("OS_LAPS")) s2_laps = atoi(getenv("OS_LAPS")) > 0 ? atoi(getenv("OS_LAPS")) : 3;
    g_s2_track = track; g_s2_end_t = 0.0f; g_s2_end_sel = 0;
    s2_attract_start();
    /* the cars as the menu gives them (0x339e7): the player's team (0 until there is a menu)
       and its car -- every team's choice starts as car 1 (0x32d53) -- and the others the
       next teams and the next cars round, of four */
    int model[S2_CARS] = { 1, 2, 3, 0 };
    if (getenv("OS_CAR")) {
        model[0] = (atoi(getenv("OS_CAR")) % 5 + 5) % 5;
        for (int t = 1; t < S2_CARS; t++) model[t] = (model[0] + t) % 4;
    }
    int n = getenv("OS_S2CARS") ? atoi(getenv("OS_S2CARS")) : S2_CARS;
    if (n < 1 || n > S2_CARS) n = S2_CARS;
    /* the kind of game (0x339e7): time attack is the player and the ghost, by day; a free
       run the player alone */
    if (getenv("OS_S2GAME")) s2_mode = atoi(getenv("OS_S2GAME"));
    if (getenv("OS_S2GHOST")) s2_ghost_on = atoi(getenv("OS_S2GHOST")) != 0;
    if (s2_mode == 4) n = 2;
    if (s2_mode == 8) n = 1;
    /* the player's team, and the others the next teams round (0x339e7); in the championship's
       fourth league every other car is its team's fifth */
    int team[S2_CARS] = { 0, 1, 2, 3 };
    if (getenv("OS_TEAM")) for (int t = 0; t < S2_CARS; t++) team[t] = (atoi(getenv("OS_TEAM")) + t) & 3;
    if (s2_mode == 2) {
        n = S2_CARS;
        if (getenv("OS_S2LEAGUE") && atoi(getenv("OS_S2LEAGUE")) == 3) for (int t = 1; t < S2_CARS; t++) model[t] = 4;
    }
    if (getenv("OS_S2ENTRIES") && getenv("OS_FROM_MENU")) {     /* (the game's menu: the cars its record has, 0x339e7) */
        const char *p = getenv("OS_S2ENTRIES");
        int cnt = 0, used = 0;
        if (sscanf(p, "%d%n", &cnt, &used) == 1 && cnt >= 1 && cnt <= S2_CARS) {
            int ok = 1, tm[S2_CARS], md[S2_CARS];
            p += used;
            for (int t = 0; t < cnt && ok; t++) { ok = sscanf(p, "%d %d%n", &tm[t], &md[t], &used) == 2 && tm[t] >= 0 && tm[t] < 4 && md[t] >= 0 && md[t] < 5; p += used; }
            if (ok) { n = cnt; for (int t = 0; t < cnt; t++) { team[t] = tm[t]; model[t] = md[t]; } }
        }
    }
    s2_attack_plan(track, team, model);
    for (int t = 0; t < n && g_ncar < MAX_CARS; t++) {
        char file[600];
        snprintf(file, sizeof file, "build/dec/CARS%d%d1/AUTO%d.MLB", team[t] + 1, g_s2_cond & 1, model[t] + 1);
        const int base = mlb_add(file);
        snprintf(file, sizeof file, "build/dec/CARS%d%d1/AUTO%d.3DX", team[t] + 1, g_s2_cond & 1, model[t] + 1);
        if (base < 0 || mesh_load(&g_s2_car[t], file) != 0) { fprintf(stderr, "note: no car at %s\n", file); return; }
        mesh_rebase(&g_s2_car[t], base);
        if (getenv("OS_S2WHEELS")) printf("s2 model %d: wheelbase %.0f (front %.0f, rear %.0f), half track %.0f\n", t, g_s2_car[t].wheelbase, g_s2_car[t].axle_front, g_s2_car[t].axle_rear, g_s2_car[t].track_half);
        scene_object *o = &g_car[g_ncar++];
        memset(o, 0, sizeof *o);
        o->m = &g_s2_car[t];
        o->pose = &g_s2_pose[t];
        o->phase = -1;
        o->radius = 1200.0f;
        g_s2_obj[t] = o;
    }
    /* the reflections' map (0x2ecb0, after the cars): "env" of gfx\envp<track><look & 1>1.dat,
       flag 4 -- every car's glass reads it (g_env_mat) */
    {
        char file[600];
        snprintf(file, sizeof file, "build/dec/ENVP%d%d1/ENV.MLB", track, g_s2_cond & 1);
        g_env_mat = mlb_add(file);
        memset(g_s2_slide, 0, sizeof g_s2_slide);
    }
    /* the shadow: ASHADOW.3DX of the track's pictures, an object of its own for every car
       (0x2f519; placed by 0x1f4a8) */
    {
        char file[600];
        snprintf(file, sizeof file, "%s/MISC.MLB", gfxdir);        /* its materials: the library "misc" (0x4feb4) */
        const int base = mlb_add(file);
        snprintf(file, sizeof file, "%s/ASHADOW.3DX", gfxdir);
        g_shadow_ok = (base >= 0 && mesh_load(&g_shadow_mesh, file) == 0);
        if (g_shadow_ok) mesh_rebase(&g_shadow_mesh, base);
        for (int t = 0; t < n; t++) g_s2_shadow[t] = shadow_new();
        /* the smoke (0x204af, 0x2073c): ASMOKE.3DX and A2SMOKE.3DX, a square that always
           faces the screen, eight of each a car in the game; here a pool the puffs that
           are on take turns with */
        g_s2_npuff = 0;
        if (base >= 0 && !getenv("OS_NOSMOKE")) {
            int ok = 1;
            for (int k = 0; k < 2 && ok; k++) {
                snprintf(file, sizeof file, "%s/%s.3DX", gfxdir, k ? "A2SMOKE" : "ASMOKE");
                ok = mesh_load(&g_s2_smoke[k], file) == 0;
                if (ok) mesh_rebase(&g_s2_smoke[k], base);
            }
            for (int i = 0; i < S2_PUFFS && ok && g_ncar < MAX_CARS; i++) {
                scene_object *o = &g_car[g_ncar++];
                memset(o, 0, sizeof *o);
                o->m = &g_s2_smoke[0];
                o->sprite = 1;
                o->over = 1;
                o->radius = 500.0f;
                /* The game draws its objects back to front, whole: a puff, whose middle is at
                   the wheel's foot, shows its lower half too, over the road in front of it
                   (the thicker half).  `over`: the road between it and the camera does not
                   hide that half, a crest or a wall does (over_ground). */
                o->z = -1.0e6f;
                g_s2_puff[g_s2_npuff++] = o;
            }
            /* the obstacles: their meshes are read when the game names them */
            g_s2_obst_base = base; g_s2_obst_kinds = 0; g_s2_nobst = 0;
            memset(g_s2_obst_was, 0, sizeof g_s2_obst_was); memset(g_s2_obst_is, 0, sizeof g_s2_obst_is);
            memset(g_s2_spark_was, 0, sizeof g_s2_spark_was); memset(g_s2_spark_is, 0, sizeof g_s2_spark_is);
            snprintf(g_s2_obst_dir, sizeof g_s2_obst_dir, "%s", gfxdir);
            for (int i = 0; i < S2_OBST && g_ncar < MAX_CARS - S2_SPARKS; i++) {
                scene_object *o = &g_car[g_ncar++];
                memset(o, 0, sizeof *o);
                o->phase = -1;
                o->radius = 300.0f;
                o->z = -1.0e6f;
                g_s2_obst[g_s2_nobst++] = o;
            }
            /* the track's animations */
            g_s2_nanimobj = 0;
            memset(g_s2_anim_was, 0, sizeof g_s2_anim_was); memset(g_s2_anim_is, 0, sizeof g_s2_anim_is);
            for (int i = 0; i < S2_ANIMS && g_ncar < MAX_CARS - S2_SPARKS; i++) {
                scene_object *o = &g_car[g_ncar++];
                memset(o, 0, sizeof *o);
                o->phase = -1;
                o->radius = 3000.0f;
                o->z = -1.0e6f;
                g_s2_anim[g_s2_nanimobj++] = o;
            }
            /* the sparks: AZTRI.3DX and AZ1TRI.3DX */
            g_s2_nspark = 0;
            for (int k = 0; k < 2 && ok; k++) {
                snprintf(file, sizeof file, "%s/%s.3DX", gfxdir, k ? "AZ1TRI" : "AZTRI");
                ok = mesh_load(&g_s2_tri[k], file) == 0;
                if (ok) mesh_rebase(&g_s2_tri[k], base);
            }
            for (int i = 0; i < S2_SPARKS && ok && g_ncar < MAX_CARS; i++) {
                scene_object *o = &g_car[g_ncar++];
                memset(o, 0, sizeof *o);
                o->m = &g_s2_tri[0];
                o->phase = -1;
                o->radius = 40.0f;
                o->z = -1.0e6f;
                g_s2_spark[g_s2_nspark++] = o;
            }
        }
    }
    if (getenv("OS_DIFF")) s2_level = atoi(getenv("OS_DIFF")) < 0 ? 0 : atoi(getenv("OS_DIFF")) > 2 ? 2 : atoi(getenv("OS_DIFF"));   /* 0 pro, 1 normal, 2 rookie (the port starts in rookie) */
    /* the sound: the game's own calls are answered by the port's mixer (s2_hooks.c) */
    s2_sound_start(track);
    volumes_apply(1.0f);
    if (os_once(2)) atexit(snd_close);
    if (!s2_race_start(track, g_s2_cond, n, team, model)) { fprintf(stderr, "screamer 2: the race did not start\n"); return; }
    g_s2_on = n;
    if (getenv("OS_S2MIRROR")) g_s2_mirror_on = atoi(getenv("OS_S2MIRROR")) != 0;
    s2_mirror_set(g_s2_mirror_on);
    g_s2_mirror_frame_ok = -1;
    g_s2_mir_was_ok = g_s2_mir_is_ok = 0;
    s2_hud_load();
    s2_race_read(); s2_race_read();
}

/*
 * HOW OFTEN THE GAME'S RACE FRAME IS RUN.  The game runs it once a picture, with the time the
 * last pictures took as its step (0x4ae9a: eased, never under 0.02 s nor over 0.2 s).  Its
 * arithmetic is tied to that: a car's place is whole numbers with the fraction CUT OFF every
 * frame (0x1c7f8), the corners are rebuilt from it (0x1d9fc) and the speed is measured from
 * them (0x1e529), while a wheel keeps 1 - grip x step of its sideways speed (0x19c61).  So
 * what is cut off -- up to a unit a frame, always towards the map's origin -- comes back as
 * speed, and the shorter the step the more of it: on level ground, facing 45 degrees, 528
 * units a second sideways at 50 frames a second, 124 at 25; facing away from the origin on
 * both axes the car does not leave the grid at all at 50.  The port therefore runs the frame
 * at the pace measured of the original (s2_hz below; it was 25 times a second first) and draws between the last
 * two (the game does the same for its replays: 0x16878's second argument).  The port's own.
 */
/* The original, as measured running in DOSBox at the speed of a Pentium of its day (NOTES2
   S2-5w): a picture every 0.0565 s, the step its routines get 0.062 -- the time the last
   pictures took, eased, times 1.1 (0x4ae9a) -- so its race goes a tenth faster than the clock
   on the wall.  The port: a frame of 0.062 every 0.0565 s (OS_S2HZ = frames a second, 17.7). */
static float s2_hz(void)
{
    static float hz;
    if (hz == 0.0f) {
        hz = getenv("OS_S2HZ") ? (float)atof(getenv("OS_S2HZ")) : 17.7f;
        if (hz < 5.0f) hz = 5.0f;
        if (hz > 50.0f) hz = 50.0f;
    }
    return hz;
}
static float s2_step(void) { return 1.1f / s2_hz(); }          /* the step of the game's routines */
static float s2_every(void) { return 1.0f / s2_hz(); }         /* seconds between two frames */

static s2_car_state    g_s2_was[S2_CARS], g_s2_is[S2_CARS];
static s2_camera_state g_s2_cam_was, g_s2_cam_is;

static void s2_race_read(void)
{
    for (int i = 0; i < g_s2_on; i++) { g_s2_was[i] = g_s2_is[i]; s2_car(i, &g_s2_is[i]); }
    {   /* the sparks and the obstacles, by the game's own record numbers */
        s2_spark sp[S2_SPARKS]; s2_obstacle ob[S2_OBST];
        const int ns = s2_sparks(sp, S2_SPARKS), no = s2_obstacles(ob, S2_OBST);
        memcpy(g_s2_spark_was, g_s2_spark_is, sizeof g_s2_spark_was);
        memcpy(g_s2_obst_was, g_s2_obst_is, sizeof g_s2_obst_was);
        memset(g_s2_spark_is, 0, sizeof g_s2_spark_is); memset(g_s2_obst_is, 0, sizeof g_s2_obst_is);
        for (int i = 0; i < ns; i++) if (sp[i].id >= 0 && sp[i].id < S2_SPARKS) { g_s2_spark_is[sp[i].id].on = 1; g_s2_spark_is[sp[i].id].s = sp[i]; }
        for (int i = 0; i < no; i++) if (ob[i].id >= 0 && ob[i].id < S2_OBST) { g_s2_obst_is[ob[i].id].on = 1; g_s2_obst_is[ob[i].id].s = ob[i]; }
        s2_anim an[40];
        const int na = s2_anims(an, 40);
        memcpy(g_s2_anim_was, g_s2_anim_is, sizeof g_s2_anim_was);
        memset(g_s2_anim_is, 0, sizeof g_s2_anim_is);
        for (int i = 0; i < na; i++) if (an[i].id >= 0 && an[i].id < 40) { g_s2_anim_is[an[i].id].on = 1; g_s2_anim_is[an[i].id].s = an[i]; }
    }
    g_s2_cam_was = g_s2_cam_is; s2_camera(&g_s2_cam_is);
    g_s2_mir_was = g_s2_mir_is; g_s2_mir_was_ok = g_s2_mir_is_ok;
    g_s2_mir_is_ok = s2_mirror(&g_s2_mir_is);
}

/* one frame of the game's race */
static void s2_race_step(void)
{
    if (s2_mode & 0x80) s2_attract_step();               /* (the attract race: the game drives every car) */
    if (getenv("OS_S2ROBOT")) {                          /* a test: the game's driver has the player's car for so many frames */
        static int f, said;
        s2_robot(f++ < atoi(getenv("OS_S2ROBOT")));
        if (!said && s2_laps_done(0) > 0) { said = 1; printf("s2: the player's car has done a lap at frame %d\n", f); }
    }
    s2_frame_times(s2_every());                          /* (the race's clock goes by the time itself, the routines by the eased step) */
    s2_frame(s2_step());
    s2_race_read();
    snd_render(s2_every());                              /* (only for OS_SNDWAV: the mix written to a file) */
    if (getenv("OS_S2LOG")) {                            /* a test: every frame, every car's place and speed */
        static int f;
        printf("f %d touch %d view %d shot %d cam %d %d %d", f++, s2_touched(), g_s2_cam_is.view, g_s2_cam_is.shot, g_s2_cam_is.x, g_s2_cam_is.y, g_s2_cam_is.height);
        for (int k = 0; k < g_s2_on; k++) printf(" | %d %d %.1f", g_s2_is[k].x, g_s2_is[k].y, (double)g_s2_is[k].speed);
        printf("\n");
    }
}

static float s2_half_turn(float a)
{
    while (a >  (float)M_PI) a -= 2 * (float)M_PI;
    while (a < -(float)M_PI) a += 2 * (float)M_PI;
    return a;
}
static float s2_between(float a, float b, float t) { return a + (b - a) * t; }
static float s2_angle_between(float a, float b, float t) { return a + s2_half_turn(b - a) * t; }

/* the cars where the game's routines have them, `t` of the way from the frame before to the
   last: a car's place is car +0x4d8 (x, height, y), its heading +0x4e8 with forward =
   (sin, -cos), which is the port's yaw as it is */
/* F12 in a race also writes shot_NNN.txt: every object that owns pixels of the picture, how
   many and where on the screen, and what it is -- a piece of the track (its model, record run,
   place, materials) or one of the race's own (a car, a shadow, a puff, a spark, an obstacle,
   moving scenery).  For finding what something on the screen is. */
static const char *s2_what(const scene_object *o, int *k)
{
    for (*k = 0; *k < S2_CARS; (*k)++) { if (o == g_s2_obj[*k]) return "car"; if (o == g_s2_shadow[*k]) return "car's shadow"; }
    for (*k = 0; *k < g_s2_npuff; (*k)++) if (o == g_s2_puff[*k]) return "smoke puff";
    for (*k = 0; *k < g_s2_nspark; (*k)++) if (o == g_s2_spark[*k]) return "spark";
    for (*k = 0; *k < g_s2_nobst; (*k)++) if (o == g_s2_obst[*k]) return "obstacle";
    for (*k = 0; *k < g_s2_nanimobj; (*k)++) if (o == g_s2_anim[*k]) return "moving scenery";
    *k = -1;
    return "other (the port's own)";
}
static void shot_report(const char *ppm, const camera *cam)
{
    char name[96];
    snprintf(name, sizeof name, "%.*s.txt", (int)(strlen(ppm) > 4 ? strlen(ppm) - 4 : strlen(ppm)), ppm);
    FILE *f = fopen(name, "w");
    if (!f || !g_scene) { if (f) fclose(f); return; }
    const int nid = g_scene->count + g_ncar;
    int *cnt = calloc((size_t)nid + 1, sizeof(int)), *box = malloc(sizeof(int) * 4 * ((size_t)nid + 1));
    if (!cnt || !box) { free(cnt); free(box); fclose(f); return; }
    for (int i = 0; i < nid; i++) { box[4 * i] = box[4 * i + 1] = 1 << 30; box[4 * i + 2] = box[4 * i + 3] = -1; }
    for (int y = 0; y < FB_H; y++)
        for (int x = 0; x < FB_W; x++) {
            const int id = idb[y * FB_W + x];
            if (id < 0 || id >= nid) continue;
            cnt[id]++;
            if (x < box[4 * id]) box[4 * id] = x; if (y < box[4 * id + 1]) box[4 * id + 1] = y;
            if (x > box[4 * id + 2]) box[4 * id + 2] = x; if (y > box[4 * id + 3]) box[4 * id + 3] = y;
        }
    fprintf(f, "picture %s, %dx%d; camera at %.0f %.0f height %.0f, heading %.3f, pitch %.3f\n", ppm, FB_W, FB_H, cam->x, cam->y, cam->z, cam->yaw, cam->pitch);
    if (g_s2_on && g_s2_obj[s2_player()]) fprintf(f, "the player's car at %.0f %.0f height %.0f\n", g_s2_obj[s2_player()]->x, g_s2_obj[s2_player()]->y, g_s2_obj[s2_player()]->z);
    fprintf(f, "pixels  screen box (x0 y0 x1 y1)  what\n");
    for (int id = 0; id < nid; id++) {
        if (!cnt[id]) continue;
        const scene_object *o = id < g_scene->count ? &g_scene->obj[id] : &g_car[id - g_scene->count];
        fprintf(f, "%6d  %3d %3d %3d %3d  ", cnt[id], box[4 * id], box[4 * id + 1], box[4 * id + 2], box[4 * id + 3]);
        if (id < g_scene->count) fprintf(f, "track piece V%d run %d", o->v, o->layer);
        else { int k; const char *w = s2_what(o, &k); fprintf(f, "%s %d", w, k); }
        fprintf(f, " at %.0f %.0f height %.0f, heading %.3f, pitch %.3f, roll %.3f", o->x, o->y, o->z, o->yaw, o->pitch, o->roll);
        if (o->m) {
            fprintf(f, "; %d faces, materials", o->m->nface);
            int seen = 0;
            for (int fi = 0; fi < o->m->nface && seen < 6; fi++) {
                const int mi = o->m->material[fi] / 16;
                int dup = 0;
                for (int fj = 0; fj < fi; fj++) if (o->m->material[fj] / 16 == mi) { dup = 1; break; }
                if (dup) continue;
                fprintf(f, " %d(routine %d)", mi, mi >= 0 && mi < MAT_MAX ? g_mat[mi].type : -1);
                seen++;
            }
        }
        fputc('\n', f);
    }
    free(cnt); free(box);
    fclose(f);
}
static void s2_race_show_cars(float t);
static void s2_race_show(float t)
{
    g_s2_show_t = t;
    s2_race_show_cars(t);
    s2_puffs_show(t);                                   /* (after the cars: some puffs go where their car is drawn) */
}
static void s2_race_show_cars(float t)
{
    for (int i = 0; i < g_s2_on; i++) {
        const s2_car_state *a = &g_s2_was[i], *b = &g_s2_is[i];
        scene_object *o = g_s2_obj[i];
        o->x = s2_between((float)a->x, (float)b->x, t); o->y = s2_between((float)a->y, (float)b->y, t);
        o->z = s2_between((float)a->height, (float)b->height, t);
        o->yaw = s2_angle_between(a->angle[1], b->angle[1], t);
        /* the game's is + nose up, the port's tilt() + tail up */
        o->pitch = -s2_angle_between(a->angle[0], b->angle[0], t);
        o->roll = s2_angle_between(a->angle[2], b->angle[2], t);
        o->phase = b->look;     /* (which picture of each texture: brake lights, wheels' blur, dents -- s2_car_state.look) */
        g_s2_pose[i].env = b->env;                      /* (the reflections' picture) */
        if (getenv("OS_S2LOOKFLAGS")) o->phase = (int)strtol(getenv("OS_S2LOOKFLAGS"), NULL, 16);   /* a test: those flags drawn */
        /* the shadow lies on the ground under the car, at the ground's own slope (0x1f4a8; the
           game also moves it 30 units away from the camera and 2 under the road for its
           back-to-front drawing -- the port's depth test does not need that) */
        if (g_s2_shadow[i]) {
            scene_object *sh = g_s2_shadow[i];
            sh->x = o->x; sh->y = o->y; sh->yaw = o->yaw;
            sh->z = s2_between((float)a->ground, (float)b->ground, t) + 4.0f;
            sh->pitch = -s2_angle_between(a->ground_pitch, b->ground_pitch, t);
            sh->roll = s2_angle_between(a->ground_roll, b->ground_roll, t);
        }
        /* the wheels and the body on its springs, as 0x16878 gives them to the engine */
        car_pose *p = &g_s2_pose[i];
        p->per_wheel = 1;
        memset(p->role, 0, sizeof p->role);
        for (int k = 0; k < 4; k++) if (b->body_block[k] >= 0 && b->body_block[k] < 16) p->role[b->body_block[k]] = 1;
        for (int w = 0; w < 4; w++) {
            if (b->wheel_block[w][0] >= 0 && b->wheel_block[w][0] < 16) p->role[b->wheel_block[w][0]] = (unsigned char)(0x10 + w);
            if (b->wheel_block[w][1] >= 0 && b->wheel_block[w][1] < 16) p->role[b->wheel_block[w][1]] = (unsigned char)(0x20 + w);
        }
        /* (the game's turn is + towards the car's +X, as its heading; the port's wheel turns its
           front edge towards -X) */
        p->steer = -s2_angle_between(a->steer_shown, b->steer_shown, t);
        p->body_pitch = s2_angle_between(a->body_pitch, b->body_pitch, t);
        p->body_roll = s2_angle_between(a->body_roll, b->body_roll, t);
        p->body_drop = -s2_between((float)a->body_lift, (float)b->body_lift, t);
        for (int w = 0; w < 4; w++) {
            p->wheel_spin[w] = s2_angle_between(a->wheel_spin[w], b->wheel_spin[w], t);
            p->wheel_in[w] = s2_between((float)a->wheel_in[w], (float)b->wheel_in[w], t);
        }
        if (!b->shown) {                                /* (time attack's ghost, none to show) */
            o->z = -1.0e7f;
            if (g_s2_shadow[i]) g_s2_shadow[i]->z = -1.0e7f;
        }
    }
}

/* the view as the game's camera routine has it (s2_hooks.c s2_view): its place, and its
   three angles -- the heading is a car's (forward = (sin, -cos)), down is negative, and
   the lean is the car's own, the other way round from the port's camera (checked on a
   bank: the ground under the car comes out level in view 2).  From inside the car (views
   2 and 6) the car is not drawn, as in the game (0x2b6b4 takes it off the list for the
   picture). */
static const int g_s2_views[5] = { 2, 6, 4, 3, 1 };          /* F1..F4, the game's order (0x132b4) */
static int g_s2_view = 0;                                    /* (a race starts in view 2: s2_race_start) */
static void s2_camera_show(camera *cam, float t)
{
    const s2_camera_state *a = &g_s2_cam_was, *b = &g_s2_cam_is;
    /* a change of view is at once, and so is a cut from one trackside camera to the next */
    if (a->view != b->view || (b->view == 1 && a->shot != b->shot)) t = 1.0f;
    /* the night's and the fog's distances are the view's (0x12d31: nearer in view 1 at night) */
    if (g_fade_on && g_s2_cond && b->fade_far > b->fade_near) { g_fade_near = (float)b->fade_near; g_fade_far = (float)b->fade_far; }
    cam->x = s2_between((float)a->x, (float)b->x, t); cam->y = s2_between((float)a->y, (float)b->y, t);
    cam->z = s2_between((float)a->height, (float)b->height, t);
    const float h = s2_angle_between(a->angle[1], b->angle[1], t);
    cam->yaw = atan2f(-cosf(h), sinf(h));
    cam->pitch = s2_half_turn(s2_angle_between(a->angle[0], b->angle[0], t));
    cam->roll = -s2_half_turn(s2_angle_between(a->angle[2], b->angle[2], t));
    g_hide = (b->view == 2 || b->view == 6) ? g_s2_obj[s2_player()] : NULL;
    /* the scene's rows are the view's, the eye line 0.65 of them (0x12dff -> 0x4f6c4): view
       6 draws only down to the dashboard */
    if (s2_screen_hi()) { if (b->rows > 0 && b->rows <= 400) { g_vh = b->rows; g_cy = EYE_LINE * (float)g_vh; } }   /* (STARTH: the rows are the screen's own) */
    else if (b->rows > 0 && b->rows <= 200) { g_vh = b->rows * SY + (b->rows == 200 ? g_tall : 0); g_cy = EYE_LINE * (float)g_vh; }
}

/*
 * THE MIRROR (s2_hooks.c s2_mirror has what the game does).  The port draws the mirror's
 * scene first, into the top rows of the picture, keeps it squeezed and turned round, draws
 * the picture itself, and then lays the kept strip and its frame over it.  R turns it on and
 * off (OS_S2MIRROR=1 starts with it on); the game has it as an option of its menu.
 */
static int g_s2_mirror_have, g_s2_mirror_w, g_s2_mirror_h;
static uint32_t g_s2_mirror_px[(FB_MAXW / 2) * 100];
static int16_t g_s2_mirror_frame[FB_MAXW * FB_MAXH];

static void s2_render(const camera *cam, const sky *sk)
{
    g_s2_mirror_have = 0;
    if (g_s2_on && g_s2_mirror_on && g_s2_mir_is_ok) {
        const s2_camera_state m = g_s2_mir_is, *b = &g_s2_mir_is, *a = g_s2_mir_was_ok ? &g_s2_mir_was : b;
        const float t = g_s2_show_t;
        camera mc = *cam;
        const scene_object *hid = g_hide;
        const int vh = g_vh; const float cy = g_cy;
        float shadow_z = 0.0f;
        const float h = s2_angle_between(a->angle[1], b->angle[1], t);
        mc.x = s2_between((float)a->x, (float)b->x, t); mc.y = s2_between((float)a->y, (float)b->y, t);
        mc.z = s2_between((float)a->height, (float)b->height, t);
        mc.yaw = atan2f(-cosf(h), sinf(h));
        mc.pitch = s2_half_turn(s2_angle_between(a->angle[0], b->angle[0], t));
        mc.roll = -s2_half_turn(s2_angle_between(a->angle[2], b->angle[2], t));
        g_vh = s2_screen_hi() ? m.rows : m.rows * SY; g_cy = EYE_LINE * (float)g_vh;
        g_hide = g_s2_obj[s2_player()];                       /* (0x2b6b4: the car and its shadow are not in it) */
        if (g_s2_shadow[0]) { shadow_z = g_s2_shadow[0]->z; g_s2_shadow[0]->z = -1.0e6f; }
        render_frame(&mc, sk);
        g_s2_mirror_w = FB_W / 2; g_s2_mirror_h = g_vh;
        for (int y = 0; y < g_s2_mirror_h && y < 100; y++)
            for (int x = 0; x < g_s2_mirror_w; x++)
                g_s2_mirror_px[y * (FB_MAXW / 2) + x] = fb[y * FB_W + (FB_W - 2 - 2 * x)];
        g_s2_mirror_have = 1;
        if (g_s2_shadow[0]) g_s2_shadow[0]->z = shadow_z;
        g_hide = hid; g_vh = vh; g_cy = cy;
    }
    render_frame(cam, sk);
    if (g_s2_mirror_have) {
        const int x0 = 80 * g_s, y0 = 14 * SY;
        for (int y = 0; y < g_s2_mirror_h && y0 + y < FB_H; y++)
            for (int x = 0; x < g_s2_mirror_w && x0 + x < FB_W; x++)
                fb[(y0 + y) * FB_W + x0 + x] = g_s2_mirror_px[y * (FB_MAXW / 2) + x];
        if (g_s2_mirror_frame_ok < 0) { g_s2_mirror_frame_ok = pkc_load(s2_screen_hi() ? "build/dec/GAME1/MIRROR_H.PKC" : "build/dec/GAME1/MIRROR_L.PKC", g_s2_mirror_frame); g_pkc_loads++; }
        if (g_s2_mirror_frame_ok > 0) { LAY_TOP(); pkc_draw(g_s2_mirror_frame); }
    }
}

/* the path sample nearest a point -- "where on the lap am I?" */
static int path_nearest(const path *r, float x, float y)
{
    int bi = 0;
    float best = 1e30f;
    for (int i = 0; i < r->count; i++) {
        float dx = r->p[i][0] - x, dy = r->p[i][1] - y;
        float d = dx * dx + dy * dy;
        if (d < best) { best = d; bi = i; }
    }
    return bi;
}

/* ------------------------------------------------------------ the player */

/*
 * The player's car.  drive.c is the game's own model of how it moves; what
 * follows is the part that ties it to the track, from the game as well:
 *
 *   ground      player_contact(): the four wheels on the drivable surface
 *               (0x11a30, 0x16698), found by track_under();
 *   walls       player_walls(): the body's four corners against the track's
 *               own wall segments (0x11e74, 0x11f98, 0x16b88, 0x16c90);
 *   camera      the view table's routines (0x19aa0...), see g_views.
 */
#define CAR_SPECS_MAX  16

static car_state     g_pc;
/* the chase camera's own eased heading / pitch / lean / height (see chase_step) */
static struct { float yaw, pitch, roll, height; int init; } g_chase;
static car_spec      g_spec[CAR_SPECS_MAX];
static int           g_nspec, g_spec_now;
static scene_object *g_pobj;                 /* the player's car as a drawn object */
static int           g_pat;                  /* path sample the car is at */
static int           g_wall_hits;
/* the body's four corners in the car's own frame: x to the right, z forward.
   0 front-left, 1 rear-left, 2 front-right, 3 rear-right -- the game's order,
   which the wall response depends on */
static float         g_corner[4][2];

/*
 * MAPn.MRK -- the lap's MARKERS.  int32 count, then 32-byte records:
 *
 *     +0x00 int   radius     you have reached the marker when this close
 *     +0x04 int   kind       (1 inside track 1's tunnel; the game switches the
 *                             engine sound on it)
 *     +0x08 float PACE       the opponents' speed in this stretch, as a
 *                            fraction of their top speed (0x1ced0)
 *     +0x0C float SLIP       how readily the car skids from here on
 *     +0x10 int   flags      2 = the start/finish line
 *     +0x14 int   x, +0x18 0, +0x1C int y
 *
 * The game (0x1820c) keeps the index of the marker last reached and looks only
 * at that one and its two neighbours; the marker's SLIP becomes the factor in
 * the cornering-force sum (player +0x670) until the next marker is reached.
 * On track 1 it is 0 on every straight and 0.98 / 1.0 / 0.8 / 0.6 at the four
 * bends -- so the car can only be made to skid where the track's authors
 * meant it to.  A lap counts (0x19490) when the finish marker is reached with
 * every other marker passed.
 */
typedef struct { float radius, slip, pace, x, y; int kind, flags; } marker;
#define MARKERS_MAX 64
static marker g_mark[MARKERS_MAX];
static int    g_nmark, g_mark_at, g_mark_seen[MARKERS_MAX], g_mark_count;
static int    g_laps;
static float  g_progress;      /* player +0x680: how far round the race it is, see marker_progress() */
static float  g_lap_time, g_last_lap, g_best_lap;

/*
 * RECORDS, THE PLAYER'S NAME, WHAT IS UNLOCKED (build/decomp/records.c).
 * The game keeps these in CONFIG\CHOICE.DAT (0x460 bytes at 0x203c0); the
 * port keeps its own text file, openscreamer.records, and never touches the
 * game's.  Per track (0x50 bytes from 0x2042c and from 0x2060c): five best
 * laps and five best race totals, each with a three-letter name; the
 * defaults are 180 s and 900 s under the name "GRA" (0x1bbb8).
 * 0x2e5e4 enters a result: only after a normal or a championship race, never
 * one that ran out of time or was left (flags 0x1020); the lap always, the
 * total only for a THREE-lap race.  A time goes in ahead of the first entry
 * it is not slower than.
 * level (0x20814): tracks 1..level+3 can be chosen; seen (0x20818) is the
 * level the player has been shown ("NEW TRACK AVAILABLE" blinks until then);
 * bullet (0x207ec): the thirteenth car and the fourth difficulty.
 */
typedef struct { float lap[5], total[5]; char lap_name[5][4], total_name[5][4]; int rpl[5]; } track_records;
static struct { char name[4]; int level, seen, bullet, intro; track_records t[6]; } g_save;
static int   g_reverse;
static int   g_dev;                /* -d: the developer's keys and modes are on */
/* 0x21e50: every time a race is set up -- the first time, PLAY AGAIN, the
   replay, the intro -- picture and sound come up from nothing in half a
   second (0x18e34 from 0 to 1 at 2 a second); 0x21d8c takes them down the
   same way when the race is left */
static float g_fade_in = 0.0f;
static int   g_intro;              /* this run is the intro: a race with nobody in it (OS_INTRO) */
/* the option word 0x1d270, as the cheats leave it (OS_CHEAT): 1 / 2 the
   second / third set of props, 4 the countdown stands still, 0x10 the picture
   is mirrored (and the steering with it), 0x20 it is upside down */
static int   g_cheat;
static float g_record_lap, g_record_shown;      /* 0x1d268 and the text at 0x1f88 */
static float g_res_lap, g_res_total;            /* a finished race waiting to be entered */
static int   g_res_pending, g_res_laps;

static void save_load(void)
{
    memset(&g_save, 0, sizeof g_save);
    strcpy(g_save.name, "GRA");
    for (int t = 0; t < 6; t++)
        for (int i = 0; i < 5; i++) {
            g_save.t[t].lap[i] = 180.0f; g_save.t[t].total[i] = 900.0f;
            strcpy(g_save.t[t].lap_name[i], "GRA"); strcpy(g_save.t[t].total_name[i], "GRA");
        }
    FILE *f = fopen("openscreamer.records", "r");
    if (!f) return;
    char name[8], line[128];
    int level, seen, bullet;
    if (fgets(line, sizeof line, f) && sscanf(line, "%3s %d %d %d %d", name, &level, &seen, &bullet, &g_save.intro) >= 4) {
        g_save.intro = (g_save.intro % 6 + 6) % 6;
        snprintf(g_save.name, sizeof g_save.name, "%.3s", name);
        g_save.level = level; g_save.seen = seen; g_save.bullet = bullet;
        for (int t = 0; t < 6; t++)
            for (int i = 0; i < 5; i++) {
                float lap, total; char ln[8], tn[8];
                int rpl = 0;                     /* the fifth number came later: older files have four */
                if (!fgets(line, sizeof line, f) ||
                    sscanf(line, "%f %3s %f %3s %d", &lap, ln, &total, tn, &rpl) < 4) { fclose(f); return; }
                g_save.t[t].lap[i] = lap; g_save.t[t].total[i] = total; g_save.t[t].rpl[i] = rpl;
                snprintf(g_save.t[t].lap_name[i], 4, "%.3s", ln);
                snprintf(g_save.t[t].total_name[i], 4, "%.3s", tn);
            }
    }
    fclose(f);
}

static void save_write(void)
{
    FILE *f = fopen("openscreamer.records", "w");
    if (!f) return;
    fprintf(f, "%s %d %d %d %d\n", g_save.name, g_save.level, g_save.seen, g_save.bullet, g_save.intro);
    for (int t = 0; t < 6; t++)
        for (int i = 0; i < 5; i++)
            fprintf(f, "%.4f %s %.4f %s %d\n", g_save.t[t].lap[i], g_save.t[t].lap_name[i],
                    g_save.t[t].total[i], g_save.t[t].total_name[i], g_save.t[t].rpl[i]);
    fclose(f);
}

static int record_put(float *time, char name[][4], int *rpl, float value)       /* 0x2e5e4 */
{
    int at = 0;
    while (at < 5 && !(value <= time[at])) at++;
    if (at >= 5) return -1;
    for (int i = 3; i >= at; i--) {
        time[i + 1] = time[i]; memcpy(name[i + 1], name[i], 4);
        if (rpl) rpl[i + 1] = rpl[i];
    }
    time[at] = value;
    if (rpl) rpl[at] = 0;
    snprintf(name[at], 4, "%.3s", g_save.name);
    return at;
}

static int replay_write(const char *file, int track);
static void replay_name(char *file, size_t n, int track, int number)
{
    snprintf(file, n, "openscreamer.replays/replay%d%d.rpl", track, number);
}

static void records_enter(int track)
{
    if (!g_res_pending || track < 1 || track > 6) return;
    g_res_pending = 0;
    save_load();                                 /* the menu may have changed the name since */
    track_records *r = &g_save.t[track - 1];
    record_put(r->lap, r->lap_name, NULL, g_res_lap);
    if (g_res_laps == 3) {
        int at = record_put(r->total, r->total_name, r->rpl, g_res_total);
        /* 0x2e9bd: a record total keeps its replay.  It gets the lowest number
           no other total of the track has (the game renames TEMP.RPL to
           REPLAY<track><number>.RPL), shown as an "R" beside the time */
        if (at >= 0) {
            int number = 1;
            for (int i = 0; i < 5; i++)
                if (r->rpl[i] == number) { number++; i = -1; }
            char file[128];
            replay_name(file, sizeof file, track, number);
            if (replay_write(file, track)) r->rpl[at] = number;
        }
    }
    save_write();
}
/*
 * The clocks (0x18a04, 0x19490, 0x195fc, 0x19668; build/decomp/timers.c).
 * Player +0x78c = total time and +0x794 = lap time, which only start to run
 * the first time the finish line is crossed (+0x784) -- the grid is behind
 * it.  Player +0x790 is the COUNTDOWN in the middle of the screen: 15 s at the
 * start, running from the green light, and at the line and at every
 * checkpoint -- a marker with flag 2 -- it becomes  what is left / 5 +
 * bonus_time  (TRACK_DATA in MAPnA.DTR).  At zero the engine stops pulling (flag 0x40 at
 * 0x18e19, tested by the engine 0x18da0) and the race is over once the car
 * has rolled to a stop.
 */
static float  g_total_time, g_timer = 15.0f, g_bonus_time = 50.0f;
static int    g_clock_on, g_time_out, g_finished, g_race_on;
static int    g_final_rank, g_game_over;
static struct { int ready; float h[4], ground; float hc[4], cam_pitch, cam_roll; } g_contact;
static int    g_total_laps = 3;        /* the game's option at 0x1d2ac, + 1 */
/*
 * THE SPECIAL MODES (the game-mode word at 0x1d25c: 1 normal, 0x20 cone
 * carnage, 0x40 slalom; build/decomp/modes.c).  Both: the player alone on the
 * track (set-up 0x1311c / 0x1309c makes no opponents, and the race frames
 * 0x20350 / 0x203b4 update only the player), ONE lap (0x2a216 passes 0 for
 * the lap option), from the grid place of the last opponent (0x13ded), and the
 * time given at the line and the checkpoints is TRACK_DATA's fourth number,
 * "carnage", instead of bonus_time (0x1962f).
 *   cone carnage  props from MAPnCNG.SPL; every frame in which a new prop was
 *                 knocked over adds one second (0x18b50); PERFECT = all hit.
 *   slalom        props from MAPnSLA.SPL, gates from MAPnSLA.MRK (32 bytes:
 *                 radius, .., +0x10 seconds, +0x14 x, +0x1c y); driving
 *                 through a gate once adds its seconds and plays DOOR
 *                 (0x18b98); PERFECT = every gate and not one prop touched.
 * At the end (0x24204 / 0x241ac): "YOU MADE IT!" at y 90, "PERFECT!" at 120.
 */
#define MODE_CONES  0x20
#define MODE_SLALOM 0x40
#define MODE_ATTACK 4
static int    g_mode = 1, g_perfect, g_attack_won;
static int    g_replay, g_replay_seen, g_dialog, g_rpl_n, g_rpl_at;
static float  g_end_clock, g_rpl_stray;
static float  g_carnage_time = 20.0f;
#define GATES_MAX 64
static struct { float radius, x, y, seconds; int done; } g_gate[GATES_MAX];
static int    g_ngate, g_gates_done;
static void   attack_finish(void);
static int    props_total(void);
static int    props_hit(void);

static float line_bonus(void)                    /* 0x195fc */
{
    return (g_mode & (MODE_CONES | MODE_SLALOM)) ? g_carnage_time : g_bonus_time;
}

static void gates_load(const char *data, int track)
{
    char file[600];
    snprintf(file, sizeof file, "%s/MAP%dSLA.MRK", data, track);
    FILE *f = fopen(file, "rb");
    g_ngate = 0;
    if (!f) return;
    int32_t n = 0, r[8];
    if (fread(&n, 4, 1, f) == 1)
        for (int i = 0; i < n && i < GATES_MAX; i++) {
            if (fread(r, 4, 8, f) != 8) break;
            g_gate[g_ngate].radius = (float)r[0];
            if (getenv("OS_GATEWIDE")) g_gate[g_ngate].radius *= 2.0f;      /* testing: the robot does not slalom */
            g_gate[g_ngate].seconds = (float)r[4];
            g_gate[g_ngate].x = (float)r[5];
            g_gate[g_ngate].y = (float)r[7];
            g_gate[g_ngate].done = 0;
            g_ngate++;
        }
    fclose(f);
    printf("slalom: %d gates\n", g_ngate);
}

static void comment(int ev, int mode, float delay);
static void comment_rearm(int ev);

static void markers_load(const char *file)
{
    g_nmark = 0;
    FILE *f = fopen(file, "rb");
    if (!f) { fprintf(stderr, "note: no markers at %s\n", file); return; }
    int32_t n = 0;
    if (fread(&n, 4, 1, f) == 1 && n > 0 && n <= MARKERS_MAX)
        for (int i = 0; i < n; i++) {
            int32_t rec[8];
            float fl[8];
            if (fread(rec, 4, 8, f) != 8) break;
            memcpy(fl, rec, sizeof fl);
            marker *m = &g_mark[g_nmark++];
            m->radius = (float)rec[0];
            m->kind   = rec[1];
            m->slip   = fl[3];
            m->pace   = fl[2];
            m->flags  = rec[4];
            m->x      = (float)rec[5];
            m->y      = (float)rec[7];
        }
    fclose(f);
    printf("markers: %d", g_nmark);
    for (int i = 0; i < g_nmark; i++) printf("%s%.2f", i ? ", " : "  slip ", g_mark[i].slip);
    printf("\n");
}

/*
 * HOW FAR A CAR HAS GOT (0x1820c's last lines for the player, 0x1db84's for an
 * opponent): laps x 100 + the number of the marker last reached + how far
 * past it the car is, as its distance from that marker over the distance
 * from that marker to the next (both cut to whole twentieths of a unit).
 * The running order (0x20418), the opponents' catch-up and "lapped" all go
 * by this number.
 */
static float marker_progress(int at, int laps, float x, float y)
{
    if (!g_nmark) return 0.0f;
    const marker *a = &g_mark[at], *b = &g_mark[(at + 1) % g_nmark];
    int span = (int)(sqrtf((b->x - a->x) * (b->x - a->x) + (b->y - a->y) * (b->y - a->y)) * 0.05f);
    int gone = (int)(sqrtf((x - a->x) * (x - a->x) + (y - a->y) * (y - a->y)) * 0.05f);
    return (span ? (float)gone / (float)span : 0.0f) + (float)at + (float)(laps * 100);
}

static void markers_reset(void)
{
    memset(g_mark_seen, 0, sizeof g_mark_seen);
    g_progress = 0.0f;
    g_mark_at = 0;
    g_mark_count = 0;
    g_lap_time = 0.0f;
}

/* 0x1820c + 0x19490, once per physics step */
static void markers_step(car_state *c)
{
    if (g_race_on && !g_finished) {
        if (g_clock_on) { g_lap_time += DRIVE_DT; g_total_time += DRIVE_DT; }
        if (!(g_cheat & 4)) g_timer -= DRIVE_DT;                 /* 0x18ad0 */
        g_time_out = (g_timer < 0.0f);
        if (g_time_out) g_timer = 0.0f;
    }
    if (g_race_on && g_mode == MODE_CONES) {                 /* 0x18b50 */
        static int paid;
        if (props_hit() < paid) paid = 0;
        if (paid < props_hit()) {
            g_timer += 1.0f; paid = props_hit();
            if (getenv("OS_MODELOG")) printf("prop %d of %d down: +1 s, %.1f left\n", paid, props_total(), g_timer);
        }
    }
    if (g_race_on && g_mode == MODE_SLALOM)                  /* 0x18b98 */
        for (int i = 0; i < g_ngate; i++) {
            float dx = c->x - g_gate[i].x, dy = c->y - g_gate[i].y, rad = g_gate[i].radius;
            if (fabsf(dx) >= rad || fabsf(dy) >= rad || sqrtf(dx * dx + dy * dy) > rad) continue;
            if (!g_gate[i].done) {
                g_gate[i].done = 1;
                g_gates_done++;
                g_timer += g_gate[i].seconds;
                snd_play(SND_DOOR);
                if (getenv("OS_MODELOG")) printf("gate %d of %d: +%.0f s, %.1f left\n", i, g_ngate, g_gate[i].seconds, g_timer);
            }
            break;
        }
    if (g_nmark == 0) return;
    for (int k = -1; k <= 1; k++) {
        int i = ((g_mark_at + k) % g_nmark + g_nmark) % g_nmark;
        const marker *m = &g_mark[i];
        float dx = c->x - m->x, dy = c->y - m->y;
        if (fabsf(dx) >= m->radius || fabsf(dy) >= m->radius) continue;
        if (sqrtf(dx * dx + dy * dy) > m->radius) continue;
        g_mark_at = i;
        c->surface = m->slip;
        if (i == 0 && !g_clock_on && g_race_on) {             /* the start: clocks on */
            g_clock_on = 1;
            g_lap_time = 0.0f;
            g_timer = g_timer / 5.0f + line_bonus();
            comment_rearm(22);
        } else if (i == 0 && g_mark_count >= g_nmark - 1) {   /* across the line */
            if (g_laps + 1 >= g_total_laps) {
                g_finished = 1;
                /* 0x23fbc / 0x23f68 */
                if (g_mode == MODE_CONES)  g_perfect = props_total() == props_hit();
                if (g_mode == MODE_SLALOM) g_perfect = g_ngate == g_gates_done && props_hit() == 0;
                attack_finish();
            }
            else if (!g_finished) { g_timer = g_timer / 5.0f + line_bonus(); comment_rearm(22); }
            g_laps++;
            g_last_lap = g_lap_time;
            {
                /* 0x19668: "new lap record" for a best lap, then the lap call:
                   recording 2 going into the last lap, else 1 after a best lap, else 0 */
                int best = (g_best_lap == 0.0f || g_last_lap < g_best_lap);
                /* 0x196ea: a lap under the RECORD (0x1d268) -- the record on
                   the screen changes and "new lap record" is said */
                if (g_last_lap < g_record_lap) {
                    g_record_lap = g_record_shown = g_last_lap;
                    comment(7, 3, 0.2f);
                }
                if (!g_finished) comment(5, g_laps + 1 == g_total_laps ? 2 : best ? 1 : 0, 0.0f);
            }
            if (g_best_lap == 0.0f || g_last_lap < g_best_lap) g_best_lap = g_last_lap;
            if (g_finished && !g_replay) {       /* 0x20eb8: the race's total and best lap are handed back */
                g_record_lap = g_record_shown = g_best_lap;
                if (g_mode == 1) {
                    g_res_lap = g_best_lap; g_res_total = g_total_time;
                    g_res_laps = g_total_laps; g_res_pending = 1;
                }
            }
            printf("lap %d: %.2f s\n", g_laps, g_last_lap);
            fflush(stdout);
            memset(g_mark_seen, 0, sizeof g_mark_seen);
            g_mark_count = 0;
            g_lap_time = 0.0f;
        }
        if (i != 0 && !g_mark_seen[i]) {
            g_mark_seen[i] = 1; g_mark_count++;
            /* a CHECKPOINT: any other marker with flag 2 (tracks 2-6 have one
               to three of them) gives the bonus as well */
            if ((m->flags & 2) && g_clock_on && !g_finished) {
                /* 0x19490: the checkpoint call goes by the time that was left:
                   under 3 s recording 2, up to 15 s recording 0, more recording 1 */
                comment(4, g_timer < 3.0f ? 2 : g_timer <= 15.0f ? 0 : 1, 0.0f);
                g_timer = g_timer / 5.0f + line_bonus();
                comment_rearm(22);
            }
        }
    }
    /* 0x19490's last lines: only once the marker it is at has been passed
       properly (the line itself: once the clocks run) */
    if (g_mark_at == 0 ? g_clock_on : g_mark_seen[g_mark_at])
        g_progress = marker_progress(g_mark_at, g_laps, c->x, c->y);
}

/* the G overlay: the game's wall segments as dots in the world */
static void guides_build(const path *r)
{
    static float guide[SCENE_MAX_WALL * 4][3];
    int n = 0;
    for (int i = 0; g_scene && i < g_scene->nwall; i++) {
        const scene_wall *w = &g_scene->wall[i];
        float z = r->p[path_nearest(r, w->x, w->y)][2] + 80.0f;
        for (int k = 0; k < 4; k++) {
            float t = (float)k / 4.0f;
            guide[n][0] = w->x1 + (w->x2 - w->x1) * t;
            guide[n][1] = w->y1 + (w->y2 - w->y1) * t;
            guide[n][2] = z;
            n++;
        }
    }
    g_guide = guide;
    g_guide_n = n;
}

/* which sample is the car at?  look near the last answer first */
static int path_track(const path *r, float x, float y, int from)
{
    int bi = from;
    float best = 1e30f;
    for (int k = -40; k <= 40; k++) {
        int i = ((from + k) % r->count + r->count) % r->count;
        float dx = r->p[i][0] - x, dy = r->p[i][1] - y;
        float d = dx * dx + dy * dy;
        if (d < best) { best = d; bi = i; }
    }
    if (best > 6000.0f * 6000.0f) bi = path_nearest(r, x, y);
    return bi;
}

/*
 * WHEELS AND SHADOW (0x15c0c, once a frame for the player's car).
 *
 * Each of the car's blocks has a record of three angles and a position, and
 * the game turns the wheel blocks there: about the axle by speed / 15 of the
 * engine's 2048-a-turn units per FRAME -- at the game's frame rate that is the
 * wheel rolling without slipping, which is what the port does -- and the two
 * front wheels about the vertical by  steer / (steer limit x 1.5)  radians:
 * always 38 degrees at full lock, however little the car really steers at
 * speed.  (The same routine leans the body blocks on the suspension, +0x4e0
 * and +0x4e8: suspension_step.)
 *
 * The shadow is SHADOW.3DX, one see-through rectangle (material routine 224:
 * it darkens what is under it through MAPn.SMG), a separate object that is
 * given the car's place, heading and tilt and the height of the ground.  The
 * game draws back to front and so puts it 2 units under the road; the port
 * has a depth buffer, so it lies on the road and wins ties instead.
 */
static car_pose      g_ppose;
static scene_object *g_pshadow;

static scene_object *shadow_new(void)
{
    if (!g_shadow_ok || g_ncar >= MAX_CARS || getenv("OS_NOSHADOWS")) return NULL;
    scene_object *s = &g_car[g_ncar++];
    memset(s, 0, sizeof *s);
    s->m = &g_shadow_mesh;
    s->radius = 1200.0f;
    s->phase = -1;
    s->zbias = 1.01f;
    return s;
}

static void shadow_follow(scene_object *s, const scene_object *car)
{
    if (!s || !car) return;
    s->x = car->x; s->y = car->y; s->z = car->z + 4.0f;
    s->yaw = car->yaw; s->pitch = car->pitch; s->roll = car->roll;
}

/*
 * THE BODY ON ITS SUSPENSION (0x168a4, drawn by 0x15c0c).  Each wheel has a
 * deflection, at most 18 units either way, that is kicked by the car's
 * change of speed (front one way, rear three times as much the other) and by
 * how hard it is cornering (left against right), swings at 2 Hz and dies
 * away.  The four body blocks are then dropped by the average, pitched by
 * twice atan((front right - rear right) / wheelbase) and rolled by four
 * times atan((front left - front right) / track): the nose dips under
 * braking, lifts under power, and the body leans in the bends.
 */
static struct { float phase, lat, lng; int defl; } g_susp[4];
static float g_susp_prev_speed;

static void suspension_step(car_pose *w, const car_state *c, const mesh *mm, float dt)
{
    /* the change of speed over one GAME frame (1/30 s), not one 1/50 s step */
    float a = (c->speed - g_susp_prev_speed) * 0.00175f * (50.0f / 30.0f);
    float b = (c->turn / dt) * c->speed * c->speed * 1.5e-9f;
    g_susp_prev_speed = c->speed;
    for (int n = 0; n < 4; n++) {                 /* front-left, rear-left, front-right, rear-right */
        /* the game divides these by the FRAME time, so its cars leaned more
           on a faster PC; the port takes a 30 frames/s machine (as for the
           props), not its own 50 steps a second */
        const float frame = 1.0f / 30.0f;
        float lng = ((n & 1) ? a * 3.0f : -a) / frame, lat = ((n & 2) ? b : -b) / frame;
        if (fabsf(g_susp[n].lat) < fabsf(lat)) { g_susp[n].phase = 0.0f; g_susp[n].lat = lat; }
        if (fabsf(g_susp[n].lng) < fabsf(lng)) {
            g_susp[n].phase = 0.0f;
            g_susp[n].lng = (g_susp[n].lng - lng) * drive_damp(0x3d) + lng;
        }
        int d = (int)(cosf(g_susp[n].phase * 12.566371f + 3.1415927f) * (g_susp[n].lat + g_susp[n].lng));
        g_susp[n].defl = d > 18 ? 18 : d < -18 ? -18 : d;
        g_susp[n].lat *= drive_damp(61);
        if (fabsf(g_susp[n].lat) < 2.0f) g_susp[n].lat = 0.0f;
        g_susp[n].lng *= drive_damp(71);
        if (fabsf(g_susp[n].lng) < 2.0f) g_susp[n].lng = 0.0f;
        g_susp[n].phase += dt;
    }
    float track = 2.0f * (mm->track_half > 0.0f ? mm->track_half : 226.0f);
    float wb = mm->wheelbase > 0.0f ? mm->wheelbase : 935.0f;
    w->body_drop  = (float)((g_susp[0].defl + g_susp[1].defl + g_susp[2].defl + g_susp[3].defl) / 4);
    /* the game's track width (+0x2e4) is  -(front-left x - front-right x),
       and the model's +X is the car's left, so it is NEGATIVE: the two minus
       signs cancel.  (First port had one of them, and the car leaned into
       the bend like a motorbike -- Danik.) */
    w->body_roll  = atanf((float)(g_susp[0].defl - g_susp[2].defl) / track) * 4.0f;
    w->body_pitch = atanf((float)(g_susp[2].defl - g_susp[3].defl) / wb) * 2.0f;
}

static void wheels_step(car_pose *w, float speed, float steer_part, float dt)
{
    w->env_scroll = fmodf(w->env_scroll + speed * dt * (w->env ? 0.03f : 0.002f), 256.0f);
    w->spin -= speed * dt / 108.6f;                 /* = speed x 3 units a second */
    w->spin = fmodf(w->spin, 6.2831853f);
    w->steer = steer_part / 1.5f;
}

/*
 * TYRE SMOKE (0x19818 starts a puff, 0x19970 moves them; build/decomp/smoke.c).
 *
 * A car has eight puffs (objects made from SMOKE.3DX, one upright rectangle
 * with a 6-frame see-through texture).  While the car is skidding -- wheels
 * locked, sliding, the body more than 0.23 rad off its heading, spinning, or
 * spinning its wheels off the line -- every 0.05 s one puff starts at each
 * REAR wheel, up to 357 units outwards and 357 behind (by the car's place).  A puff plays
 * its texture's frames once in 2/3 s (its record +0x20 gains 1.5 x 65535 a
 * second) and all that time it is carried along with the car, turning with it.
 *
 * A puff always faces the screen: 0x118a4 sets bit 0x8000 of its object
 * record (+0x04) when it is made, and the engine turns such an object by its
 * own angles LESS the camera's (0x8abc7 -> 0x8b26c -> 0x4bca0).  Only the
 * player's car smokes: the opponents are given no smoke objects (0x142e8).
 */
#define PUFFS 8
typedef struct { int on; float phase, lx, lz; scene_object *obj; } puff;
static puff  g_puff[PUFFS];
static int   g_skidding;               /* the game's "skid sound on" flag (0x17d04) */
static int   g_crash_puff;             /* steps of smoke still owed after a crash landing */
static float g_puff_timer;
static mesh  g_smoke_mesh;
static int   g_smoke_ok;

static void smoke_init(const char *gfxdir)
{
    char file[600];
    snprintf(file, sizeof file, "%s/SMOKE.3DX", gfxdir);
    g_smoke_ok = (mesh_load(&g_smoke_mesh, file) == 0) && !getenv("OS_NOSMOKE");
    for (int i = 0; i < PUFFS && g_smoke_ok && g_ncar < MAX_CARS; i++) {
        scene_object *o = &g_car[g_ncar++];
        memset(o, 0, sizeof *o);
        o->m = &g_smoke_mesh;
        o->sprite = 1;
        o->over = 1;
        o->radius = 500.0f;
        o->z = -1.0e6f;
        g_puff[i].obj = o;
        g_puff[i].on = 0;
    }
}

static void smoke_step(float dt)
{
    const car_state *c = &g_pc;
    if (!g_smoke_ok || !g_pobj || !g_pobj->m) return;
    int skid = g_skidding = c->lock != 0.0f || c->slide > 0.0f || c->spin || fabsf(c->drift) > 0.23f
            || (c->launch && c->throttle > 0.0f && g_race_on) || getenv("OS_SMOKETEST")
            || g_crash_puff > 0;
    if (g_crash_puff > 0) g_crash_puff--;
    if (skid) {
        if (g_puff_timer < 0.05f) g_puff_timer += dt;
        else {
            g_puff_timer = 0.0f;
            for (int side = 0; side < 2; side++)
                for (int i = 0; i < PUFFS; i++) {
                    puff *q = &g_puff[i];
                    if (q->on || !q->obj) continue;
                    /* 0x198b7: outwards by (x & 7) x 51 and back by (z & 7) x 51 of
                       the car's own whole-number position -- no dice.  (The 51 is
                       the low bits of the word at 0x7fd, which is the middle of a
                       floating-point constant: it looks like a slip of the game's
                       makers, and it is what the game does.) */
                    q->on = 1; q->phase = 0.0f;
                    q->lx = (side ? -1.0f : 1.0f) * (g_pobj->m->track_half + (float)(((int)c->x & 7) * 51));
                    q->lz = g_pobj->m->axle_rear + (float)(((int)c->y & 7) * 51);
                    break;
                }
        }
    }
    float cs = cosf(g_pobj->yaw), sn = sinf(g_pobj->yaw);
    for (int i = 0; i < PUFFS; i++) {
        puff *q = &g_puff[i];
        if (!q->obj) continue;
        if (q->on) {
            q->phase += dt * 1.5f;
            if (q->phase >= 1.0f) q->on = 0;
        }
        if (!q->on) { q->obj->z = -1.0e6f; continue; }
        q->obj->x = c->x + q->lx * cs - q->lz * sn;
        q->obj->y = c->y + q->lx * sn + q->lz * cs;
        q->obj->z = c->z;
        q->obj->phase = (int)(q->phase * 65535.0f);
    }
}

static void smoke_dump(void)
{
    static int n = 0;
    if (!getenv("OS_SMOKEDUMP") || n++ % 100) return;
    for (int i = 0; i < PUFFS; i++)
        if (g_puff[i].obj)
            printf("  puff %d on=%d phase %.2f at %.0f, %.0f, %.0f  (car %.0f, %.0f, %.0f) frame %d\n",
                   i, g_puff[i].on, g_puff[i].phase, g_puff[i].obj->x, g_puff[i].obj->y,
                   g_puff[i].obj->z, g_pc.x, g_pc.y, g_pc.z, g_puff[i].obj->phase);
}

/*
 * THE CRASH (0x16c90 decides, 0x18518 states 2 and 1 play it).
 *
 * Hitting a wall is a crash when   speed x (angle off the road / 90 degrees)
 * is more than the car's maxspd x 0.3 -- flat out, that is anything over 27
 * degrees.  Then (state 2, once): the car keeps 0.6 of its speed, is pointed
 * straight down the road, and is thrown up at 3000 units/s turning over at
 * 12 rad/s nose-over-tail and 12 rad/s sideways, and the game cuts to its low
 * chase view.  After that (state 1, every frame): gravity is 6000 units/s per
 * second, both turning rates die away (damp[97]); each time it comes down it
 * puffs smoke, bounces with 0.4 of its speed, is set level again and given a
 * small wobble (+-0.6 and -1 rad/s); the fourth landing ends it and the view
 * goes back.  All the while the throttle, the skid model and the wall
 * response are off (each tests the state), and the shadow stays flat.
 */
static struct { int on, bounces, view; float vy, height, pitch, roll, pitch_rate, roll_rate; } g_crash;

/*
 * RUMBLE.  The port's own: the game of 1995 had nothing of the kind (its
 * controls only read the keyboard and the game port).  A controller has a
 * heavy, slow motor and a light, fast one; what the car goes through is
 * turned into strengths 0..1 for the two.
 *
 *   KNOCKS (rumble_kick: a strength held for a moment, then dying away)
 *     a wall            by the speed it cost: a scrape is a tremble, a square hit a thump
 *     the crash         both motors full as the car is thrown up, then each landing,
 *                       weaker every time
 *     another car       a short thud for every moment of contact
 *     a cone or sign    a tick, mostly the light motor
 *   WHAT GOES ON AS LONG AS IT LASTS (rumble_step)
 *     grass             the heavy motor, by the wheels on it and the speed, shuddering
 *                       with the same  (x + y) & 15  that jolts the wheels' heights
 *     skidding          the light motor, more the faster the car slides sideways
 *     locked wheels     both, lightly
 *     wheelspin off the line   mostly the light one
 *     a spin            the heavy one
 *
 * Nothing while a replay or the intro runs, once the race is over, or while
 * the race stands still (the question, the CD player).  OS_NORUMBLE=1 and, on
 * the MiSTer, the core's Rumble option (Off / On / Strong) switch it off;
 * OS_RUMBLE_GAIN=x multiplies it; OS_RUMBLELOG=1 prints
 * what is sent.
 */
static float g_race_clock;                       /* (defined with the race's clocks, further down) */
#define RUMBLE_STRONG 1.75f                      /* the MiSTer core's "Strong": everything this much more, up to the motors' most */
static struct { float low, high, hold; } g_kick;
static float g_rumble_low, g_rumble_high;

static void rumble_kick(float low, float high, float seconds)
{
    if (low > g_kick.low) g_kick.low = low;
    if (high > g_kick.high) g_kick.high = high;
    if (seconds > g_kick.hold) g_kick.hold = seconds;
}

/* one 1/50 s step of the race */
static void rumble_step(float dt)
{
    const car_state *c = &g_pc;
    float low = 0.0f, high = 0.0f;
    if (g_kick.hold > 0.0f) g_kick.hold -= dt;
    else {                                           /* a knock dies away in about a fifth of a second */
        g_kick.low -= dt * 5.0f;  if (g_kick.low < 0.0f) g_kick.low = 0.0f;
        g_kick.high -= dt * 5.0f; if (g_kick.high < 0.0f) g_kick.high = 0.0f;
    }
    if (!g_crash.on) {
        float v = c->speed / 8000.0f;
        if (v > 1.0f) v = 1.0f;
        int on_grass = c->grass[0] + c->grass[1] + c->grass[2] + c->grass[3];
        if (on_grass && v > 0.02f) {
            float jolt = (float)(((int)c->x + (int)c->y) & 15) / 15.0f;
            low += (float)on_grass * 0.25f * v * (0.3f + 0.5f * jolt);
            high += (float)on_grass * 0.25f * v * 0.15f;
        }
        if (g_skidding && !c->launch) {
            float s = c->slide / 3000.0f;
            high += 0.25f + 0.3f * (s > 1.0f ? 1.0f : s);
        }
        if (c->lock != 0.0f) { high += 0.2f; low += 0.15f; }
        if (c->launch) { high += 0.4f; low += 0.15f; }
        if (c->spin) low += 0.3f;
    }
    low += g_kick.low; high += g_kick.high;
    g_rumble_low = low > 1.0f ? 1.0f : low;
    g_rumble_high = high > 1.0f ? 1.0f : high;
}

/* once a picture: `driving` is 0 whenever the player is not at the wheel */
static void rumble_show(int driving)
{
    static int on = -1;
    if (on < 0) on = !getenv("OS_NORUMBLE");
    int go = on && driving;
    float gain = 1.0f;
#ifdef MISTER_HYBRID
    go = go && mister_rumble_on();
    if (mister_rumble_on() == 2) gain = RUMBLE_STRONG;
#endif
    if (getenv("OS_RUMBLE_GAIN")) gain = (float)atof(getenv("OS_RUMBLE_GAIN"));
    if (!go) { g_kick.low = g_kick.high = g_kick.hold = 0.0f; g_rumble_low = g_rumble_high = 0.0f; }
    controls_rumble(g_rumble_low * gain, g_rumble_high * gain);
    if (getenv("OS_RUMBLELOG") && (g_rumble_low > 0.0f || g_rumble_high > 0.0f))
        printf("rumble %.2fs  heavy %.2f  light %.2f\n", g_race_clock, g_rumble_low, g_rumble_high);
}
static int g_snd_engine = -1, g_snd_skid = -1, g_snd_wall = -1;
static void view_change(int to);
static int  g_view;
#define CRASH_VIEW 4

static void crash_start(void)
{
    if (g_crash.on || getenv("OS_NOCRASH")) return;
    memset(&g_crash, 0, sizeof g_crash);
    g_crash.on = 1;
    g_crash.view = g_view;
    g_crash.vy = 3000.0f;
    g_crash.pitch_rate = g_crash.roll_rate = 12.0f;
    rumble_kick(1.0f, 1.0f, 0.4f);
    g_pc.speed *= 0.6f;
    const scene_dir *d = nearest_dir(g_pc.x, g_pc.y);
    if (d) g_pc.yaw = d->angle;
    g_pc.drift = 0.0f; g_pc.spin = 0;
    view_change(CRASH_VIEW);
    snd_play(SND_DENT);                             /* 0x18540 */
    comment(9, 3, 0.5f);
    printf("crash at %.0f km/h, total time %.2f\n", drive_kmh(&g_pc) / 0.6f, g_total_time);
}

static void crash_step(float dt)
{
    if (!g_crash.on) return;
    g_crash.height += g_crash.vy * dt;
    g_crash.vy -= dt * 6000.0f;
    g_crash.pitch_rate *= drive_damp(97);
    g_crash.pitch += g_crash.pitch_rate * dt;
    g_crash.roll_rate *= drive_damp(97);
    g_crash.roll += g_crash.roll_rate * dt;
    if (g_crash.height > 0.0f) return;
    g_crash.bounces++;
    rumble_kick(1.0f / (float)g_crash.bounces, 0.6f / (float)g_crash.bounces, 0.15f);
    g_crash.height = 0.0f;
    g_crash.vy = -g_crash.vy * 0.4f;
    g_crash.pitch = g_crash.roll = 0.0f;
    g_crash.roll_rate = (g_crash.bounces & 1) ? -0.6f : 0.6f;
    g_crash.pitch_rate = -1.0f;
    if (g_crash.bounces > 3) {
        g_crash.on = 0;
        view_change(g_crash.view);
    }
}

static void player_sync(void)
{
    if (!g_pobj) return;
    g_pobj->x = g_pc.x; g_pobj->y = g_pc.y; g_pobj->z = g_pc.z;
    /*
     * THE MODEL'S NOSE IS LOCAL -Z.  The wide, tall wheels (radius 109) sit at
     * z = +490 and the smaller ones (94) at z = -445, and on these mid-engined
     * cars the big wheels are the REAR ones.  (First reading had it the other
     * way round and Danik found himself looking at the headlights.)
     * place() sends local +Z to yaw + 90 degrees, so local -Z points along
     * yaw - 90, which is exactly the car's heading (sin yaw, -cos yaw).
     * With the nose at -Z the model's +X is the car's LEFT, hence the signs.
     */
    g_pobj->yaw = g_pc.yaw + g_pc.drift;
    g_pobj->pitch = -g_pc.pitch;
    g_pobj->roll = -g_pc.roll;
    /*
     * 0x173a9: WHICH OF A CAR TEXTURE'S 8 FRAMES.  Record +0x20 is
     *     paint job (0 / 0x8000)  |  0x4000 when NOT braking  |  0x2000 at or under 4000 units/s
     * and the frame is that >> 13: +4 = second paint job, +2 = brake lights
     * off, +1 = slow (the wheel's still picture; the other is its blur).
     */
    g_pobj->phase = g_spec[g_spec_now].livery | (g_pc.brake == 0.0f ? 0x4000 : 0)
                  | (g_pc.speed <= 4000.0f ? 0x2000 : 0);
    g_pobj->pose = &g_ppose;
    /* 0x1813c: the reflection picture goes by the kind of the lap marker the
       car is in -- 0 the sky, 1 (a tunnel) the lights, 2 the third picture */
    {
        int kind = g_nmark ? g_mark[g_mark_at].kind : 0;
        g_ppose.env = kind == 0 ? 0 : kind == 1 ? 0x8000 : 0xff00;
    }
    shadow_follow(g_pshadow, g_pobj);
    if (g_crash.on) {                   /* thrown up and tumbling; the shadow stays put */
        g_pobj->z += g_crash.height;
        g_pobj->pitch += g_crash.pitch;
        g_pobj->roll += g_crash.roll;
        if (g_pshadow) { g_pshadow->pitch = -g_pc.pitch; g_pshadow->roll = -g_pc.roll; }
    }
}

/*
 * Put the car on the road near a path sample, standing still: on the road's
 * centre line and facing along it, both taken from the track file's own
 * road-direction points -- which is what the game does at the start (0x136c8).
 */
static void player_reset(const path *r, int sample)
{
    int i = (sample % r->count + r->count) % r->count, j = (i + 1) % r->count;
    car_spec keep = g_pc.spec;
    float wb = g_pc.wheelbase;
    drive_init(&g_pc, &keep, wb);
    g_pc.x = r->p[i][0];
    g_pc.y = r->p[i][1];
    g_pc.yaw = atan2f(r->p[j][0] - r->p[i][0], -(r->p[j][1] - r->p[i][1]));
    const scene_dir *d = nearest_dir(g_pc.x, g_pc.y);
    if (d) {
        g_pc.x = d->x; g_pc.y = d->y;
        /* face the way the lap goes, whichever way the point was authored */
        float fx = sinf(d->angle), fy = -cosf(d->angle);
        float along = fx * sinf(g_pc.yaw) + fy * -cosf(g_pc.yaw);
        g_pc.yaw = d->angle + (along < 0.0f ? (float)M_PI : 0.0f);
    }
    g_pc.z = r->p[i][2];
    surface_z(g_pc.x, g_pc.y, g_pc.z, &g_pc.z);
    g_pat = i;
    g_chase.init = 0;
    g_pc.surface = 0.0f;
    if (g_pobj) {
        float z, pitch, roll;
        if (car_ground(g_pobj->m, g_pc.x, g_pc.y, g_pc.z, g_pc.yaw, &z, &pitch, &roll)) {
            g_pc.z = z; g_pc.pitch = pitch; g_pc.roll = roll;
        }
    }
    player_sync();
}

static void cockpit_load(int model, int livery);
static void dash_load(int model);
static int g_art_model = -1, g_art_livery;

static void opp_assign_cars(void);
/* choose which car is driven: the row of CARS.DTR, and its model */
static void player_car(const path *r, int which)
{
    if (g_nspec == 0) return;
    g_spec_now = (which % g_nspec + g_nspec) % g_nspec;
    const car_spec *sp = &g_spec[g_spec_now];
    int model = atoi(sp->name + 4);                 /* "AUTO03" -> 3 */
    if (model < 0 || model > 6 || !car_mesh_ok[model]) model = 0;
    const mesh *mm = &car_mesh[model];
    float px = g_pc.x, py = g_pc.y, pz = g_pc.z, pyaw = g_pc.yaw;
    int had = (g_pobj != NULL);
    drive_init(&g_pc, sp, mm->wheelbase > 0 ? mm->wheelbase : 935.0f);
    if (!g_pobj) {
        g_pobj = &g_car[g_ncar++];
        memset(g_pobj, 0, sizeof *g_pobj);
        g_pshadow = shadow_new();
    }
    g_pobj->m = mm;
    g_pobj->v = model;
    g_pobj->phase = sp->livery;                     /* the second paint job: frame 4 of 8 */
    cockpit_load(model, sp->livery != 0);
    dash_load(model);
    g_art_model = model; g_art_livery = sp->livery != 0;
    g_pobj->radius = 1200.0f;

    /*
     * The body's corners, as the game builds them (0x136c8): each WHEEL's
     * position, moved outward along the car -- 250 units ahead of the front
     * wheels, 150 behind the rear ones.  Sideways they are at the wheels,
     * 226 units out, which is narrower than the bodywork: a car can lean its
     * flank on a wall.
     *
     * Do not measure the width off the model.  On track 4 the nose block
     * carries the headlight beam, a see-through fan 1600 units to each side
     * and 4000 ahead; taking the model's extent made the BEAM solid, and the
     * car stopped against walls it was nowhere near (Danik: "the headlight
     * effect is part of the collision system").
     */
    float half = mm->track_half > 0.0f ? mm->track_half : 226.0f;
    float zf = mm->axle_front + 250.0f;     /* forward of the car's origin */
    float zr = mm->axle_rear + 150.0f;      /* behind it */
    g_corner[0][0] = -half; g_corner[0][1] =  zf;
    g_corner[1][0] = -half; g_corner[1][1] = -zr;
    g_corner[2][0] =  half; g_corner[2][1] =  zf;
    g_corner[3][0] =  half; g_corner[3][1] = -zr;

    opp_assign_cars();
    if (had) { g_pc.x = px; g_pc.y = py; g_pc.z = pz; g_pc.yaw = pyaw; player_sync(); }
    else player_reset(r, 2);
    printf("car %d: %s  top speed %.0f km/h, %d gears, %s, grip %.2f\n",
           g_spec_now, sp->name, sp->maxspd * 340.0f / 21000.0f, sp->nshift,
           sp->automatic ? "automatic" : "manual (A / Z)", sp->grip);
    fflush(stdout);
}

/*
 * WALLS -- the game's own collision, 0x11e74 / 0x11f98 / 0x16b88 / 0x16c90.
 *
 *  1. each of the body's four corners is tested: is it still on the track?
 *  2. for a corner that is not, the nearest wall segment is looked up and the
 *     corner is moved along that wall's normal to 30 units inside the wall's
 *     line -- straight back onto the track (one departure from the game
 *     here, explained at the push);
 *  3. the car is moved by the largest push in x and the largest in y;
 *  4. the wall response (drive_hit_wall): speed lost by how squarely the car
 *     is heading into the wall, the nose swung away from it by which FRONT
 *     corner is out, and a shove along the push.
 *
 * So you can drive anywhere the track pieces go -- verges included -- and
 * what stops you is the wall, at the angle the wall really has.
 */
static void player_walls(void)
{
    car_state *c = &g_pc;
    float fx = sinf(c->yaw), fy = -cosf(c->yaw);      /* forward */
    float rx = -cosf(c->yaw), ry = -sinf(c->yaw);     /* right */
    float cr = fabsf(cosf(c->roll)), cp = fabsf(cosf(c->pitch));
    int out[4], any = 0;
    float px[4], py[4], wall_angle = 0.0f;
    for (int k = 0; k < 4; k++) {
        float lx = g_corner[k][0] * cr, lz = g_corner[k][1] * cp;
        px[k] = c->x + rx * lx + fx * lz;
        py[k] = c->y + ry * lx + fy * lz;
        out[k] = !on_track(px[k], py[k]);
        any |= out[k];
    }
    if (!any) return;

    float mx = 0.0f, my = 0.0f;
    for (int k = 0; k < 4; k++) {
        if (!out[k]) continue;
        const scene_wall *w = nearest_wall(px[k], py[k]);
        if (!w) continue;
        float nx = cosf(w->angle), ny = -sinf(w->angle);        /* the wall's normal */
        float across = (px[k] - w->x) * nx + (py[k] - w->y) * ny;
        /*
         * WHERE THE CORNER IS PUT BACK TO: 30 units inside the wall's line,
         * the whole way in one go.
         *
         * The game (0x11f98) puts it 30 units on the OTHER side of the line
         * from where it is.  That is the same thing for a corner that has gone
         * through the wall -- but not on the inside of a bend, where the wall
         * segments are straight chords and the track's surface ends up to 130
         * units short of them.  A corner in that sliver is off the track but
         * still inside the line; "the other side" then throws it OUT through
         * the wall, and the next frame back in, for ever.  That was the car
         * "stuck inside the wall" and the "invisible wall" in track 4's bend
         * (Danik).  So this is a deliberate departure: `side` says which way
         * is in (measured once per wall, see walls_prepare), a corner is only
         * ever moved inwards, and one that is already inside the line by 30
         * is left alone.
         */
        float in = g_wall_in[w - g_scene->wall];
        float depth = across * in;            /* >0 = inside the line */
        if (depth >= 30.0f) continue;
        float push = (30.0f - depth) * in;
        float qx = push * nx, qy = push * ny;
        if (fabsf(qx) > fabsf(mx)) { mx = qx; wall_angle = w->angle; }
        if (fabsf(qy) > fabsf(my)) { my = qy; wall_angle = w->angle; }
    }
    if (mx == 0.0f && my == 0.0f) return;        /* off the surface but not at a wall */
    c->x += mx;
    c->y += my;
    /* front-right out swings the nose left (+), front-left out swings it right */
    if (getenv("OS_HITDUMP"))
        printf("hit t=%d at (%.0f, %.0f) corners out %d%d%d%d  push (%.0f, %.0f)  %.0f km/h\n",
               g_pat, c->x, c->y, out[0], out[1], out[2], out[3], mx, my, drive_kmh(c));
    g_wall_hits++;
    if (g_crash.on) return;
    g_snd_wall = snd_play_once(SND_WALL, g_snd_wall);   /* 0x16c90: unless it is still sounding */             /* already in the air: no wall response (0x16cdf) */
    {
        /* 0x16c90: a crash, or only a knock? */
        const scene_dir *d = nearest_dir(c->x, c->y);
        float off = d ? fabsf(c->yaw - d->angle) : 0.0f;
        while (off > 3.1415927f) off -= 3.1415927f;
        if (off > 1.5707964f) off = 3.1415927f - off;
        if (c->speed * (off / 1.5707964f) > c->k_wall && !getenv("OS_NOCRASH")) { crash_start(); return; }
    }
    {
        const float before = c->speed;
        drive_hit_wall(c, mx, my, (float)(out[2] - out[0]), wall_angle);
        float lost = (before - c->speed) / 4000.0f;      /* a scrape costs next to nothing, a square hit most of the speed */
        if (lost < 0.0f) lost = 0.0f;
        if (lost > 1.0f) lost = 1.0f;
        rumble_kick(0.2f + 0.8f * lost, 0.2f + 0.5f * lost, 0.05f + 0.15f * lost);
    }
}

/*
 * THE GAME'S CAMERAS (0x19e68 picks one by the number at camera +0x88; the
 * camera record is at 0x10a1c: position, then pitch / heading / lean).
 *
 *   view  routine            behind  above  looks down
 *    9    0x1a3a8(.., 0)      1700    300     7.2 deg     far chase
 *    1    0x1a3a8(.., 1)      1300    300    10.8 deg     near chase
 *    6    0x1a3a8(.., 2)      1400    100     7.2 deg     low chase
 *    0    0x1a2ac               --  camera_hi   --        bumper cam
 *    3    0x1a620             the trackside cameras of MAPnRPL.SPL (trackside_camera)
 *    8    0x1aeb4             the intro's cameras at the posts of MAPnCAM.SPL (intro_cam_car)
 *
 * The chase views put the camera `behind` units back along the car's heading
 * and  car height + above - behind * sin(pitch)  up, where the camera's
 * heading, LEAN and pitch each ease towards the car's (stiffness 0x51) -- so
 * on a banked road the whole picture leans with the car.  The view from the
 * car sits `camera_hi` (TRACK_DATA in MAPnA.DTR; 300 on track 1) above the
 * car's origin and turns rigidly with it, drift angle included.
 */
typedef struct { const char *name; float behind, above, down; int incar, height, cockpit; } cam_view;
/*
 * The views the player can choose, in the order of the game's own table at
 * 0x1da0 (seven rows of: key, camera routine, HEIGHT OF THE 3D VIEW, overlay).
 * Row 1 is the cockpit: the bumper cam's camera, but the 3D view is
 * only 132 rows tall and the car's dashboard picture covers the rest.
 *
 * THE PLAYER HAS FOUR VIEWS (Danik, from the original): rows 0, 1, 2 and 6.
 * Rows 3 (trackside: the end of a race and replays), 4 (the intro) and 5 -- a
 * chase from 1400 back and only 100 up, which looks straight at the bumper:
 * the crash -- are in the table but are not offered while driving.  The port
 * offered row 5 for a day; do not put it back.
 */
static const cam_view g_views[] = {
    { "bumper cam",       0.0f,   0.0f, 0.0f,    1, 200, 0 },   /* table row 0 */
    { "cockpit",          0.0f,   0.0f, 0.0f,    1, 132, 1 },   /* row 1 */
    { "near chase",    1300.0f, 300.0f, 0.1885f, 0, 200, 0 },   /* row 2 */
    { "far chase",     1700.0f, 300.0f, 0.1257f, 0, 200, 0 },   /* row 6 */
    /* row 5 is not the player's to choose: it is the view the game cuts to
       while the car is CRASHING (0x18584 raises its request flag at 0x1e14) */
    { "crash",         1400.0f, 100.0f, 0.1257f, 0, 200, 0 },   /* row 5 */
};
#define NVIEWS ((int)(sizeof g_views / sizeof g_views[0]))
#define PLAYER_VIEWS 4
#define CRASH_VIEW 4
static int   g_view = 2;               /* 0x1d2a4, default 2: a race starts in the near chase */
static float g_camera_hi = 300.0f;

/*
 * CHANGING VIEW (0x19ad8 / 0x19ef0 / 0x1a134).  The game does not cut: it
 * notes how far the old camera was from the new one -- position and the three
 * angles -- and draws from  new camera + that difference  while the
 * difference dies away: position x damp[90] each frame, angles eased to zero
 * with stiffness 0x19.  It is over when every angle's cosine is above 0.99995
 * and every distance under 200 x the frame time; only then does the 3D view
 * take its new height and the overlay appear.
 */
static struct { int on; float x, y, z, yaw, pitch, roll; } g_swoop;

/*
 * The dashboard: COCKnn{A,B}{L,H}.PKC in GAME.DAT (L = 320x200).  A .PKC is a
 * run of int32s: a NEGATIVE number skips that many pixels (see-through), a
 * positive one is followed by that many pixels to copy.  Pixels are palette
 * indices, in the track's palette.
 */
static int16_t g_cockpit[FB_MAXW * FB_MAXH];
static int     g_have_cockpit;

static int pkc_load(const char *file, int16_t *out)
{
    for (int i = 0; i < ART_W * ART_H; i++) out[i] = -1;
    file = res_name(file);
    FILE *f = fopen(file, "rb");
    if (!f) { fprintf(stderr, "note: no picture at %s\n", file); return 0; }
    long at = 0;
    int32_t n;
    while (at < ART_W * ART_H && fread(&n, 4, 1, f) == 1) {
        if (n < 0) { at += -(long)n; continue; }
        for (int k = 0; k < n; k++) {
            int ch = fgetc(f);
            if (ch == EOF) break;
            if (at < ART_W * ART_H) out[at] = (int16_t)ch;
            at++;
        }
    }
    fclose(f);
    return 1;
}

static int g_pkc_loads;                 /* goes up whenever the pictures are read again: what pkc_draw remembers is stale */

static void pkc_draw(const int16_t *pic)
{
    /* where a picture has pixels at all is found once (most of it is
       see-through): runs of them, a row at a time, with where on the screen
       each goes -- in a wide picture only the even rows have a place */
    static struct { const int16_t *pic; int n, loads; int *run; } known[6];
    int k = 0;
    uint32_t *const to = fb + g_oy * FB_W;                 /* (a picture is 200 rows of the layout: see g_tall) */
    while (k < 6 && known[k].pic && known[k].pic != pic) k++;
    if (k == 6) k = 0;
    if (known[k].pic != pic || known[k].loads != g_pkc_loads) {
        int n = 0;
        for (int pass = 0; pass < 2; pass++) {
            n = 0;
            for (int y = 0; y < ART_H; y++)
                for (int x = 0; x < ART_W; x++) {
                    /* (WIDE: an odd row only where the even row over it is open: see hud_text) */
#define PKC_SHOWS(xx) (pic[y * ART_W + (xx)] >= 0 && !(g_yd == 2 && (y & 1) && pic[(y - 1) * ART_W + (xx)] >= 0))
                    if (!PKC_SHOWS(x)) continue;
                    int x1 = x;
                    while (x1 < ART_W && PKC_SHOWS(x1)) x1++;
                    if (pass) { known[k].run[3 * n] = y * ART_W + x; known[k].run[3 * n + 1] = y / g_yd * FB_W + x; known[k].run[3 * n + 2] = x1 - x; }
                    n++;
                    x = x1;
                }
            if (!pass) known[k].run = malloc(sizeof(int) * 3 * (size_t)(n + 1));
        }
        known[k].n = n; known[k].pic = pic; known[k].loads = g_pkc_loads;
    }
    for (int r = 0; r < known[k].n; r++) {
        const int16_t *from = pic + known[k].run[3 * r];
        uint32_t *at = to + known[k].run[3 * r + 1];
        const int len = known[k].run[3 * r + 2];
        for (int i = 0; i < len; i++) at[i] = g_pal[from[i]];
    }
}

static void cockpit_load(int model, int livery)
{
    char file[128];
    snprintf(file, sizeof file, "build/dec/GAME/COCK%02d%cL.PKC", model, livery ? 'B' : 'A');
    g_have_cockpit = pkc_load(file, g_cockpit);
}

static void cockpit_draw(void)
{
    if (!g_have_cockpit) return;
    pkc_draw(g_cockpit);
}

static float wrap_pi(float a)
{
    while (a >  (float)M_PI) a -= 2.0f * (float)M_PI;
    while (a < -(float)M_PI) a += 2.0f * (float)M_PI;
    return a;
}

/* one 1/50 s step of the chase camera's easing */
static void chase_step(void)
{
    const cam_view *v = &g_views[g_view];
    const car_state *c = &g_pc;
    /* 0x1a53d: it eases to the car's heading (without its skid angle), and to
       the lean and nose-up of the heading-only, jolt-free wheel set */
    const float car_pitch = g_contact.ready ? g_contact.cam_pitch : c->pitch;
    const float car_roll  = g_contact.ready ? g_contact.cam_roll  : c->roll;
    float want_pitch = car_pitch - v->down;
    if (!g_chase.init) {
        g_chase.yaw = c->yaw; g_chase.roll = car_roll; g_chase.pitch = want_pitch;
        g_chase.height = c->z + v->above - v->behind * sinf(g_chase.pitch);
        g_chase.init = 1;
        return;
    }
    float k = drive_damp(0x51);
    g_chase.yaw   = c->yaw     + wrap_pi(g_chase.yaw   - c->yaw)     * k;
    g_chase.roll  = car_roll   + wrap_pi(g_chase.roll  - car_roll)   * k;
    g_chase.pitch = want_pitch + wrap_pi(g_chase.pitch - want_pitch) * k;
    float want_h = c->z + v->above - v->behind * sinf(g_chase.pitch);
    g_chase.height = want_h + (g_chase.height - want_h) * drive_damp(0);
}

static void view_pose(camera *cam);

/* the camera to draw from: the view's own, plus what is left of a change */
static void chase_camera(camera *cam)
{
    view_pose(cam);
    if (g_swoop.on) {
        cam->x += g_swoop.x; cam->y += g_swoop.y; cam->z += g_swoop.z;
        cam->yaw += g_swoop.yaw; cam->pitch += g_swoop.pitch; cam->roll += g_swoop.roll;
        /* during the change the view stays full height; the car is already
           hidden if the view being flown to is one from inside it, as in the
           game (0x1a2ac takes it off the draw list every frame) */
        g_vh = FB_H;
    } else {
        g_vh = g_views[g_view].height * SY + g_tall;      /* (what covers the rest sits at the bottom) */
    }
    g_cy = EYE_LINE * (float)g_vh;
}

static void swoop_done(void)
{
    float lim = DRIVE_DT * 200.0f;
    if (cosf(g_swoop.yaw) > 0.99995f && cosf(g_swoop.pitch) > 0.99995f &&
        cosf(g_swoop.roll) > 0.99995f && fabsf(g_swoop.x) < lim &&
        fabsf(g_swoop.y) < lim && fabsf(g_swoop.z) < lim)
        g_swoop.on = 0;
}

/* choose a view: remember where the camera was so it can fly across */
static void view_change(int to)
{
    /*
     * Where the camera is NOW, in the view being left -- worked out afresh
     * from the car's present position, as the game does (0x19ad8 calls the old
     * view's routine once more before it saves the camera).  Using the camera
     * the last frame was drawn from instead leaves a car's-length of travel in
     * the difference, and that takes two seconds to die away: going from the
     * bumper cam to the cockpit, which are the SAME camera, the dashboard then
     * turned up seconds late (Danik).  Done this way the difference is zero
     * and the change is immediate.
     */
    camera old, now;
    view_pose(&old);
    const camera *from = &old;
    g_view = ((to % NVIEWS) + NVIEWS) % NVIEWS;
    g_chase.init = 0;
    chase_step();                      /* the new view's camera, settled */
    view_pose(&now);
    g_swoop.x = from->x - now.x; g_swoop.y = from->y - now.y; g_swoop.z = from->z - now.z;
    g_swoop.yaw   = wrap_pi(from->yaw - now.yaw);
    g_swoop.pitch = wrap_pi(from->pitch - now.pitch);
    g_swoop.roll  = wrap_pi(from->roll - now.roll);
    g_swoop.on = 1;
    swoop_done();                      /* nothing to fly across?  then it is over */
    if (getenv("OS_SWOOPDBG"))
        printf("swoop: from (%.0f %.0f %.0f) yaw %.2f  to (%.0f %.0f %.0f) yaw %.2f  car (%.0f %.0f %.0f)\n",
               from->x, from->y, from->z, from->yaw, now.x, now.y, now.z, now.yaw,
               g_pc.x, g_pc.y, g_pc.z);
    printf("camera: %s\n", g_views[g_view].name);
    fflush(stdout);
}

/* one 1/50 s step of the change dying away */
static void swoop_step(void)
{
    if (!g_swoop.on) return;
    float kp = drive_damp(90), ka = drive_damp(0x19);
    g_swoop.x *= kp; g_swoop.y *= kp; g_swoop.z *= kp;
    g_swoop.yaw *= ka; g_swoop.pitch *= ka; g_swoop.roll *= ka;
    swoop_done();
}

static void view_pose(camera *cam)
{
    const cam_view *v = &g_views[g_view];
    const car_state *c = &g_pc;
    g_hide = NULL;
    if (v->incar) {
        float heading = c->yaw + c->drift;
        cam->x = c->x; cam->y = c->y;
        /* 0x1a2ac: from inside, the picture rides on the SUSPENSION too --
           it dips under braking and leans in the bends with the body: pitch +
           the body's (+0x4e0), lean + 3 x the body's (+0x4e8), height + its
           drop (+0x4c0).  (The pose holds the body angles x2 and x4, as the
           body blocks use them.) */
        cam->z = c->z + g_camera_hi + g_ppose.body_drop;
        cam->yaw = heading - (float)M_PI / 2.0f;   /* port camera: forward = (cos, sin) */
        cam->pitch = c->pitch + g_ppose.body_pitch * 0.5f;
        cam->roll = c->roll - g_ppose.body_roll * 0.75f;
        g_hide = g_pobj;
        return;
    }
    float cp = cosf(g_chase.pitch);
    cam->x = c->x - cp * v->behind * sinf(g_chase.yaw);
    cam->y = c->y + cp * v->behind * cosf(g_chase.yaw);
    cam->z = g_chase.height;
    cam->yaw = g_chase.yaw - (float)M_PI / 2.0f;
    cam->pitch = g_chase.pitch;
    cam->roll = g_chase.roll;
}

/* SCREAMER 2's TRACK_DATA (MAPnDAT.DTR): how far it is drawn (a negative depth), the
   background as red, green, blue, and as a palette entry -- the first five numbers */
/*
 * SCREAMER 2'S SEE-THROUGH TABLE (0x504f2, 0x508be, 0x503a1; called at 0x2ee76 when a race is
 * set up).  The program reads no .SMG file: it makes the table from the palette (its 6-bit
 * values) and two colours of the track's TRACK_DATA.  A row is, for each of the 256 colours,
 * the palette's nearest (the first of equals, by the squares of the three differences) to a
 * mixture; the texel of a see-through face (routine 208) picks the row:
 *    0 ..  31   towards "asmoke" (its rgb / 4), row i by i / 31         the smoke on the road
 *   32 ..  47   towards black, by i / 15                                 the cars' shadow
 *   48 ..  79   towards "a2smoke", by i / 31                             the dust off the road
 *   80 ..  95   brighter, x (1 + i / 15), no more than 63
 *   96 .. 111   towards white (63), by i / 15
 * (MAPn.SMG, which the files do have for six tracks, has black where the smoke's rows are.)
 */
static void s2_blend_rows(int first, int n, float r, float g, float b, int brighten)
{
    for (int row = 0; row < n; row++) {
        const float t = brighten ? (float)row / 15.0f + 1.0f : (float)row / (float)(n - 1);
        for (int i = 0; i < 256; i++) {
            float c[3] = { (float)(g_pal[i] >> 18 & 63), (float)(g_pal[i] >> 10 & 63), (float)(g_pal[i] >> 2 & 63) };
            const float to[3] = { r, g, b };
            for (int k = 0; k < 3; k++) {
                if (brighten) { c[k] = c[k] * t; if (c[k] > 63.0f) c[k] = 63.0f; }
                else c[k] = (to[k] - c[k]) * t + c[k];
            }
            float least = 5e6f; int best = 0;
            for (int q = 0; q < 256; q++) {
                const float dr = (float)(g_pal[q] >> 18 & 63) - c[0], dg = (float)(g_pal[q] >> 10 & 63) - c[1], db = (float)(g_pal[q] >> 2 & 63) - c[2];
                const float d = db * db + (dg * dg + dr * dr);
                if (d < least) { least = d; best = q; }
            }
            g_smg[(first + row) * 256 + i] = (uint8_t)best;
        }
    }
}
static void s2_blend_make(const int smoke[3], const int dust[3])
{
    for (int row = 0; row < 112; row++) for (int i = 0; i < 256; i++) g_smg[row * 256 + i] = (uint8_t)i;
    /* (0x504f2: with all three of asmoke's below 0 only the shadow's rows -- the menu's rooms) */
    const int all = !(smoke[0] < 0 && smoke[1] < 0 && smoke[2] < 0);
    if (all) s2_blend_rows(0, 32, (float)(smoke[0] >> 2), (float)(smoke[1] >> 2), (float)(smoke[2] >> 2), 0);
    s2_blend_rows(32, 16, 0.0f, 0.0f, 0.0f, 0);
    if (all) {
        s2_blend_rows(48, 32, (float)(dust[0] >> 2), (float)(dust[1] >> 2), (float)(dust[2] >> 2), 0);
        s2_blend_rows(80, 16, 0.0f, 0.0f, 0.0f, 1);
        s2_blend_rows(96, 16, 63.0f, 63.0f, 63.0f, 0);
    }
    g_smg_rows = 112;
}

/* the first numbers of TRACK_DATA in MAPnDAT.DTR: the depth, the background's rgb and palette
   entry, asmoke's rgb, a2smoke's rgb */
static int s2_track_data(const char *file, int v[11])
{
    FILE *f = fopen(file, "r");
    char line[256];
    int in = 0, n = 0;
    if (!f) return 0;
    while (n < 11 && fgets(line, sizeof line, f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (!in) { if (!strncmp(p, "TRACK_DATA", 10)) in = 1; continue; }
        if (*p == '*') continue;
        while (n < 11 && (p = strchr(p, 'd')) != NULL) { v[n++] = atoi(p + 1); p++; }
    }
    fclose(f);
    return n == 11;
}

/* TRACK_DATA in MAPnA.DTR: colour, camera_hi, bonus_time, carnage */
static void track_data_load(const char *file)
{
    FILE *f = fopen(file, "r");
    if (!f) return;
    char line[256];
    int in = 0;
    while (fgets(line, sizeof line, f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (!in) { if (!strncmp(p, "TRACK_DATA", 10)) in = 1; continue; }
        if (*p == '*' || *p == '\r' || *p == '\n') continue;
        int colour = 0, hi = 0, bonus = 0, carnage = 0;
        int got = sscanf(p, "d%d d%d d%d d%d", &colour, &hi, &bonus, &carnage);
        if (got >= 4 && carnage > 0) g_carnage_time = (float)carnage;
        if (got >= 1 && colour >= 0 && colour < 256) g_ground_colour = colour;
        if (got >= 2 && hi > 0) g_camera_hi = (float)hi;
        if (got >= 3 && bonus > 0) g_bonus_time = (float)bonus;
        break;
    }
    fclose(f);
}

/* -------------------------------------------------------------- opponents */

/*
 * THE OTHER NINE CARS, from the game (frame 0x2002f calls 0x1cd34, 0x1c750,
 * 0x1c800, 0x1c78c, 0x1c7c0, 0x1c83c on each 0x590-byte record at 0x10ab4;
 * build/decomp/opponent.c).
 *
 * MAPnOPP.SPL holds TEN splines, not one: `int count, int start_node, count x
 * 56 bytes`, ten times over.  Opponent k drives spline k; the tenth is the
 * player's.  Each spline's start node is that car's place on the GRID -- two
 * columns, the player last.
 *
 * The curve (0x1d54c + 0x1e7a4) is a uniform quadratic B-spline over the
 * nodes' HANDLES, two pieces per node:
 *     piece 0:  node.in,  node.out, next.in
 *     piece 1:  node.out, next.in,  next.out
 *     point(u) = A*(1-u)^2/2 + C*u^2/2 + B*(the rest)
 * (the node's own point is not on it; it is only the grid position).
 *
 * An opponent does not steer or skid.  It has a speed (0x1ced0):
 *     target = max_spd * PACE of the marker stretch it is in
 *              (+ `skill` if the player is ahead of it -- that is the catch-up)
 *     below target: speed += accel * dt     above: speed -= brakes * dt
 * and each frame it moves that far along its curve (0x1d2fc).  Its position
 * is the curve's point plus a sideways offset that collisions give it and
 * that drains away after three seconds (0x1d6ac).  max_spd, accel, brakes,
 * skill are the nine rows of OPPONENTS in MAPnA.DTR.
 *
 * Contact between cars is the game's too (cars_collide, below), and so are
 * how a car stands on the road (0x1cb9c: one point, track_under) and the
 * running order (0x20418, at the end of opponents_step).
 */
#define OPP_MAX 9
#define SPL_NODES_MAX 96
typedef struct { int n, start; int32_t node[SPL_NODES_MAX][14]; } gspline;
static gspline g_opp_spl[OPP_MAX + 1];
static int     g_nopp_spl;

typedef struct { float max_spd, accel, brakes, skill, sband; } opp_spec;
static opp_spec g_opp_spec[OPP_MAX];
static int      g_nopp_spec;

typedef struct {
    int   node, half;       /* which piece of its curve */
    float u;                /* 0..1 along that piece */
    float speed;
    float offx, offy;       /* pushed aside by a collision */
    float off_age;
    float shove, shove_x, shove_y;   /* still being pushed (+0x450, +0x460/468) */
    float spin;             /* knocked round: added to its heading, then eases back (+0x4c8) */
    /* crashing (+0x2f8): 1 tumbling, 4 spinning round twice */
    int   crash, bounces;
    float vy, up, tip, turn, lean, tip_rate, turn_rate, lean_rate, prev_speed;
    float prev_yaw, body;   /* for the lean in bends (0x1df24) */
    float x, y, z, yaw, pitch, roll;
    int   mark_at;          /* marker last reached (+0x320): PACE, and how far it has got */
    int   mk_count, mk_laps;             /* markers passed this lap (+0x324), laps done (+0x40c) */
    unsigned char mk_seen[MARKERS_MAX];  /* +0x328 */
    float progress;         /* +0x408, see marker_progress() */
    int   rank;             /* +0x410: its place in the running order */
    scene_object *obj;
} opponent;
static opponent g_opp[OPP_MAX];
/*
 * DIFFICULTY (0x1d2b0, the OPTIONS line: 0 ROOKIE, 1 NORMAL -- the game's
 * default -- 2 PRO; 3 AFTERBURNER and 4 DUMMY are in the text but beyond what
 * the menu line reaches).  It is the LETTER of the track's data file:
 * MAPnA.DTR .. MAPnE.DTR (0x13251: 'A' + difficulty) -- each with its own
 * OPPONENTS rows (top speed 14000 / 15000 / 16800 on track 1) and its own
 * bonus_time (50 / 40 / 35 s).  Above 2 every opponent drives car 12, the
 * Bullet (0x14373).  OS_DIFF sets it; the menu passes it.
 */
static int difficulty(void)
{
    int d = (getenv("OS_DIFF") && !g_intro) ? atoi(getenv("OS_DIFF")) : 1;       /* the intro: NORMAL (0x2777f) */
    return d < 0 ? 0 : d > 4 ? 4 : d;
}

static int g_opp_livery[OPP_MAX];
static car_pose g_opp_pose[OPP_MAX];
static scene_object *g_opp_shadow[OPP_MAX];
static int      g_nopp;
static float    g_race_clock;        /* seconds since the cars were lined up */
static int      g_player_rank = 1;                /* player +0x688 */
static int      g_player_spl;                  /* which spline's start node is the player's place */
static int      g_attack_slot;
static int      g_car_contact, g_lapped_one;   /* this frame: touching a car / a lap up on one */
static float    g_hitcar;                      /* player +0x7c4: contact, eased */
#define RACE_HOLD 6.0f               /* held until the green light (0x20d45), then go */

static int opp_splines_load(const char *file)
{
    FILE *f = fopen(file, "rb");
    g_nopp_spl = 0;
    if (!f) return 0;
    int32_t hdr[2];
    while (g_nopp_spl <= OPP_MAX && fread(hdr, 4, 2, f) == 2) {
        gspline *sp = &g_opp_spl[g_nopp_spl];
        if (hdr[0] < 2 || hdr[0] > SPL_NODES_MAX) break;
        if (fread(sp->node, 56, (size_t)hdr[0], f) != (size_t)hdr[0]) break;
        sp->n = hdr[0];
        sp->start = (hdr[1] >= 0 && hdr[1] < hdr[0]) ? hdr[1] : 0;
        g_nopp_spl++;
    }
    fclose(f);
    return g_nopp_spl;
}

/* OPPONENTS in MAPnA.DTR: rows of  max_spd accel brakes skill sband */
static void opp_specs_load(const char *file)
{
    g_nopp_spec = 0;
    FILE *f = fopen(file, "r");
    if (!f) return;
    char line[256];
    int in = 0;
    while (fgets(line, sizeof line, f) && g_nopp_spec < OPP_MAX) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (!in) { if (!strncmp(p, "OPPONENTS", 9)) in = 1; continue; }
        if (!strncmp(p, "END_LIST", 8)) break;
        opp_spec *o = &g_opp_spec[g_nopp_spec];
        if (sscanf(p, "f%f f%f f%f f%f f%f", &o->max_spd, &o->accel, &o->brakes,
                   &o->skill, &o->sband) == 5)
            g_nopp_spec++;
    }
    fclose(f);
}

/* the three control points of the piece an opponent is on */
static void opp_piece(const gspline *sp, int node, int half, float A[2], float B[2], float C[2])
{
    const int32_t *n = sp->node[node];
    int nx = n[4];
    if (nx < 0 || nx >= sp->n) nx = 0;
    const int32_t *m = sp->node[nx];
    if (half == 0) {
        A[0] = (float)n[5]; A[1] = (float)n[6];
        B[0] = (float)n[8]; B[1] = (float)n[9];
        C[0] = (float)m[5]; C[1] = (float)m[6];
    } else {
        A[0] = (float)n[8]; A[1] = (float)n[9];
        B[0] = (float)m[5]; B[1] = (float)m[6];
        C[0] = (float)m[8]; C[1] = (float)m[9];
    }
}

/* point and direction of travel on a piece */
static void opp_curve(const float A[2], const float B[2], const float C[2], float u,
                      float pos[2], float der[2])
{
    float w0 = (1.0f - u) * (1.0f - u) * 0.5f, w2 = u * u * 0.5f, w1 = 1.0f - w0 - w2;
    for (int k = 0; k < 2; k++) {
        pos[k] = A[k] * w0 + B[k] * w1 + C[k] * w2;
        der[k] = (1.0f - u) * (B[k] - A[k]) + u * (C[k] - B[k]);
    }
}

static void opp_place(opponent *o, int k)
{
    const gspline *sp = &g_opp_spl[k];
    float A[2], B[2], C[2], pos[2], der[2];
    opp_piece(sp, o->node, o->half, A, B, C);
    opp_curve(A, B, C, o->u, pos, der);
    o->x = pos[0] + o->offx;
    o->y = pos[1] + o->offy;
    o->yaw = atan2f(der[0], -der[1]);            /* forward = (sin yaw, -cos yaw) */
    if (o->obj) {
        /*
         * 0x1cb9c: an opponent stands on ONE point, not four wheels -- the
         * height of the drivable surface under its middle -- and is tilted
         * to that surface's slope (the surface's normal turned into the
         * car's frame, x pi / 46080 of its 16384-unit length: 1.117 x the
         * slope).  No smoothing, no suspension, no lean in the bends.
         */
        float z;
        if (track_under(o->x, o->y, 1, &z)) {
            o->z = z;
            slope_tilt(o->yaw, &o->pitch, &o->roll);
            if (getenv("OS_TILTCHECK")) {       /* against the four-wheel measure, which is independent */
                static float worst_roll, worst_pitch, steepest;
                static int n;
                float z4, p4, r4;
                if (car_ground(o->obj->m, o->x, o->y, o->z, o->yaw, &z4, &p4, &r4)) {
                    if (fabsf(o->roll - r4) > worst_roll) worst_roll = fabsf(o->roll - r4);
                    if (fabsf(o->pitch - p4) > worst_pitch) worst_pitch = fabsf(o->pitch - p4);
                    if (fabsf(r4) > steepest) steepest = fabsf(r4);
                }
                if (++n % 20000 == 0)
                    printf("tilt check: steepest banking met %.1f deg; one-point lean differs from the "
                           "four-wheel one by at most %.1f deg, nose-up by %.1f deg\n",
                           steepest * 57.3f, worst_roll * 57.3f, worst_pitch * 57.3f);
            }
        }
        /* 0x1cb9c again: pushed OFF the drivable area while doing more than
           3000 units/s, an opponent is stopped dead -- and stopping dead is
           more than the 5000 that makes it crash, below.  This is how you
           send one flying: shove it off the road. */
        if (o->speed > 3000.0f && !on_track(o->x, o->y)) o->speed = 0.0f;

        /*
         * 0x1cffc, 0x1d824: AN OPPONENT CRASHES when it has lost more than
         * 5000 units/s of speed since the last frame (a collision did that).
         * One of two things, by where it is: it is thrown up and tumbles --
         * the player's crash, with the same numbers, turning 12 rad/s nose
         * over tail and 12 rad/s round -- or it stays on the road and spins
         * twice round, starting at 24 rad/s.
         */
        if (!o->crash && o->prev_speed - o->speed > 5000.0f && !getenv("OS_NOCRASH")) {
            if ((((int)o->x + (int)o->y) >> 2) & 1) {
                o->crash = 4; o->turn = 0.0f; o->turn_rate = 24.0f;
            } else {
                o->crash = 1; o->bounces = 0; o->vy = 3000.0f; o->up = 0.0f;
                o->tip = o->turn = o->lean = 0.0f;
                o->tip_rate = o->turn_rate = 12.0f; o->lean_rate = 0.0f;
            }
            if (getenv("OS_OPPDUMP")) printf("opponent %d %s\n", k, o->crash == 4 ? "spins" : "tumbles");
        }
        if (getenv("OS_OPPCRASH")) {        /* "car,seconds,kind": crash an opponent on purpose (1 tumble, 4 spin) */
            static int done = 0;
            int who = 0, kind = 1; float when = 0.0f;
            sscanf(getenv("OS_OPPCRASH"), "%d,%f,%d", &who, &when, &kind);
            if (!done && who == k && g_race_clock >= when) {
                done = 1;
                if (kind == 4) { o->crash = 4; o->turn = 0.0f; o->turn_rate = 24.0f; }
                else {
                    o->crash = 1; o->bounces = 0; o->vy = 3000.0f; o->up = 0.0f;
                    o->tip = o->turn = o->lean = 0.0f;
                    o->tip_rate = o->turn_rate = 12.0f; o->lean_rate = 0.0f;
                }
            }
        }
        o->prev_speed = o->speed;
        if (o->crash == 1) {
            float d = drive_damp(97);
            o->up += o->vy * DRIVE_DT; o->vy -= 6000.0f * DRIVE_DT;
            o->tip_rate *= d;  o->tip  += o->tip_rate * DRIVE_DT;
            o->turn_rate *= d; o->turn += o->turn_rate * DRIVE_DT;
            o->lean_rate *= d; o->lean += o->lean_rate * DRIVE_DT;
            if (o->up <= 0.0f) {
                o->bounces++;
                o->up = 0.0f; o->vy = -o->vy * 0.4f;
                o->tip = o->turn = o->lean = 0.0f;
                o->lean_rate = (o->bounces & 1) ? -0.6f : 0.6f;
                o->tip_rate = -1.0f; o->turn_rate = 0.0f;
                if (o->bounces > 3) { o->crash = 0; o->tip = o->lean = 0.0f; }
            }
        } else if (o->crash == 4) {
            o->turn_rate *= drive_damp(97);
            o->turn += o->turn_rate * DRIVE_DT;
            if (o->turn >= 12.566371f || o->turn_rate < 0.05f) { o->turn = 0.0f; o->crash = 0; }
        }
        o->obj->x = o->x; o->obj->y = o->y; o->obj->z = o->z + (o->crash == 1 ? o->up : 0.0f);
        o->obj->yaw = o->yaw + o->spin + (o->crash ? o->turn : 0.0f);   /* nose is local -Z: see player_sync */
        o->obj->pitch = -o->pitch + (o->crash == 1 ? o->tip : 0.0f);
        o->obj->roll = -o->roll + (o->crash == 1 ? o->lean : 0.0f);
        o->obj->phase = g_opp_livery[k] | 0x4000 | (o->speed <= 4000.0f ? 0x2000 : 0);
        {
            int kind = g_nmark ? g_mark[((o->mark_at % g_nmark) + g_nmark) % g_nmark].kind : 0;
            g_opp_pose[k].env = kind == 0 ? 0 : kind == 1 ? 0x8000 : 0xff00;
        }
        wheels_step(&g_opp_pose[k], o->speed, 0.0f, DRIVE_DT);
        shadow_follow(g_opp_shadow[k], o->obj);
        if (g_opp_shadow[k] && o->crash == 1) {         /* the shadow stays on the road, level */
            g_opp_shadow[k]->z = o->z + 4.0f;
            g_opp_shadow[k]->pitch = -o->pitch; g_opp_shadow[k]->roll = -o->roll;
        }
    }
}

/*
 * An opponent and the lap's markers (0x1db84): like the player it looks only
 * at the marker it last reached and that one's two neighbours.  A marker
 * counts when the one before it has been passed and it has not; the line
 * with every marker passed is a lap.
 */
static void opp_markers(opponent *o)
{
    for (int d = -1; d <= 1 && g_nmark; d++) {
        int i = ((o->mark_at + d) % g_nmark + g_nmark) % g_nmark;
        const marker *m = &g_mark[i];
        float dx = o->x - m->x, dy = o->y - m->y;
        if (fabsf(dx) >= m->radius || fabsf(dy) >= m->radius) continue;
        if (sqrtf(dx * dx + dy * dy) > m->radius) continue;
        o->mark_at = i;
        if ((i != 0 && !o->mk_seen[i - 1]) || o->mk_seen[i]) {
            if (i == 0 && o->mk_count == g_nmark) {
                memset(o->mk_seen + 1, 0, sizeof o->mk_seen - 1);
                o->mk_count = 1;
                o->mk_laps++;
            }
        } else {
            o->mk_seen[i] = 1;
            o->mk_count++;
        }
    }
    o->progress = marker_progress(o->mark_at, o->mk_laps, o->x, o->y);
}

/*
 * Give the opponents their cars.  The game (0x142e8) walks the car table and
 * hands each opponent the next row, skipping the one the player took.
 */
static void opp_assign_cars(void)
{
    int row = 0;
    for (int k = 0; k < g_nopp; k++) {
        if (g_nspec == 0) break;
        if (row >= g_nspec) row = 0;
        if (row == g_spec_now) row++;
        if (row >= g_nspec) row = 0;
        if (difficulty() > 2 && g_nspec > 12) row = 12;
        int model = atoi(g_spec[row].name + 4);
        if (model < 0 || model > 6 || !car_mesh_ok[model]) model = 0;
        if (g_opp[k].obj) {
            g_opp[k].obj->m = &car_mesh[model]; g_opp[k].obj->v = model;
            g_opp[k].obj->phase = g_spec[row].livery;
            g_opp_livery[k] = g_spec[row].livery;
        }
        row++;
    }
}

static void player_sync(void);
static void attack_restart(void);
static void props_reset(void);
static void sound_restart(void);

/* line everybody up on the grid */
static void race_restart(const path *r)
{
    g_race_clock = 0.0f;
    g_player_rank = g_nopp + 1;            /* 0x12c24: the player starts last ... */
    g_laps = 0; g_last_lap = 0.0f;
    g_total_time = 0.0f; g_timer = 15.0f;
    g_clock_on = g_time_out = g_finished = g_race_on = 0;
    g_final_rank = g_game_over = 0;
    g_contact.ready = 0;
    if (getenv("OS_LAPS") && atoi(getenv("OS_LAPS")) > 0) g_total_laps = atoi(getenv("OS_LAPS"));
    if (g_mode != 1) g_total_laps = 1;
    attack_restart();
    g_perfect = 0; g_gates_done = 0;
    g_end_clock = 0.0f; g_dialog = 0; g_rpl_at = 0; g_rpl_stray = 0.0f;
    g_fade_in = 0.0f;
    if (!g_replay) { g_rpl_n = 0; g_replay_seen = 0; }
    for (int i = 0; i < g_ngate; i++) g_gate[i].done = 0;
    if (getenv("OS_TIMER")) g_timer = (float)atof(getenv("OS_TIMER"));   /* the countdown's start, for testing */
    markers_reset();
    props_reset();
    sound_restart();
    for (int k = 0; k < g_nopp; k++) {
        opponent *o = &g_opp[k];
        scene_object *keep = o->obj;
        memset(o, 0, sizeof *o);
        o->obj = keep;
        o->node = g_opp_spl[k].start;
        o->z = r->p[0][2];
        float A[2], B[2], C[2], pos[2], der[2];
        opp_piece(&g_opp_spl[k], o->node, 0, A, B, C);
        opp_curve(A, B, C, 0.0f, pos, der);
        surface_z(pos[0], pos[1], o->z, &o->z);
        o->rank = k + 1;                   /* 0x1491b: ... and the others in grid order */
        for (int it = 0; it < 12; it++) opp_place(o, k);     /* settle on the road */
    }
    /* the player: the start node of the last spline is its place on the grid */
    if (g_nopp_spl > g_player_spl && g_pobj) {
        const gspline *sp = &g_opp_spl[g_player_spl];
        const int32_t *n = sp->node[sp->start];
        float A[2], B[2], C[2], pos[2], der[2];
        opp_piece(sp, sp->start, 0, A, B, C);
        opp_curve(A, B, C, 0.0f, pos, der);
        car_spec keep = g_pc.spec;
        float wb = g_pc.wheelbase;
        drive_init(&g_pc, &keep, wb);
        g_pc.x = (float)n[1]; g_pc.y = (float)n[2];
        g_pc.yaw = atan2f(der[0], -der[1]);
        g_pc.z = r->p[0][2];
        surface_z(g_pc.x, g_pc.y, g_pc.z, &g_pc.z);
        float z, pitch, roll;
        if (car_ground(g_pobj->m, g_pc.x, g_pc.y, g_pc.z, g_pc.yaw, &z, &pitch, &roll)) {
            g_pc.z = z; g_pc.pitch = pitch; g_pc.roll = roll;
        }
        g_pat = path_nearest(r, g_pc.x, g_pc.y);
        g_chase.init = 0;
        g_pc.surface = 0.0f;
        player_sync();
    }
}

static void opponents_init(const path *r, const char *data, int track)
{
    char file[600];
    snprintf(file, sizeof file, "%s/MAP%dOPP.SPL", data, track);
    opp_splines_load(file);
    snprintf(file, sizeof file, "%s/MAP%d%c.DTR", data, track, 'A' + difficulty());
    opp_specs_load(file);
    g_nopp = g_nopp_spl - 1;                     /* the last spline is the player's */
    if (g_nopp > g_nopp_spec) g_nopp = g_nopp_spec;
    if (g_nopp < 0) g_nopp = 0;
    if (getenv("OS_NOOPP")) g_nopp = 0;
    g_player_spl = g_nopp;
    if (g_mode != 1) {                           /* 0x13ded: the last opponent's place, and nobody else */
        g_player_spl = g_nopp > 0 ? g_nopp - 1 : 0;
        g_attack_slot = g_player_spl;
        g_nopp = 0;
    }
    for (int k = 0; k < g_nopp && g_ncar < MAX_CARS; k++) {
        scene_object *c = &g_car[g_ncar++];
        memset(c, 0, sizeof *c);
        c->m = &car_mesh[0];
        c->radius = 1200.0f;
        g_opp[k].obj = c;
        c->pose = &g_opp_pose[k];
        g_opp_shadow[k] = shadow_new();
    }
    opp_assign_cars();
    race_restart(r);
    printf("opponents: %d (of %d splines, %d rows of numbers)\n", g_nopp, g_nopp_spl,
           g_nopp_spec);
    if (getenv("OS_GRIDDUMP")) {
        for (int k = 0; k < g_nopp; k++)
            printf("  grid %d  model AUTO%02d  at %.0f, %.0f, %.0f  heading %.2f\n", k,
                   g_opp[k].obj ? g_opp[k].obj->v : -1, g_opp[k].x, g_opp[k].y, g_opp[k].z, g_opp[k].yaw);
        printf("  player  model AUTO%02d  at %.0f, %.0f\n", g_pobj ? g_pobj->v : -1, g_pc.x, g_pc.y);
    }
}

/* one 1/50 s step of every opponent */
static void opponents_step(const path *r)
{
    const float dt = DRIVE_DT;
    (void)r;
    int go = (g_race_clock >= RACE_HOLD);

    for (int k = 0; k < g_nopp; k++) {
        opponent *o = &g_opp[k];
        const opp_spec *sp = &g_opp_spec[k];
        int player_is_ahead = (g_player_rank < o->rank);     /* 0x1ced0 goes by the running order */
        /* 0x24464: a whole lap ahead of this one */
        if (k == 0) g_lapped_one = 0;
        if ((o->mk_laps < g_laps && o->progress - (float)(o->mk_laps * 100) < g_progress - (float)(g_laps * 100))
            || g_laps - o->mk_laps > 1)
            g_lapped_one = 1;

        /* 0x1ced0: the speed */
        if (go) {
            opp_markers(o);
            float pace = g_nmark ? g_mark[o->mark_at].pace : 1.0f;
            float target = (sp->max_spd + (player_is_ahead ? sp->skill : 0.0f)) * pace;
            if (o->speed < target) {
                o->speed += sp->accel * dt;
                if (o->speed > target) o->speed = target;
            } else if (o->speed > target) {
                o->speed -= sp->brakes * dt;
                if (o->speed < target) o->speed = target;
            }
        }

        /* 0x1d2fc: that far along the curve */
        float left = o->speed * dt;
        const gspline *spl = &g_opp_spl[k];
        for (int guard = 0; guard < 8 && left > 0.0f; guard++) {
            float A[2], B[2], C[2], pos[2], der[2];
            opp_piece(spl, o->node, o->half, A, B, C);
            opp_curve(A, B, C, o->u, pos, der);
            float len = sqrtf(der[0] * der[0] + der[1] * der[1]);
            if (len < 1.0f) len = 1.0f;
            float du = left / len;
            if (o->u + du < 1.0f) { o->u += du; break; }
            left -= (1.0f - o->u) * len;
            o->u = 0.0f;
            if (o->half == 0) o->half = 1;
            else {
                o->half = 0;
                int nx = spl->node[o->node][4];
                o->node = (nx >= 0 && nx < spl->n) ? nx : 0;
            }
        }

        /* 0x1d6ac: the push from a collision carries on for a moment and
           fades; then, three seconds after the last one, the car drifts back
           to its line.  0x1df24: a car knocked round straightens up. */
        if (o->shove > 0.0f) {
            o->offx += o->shove_x * o->shove * dt;
            o->offy += o->shove_y * o->shove * dt;
            o->shove *= drive_damp(79);
            if (o->shove < 10.0f) o->shove = 0.0f;
            o->off_age = 0.0f;
        }
        /*
         * 0x1df24: IN A BEND an opponent's nose turns in and its body leans a
         * little.  v = (speed x sband)^2 x (how far its heading turned this
         * frame), held to +-2; its heading offset eases to v x 0.5 (so a
         * knock from a collision, which sets the same offset, dies away
         * towards that), and the body blocks to v x 0.02 of roll and of pitch
         * -- at most 2.3 degrees.  sband is the last number of its OPPONENTS row.
         */
        {
            float v = o->speed * g_opp_spec[k].sband;
            v = v * v * wrap_pi(o->yaw - o->prev_yaw) * (50.0f / 30.0f);
            if (v > 2.0f) v = 2.0f;
            if (v < -2.0f) v = -2.0f;
            if (!go) v = 0.0f;
            o->prev_yaw = o->yaw;
            o->spin = (o->spin - v * 0.5f) * drive_damp(0x4f) + v * 0.5f;
            o->body = (o->body - v * 0.02f) * drive_damp(0x4c) + v * 0.02f;
            g_opp_pose[k].body_roll = o->body;
            g_opp_pose[k].body_pitch = o->body;
        }
        if (o->offx != 0.0f || o->offy != 0.0f) {
            o->off_age += dt;
            if (o->off_age >= 3.0f) {
                float kk = drive_damp(0x60);
                o->offx *= kk; o->offy *= kk;
                if (fabsf(o->offx) < 1.0f && fabsf(o->offy) < 1.0f) o->offx = o->offy = 0.0f;
            }
        }

        opp_place(o, k);

    }
    /*
     * THE RUNNING ORDER (0x20418).  Every car has a place; the player starts
     * last and the others in grid order.  Once the race is on and the player
     * has crossed the line, each frame the first opponent found on the wrong
     * side of the player -- further on but with a worse place, or the other
     * way round -- changes places with it.  One exchange a frame; opponents
     * never change places among themselves.
     */
    if (go && g_clock_on)
        for (int k = 0; k < g_nopp; k++)
            if ((g_progress - g_opp[k].progress) * (float)(g_opp[k].rank - g_player_rank) < 0.0f) {
                int t = g_opp[k].rank; g_opp[k].rank = g_player_rank; g_player_rank = t;
                break;
            }
}

/*
 * CAR AGAINST CAR, from the game (0x12378 / 0x173f8 / 0x17818 for the player,
 * 0x126e4 / 0x1cffc / 0x1d1f0 between opponents, 0x1e404 for overtaking).
 *
 * Every car is TWO CIRCLES on its centre line, a quarter of the wheelbase in
 * from each axle, each a quarter of the wheelbase in radius (0x136c8 builds
 * them).  Two cars touch when a circle of one overlaps a circle of the other.
 */
static void car_circles(const mesh *mm, float x, float y, float yaw,
                        float cx[2], float cy[2], float *radius)
{
    /*
     * 0x136c8 (player) and 0x142e8 (opponents) build them from the BODY's
     * corners, not from the axles: the nose is 250 units ahead of the front
     * wheels and the tail 150 behind the rear ones (the same corners the wall
     * test uses).  With L that whole length, the two circles sit a quarter of
     * L in from each end and are a quarter of L in radius -- 334 units on
     * AUTO00, against a half-width of 364.
     *
     * The port first took a quarter of the WHEELBASE in from each axle (234
     * units): circles a third too small and 250 short of the nose, so a car
     * could put its nose or its flank well inside another before anything
     * happened (Danik).
     */
    float nose = mm->axle_front + 250.0f, tail = mm->axle_rear + 150.0f;
    float quarter = (nose + tail) * 0.25f;
    float fx = sinf(yaw), fy = -cosf(yaw);
    float zf = nose - quarter, zr = -(tail - quarter);       /* distances ahead of the car's origin */
    cx[0] = x + fx * zf; cy[0] = y + fy * zf;
    cx[1] = x + fx * zr; cy[1] = y + fy * zr;
    *radius = quarter;
}

/*
 * The player touches an opponent (0x17818).  `nx, ny` points from the
 * opponent's circle to the player's, `pen` is how far they overlap.
 *
 *  - It is the OPPONENT that is moved out of the way -- unless that would put
 *    it off the track, in which case the player is moved instead.
 *  - Opponent in front (the player ran into it): it is shoved on at half the
 *    player's speed and knocked round by speed/10000 radians; the player
 *    loses 5% of its speed for every frame of contact, is shoved back at
 *    0.21 of its speed and has its nose pushed away from the other car.
 *  - Opponent behind (it ran into the player): it drops to 70% of its speed.
 */
static void player_hits(opponent *o, float nx, float ny, float pen)
{
    car_state *c = &g_pc;
    float px = nx * pen, py = ny * pen;
    g_car_contact = 1;
    /* 0x17818 opens with the CRASH sample, started afresh on every frame of
       contact; the port steps 50 times a second, so at the game's 30 */
    {
        static float last = -1.0f;
        if (g_race_clock < last || g_race_clock - last >= 1.0f / 30.0f) {
            snd_play(SND_CRASH);
            rumble_kick(0.55f, 0.35f, 0.1f);
            last = g_race_clock;
        }
    }
    if (!on_track(o->x - px, o->y - py)) {
        c->x += px; c->y += py;
        return;
    }
    float fx = sinf(c->yaw), fy = -cosf(c->yaw);
    float ahead = (o->x - c->x) * fx + (o->y - c->y) * fy;
    if (ahead >= 0.0f) {
        float ofx = sinf(o->yaw), ofy = -cosf(o->yaw);
        float orx = -cosf(o->yaw), ory = -sinf(o->yaw);          /* its right */
        float rel_x = c->x - o->x, rel_y = c->y - o->y;
        int player_left   = (rel_x * orx + rel_y * ory) < 0.0f;
        int player_behind = (rel_x * ofx + rel_y * ofy) < 0.0f;
        o->shove = c->speed * 0.5f;
        float turn = c->speed * 0.0001f;
        if (turn > 1.5707964f) turn = 1.5707964f;
        o->spin = (player_behind ? 1.0f : -1.0f) * (player_left ? 1.0f : -1.0f) * turn;
        c->shove = c->speed * 0.21f;
        c->shove_x = nx; c->shove_y = ny;
        c->yaw_rate = c->speed * (player_left ? 0.0002f : -0.0002f);
        c->speed *= drive_damp(90);
        g_wall_hits++;
    } else {
        o->speed *= 0.7f;
    }
    o->shove_x = -nx; o->shove_y = -ny;
    o->offx -= px; o->offy -= py;
    o->off_age = 0.0f;
}

static void cars_collide(void)
{
    car_state *c = &g_pc;
    if (!g_pobj) return;
    float pcx[2], pcy[2], pr;
    car_circles(g_pobj->m, c->x, c->y, c->yaw, pcx, pcy, &pr);
    /* 0x175aa / 0x177f7: +0x7c4 eases to 1 while touching a car, to 0 when
       not (stiffness 0x32); the commentator speaks when it is over 0.8 */
    {
        float kk = drive_damp(0x32);
        g_hitcar = (g_hitcar - (float)g_car_contact) * kk + (float)g_car_contact;
        g_car_contact = 0;
    }

    for (int k = 0; k < g_nopp; k++) {
        opponent *o = &g_opp[k];
        if (!o->obj) continue;
        float ocx[2], ocy[2], orad;
        car_circles(o->obj->m, o->x, o->y, o->yaw, ocx, ocy, &orad);

        /* the player and this opponent (0x12378: only within 1500 of each other) */
        if (fabsf(c->x - o->x) < 1500.0f && fabsf(c->y - o->y) < 1500.0f &&
            fabsf(c->z - o->z) < 600.0f) {
            for (int a = 0; a < 2; a++)
                for (int b = 0; b < 2; b++) {
                    float dx = pcx[a] - ocx[b], dy = pcy[a] - ocy[b];
                    float dist = sqrtf(dx * dx + dy * dy);
                    if (dist < 1.0f) dist = 1.0f;
                    if (dist < pr + orad) {
                        player_hits(o, dx / dist, dy / dist, pr + orad - dist);
                        car_circles(o->obj->m, o->x + 0, o->y + 0, o->yaw, ocx, ocy, &orad);
                        break;
                    }
                }
        }

        /*
         * 0x1e404: OVERTAKING.  The opponent looks three radii ahead of
         * itself; if the player is there and going slower, it moves its line
         * over by just enough to clear -- that is all its steering is.
         */
        {
            float fx = sinf(o->yaw), fy = -cosf(o->yaw);
            float qx = o->x + fx * orad * 3.0f, qy = o->y + fy * orad * 3.0f;
            if (fabsf(qx - c->x) < 5000.0f && fabsf(qy - c->y) < 5000.0f &&
                c->speed < o->speed && fabsf(c->z - o->z) < 600.0f) {
                float thr = orad * 2.0f + pr;
                for (int a = 0; a < 2; a++) {
                    float dx = qx - pcx[a], dy = qy - pcy[a];
                    float dist = sqrtf(dx * dx + dy * dy);
                    if (dist < thr) {
                        dist += 1.0f;
                        o->offx += thr * dx / dist - dx;
                        o->offy += thr * dy / dist - dy;
                        o->off_age = 0.0f;
                    }
                }
            }
        }

        /* this opponent and the ones after it (0x1cffc / 0x1d1f0): the faster
           of the two drops to their average speed, the other is moved over */
        for (int j = k + 1; j < g_nopp; j++) {
            opponent *q = &g_opp[j];
            if (!q->obj) continue;
            if (fabsf(o->x - q->x) >= 1500.0f || fabsf(o->y - q->y) >= 1500.0f) continue;
            float qcx[2], qcy[2], qrad;
            car_circles(q->obj->m, q->x, q->y, q->yaw, qcx, qcy, &qrad);
            for (int a = 0; a < 2; a++)
                for (int b = 0; b < 2; b++) {
                    float dx = ocx[a] - qcx[b], dy = ocy[a] - qcy[b];
                    float dist = sqrtf(dx * dx + dy * dy);
                    if (dist < 1.0f) dist = 1.0f;
                    if (dist >= orad + qrad) continue;
                    float avg = (o->speed + q->speed) * 0.5f;
                    if (q->speed <= o->speed) o->speed = avg; else q->speed = avg;
                    float pen = orad + qrad - dist;
                    q->offx -= dx / dist * pen;
                    q->offy -= dy / dist * pen;
                    q->off_age = 0.0f;
                }
        }
    }
}

/* ------------------------------------------------------------ the dashboard */

/*
 * NEEDLES AND GEAR LAMP (0x236dc..0x237ee, needle 0x2390c).
 *
 * Where they go is in CARS.DTR, one COCKnn{L,H} section per car model:
 *     needle length, first gear-lamp sprite
 *     speedometer pivot x y, rev-counter pivot x y
 *     lamp position for N, 1, 2, 3, 4, 5, 6
 * The speed needle sits at  speed / max speed * 4.084 rad, the rev needle at
 * rpm / max rpm * 3.613 rad (and no further), each eased with stiffness 0x5a;
 * a needle is drawn at that angle less 2 radians, measured clockwise from
 * straight up, as three lines fanning from the pivot to one tip.
 *
 * The lamp is a sprite from GAMEL.SPR: the dashboard's first lamp + gear.
 */
static int   g_dash[29];
static int   g_have_dash;
static float g_needle_speed, g_needle_rpm;

static void dash_load(int model)
{
    char want[16];
    snprintf(want, sizeof want, "COCK%02d%c", model, g_s == 2 ? 'H' : 'L');
    g_have_dash = 0;
    FILE *f = fopen("build/dec/GEN/CARS.DTR", "r");
    if (!f) return;
    char line[256];
    int in = 0, n = 0;
    while (fgets(line, sizeof line, f) && n < 29) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (!in) { if (!strncmp(p, want, strlen(want))) in = 1; continue; }
        if (!strncmp(p, "END_LIST", 8)) break;
        for (char *tok = strtok(p, " \t\r\n"); tok && n < 29; tok = strtok(NULL, " \t\r\n")) {
            if (tok[0] == '*') break;
            if (tok[0] == 'd') g_dash[n++] = atoi(tok + 1);
        }
    }
    fclose(f);
    g_have_dash = (n == 29);
}

static void dash_line(int x0, int y0, int x1, int y1, uint32_t col)
{
    int dx = abs(x1 - x0), dy = abs(y1 - y0), n = dx > dy ? dx : dy;
    for (int i = 0; i <= n; i++) {
        int x = n ? x0 + (x1 - x0) * i / n : x0, y = n ? y0 + (y1 - y0) * i / n : y0;
        if (g_yd == 2) { if (y & 1) continue; y >>= 1; }
        y += g_oy;
        if (x >= 0 && y >= 0 && x < FB_W && y < FB_H) fb[y * FB_W + x] = col;
    }
}

static void dash_needle(int px, int py, int len, float angle, int colour)
{
    float a = angle - 2.0f;
    px *= g_s; py *= g_s; len *= g_s;
    int tx = (int)((float)px + (float)len * sinf(a));
    int ty = (int)((float)py - (float)len * cosf(a));
    uint32_t col = g_pal[colour];
    dash_line(px, py, tx, ty, col);
    float b = a + 1.5707964f;
    dash_line((int)((float)px + sinf(b)), (int)((float)py - cosf(b)), tx, ty, col);
    dash_line((int)((float)px - sinf(b)), (int)((float)py + cosf(b)), tx, ty, col);
}

/* one 1/50 s step of the needles' easing */
static void dash_step(void)
{
    const car_state *c = &g_pc;
    float k = drive_damp(0x5a);
    float ws = c->speed / c->spec.maxspd * 4.0840706f;
    /* the rev counter's sweep: 3.613 rad on a dashboard, pi on the outside
       views' instrument (0x2367c, modes 1 and 2) */
    float full = g_views[g_view].cockpit ? 3.6128317f : 3.1415927f;
    float wr = c->rpm / c->spec.maxrpm * full;
    if (wr > full) wr = full;
    g_needle_speed = ws + (g_needle_speed - ws) * k;
    g_needle_rpm   = wr + (g_needle_rpm - wr) * k;
}

/* ------------------------------------------------------------------- HUD */

/*
 * GAMEL.FNT (loader 0x255f8, drawing 0x24fc0, measuring 0x25448):
 *
 *   byte   number of fonts - 1                          (7 fonts)
 *   per font, 12 bytes:  int32 height, first character, number of characters
 *   per font, per character, 8 bytes:  int32 width, offset into the pixels
 *   int32  size of the pixels, then the pixels
 *
 * A glyph is `height` rows of (width + 1) / 2 bytes, TWO PIXELS PER BYTE, the
 * high half first.  A pixel is a shade 1..15 (0 = see-through) and goes on the
 * screen as  colour * 16 + shade : the text colour picks one of the palette's
 * sixteen-colour ramps.  A character is followed by one empty column, and
 * preceded by one more unless switched off; in a string, byte 1 switches to
 * fixed pitch (every character as wide as the font's widest + 2, centred),
 * 2 back to proportional, 3 / 4 the extra column off / on.  Characters the
 * font does not have are skipped without moving on.
 *
 * GAMEL.SPR (loader 0x25750, drawing 0x25ab0):  int32 count, int32 size of
 * the pixels, count x 20 bytes [x offset, y offset, width, height, offset
 * into the pixels], then the pixels: one palette index each, 0 = see-through.
 *   0-3    the start lights (three out / one red / two red / green)
 *   4, 5   the radar's player arrow and opponent dot
 *   7-54   gear lamps N 1..6 in the sizes the dashboards use
 */
#define FONTS_MAX 12                 /* (Screamer 2's GAMEL.FNT has ten) */
typedef struct { int height, rows, first, count; int32_t (*glyph)[2]; int widest; } hud_font;
static hud_font g_font[FONTS_MAX];
static int      g_nfont;
static uint8_t *g_font_px;
typedef struct { int32_t ox, oy, w, h, at; } hud_sprite;
static hud_sprite *g_spr;
static int         g_nspr;
static uint8_t    *g_spr_px;
static int16_t     g_implet[FB_MAXW * FB_MAXH], g_impres[FB_MAXW * FB_MAXH], g_cdfront[FB_MAXW * FB_MAXH];
static int         g_have_cdfront;
static int         g_have_implet, g_have_impres;
/* GAME.TXT of LOCAL.DAT, one line each: 0 POSITION, 1-10 1ST..10TH, 11 GREAT!,
   12 NEW RECORD!, 13 TRY AGAIN, 14 YOU MADE IT!, 15 PERFECT!, ... 22 TIME OVER
   (the game keeps pointers to them from 0x18f90) */
#define GAME_TEXTS 26
static char g_text[GAME_TEXTS][32];

static int g_window_closed;
static const char *g_fnt_file = "build/dec/GAME/GAMEL.FNT";
static const char *g_spr_file = "build/dec/GAME/GAMEL.SPR";

static void hud_load(void)
{
    g_pkc_loads++;
    g_nfont = g_nspr = 0;
    FILE *f = fopen(res_name(g_fnt_file), "rb");
    if (f) {
        int n = fgetc(f) + 1;
        if (os_game() == 2) n--;                        /* (Screamer 2's first byte is the number itself: nine) */
        if (n > 0 && n <= FONTS_MAX) {
            for (int i = 0; i < n; i++) {
                int32_t h[3];
                if (fread(h, 4, 3, f) != 3) { n = 0; break; }
                /* SCREAMER 2: a fourth number (19 for a font 18 rows high: unread) and the
                   picture's name, 16 letters, after the three */
                if (os_game() == 2) { char more[20]; if (fread(more, 1, 20, f) != 20) { n = 0; break; } }
                g_font[i].rows = h[0]; g_font[i].height = h[0] / g_s; g_font[i].first = h[1]; g_font[i].count = h[2];
            }
            for (int i = 0; i < n; i++) {
                hud_font *ft = &g_font[i];
                ft->glyph = malloc(sizeof *ft->glyph * (size_t)ft->count);
                if (fread(ft->glyph, 8, (size_t)ft->count, f) != (size_t)ft->count) { n = 0; break; }
                ft->widest = 1;                         /* as 0x250d0 starts it */
                for (int k = 0; k < ft->count; k++)
                    if (ft->glyph[k][0] > ft->widest) ft->widest = ft->glyph[k][0];
            }
            int32_t size = 0;
            if (n && fread(&size, 4, 1, f) == 1 && size > 0) {
                g_font_px = malloc((size_t)size);
                if (fread(g_font_px, 1, (size_t)size, f) == (size_t)size) g_nfont = n;
            }
        }
        fclose(f);
    }
    f = fopen(res_name(g_spr_file), "rb");
    if (f) {
        int32_t h[2];
        if (fread(h, 4, 2, f) == 2 && h[0] > 0 && h[0] < 1024 && h[1] > 0) {
            g_spr = malloc(sizeof *g_spr * (size_t)h[0]);
            g_spr_px = malloc((size_t)h[1]);
            if (fread(g_spr, 20, (size_t)h[0], f) == (size_t)h[0] &&
                fread(g_spr_px, 1, (size_t)h[1], f) == (size_t)h[1]) g_nspr = h[0];
        }
        fclose(f);
    }
    f = fopen("build/dec/LOCAL/GAME.TXT", "r");
    if (f) {
        for (int i = 0; i < GAME_TEXTS && fgets(g_text[i], sizeof g_text[i], f); i++)
            g_text[i][strcspn(g_text[i], "\r\n")] = 0;
        fclose(f);
    }
    if (os_game() != 2) {                    /* (Screamer's race screen's pictures: Screamer 2 has none of them) */
        g_have_implet = pkc_load("build/dec/GAME/IMPLET_L.PKC", g_implet);
        g_have_cdfront = pkc_load("build/dec/GAME/CDFRONTL.PKC", g_cdfront);
        g_have_impres = pkc_load("build/dec/GAME/IMPRES_L.PKC", g_impres);
    } else g_have_implet = g_have_cdfront = g_have_impres = 0;
    if (!g_nfont || !g_nspr) fprintf(stderr, "note: no HUD font / sprites in build/dec/GAME\n");
}

static void hud_put(int x, int y, int idx)
{
    if (g_yd == 2) y >>= 1;                          /* (a wide picture: two rows of the high set to one of the screen) */
    y += g_oy;
    if (x < g_clip[0] || x >= g_clip[1] || y < g_clip[2] || y >= g_clip[3]) return;
    fb[y * FB_W + x] = g_pal[idx & 255];
    if (pxi) pxi[y * FB_W + x] = (int16_t)(idx & 255);   /* (the colour's number too: what is darkened, blended, faded) */
}

/* 0x24fc0 with draw = 1, 0x25448 with draw = 0.  Positions and the width
   returned are in 320 x 200 units whatever the picture's size */
/* SCREAMER 2's text routine (0x50f45) has a seventh argument: what is left between two
   letters, in pixels (-1 for the small font, 1 or 2 for the figures).  NO_SPACE = Screamer's
   own fixed spacing. */
#define NO_SPACE (-99)
static int g_hud_space = NO_SPACE;

/* Screamer 2's menu in high resolution: the game's routines give places, gaps and widths in the
   screen's own pixels (STARTH doubles them itself, docs/NOTES2.md S2-9) -- hud_text then scales
   nothing */
static int g_hud_native;
static int hud_text(int x, int y, const char *str, int font, int colour, int draw)
{
    if (font < 0 || font >= g_nfont) return 0;
    const hud_font *ft = &g_font[font];
    const int s_ = g_hud_native ? 1 : g_s;
    x *= s_; y *= s_;
    /* (figures in a column, after a byte 1: every one takes the widest letter's room and
       what is left between two -- in Screamer 2 the routine's own argument, which makes
       the clocks' figures 5 apart in the small font, as the original's picture has them) */
    int cell = ft->widest + (g_hud_space != NO_SPACE ? g_hud_space * s_ : 2 * s_), fixed = 0, gap = s_, x0 = x;
    for (const unsigned char *p = (const unsigned char *)str; *p; p++) {
        int ch = *p;
        if (ch < ft->first || ch >= ft->first + ft->count) {
            if (ch == 1) fixed = 1; else if (ch == 2) fixed = 0;
            else if (ch == 3) gap = 0; else if (ch == 4) gap = s_;
            continue;
        }
        int w = ft->glyph[ch - ft->first][0];
        int gx = x + (g_hud_space != NO_SPACE ? 0 : gap) + (fixed ? (cell - (w + s_)) / 2 : 0);
        if (draw) {
            const uint8_t *px = g_font_px + ft->glyph[ch - ft->first][1];
            int bw = (w + 1) / 2;
            for (int r = 0; r < ft->rows; r++)
                for (int c = 0; c < w; c++) {
                    int b = px[r * bw + c / 2];
                    int shade = (c & 1) ? (b & 15) : (b >> 4);
                    if (!shade) continue;
                    if (g_yd == 2 && ((y + r) & 1) && r > 0) {
                        /* WIDE: the odd row shares a screen row with the even
                           one above it, which is the one that shows; the odd
                           row only fills what that one leaves open, so that
                           no stroke one row thick is lost */
                        int above = px[(r - 1) * bw + c / 2];
                        if ((c & 1) ? (above & 15) : (above >> 4)) continue;
                    }
                    hud_put(gx + c, y + r, colour * 16 + shade);
                }
        }
        x += fixed ? cell : g_hud_space != NO_SPACE ? w + g_hud_space * s_ : w + s_ + gap;
    }
    return (x - x0 + s_ - 1) / s_;
}

/* 0x2e598: a string centred on the screen */
static void hud_text_centred(int y, const char *str, int font, int colour)
{
    hud_text((320 - hud_text(0, 0, str, font, colour, 0)) / 2, y, str, font, colour, 1);
}

/* a sprite at a place given in the picture's own pixels */
static void hud_sprite_px(int x, int y, int n)
{
    if (n < 0 || n >= g_nspr) return;
    const hud_sprite *sp = &g_spr[n];
    for (int r = 0; r < sp->h; r++)
        for (int c = 0; c < sp->w; c++) {
            int v = g_spr_px[sp->at + r * sp->w + c];
            if (!v) continue;
            if (g_yd == 2 && ((y + sp->oy + r) & 1) && r > 0 && g_spr_px[sp->at + (r - 1) * sp->w + c]) continue;   /* (as in hud_text) */
            hud_put(x + sp->ox + c, y + sp->oy + r, v);
        }
}

/* ... and at a place in 320 x 200 units */
static void hud_sprite_draw(int x, int y, int n)
{
    hud_sprite_px(x * g_s, y * g_s, n);
}

static void hud_clock(char *out, size_t n, float t)
{
    int whole = (int)t;                 /* the game's float -> int (0x4d7b4) cuts, it does not round */
    int hund = (int)((t - (float)whole) * 100.0f);
    snprintf(out, n, "%02d:%02d:%02d", whole / 60, whole % 60, hund);
}
/* Screamer 2's (0x37e06): the same, but "no time yet" (3599.999, the lists' empty row) is
   "--:--:--" */
static void s2_clock(char *out, size_t n, float t)
{
    uint32_t bits;
    memcpy(&bits, &t, 4);
    if (bits == 0x4560fffcu) snprintf(out, n, "--:--:--");
    else hud_clock(out, n, t);
}

/*
 * The radar (0x23a68; the lines are 0x1057c): a 72 x 72 window at (4, 124),
 * the picture under it darkened to three quarters (each pixel becomes the
 * palette's nearest colour to 0.75 of itself -- the table built at 0x25870),
 * the track's WALL SEGMENTS drawn as lines in colour 255 turned so that the
 * car's nose is up, every opponent a dot (sprite 5) and the player the arrow
 * (sprite 4) in the middle.  One pixel is 32768 / zoom units, zoom = 24, 48
 * or 72 from the game's options (0x1d2a0; 0 there = no radar).
 */
static int g_radar_zoom = 24;       /* 0x20810 starts at 1 = the widest of the three (0x23a85); M steps on */

static void hud_radar(void)
{
    if (!g_radar_zoom) return;
    const car_state *c = &g_pc;
    g_clip[0] = 4 * g_s; g_clip[1] = 0x4c * g_s; g_clip[2] = 0x7c * SY + g_oy; g_clip[3] = 0xc4 * SY + g_oy;
    /* The search through the palette for every pixel of the window was 45 %
       of a hi-res frame.  The answer only depends on the pixel's colour, so
       it is looked up once a colour and remembered (the game does the same
       thing with a table built once, 0x25870). */
    static uint32_t seen[4096], gives[4096], pal_sum;
    static int      seen_ok[4096];
    uint32_t sum = 0;
    for (int i = 0; i < 256; i++) sum = sum * 31u + g_pal[i];
    if (sum != pal_sum) { pal_sum = sum; memset(seen_ok, 0, sizeof seen_ok); }
    for (int y = g_clip[2]; y < g_clip[3]; y++)
        for (int x = g_clip[0]; x < g_clip[1]; x++) {
            uint32_t p = fb[y * FB_W + x];
            uint32_t h = ((p >> 16 & 255) * 7u + (p >> 8 & 255) * 61u + (p & 255) * 131u) & 4095u;
            if (seen_ok[h] && seen[h] == p) { fb[y * FB_W + x] = gives[h]; continue; }
            int r = (int)((p >> 16) & 255) * 3 / 4, g = (int)((p >> 8) & 255) * 3 / 4,
                b = (int)(p & 255) * 3 / 4, best = 0;
            long bd = 1L << 30;
            for (int i = 0; i < 256; i++) {
                int dr = (int)((g_pal[i] >> 16) & 255) - r, dg = (int)((g_pal[i] >> 8) & 255) - g,
                    db = (int)(g_pal[i] & 255) - b;
                long d = (long)dr * dr + dg * dg + db * db;
                if (d < bd) { bd = d; best = i; }
            }
            seen[h] = p; gives[h] = g_pal[best]; seen_ok[h] = 1;
            fb[y * FB_W + x] = g_pal[best];
        }
    float s = (float)(g_radar_zoom * g_s) / 32768.0f, cs = cosf(c->yaw), sn = sinf(c->yaw);
    for (int i = 0; g_scene && i < g_scene->nwall; i++) {
        const scene_wall *w = &g_scene->wall[i];
        float ax = w->x1 - c->x, ay = w->y1 - c->y, bx = w->x2 - c->x, by = w->y2 - c->y;
        if (fabsf(ax) > 70000.0f || fabsf(ay) > 70000.0f) continue;
        int x0 = 40 * g_s - (int)floorf((ax * cs + ay * sn) * s), y0 = 160 * g_s + (int)floorf((ay * cs - ax * sn) * s);
        int x1 = 40 * g_s - (int)floorf((bx * cs + by * sn) * s), y1 = 160 * g_s + (int)floorf((by * cs - bx * sn) * s);
        int dx = abs(x1 - x0), dy = abs(y1 - y0), n = dx > dy ? dx : dy;
        for (int k = 0; k <= n; k++)
            hud_put(n ? x0 + (x1 - x0) * k / n : x0, n ? y0 + (y1 - y0) * k / n : y0, 255);
    }
    for (int k = 0; k < g_nopp; k++) {
        float dx = g_opp[k].x - c->x, dy = g_opp[k].y - c->y;
        hud_sprite_px((int)(39.0f * (float)g_s - (dy * sn + dx * cs) * s),
                      (int)(159.0f * (float)g_s + (dy * cs - dx * sn) * s), 5);
    }
    hud_sprite_draw(0x26, 0x9e, 4);
    g_clip[0] = 0; g_clip[1] = FB_W; g_clip[2] = 0; g_clip[3] = FB_H;
}

/*
 * The race screen (0x23400, and 0x2367c for the instruments).  IMPLET_L.PKC is
 * the sheet of labels -- LAP, POSITION, TOTAL, LAP TIME, BEST LAP -- and the
 * numbers go beside and under them:
 *
 *   (8, 11)    font 1 colour 5   lap, with the font's ':' which is drawn as '/'
 *   after it   font 4 colour 5   number of laps, at y 20
 *   (276, 11)  font 1 colour 5   position
 *   centred, y 8, font 6 colour 2   the countdown, whole seconds
 *   (8, 56) (8, 72) (8, 88)  font 3 colour 2   total, lap and best lap time
 *
 * Instruments: in the cockpit view the dashboard picture, two needles and the
 * gear lamp (a sprite: the dashboard's first lamp + gear).  In the other
 * views IMPRES_L.PKC, a rev counter with its needle pivoted at (271, 168),
 * 30 long, colour 0x75; the speed as three fixed-pitch digits at (271, 187),
 * font 2 colour 7, which is speed x 0.017; the gear at (304, 187), colour 1.
 *
 * The start lights (0x20d45): sprite number  whole seconds / 2  at (120, 54)
 * -- out, one red, two red, and at 6 s green, which is the start; they go
 * at 8 s.
 */
/*
 * THE REPLAY (0x22c10 / 0x22f34 record, 0x22e74 play, 0x1fd64 the loop round
 * the race, 0x219d0 the question; build/decomp/replay.c).
 *
 * The game does not record where the cars were: it records the CONTROLS, 20
 * bytes a frame -- the frame's two time values and six 16-bit control words
 * (0x1987c..0x19890) -- and after a normal race that was finished (not one
 * run out of time or left, and not the special modes) it writes them to
 * REPLAYS\TEMP.RPL and RUNS THE RACE AGAIN from them, everything else being
 * worked out afresh.  It asks for view-table row 3, the trackside cameras
 * (0x12afe), cuts instead of flying between views, leaves the instruments,
 * clocks and radar off the screen and blinks an "R" at (8, 170) (0x2346c).
 * When the recording runs out the race is left (0x22dd8), and "PLAY AGAIN?
 * YES / NO" is asked over the darkened picture.
 *
 * Port: the same -- the controls of every 1/50 s step are kept and the race
 * is run again from them (OS_REPLAYCHECK=1 prints how far the second run
 * strays from the first).  Not written to a file.
 */
#define REPLAY_MAX 60000                         /* twenty minutes */
typedef struct { drive_input in; float x, y; int reset; } replay_step;
static int g_reset_key;
static replay_step *g_rec;

/*
 * A REPLAY KEPT IN A FILE.  The game's (0x1b728 writes, 0x1b830 reads) is its
 * own recording of the controls with a header of count / car / laps /
 * difficulty / mode / options; the port's is the same idea in its own format
 * -- the two programs' physics are not step for step the same, so neither
 * could play the other's -- and in its own directory, openscreamer.replays.
 */
typedef struct { char magic[8]; int track, reverse, car, laps, diff, cheat, steps; } replay_head;
static int g_rpl_file;                           /* this run only plays a file back */

static int replay_write(const char *file, int track)
{
    if (!g_rec || g_rpl_n <= 0) return 0;
    mkdir(os_path("openscreamer.replays"), 0755);
    FILE *f = fopen(file, "wb");
    if (!f) return 0;
    replay_head h = { "OSRPL01", track, g_reverse, g_spec_now, g_total_laps,
                      getenv("OS_DIFF") ? atoi(getenv("OS_DIFF")) : 1, g_cheat, g_rpl_n };
    int ok = fwrite(&h, sizeof h, 1, f) == 1 && fwrite(g_rec, sizeof *g_rec, (size_t)g_rpl_n, f) == (size_t)g_rpl_n;
    fclose(f);
    if (!ok) remove(os_path(file));
    return ok;
}

static int replay_head_read(const char *file, replay_head *h)
{
    FILE *f = fopen(file, "rb");
    if (!f) return 0;
    int ok = fread(h, sizeof *h, 1, f) == 1 && !memcmp(h->magic, "OSRPL01", 8)
             && h->steps > 0 && h->steps <= REPLAY_MAX && h->track >= 1 && h->track <= 6;
    fclose(f);
    return ok;
}

static int replay_read(const char *file)
{
    replay_head h;
    if (!replay_head_read(file, &h)) return 0;
    FILE *f = fopen(file, "rb");
    if (!f) return 0;
    if (!g_rec) g_rec = calloc(REPLAY_MAX, sizeof *g_rec);
    fseek(f, (long)sizeof h, SEEK_SET);
    int ok = fread(g_rec, sizeof *g_rec, (size_t)h.steps, f) == (size_t)h.steps;
    fclose(f);
    if (ok) g_rpl_n = h.steps;
    return ok;
}
static int   g_dialog_sel, g_rpl_view = -1;


/*
 * THE CD PLAYER (0x21328; P in a race, not in the network game).
 *
 * A screen over the stopped, twice-darkened race: the picture CDFRONTL.PKC, a
 * row of the disc's track numbers (font 5 colour 9, 13 apart from (64, 78),
 * the chosen one blinking), and seven buttons along y 97 -- a sprite drawn
 * over the one the cursor is on, its "pressed" twin for half a second after
 * ENTER (table at 0x1f1c: x, sprite, pressed sprite):
 *     0 play: leave, and the chosen track plays     1 eject (to change the disc)
 *     2 / 3 previous / next track
 *     4 / 5 the chosen volume down / up             6 which volume: CD, FX, MU
 * For 4..6 the volume is shown: its name at (64, 69) and a bar of value /
 * 1024 strokes 3 apart from x 85, coloured in tens (0x1fa0).  The three are
 * the game's master volumes (0x1d2bc CD audio, 0x1d2b8 digital effects,
 * 0x1d2c0 MIDI music; all start at 0x6000 of 0x7fff, saved with the options).
 * While the screen is up all three fade to nothing at 16384 a second and the
 * music stops; leaving puts them back.  LEFT / RIGHT move, ENTER presses,
 * ESC leaves.
 *
 * Port: there is no disc.  The seven songs (MUSIC1..7.HMP) stand in for its
 * tracks, and since the port's music is that one stream, the CD and MU
 * volumes both act on it.  Eject does nothing.  A press of - / + is one
 * stroke of the bar (the game's is 48000 a second of key).
 */
static struct { int on, sel, chan, track; float press, fade; } g_cd;
static int     g_song_now;
static void champ_darken(int x0, int y0, int w, int h);

static void volumes_apply(float fade)
{
    snd_masters((float)g_cfg.vol_fx / 24576.0f * fade,
                (float)g_cfg.vol_cd / 24576.0f * (float)g_cfg.vol_mu / 24576.0f * fade);
}

static void cd_open(void)
{
    memset(&g_cd, 0, sizeof g_cd);
    g_cd.on = 1; g_cd.fade = 1.0f;
    g_cd.track = (g_song_now >= 1 && g_song_now <= 7) ? g_song_now : 1;
}

static void cd_close(void)
{
    g_cd.on = 0;
    cfg_save();
    volumes_apply(1.0f);
    g_song_now = g_cd.track;
    if (!getenv("OS_NOMUSIC")) snd_music("build/dec/MUSIC", g_song_now);
}

static void cd_key(int key)
{
    static int *const vol[3] = { &g_cfg.vol_cd, &g_cfg.vol_fx, &g_cfg.vol_mu };
    if (key == SDLK_LEFT && g_cd.sel > 0)  { g_cd.sel--; g_cd.press = 0.0f; }
    if (key == SDLK_RIGHT && g_cd.sel < 6) { g_cd.sel++; g_cd.press = 0.0f; }
    if (key == SDLK_ESCAPE) cd_close();
    if (key != SDLK_RETURN && key != SDLK_KP_ENTER) return;
    g_cd.press = 0.01f;
    switch (g_cd.sel) {
    case 0: cd_close(); break;
    case 2: if (g_cd.track > 1) g_cd.track--; break;
    case 3: if (g_cd.track < 7) g_cd.track++; break;
    case 4: *vol[g_cd.chan] -= 1024; if (*vol[g_cd.chan] < 0) *vol[g_cd.chan] = 0; break;
    case 5: *vol[g_cd.chan] += 1024; if (*vol[g_cd.chan] > 0x7fff) *vol[g_cd.chan] = 0x7fff; break;
    case 6: g_cd.chan = (g_cd.chan + 1) % 3; break;
    default: break;
    }
}

static void cd_draw(float dt)
{
    static const int button[7][3] = { { 59, 7, 11 }, { 83, 8, 12 }, { 113, 9, 13 }, { 137, 10, 14 },
                                      { 167, 57, 59 }, { 167, 58, 60 }, { 217, 61, 62 } };
    static const char *name[3] = { "CD", "FX", "MU" };
    static const int *const vol[3] = { &g_cfg.vol_cd, &g_cfg.vol_fx, &g_cfg.vol_mu };
    static const int bar_colour[4] = { 22, 214, 208, 208 };
    static Uint32 born;
    if (!born) born = SDL_GetTicks();
    /* the race's sound dies away (0xd11: 16384 a second of 0x6000) and the music stops */
    if (g_cd.fade > 0.0f) {
        g_cd.fade -= dt * (16384.0f / 24576.0f);
        if (g_cd.fade <= 0.0f) { g_cd.fade = 0.0f; snd_music("build/dec/MUSIC", 0); }
        volumes_apply(g_cd.fade);
    }
    g_oy = 0;
    champ_darken(0, 0, 320, FB_H / SY);
    champ_darken(0, 0, 320, FB_H / SY);
    LAY_MID();
    if (g_have_cdfront) pkc_draw(g_cdfront);
    char buf[8];
    int blink = ((SDL_GetTicks() - born) / 320) & 1;
    for (int i = 0; i < 7; i++) {
        if (i + 1 == g_cd.track && blink) continue;
        snprintf(buf, sizeof buf, "%d", i + 1);
        hud_text(0x40 + 13 * i, 0x4e, buf, 5, 9, 1);
    }
    if (g_cd.press != 0.0f) { g_cd.press += dt; if (g_cd.press > 0.5f) g_cd.press = 0.0f; }
    hud_sprite_draw(button[g_cd.sel][0], 0x61, button[g_cd.sel][g_cd.press != 0.0f ? 2 : 1]);
    if (g_cd.sel >= 4) {                                         /* 0x23dac */
        hud_text(0x40, 0x45, name[g_cd.chan], 5, 3, 1);
        for (int i = 0; i < *vol[g_cd.chan] / 1024; i++)
            for (int y = 0x45 * SY + g_oy; y <= 0x4a * SY + SY - 1 + g_oy; y++)
                for (int x = (0x55 + i * 3) * g_s; x < (0x55 + i * 3) * g_s + g_s; x++)
                    if (x < FB_W && y < FB_H) fb[y * FB_W + x] = g_pal[bar_colour[(i / 10) & 3]];
    }
}

/*
 * SCREAMER 2'S RACE SCREEN (0x45882, with 0x46b76 the instrument, 0x456fb the bar of who is
 * where, 0x333c1 a sprite in the middle).  The texts are GAME.TXT's, the font and sprites
 * GAMEL / GAMEH of GAME1; places are the game's, in 320 x 200.  What it shows is read from
 * the game's memory (s2_hud).  Not here yet: the bend arrows (0x454fc), the other modes'
 * tables, the replay's and the end's screens.
 */
static char g_s2_gt[64][40];        /* GAME.TXT's lines, the notes (*) left out: 0 1ST .. 3 4TH, 4 POSITION,
                                       5 CONGRATULATIONS !, 11 REPLAY, 12 RACE AGAIN, 13 EXIT, 20 THIS LAP ... */
#define g_s2_text (g_s2_gt + 20)    /* THIS LAP, TOTAL, BEST LAP, BEST TOTAL, BEST LAP!, WRONG WAY, POSITION, LAP */
static float g_s2_dial_speed, g_s2_dial_revs;
static float g_s2_end_t;            /* seconds since the player's car finished (0x133050) */
static int   g_s2_end_sel;          /* the end's choice, from the left (0x13306c) */
static int   s2_champ_kept(s2_result *r);
static int   g_s2_track;
static char  g_s2_team[4][16] = { "CONDOR", "ANGELS", "WASP", "ZEUS" };     /* (STRINGS.TXT's, 0x127edc) */
static int text_lines(const char *file, char out[][40], int max);

static void s2_hud_load(void)
{
    {
        static char str[260][40];
        const int n = text_lines("build/dec/LOCAL/STRINGS.TXT", str, 260);
        for (int i = 0; i + 3 < n; i++)
            if (!strcmp(str[i], "CONDOR")) { for (int k = 0; k < 4; k++) snprintf(g_s2_team[k], sizeof g_s2_team[k], "%s", str[i + k]); break; }
    }
    g_fnt_file = "build/dec/GAME1/GAMEL.FNT";
    g_spr_file = "build/dec/GAME1/GAMEL.SPR";
    hud_load();
    FILE *f = fopen("build/dec/LOCAL/GAME.TXT", "r");
    if (f) {
        char line[128];
        int at = 0;
        while (at < 64 && fgets(line, sizeof line, f)) {
            line[strcspn(line, "\r\n")] = 0;
            if (line[0] == '*') continue;
            snprintf(g_s2_gt[at++], sizeof g_s2_gt[0], "%s", line);
        }
        fclose(f);
    }
}

/*
 * A DARKENED BOX (0x51698(x, y, width, height); x and the width cut to fours): every pixel
 * goes through a table of 256 built from the palette when it is set (0x5105e) -- for each
 * colour the palette's nearest to 0.67 of it (the first of equals), and where that is the
 * colour itself, the nearest to 0.7 of that again.  (The routine builds a second table,
 * 0.6, for another use.)
 */
static void s2_dark_box(int x, int y, int w, int h)
{
    const uint8_t *dark = g_s2_dark[0];
    s2_dark_make();
    x &= ~3; w &= ~3;
    for (int yy = y * SY + g_oy; yy < (y + h) * SY + g_oy && yy < FB_H; yy++)
        for (int xx = x * g_s; xx < (x + w) * g_s && xx < FB_W; xx++) {
            const uint32_t c = fb[yy * FB_W + xx] | 0xFF000000u;
            static uint32_t seen[256]; static uint8_t is[256]; static int ok[256];
            const unsigned hsh = (c * 2654435761u) >> 24;
            int idx = -1;
            if (ok[hsh] && seen[hsh] == c) idx = is[hsh];
            else {
                long least = 1L << 40;
                for (int q = 0; q < 256; q++) {
                    const long dr = (long)((g_pal[q] >> 16) & 255) - (long)((c >> 16) & 255), dg = (long)((g_pal[q] >> 8) & 255) - (long)((c >> 8) & 255),
                               db = (long)(g_pal[q] & 255) - (long)(c & 255);
                    if (dr * dr + dg * dg + db * db < least) { least = dr * dr + dg * dg + db * db; idx = q; }
                }
                seen[hsh] = c; is[hsh] = (uint8_t)idx; ok[hsh] = 1;
            }
            fb[yy * FB_W + xx] = g_pal[dark[idx]];
        }
}

static int s2_text(int x, int y, const char *str, int font, int colour, int space)      /* 0x50f45 */
{
    g_hud_space = space;
    const int w = hud_text(x, y, str, font, colour, 1);
    g_hud_space = NO_SPACE;
    return w;
}
static int s2_text_wide(const char *str, int font, int space)                           /* 0x51020 */
{
    g_hud_space = space;
    const int w = hud_text(0, 0, str, font, 0, 0);
    g_hud_space = NO_SPACE;
    return w;
}
static void s2_text_centred(int y, const char *str, int font, int colour, int space)   /* 0x3327c */
{
    s2_text((320 - s2_text_wide(str, font, space)) >> 1, y, str, font, colour, space);
}

static void s2_sprite_centred(int y, int n)              /* 0x333c1 */
{
    if (n < 0 || n >= g_nspr) return;
    hud_sprite_draw((320 - (g_spr[n].w / g_s + g_spr[n].ox / g_s)) / 2 - g_spr[n].ox / g_s / 2, y, n);
}

/* RACE AGAIN: this part once more, with its own two arguments (the track's directory and
   number) */
static void s2_race_again(void)
{
    char dir[64], num[16];
    snprintf(dir, sizeof dir, "build/dec/MAP%d", g_s2_track);
    snprintf(num, sizeof num, "%d", g_s2_track);
    os_go(3, dir, num);
}

/*
 * THE DASHBOARD of the second view from the car, view 6 (0x46b76 -> 0x46589(speed, revs,
 * gear)): the picture COCK<n + 1>_L.PKC over the scene, then from the dashboard's record of
 * CRUSC.DTR (22 numbers, places in the 640-wide picture's units, halved here):
 *   +0x04 x, y, length, angle at nought, angle at the most, the most   the speed's needle
 *   +0x1c x, y, length, angle at nought, angle at the most             the revs' needle
 *   +0x30 five (x, y)                                                  the gear's lamp
 * A needle stands at  nought - value / most * (nought - most)  radians (under 0.001: 0),
 * counted anticlockwise from the right; its tip is length / 2 away; three lines of colour
 * 12 to the tip, from the centre and from one pixel to either side of it.  The lamp is
 * sprite 0x46 + 5 * dashboard + gear at that gear's place.  The first number is not used
 * by this routine.
 */
static int16_t g_s2_cock[FB_MAXW * FB_MAXH];
static int g_s2_cock_n = -1, g_s2_cock_ok;

static void s2_dash_needle(const int32_t *r, float part)
{
    float a0, a1;
    memcpy(&a0, r + 3, 4); memcpy(&a1, r + 4, 4);
    float a = a0 - part * (a0 - a1);
    if (fabsf(a) < 0.001f) a = 0.0f;
    if (s2_screen_hi()) {                      /* (STARTH: the table's numbers whole, in the screen's own pixels) */
        const int cx = r[0], cy = r[1];
        const int tx = (int)(cosf(a) * (float)r[2] / 1.0f + (float)cx), ty = (int)((float)cy - sinf(a) * (float)r[2] / 1.0f);
        const float b = a + 1.5707964f, c = b - 3.1415927f;
        const uint32_t col = g_pal[12];
        dash_line(cx, cy, tx, ty, col);
        dash_line((int)(cosf(b) + (float)cx), (int)((float)cy - sinf(b)), tx, ty, col);
        dash_line((int)(cosf(c) + (float)cx), (int)((float)cy - sinf(c)), tx, ty, col);
        return;
    }
    const int cx = r[0] / 2, cy = r[1] / 2;
    const int tx = (int)(cosf(a) * (float)r[2] * 0.5f + (float)cx), ty = (int)((float)cy - sinf(a) * (float)r[2] * 0.5f);
    const float b = a + 1.5707964f, c = b - 3.1415927f;
    const uint32_t col = g_pal[12];
    dash_line(cx * g_s, cy * g_s, tx * g_s, ty * g_s, col);
    dash_line((int)(cosf(b) + (float)cx) * g_s, (int)((float)cy - sinf(b)) * g_s, tx * g_s, ty * g_s, col);
    dash_line((int)(cosf(c) + (float)cx) * g_s, (int)((float)cy - sinf(c)) * g_s, tx * g_s, ty * g_s, col);
}

static void s2_dash_draw(const s2_hud_state *h)
{
    const int32_t *r = (const int32_t *)h->dash_rec;
    if (g_s2_cock_n != h->dash) {
        char file[64];
        snprintf(file, sizeof file, "build/dec/GAME1/COCK%d_%c.PKC", h->dash + 1, s2_screen_hi() ? 'H' : 'L');
        g_s2_cock_ok = pkc_load(file, g_s2_cock);
        g_s2_cock_n = h->dash;
        g_pkc_loads++;
    }
    /* (the scene stops at the view's rows; where the picture is open below them -- a few
       pixels at its upper edge -- the port leaves black: its own) */
    for (int y = g_vh; y < FB_H; y++) for (int x = 0; x < FB_W; x++) fb[y * FB_W + x] = 0xff000000u;
    if (g_s2_cock_ok) pkc_draw(g_s2_cock);
    const int gear = h->gear < 0 ? 0 : h->gear > 4 ? 4 : h->gear;
    if (s2_screen_hi()) hud_sprite_px(r[12 + gear * 2], r[13 + gear * 2], h->dash * 5 + 0x46 + gear);
    else hud_sprite_draw(r[12 + gear * 2] / 2, r[13 + gear * 2] / 2, h->dash * 5 + 0x46 + gear);
    s2_dash_needle(r + 1, r[6] ? g_s2_dial_speed / (float)r[6] : 0.0f);
    s2_dash_needle(r + 7, g_s2_dial_revs);
}

/*
 * THE ATTRACT RACE'S SCREEN (0x37ea9, called by the race screen 0x45882 in its place when the
 * mode has 0x80).  ATTRACT.TXT's lines: 0 and 1 under the two logos, then groups ended by '#'
 * (the empty lines in them are places), an empty group the end.  By stages (0x127ecc):
 *   0  the screen black; the next 1, the first group 2, the logo's place x 1000
 *   1, 2  the screen black, sprite 0x6d + stage at (x, 10), x eased towards the middle by row
 *         0x55 (0.925 to the power 50 x the step); once within 5 of it the line 0 / 1 under it
 *         (y 0xa5, font 0 colour 1, spaced 2); three seconds after, the next, x 640
 *   3  the race, out of white (the palette from white over 1 / 1.2 s), SCREAMER 2 (sprite 0x71
 *      in the middle at y 4); the CD's song from here (0x4d758)
 *   4  the logo, and the group (y 100 + 16 a line) for four seconds, then a second of nothing,
 *      then the next group
 * An empty group, or ENTER / ESC (0x131f20 0x10, 0x11, 0x20), ends it: before stage 3 at once,
 * else after half a second's fade to black (0x13249c += 2 x step); then the race is over
 * (0x11f0a8 |= 3).
 */
static struct { int stage, group, end, over; float x, shown, pause, black; } g_s2_att;
static char g_s2_att_txt[160][40];
static int  g_s2_att_n;

static void s2_attract_start(void)
{
    memset(&g_s2_att, 0, sizeof g_s2_att);
    g_s2_att_n = text_lines("build/dec/LOCAL/ATTRACT.TXT", g_s2_att_txt, 160);
}

static void s2_attract_fill(uint32_t to, float k)      /* every pixel k of the way to the colour */
{
    const float r = (float)(to >> 16 & 255), g = (float)(to >> 8 & 255), b = (float)(to & 255);
    for (int i = 0; i < FB_W * FB_H; i++) {
        const uint32_t c = fb[i];
        fb[i] = 0xff000000u | (uint32_t)((float)(c >> 16 & 255) + (r - (float)(c >> 16 & 255)) * k) << 16
              | (uint32_t)((float)(c >> 8 & 255) + (g - (float)(c >> 8 & 255)) * k) << 8 | (uint32_t)((float)(c & 255) + (b - (float)(c & 255)) * k);
    }
}

static void s2_attract_draw(float dt)
{
    LAY_TOP();
    if (g_s2_att.stage == 0) {
        for (int i = 0; i < FB_W * FB_H; i++) fb[i] = g_pal[0];
        g_s2_att.group = 2; g_s2_att.stage = 1; g_s2_att.x = s2_screen_hi() ? 2000.0f : 1000.0f; g_s2_att.pause = 0.0f;
    } else if (g_s2_att.stage < 3) {
        for (int i = 0; i < FB_W * FB_H; i++) fb[i] = g_pal[0];
        const int n = g_s2_att.stage + 0x6d;
        if (n < g_nspr) {
            /* (STARTH: the logo's place in the screen's own pixels -- from 2000, aimed at twice the
               low set's middle worked out from the H sprite halved, again from 1280; its height
               10 not doubled) */
            const int ox = g_spr[n].ox, w = g_spr[n].w;
            const float to = s2_screen_hi() ? (float)((((320 - (w + ox) / 2) >> 1) - ((ox - (ox >> 31)) >> 1) / 2) * 2)
                                            : (float)((320 - (w / g_s + ox / g_s)) / 2 - ox / g_s / 2);
            g_s2_att.x = (g_s2_att.x - to) * powf(0.925f, 50.0f * dt) + to;
            if (s2_screen_hi()) hud_sprite_px((int)g_s2_att.x, 10, n);
            else hud_sprite_draw((int)g_s2_att.x, 10, n);
            if (fabsf(g_s2_att.x - to) < 5.0f) {
                if (g_s2_att.stage - 1 < g_s2_att_n) s2_text_centred(0xa5, g_s2_att_txt[g_s2_att.stage - 1], 0, 1, 2);
                g_s2_att.shown += dt;
                if (g_s2_att.shown > 3.0f) {
                    g_s2_att.shown = 0.0f; g_s2_att.x = s2_screen_hi() ? 1280.0f : 640.0f;
                    if (++g_s2_att.stage > 2) g_s2_att.shown = 1.0f;
                }
            }
        } else g_s2_att.stage = 3, g_s2_att.shown = 1.0f;
    } else if (g_s2_att.stage == 3) {
        s2_sprite_centred(4, 0x71);
        s2_attract_fill(0xffffffffu, g_s2_att.shown > 0.0f ? g_s2_att.shown : 0.0f);
        if (!g_s2_att.over) { g_s2_att.over = 1; s2_race_music(); }
        g_s2_att.shown -= dt * 1.2f;
        if (g_s2_att.shown < 0.0f) { g_s2_att.shown = 1.0f; g_s2_att.pause = 0.0f; g_s2_att.stage = 4; }
    } else if (g_s2_att.stage == 4) {
        s2_sprite_centred(4, 0x71);
        if (g_s2_att.pause == 0.0f) {
            int k = 0;
            while (g_s2_att.group + k < g_s2_att_n && g_s2_att_txt[g_s2_att.group + k][0] != '#') {
                s2_text_centred(k * 0x10 + 100, g_s2_att_txt[g_s2_att.group + k], 0, 1, 2);
                k++;
            }
            if (k == 0) { g_s2_att.stage = 5; g_s2_att.end = 1; }
            else {
                g_s2_att.shown -= dt * 0.25f;
                if (g_s2_att.shown <= 0.0f) { g_s2_att.group += k + 1; g_s2_att.shown = 1.0f; g_s2_att.pause = 1.0f; }
            }
        } else {
            g_s2_att.pause -= dt;
            if (g_s2_att.pause <= 0.0f) g_s2_att.pause = 0.0f;
        }
    }
    if (g_s2_att.black > 0.0f) s2_attract_fill(0xff000000u, g_s2_att.black > 1.0f ? 1.0f : g_s2_att.black);
    if (g_s2_att.end) {
        if (g_s2_att.stage < 3) g_s2_att.end = 2;                  /* (the race over now) */
        else if (g_s2_att.stage > 3) {
            g_s2_att.black += dt * 2.0f;
            if (g_s2_att.black >= 1.0f) g_s2_att.end = 2;
        }
    }
}

/*
 * THE RACE'S PAUSE SCREEN (0x49f8e; ESC in a race while the player's car has not finished --
 * not in the attract race, nor in a replay: 0x2d7c0).  The items are GAME.TXT's RESUME,
 * RESTART (not in the championship), CD PLAYER (when there is a drive: here, when there is a
 * song file) and EXIT; n of them centred at y 0x66 - 11 n + 22 i (font 0 colour 1, spaced 2)
 * over the race's picture, standing, darkened (0x51698), and a box of it darkened again over
 * them (0x50, 0x5e - 11 n, 0xa0, 22 n + 12); the chosen one is drawn over that box, or under it
 * while bit 0x20 of the hundredths' clock is set.  UP / DOWN round, ENTER, ESC = RESUME.  The
 * effects die away at 32767 a second while it is up.  RESTART: the race again (0x11f0a8 |= 1);
 * EXIT: the race over (|= 3); CD PLAYER (0x49aaf), then the race goes on.
 *
 * THE CD PLAYER (0x49aaf): the picture darkened twice more, sprite 10 at (0x1c, 0x28); the
 * disc's tracks "\3%d" 13 apart from (0x40, 0x4e), the tenth and on 7 lower from the start
 * again (font 5 colour 4, spaced 2), the one chosen blinking; seven buttons, the one under the
 * cursor drawn at (x, 0x61) -- for 0.2 s after ENTER its pressed sprite (0x113480: x, pressed,
 * plain); LEFT / RIGHT; ENTER: 0 play / pause, 1 eject (nothing here), 2 / 3 the track before /
 * after (playing on if it was playing, else only chosen), 4 / 5 the chosen volume down / up by
 * 480 a hundredth while held, 6 which volume, CD (0x130d5c) or FX (0x130d58); for 4..6 its
 * name at (0x40, 0x45) (font 5 colour 2) and a stroke of colour 66 / 33 / 12 every 1024, 3
 * apart from x 0x55, y 0x45..0x4a.  ESC leaves it.
 */
static struct { int on, sel, n, item[4], cd, cd_sel, chan, first, last, track; float clock, pressed; } g_s2_pz;

static void s2_pause_open(void)
{
    char f[600];
    memset(&g_s2_pz, 0, sizeof g_s2_pz);
    g_s2_pz.first = 2; g_s2_pz.last = 0;
    for (int t = 2; t < 100 && cd_audio_find(t, f, sizeof f); t++) g_s2_pz.last = t;      /* (the disc's audio tracks: the files there are) */
    const int has[4] = { 1, !(s2_mode & 2), g_s2_pz.last >= 2, 1 };
    for (int i = 0; i < 4; i++) if (has[i]) g_s2_pz.item[g_s2_pz.n++] = i;
    g_s2_pz.on = 1;
}

static void s2_pause_line(int x0, int y0, int y1, int colour)
{
    for (int y = y0 * SY + g_oy; y < (y1 + 1) * SY + g_oy && y < FB_H; y++)
        for (int x = x0 * g_s; x < x0 * g_s + g_s && x < FB_W; x++) fb[y * FB_W + x] = g_pal[colour & 255];
}

static void s2_pause_draw(float dt)
{
    static const int button[7][3] = { { 59, 15, 11 }, { 83, 16, 12 }, { 113, 17, 13 }, { 137, 18, 14 }, { 167, 22, 20 }, { 167, 23, 21 }, { 217, 25, 24 } };
    static const char *name[2] = { "CD", "FX" };
    static const int bar[4] = { 66, 33, 12, 12 };
    char buf[16];
    g_s2_pz.clock += dt * 100.0f;
    if (!g_s2_pz.cd) s2_sound_pause(dt, 1);
    const int blink = ((int)g_s2_pz.clock & 0x20) != 0, n = g_s2_pz.n;
    g_oy = 0;
    s2_dark_box(0, 0, 320, FB_H / SY);
    for (int i = 0; i < n; i++)
        if (i != g_s2_pz.sel || blink) s2_text_centred(i * 0x16 - n * 0xb + 0x66, g_s2_gt[34 + g_s2_pz.item[i]], 0, 1, 2);
    s2_dark_box(0x50, 0x5e - n * 0xb, 0xa0, n * 0x16 + 0xc);
    if (!blink) s2_text_centred(g_s2_pz.sel * 0x16 - n * 0xb + 0x66, g_s2_gt[34 + g_s2_pz.item[g_s2_pz.sel]], 0, 1, 2);
    if (!g_s2_pz.cd) return;
    /* the CD player */
    s2_dark_box(0, 0, 320, FB_H / SY);
    s2_dark_box(0, 0, 320, FB_H / SY);
    hud_sprite_draw(0x1c, 0x28, 10);
    const int now = snd_cd_track() ? snd_cd_track() : g_s2_pz.track;
    if (now) g_s2_pz.track = now;
    for (int k = 0, x = 0, y = 0; k <= g_s2_pz.last - g_s2_pz.first; k++, x += 0xd) {
        if (k == 10 && y == 0) { y = 7; x = 0; }
        if (k + g_s2_pz.first == g_s2_pz.track && !blink) continue;
        snprintf(buf, sizeof buf, "\3%d", k + g_s2_pz.first);
        s2_text(x + 0x40, y + 0x4e, buf, 5, 4, 2);
    }
    const int sel = g_s2_pz.cd_sel;
    hud_sprite_draw(button[sel][0], 0x61, g_s2_pz.pressed > g_s2_pz.clock ? button[sel][1] : button[sel][2]);
    if (sel == 4 || sel == 5) {                        /* (ENTER held: 480 a hundredth) */
        const Uint8 *ks = SDL_GetKeyboardState(NULL);
        if (ks[SDL_SCANCODE_RETURN] || ks[SDL_SCANCODE_KP_ENTER]) {
            s2_volume_set(g_s2_pz.chan, s2_volume(g_s2_pz.chan) + (sel == 4 ? -1 : 1) * (int)(dt * 100.0f * 480.0f));
            if (g_s2_pz.chan == 0) snd_cd_volume((float)(s2_volume(0) >> 7) / 255.0f);
        }
    }
    if (sel > 3) {
        s2_text(0x40, 0x45, name[g_s2_pz.chan], 5, 2, 2);
        for (int k = 0; k < s2_volume(g_s2_pz.chan) / 1024; k++) s2_pause_line(k * 3 + 0x55, 0x45, 0x4a, bar[(k / 10) & 3]);
    }
}

static void s2_pause_close(int pick)
{
    g_s2_pz.on = 0;
    s2_sound_pause(0.0f, 0);
    if (pick == 1) s2_race_again();                    /* RESTART */
}

/* a key on the pause screen: 1 = the race is to end (EXIT) */
static int s2_pause_key(SDL_Keycode k)
{
    if (g_s2_pz.cd) {
        if (k == SDLK_ESCAPE) { g_s2_pz.cd = 0; s2_pause_close(0); return 0; }
        if (k == SDLK_LEFT && g_s2_pz.cd_sel > 0) g_s2_pz.cd_sel--;
        if (k == SDLK_RIGHT && g_s2_pz.cd_sel < 6) g_s2_pz.cd_sel++;
        if (k != SDLK_RETURN && k != SDLK_KP_ENTER) return 0;
        g_s2_pz.pressed = g_s2_pz.clock + 0x14;
        const int playing = snd_cd_track() && !snd_cd_paused();
        switch (g_s2_pz.cd_sel) {
        case 0:
            if (playing) snd_cd_pause(1);
            else if (snd_cd_track() == g_s2_pz.track && snd_cd_paused()) snd_cd_pause(0);
            else if (g_s2_pz.track) snd_cd(g_s2_pz.track, 1);
            break;
        case 2: case 3: {
            const int t = g_s2_pz.track + (g_s2_pz.cd_sel == 2 ? -1 : 1);
            if (!g_s2_pz.track || t < g_s2_pz.first || t > g_s2_pz.last) break;
            g_s2_pz.track = t;
            if (playing) snd_cd(t, 1); else snd_cd(0, 0);
            break;
        }
        case 6: g_s2_pz.chan ^= 1; break;
        }
        return 0;
    }
    const int n = g_s2_pz.n;
    if (k == SDLK_ESCAPE) { s2_pause_close(0); return 0; }
    if (k == SDLK_UP) g_s2_pz.sel = (g_s2_pz.sel + n - 1) % n;
    if (k == SDLK_DOWN) g_s2_pz.sel = (g_s2_pz.sel + 1) % n;
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
        const int pick = g_s2_pz.item[g_s2_pz.sel];
        if (pick == 2) { g_s2_pz.cd = 1; g_s2_pz.cd_sel = 0; g_s2_pz.clock = 0.0f; return 0; }
        s2_pause_close(pick);
        return pick == 3 || pick == 1;
    }
    return 0;
}

static void s2_hud_draw(float dt)
{
    s2_hud_state h;
    char buf[64];
    if (!g_nfont || !g_nspr) return;
    s2_hud(&h);
    if (getenv("OS_S2ENDTEST")) { h.finished = 1; g_s2_end_t = (float)atof(getenv("OS_S2ENDTEST")); }   /* a test: the end's screen, so many seconds in */
    /*
     * "1ST" OVER THE LEADING CAR (0x4962c, called after the scene is drawn and before the
     * race screen, in every view): for the car that is first, when it is not the player's
     * (mode bit 1 set, bits 0x30 clear), the point 500 above its place is put through the
     * camera (0x492d1) and sprite 0x26 drawn 10 to the left of it and 12 above -- no further
     * than 20000 away, and within the scene's rows.  (The game's own sum divides by the depth
     * + 128; the port uses its own lens, so that the mark sits over the car as drawn.)
     */
    if (h.leader >= 0 && h.leader < S2_CARS && g_s2_obj[h.leader]) {
        const scene_object *o = g_s2_obj[h.leader];
        const float w[3] = { o->x, o->y, o->z + 500.0f };
        vtx v;
        to_view(w, &v);
        if (v.z > 1.0f && v.z <= 20000.0f) {
            const float f = g_focal_x / (float)g_s;
            const int lx = (int)(160.0f + v.x * f / v.z) - 10, ly = (int)(g_cy / (float)SY - v.y * f / v.z) - 12;
            if (lx > -20 && lx < 320 && ly > -16 && ly < g_vh / SY) { LAY_TOP(); hud_sprite_draw(lx, ly, 0x26); }
        }
    }
    LAY_TOP();
    /* the instrument (0x46b76): the dial, the speed, the gear, the rev needle -- both eased
       by table entry 0x32, 0.75 to the power of fiftieths of a second */
    {
        const float k = powf(0.75f, 50.0f * dt);
        const float ws = h.speed * 1.513f, wr = h.revs;
        g_s2_dial_speed = ws + (g_s2_dial_speed - ws) * k;
        g_s2_dial_revs = wr + (g_s2_dial_revs - wr) * k;
    }
    /* (0x45882: the instrument every frame but the attract race's, the race over too; the
       texts below only while the player's car has not finished, car +0x4c bit 0) */
    {
        LAY_BOT();
        if (h.view == 6) s2_dash_draw(&h);                /* (0x46b76: view 6 the dashboard, view 1 nothing, else the dial) */
        else if (h.view != 1) {
        hud_sprite_draw(0xee, 0x84, 0x1a);
        /* in miles (/ 1.6) when GAME.TXT's word for the unit begins with an M (0x2ef4e sets
           0x124654 so; the English file's is MPH) */
        snprintf(buf, sizeof buf, "%03d", (int)((g_s2_gt[28][0] | 0x20) == 'm' ? g_s2_dial_speed / 1.6f : g_s2_dial_speed));
        s2_text(0x10b, 0xb1, buf, 2, 5, 1);
        snprintf(buf, sizeof buf, "%d", h.gear);
        s2_text(299, 0xb1, buf, 2, 4, 2);
        dash_needle(0x10f, 0xa8, 0x1e, g_s2_dial_revs * 3.1415927f, 0x5b);
        }
    }
    if (!h.finished && h.view != 1) {                    /* (0x45882: not over the trackside cameras) */
        LAY_TOP();
        if (h.ncars > 1 && (!(h.mode & 4) || h.ghost)) {
            if (h.position == 1) hud_sprite_draw(0x10e, 4, 0x72);
            s2_text(0x138 - s2_text_wide(g_s2_text[6], 3, -1), 2, g_s2_text[6], 3, 3, -1);
            snprintf(buf, sizeof buf, "%d", h.position);
            s2_text(0x11e, 0xb, buf, 1, 1, 2);
        }
        if (*h.message_left > 0.0f) {
            if (h.message[1] == '+') s2_text_centred(0x23, h.message, 8, 3, -1);
            else s2_text_centred(0x23, h.message, 7, 1, -1);
        }
        if (!(h.mode & 8)) {
        s2_text(8, 2, g_s2_text[7], 3, 3, -1);
        snprintf(buf, sizeof buf, "%d:", h.lap);         /* ("%d%c": the figures' font has ':' after '9' and nothing else) */
        int w = s2_text_wide(buf, 1, 2);
        s2_text(8, 0xb, buf, 1, 1, 2);
        snprintf(buf, sizeof buf, "%d", h.laps);
        s2_text(w + 5, 0x14, buf, 4, 1, 2);
        buf[0] = 1; s2_clock(buf + 1, sizeof buf - 1, h.this_lap);
        s2_text(8, 0x38, g_s2_text[0], 3, 3, -1);
        s2_text(8, 0x40, buf, 3, 1, -1);
        buf[0] = 1; s2_clock(buf + 1, sizeof buf - 1, h.total);
        s2_text(8, 0x48, g_s2_text[1], 3, 3, -1);
        s2_text(8, 0x50, buf, 3, 1, -1);
        s2_text(0x10e, 0x38, g_s2_text[2], 3, 3, -1);
        s2_text(0x10e, 0x40, h.best_lap, 3, 1, -1);
        s2_text(0x10e, 0x48, g_s2_text[3], 3, 3, -1);
        s2_text(0x10e, 0x50, h.best_total, 3, 1, -1);
        }
        if (*h.best_lap_left > 0.0f && ((int)(*h.best_lap_left * 2.0f) & 1)) s2_text_centred(0xf, g_s2_text[4], 0, 1, 2);
        if (h.mode & 8) {
            /* a free run: the lap's five times, A to E, and against the best lap's; this
               lap's clock and the best lap above them */
            for (int k = 0; k < 5; k++) {
                const char letter[2] = { (char)('A' + k), 0 };
                s2_text(8, k * 8 + 0x38, letter, 3, 3, -1);
                s2_text(0x105, k * 8 + 0x38, letter, 3, 3, -1);
                s2_text(0x12, k * 8 + 0x38, h.split_now + k * 0xb, 3, 1, -1);
                s2_text(0x10a, k * 8 + 0x38, h.split_best + k * 0xb, 3, 1, -1);
            }
            buf[0] = 1; s2_clock(buf + 1, sizeof buf - 1, h.this_lap);
            s2_text(8, 0x10, g_s2_text[0], 3, 3, -1);
            s2_text(8, 0x18, buf, 3, 1, -1);
            s2_text(0x10e, 0x10, g_s2_text[2], 3, 3, -1);
            s2_text(0x10e, 0x18, h.best_lap, 3, 1, -1);
        }
        /* who is where (0x456fb): the bar, every other car's mark 50 pixels a 200000 of the
           track ahead of or behind the middle, the player's in the middle */
        if (h.ncars > 1 && (!(h.mode & 4) || h.ghost)) {
            s2_dark_box(0x6c, 4, 0x68, 7);
            hud_sprite_draw(0x6c, 2, 0x25);
            for (int i = 0; i < h.ncars && i < 4; i++)
                if (i != s2_player()) hud_sprite_draw((int)(160.0f - h.ahead[i] * 50.0f - 4.0f), 3, h.team[i] + 4);
            hud_sprite_draw(0x9c, 3, 9);
        }
    }
    /*
     * A REPLAY'S BAND (0x463c0, while the player's car runs): a darkened band along the
     * bottom and on it, in the big letters, the team of the car the cameras follow and the
     * view -- "CONDOR - EXTERNAL" (the team's name from STRINGS.TXT, the view's from GAME.TXT:
     * 1 EXTERNAL, 2 INCAR, 3 BACK, 4 NEAR, 6 INSIDE; no text for another view).
     */
    {
        int team, view;
        if (s2_replay_band(&team, &view)) {
            static const int word[7] = { -1, 29, 30, 31, 32, -1, 33 };
            LAY_TOP();
            s2_dark_box(0, 0xb4, 0x140, 0x14);
            if (view >= 0 && view < 7 && word[view] >= 0 && team >= 0 && team < 4) {
                snprintf(buf, sizeof buf, "%s - %s", g_s2_team[team], g_s2_gt[word[view]]);
                s2_text(10, 0xb4, buf, 0, 1, 2);
            }
        }
    }
    /*
     * THE END (0x4746a, a race's; the car is the trackside cameras' now, 0x201ee).  Four
     * seconds of where the player came and the time, then the track's name, and the choices
     * on a darkened band -- left and right choose, ENTER takes -- and the track's best times.
     */
    if (h.finished) {
        if (g_s2_end_t < 4.0f && (h.mode & 4)) {
            /* time attack's (0x48980): the total and the best lap */
            s2_text_centred(0x3c, g_s2_text[1], 0, 1, 2);
            s2_clock(buf, sizeof buf, h.total);
            s2_text_centred(0x50, buf, 0, 1, 2);
            s2_text_centred(100, g_s2_text[18], 0, 1, 2);
            s2_clock(buf, sizeof buf, h.lap_best);
            s2_text_centred(0x78, buf, 0, 1, 2);
        } else if (g_s2_end_t < 4.0f) {
            if (h.position == 1) s2_text_centred(0x46, g_s2_gt[5], 0, 1, 2);
            snprintf(buf, sizeof buf, "%s %s", g_s2_gt[h.position >= 1 && h.position <= 4 ? h.position - 1 : 3], g_s2_gt[4]);
            s2_text_centred(100, buf, 0, 1, 2);
            s2_clock(buf, sizeof buf, h.total);
            s2_text_centred(0x82, buf, 0, 1, 2);
        } else if (h.mode & 2) {
            /* the championship's (0x47e75): CONTINUE -- REPLAY and CONTINUE in halves when there
               is a replay -- and the cars that have done their laps by their places: name, car,
               time and the points for the place (in a replay, the race's own: 0x132384) */
            static const int points_of[4] = { 10, 6, 3, 0 };
            s2_result r[4];
            const int lit = (int)(g_s2_end_t * 3.0f) & 1, nr = s2_replaying() ? s2_champ_kept(r) : s2_results(r);
            int done = 0;
            for (int k = 0; k < nr; k++) done += r[k].done != 0;
            s2_records_enter();
            s2_sprite_centred(4, 0x27 + g_s2_track - 1);
            s2_dark_box(0, 0xb4, 0x140, 0x14);
            if (!s2_replay_offered())
                s2_text_centred(0xb8, g_s2_gt[15], lit ? 8 : 7, lit ? 3 : 1, 2);
            else
                for (int k = 0; k < 2; k++) {
                    const int on = lit && k == g_s2_end_sel;
                    const char *t = g_s2_gt[k ? 15 : 11];
                    s2_text(k * 0xa0 + ((0xa0 - s2_text_wide(t, on ? 8 : 7, 2)) >> 1), 0xb8, t, on ? 8 : 7, on ? 3 : 1, 2);
                }
            s2_dark_box(6, (done - 1) * -5 + 0x4f, 0x138, done * 0xd + 0xe);
            s2_text(0x26, (done - 1) * -5 + 0x51, g_s2_gt[16], 7, 1, 2);
            for (int k = 0; k < nr; k++) {
                char clock[24];
                const int y = (done - 1) * -5 + 0x5f + k * 0xc;
                if (!r[k].done) continue;
                s2_clock(clock, sizeof clock, r[k].time);
                snprintf(buf, sizeof buf, "\001%c. %s        %s %s%d", '1' + k, r[k].car == s2_player() ? s2_name : "CPU", clock, points_of[k] < 10 ? " " : "", points_of[k]);
                s2_text(0x26, y, buf, 7, 1, 2);
                hud_sprite_draw(0x71, y - 1, r[k].team * 5 + 0x2e + r[k].model);
            }
        } else {
            /* REPLAY / RACE AGAIN / EXIT in thirds, or RACE AGAIN / EXIT in halves when there
               is no replay (0x4746a) */
            const int three = s2_replay_offered(), wide = three ? 0x6a : 0xa0;
            s2_sprite_centred(4, 0x27 + g_s2_track - 1);
            s2_dark_box(0, 0xb4, 0x140, 0x14);
            for (int k = 0; k < 2 + three; k++) {
                const int lit = (k == g_s2_end_sel) && ((int)(g_s2_end_t * 3.0f) & 1);
                const char *t = g_s2_gt[11 + !three + k];
                s2_text(k * wide + ((wide - s2_text_wide(t, lit ? 8 : 7, 2)) >> 1), 0xb8, t, lit ? 8 : 7, lit ? 3 : 1, 2);
            }
            /*
             * The track's fifteen best times of this kind of race (0x4746a, 0x48980): the
             * race is put into them as this screen comes up (0x353b3) and its row is lit.
             * Six rows 12 apart from row 80 on a darkened band, "n. NAME      time" and the
             * car's picture (sprite 0x2e + 5 * team + model); after a second the list moves
             * up a row at a time, four rows a second, until the player's is the third.
             * (The game cuts the rows off at the band's edges as they move; here a row
             * is shown whole or not at all.)
             */
            {
                static int first; static float part;
                const int mine = s2_records_enter();
                const uint8_t *list = s2_records_list();
                int want = mine < 0 ? 0 : mine - 2;
                if (want < 0) want = 0; else if (want > 14) want = 10;
                if (g_s2_end_t - dt < 4.0f) { first = 0; part = 0.0f; }
                s2_dark_box(0x1e, 0x4e, 0x104, 0x40);
                for (int k = first, y = (int)(80.0f - part * 12.0f); list && k < first + 6 && k < 15; k++, y += 12) {
                    float time; int team, model;
                    char name[5] = { 0 }, clock[24];
                    memcpy(&time, list + k * 4, 4); memcpy(name, list + 0x3c + k * 4, 4);
                    memcpy(&team, list + 0xb4 + k * 4, 4); memcpy(&model, list + 0xf0 + k * 4, 4);
                    if (y < 0x4f || y > 0x84) continue;
                    s2_clock(clock, sizeof clock, time);
                    snprintf(buf, sizeof buf, "\001%s%d. %s      %s", k < 9 ? " " : "", k + 1, name, clock);
                    s2_text(0x32, y, buf, k == mine ? 8 : 7, k == mine ? 3 : 1, 2);
                    if (team >= 0 && team < 4 && model >= 0 && model < 5) hud_sprite_draw(0x86, y - 1, model + team * 5 + 0x2e);   /* (every row: an untouched one is "MIL", team row & 3, model 0) */
                }
                if (first < want && g_s2_end_t >= 4.5f && (part += dt * 4.0f) > 1.0f) { part = 0.0f; first++; }
            }
        }
        g_s2_end_t += dt;
    }
    /* the timers the screen itself runs down (0x45882) */
    *h.message_left -= dt; if (*h.message_left < 0.0f) *h.message_left = 0.0f;
    *h.best_lap_left -= dt; if (*h.best_lap_left < 0.0f) *h.best_lap_left = 0.0f;
    if (h.wrong_way) {
        static float on;
        on += dt;
        if ((int)on & 1) s2_text_centred(0xf, g_s2_text[5], 0, 1, 2);
    }
    /* the start (0x45882's end): a sprite for each of the first four seconds */
    if (h.bend >= 0) s2_sprite_centred(0x32, h.bend);          /* the bend's arrow (0x454fc) */
    if (h.clock < 4.0f && !h.finished) s2_sprite_centred(0x3c, 0x42 + (int)h.clock);
}

static void hud_draw(int instruments)
{
    char buf[64];
    if (g_replay) instruments = 0;
    /* (a taller picture: the labels and what goes with them at the top, the
       instruments and the radar at the bottom, the end's words in the middle) */
    LAY_TOP();
    if (g_have_implet) pkc_draw(g_implet);
    LAY_BOT();
    if (instruments && g_views[g_view].cockpit) {
        cockpit_draw();
        if (g_have_dash) {
            dash_needle(g_dash[2], g_dash[3], g_dash[0], g_needle_speed, 255);
            dash_needle(g_dash[5], g_dash[6], g_dash[0], g_needle_rpm, 255);
            int gear = g_pc.gear;
            if (gear >= 0 && gear <= 6)
                /* the lamps' places ARE in the picture's own pixels (the H section's are doubled) */
                hud_sprite_px(g_dash[8 + gear * 3], g_dash[9 + gear * 3], g_dash[1] + gear);
        }
    } else if (instruments) {
        if (g_have_impres) pkc_draw(g_impres);
        snprintf(buf, sizeof buf, "\001%03d", (int)(g_pc.speed * 0.017f));
        hud_text(0x10f, 0xbb, buf, 2, 7, 1);
        snprintf(buf, sizeof buf, "\001%d", g_pc.gear);
        hud_text(0x130, 0xbb, buf, 2, 1, 1);
        dash_needle(0x10f, 0xa8, 0x1e, g_needle_rpm, 0x75);
    }

    LAY_TOP();
    snprintf(buf, sizeof buf, "%d", (int)(g_timer));
    hud_text_centred(8, buf, 6, 2);
    int lap = g_laps < g_total_laps ? g_laps + 1 : g_laps;
    snprintf(buf, sizeof buf, "%d:", lap);
    int w = hud_text(8, 0xb, buf, 1, 5, 1);
    snprintf(buf, sizeof buf, "%d", g_total_laps);
    hud_text(w + 5, 0x14, buf, 4, 5, 1);
    snprintf(buf, sizeof buf, "%d", g_player_rank);
    hud_text(0x114, 0xb, buf, 1, 5, 1);
    if (g_replay) {
        if (((int)(g_race_clock * 30.0f)) & 0x80 || ((int)(g_race_clock * 2.0f) & 1))
            { LAY_BOT(); hud_text(8, 0xaa, "R", 0, 6, 1); LAY_TOP(); }
    } else {
        hud_clock(buf, sizeof buf, g_total_time);
        hud_text(8, 0x38, buf, 3, 2, 1);
        hud_clock(buf, sizeof buf, g_lap_time);
        hud_text(8, 0x48, buf, 3, 2, 1);
        hud_clock(buf, sizeof buf, g_record_shown);     /* 0x23658: the record, not this race's best */
        hud_text(8, 0x58, buf, 3, 2, 1);
        LAY_BOT();
        hud_radar();
        LAY_TOP();
    }

    if (g_race_clock < 8.0f) hud_sprite_draw(0x78, 0x36, (int)(g_race_clock) / 2);
    LAY_MID();

    /*
     * THE END OF THE RACE (0x20eb8 finished, 0x20f88 out of time, 0x21048 draws
     * it; build/decomp/race_end.c).  Finished: "GREAT!" at y 62 if you won,
     * "<n>TH POSITION" at y 90 and the total time at y 120, all centred in
     * font 0 colour 2.  Out of time, once the car has rolled to a stop:
     * "TIME OVER" at y 90.  The game holds that for five seconds (and the
     * voice), on its trackside camera, and leaves the race; the port keeps it
     * on screen until ENTER starts the race again.
     */
    if (g_finished && g_mode == MODE_ATTACK) {
        hud_text_centred(0x5a, g_text[g_attack_won ? 14 : 13], 0, 2);
    } else if (g_finished && g_mode != 1) {
        hud_text_centred(0x5a, g_text[14], 0, 2);
        if (g_perfect) hud_text_centred(0x78, g_text[15], 0, 2);
    } else if (g_finished) {
        if (!g_final_rank) g_final_rank = g_player_rank;
        if (g_final_rank == 1) hud_text_centred(0x3e, g_text[11], 0, 2);
        snprintf(buf, sizeof buf, "%s %s", g_text[g_final_rank >= 1 && g_final_rank <= 10 ? g_final_rank : 10],
                 g_text[0]);
        hud_text_centred(0x5a, buf, 0, 2);
        hud_clock(buf, sizeof buf, g_total_time);
        hud_text_centred(0x78, buf, 0, 2);
    } else if (g_game_over) {
        hud_text_centred(0x5a, g_text[22], 0, 2);
    }
}

/* 0x219d0: the picture darkened three times, the question, YES and NO */
static void dialog_draw(int tick)
{
    for (int i = 0; i < FB_W * FB_H; i++) {
        uint32_t c = fb[i];
        uint32_t r = (c >> 16 & 255) * 27 / 64, g = (c >> 8 & 255) * 27 / 64, b = (c & 255) * 27 / 64;
        fb[i] = 0xff000000u | r << 16 | g << 8 | b;
    }
    int blink = tick & 0x10;
    hud_text_centred(0x34, g_text[19], 0, 2);
    hud_text_centred(0x5c, g_text[20], 0, (g_dialog_sel == 0 && blink) ? 2 : 0xe);
    hud_text_centred(0x7a, g_text[21], 0, (g_dialog_sel == 1 && blink) ? 2 : 0xe);
}

/* ------------------------------------------- props and moving scenery */

/* a list of MAPnDAT.DTR as rows of words, type prefixes still on */
static int dtr_rows(const char *file, const char *section, char rows[][8][24], int max)
{
    FILE *f = fopen(file, "r");
    if (!f) return 0;
    char line[256];
    int in = 0, n = 0;
    while (fgets(line, sizeof line, f) && n < max) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (!in) { if (!strncmp(p, section, strlen(section))) in = 1; continue; }
        if (!strncmp(p, "END_LIST", 8)) break;
        if (*p == '*' || *p == '\r' || *p == '\n' || !*p) continue;
        int k = 0;
        memset(rows[n], 0, sizeof rows[n]);
        for (char *tok = strtok(p, " \t\r\n"); tok && k < 8; tok = strtok(NULL, " \t\r\n"))
            snprintf(rows[n][k++], 24, "%s", tok);
        if (k) n++;
    }
    fclose(f);
    return n;
}

static int splines_load(const char *file, gspline *out, int max)
{
    FILE *f = fopen(file, "rb");
    int n = 0;
    if (!f) return 0;
    int32_t hdr[2];
    while (n < max && fread(hdr, 4, 2, f) == 2) {
        gspline *sp = &out[n];
        if (hdr[0] < 1 || hdr[0] > SPL_NODES_MAX) break;
        if (fread(sp->node, 56, (size_t)hdr[0], f) != (size_t)hdr[0]) break;
        sp->n = hdr[0];
        sp->start = (hdr[1] >= 0 && hdr[1] < hdr[0]) ? hdr[1] : 0;
        n++;
    }
    fclose(f);
    return n;
}

static int mesh_by_name(mesh *m, const char *gfxdir, const char *name)
{
    char file[600], up[24];
    int i = 0;
    for (; name[i] && i < 23; i++) up[i] = (char)toupper((unsigned char)name[i]);
    up[i] = 0;
    snprintf(file, sizeof file, "%s/%s.3DX", gfxdir, up);
    return mesh_load(m, file) == 0;
}

/*
 * THE KNOCK-OVER PROPS (0x15178 sets them out, 0x124c0 is the car hitting
 * one, 0x205b4 moves one that has been hit; build/decomp/props_and_animations.c).
 *
 * MAPnOST.SPL is not a path at all: every node of its one spline is ONE PROP.
 * Its point is where the prop stands and the number where a spline keeps its
 * height (+0x0C) is the prop's KIND: the row of OSTACLES in MAPnDAT.DTR.  A
 * row has three names -- cone / bunny / skittle, tyre stack / lily / TNT,
 * gate / girl / straw -- and a game option (0x1d270 & 3) picks the column;
 * the port uses the first unless OS_PROPS=1 or 2.
 *
 * A prop stands on the track under it, turned the way the road runs.  The car
 * hits it when either of its two circles comes within radius + 200 of it:
 * the prop is thrown the way the car is heading at 1.4 x the car's speed and
 * upwards at 0.02 x per frame.  In the air it loses 2000 units/s of speed,
 * falls at 500 units/s per second, tumbles in proportion to its speed, bounces
 * off the ground keeping 0.8 of its fall until that is under 50, and where it
 * would leave the track it turns back with 0.3 of its speed.  Then it lies
 * where it stopped, and can be hit again.
 *
 * One stand-in: the game adds the upward speed to the height once per FRAME,
 * whatever the frame took, so how high a prop flies depended on the PC.  The
 * port steps 50 times a second and scales that to a 30 frames/s machine.
 */
#define PROPS_MAX 64
#define PROP_KINDS 8
typedef struct {
    float x, y, z, yaw, tilt[3];
    float x0, y0, z0, yaw0, pitch0, roll0;
    float gz;               /* the ground under it while it flies */
    float dx, dy, v, vz;
    int   flying;
    int   counted;          /* +0x3c: has been knocked over this race */
    scene_object *obj;
} prop;
static prop  g_prop[PROPS_MAX];
static int   g_nprop;
static mesh  g_prop_mesh[PROP_KINDS];
static int   g_prop_mesh_ok[PROP_KINDS];
static int   g_props_hit;

static int props_total(void) { return g_nprop; }
static int props_hit(void)   { return g_props_hit; }

static void props_reset(void)
{
    g_props_hit = 0;
    for (int i = 0; i < g_nprop; i++) {
        prop *q = &g_prop[i];
        q->counted = 0;
        q->x = q->x0; q->y = q->y0; q->z = q->z0; q->yaw = q->yaw0;
        q->tilt[0] = q->pitch0; q->tilt[1] = 0.0f; q->tilt[2] = q->roll0;
        q->gz = q->z0;
        q->v = q->vz = 0.0f; q->flying = 0;
    }
}

static void props_show(void)
{
    for (int i = 0; i < g_nprop; i++) {
        prop *q = &g_prop[i];
        if (!q->obj) continue;
        q->obj->x = q->x; q->obj->y = q->y; q->obj->z = q->z;
        q->obj->yaw = q->yaw + q->tilt[1];
        q->obj->pitch = q->tilt[0]; q->obj->roll = q->tilt[2];
    }
}

static void props_init(const path *r, const char *data, const char *gfxdir, int track)
{
    static gspline spl[1];
    static char rows[PROP_KINDS][8][24];
    char file[600];
    g_nprop = 0;
    if (getenv("OS_NOPROPS")) return;
    snprintf(file, sizeof file, "%s/MAP%dDAT.DTR", data, track);
    int nkind = dtr_rows(file, "OSTACLES", rows, PROP_KINDS);
    int column = getenv("OS_PROPS") ? atoi(getenv("OS_PROPS")) % 3 : (g_cheat & 3) % 3;     /* 0x1532c */
    for (int k = 0; k < nkind; k++)
        g_prop_mesh_ok[k] = rows[k][column][0] == 's' &&
                            mesh_by_name(&g_prop_mesh[k], gfxdir, rows[k][column] + 1);
    /* OS_PROPFILE=CNG or SLA: the layouts of the game's other two modes */
    snprintf(file, sizeof file, "%s/MAP%d%s.SPL", data, track,
             getenv("OS_PROPFILE") ? getenv("OS_PROPFILE")
             : g_mode == MODE_CONES ? "CNG" : g_mode == MODE_SLALOM ? "SLA" : "OST");   /* 0x14fb4 */
    if (!splines_load(file, spl, 1)) return;
    int node = spl[0].start;
    for (int i = 0; i < spl[0].n && g_nprop < PROPS_MAX && g_ncar < MAX_CARS; i++) {
        const int32_t *n = spl[0].node[node];
        int kind = n[3];
        node = (n[4] >= 0 && n[4] < spl[0].n) ? n[4] : 0;
        if (kind < 0 || kind >= nkind || !g_prop_mesh_ok[kind]) continue;
        prop *q = &g_prop[g_nprop++];
        memset(q, 0, sizeof *q);
        q->x0 = (float)n[1]; q->y0 = (float)n[2];
        const scene_dir *d = nearest_dir(q->x0, q->y0);
        q->yaw0 = d ? d->angle : 0.0f;
        /* 0x15178: it stands on the drivable surface under it, tilted to it.
           A prop with no track under it gets whatever the search found last
           -- the prop before it (track 5's normal-mode props are track 4's
           file and stand nowhere near its road) */
        {
            static float last;
            static int   have;
            float g;
            if (g_nprop == 1) have = 0;
            if (track_under(q->x0, q->y0, 1, &g)) {
                last = g; have = 1;
                slope_tilt(q->yaw0, &q->pitch0, &q->roll0);
            }
            q->z0 = have ? last : r->p[path_nearest(r, q->x0, q->y0)][2];
        }
        q->obj = &g_car[g_ncar++];
        memset(q->obj, 0, sizeof *q->obj);
        q->obj->m = &g_prop_mesh[kind];
        q->obj->phase = -1;
        q->obj->radius = 3000.0f;
    }
    props_reset();
    props_show();
    printf("props: %d of %d kinds\n", g_nprop, nkind);
    if (getenv("OS_PROPDUMP"))
        for (int i = 0; i < g_nprop; i++)
        {
            float g;
            int hit = surface_pick(g_prop[i].x0, g_prop[i].y0, g_prop[i].z0 + 600.0f, 1, &g);
            const scene_object *under = (hit && g_pick_obj >= 0 && g_pick_obj < g_scene->count)
                                      ? &g_scene->obj[g_pick_obj] : NULL;
            printf("  prop %2d  kind %d  at %.0f, %.0f, %.0f  stands on piece V%d (run %d), material %d %dx%d\n",
                   i, (int)(g_prop[i].obj->m - g_prop_mesh), g_prop[i].x0, g_prop[i].y0, g_prop[i].z0,
                   under ? under->v : -1, under ? under->layer : -1, hit ? g_pick_mat : -1,
                   hit && g_pick_mat >= 0 ? g_mat[g_pick_mat].su : 0, hit && g_pick_mat >= 0 ? g_mat[g_pick_mat].sv : 0);
        }
}

/* one 1/50 s step: the car against the props, and the ones in the air */
static void props_step(void)
{
    const float dt = DRIVE_DT, frames = DRIVE_DT * 30.0f;
    const car_state *c = &g_pc;
    float cx[2], cy[2], rad = 0.0f;
    if (g_pobj && g_pobj->m) car_circles(g_pobj->m, c->x, c->y, c->yaw, cx, cy, &rad);
    for (int i = 0; i < g_nprop; i++) {
        prop *q = &g_prop[i];
        if (!q->flying && g_pobj && fabsf(c->x - q->x) < 1500.0f &&
            fabsf(c->y - q->y) < 1500.0f && fabsf(c->z - q->z) < 1500.0f)
            for (int k = 0; k < 2 && !q->flying; k++) {
                float dx = cx[k] - q->x, dy = cy[k] - q->y;
                if (sqrtf(dx * dx + dy * dy) - (rad + 100.0f) >= 100.0f) continue;
                q->dx = sinf(c->yaw); q->dy = -cosf(c->yaw);
                q->vz = c->speed * 0.02f;
                q->v  = c->speed * 1.4f;
                q->flying = 1;
                if (!q->counted) { q->counted = 1; g_props_hit++; }   /* 0x124c0: once a prop */
                snd_play(SND_HIT);                      /* 0x124c0 */
                rumble_kick(0.25f, 0.5f, 0.06f);
            }
        /* OS_PROPKICK=prop,speed: throw one as if hit at that speed, and print its flight */
        if (getenv("OS_PROPKICK") && g_race_clock > 0.2f) {
            static int done = 0;
            int which = 0; float sp = 0.0f;
            sscanf(getenv("OS_PROPKICK"), "%d,%f", &which, &sp);
            if (!done && which == i) {
                done = 1;
                q->dx = sinf(q->yaw); q->dy = -cosf(q->yaw);
                q->vz = sp * 0.02f; q->v = sp * 1.4f; q->flying = 1;
            }
            if (which == i && q->flying)
                printf("  prop %d  %.2fs  at %.0f, %.0f  height %.0f  speed %.0f\n", i,
                       g_race_clock, q->x, q->y, q->z, q->v);
        }
        if (!q->flying) continue;
        float ox = q->x, oy = q->y, ground = q->gz;
        q->x += q->v * q->dx * dt;
        q->y += q->v * q->dy * dt;
        q->tilt[0] -= q->v * q->dx * dt * 0.008f;
        q->tilt[2] -= q->v * q->dy * dt * 0.0015f;
        q->v -= dt * 2000.0f;
        if (q->v < 0.0f) q->v = 0.0f;
        /* 0x206f4: the ground is the drivable surface under it; over nothing
           it is turned back with 0.3 of its speed and, for this frame, the
           ground is the height it is at */
        float g;
        int off = !track_under(q->x, q->y, 1, &g);
        if (off) {
            ground = q->z;
            q->x = ox; q->y = oy;
            q->dx = -q->dx; q->dy = -q->dy;
            q->v *= 0.3f;
        } else ground = g;
        q->vz -= dt * 500.0f;
        q->z += q->vz * frames;
        if (q->z <= ground) {
            q->z = ground;
            q->vz = -q->vz * 0.8f;
            if (fabsf(q->vz) < 50.0f) q->vz = 0.0f;
            if (q->v == 0.0f && q->vz == 0.0f) {
                q->flying = 0;
                if (off) q->z = -20000.0f;          /* off the track: gone */
                else {                              /* it lies on its side, along the slope (0x20880) */
                    float pitch, roll;
                    slope_tilt(q->yaw + q->tilt[1], &pitch, &roll);
                    q->tilt[0] = pitch + 1.5707964f;
                }
            }
        }
        q->gz = ground;
    }
    props_show();
}

/*
 * THE MOVING SCENERY (0x15374 sets it up, 0x20944 moves it, 0x20b0c steps
 * along the curve).  ANIMATIONS in MAPnDAT.DTR is rows of
 *     name   speed   spline   sound
 * and each object drives round spline number `spline` of MAPnANM.SPL at
 * `speed` units a second, for ever: buses and a truck over track 1's
 * bridges, a jet, trams, a helicopter, a cable car.  The curve is the same
 * quadratic B-spline over the nodes' handles that the opponents use; the
 * HEIGHT is the nodes' own (+0x0C), run straight from one node to the next;
 * the object faces the way it is going.  Speed 0 = it stands where its spline
 * starts (the AKKNULLA rows, an empty object that only carries a sound).
 * The game moves one only while it is within 52000 units of the camera (so
 * does the port), and not at all with its "animations" option off.
 */
#define ANIMS_MAX 8
typedef struct { int node, half, sound; float u, speed; const gspline *sp; scene_object *obj; } anim;
static anim    g_anim[ANIMS_MAX];
static int     g_nanim;
static gspline g_anim_spl[ANIMS_MAX];
static mesh    g_anim_mesh[ANIMS_MAX];

static void anim_place(anim *a)
{
    float A[2], B[2], C[2], pos[2], der[2];
    opp_piece(a->sp, a->node, a->half, A, B, C);
    opp_curve(A, B, C, a->u, pos, der);
    const int32_t *n = a->sp->node[a->node];
    int nx = (n[4] >= 0 && n[4] < a->sp->n) ? n[4] : 0;
    float z0 = (float)n[3], z1 = (float)a->sp->node[nx][3];
    a->obj->x = pos[0]; a->obj->y = pos[1];
    a->obj->z = z0 + ((float)a->half + a->u) * (z1 - z0) * 0.5f;
    if (der[0] != 0.0f || der[1] != 0.0f) a->obj->yaw = atan2f(der[0], -der[1]);
}

static void anims_init(const char *data, const char *gfxdir, int track)
{
    static char rows[ANIMS_MAX][8][24];
    char file[600];
    g_nanim = 0;
    if (getenv("OS_NOANIMS") || g_detail == 0) return;      /* LOW DETAIL has none (0x15374) */
    snprintf(file, sizeof file, "%s/MAP%dANM.SPL", data, track);
    int nspl = splines_load(file, g_anim_spl, ANIMS_MAX);
    snprintf(file, sizeof file, "%s/MAP%dDAT.DTR", data, track);
    int nrow = dtr_rows(file, "ANIMATIONS", rows, ANIMS_MAX);
    for (int k = 0; k < nrow && g_ncar < MAX_CARS; k++) {
        int spl = atoi(rows[k][2] + 1);
        if (rows[k][0][0] != 's' || spl < 0 || spl >= nspl || g_anim_spl[spl].n < 2) continue;
        if (!mesh_by_name(&g_anim_mesh[g_nanim], gfxdir, rows[k][0] + 1)) continue;
        anim *a = &g_anim[g_nanim];
        memset(a, 0, sizeof *a);
        a->sp = &g_anim_spl[spl];
        a->node = a->sp->start;
        a->speed = (float)atof(rows[k][1] + 1);
        a->sound = rows[k][3][0] == 'd' ? atoi(rows[k][3] + 1) : -1;
        a->obj = &g_car[g_ncar++];
        memset(a->obj, 0, sizeof *a->obj);
        a->obj->m = &g_anim_mesh[g_nanim];
        a->obj->phase = -1;
        a->obj->radius = 12000.0f;
        anim_place(a);
        g_nanim++;
    }
    printf("moving scenery: %d of %d rows, %d splines\n", g_nanim, nrow, nspl);
}

static void anims_step(void)
{
    for (int k = 0; k < g_nanim; k++) {
        anim *a = &g_anim[k];
        /* only within 52000 units of the viewpoint, as the game: each one
           waits where its spline starts until the player comes near, which
           is what times the bus onto the bridge as you arrive */
        if (fabsf(g_pc.x - a->obj->x) >= 52000.0f || fabsf(g_pc.y - a->obj->y) >= 52000.0f)
            continue;
        float left = a->speed * DRIVE_DT;
        for (int guard = 0; guard < 8 && left > 0.0f; guard++) {
            float A[2], B[2], C[2], pos[2], der[2];
            opp_piece(a->sp, a->node, a->half, A, B, C);
            opp_curve(A, B, C, a->u, pos, der);
            float len = sqrtf(der[0] * der[0] + der[1] * der[1]);
            if (len < 1.0f) len = 1.0f;
            float du = left / len;
            if (a->u + du < 1.0f) { a->u += du; break; }
            left -= (1.0f - a->u) * len;
            a->u = 0.0f;
            if (a->half == 0) a->half = 1;
            else {
                a->half = 0;
                int nx = a->sp->node[a->node][4];
                a->node = (nx >= 0 && nx < a->sp->n) ? nx : 0;
            }
        }
        anim_place(a);
        if (getenv("OS_PROPDUMP") && fmodf(g_race_clock, 2.0f) < DRIVE_DT)
            printf("  moving %d  %.1fs  at %.0f, %.0f, %.0f  heading %.2f\n", k, g_race_clock,
                   a->obj->x, a->obj->y, a->obj->z, a->obj->yaw);
    }
}

/*
 * THE TRACKSIDE CAMERA (view-table row 3, routine 0x1a620).  The game cuts to
 * it when the race ends, and its replays use it.  The camera positions are the
 * nodes of MAPnRPL.SPL (not MAPnCAM.SPL, which the code loads next to it):
 * the node nearest the car is taken, the camera looks from there straight at
 * the car, and if the car is more than 1500 units away the camera is moved
 * along its own line of sight until it is 1500 from it.  So it is a camera
 * that always sits 1500 from the car (4.5 m), on the side of the nearest node
 * and at the angle that node's height gives.
 */
static gspline g_rpl_spl[1];
static int     g_have_rpl;

static void trackside_init(const char *data, int track)
{
    char file[600];
    snprintf(file, sizeof file, "%s/MAP%dRPL.SPL", data, track);
    g_have_rpl = splines_load(file, g_rpl_spl, 1);
}

static int trackside_camera(camera *cam)
{
    if (!g_have_rpl || !g_pobj) return 0;
    const car_state *c = &g_pc;
    float best = 1e30f;
    const int32_t *at = NULL;
    for (int i = 0; i < g_rpl_spl[0].n; i++) {
        const int32_t *n = g_rpl_spl[0].node[i];
        float dx = (float)n[1] - c->x, dy = (float)n[2] - c->y, dz = (float)n[3] - c->z;
        float d = sqrtf(dx * dx + dy * dy + dz * dz);
        if (d > 0.0f && d < best) { best = d; at = n; }
    }
    if (!at) return 0;
    float x = (float)at[1], y = (float)at[2], z = (float)at[3];
    float dx = c->x - x, dy = c->y - y, dz = c->z - z;
    float flat = sqrtf(dx * dx + dy * dy);
    cam->yaw = atan2f(dy, dx);
    cam->pitch = atan2f(dz, flat);
    cam->roll = 0.0f;
    float move = best - 1500.0f;
    if (move > 0.0f) { x += dx / best * move; y += dy / best * move; z += dz / best * move; }
    cam->x = x; cam->y = y; cam->z = z;
    return 1;
}

/*
 * SOUND, once a physics step.  What the game does with its sound effects:
 *
 *   engine   one looping voice for the player's car, started when the race is
 *            set up (0x128dc) and stopped when it ends; its pitch is
 *            rpm x 14 + 32768 in 16.16 (0x19408), so idling it plays at half
 *            speed and at 8000 rpm at 2.2 times.  The sample is AUTOMOTO, or
 *            AUTOGALL while the lap marker the car is in has kind 1 -- a
 *            tunnel (0x1813c, table at 0x1d78);
 *   skid     SKIDFX loops while the car is skidding -- the same test that
 *            makes the tyre smoke (0x17d04) -- and stops with it;
 *   wall     WALLH1 on a wall hit unless it is still sounding; DENTONE when
 *            a crash starts; HIT_0 when a prop is knocked over.
 *
 * The voices, the opponents' engines and the scenery's sounds follow below.
 */
/*
 * THE COMMENTARY (0x2479c queues an event, 0x24820 plays the queue, 0x24a24
 * picks the sample; the per-frame triggers are 0x2425c).
 *
 * There are 27 EVENTS, each with up to three recordings (table at 0x1fb0,
 * three sample numbers an event, counted through SPFXBASE.DTR then
 * SPFXFULL.DTR):
 *    0-3 three two one go    4 check   5 lap      7 newlap   8 beat
 *    9 crash   10 skid   11 win   12 lose   13 bad   14 well   15 hitcar
 *   16 back    17 stop   19 cones  20 bend   21 lead  22 hurry  23 lapped
 *   24 loslea  25 notime 26 all
 * An event is queued with a MODE (0, 1, 2 = that recording; 3 = take turns,
 * starting anywhere) and a DELAY; while no voice is speaking a clock runs,
 * the queued event with the shortest delay goes first and is spoken once the
 * clock has passed its delay, and the clock starts again when it has finished.
 * An event already in the queue is not queued twice.
 *
 * Triggers, with their own timers (0x19228): "once it has been true for T
 * seconds, say it, and not again until it has been false" (0x24bec), "say it
 * every T seconds while it is true" (0x24c68), "say it once until re-armed"
 * (0x24cd0 / 0x24d1c).
 */
#define EVENTS 27
#define QUEUE  8
static const char *g_ev_name[EVENTS][3] = {
    { "three" }, { "two" }, { "one" }, { "go" },
    { "check0", "check1", "check2" }, { "lap0", "lap1", "lap2" }, { 0 },
    { "newlap0", "newlap1", "newlap2" }, { "beat0", "beat1" }, { "crash0", "crash1", "crash2" },
    { "skid0", "skid1" }, { "win0", "win1", "win2" }, { "lose0", "lose1", "lose2" },
    { "bad0", "bad1", "bad2" }, { "well0", "well1" }, { "hitcar0", "hitcar1" },
    { "back0", "back1" }, { "stop0", "stop1" }, { 0 }, { "cones0", "cones1", "cones2" },
    { "bend0", "bend1", "bend2" }, { "lead0", "lead1", "lead2" }, { "hurry0", "hurry1", "hurry2" },
    { "lapped0" }, { "loslea0", "loslea1", "loslea2" }, { "notime0", "notime1" },
    { "all0", "all1", "all2" },
};
static int   g_ev_sample[EVENTS][3], g_ev_turn[EVENTS];
static float g_ev_timer[EVENTS];
static struct { int ev, mode; float delay; } g_queue[QUEUE];
static int   g_nqueue, g_voice_handle = -1;
static float g_voice_clock;
static int   g_snd_stage = -1, g_snd_over, g_had_lead;
static int   g_snd_track0;                    /* first of the track's own sounds */
static int   g_anim_snd[ANIMS_MAX], g_opp_snd[OPP_MAX];
static float g_anim_dist[ANIMS_MAX], g_opp_dist[OPP_MAX];

static void comment(int ev, int mode, float delay)              /* 0x2479c */
{
    if (ev < 0 || ev >= EVENTS || g_nqueue >= QUEUE) return;
    if (getenv("OS_NOSPEECH")) return;          /* SPEECH OFF (the game's flag at 0x1d2c4) */
    for (int i = 0; i < g_nqueue; i++) if (g_queue[i].ev == ev) return;
    g_queue[g_nqueue].ev = ev; g_queue[g_nqueue].mode = mode; g_queue[g_nqueue].delay = delay;
    g_nqueue++;
}

static int comment_hold(int cond, int ev, float t)              /* 0x24bec */
{
    if (!cond) { g_ev_timer[ev] = 0.0f; return 0; }
    if (g_ev_timer[ev] < 0.0f) return 0;
    g_ev_timer[ev] += DRIVE_DT;
    if (g_ev_timer[ev] < t) return 0;
    comment(ev, 3, 0.7f);
    g_ev_timer[ev] = -1.0f;
    return 1;
}

static void comment_repeat(int cond, int ev, float t)           /* 0x24c68 */
{
    if (!cond) { g_ev_timer[ev] = 0.0f; return; }
    g_ev_timer[ev] += DRIVE_DT;
    if (g_ev_timer[ev] >= t) { comment(ev, 3, 0.7f); g_ev_timer[ev] = 0.0f; }
}

static void comment_once(int ev, int mode)                      /* 0x24cd0 */
{
    if (g_ev_timer[ev] == -1.0f) return;
    comment(ev, mode, 0.7f);
    g_ev_timer[ev] = -1.0f;
}

static void comment_rearm(int ev) { g_ev_timer[ev] = 0.0f; }    /* 0x24d1c */

static int comment_pick(int ev, int mode)                       /* 0x24a24 */
{
    if (mode >= 0 && mode < 3) return g_ev_sample[ev][mode];
    for (int tries = 0; tries < 3; tries++) {
        g_ev_turn[ev] = (g_ev_turn[ev] + 1) % 3;
        if (g_ev_sample[ev][g_ev_turn[ev]] >= 0) break;
    }
    return g_ev_sample[ev][g_ev_turn[ev]];
}

static void comment_step(void)                                  /* 0x24820 */
{
    if (g_voice_handle != -1) {
        if (!snd_playing(g_voice_handle)) {
            g_voice_handle = -1;
            if (g_nqueue) memmove(&g_queue[0], &g_queue[1], sizeof g_queue[0] * (size_t)--g_nqueue);
            g_voice_clock = 0.0f;
        }
        return;
    }
    g_voice_clock += DRIVE_DT;
    if (!g_nqueue) return;
    int first = 0;
    for (int i = 1; i < g_nqueue; i++) if (g_queue[i].delay < g_queue[first].delay) first = i;
    if (first) { __typeof__(g_queue[0]) t = g_queue[0]; g_queue[0] = g_queue[first]; g_queue[first] = t; }
    if (g_voice_clock < g_queue[0].delay) return;
    {
        int smp = comment_pick(g_queue[0].ev, g_queue[0].mode);
        g_voice_handle = snd_play(smp);
        if (getenv("OS_SNDLOG")) {
            const char *nm = "-";
            for (int v = 0; v < 3; v++) if (g_ev_sample[g_queue[0].ev][v] == smp && g_ev_name[g_queue[0].ev][v]) nm = g_ev_name[g_queue[0].ev][v];
            printf("voice  %6.2fs  event %2d  %s%s\n", g_race_clock, g_queue[0].ev, nm,
                   g_voice_handle == -1 ? "  (no sample)" : "");
        }
    }
    if (g_voice_handle == -1) {                 /* nothing to say for it: drop it */
        memmove(&g_queue[0], &g_queue[1], sizeof g_queue[0] * (size_t)--g_nqueue);
        g_voice_clock = 0.0f;
    }
}

/*
 * WHICH SONG.  The table at 0x1efc holds placeholders in the program; at
 * start-up (0x127b4) it is filled from the CD_TRACKS list of MENU.DTR, eight
 * numbers: [0] game mode 0x100, [1] the menu, [2..7] the six tracks.  As
 * shipped: 1, 2, 5, 3, 4, 6, 7, 1.  The same number is the MUSICn.HMP file
 * and, plus one, the CD track.
 */
static int song_for(int slot)
{
    static int table[8] = { 1, 2, 5, 3, 4, 6, 7, 1 }, loaded;
    if (!loaded) {
        loaded = 1;
        FILE *f = fopen("build/dec/GEN/MENU.DTR", "r");
        if (f) {
            char line[256];
            int in = 0, n = 0, v;
            while (fgets(line, sizeof line, f)) {
                char *p = line;
                while (*p == ' ' || *p == '\t') p++;
                if (!in) { if (!strncmp(p, "CD_TRACKS", 9)) in = 1; continue; }
                if (!strncmp(p, "END_LIST", 8)) break;
                if (n < 8 && sscanf(p, "d%d", &v) == 1) table[n++] = v;
            }
            fclose(f);
        }
    }
    return table[slot & 7];
}

static void sound_load(const char *data, int track)
{
    char file[600];
    snd_init("build/dec/SOUND", "build/dec/GEN/SFXSBASE.DTR");
    for (int e = 0; e < EVENTS; e++)
        for (int v = 0; v < 3; v++)
            g_ev_sample[e][v] = g_ev_name[e][v] ? snd_add(g_ev_name[e][v], 1.0f, 0, 0, 0) : -1;
    snprintf(file, sizeof file, "%s/MAP%dDAT.DTR", data, track);
    g_snd_track0 = snd_add_list(file);
    /* 0x134f8: with MUSIC on (0x1d2c8) the race plays "music%d" of song_for(2 + track) */
    {
        int n = getenv("OS_MUSIC") ? atoi(getenv("OS_MUSIC")) : g_intro ? song_for(0) : song_for(2 + (track - 1) % 6);
        /* everything starts SILENT: a race and the intro both fade in from
           nothing (0x21e50), and the song must not be heard at full volume
           for the moment the rest of the loading takes (Danik) */
        volumes_apply(0.0f);
        if (!getenv("OS_NOMUSIC")) snd_music("build/dec/MUSIC", n);
        g_song_now = n;
        volumes_apply(0.0f);
        if (getenv("OS_SNDLOG")) printf("music: track %d plays MUSIC%d.HMP\n", track, n);
    }
    if (os_once(2)) atexit(snd_close);
}

static void sound_restart(void)
{
    g_snd_engine = snd_stop(g_snd_engine);
    g_snd_skid = snd_stop(g_snd_skid);
    g_voice_handle = snd_stop(g_voice_handle);
    for (int k = 0; k < ANIMS_MAX; k++) { g_anim_snd[k] = snd_stop(g_anim_snd[k]); g_anim_dist[k] = 0.0f; }
    for (int k = 0; k < OPP_MAX; k++) { g_opp_snd[k] = snd_stop(g_opp_snd[k]); g_opp_dist[k] = 0.0f; }
    g_snd_engine = snd_play(SND_ENGINE);
    g_snd_stage = -1;
    g_snd_over = 0;
    g_nqueue = 0; g_voice_clock = 0.7f;
    g_had_lead = 0;
    for (int e = 0; e < EVENTS; e++) { g_ev_timer[e] = 0.0f; g_ev_turn[e] = rand() % 3; }
}

/*
 * A SOUND THAT COMES FROM SOMEWHERE (0x21b80, the moving scenery): it starts
 * when its source comes within 40000 units and stops beyond that; its
 * loudness is ((40000 - distance) / 40000) squared, and its pitch rises and
 * falls with how fast the distance is closing: 65536 + 9 x (units nearer this
 * frame).  It is panned too: sound_pan().
 */
/*
 * 0x21cfb / 0x1dffc: the pan.  The line from the source to the camera is
 * turned into the camera's frame; pan = 0x8000 +- 32767 x sqrt(|sideways| /
 * distance) -- a square root, so a source only a little off to one side is
 * already well over to that speaker.  Returned as -1 (left) .. +1 (right).
 */
static float g_ear[2], g_ear_yaw;      /* where sounds are heard from, and which way it faces */

static float sound_pan(float x, float y)
{
    float dx = x - g_ear[0], dy = y - g_ear[1];
    float d = sqrtf(dx * dx + dy * dy);
    if (d < 1.0f) return 0.0f;
    float side = (sinf(g_ear_yaw) * dx - cosf(g_ear_yaw) * dy) / d;   /* to_view's x */
    return side < 0.0f ? -sqrtf(-side) : sqrtf(side);
}

/*
 * THE DOPPLER SHIFT, and who is listening (0x1dffc the cars, 0x21b80 the
 * scenery; the distance is worked out at 0x1e34b).
 *
 * WHO LISTENS (0x212c0, once a frame from the camera's update): while a race
 * is being run (flag 0x100, set with the race) and it is not the intro (flag
 * 0x2000), THE PLAYER'S CAR -- its place and its heading, whatever view is on
 * the screen, the trackside cameras of the end and the replay included; in
 * the intro, where there is no player, THE CAMERA.  (The port said "the
 * camera, always" for a day: the record at 0x13cc8 is filled by this routine,
 * not by the camera.)  The pitch of a sound with a place has
 *     9 x (distance last frame - distance this frame)
 * added, in 16.16 where 65536 is the sample as recorded: a car coming at the
 * camera sounds higher, one going away lower, and one passing a camera post
 * drops from one to the other.  When the camera CUTS to another place the
 * game starts the comparison afresh (the car's +0x578 flag puts last = this),
 * or the jump of the camera would be heard as a squeal.
 *
 * It is "per frame" in the game, so its strength went with the frame rate;
 * the port takes 30 frames a second (OS_DOPPLER_FPS=n to try another: fewer
 * frames = a stronger shift).
 */
static int   g_ear_cut;                /* the listener has just jumped: no shift this step */
static float g_doppler = 50.0f / 30.0f;

static int sound_from(int handle, float *last, int sample, float x, float y, float pitch_scale)
{
    float d = sqrtf((x - g_ear[0]) * (x - g_ear[0]) + (y - g_ear[1]) * (y - g_ear[1]));
    if (g_ear_cut) *last = d;
    if (d >= 40000.0f || sample < 0) {
        if (handle != -1) handle = snd_stop(handle);
        *last = d;
        return handle;
    }
    if (!snd_playing(handle)) { handle = snd_play(sample); *last = d; }
    float k = (40000.0f - d) / 40000.0f;
    snd_volume(handle, k * k);
    snd_pan(handle, sound_pan(x, y));
    /* per 1/30 s frame, as for the other frame-rate-bound rules */
    float closing = (*last - d) * g_doppler;
    snd_pitch(handle, (int)((65536.0f + 9.0f * closing) * pitch_scale));
    *last = d;
    return handle;
}

static void sound_world(float lx, float ly, int over);

static void sound_step(void)
{
    const car_state *c = &g_pc;
    int over = g_finished || g_game_over;
    if (over && !g_snd_over) {
        g_snd_over = 1;
        g_snd_engine = snd_stop(g_snd_engine);
        g_snd_skid = snd_stop(g_snd_skid);
        g_nqueue = 0;
        if (g_game_over) comment(25, 3, 0.0f);                  /* 0x20f88 */
        else if (g_mode == MODE_ATTACK) comment(g_attack_won ? 11 : 12, 3, 0.0f);   /* 0x23ef0 */
        else comment(g_player_rank == 1 ? 11 : 12, 3, 0.0f);    /* 0x23e78 */
    }
    if (!over) {
        int kind = g_nmark ? g_mark[g_mark_at].kind : 0;
        snd_swap(g_snd_engine, kind == 1 ? SND_ENGINE_TUNNEL : SND_ENGINE);
        snd_pitch(g_snd_engine, (int)(c->rpm * 14.0f + 32768.0f));
        if (g_skidding && !g_crash.on) {
            if (!snd_playing(g_snd_skid)) g_snd_skid = snd_play(SND_SKID);
        } else if (g_snd_skid != -1) {
            g_snd_skid = snd_stop(g_snd_skid);
        }
        /* 0x20d45: the countdown, one event a state of the lights */
        int stage = (int)g_race_clock / 2;
        if (stage != g_snd_stage && stage < 4) { g_snd_stage = stage; comment(stage, 0, 0.0f); }
    }

    /* 0x2425c: what the commentator notices, once the race is on */
    if (g_race_on && !over) {
        const scene_dir *d = nearest_dir(c->x, c->y);
        float off = d ? fabsf(wrap_pi(c->yaw - (d->angle + 3.1415927f))) : 3.0f;
        int wrong_way = off < 0.7853982f;
        comment_repeat(wrong_way, 16, 5.0f);
        comment_repeat(c->speed < 1000.0f, 17, 8.0f);
        /* which of these a mode has is the game's own masks on 0x1d25c */
        if ((g_mode == 1 || g_mode == MODE_ATTACK) && g_timer <= 5.0f) comment_once(22, 3);
        if (g_mode == 1 && comment_hold(g_player_rank == 1, 21, 5.0f)) g_had_lead = 1;
        if (comment_hold(g_had_lead && g_player_rank >= 2, 24, 5.0f)) g_had_lead = 0;
        if (g_mode == MODE_CONES && props_total() == props_hit()) comment_once(26, 3);      /* 0x2437b */
        if (g_mode == MODE_SLALOM && g_ngate == g_gates_done && props_hit() == 0) comment_once(26, 3);
        if (g_mode == MODE_CONES && g_clock_on) comment_once(19, 3);                        /* 0x24570 */
        if (!wrong_way) comment_hold(g_nmark && (g_mark[g_mark_at].flags & 4), 20, 0.0f);
        comment_hold((int)c->slide_want != 0, 10, c->spec.grip * 2.9f);      /* 0x243e3: player +0x488 is the car's grip */
        /* player +0x788 is the number of opponents, written once when the race
           is set up (0x12c34): "bad" is for running LAST for eight seconds */
        comment_hold((g_nopp < 2 ? 2 : g_nopp) < g_player_rank, 13, 8.0f);
        if (g_mode == 1) comment_hold(g_lapped_one, 23, 2.0f);   /* 0x24464 */
        comment_repeat(g_hitcar > 0.8f, 15, 0.0f);         /* 0x2454a */
    }
    comment_step();
    g_ear_yaw = c->yaw;
    sound_world(c->x, c->y, over);                       /* heard from the player's car (0x212c0) */
    snd_render(DRIVE_DT);
}

/* the sounds that are not the player's own: the moving scenery and the other
   cars' engines, heard from (lx, ly) -- the player's car, or in the intro the
   camera */
static void sound_world(float lx, float ly, int over)
{
    /* a camera that moved further in one step than any car can go has cut
       to another place (the game has a flag for it; the port sees the jump) */
    static int ear_ok;
    if (getenv("OS_DOPPLER_FPS") && atof(getenv("OS_DOPPLER_FPS")) > 1.0) g_doppler = 50.0f / (float)atof(getenv("OS_DOPPLER_FPS"));
    g_ear_cut = !ear_ok || hypotf(lx - g_ear[0], ly - g_ear[1]) > 1000.0f;
    g_ear[0] = lx; g_ear[1] = ly; ear_ok = 1;
    /* the moving scenery */
    for (int k = 0; k < g_nanim; k++)
        g_anim_snd[k] = sound_from(g_anim_snd[k], &g_anim_dist[k],
                                   g_anim[k].sound >= 0 ? g_snd_track0 + g_anim[k].sound : -1,
                                   g_anim[k].obj->x, g_anim[k].obj->y, 1.0f);
    /*
     * 0x1dffc: THE OPPONENTS' ENGINES.  Only cars the game is simulating in
     * full (within 35000 units) can be heard, and only THREE at a time: a
     * nearer car takes the voice of a farther one.  Pitch (16.16) is
     *     speed x 2.4 + 13568 + 9 x (units nearer this frame)
     * and loudness ((35000 - distance) / 35000) squared x 23552 of 32767.
     * Panned by sound_pan().
     */
    int near[3] = { -1, -1, -1 };
    float dist[OPP_MAX];
    for (int k = 0; k < g_nopp; k++) {
        dist[k] = hypotf(g_opp[k].x - lx, g_opp[k].y - ly);
        if (over || dist[k] >= 35000.0f || getenv("OS_NOOPPSOUND")) continue;
        for (int j = 0; j < 3; j++)
            if (near[j] < 0 || dist[k] < dist[near[j]]) {
                for (int m = 2; m > j; m--) near[m] = near[m - 1];
                near[j] = k;
                break;
            }
    }
    for (int k = 0; k < g_nopp; k++) {
        int heard = (k == near[0] || k == near[1] || k == near[2]);
        if (!heard) {
            if (g_opp_snd[k] != -1) g_opp_snd[k] = snd_stop(g_opp_snd[k]);
            g_opp_dist[k] = dist[k];
            continue;
        }
        if (!snd_playing(g_opp_snd[k])) { g_opp_snd[k] = snd_play(SND_OPP_ENGINE); g_opp_dist[k] = dist[k]; }
        if (g_ear_cut) g_opp_dist[k] = dist[k];
        if (getenv("OS_DOPPLERLOG") && k == near[0]) {
            static int n;
            if (n++ % 10 == 0)
                printf("doppler %6.2fs  nearest car %d  %6.0f units  engine %6.0f  shift %+6.0f (%+.1f%%)%s\n",
                       g_race_clock, k, dist[k], g_opp[k].speed * 2.4f + 13568.0f,
                       9.0f * (g_opp_dist[k] - dist[k]) * g_doppler,
                       100.0f * 9.0f * (g_opp_dist[k] - dist[k]) * g_doppler / (g_opp[k].speed * 2.4f + 13568.0f),
                       g_ear_cut ? "  CUT" : "");
        }
        float q = (35000.0f - dist[k]) / 35000.0f;
        snd_volume(g_opp_snd[k], q * q * (23552.0f / 32767.0f));
        snd_pan(g_opp_snd[k], sound_pan(g_opp[k].x, g_opp[k].y));
        /* 0x20e63: ON THE GRID THE OTHERS REV.  Until the green light the
           speed their engine note is made from is not their speed (they stand
           still) but  sin(car number x 0.5 x seconds on the grid) x 15000 --
           car 0 idles, each one further back swings faster.  At the start it
           is put to 0 and the real speed takes over.  Where the swing goes
           below what a pitch can be, the driver's answer is not known; the
           port holds the note at its lowest there. */
        float note_speed = (!g_race_on && !g_intro) ? sinf((float)k * 0.5f * g_race_clock) * 15000.0f : g_opp[k].speed;
        snd_pitch(g_opp_snd[k], (int)(note_speed * 2.4f + 13568.0f
                                      + 9.0f * (g_opp_dist[k] - dist[k]) * g_doppler));
        g_opp_dist[k] = dist[k];
    }
}

/*
 * THE FOUR WHEELS ON THE GROUND (0x11a30 finds the heights, 0x16698 makes the
 * car's height and tilt of them).  Wheels are front-left, rear-left,
 * front-right, rear-right.
 *
 *  - where a wheel is: its place in the model, drawn in by |cos roll|
 *    sideways and |cos pitch| lengthways, turned by the car's heading plus
 *    the angle its body is skidding at (+0x128);
 *  - its height: that of the DRIVABLE surface under it (a record-run-0 piece;
 *    0x11c00).  A wheel over nothing drivable keeps the height it had;
 *  - a wheel on a GRASS piece (name's fourth letter 'e') is rough: its height
 *    is jolted by up to 15 units, up or down, from the wheel's own x + z --
 *    so the car shudders as it moves -- and a FRONT wheel on grass raises the
 *    flag that makes hard braking spin the car and tugs at the steering
 *    (drive.c);
 *  - the car's height is the plain average of the four (jolts and all), with
 *    no smoothing; nose-up is atan((front right - rear right) / wheelbase)
 *    and lean atan((front left - front right) / track) -- one side and one
 *    axle only, as the game has it;
 *  - the shadow's height is the average without the jolts (+0x4ec).
 *
 * One thing kept from the port: of several drivable surfaces over a spot (a
 * road passing under another) the wheel takes the one nearest its own height;
 * the game takes the first its piece list gives, an order the port does not
 * keep.
 */

static void player_contact(void)
{
    car_state *c = &g_pc;
    const mesh *mm = g_pobj->m;
    float tr = mm->track_half > 0.0f ? mm->track_half : 226.0f;
    float af = mm->axle_front, ar = mm->axle_rear;
    const float lx[4] = { tr, tr, -tr, -tr }, lz[4] = { -af, ar, -af, ar };
    float cr = fabsf(cosf(c->roll)), cp = fabsf(cosf(c->pitch));
    float a = c->yaw + c->drift, cs = cosf(a), sn = sinf(a);
    float jolted[4];
    if (!g_contact.ready)
        for (int n = 0; n < 4; n++) g_contact.h[n] = c->z;
    c->rough = 0;
    for (int n = 0; n < 4; n++) {
        float x = lx[n] * cr, z = lz[n] * cp;
        float wx = c->x + x * cs - z * sn, wy = c->y + x * sn + z * cs;
        float h;
        int rough = 0;
        if (track_under(wx, wy, 1, &h)) {         /* over nothing, a wheel keeps its height */
            g_contact.h[n] = h;
            rough = (g_scene->obj[g_under_obj].m->surface == 'e');
            c->grass[n] = rough;                  /* the wheel's own flag (+0x2c): kept while it is over nothing */
            if (rough && !(n & 1)) c->rough = 1;
        }
        jolted[n] = g_contact.h[n];
        if (c->grass[n]) {
            int spot = (int)wx + (int)wy;
            jolted[n] += (float)((spot & 2) ? -(spot & 0xf) : (spot & 0xf));
        }
    }
    /* the same again turned by the heading alone, and with no grass jolts:
       these are what the chase camera follows (+0x4d4, +0x4dc), so the picture
       neither swings with a skid nor shakes on the grass */
    {
        float cs2 = cosf(c->yaw), sn2 = sinf(c->yaw);
        for (int n = 0; n < 4; n++) {
            float x = lx[n] * cr, z = lz[n] * cp, h;
            if (!g_contact.ready) g_contact.hc[n] = c->z;
            if (track_under(c->x + x * cs2 - z * sn2, c->y + x * sn2 + z * cs2, 1, &h))
                g_contact.hc[n] = h;
        }
        g_contact.cam_pitch = atanf((g_contact.hc[2] - g_contact.hc[3]) / (af + ar));
        g_contact.cam_roll  = atanf((g_contact.hc[2] - g_contact.hc[0]) / (2.0f * tr));
    }
    g_contact.ready = 1;
    g_contact.ground = 0.25f * (g_contact.h[0] + g_contact.h[1] + g_contact.h[2] + g_contact.h[3]);
    c->z = 0.25f * (jolted[0] + jolted[1] + jolted[2] + jolted[3]);
    c->pitch = atanf((jolted[2] - jolted[3]) / (af + ar));
    c->roll  = atanf((jolted[2] - jolted[0]) / (2.0f * tr));   /* the port's lean is + to the right */
    if (getenv("OS_GROUNDDUMP")) {
        static int n = 0;
        if (n++ % 5 == 0)
            printf("  ground at %.0f, %.0f  %3.0f km/h  wheels %6.0f %6.0f %6.0f %6.0f  car %7.1f  nose %+5.1f  lean %+5.1f deg%s\n",
                   c->x, c->y, drive_kmh(c), g_contact.h[0], g_contact.h[1], g_contact.h[2], g_contact.h[3], c->z,
                   c->pitch * 57.3f, c->roll * 57.3f, c->rough ? "  GRASS under a front wheel" : "");
    }
}

/* after each physics step: walls, then keep the car on the ground */
/*
 * TIME ATTACK (mode 4; set-up 0x12e14, frame 0x202b0, finish 0x23ef0, the
 * loop round it 0x1fd64; build/decomp/time_attack.c).
 *
 * One lap, alone, from the grid.  The whole run is RECORDED, one 40-byte
 * record a frame (0x231d0): position x, height, z; pitch, heading, roll
 * (floats, radians); the texture-frame word; the reflection picture; the
 * progress round the lap; the race clock.  There are two such recordings
 * (headers of 36 bytes at 0x18e88, the one being written is 0x18ed0): when a
 * run is finished and is the first, or faster than the other recording's
 * total time, it is written to REPLAYS\MAPn.ATK (0x1b964: the header, then
 * the records) and becomes the GHOST -- the next run records into the other
 * one while this one is played back as a second car, by the clock (0x23018;
 * its place run along a curve through four records, 0x22724 -- ghost_curve)
 * and not collided with.  "YOU MADE IT!"
 * if the ghost was beaten (or there was none), else "TRY AGAIN" (0x24174).
 * The grid place alternates with the recording (0x13ded: slot = recording +
 * opponents - 1), so the ghost starts beside you.  RESUME in the menu loads
 * the file's ghost; NEW starts without one.
 *
 * Port: the file has the game's layout, but whether the DOS game reads the
 * port's files (or the other way) is NOT tested; a ghost loaded from a file
 * does not turn its wheels (that is not in the record).
 */
#define GHOST_MAX 30000                          /* ten minutes */
typedef struct { int32_t x, h, y; float pitch, yaw, roll; int32_t phase, env; float progress, time; } ghost_frame;
typedef struct { ghost_frame *f; car_pose *pose; int n, car; float total; } ghost_run;
static ghost_run     g_run[2];
static int           g_run_now, g_have_ghost, g_attack_flip, g_ghost_at;
static scene_object *g_ghost;
static car_pose      g_ghost_pose;
static int           g_attack_track = 1;

static void attack_file(char *file, size_t n) { snprintf(file, n, "REPLAYS/MAP%d%s.ATK", g_attack_track, g_reverse ? "I" : ""); }

static void attack_save(const ghost_run *run)                   /* 0x1b964 */
{
    char file[64];
    attack_file(file, sizeof file);
    FILE *f = fopen(file, "wb");
    if (!f) { fprintf(stderr, "time attack: cannot write %s\n", file); return; }
    int32_t h[9] = { GHOST_MAX, run->n, run->n, 1, g_run_now, 0, run->car, 0, 0 };
    memcpy(&h[7], &run->total, 4);
    fwrite(h, 4, 9, f);
    fwrite(run->f, sizeof(ghost_frame), (size_t)run->n, f);
    fclose(f);
    printf("time attack: %.2f s saved to %s\n", run->total, file);
}

static int attack_load(void)                                    /* 0x1ba3c */
{
    ghost_run *run;
    char file[64];
    attack_file(file, sizeof file);
    FILE *f = fopen(file, "rb");
    if (!f) return 0;
    int32_t h[9];
    int ok = fread(h, 4, 9, f) == 9 && h[2] > 0 && h[2] <= GHOST_MAX;
    /* the file says which of the two recordings it was; the next run uses the other */
    run = &g_run[ok ? h[4] & 1 : 0];
    ok = ok && fread(run->f, sizeof(ghost_frame), (size_t)h[2], f) == (size_t)h[2];
    fclose(f);
    if (!ok) return 0;
    run->n = h[2];
    run->car = h[6];
    memcpy(&run->total, &h[7], 4);
    memset(run->pose, 0, sizeof(car_pose) * (size_t)run->n);
    g_run_now = (h[4] & 1) ^ 1;
    return 1;
}

static void attack_init(int track)
{
    if (g_mode != MODE_ATTACK || !g_pobj || g_ncar >= MAX_CARS) return;
    g_attack_track = track;
    for (int i = 0; i < 2; i++) {
        g_run[i].f = calloc(GHOST_MAX, sizeof(ghost_frame));
        g_run[i].pose = calloc(GHOST_MAX, sizeof(car_pose));
        g_run[i].n = 0;
    }
    g_ghost = &g_car[g_ncar++];
    *g_ghost = *g_pobj;
    g_ghost->pose = &g_ghost_pose;
    g_ghost->z = -1.0e7f;
    g_run_now = 0;
    g_have_ghost = 0;
    if (getenv("OS_GHOST") && attack_load()) {
        g_have_ghost = 1;
        printf("time attack: ghost of %.2f s loaded\n", g_run[g_run_now ^ 1].total);
    }
}

/* at the grid: which recording, which slot, the ghost back to its start */
static void attack_restart(void)
{
    if (g_mode != MODE_ATTACK || !g_ghost) return;
    if (g_attack_flip) { g_run_now ^= 1; g_attack_flip = 0; }
    g_run[g_run_now].n = 0;
    g_run[g_run_now].car = g_spec_now;
    g_ghost_at = 0;
    g_attack_won = 0;
    g_player_spl = g_attack_slot + g_run_now;
    if (g_player_spl >= g_nopp_spl) g_player_spl = g_nopp_spl - 1;
    if (g_player_spl < 0) g_player_spl = 0;
}

/* once a physics step, after the player's car has been placed */
/*
 * 0x22724: the ghost's place between two records P1 and P2 of its recording,
 * with the ones before and after (P0, P3) giving the direction at each end --
 * a cubic curve from P1 to P2 whose two inner points are
 *     C1 = P1 + (P2 - P0) * (|P2 - P1| / 3) / |P2 - P0|
 *     C2 = P2 + (P1 - P3) * (|P2 - P1| / 3) / |P1 - P3|
 * all in whole units, cut not rounded, as the game works it out.
 */
static void ghost_curve(const ghost_frame *p0, const ghost_frame *p1, const ghost_frame *p2,
                        const ghost_frame *p3, float t, int out[3])
{
    const int P0[3] = { p0->x, p0->h, p0->y }, P1[3] = { p1->x, p1->h, p1->y };
    const int P2[3] = { p2->x, p2->h, p2->y }, P3[3] = { p3->x, p3->h, p3->y };
    float d = 0.0f, d02 = 0.0f, d13 = 0.0f;
    for (int k = 0; k < 3; k++) {
        d   += (float)(P2[k] - P1[k]) * (float)(P2[k] - P1[k]);
        d02 += (float)(P2[k] - P0[k]) * (float)(P2[k] - P0[k]);
        d13 += (float)(P1[k] - P3[k]) * (float)(P1[k] - P3[k]);
    }
    d = sqrtf(d) / 3.0f; d02 = sqrtf(d02); d13 = sqrtf(d13);
    for (int k = 0; k < 3; k++) {
        int c1 = P1[k], c2 = P2[k];
        if (d02 != 0.0f) c1 = (int)((float)P1[k] + (float)(P2[k] - P0[k]) * d / d02);
        if (d13 != 0.0f) c2 = (int)((float)P2[k] + (float)(P1[k] - P3[k]) * d / d13);
        out[k] = (int)((float)P1[k] + (float)(c1 * 3 - P1[k] * 3) * t
                       + (float)(c2 * 3 - c1 * 6 + P1[k] * 3) * t * t
                       + (float)(c1 * 3 + P2[k] - c2 * 3 - P1[k]) * t * t * t);
    }
}

static void attack_step(void)
{
    if (g_mode != MODE_ATTACK || !g_ghost) return;
    ghost_run *rec = &g_run[g_run_now], *gh = &g_run[g_run_now ^ 1];
    if (g_race_on && !g_finished && !g_game_over && rec->n < GHOST_MAX) {      /* 0x231d0 */
        ghost_frame *q = &rec->f[rec->n];
        q->x = (int32_t)g_pobj->x; q->y = (int32_t)g_pobj->y; q->h = (int32_t)g_pobj->z;
        q->pitch = g_pobj->pitch; q->yaw = g_pobj->yaw; q->roll = g_pobj->roll;
        q->phase = g_pobj->phase; q->env = g_ppose.env;
        q->progress = g_progress; q->time = g_race_clock;
        rec->pose[rec->n] = g_ppose;
        rec->n++;
    }
    if (!g_have_ghost || gh->n < 2) { g_ghost->z = -1.0e7f; return; }
    while (g_ghost_at < gh->n - 1 && gh->f[g_ghost_at].time < g_race_clock) g_ghost_at++;   /* 0x23018 */
    /* the record before the one the clock has not reached yet is taken whole
       -- place, angles, texture word -- and, away from the two ends of the
       recording, the PLACE alone is then run along a curve through four
       records (0x22724) */
    int i = g_ghost_at > 0 ? g_ghost_at - 1 : 0;
    const ghost_frame *a = &gh->f[i];
    g_ghost->x = (float)a->x; g_ghost->y = (float)a->y; g_ghost->z = (float)a->h;
    g_ghost->yaw = a->yaw; g_ghost->pitch = a->pitch; g_ghost->roll = a->roll;
    if (i > 1 && gh->n - i > 2) {
        float span = gh->f[g_ghost_at].time - a->time;
        if (span != 0.0f) {
            int out[3];
            ghost_curve(&gh->f[i - 1], a, &gh->f[i + 1], &gh->f[i + 2], (g_race_clock - a->time) / span, out);
            g_ghost->x = (float)out[0]; g_ghost->z = (float)out[1]; g_ghost->y = (float)out[2];
        }
    }
    /* 0x20418, time attack: second while the ghost is further on (once its
       recording has run out: if its whole time was the better one) */
    g_player_rank = 1 + (g_ghost_at < gh->n - 1 ? g_progress <= a->progress : gh->total <= g_total_time);
    g_ghost->phase = a->phase;
    g_ghost_pose = gh->pose[i];
    g_ghost_pose.env = a->env;
    if (getenv("OS_MODELOG") && ((int)(g_race_clock * 50.0f)) % 250 == 0)
        printf("ghost %.0f s: %.0f units from the player, record %d of %d\n", g_race_clock,
               hypotf(g_ghost->x - g_pobj->x, g_ghost->y - g_pobj->y), i, gh->n);
    if (gh->car >= 0 && gh->car < g_nspec) {
        int model = atoi(g_spec[gh->car].name + 4);
        if (model >= 0 && model <= 6 && car_mesh_ok[model]) { g_ghost->m = &car_mesh[model]; g_ghost->v = model; }
    }
}

/* 0x23ef0, then 0x1fd64's save */
static void attack_finish(void)
{
    if (g_mode != MODE_ATTACK || !g_ghost) return;
    ghost_run *rec = &g_run[g_run_now];
    g_attack_won = !g_have_ghost || g_total_time < g_run[g_run_now ^ 1].total;
    if (!g_attack_won) return;
    rec->total = g_total_time;
    attack_save(rec);
    g_have_ghost = 1;
    g_attack_flip = 1;
}

static void player_env(const path *r)
{
    car_state *c = &g_pc;
    {
        g_pat = path_track(r, c->x, c->y, g_pat);
    }
    player_walls();
    markers_step(c);

    cars_collide();
    attack_step();

    /* ground: stand the car on its four wheels, the game's way */
    if (g_pobj) player_contact();
    player_sync();
    if (g_pshadow && g_contact.ready && !g_crash.on) g_pshadow->z = g_contact.ground + 4.0f;
}

/* ------------------------------------------------------------------ menu */

/*
 * TRACK AND CAR SELECTION -- a first piece of the menus.
 *
 * The original's menu is one 3D scene (0x27eb4 is its loop, 0x27f9c a frame,
 * 0x28080 the keys, 0x29530 a camera that flies from screen to screen, and
 * the screens' 2D parts are drawn by 0x2df38 / 0x2e08c / 0x2e384).  What is
 * from the game here:
 *   - the backgrounds MENU2L.PCX (cars) and MENU3L.PCX (tracks) of GRAF.DAT,
 *     and their palettes;
 *   - MENUL.FNT / MENUL.SPR: four fonts; sprites 0-3 the arrows, 4-8 and 9-13
 *     the five-step GRIP and ACCELERATION bars;
 *   - the texts: MENU.TXT, CARDAT.TXT (name, gearbox, top speed in km/h,
 *     grip step, acceleration step), TRACKDAT.TXT;
 *   - the car screen's layout (0x2e08c): title at y 2 in font 3; the name
 *     centred at 3 x its height in font 0; the gearbox at 200 - 5 x height;
 *     three lines starting at 210 - 5 x (font 1's height): MAX SPEED and the
 *     number, GRIP and its bar, ACCELERATION and its bar, the labels' column
 *     as wide as the longest label written in 'A's; arrows at (8, 100) and
 *     (238, 100), missing at the ends of the list;
 *   - the track screen's (0x2e384): title, the name at 200 - 2 x height in
 *     font 2, the arrows;
 *   - the models: AAUTOnn.3DX and AMAPn.3DX with MENU.MLB.
 * The 3D scene these are drawn over -- the cubes of the pages, the rows of
 * cars and tracks, the camera -- is ms_* further down (0x29530, 0x2af30...).
 */
static int pcx_load(const char *file, uint8_t *pix, uint32_t pal[256])
{
    FILE *f = fopen(res_name(file), "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc((size_t)n);
    if (n < 128 + 769 || fread(d, 1, (size_t)n, f) != (size_t)n) { free(d); fclose(f); return 0; }
    fclose(f);
    int w = (d[8] | d[9] << 8) - (d[4] | d[5] << 8) + 1, h = (d[10] | d[11] << 8) - (d[6] | d[7] << 8) + 1;
    int bpl = d[66] | d[67] << 8;
    long p = 128;
    for (int y = 0; y < h && y < ART_H; y++) {
        int x = 0;
        while (x < bpl && p < n - 769) {
            int c = d[p++], run = 1;
            if ((c & 0xc0) == 0xc0) { run = c & 0x3f; c = d[p++]; }
            while (run-- > 0 && x < bpl) { if (x < ART_W && x < w) pix[y * ART_W + x] = (uint8_t)c; x++; }
        }
    }
    const uint8_t *q = d + n - 768;
    for (int i = 0; i < 256; i++)
        pal[i] = 0xFF000000u | ((uint32_t)q[3 * i] << 16) | ((uint32_t)q[3 * i + 1] << 8) | q[3 * i + 2];
    free(d);
    return 1;
}

#define MENU_CARS 16
typedef struct { char name[24]; int gearbox, kmh, grip, accel; } menu_car;
static menu_car g_menu_car[MENU_CARS];
static int      g_nmenu_car;
static char     g_menu_track[12][24];             /* 7..12: the same six, driven the other way */
static char     g_menu_text[68][40];
/* 0x287a8 / 0x28726: the last track and the last car that can be chosen */
static int      g_track_max = 5, g_car_max = 12, g_menu_tick;

static int text_lines(const char *file, char out[][40], int max)
{
    FILE *f = fopen(file, "r");
    int n = 0;
    if (!f) return 0;
    char line[128];
    while (n < max && fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (line[0] == '*') continue;                  /* a comment in the text files */
        snprintf(out[n++], 40, "%.39s", line);
    }
    fclose(f);
    return n;
}

static void menu_frame(int screen, int pick, float turn, const uint8_t *bg, mesh *model, int livery)
{
    if (bg) pic_put(bg, 1);
    g_vh = FB_H; g_cy = (float)FB_H * 0.5f;
    if (model && model->nface) {
        static scene_object o;
        memset(&o, 0, sizeof o);
        o.m = model;
        o.yaw = turn;
        o.radius = 20000.0f;
        o.phase = (livery ? 0x8000 : 0) | 0x6000;
        /* a car is drawn as a car here too: without a pose its wheels got none
           of the depth nudges of draw_object and the body cut into them */
        static car_pose still;
        if (screen == 1) o.pose = &still;
        float reach = 0.0f;
        for (int i = 0; i < model->nvert; i++) {
            float d = sqrtf(model->vert[i].x * model->vert[i].x + model->vert[i].z * model->vert[i].z);
            if (d > reach) reach = d;
        }
        float back = screen == 0 ? 1.45f : 2.2f;       /* the track models are flat and wide */
        if (getenv("OS_MENU_ZOOM")) back /= (float)atof(getenv("OS_MENU_ZOOM"));   /* closer, for checking */
        camera cam = { -reach * back, 0.0f, reach * back * 0.34f, 0.0f, -0.30f, 0.0f };
        view_setup(&cam);
        for (int pass = 0; pass < 2; pass++) draw_object(&o, 0, pass, &cam);
    }
    char buf[64];
    if (screen == 0) {                                                  /* 0x2e384 */
        hud_text_centred(2, g_menu_text[45], 3, 2);
        int h = g_font[2].height;
        hud_text_centred(200 - 2 * h, g_menu_track[pick], 2, 2);
        /* 0x2e424: blinks until the track screen has been left once */
        if (g_save.seen < g_save.level && (g_menu_tick & 0x3f) < 0x20)
            hud_text_centred(0x20, g_menu_text[53], 1, 2);
        if (pick > 0) hud_sprite_draw(8, 100, 0);
        if (pick < g_track_max) hud_sprite_draw(0xee, 100, 1);
    } else {                                                            /* 0x2e08c */
        const menu_car *c = &g_menu_car[pick];
        hud_text_centred(2, g_menu_text[44], 3, 2);
        int h0 = g_font[0].height, h1 = g_font[1].height;
        hud_text_centred(h0 * 3, c->name, 0, 2);
        hud_text_centred(200 - 5 * h0, g_menu_text[37 + (c->gearbox ? 1 : 0)], 0, 2);
        size_t longest = 0;
        for (int k = 0; k < 3; k++) if (strlen(g_menu_text[39 + k]) > longest) longest = strlen(g_menu_text[39 + k]);
        char as[40];
        memset(as, 'A', longest); as[longest] = 0;
        int lw = hud_text(0, 0, as, 1, 2, 0);
        int x = (320 - (lw + 0x40)) / 2, y = 210 - 5 * h1;
        hud_text(x, y, g_menu_text[39], 1, 2, 1);
        snprintf(buf, sizeof buf, "%d", c->kmh);
        hud_text(x + lw, y, buf, 1, 2, 1);
        y += h1;
        hud_text(x, y, g_menu_text[40], 1, 2, 1);
        hud_sprite_draw(x + lw, y + 1, c->grip + 4);
        y += h1;
        hud_text(x, y, g_menu_text[41], 1, 2, 1);
        hud_sprite_draw(x + lw, y + 1, c->accel + 9);
        if (pick > 0) hud_sprite_draw(8, 100, 0);
        if (pick < g_car_max) hud_sprite_draw(0xee, 100, 1);
    }
}

/*
 * THE MENU'S PAGES, from the game's own tables (pointers at 0x2de4, items of
 * 16 bytes: line of MENU.TXT, id, value, highest value):
 *   page 0  NORMAL GAME / CHAMPIONSHIP / SPECIAL MODES / NETWORK GAME / OPTIONS
 *   page 1  detail (LOW MID HI, starts HI) / difficulty (ROOKIE NORMAL PRO,
 *           starts NORMAL) / LAP NUMBER (3 5 10 25, starts 3) / SPEECH (starts
 *           ON) / MUSIC (starts ON) / VIEW RECORDS
 *   page 3, 5  NEW / RESUME      page 4  TIME ATTACK / CONE CARNAGE / SLALOM
 *   page 6  the six tracks (records)
 * An option's line is its first line + its value.  In the game the words are
 * 3D objects in the menu scene; here they are a plain list in the menu's own
 * font, and only what the port can do is offered as working: NORMAL GAME, and
 * under OPTIONS the lap number and the speech.  The rest is listed, dimmed.
 */
typedef struct { int line, works, *value, max; } menu_item;
static int g_opt_detail = 2, g_opt_res = 0, g_opt_diff = 1, g_opt_laps = 0, g_opt_speech = 1, g_opt_music = 1;
static const menu_item g_page_main[] = {
    { 0, 1, NULL, 0 }, { 1, 1, NULL, 0 }, { 2, 1, NULL, 0 }, { 3, 0, NULL, 0 }, { 4, 1, NULL, 0 },
};
static const menu_item g_page_special[] = {      /* the game's page 4 */
    { 28, 1, NULL, 0 }, { 29, 1, NULL, 0 }, { 30, 1, NULL, 0 },
};
static const menu_item g_page_attack[] = {       /* the game's page 3: NEW / RESUME */
    { 24, 1, NULL, 0 }, { 25, 1, NULL, 0 },
};
static menu_item g_page_options[] = {            /* the difficulty's highest value is 0x2081c */
    /* the game's first line is its DETAIL option (how far it draws: see
       g_far_z); the port adds a fourth value, ULTRA, and a line of its own,
       the RESOLUTION (texts 60..65 are the port's: see menu_texts) */
#ifdef MISTER_HYBRID
    { 62, 1, &g_opt_detail, 2 },                 /* MISTER: the game's three levels only, and the picture's size is the core's own option */
#define OPT_DIFF 1
#define OPT_LINES 6
#else
    { 62, 1, &g_opt_detail, 3 }, { 60, 1, &g_opt_res, 1 },
#define OPT_DIFF 2
#define OPT_LINES 7
#endif
    { 8, 1, &g_opt_diff, 2 }, { 13, 1, &g_opt_laps, 3 },
    { 17, 1, &g_opt_speech, 1 }, { 19, 1, &g_opt_music, 1 }, { 21, 1, NULL, 0 },
};
static const menu_item g_page_records[] = {      /* the game's page 6: a track's records */
    { 31, 1, NULL, 0 }, { 32, 1, NULL, 0 }, { 33, 1, NULL, 0 }, { 34, 1, NULL, 0 }, { 35, 1, NULL, 0 }, { 36, 1, NULL, 0 },
};

static void champ_darken(int x0, int y0, int w, int h);

/* 0x2d348: a track's records, over the track's loading picture (0x2c897: "load%d%c.pcx") */
static void records_draw(const uint8_t *bg, int track, int sel)
{
    const track_records *r = &g_save.t[track];
    char buf[64];
    pic_put(bg, 0);
    champ_darken(0x30, 0x24, 0xe2, 0xa0);
    champ_darken(0x30, 0x24, 0xe2, 0xa0);
    hud_text_centred(2, g_menu_text[31 + track], 3, 0);
    hud_text_centred(0x28, g_menu_text[42], 0, 0);
    hud_text_centred(0x78, g_menu_text[43], 0, 0);
    for (int i = 0; i < 5; i++) {
        float t = r->total[i];
        snprintf(buf, sizeof buf, "\x01%d. %3s %02d.%02d.%02d %c", i + 1, r->total_name[i],
                 (int)t / 60, (int)t % 60, (int)((t - (float)(int)t) * 100.0f), r->rpl[i] ? 'R' : ' ');
        hud_text_centred(0x3c + 10 * i, buf, 1, sel == i);
        t = r->lap[i];
        snprintf(buf, sizeof buf, "\x01%d. %3s %02d.%02d.%02d  ", i + 1, r->lap_name[i],
                 (int)t / 60, (int)t % 60, (int)((t - (float)(int)t) * 100.0f));
        hud_text_centred(0x8c + 10 * i, buf, 1, 0);
    }
}

/* 0x2e4f8: ENTER YOUR NAME, the three letters and a dash under the one being set */
static void name_draw(const uint8_t *bg, int pos)
{
    char buf[16];
    if (bg) pic_put(bg, 0);
    hud_text_centred(2, g_menu_text[46], 3, 2);
    snprintf(buf, sizeof buf, "\x01%s", g_save.name);
    hud_text(0x83, 0x4c, buf, 2, 2, 1);
    hud_text(0x83 + pos * 0x15, 0x58, "\x01-", 2, 2, 1);
}

/*
 * THE CHAMPIONSHIP (game mode 2; build/decomp/championship.c).
 *
 * A table of 53 STAGES at 0x22b8 (44 bytes each: kind, a number, a number,
 * text, next stage, stage on failure, picture name, .., race index, font)
 * drives it; the current stage is 0x203d8, the one to come back to 0x203dc.
 * The table is perfectly regular, so champ_stage_at() rebuilds it:
 *   four LEAGUES (rookie, amateur, pro, bullet) of 13 stages each --
 *     a text screen ("WELCOME TO ..." before race 1, "PREPARE FOR THE NEXT
 *     RACE!" before the others), ENTER starts the race: tracks 1..6 in
 *     order, THREE laps, the opponents' file by league (A, B, C, and E for
 *     the bullet league, where they all drive the Bullet) -- 0x28ef6;
 *     then the STANDINGS (0x2ce58 draws them, 0x292c8 decides);
 *     and after the sixth, "CONGRATULATIONS!".
 *   Points (0x21ec): 9 / 7 / 5 for first / second / third, nothing below.
 *   To go on you must finish in the first THREE (in the bullet league:
 *   WIN), not run out of time, and after the sixth race have the league's
 *   total: 42 / 46 / 50 / 54.  The game also fails you as soon as the total
 *   can no longer be reached with wins in the races left (0x29acc).
 *   Failing shows "LOSER!!" (stage 52); RESUME goes back to the screen
 *   before the race that was lost (0x28c9e), NEW starts again.
 * The game keeps all of it in CONFIG\CHOICE.DAT with its options and
 * records; the port keeps its own small file, openscreamer.champ.
 */
typedef struct { int kind, league, race, text, next, pic, font, need; } champ_stage;
typedef struct { int stage, prev, car, failed; int pos[6], pts[6]; float time[6]; } champ_state;
static champ_state g_ch;
static char g_champ_text[96][40];
static int  g_champ_block[32], g_champ_blocks;

static champ_stage champ_stage_at(int s)
{
    champ_stage st = { 2, 0, 0, 19, 0, 4, 2, 0 };                 /* 52: lost */
    if (s == 51) { st.text = 18; st.pic = 6; return st; }
    if (s < 0 || s >= 52) return st;
    int L = s / 13, i = s % 13;
    st.league = L; st.race = i / 2; st.need = 42 + 4 * L; st.next = s + 1; st.font = 3;
    if (i == 12) {
        static const int pic[3] = { 0, 1, 7 };
        st.kind = 1; st.text = 14 + L; st.pic = pic[L % 3]; st.font = 2;
    } else if (!(i & 1)) {
        st.kind = 0; st.text = i == 0 ? 10 + L : 17; st.pic = i == 0 ? L : 5;
    } else {
        st.kind = 3; st.text = 6 + L; st.pic = L;
        if (i == 11 && L == 3) st.next = 51;
    }
    return st;
}

static void champ_save(void)
{
    FILE *f = fopen("openscreamer.champ", "w");
    if (!f) return;
    fprintf(f, "%d %d %d %d\n", g_ch.stage, g_ch.prev, g_ch.car, g_ch.failed);
    for (int i = 0; i < 6; i++) fprintf(f, "%d %d %f\n", g_ch.pos[i], g_ch.pts[i], g_ch.time[i]);
    fclose(f);
}

static void champ_new(void)
{
    memset(&g_ch, 0, sizeof g_ch);
    for (int i = 0; i < 6; i++) g_ch.pos[i] = -1;
}

static int champ_load(void)
{
    champ_new();
    FILE *f = fopen("openscreamer.champ", "r");
    if (!f) return 0;
    int ok = fscanf(f, "%d %d %d %d", &g_ch.stage, &g_ch.prev, &g_ch.car, &g_ch.failed) == 4;
    for (int i = 0; ok && i < 6; i++) ok = fscanf(f, "%d %d %f", &g_ch.pos[i], &g_ch.pts[i], &g_ch.time[i]) == 3;
    fclose(f);
    if (!ok || g_ch.stage < 0 || g_ch.stage > 52 || g_ch.prev < 0 || g_ch.prev > 52) { champ_new(); return 0; }
    return 1;
}

/* 0x29acc: the race is over -- position 1.., its total time, out of time or not */
static void champ_result(int pos, float time, int timeover)
{
    champ_stage st = champ_stage_at(g_ch.stage);
    if (st.kind != 0) return;
    static const int points[4] = { 0, 9, 7, 5 };
    g_ch.failed = timeover ? 1 : 0;
    g_ch.pos[st.race] = pos;
    g_ch.time[st.race] = time;
    g_ch.pts[st.race] = (pos >= 1 && pos <= 3) ? points[pos] : 0;
    int reach = 0;
    for (int i = 0; i <= st.race; i++) reach += g_ch.pts[i];
    reach += 9 * (5 - st.race);
    g_ch.stage++;
    if (reach < champ_stage_at(g_ch.stage).need) g_ch.failed = 2;
    champ_save();
}

static int champ_total(void)
{
    int t = 0;
    for (int i = 0; i < 6; i++)
        if (g_ch.pos[i] != -1 && !(g_ch.failed == 1 && champ_stage_at(g_ch.stage).race == i)) t += g_ch.pts[i];
    return t;
}

/* ENTER on a stage.  Returns 1 = start the stage's race, 2 = back to the main menu, 0 = next screen */
static int champ_enter(void)
{
    champ_stage st = champ_stage_at(g_ch.stage);
    int ret = 0;
    if (st.kind == 0) return 1;
    if (st.kind == 3) {                                           /* 0x292c8 */
        int pos = g_ch.pos[st.race], fail = 0;
        if (pos > 3 || pos < 1 || g_ch.failed) fail = 1;
        else if (st.league < 3 || pos == 1) {
            if (st.race != 5) { g_ch.prev = g_ch.stage; g_ch.stage = st.next; }
            else if (st.need <= champ_total()) {
                g_ch.stage = st.next; g_ch.prev = g_ch.stage;
                for (int i = 0; i < 6; i++) g_ch.pos[i] = -1;
            } else fail = 1;
        } else fail = 1;
        if (fail) { g_ch.prev = g_ch.stage - 1; g_ch.stage = 52; }
    } else if (st.kind == 1) {                                    /* 0x290a5: the league is won */
        if (g_save.level < st.league + 1) { g_save.level = st.league + 1; save_write(); }   /* one more track */
        g_ch.prev = g_ch.stage; g_ch.stage = st.next;
    } else {
        if (g_ch.stage == 51) {                                   /* 0x29141: everything won */
            g_save.bullet = 1; g_save.level = 9; save_write();
        }
        ret = 2;
    }
    champ_save();
    return ret;
}

static void champ_darken(int x0, int y0, int w, int h)           /* 0x25cc8: to 75% */
{
    x0 *= g_s; y0 = y0 * SY + g_oy; w *= g_s; h *= SY;
    for (int y = y0; y < y0 + h && y < FB_H; y++)
        for (int x = x0; x < x0 + w && x < FB_W; x++) {
            uint32_t c = fb[y * FB_W + x];
            uint32_t r = (c >> 16 & 255) * 3 / 4, g = (c >> 8 & 255) * 3 / 4, b = (c & 255) * 3 / 4;
            fb[y * FB_W + x] = 0xff000000u | r << 16 | g << 8 | b;
        }
}

static const char *champ_line(int block, int line)
{
    if (block < 0 || block >= g_champ_blocks) return "";
    int at = g_champ_block[block] + line;
    return at < g_champ_block[block + 1] - 1 ? g_champ_text[at] : "";
}

static void champ_draw(const uint8_t *bg, int tick)
{
    champ_stage st = champ_stage_at(g_ch.stage);
    char buf[64];
    pic_put(bg, 0);
    if (st.kind == 3) {                                           /* 0x2ce58 */
        champ_darken(0x10, 0x24, 0x120, 0x88);
        champ_darken(0x10, 0x24, 0x120, 0x88);
        hud_text_centred(2, champ_line(st.text, 0), 2, 2);
        hud_text(0x1c, 0x30, champ_line(1, 0), 0, 0xf, 1);
        hud_text(0x58, 0x30, champ_line(0, 0), 0, 0xf, 1);
        hud_text(0xaa, 0x30, champ_line(2, 0), 0, 0xf, 1);
        hud_text(0xf6, 0x30, champ_line(3, 0), 0, 0xf, 1);
        int y = 0x3e;
        for (int i = 0; i < 6; i++, y += 0xe) {
            snprintf(buf, sizeof buf, "\x01%d", i + 1);
            hud_text(0x26, y, buf, 0, 0xf, 1);
            if (g_ch.pos[i] == -1) {
                hud_text(0x58, y, "\x01-", 0, 0xf, 1);
                hud_text(0x8c, y, "\x01--.--.--", 0, 0xf, 1);
                hud_text(0x10a, y, "\x01-", 0, 0xf, 1);
            } else if (g_ch.failed == 1 && st.race == i) {
                hud_text(0x58, y, "\x01" "10", 0, 0xf, 1);
                hud_text(0x8c, y, "\x01" "00.00.00", 0, 0xf, 1);
                hud_text(0x10a, y, "\x01" "0", 0, 0xf, 1);
            } else {
                float t = g_ch.time[i];
                snprintf(buf, sizeof buf, "\x01%d", g_ch.pos[i]);
                hud_text(0x58, y, buf, 0, 0xf, 1);
                snprintf(buf, sizeof buf, "\x01%02d.%02d.%02d", (int)t / 60, (int)t % 60, (int)((t - (float)(int)t) * 100.0f));
                hud_text(0x8c, y, buf, 0, 0xf, 1);
                snprintf(buf, sizeof buf, "\x01%d", g_ch.pts[i]);
                hud_text(0x10a, y, buf, 0, 0xf, 1);
            }
        }
        y += 4;
        snprintf(buf, sizeof buf, "%s:\x01%02d", champ_line(5, 0), champ_total());
        hud_text(0x114 - hud_text(0, 0, buf, 0, 0xf, 0), y, buf, 0, 0xf, 1);
        y += 0x18;
        snprintf(buf, sizeof buf, "%s:%d", champ_line(4, 0), st.need);
        hud_text((320 - hud_text(0, 0, buf, 0, 0xf, 0)) / 2, y, buf, 0, 0xf, 1);
    } else {
        int y = 0x20, n = g_champ_block[st.text + 1] - 1 - g_champ_block[st.text];
        for (int i = 0; i < n; i++, y += g_font[st.font].height)
            hud_text_centred(y, champ_line(st.text, i), st.font, 2);
    }
    if ((tick & 0x7f) < 0x48) hud_text_centred(0xbe, g_menu_text[56], 0, 2);
}

static void champ_texts(void)
{
    int n = text_lines("build/dec/LOCAL/CHAMP.TXT", g_champ_text, 96);
    g_champ_blocks = 0;
    g_champ_block[0] = 0;
    for (int i = 0; i < n && g_champ_blocks < 30; i++)
        if (g_champ_text[i][0] == '#') g_champ_block[++g_champ_blocks] = i + 1;
}

/*
 * THE MENU'S 3D SCENE (build/decomp/menu_scene.c).
 *
 * The original's menu is one 3D scene drawn over a still picture:
 *  - every letter of a menu page is a CUBE (ALETTER.3DX, 516 units; 80 of
 *    them, records of 100 bytes at 0x1db74).  The cube's texture has one
 *    frame per character (record +0x20 = 0x35e * (character - 32) + 0x226),
 *    its front face shows the frame's top half and its back the bottom half:
 *    the chosen line shows fronts, the others are turned half round;
 *  - the cars stand in a row 4000 apart, the track models 40000 apart, each
 *    turning on the spot, and the camera slides along the row;
 *  - the name screen is a ring of 26 cubes, the camera walking round it;
 *  - one camera (0x29530) eases from a start to a target by a factor that
 *    runs from 1 to 0 (0x20378, speed 0x2037c; raised to the 4th power when
 *    0x1d23c is set); screens change through a fade of the palette (0x29738).
 * Engine space here: x across (+x is screen LEFT), height up, z towards the
 * viewer; the camera looks along -z.  Port: x = x, y = z, z = height.
 */
typedef struct {
    float pitch, roll, head0, head1, p0[3], p1[3], f, speed;
    int   pow4;
    float now[3], head;
} ms_cam;
typedef struct {
    int   used, line, code, unsel, ring, bounce;
    float a4, head, s[3], t[3], ax, ay, p, rate, delay, top;
    float pos[3];
} ms_letter;

static ms_cam    g_mc;
#define MS_LETTERS 96      /* the game has 80 (0x1db74); the port's seven-line OPTIONS page needs more */
static ms_letter g_ml[MS_LETTERS];
static mesh      g_ms_cube, g_ms_car[7], g_ms_map[6];
static int       g_ms_car_ok[7], g_ms_map_ok[6], g_ms_cube_ok;
static int       g_ms_style, g_ms_shown = -1, g_ms_leaving, g_ms_ready;
static float     g_ms_wave, g_ms_logo[2], g_ms_turn_car = 1.5707964f, g_ms_turn_map = 1.5707964f;

static void ms_cam_set(float x, float h, float z, float pitch, float head)
{
    memset(&g_mc, 0, sizeof g_mc);
    g_mc.pitch = pitch; g_mc.head0 = g_mc.head1 = g_mc.head = head;
    g_mc.p0[0] = g_mc.p1[0] = g_mc.now[0] = x;
    g_mc.p0[1] = g_mc.p1[1] = g_mc.now[1] = h;
    g_mc.p0[2] = g_mc.p1[2] = g_mc.now[2] = z;
    g_mc.speed = 0.95f; g_mc.pow4 = 1;
}

/* a new target: the start is where the camera is now */
static void ms_cam_go(float x, float h, float z, float head)
{
    memcpy(g_mc.p0, g_mc.now, sizeof g_mc.p0);
    g_mc.head0 = g_mc.head;
    g_mc.p1[0] = x; g_mc.p1[1] = h; g_mc.p1[2] = z; g_mc.head1 = head;
    g_mc.f = 1.0f;
}

static void ms_cam_step(float dt)                                 /* 0x29530 */
{
    float f = g_mc.pow4 ? powf(g_mc.f, 4.0f) : g_mc.f;
    float d = g_mc.head0 - g_mc.head1;
    while (d > (float)M_PI) d -= 2.0f * (float)M_PI;
    while (d < -(float)M_PI) d += 2.0f * (float)M_PI;
    g_mc.head = g_mc.head1 + d * f;
    for (int i = 0; i < 3; i++) g_mc.now[i] = g_mc.p1[i] + (g_mc.p0[i] - g_mc.p1[i]) * f;
    if (g_mc.f != 0.0f) {
        g_mc.f -= dt * g_mc.speed;
        if (g_mc.f < 0.0f) {
            memcpy(g_mc.p0, g_mc.p1, sizeof g_mc.p0);
            g_mc.head0 = g_mc.head1; g_mc.f = 0.0f;
        }
    }
}

static camera ms_cam_port(void)
{
    camera c = { g_mc.now[0], g_mc.now[2], g_mc.now[1],
                 -(float)M_PI / 2.0f + g_mc.head, g_mc.pitch, g_mc.roll };
    return c;
}

static void ms_draw(const mesh *m, float x, float h, float z, float pitch, float head, float roll,
                    int phase, const car_pose *pose, const camera *cam)
{
    scene_object o;
    memset(&o, 0, sizeof o);
    o.m = m; o.x = x; o.y = z; o.z = h;
    o.yaw = head; o.pitch = -pitch; o.roll = roll;     /* the port's tilt is nose-up positive */
    o.phase = phase; o.pose = pose; o.radius = 60000.0f;
    for (int pass = 0; pass < 2; pass++) draw_object(&o, 0, pass, cam);
}

static void ms_begin(const uint8_t *bg)
{
    pic_put(bg, 1);
    g_vh = FB_H; g_cy = (float)FB_H * 0.5f;
}

static void ms_load(const car_spec *specs, int nspec)
{
    if (g_ms_ready) return;
    g_ms_ready = 1;
    g_ms_cube_ok = mesh_load(&g_ms_cube, "build/dec/MENU/ALETTER.3DX") == 0;
    for (int i = 0; i < 7; i++) {
        char file[64];
        snprintf(file, sizeof file, "build/dec/MENU/AAUTO%02d.3DX", i);
        g_ms_car_ok[i] = mesh_load(&g_ms_car[i], file) == 0;
    }
    for (int i = 0; i < 6; i++) {
        char file[64];
        snprintf(file, sizeof file, "build/dec/MENU/AMAP%d.3DX", i + 1);
        g_ms_map_ok[i] = mesh_load(&g_ms_map[i], file) == 0;
    }
    (void)specs; (void)nspec;
}

/* ---- the letters of a page: 0x2af30 lays a line out ---- */
static const int g_ms_fly[8][5] = {              /* 0x2218: where a page's letters fly in from, and their swing */
    { 200, 200, -4500, 1000, 1000 }, { 3000, -3000, 1500, 200, 800 }, { 1000, 1000, -8000, 3000, 3000 },
    { 3000, 0, 5000, 0, 0 }, { 2000, 50, 4500, 0, 0 }, { 0, 0, -10500, 500, 800 },
    { -2000, 2000, 3500, 0, 0 }, { 50, 5000, 4500, 3000, 3000 },
};

static void ms_page_build(const menu_item *items, int n, int sel, int fly)      /* 0x29838 */
{
    memset(g_ml, 0, sizeof g_ml);
    const int *st = g_ms_fly[g_ms_style];
    float sx = (float)st[0], sy = (float)st[1];
    int row = 8 - n, at = 0;
    for (int i = 0; i < n; i++, row += 2) {
        const char *text = g_menu_text[items[i].line + (items[i].value ? *items[i].value : 0)];
        int len = (int)strlen(text);
        int x = 0x1200 - (((len - 1) * -0x280 + 0x2400) >> 1), y = row * -0x180 + 0x820;
        /* the game's pages have six lines at most (rows 2..12); the port's
           OPTIONS page has seven, which are set a little closer to fill the
           same height */
        if (n > 6) y = (int)((2.0f + (float)i * 10.0f / (float)(n - 1)) * -384.0f) + 0x820;
        for (int k = 0; k < len && at < MS_LETTERS; k++) {
            unsigned char ch = (unsigned char)text[k];
            if (ch < 0x20 || ch > 0x7f) break;
            ms_letter *l = &g_ml[at++];
            l->used = 1; l->line = i; l->code = ch - 0x20; l->unsel = (i != sel);
            l->t[0] = (float)x; l->t[1] = (float)y; l->t[2] = 0.0f;
            l->s[0] = sx != 0.0f ? (float)(int)sx : (float)x;
            l->s[1] = sy != 0.0f ? (float)(int)sy : (float)y;
            l->s[2] = (float)st[2];
            l->a4 = l->unsel ? (float)M_PI : 0.0f;
            l->ax = (float)st[3]; l->ay = (float)st[4];
            l->p = 1.0f; l->rate = 1.0f;
            x -= 0x280;
            float c = cosf(0.09f), s = sinf(0.09f), ox = sx;
            sx = (float)(int)(ox * c + sy * s);
            sy = (float)(int)(sy * c - ox * s);
            if (!fly) {                          /* 0x2b380: a page turned to is simply there */
                l->p = 0.0f;
                if (!l->unsel) l->a4 = (float)M_PI / 2.0f;
            }
        }
    }
    g_ms_style = (g_ms_style + 1) % 8;
}

/* 0x2b8b8: one frame of the page's letters.  Returns 1 when a page that is
   being left has turned far enough for the next one to take its place. */
static int ms_page_step(float dt, int sel)
{
    int turned = 0;
    g_ms_wave += dt * 9.424778f;
    if (g_ms_wave > 25.132742f) g_ms_wave = 0.0f;
    for (int i = 0; i < MS_LETTERS; i++) {
        ms_letter *l = &g_ml[i];
        if (!l->used) continue;
        l->unsel = (l->line != sel);                                      /* 0x2b1f4 */
        float e = 1.0f - cosf(l->p * 1.5707964f), w = l->p * 6.2831855f;
        l->pos[0] = cosf(w) * l->ax * l->p + l->t[0] + e * (l->s[0] - l->t[0]);
        l->pos[1] = sinf(w) * l->ay * l->p + l->t[1] + e * (l->s[1] - l->t[1]);
        l->pos[2] = l->t[2] + e * (l->s[2] - l->t[2]);
        float fx = l->unsel ? (l->pos[0] + 4608.0f) * 0.0001085069f : 0.5f;
        float fy = (l->pos[1] + 2080.0f) * 0.0002403846f;
        l->pos[2] += sinf(fx * 6.2831855f + fy * 3.1415927f + g_ms_wave * (l->unsel ? 1.0f : 0.25f)) * 150.0f;
        if (l->p != 0.0f) { l->p -= dt * l->rate; if (l->p < 0.0f) l->p = 0.0f; }
        if (!l->unsel && !g_ms_leaving) {
            if (l->a4 != 0.0f) { l->a4 -= dt * 12.566371f; if (l->a4 < 0.0f) l->a4 = 0.0f; }
        } else if (l->a4 != (float)M_PI) {
            if (g_ms_leaving && l->a4 > 1.5707964f) turned = 1;
            l->a4 += dt * 12.566371f;
            if (l->a4 > (float)M_PI) l->a4 = (float)M_PI;
        }
    }
    return turned;
}

/* ---- the alphabet ring of the name screen: 0x2c320 sets it up ---- */
static void ms_ring_build(void)
{
    memset(g_ml, 0, sizeof g_ml);
    for (int k = 0; k < 26; k++) {
        ms_letter *l = &g_ml[k];
        float a = (float)k * -0.24166098f;
        l->used = 1; l->ring = 1; l->line = k; l->code = k + 0x21; l->unsel = 1;
        l->pos[0] = sinf(a) * 3500.0f; l->pos[2] = cosf(a) * 3500.0f;
        l->t[1] = 160.0f;                                    /* where it comes to rest */
        l->top = 16.0f * 160.0f + (float)((k & 1) * 500);    /* where it drops from */
        l->delay = (float)k * 0.04f;
        l->a4 = 2.1991148f; l->head = (float)k * 0.24166098f;
        l->p = 0.0f; l->bounce = 1;
    }
}

static void ms_ring_step(float dt, int letter)                    /* 0x2cd74, 0x2b54c */
{
    for (int k = 0; k < 26; k++) {
        ms_letter *l = &g_ml[k];
        l->unsel = (k != letter);
        l->delay -= dt;
        if (l->delay >= 0.0f) continue;
        l->delay = 0.0f;
        l->pos[1] = l->top + l->p * l->p * (l->t[1] - l->top);
        if (l->bounce > 0) {                     /* falling */
            l->p += dt * 2.5f;
            if (l->p > 1.0f) {
                l->p = 1.0f; l->bounce = 0;
                l->top = (float)(int)(l->top * 0.4f);
                if (l->top - l->t[1] < 0.0f) { l->bounce = -1; l->top = l->t[1]; }
            }
        } else if (l->bounce == 0) {             /* up again, less high */
            l->p -= dt * 2.0f;
            if (l->p < 0.0f) { l->p = 0.0f; l->bounce = 1; }
        }
        if (!l->unsel) {
            if (l->a4 != -0.6283185f) { l->a4 -= dt * 12.566371f; if (l->a4 < -0.6283185f) l->a4 = -0.6283185f; }
        } else if (l->a4 != 2.1991148f) {
            l->a4 += dt * 12.566371f;
            if (l->a4 > 2.1991148f) l->a4 = 2.1991148f;
        }
    }
}

static void ms_letters_draw(const camera *cam)
{
    if (!g_ms_cube_ok) return;
    for (int i = 0; i < MS_LETTERS; i++) {
        const ms_letter *l = &g_ml[i];
        if (!l->used || (l->ring && l->delay > 0.0f)) continue;
        ms_draw(&g_ms_cube, l->pos[0], l->pos[1], l->pos[2], l->a4, l->head, 0.0f,
                0x35e * l->code + 0x226, NULL, cam);
    }
}

/* the two SCREAMER logos sliding in (0x2df38: sprites 15 and 14) */
static void ms_logos(float dt)
{
    if (g_nspr <= 15) return;
    float to0 = (float)((320 - g_spr[15].w / g_s) / 2), to1 = (float)((320 - g_spr[14].w / g_s) / 2);
    if (g_ms_logo[0] < to0) g_ms_logo[0] += dt * 350.0f;
    if (g_ms_logo[0] > to0) g_ms_logo[0] = to0;
    if (g_ms_logo[1] < to1) g_ms_logo[1] += dt * 700.0f;
    if (g_ms_logo[1] > to1) g_ms_logo[1] = to1;
    hud_sprite_draw((int)(g_ms_logo[0] + 4.0f), 0, 15);
    hud_sprite_draw((int)g_ms_logo[1], 0x10, 14);
}

/* entering a screen: its camera, as the game's set-up routines leave it */
static void ms_enter(int scr, int pick_car, int pick_track, int letter)
{
    if (scr == 0) {                              /* 0x29ea0 */
        ms_cam_set(0.0f, 0.0f, 4100.0f, 0.0f, 0.0f);
        g_mc.speed = 0.6f;
        g_ms_shown = -1; g_ms_leaving = 0;
        if (g_nspr > 15) {
            g_ms_logo[0] = (float)((320 - g_spr[15].w / g_s) / 2 - 400);
            g_ms_logo[1] = (float)((320 - g_spr[14].w / g_s) / 2 - 800);
        }
    } else if (scr == 2) {                       /* 0x2bdcc */
        ms_cam_set((float)pick_car * -4000.0f, 500.0f, 4100.0f, -0.3926991f, 0.0f);
    } else if (scr == 3) {                       /* 0x2c078 */
        ms_cam_set((float)pick_track * -40000.0f, 4800.0f, 13000.0f, -0.6283185f, 0.0f);
    } else if (scr == 4) {                       /* 0x2c320 */
        float a = (float)letter * -0.24166098f;
        ms_cam_set(sinf(a) * 4500.0f, 1800.0f, cosf(a) * 4500.0f, -0.6283185f, (float)letter * 0.24166098f);
        g_mc.speed = 8.0f; g_mc.pow4 = 0;
        ms_ring_build();
    }
}

/*
 * THE FPS COUNTER (F5; the port's own, not the game's): a
 * number at the top left, counted over the last half second, in a tiny
 * 3 x 5 figure of its own so that it works on every screen whatever font is
 * loaded.  Remembered for the session through OS_FPS.
 */
#ifdef MISTER_HYBRID
/* MISTER: the race's flat artwork read again, in the set that goes with the
   picture's new width (tall_follow) */
static void race_art_load(void)
{
    hud_load();
    if (g_art_model >= 0) { cockpit_load(g_art_model, g_art_livery); dash_load(g_art_model); }
}
#endif

static int g_fps_on = -1, g_fps_shown;

static void fps_toggle(void)
{
#ifdef MISTER_HYBRID
    return;                                  /* MISTER: the core's own FPS Counter option (the hybrid library draws it) */
#endif
    g_fps_on = !g_fps_on;
    if (g_fps_on) setenv("OS_FPS", "1", 1); else unsetenv("OS_FPS");
}

/* every screen's events: F5 turns the frames a second on and off wherever it is pressed, and
   no screen sees it */
static int poll_event(SDL_Event *e)
{
    for (;;) {
        if (!controls_poll(e)) return 0;
        if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_F5) { if (!e->key.repeat) fps_toggle(); continue; }
        if (e->type == SDL_KEYUP && e->key.keysym.sym == SDLK_F5) continue;
        return 1;
    }
}

/* a number in the port's own 3 x 5 figures, at the top left, on line `line` */
static void tiny_number(int value, int line)
{
    static const char *digit[10] = {
        "111101101101111", "010110010010111", "111001111100111", "111001111001111", "101101111001001",
        "111100111001111", "111100111101111", "111001010010010", "111101111101111", "111101111001111" };
    char text[16];
    snprintf(text, sizeof text, "%d", value > 999999 ? 999999 : value < 0 ? 0 : value);
    int k = g_s * 2, x0 = 2 * g_s, y0 = 2 * g_s + line * 7 * k;
    for (int pass = 0; pass < 2; pass++)             /* a dark edge first, then the figure */
        for (int c = 0; text[c]; c++)
            for (int r = 0; r < 5; r++)
                for (int q = 0; q < 3; q++) {
                    if (digit[text[c] - '0'][r * 3 + q] != '1') continue;
                    for (int yy = -1 + pass; yy < k + 1 - pass; yy++)
                        for (int xx = -1 + pass; xx < k + 1 - pass; xx++) {
                            int x = x0 + (c * 4 + q) * k + xx, y = y0 + r * k + yy;
                            if (x >= 0 && y >= 0 && x < FB_W && y < FB_H)
                                fb[y * FB_W + x] = pass ? 0xffffff00u : 0xff000000u;
                        }
                }
}

static void fps_draw(void)
{
    static Uint32 since;
    static int frames;
#ifdef MISTER_HYBRID
    mister_frame_done();                               /* MISTER: OS_FPSLOG=1 writes the real speed to the log; */
    return;                                            /* the number on the screen is the core's FPS Counter option */
#endif
    if (g_fps_on < 0) g_fps_on = getenv("OS_FPS") != NULL;
    Uint32 now = SDL_GetTicks();
    frames++;
    if (now - since >= 500) { g_fps_shown = (int)((float)frames * 1000.0f / (float)(now - since) + 0.5f); frames = 0; since = now; }
    if (g_fps_on) tiny_number(g_fps_shown, 0);
}

/* returns 1 with *track (1..6) and *car (row of CARS_DATA) chosen, 0 to quit */
static int menu_run(int *track, int *car)
{
    static char lines[96][40];
    static uint8_t bg[2][FB_MAXW * FB_MAXH];
    static uint32_t bgpal[2][256];
    static mesh model;
    int n = text_lines("build/dec/LOCAL/CARDAT.TXT", lines, 96);
    g_nmenu_car = 0;
    for (int i = 0; i + 5 < n + 1 && g_nmenu_car < MENU_CARS && i + 4 < n; i += 6) {
        menu_car *c = &g_menu_car[g_nmenu_car++];
        snprintf(c->name, sizeof c->name, "%.23s", lines[i]);
        c->gearbox = atoi(lines[i + 1]); c->kmh = atoi(lines[i + 2]);
        c->grip = atoi(lines[i + 3]);    c->accel = atoi(lines[i + 4]);
    }
    n = text_lines("build/dec/LOCAL/TRACKDAT.TXT", lines, 96);
    for (int i = 0; i < 12 && i * 4 < n; i++) snprintf(g_menu_track[i], sizeof g_menu_track[i], "%.23s", lines[i * 4]);
    text_lines("build/dec/LOCAL/MENU.TXT", g_menu_text, 60);
    strcpy(g_menu_text[60], "LOW RESOLUTION");
    strcpy(g_menu_text[61], "HIGH RESOLUTION");
    for (int k = 0; k < 3; k++) strcpy(g_menu_text[62 + k], g_menu_text[5 + k]);     /* LOW / MID / HI DETAIL */
    strcpy(g_menu_text[65], "ULTRA DETAIL");
    car_spec specs[CAR_SPECS_MAX];
    int nspec = cars_dtr_load(specs, CAR_SPECS_MAX, "build/dec/GEN/CARS.DTR");
    if (g_nmenu_car > nspec) g_nmenu_car = nspec;

    g_fnt_file = "build/dec/GRAF/MENUL.FNT";
    g_spr_file = "build/dec/GRAF/MENUL.SPR";
    hud_load();
    mlb_load("build/dec/MENU/MENU.MLB");
    if (!pcx_load("build/dec/GRAF/MENU3L.PCX", bg[0], bgpal[0]) ||
        !pcx_load("build/dec/GRAF/MENU2L.PCX", bg[1], bgpal[1]) || !g_nfont || !g_nmenu_car) {
        fprintf(stderr, "menu: its files are missing under build/dec (GRAF, MENU, LOCAL)\n");
        return 0;
    }
    g_focal_x = GAME_FOCAL * (float)g_s; g_focal_y = GAME_FOCAL * (float)SY;
    fb  = malloc(sizeof(uint32_t) * FB_MAXW * (FB_MAXH + TALL_ROWS));     /* (the biggest: the size can change as it runs) */
    zb  = malloc(sizeof(float)    * FB_MAXW * (FB_MAXH + TALL_ROWS));
    idb = malloc(sizeof(int16_t)  * FB_MAXW * (FB_MAXH + TALL_ROWS));
    pxi = malloc(sizeof(int16_t)  * FB_MAXW * (FB_MAXH + TALL_ROWS));
    if (g_s == 2 && (!fb || !zb || !idb || !pxi)) res_fail("no memory for the bigger picture");
    static scene empty;                      /* draw_object looks at the scene's count */
    g_scene = &empty;

    static uint8_t bg_main[FB_MAXW * FB_MAXH], bg_cars[FB_MAXW * FB_MAXH];
    static uint32_t pal_main[256], pal_cars[256];
    static uint32_t frozen[FB_MAXW * FB_MAXH];
    pcx_load("build/dec/GRAF/MENU1L.PCX", bg_main, pal_main);
    pcx_load("build/dec/GRAF/MENU4L.PCX", bg_cars, pal_cars);     /* 0x1110: the cars' picture */
    ms_load(specs, nspec);
    int scr_shown = -1, leaving_set = 0;
    float fade = 0.0f, fade_in = 0.6f;
    char page_sig[512] = "";
    Uint32 last_t = 0;
    int screen = 0, pick[2] = { 0, 0 }, loaded_screen = -1, loaded_pick = -1;
    /* pages 5 and 6 are the championship: NEW / RESUME, and its stage screens */
    int page = 0, sel[9] = { 0 }, mode = 1, resume = 0, tick = 0;
    static uint8_t bg_champ[FB_MAXW * FB_MAXH];
    static uint32_t pal_champ[256];
    int champ_pic = -1;
    champ_texts();
    if (getenv("OS_REPLAYFILE")) {               /* back from a record's replay: the options as they were */
        if (getenv("OS_KEEP_DIFF"))  setenv("OS_DIFF", getenv("OS_KEEP_DIFF"), 1);
        if (getenv("OS_KEEP_LAPS"))  setenv("OS_LAPS", getenv("OS_KEEP_LAPS"), 1);
        if (getenv("OS_KEEP_CHEAT")) setenv("OS_CHEAT", getenv("OS_KEEP_CHEAT"), 1);
        unsetenv("OS_REPLAYFILE"); unsetenv("OS_KEEP_CHEAT");
    }
    if (getenv("OS_CHAMP")) {                    /* back from a championship race */
        champ_load();
        const char *res = getenv("OS_CHAMP_RESULT");
        int pos = 0, over = 0; float time = 0.0f;
        if (res && sscanf(res, "%d,%f,%d", &pos, &time, &over) == 3) champ_result(pos, time, over);
        if (getenv("OS_KEEP_DIFF")) setenv("OS_DIFF", getenv("OS_KEEP_DIFF"), 1);
        if (getenv("OS_KEEP_LAPS")) setenv("OS_LAPS", getenv("OS_KEEP_LAPS"), 1);
        unsetenv("OS_CHAMP"); unsetenv("OS_CHAMP_RESULT");
        mode = 2; page = 6;
    }            /* page 0 main, 1 options, -1 = the selection screens */
    /* coming back from a race: the options as they were left */
    save_load();
    /* OS_UNLOCK=1: everything open, for testing (not saved) */
    int unlock = getenv("OS_UNLOCK") != NULL;
    int name_pos = 0, name_letter = g_save.name[0] - 'A', next_page = -1, next_screen = 0, rec_track = 0, rec_sel = 0;
    /* the options: as the settings file has them, or as the race just left hands them back */
    g_opt_res = (g_s == 2);
    g_opt_detail = g_cfg.detail;
#ifdef MISTER_HYBRID
    if (g_opt_detail > 2) g_opt_detail = 2;  /* MISTER: no ULTRA */
#endif
    g_opt_diff = g_cfg.diff; g_opt_laps = g_cfg.laps;
    if (getenv("OS_MENU_GOTO")) { page = atoi(getenv("OS_MENU_GOTO")); unsetenv("OS_MENU_GOTO"); }
    if (getenv("OS_DIFF")) { g_opt_diff = atoi(getenv("OS_DIFF")); if (g_opt_diff > 3) g_opt_diff = 3; }
    if (getenv("OS_LAPS")) {
        int l = atoi(getenv("OS_LAPS"));
        g_opt_laps = l == 5 ? 1 : l == 10 ? 2 : l == 25 ? 3 : 0;
    }
    g_opt_speech = getenv("OS_FROM_RACE_OPTS") ? (getenv("OS_NOSPEECH") ? 0 : 1) : g_cfg.speech;
    g_opt_music = getenv("OS_FROM_RACE_OPTS") ? (getenv("OS_NOMUSIC") ? 0 : 1) : g_cfg.music;
    if (getenv("OS_NOMUSIC") && !getenv("OS_FROM_RACE_OPTS")) g_opt_music = 0;      /* asked for on the command line */
    if (getenv("OS_MENU_PAGE")) { page = atoi(getenv("OS_MENU_PAGE")); if (page > 8) page = 8; }
    if (getenv("OS_REC_TRACK")) rec_track = atoi(getenv("OS_REC_TRACK")) % 6;
    if (getenv("OS_CHAMP_STAGE")) { champ_new(); g_ch.stage = atoi(getenv("OS_CHAMP_STAGE")); g_ch.pos[0] = 2; g_ch.pts[0] = 7; g_ch.time[0] = 151.37f; }
    float turn = 0.0f;
    const char *shot = getenv("OS_MENU_SHOT");          /* "screen,pick,file": one frame, no window */
    char shotfile[256] = "";
    if (shot && sscanf(shot, "%d,%d,%255s", &screen, &pick[1], shotfile) == 3) {
        if (!getenv("OS_MENU_PAGE")) page = -1;
        pick[0] = pick[1];
        if (screen == 0 && pick[0] > 11) pick[0] = 11;
        turn = getenv("OS_MENU_TURN") ? (float)atof(getenv("OS_MENU_TURN")) : 0.9f;
    }

    SDL_Window *win = NULL; SDL_Renderer *ren = NULL; SDL_Texture *tex = NULL;
    if (!shotfile[0]) {
        win = sys_window(&ren);
        if (!win) return 0;
        SDL_SetWindowTitle(win, "OpenScreamer");
        tex = sys_texture(FB_W, FB_H);
        if (g_s == 2 && (!win || !ren || !tex)) res_fail("the window for it could not be made");
        /* the menu has its own nine samples, MENU.DTR's SOUND_FX (smenu1..9); the
           numbers below are the ones the game passes to 0x1f708 */
        snd_init("build/dec/SOUND", "build/dec/GEN/MENU.DTR");
        snd_masters((float)g_cfg.vol_fx / 24576.0f, (float)g_cfg.vol_cd / 24576.0f * (float)g_cfg.vol_mu / 24576.0f);
    }
    /* THE CHEATS (0x288dc, table at 0x2e00, words from MENU.DTR): five letters
       typed anywhere in the menu.  Letters that fit no word start again. */
    static const struct { const char *word; int what, value, flip; } cheat[9] = {
        { "TAZOR", 0, 1, 0 },        /* 0x207ec = 1: the Bullet can be chosen */
        { "VTELO", 1, 3, 0 },        /* 0x20814 = 3: the six tracks */
        { "MONTY", 2, 1, 0 },        /* 0x1d270 = 1: the second set of props */
        { "JOINT", 2, 2, 0 },        /* 0x1d270 = 2: the third set */
        { "CLOCK", 2, 4, 1 },        /* 0x1d270 ^= 4: the countdown stands still */
        { "ABURN", 3, 3, 0 },        /* the difficulty line = 3, AFTERBURNER */
        { "INVER", 1, 9, 0 },        /* 0x20814 = 9: the reversed tracks too */
        { "MIRRO", 2, 0x10, 1 },     /* 0x1d270 ^= 0x10: mirrored */
        { "UPDOW", 2, 0x20, 1 },     /* 0x1d270 ^= 0x20: upside down */
    };
#define CHEAT_KEY(k) do { \
        if ((k) >= SDLK_a && (k) <= SDLK_z) { \
            int fits = 0; \
            if (ntyped < 5) typed[ntyped++] = (char)('A' + ((k) - SDLK_a)); \
            typed[ntyped] = 0; \
            for (int c_ = 0; c_ < 9; c_++) { \
                if (strncmp(cheat[c_].word, typed, (size_t)ntyped)) continue; \
                fits = 1; \
                if (ntyped < 5) continue; \
                if (cheat[c_].what == 0) { g_save.bullet = cheat[c_].value; save_write(); } \
                if (cheat[c_].what == 1) { g_save.level = cheat[c_].value; save_write(); } \
                if (cheat[c_].what == 2) cheat_word = cheat[c_].flip ? cheat_word ^ cheat[c_].value : cheat[c_].value; \
                if (cheat[c_].what == 3) { g_opt_diff = cheat[c_].value; cheat_diff = 1; } \
                snd_play(8); \
                printf("cheat: %s\n", cheat[c_].word); fflush(stdout); \
                ntyped = 0; \
            } \
            if (!fits) ntyped = 0; \
        } } while (0)
    int net_msg = 0;
    char typed[8] = "", rplfile[128] = "";
    replay_head rplhead;
    int ntyped = 0, cheat_word = getenv("OS_CHEAT") ? atoi(getenv("OS_CHEAT")) : 0, cheat_diff = 0;
    int result = -1, music_on = 0;
    while (result < 0) {
        /* 0x29e47: the menu plays "music%d" of 0x1f00 = song_for(1), while MUSIC is on */
        if (!shotfile[0] && music_on != g_opt_music) {
            music_on = g_opt_music;
            snd_music("build/dec/MUSIC", music_on ? song_for(1) : 0);
        }
        tick++;
        g_menu_tick = tick;
        g_track_max = unlock ? 11 : g_save.level + 2 > 11 ? 11 : g_save.level + 2;
        g_car_max = (unlock || g_save.bullet) ? 12 : 11;
        if (g_car_max > g_nmenu_car - 1) g_car_max = g_nmenu_car - 1;
        g_page_options[OPT_DIFF].max = (unlock || g_save.bullet || cheat_diff) ? 3 : 2;            /* AFTERBURNER */
        if (g_opt_diff > g_page_options[OPT_DIFF].max) g_opt_diff = g_page_options[OPT_DIFF].max;
        /* OS_MENU_KEYS="DEUS..." presses keys by itself, one every 1.2 s, for
           testing: U D L R, E enter, X escape, S save a picture, a-z typed */
        {
            static const char *script; static Uint32 script_t; static int script_on, script_shots;
            if (!script_on) { script_on = 1; script = getenv("OS_MENU_KEYS"); script_t = SDL_GetTicks(); }
            if (script && SDL_GetTicks() - script_t > 1200) {
                script_t = SDL_GetTicks();
                char k = *script;
                if (!k) { result = 0; script = NULL; }
                else {
                    script++;
                    if (k == 'S') {
                        char name[64];
                        snprintf(name, sizeof name, "menu_script_%d.ppm", script_shots++);
                        memcpy(fb, frozen, sizeof(uint32_t) * (size_t)(FB_W * FB_H));
                        write_ppm(name);
                    } else {
                        SDL_Event e;
                        memset(&e, 0, sizeof e);
                        e.type = SDL_KEYDOWN;
                        e.key.keysym.sym = k == 'U' ? SDLK_UP : k == 'D' ? SDLK_DOWN : k == 'L' ? SDLK_LEFT
                                         : k == 'R' ? SDLK_RIGHT : k == 'E' ? SDLK_RETURN : k == 'X' ? SDLK_ESCAPE
                                         : (SDL_Keycode)k;
                        SDL_PushEvent(&e);
                    }
                }
            }
        }
        /* the resolution line was changed: it is saved, and the menu comes back in it */
        if (!shotfile[0] && g_opt_res != (g_s == 2)) {
            g_cfg.res = g_opt_res; g_cfg.detail = g_opt_detail; g_cfg.diff = g_opt_diff; g_cfg.laps = g_opt_laps;
            g_cfg.speech = g_opt_speech; g_cfg.music = g_opt_music; g_cfg.starting = 0;
            cfg_save();
            unsetenv("OS_RES");
            unsetenv("OS_MENU_KEYS");
            setenv("OS_MENU_GOTO", "1", 1);
            setenv("OS_INTRO_DONE", "1", 1);
            snd_close();
            os_go(1, NULL, NULL);                /* the menu again, in the new size */
        }
        float dt = 0.04f;
        int steps = 1;
        static float idle;
        if (shotfile[0]) steps = getenv("OS_MENU_STEPS") ? atoi(getenv("OS_MENU_STEPS")) : 100;
        else {
            Uint32 now_t = SDL_GetTicks();
            dt = last_t ? (float)(now_t - last_t) / 1000.0f : 0.02f;
            last_t = now_t;
            if (dt < 0.005f) dt = 0.005f;
            if (dt > 0.1f) dt = 0.1f;
        }
#define MENU_SHOW() do { \
            memcpy(frozen, fb, sizeof(uint32_t) * (size_t)(FB_W * FB_H)); \
            if (fade < 1.0f) \
                for (int i_ = 0; i_ < FB_W * FB_H; i_++) { \
                    uint32_t c_ = fb[i_]; \
                    fb[i_] = 0xff000000u | (uint32_t)((float)(c_ >> 16 & 255) * fade) << 16 \
                           | (uint32_t)((float)(c_ >> 8 & 255) * fade) << 8 | (uint32_t)((float)(c_ & 255) * fade); \
                } \
            fps_draw(); \
            SDL_UpdateTexture(tex, NULL, fb, FB_W * sizeof(uint32_t)); } while (0)
        /* the game's screens: 0 the pages, 2 cars, 3 tracks, 4 the name, 5 championship, 6 records */
        int want = page == 6 ? 5 : page == 8 ? 6 : page == -2 ? 4 : page == -1 ? (screen == 0 ? 3 : 2) : 0;
        /* 0x27f9c: the pages left alone for 30 s (0xedc) give way to the intro again */
        idle = (want == 0 && !shotfile[0]) ? idle + dt : 0.0f;
        if (idle > 30.0f && result < 0 && !getenv("OS_NOINTRO") && !getenv("OS_MENU_KEYS")) { idle = 0.0f; result = 2; }
        if (!shotfile[0] && scr_shown >= 0 && (want != scr_shown || result >= 0)) {
            /* 0x29738: the screen that is left goes dark, standing still.  The
               pages' camera pulls away as it does, rolling (0x293e8, 0x294ab) */
            if (scr_shown == 0 && !leaving_set) {
                leaving_set = 1;
                memcpy(g_mc.p0, g_mc.now, sizeof g_mc.p0);
                g_mc.p1[2] = 20000.0f; g_mc.f = 1.0f; g_mc.speed = 0.6f; g_mc.roll = -(float)M_PI;
            }
            fade -= dt * (scr_shown == 0 ? 2.0f : 1.5f);
            if (fade > 0.0f) {
                if (scr_shown == 0) {
                    ms_cam_step(dt);
                    g_mc.roll += dt * (float)M_PI;
                    memcpy(g_pal, pal_main, sizeof g_pal);
                    ms_begin(bg_main);
                    camera c = ms_cam_port();
                    view_setup(&c);
                    ms_letters_draw(&c);
                    ms_logos(0.0f);
                } else memcpy(fb, frozen, sizeof(uint32_t) * (size_t)(FB_W * FB_H));
                float keep = fade;
                MENU_SHOW();
                memcpy(frozen, fb, 0);
                fade = keep;
                SDL_RenderClear(ren);
                SDL_RenderCopy(ren, tex, NULL, NULL);
                SDL_RenderPresent(ren); res_started();
                SDL_Event ev;
                while (SDL_PollEvent(&ev)) if (ev.type == SDL_QUIT) { result = 0; fade = 0.0f; }
                if (scr_shown != 0) {            /* the still picture is kept undimmed for the next frame */
                    /* frozen was overwritten by MENU_SHOW with the same undimmed picture: nothing to do */
                }
                continue;
            }
            fade = 0.0f; scr_shown = -1; leaving_set = 0;
            if (result >= 0) break;
        }
        if (result >= 0) break;
        if (scr_shown != want) {
            scr_shown = want;
            fade = shotfile[0] ? 1.0f : 0.0f;
            fade_in = want == 0 ? 0.6f : 1.5f;
            ms_enter(want, pick[1], pick[0], name_letter);
            page_sig[0] = 0;
        }
        if (fade < 1.0f) { fade += dt * fade_in; if (fade > 1.0f) fade = 1.0f; }
        if (page == -2 || page == 8) {           /* the name (screen 4), a track's records (screen 6) */
            if (page == -2) {
                memcpy(g_pal, bgpal[1], sizeof g_pal);
                for (int k = 0; k < steps; k++) { ms_ring_step(dt, name_letter); ms_cam_step(dt); }
                ms_begin(bg[1]);
                camera c = ms_cam_port();
                view_setup(&c);
                ms_letters_draw(&c);
                name_draw(NULL, name_pos);
            }
            else {
                if (champ_pic != 100 + rec_track) {
                    char file[64];
                    snprintf(file, sizeof file, "build/dec/GRAF/LOAD%dL.PCX", rec_track + 1);
                    pcx_load(file, bg_champ, pal_champ);
                    champ_pic = 100 + rec_track;
                }
                memcpy(g_pal, pal_champ, sizeof g_pal);
                records_draw(bg_champ, rec_track, rec_sel);
            }
            loaded_screen = -1;
            if (shotfile[0]) { write_ppm(shotfile); result = 0; break; }
            MENU_SHOW();
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex, NULL, NULL);
            SDL_RenderPresent(ren); res_started();
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) result = 0;
                if (ev.type != SDL_KEYDOWN) continue;
                int key = ev.key.keysym.sym;
                if (key == SDLK_F12) { write_ppm("menu_page.ppm"); printf("saved menu_page.ppm\n"); }
                if (page == 8) {                 /* 0x283f0, 0x284e0 */
                    CHEAT_KEY(key);
                    if (key == SDLK_UP && rec_sel > 0)   { rec_sel--; snd_play(4); }
                    if (key == SDLK_DOWN && rec_sel < 4) { rec_sel++; snd_play(4); }
                    if (key == SDLK_ESCAPE) { page = 7; snd_play(0); }
                    /* 0x291f7: ENTER on a total that kept its replay plays it */
                    if ((key == SDLK_RETURN || key == SDLK_KP_ENTER) && g_save.t[rec_track].rpl[rec_sel]) {
                        replay_name(rplfile, sizeof rplfile, rec_track + 1, g_save.t[rec_track].rpl[rec_sel]);
                        if (replay_head_read(rplfile, &rplhead)) {
                            pick[0] = rplhead.track - 1 + (rplhead.reverse ? 6 : 0); pick[1] = rplhead.car;
                            mode = 1; result = 1; snd_play(3);
                        } else rplfile[0] = 0;
                    }
                    continue;
                }
                CHEAT_KEY(key);
                /* 0x284c1, 0x283d1: UP / DOWN go from letter to letter; 0x28696,
                   0x2882e: LEFT / RIGHT turn the alphabet.  ONE alphabet position
                   serves all three letters, as in the game */
                if (key == SDLK_UP && name_pos > 0)   { name_pos--; snd_play(5); }
                if (key == SDLK_DOWN && name_pos < 2) { name_pos++; snd_play(5); }
                if ((key == SDLK_LEFT || key == SDLK_RIGHT) && g_mc.f < 0.1f) {      /* 0x28819: one step at a time */
                    name_letter += key == SDLK_RIGHT ? 1 : -1;
                    if (name_letter < 0) name_letter = 25;
                    if (name_letter > 25) name_letter = 0;
                    g_save.name[name_pos] = (char)('A' + name_letter);
                    float a = (float)name_letter * -0.24166098f;       /* the camera walks round to it */
                    ms_cam_go(sinf(a) * 4500.0f, 1800.0f, cosf(a) * 4500.0f, (float)name_letter * 0.24166098f);
                }
                if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE) {   /* 0x29005 */
                    save_write();
                    page = next_page; screen = next_screen; snd_play(0);
                }
                if (key == SDLK_ESCAPE) { page = 0; snd_play(0); }
            }
            continue;
        }
        if (page == 6) {                         /* a championship stage */
            champ_stage st = champ_stage_at(g_ch.stage);
            if (champ_pic != st.pic) {
                char file[64];
                snprintf(file, sizeof file, "build/dec/GRAF/CHAMP%dL.PCX", st.pic);
                pcx_load(file, bg_champ, pal_champ);
                champ_pic = st.pic;
            }
            memcpy(g_pal, pal_champ, sizeof g_pal);
            champ_draw(bg_champ, shotfile[0] ? 0 : tick * 2);
            loaded_screen = -1;
            if (shotfile[0]) { write_ppm(shotfile); result = 0; break; }
            MENU_SHOW();
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex, NULL, NULL);
            SDL_RenderPresent(ren); res_started();
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) result = 0;
                if (ev.type != SDL_KEYDOWN) continue;
                switch (ev.key.keysym.sym) {
                case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE: {
                    int what = champ_enter();
                    if (what == 1) { pick[0] = champ_stage_at(g_ch.stage).race; pick[1] = g_ch.car; result = 1; }
                    else if (what == 2) page = 0;
                    break;
                }
                case SDLK_ESCAPE: page = 0; break;
                case SDLK_F12: write_ppm("menu_page.ppm"); printf("saved menu_page.ppm\n"); break;
                default: break;
                }
            }
            continue;
        }
        if (page >= 0) {
            const menu_item *items = page == 0 ? g_page_main : page == 1 ? g_page_options
                                   : page == 2 ? g_page_special : page == 7 ? g_page_records : g_page_attack;
            int n = page == 0 ? 5 : page == 1 ? OPT_LINES : page == 2 ? 3 : page == 7 ? 6 : 2;      /* 3 and 5: NEW / RESUME */
            memcpy(g_pal, pal_main, sizeof g_pal);
            {
                /* a page, or a line of it, that is not what the cubes spell: they
                   turn away and the new ones take their place (0x29838) */
                char sig[512] = "";
                for (int i = 0; i < n; i++) {
                    strncat(sig, g_menu_text[items[i].line + (items[i].value ? *items[i].value : 0)], 40);
                    strcat(sig, "|");
                }
                if (strcmp(sig, page_sig)) {
                    if (g_ms_shown == -1 || shotfile[0]) {
                        ms_page_build(items, n, sel[page], 1);
                        g_ms_shown = page; g_ms_leaving = 0; strcpy(page_sig, sig);
                    } else g_ms_leaving = 1;
                }
                for (int k = 0; k < steps; k++) {
                    if (ms_page_step(dt, sel[page]) && g_ms_leaving) {
                        ms_page_build(items, n, sel[page], 0);
                        g_ms_shown = page; g_ms_leaving = 0; strcpy(page_sig, sig);
                    }
                    ms_cam_step(dt);
                }
                ms_begin(bg_main);
                camera c = ms_cam_port();
                view_setup(&c);
                ms_letters_draw(&c);
                for (int k = 0; k < steps; k++) ms_logos(dt);
                /* 0x2df38: the game's own word for what the port cannot do either */
                if (net_msg && (tick * 2 & 0x7f) < 0x48) hud_text_centred(0xbe, g_menu_text[55], 0, 2);
            }
            loaded_screen = -1;
            if (shotfile[0]) { write_ppm(shotfile); result = 0; break; }
            MENU_SHOW();
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex, NULL, NULL);
            SDL_RenderPresent(ren); res_started();
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) result = 0;
                if (ev.type != SDL_KEYDOWN) continue;
                const menu_item *it = &items[sel[page]];
                idle = 0.0f;
                CHEAT_KEY(ev.key.keysym.sym);
                switch (ev.key.keysym.sym) {
                /* sounds: 1 from line to line (0x283ca); 0 into a page, 3 into a
                   game (0x28b5f, 0x28ba2); 2 for an option and 0 for ESC are the
                   port's reading of 0x28df0 and 0x280ec */
                case SDLK_UP:   if (sel[page] > 0) { sel[page]--; snd_play(1); net_msg = 0; } break;         /* 0x2848e: no wrap */
                case SDLK_DOWN: if (sel[page] < n - 1) { sel[page]++; snd_play(1); net_msg = 0; } break;
                case SDLK_LEFT:
                    if (it->value && it->works) { *it->value = (*it->value + it->max) % (it->max + 1); snd_play(2); }
                    break;
                case SDLK_RIGHT: case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE:
                    if (page == 0 && it->line == 3) net_msg = 1;     /* NETWORK GAME: "NETWORK NOT FOUND" (0x20984 = 2) */
                    if (!it->works) break;
                    snd_play(it->value ? 2 : (page == 0 && (it->line == 1 || it->line == 2 || it->line == 4)) || page == 1
                                             || (page == 2 && it->line == 28) || page == 7 ? 0 : 3);
                    if (it->value) *it->value = (*it->value + 1) % (it->max + 1);   /* 0x297d4 */
                    else if (page == 1 && it->line == 21) page = 7;                 /* VIEW RECORDS */
                    else if (page == 7) { rec_track = it->line - 31; rec_sel = 0; page = 8; }   /* 0x2c844 */
                    /* every game starts with ENTER YOUR NAME, which then goes on
                       to the screen put aside at 0x1d234 (0x28b80 .. 0x28cba) */
                    else if (page == 0 && it->line == 0) { mode = 1; page = -2; next_page = -1; next_screen = 0; }
                    else if (page == 0 && it->line == 1) page = 5;
                    else if (page == 0 && it->line == 2) page = 2;
                    else if (page == 5 && it->line == 24) {            /* NEW: choose the car first (0x28c6b) */
                        champ_new(); mode = 2; page = -2; next_page = -1; next_screen = 1;
                    } else if (page == 5) {                            /* RESUME: the screen before the last race (0x28c9e) */
                        champ_load(); g_ch.stage = g_ch.prev; mode = 2; page = -2; next_page = 6;
                    }
                    else if (page == 0 && it->line == 4) page = 1;
                    else if (page == 2 && it->line == 28) page = 3;
                    else if (page == 2) { mode = it->line == 29 ? MODE_CONES : MODE_SLALOM; page = -2; next_page = -1; next_screen = 0; }
                    else if (page == 3) { mode = MODE_ATTACK; resume = it->line == 25; page = -2; next_page = -1; next_screen = 0; }
                    break;
                case SDLK_ESCAPE: snd_play(0); if (page == 3) page = 2; else if (page == 7) page = 1; else if (page != 0) page = 0; else result = 0; break;
                case SDLK_F11:
                    SDL_SetWindowFullscreen(win, (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP)
                                                 ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                    break;
                case SDLK_F12: write_ppm("menu_page.ppm"); printf("saved menu_page.ppm\n"); break;
                default: break;
                }
            }
            continue;
        }
        if (loaded_screen != screen || loaded_pick != pick[screen]) {
            char file[128];
            if (screen == 0) snprintf(file, sizeof file, "build/dec/MENU/AMAP%d.3DX", pick[0] % 6 + 1);
            else snprintf(file, sizeof file, "build/dec/MENU/A%s.3DX", specs[pick[1]].name);
            (void)file; (void)model;
            loaded_screen = screen; loaded_pick = pick[screen];
        }
        {
            /* 0x2ca10 / 0x2cb88: the whole row is there, each one turning on the
               spot (a quarter turn a second the cars, half that the tracks) */
            static car_pose still;
            memcpy(g_pal, screen == 0 ? bgpal[0] : pal_cars, sizeof g_pal);
            for (int k = 0; k < steps; k++) {
                g_ms_turn_car += dt * 1.5707964f; if (g_ms_turn_car >= 6.2831855f) g_ms_turn_car = 0.0f;
                g_ms_turn_map += dt * 0.7853982f; if (g_ms_turn_map >= 6.2831855f) g_ms_turn_map = 0.0f;
                ms_cam_step(dt);
            }
            ms_begin(screen == 0 ? bg[0] : bg_cars);
            camera c = ms_cam_port();
            view_setup(&c);
            if (screen == 0) {
                for (int i = 0; i < 12; i++)
                    if (g_ms_map_ok[i % 6] && abs(i - pick[0]) <= 1)
                        ms_draw(&g_ms_map[i % 6], (float)i * -40000.0f, 0.0f, 2900.0f, 0.0f, g_ms_turn_map, 0.0f,
                                0x6000, NULL, &c);
            } else {
                for (int i = 0; i < g_nmenu_car; i++) {
                    int model_no = atoi(specs[i].name + 4) % 7;
                    if (g_ms_car_ok[model_no] && abs(i - pick[1]) <= 2)
                        ms_draw(&g_ms_car[model_no], (float)i * -4000.0f, 0.0f, 2900.0f, 0.0f, g_ms_turn_car, 0.0f,
                                specs[i].livery | 0x6000, &still, &c);
                }
            }
            menu_frame(screen, pick[screen], turn, NULL, NULL, 0);
        }
        if (shotfile[0]) { write_ppm(shotfile); result = 0; break; }
        MENU_SHOW();
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren); res_started();
        turn += 0.012f;
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) result = 0;
            if (ev.type != SDL_KEYDOWN) continue;
            int max = screen == 0 ? g_track_max : g_car_max;
            int key0 = ev.key.keysym.sym;
            /* 0x28fd5, 0x28255: leaving the track screen, the new track has been seen */
            if (screen == 0 && (key0 == SDLK_RETURN || key0 == SDLK_KP_ENTER || key0 == SDLK_SPACE || key0 == SDLK_ESCAPE)
                && g_save.seen != g_save.level) { g_save.seen = g_save.level; save_write(); }
            CHEAT_KEY(key0);
            switch (ev.key.keysym.sym) {
            /* 0x2859e .. 0x287cd: not until the camera is most of the way to the last one */
            case SDLK_LEFT: case SDLK_RIGHT: {
                int to = pick[screen] + (ev.key.keysym.sym == SDLK_RIGHT ? 1 : -1);
                if (to < 0 || to > max || g_mc.f >= 0.8f) break;
                pick[screen] = to; snd_play(6);
                ms_cam_go((float)to * (screen == 0 ? -40000.0f : -4000.0f), g_mc.p1[1], g_mc.p1[2], 0.0f);
                break;
            }
            case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE:
                snd_play(0);
                if (screen == 0) screen = 1;
                else if (mode == 2) { g_ch.car = pick[1]; champ_save(); page = 6; }
                else result = 1;
                break;
            /* 0x2821e: back from the tracks is the name screen */
            case SDLK_ESCAPE: if (mode == 2) { page = 0; screen = 0; } else if (screen == 1) screen = 0;
                              else { page = -2; next_page = -1; next_screen = 0; } break;
            case SDLK_F11:
                SDL_SetWindowFullscreen(win, (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP)
                                             ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                break;
            case SDLK_F12: {
                static int shots = 0;
                char name[64];
                snprintf(name, sizeof name, "menu_%03d.ppm", shots++);
                write_ppm(name);
                printf("saved %s\n", name);
                break;
            }
            default: break;
            }
        }
    }
    (void)tex; (void)ren; (void)win;         /* the window stays: it is the program's, not the menu's */
    *track = pick[0] + 1;
    *car = pick[1];
    {
        static const char *laps[4] = { "3", "5", "10", "25" };
        if (!shotfile[0] && !getenv("OS_MENU_KEYS")) {
            g_cfg.detail = g_opt_detail;
            g_cfg.diff = g_opt_diff; g_cfg.laps = g_opt_laps; g_cfg.speech = g_opt_speech; g_cfg.music = g_opt_music;
            cfg_save();
        }
        setenv("OS_FROM_RACE_OPTS", "1", 1);
        setenv("OS_LAPS", laps[g_opt_laps & 3], 1);
        char diff[8];
        snprintf(diff, sizeof diff, "%d", g_opt_diff == 3 ? 4 : g_opt_diff);     /* 0x2a131: the fourth is file E */
        setenv("OS_DIFF", diff, 1);
        if (g_opt_speech) unsetenv("OS_NOSPEECH"); else setenv("OS_NOSPEECH", "1", 1);
        if (g_opt_music) unsetenv("OS_NOMUSIC"); else setenv("OS_NOMUSIC", "1", 1);
        if (mode == MODE_CONES) setenv("OS_MODE", "cones", 1);
        else if (mode == MODE_SLALOM) setenv("OS_MODE", "slalom", 1);
        else if (mode == MODE_ATTACK) setenv("OS_MODE", "attack", 1);
        else unsetenv("OS_MODE");
        if (mode == MODE_ATTACK && resume) setenv("OS_GHOST", "1", 1); else unsetenv("OS_GHOST");
        if (mode == 2 && result == 1) {          /* 0x28ef6: three laps, the league's opponents */
            static const char *file[4] = { "0", "1", "2", "4" };
            setenv("OS_KEEP_DIFF", diff, 1);
            setenv("OS_KEEP_LAPS", laps[g_opt_laps & 3], 1);
            setenv("OS_DIFF", file[champ_stage_at(g_ch.stage).league & 3], 1);
            setenv("OS_LAPS", "3", 1);
            setenv("OS_CHAMP", "1", 1);
        } else { unsetenv("OS_CHAMP"); unsetenv("OS_KEEP_DIFF"); unsetenv("OS_KEEP_LAPS"); }
        char word[16];
        snprintf(word, sizeof word, "%d", cheat_word);
        setenv("OS_CHEAT", word, 1);
        if (rplfile[0] && result == 1) {         /* a record's replay: the race as it was */
            char v[16];
            setenv("OS_KEEP_DIFF", diff, 1);
            setenv("OS_KEEP_LAPS", laps[g_opt_laps & 3], 1);
            setenv("OS_KEEP_CHEAT", word, 1);
            snprintf(v, sizeof v, "%d", rplhead.diff);  setenv("OS_DIFF", v, 1);
            snprintf(v, sizeof v, "%d", rplhead.laps);  setenv("OS_LAPS", v, 1);
            snprintf(v, sizeof v, "%d", rplhead.cheat); setenv("OS_CHEAT", v, 1);
            setenv("OS_REPLAYFILE", rplfile, 1);
        } else unsetenv("OS_REPLAYFILE");
    }
    snd_close();
    return result;
}

/*
 * THE INTRO (0x276a0; build/decomp/menu_scene.c, demo_camera.c).
 *
 * The game opens with a real race that has no player in it (mode 0x100,
 * difficulty NORMAL, started at once: flags 0x2001), filmed from the track's
 * camera posts, with the logos and the credits over it:
 *  - camera (routine 8, 0x1aeb4 / 0x1a920): the post of MAPnCAM.SPL nearest
 *    the car being followed, turned to the car.  The last digit of the post's
 *    height says whether it closes in to 1500 units as the trackside camera
 *    does: bit 1 always, bit 2 while the car is going away, bit 4 while it is
 *    coming -- and when that changes at the same post the camera flies
 *    across instead of cutting (see intro_cam_car).  Every 1000 ticks of
 *    1/100 s (0x27e64) another car is lined up, one at a different post
 *    (0x19da3), and it takes over at the followed car's next change of post;
 *  - logos (0x279fc): LOGOL.SPR sprite 0 with the first block of credits,
 *    sprite 1 with the second (24 rows lower), sprite 2 from the third on;
 *    each slides in from 320 to the left and out again;
 *  - credits (0x27b64): the '#'-separated blocks of ATTR.TXT, font 0 colour
 *    2, 20 rows a line; the lines slide in from 350 to the right, stay until
 *    4 s are up, slide out to the left, and the next block follows;
 *  - a key (ESC, ENTER, SPACE: 0x211d0), or the end of the credits, fades
 *    picture and sound out in a second (0x27d28) and the menu follows.
 * The cars' engines and the scenery are heard from the camera.
 */
static char g_attr[200][40];
static int  g_nattr;
static struct {
    int   st, block, out, line, xo[20];
    float t, logo_x, fade;
    gspline cam[1];
    int   have_cam, car, pending, next_car, cams_ready, cam_ok;
    float change, clock;
    camera view;
} g_in;

static void intro_init(const char *data, int track)
{
    char file[600];
    memset(&g_in, 0, sizeof g_in);
    g_in.logo_x = -320.0f;
    /* the picture fades in, and the cars go the moment it is fully there
       (Danik, from the original; the one second is the fade-out's, 0x27d28) */
    g_in.fade = 0.0f;
    g_race_clock = RACE_HOLD - 0.5f;
    for (int i = 0; i < 20; i++) g_in.xo[i] = 350;
    g_nattr = text_lines("build/dec/LOCAL/ATTR.TXT", g_attr, 200);
    snprintf(file, sizeof file, "%s/MAP%dCAM.SPL", data, track);
    g_in.have_cam = splines_load(file, g_in.cam, 1);
    /* nobody is driving: the player's car is taken right out of the world */
    g_pc.x += 1.0e7f;
    if (g_pshadow) g_pshadow->x += 1.0e7f;
    g_snd_engine = snd_stop(g_snd_engine);
}

static int intro_camera_step(camera *cam);

static void intro_step(float dt)
{
    if (g_in.st != 3 && g_in.st != 4 && g_in.line < g_nattr) {
        /* the logo's slide */
        if (g_in.block < 2) g_in.logo_x = (g_in.logo_x - (float)(g_in.out * -320)) * drive_damp(0x50) + (float)(g_in.out * -320);
        else if (g_in.block == 0x12 && g_in.out) g_in.logo_x = (g_in.logo_x + 340.0f) * drive_damp(0x50) - 340.0f;
        else g_in.logo_x = g_in.logo_x * drive_damp(0x5a);
        /* the block's lines */
        int i = 0, idx = g_in.line;
        float target = (float)(g_in.out * -400);
        do {
            if (i < 20) {
                if (g_in.xo[i] > -360) g_in.xo[i] = (int)(((float)g_in.xo[i] - target) * drive_damp(0x5a) + target);
                else g_in.t = 4.0f;
            }
            i++; idx++;
        } while (idx < g_nattr && g_attr[idx][0] != '#');
        g_in.t += dt;
        if (g_in.t >= 4.0f) {
            g_in.t = 0.0f;
            g_in.out = !g_in.out;
            if (!g_in.out) {
                g_in.line = idx + 1; g_in.block++;
                for (int k = 0; k < 20; k++) g_in.xo[k] = 350;
            }
            if (g_in.line >= g_nattr) g_in.st = 3;       /* the end of the credits: 3 has the "leave" bit */
        }
    }
    g_in.clock += dt;
    if (g_in.st & 2) {                                    /* 0x27d28 */
        g_in.fade -= dt;
        if (g_in.fade <= 0.0f) { g_in.fade = 0.0f; g_in.st = 4; }
    } else if (g_in.fade < 1.0f) {
        g_in.fade += dt * 2.0f;                           /* 0x21e50 */
        if (g_in.fade > 1.0f) g_in.fade = 1.0f;
    }
    volumes_apply(g_in.fade);                             /* the sound goes with the picture, both ways */
    /* the camera is worked out here, once a step of the race -- as the game
       does it once a frame.  Worked out once a PICTURE it saw the same
       distance twice whenever two pictures fell in one step, which reads as
       "coming" and "going" at once and flicked the close-in on and off */
    g_in.cam_ok = intro_camera_step(&g_in.view);
    if (g_in.cam_ok) { g_ear_yaw = g_in.view.yaw; sound_world(g_in.view.x, g_in.view.y, 0); }
    snd_render(dt);
    g_in.change += dt;
    if (g_in.change > 10.0f) { g_in.change = 0.0f; g_in.next_car = 1; }      /* 0x27e64: every 1000 ticks of 1/100 s, another car */
}

static int intro_post(const opponent *o, float *dist)
{
    int at = -1;
    float best = 1e30f;
    for (int i = 0; i < g_in.cam[0].n; i++) {
        const int32_t *n = g_in.cam[0].node[i];
        float dx = (float)n[1] - o->x, dy = (float)n[2] - o->y, dz = (float)n[3] - o->z;
        float d = sqrtf(dx * dx + dy * dy + dz * dz);
        if (d > 0.0f && d < best) { best = d; at = i; }
    }
    if (dist) *dist = best;
    return at;
}

/*
 * EVERY CAR HAS ITS OWN CAMERA in the intro (0x1a920, the car's record from
 * +0x4f8), worked out for all of them every frame; 0x1aeb4 then shows one.
 *
 *   - the camera stands at the post of MAPnCAM.SPL nearest the car and looks
 *     at it; with "close in" on it goes along that line until it is 1500
 *     units from the car;
 *   - close in = the post's code (the last digit of its height): bit 1
 *     always, bit 2 while the car is going away from the post, bit 4 while
 *     it is coming.  At a NEW post the comparison starts from the distance
 *     itself, which counts as both: the first picture from a post with bit 2
 *     or 4 is always the close one;
 *   - a new post is a CUT.  But when close-in changes at the same post the
 *     camera FLIES there (0x19ad8 with the view table's row 4 asked for as
 *     "2"): the difference between where it was and where it now belongs is
 *     kept and dies away exactly as when the player changes view (0x19ef0:
 *     place x damp[90] a frame, the angles eased with stiffness 0x19).  So a
 *     shot of a car going away from such a post opens close to the car and
 *     swings back out to the post.
 */
static struct intro_cam {
    int    last, first, zoom, fly;
    float  prev;
    camera at;                      /* where it belongs */
    float  dx, dy, dz, dyaw, dpitch, droll;      /* what is left of a flight */
} g_ic[OPP_MAX];

static void intro_cam_place(struct intro_cam *ic, const opponent *o, int post, float d, int zoom)
{
    const int32_t *n = g_in.cam[0].node[post];
    float x = (float)n[1], y = (float)n[2], z = (float)n[3];
    float dx = o->x - x, dy = o->y - y, dz = o->z - z;
    ic->at.yaw = atan2f(dy, dx);
    ic->at.pitch = atan2f(dz, sqrtf(dx * dx + dy * dy));
    ic->at.roll = 0.0f;
    float move = d - 1500.0f;
    if (zoom && move > 0.0f) { x += dx / d * move; y += dy / d * move; z += dz / d * move; }
    ic->at.x = x; ic->at.y = y; ic->at.z = z;
}

static void intro_cam_car(int k)
{
    struct intro_cam *ic = &g_ic[k];
    const opponent *o = &g_opp[k];
    float d;
    int post = intro_post(o, &d);
    if (post < 0) return;
    if (post == ic->last) ic->first = 0;
    else { ic->prev = d; ic->last = post; ic->first = 1; }
    int code = abs(g_in.cam[0].node[post][3]) % 10;
    int zoom = (code & 1) || ((code & 2) && ic->prev - d <= 0.0f) || ((code & 4) && ic->prev - d >= 0.0f);
    if (zoom != ic->zoom && !ic->first) {
        /* close-in has changed at the same post: from where the camera is
           with the old setting, fly to where the new one puts it */
        intro_cam_place(ic, o, post, d, ic->zoom);
        camera from = ic->at;
        intro_cam_place(ic, o, post, d, zoom);
        ic->dx = from.x - ic->at.x; ic->dy = from.y - ic->at.y; ic->dz = from.z - ic->at.z;
        ic->dyaw = wrap_pi(from.yaw - ic->at.yaw);
        ic->dpitch = wrap_pi(from.pitch - ic->at.pitch);
        ic->droll = 0.0f;
        ic->fly = 1;
    } else
        intro_cam_place(ic, o, post, d, zoom);
    ic->zoom = zoom;
    ic->prev = d;
    if (ic->first) ic->fly = 0;                           /* a new post: a cut */
    if (ic->fly) {                                        /* 0x19ef0 */
        float kp = drive_damp(90), ka = drive_damp(0x19), lim = DRIVE_DT * 200.0f;
        ic->dx *= kp; ic->dy *= kp; ic->dz *= kp;
        ic->dyaw *= ka; ic->dpitch *= ka;
        if (cosf(ic->dyaw) > 0.99995f && cosf(ic->dpitch) > 0.99995f &&
            fabsf(ic->dx) < lim && fabsf(ic->dy) < lim && fabsf(ic->dz) < lim) ic->fly = 0;
    }
}

static int intro_camera_step(camera *cam)
{
    if (!g_in.have_cam || g_nopp <= 0) return 0;
    if (g_in.car >= g_nopp) g_in.car = 0;
    if (!g_in.cams_ready) {
        for (int k = 0; k < OPP_MAX; k++) { memset(&g_ic[k], 0, sizeof g_ic[k]); g_ic[k].last = -1; }
        g_in.pending = g_in.car;
        g_in.cams_ready = 1;
    }
    for (int k = 0; k < g_nopp && k < OPP_MAX; k++) intro_cam_car(k);
    if (g_in.next_car) {                                  /* 0x19da3: line up the next car that is at another post */
        g_in.next_car = 0;
        for (int tries = 0; tries < g_nopp; tries++) {
            g_in.pending = (g_in.pending + 1) % g_nopp;
            if (g_in.pending == g_in.car || g_ic[g_in.pending].last != g_ic[g_in.car].last) break;
        }
    }
    /* 0x1aef3: it takes over when the car being shown changes post */
    if (g_ic[g_in.car].first && g_in.pending != g_in.car) g_in.car = g_in.pending;
    const struct intro_cam *ic = &g_ic[g_in.car];
    if (ic->last < 0) return 0;
    if (getenv("OS_INTRO_LOG")) {
        static int was_post = -1, was_zoom = -1, was_car = -1, was_fly = -1;
        if (ic->last != was_post || ic->zoom != was_zoom || g_in.car != was_car || ic->fly != was_fly)
            printf("intro %6.2fs  car %d  post %2d (code %d)  close-in %d%s\n", g_in.clock, g_in.car, ic->last,
                   abs(g_in.cam[0].node[ic->last][3]) % 10, ic->zoom, ic->fly ? "  flying across" : "");
        was_post = ic->last; was_zoom = ic->zoom; was_car = g_in.car; was_fly = ic->fly;
    }
    *cam = ic->at;
    if (ic->fly) {                                        /* 0x1a134 */
        cam->x += ic->dx; cam->y += ic->dy; cam->z += ic->dz;
        cam->yaw += ic->dyaw; cam->pitch += ic->dpitch;
    }
    return 1;
}

static void intro_draw(void)
{
    if (g_in.st != 3 && g_in.st != 4 && g_in.line < g_nattr) {
        int spr = g_in.block < 2 ? g_in.block : 2;
        if (spr < g_nspr)
            hud_sprite_draw((320 - g_spr[spr].w / g_s) / 2 + (int)g_in.logo_x, g_in.block < 2 ? g_in.block * 24 + 4 : 4, spr);
        int i = 0, idx = g_in.line;
        do {
            if (i < 20) {
                int w = hud_text(0, 0, g_attr[idx], 0, 2, 0);
                hud_text(g_in.xo[i] + (320 - w) / 2, i * 20, g_attr[idx], 0, 2, 1);
            }
            i++; idx++;
        } while (idx < g_nattr && g_attr[idx][0] != '#');
    }
    if (g_in.fade < 1.0f)
        for (int k = 0; k < FB_W * FB_H; k++) {
            uint32_t c = fb[k];
            fb[k] = 0xff000000u | (uint32_t)((float)(c >> 16 & 255) * g_in.fade) << 16
                  | (uint32_t)((float)(c >> 8 & 255) * g_in.fade) << 8 | (uint32_t)((float)(c & 255) * g_in.fade);
        }
}

static int session(int argc, char **argv);

/*
 * THE PROGRAM.  One start: the directories, the developer switch, then the
 * parts one after another -- intro, menu, race, menu ... -- each begun from
 * the program's own untouched state (port.h), in the one window.
 *
 *   openscreamer [-d] [-data DIR] [-home DIR]            the game
 *   openscreamer [...] build/dec/MAPn n [--shot ...]     straight into a track (testing)
 */
int main(int argc, char **argv)
{
    os_init(argc, argv);
    os_state_save();
    static jmp_buf back;                     /* (static: it is put back with everything else, and set again below) */
    for (;;) {
        g_next_part = &back;
        if (setjmp(back) == 0) {
            int ac = os_argc();
            char **av = os_argv();
            os_next_done();
            int r = session(ac, av);
#ifdef MISTER_HYBRID
            if (!os_has_next()) return r ? r : mister_exit_code();
#else
            if (!os_has_next()) return r;
#endif
        }
#ifdef MISTER_HYBRID
        mister_show_wait();
#endif
        snd_close();
        os_state_restore();
    }
}

/*
 * THE LOADING SCREEN (0x2a264, the one routine every race is started through:
 * a game from the menu, a championship race, a record's replay).  The screen
 * is cleared, load<track><L|H>.pcx put up with its own palette, turned over
 * when the picture cheats are on (0x1075f, 0x107a1), LOADING -- MENU.TXT line
 * 47 -- written across the middle at row 0x50 in the menu's font 3, colour 0,
 * and it stays while the race is set up.  No fade either way: the race fades
 * in from it (0x21e50).  A reversed track shows the ordinary track's picture.
 *
 * Port: setting a race up takes a tenth of a second here, so the picture is
 * held for a second first (OS_LOADHOLD=<seconds>) -- the port's own, or it
 * would be a flicker.  OS_LOAD_SHOT=<file> saves it.  Returns 0 when the
 * window was closed meanwhile.
 */
static int loading_show(int track)
{
    static uint8_t pic[FB_MAXW * FB_MAXH];
    uint32_t pal[256];
    char file[64];
    SDL_Renderer *ren = os_slot(1);
    SDL_Texture *tex = ren ? sys_texture(FB_W, FB_H) : NULL;
    if (!fb || !g_nfont) return 1;
    snprintf(file, sizeof file, "build/dec/GRAF/LOAD%dL.PCX", track);
    if (!pcx_load(file, pic, pal)) return 1;
    memcpy(g_pal, pal, sizeof g_pal);
    pic_put(pic, 0);
    int cheat = getenv("OS_CHEAT") ? atoi(getenv("OS_CHEAT")) : 0;
    if (cheat & 0x10)
        for (int y = 0; y < FB_H; y++)
            for (int x = 0; x < FB_W / 2; x++) {
                uint32_t t = fb[y * FB_W + x];
                fb[y * FB_W + x] = fb[y * FB_W + FB_W - 1 - x]; fb[y * FB_W + FB_W - 1 - x] = t;
            }
    if (cheat & 0x20)
        for (int y = 0; y < FB_H / 2; y++)
            for (int x = 0; x < FB_W; x++) {
                uint32_t t = fb[y * FB_W + x];
                fb[y * FB_W + x] = fb[(FB_H - 1 - y) * FB_W + x]; fb[(FB_H - 1 - y) * FB_W + x] = t;
            }
    hud_text_centred(0x50, g_menu_text[47], 3, 0);
    if (getenv("OS_LOAD_SHOT")) { write_ppm(getenv("OS_LOAD_SHOT")); printf("saved %s\n", getenv("OS_LOAD_SHOT")); }
    if (!ren || !tex) return 1;
    float hold = getenv("OS_LOADHOLD") ? (float)atof(getenv("OS_LOADHOLD")) : 1.0f;
    Uint32 t0 = SDL_GetTicks();
    do {
        fps_draw();
        SDL_UpdateTexture(tex, NULL, fb, FB_W * (int)sizeof(uint32_t));
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) if (ev.type == SDL_QUIT) return 0;
        SDL_Delay(5);
    } while ((float)(SDL_GetTicks() - t0) < hold * 1000.0f);
    return 1;
}

/*
 * SCREAMER 2'S MENU, FOR NOW (the port's own: the game's is a 3D scene with its own code,
 * not read yet).  A plain list in the game's race font over the chosen track's palette:
 * the track, the car, the conditions, the opponents' level, the laps; ENTER starts the
 * race, ESC leaves.  What is chosen is handed to the race the way the test switches are
 * (OS_TRACK, OS_CAR, OS_COND, OS_DIFF, OS_LAPS), which also keeps it for the next time
 * round.  The words are the game's (LOCAL/STRINGS.TXT) where it has them.
 * Returns the track, or 0 to leave.
 */
/*
 * THE CHAMPIONSHIP, the port's side (the game's: s2_hooks.c).  What is being driven is kept
 * in a slot that outlives the port's parts; what has been WON -- the leagues and tracks
 * opened, the teams that have their fifth car -- in openscreamer.s2champ.  As the game
 * starts: league 1 only, the first three tracks of the menu's order (0x128a14 + 3 of
 * them: one more for each of the first three leagues won, the seventh when league 4 has
 * been won with all four teams).  OS_UNLOCK=1 opens everything.
 */
typedef struct {
    int on, league, race, left, team, model, report, nres;
    unsigned number;
    int points[4];
    s2_result res[4];
    int open_league, open_tracks, fifth[4];
} s2_champ_t;

static s2_champ_t *s2_champ(void);
/* a replay's end shows the race's own places (0x47e75: kept as its ENTER was taken, 0x132384) */
static int s2_champ_kept(s2_result *r)
{
    const s2_champ_t *c = s2_champ();
    if (!c || !c->report) return 0;
    memcpy(r, c->res, sizeof c->res);
    return c->nres;
}

static s2_champ_t *s2_champ(void)
{
    s2_champ_t *c = os_slot(7);
    if (!c) {
        c = os_keep_calloc(1, sizeof *c);                  /* (not calloc: a change of part frees that) */
        if (!c) return NULL;
        FILE *f = os_fopen("openscreamer.s2champ", "r");
        if (f) {
            if (fscanf(f, "league %d tracks %d fifth %d %d %d %d", &c->open_league, &c->open_tracks, &c->fifth[0], &c->fifth[1], &c->fifth[2], &c->fifth[3]) != 6)
                c->open_league = c->open_tracks = c->fifth[0] = c->fifth[1] = c->fifth[2] = c->fifth[3] = 0;
            fclose(f);
        }
        if (c->open_league < 0 || c->open_league > 3) c->open_league = 0;
        if (c->open_tracks < 0 || c->open_tracks > 4) c->open_tracks = 0;
        os_slot_set(7, c);
    }
    return c;
}
static void s2_champ_save(const s2_champ_t *c)
{
    FILE *f = os_fopen("openscreamer.s2champ", "w");
    if (!f) return;
    fprintf(f, "league %d tracks %d fifth %d %d %d %d\n", c->open_league, c->open_tracks, c->fifth[0], c->fifth[1], c->fifth[2], c->fifth[3]);
    fclose(f);
}
static int s2_open_all(void) { return getenv("OS_UNLOCK") && atoi(getenv("OS_UNLOCK")); }

/* the league's next race: its track, the weather drawn, the others' level; the track's number */
static int s2_champ_race(s2_champ_t *c)
{
    char num[24];
    const int track = s2_track_order[c->race % 7] + 1;
    const int cond = s2_champ_weather(c->league, c->race, &c->left, c->number);
    snprintf(num, sizeof num, "%d", track); setenv("OS_TRACK", num, 1);
    snprintf(num, sizeof num, "%d", cond); setenv("OS_COND", num, 1);
    snprintf(num, sizeof num, "%d", s2_league[c->league][4]); setenv("OS_DIFF", num, 1);
    snprintf(num, sizeof num, "%d", c->model); setenv("OS_CAR", num, 1);
    snprintf(num, sizeof num, "%d", c->team); setenv("OS_TEAM", num, 1);
    snprintf(num, sizeof num, "%u", s2_race_seed); setenv("OS_S2SEED", num, 1);
    snprintf(num, sizeof num, "%d", c->league); setenv("OS_S2LEAGUE", num, 1);
    setenv("OS_S2GAME", "2", 1);
    return track;
}

static void s2_menu_show(SDL_Renderer *ren, SDL_Texture *tex)
{
    fps_draw();
    SDL_UpdateTexture(tex, NULL, fb, FB_W * (int)sizeof(uint32_t));
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, NULL, NULL);
    SDL_RenderPresent(ren);
}

/*
 * SCREAMER 2'S OWN MENU: THE SCREEN ITS ROUTINES DRAW ON.  The menu's routines are the game's
 * (s2r_car.h, from 0x38540); the engine calls they make are answered in s2_menu_hooks.h, the
 * drawing ones by these.  The game draws into a picture of palette numbers and shows it through
 * the hardware palette, which the fades change (0x512f1 -> 0x51664); the port draws into its
 * own picture, keeping each pixel's number (pxi), and shows that through the palette last set.
 * The game's palette (0x1338cc, 64 levels) is the one everything is drawn with (g_pal).
 */
static uint32_t g_s2m_hw[256];                 /* the palette as last set (0x51664): what is shown */
static uint8_t  g_s2m_base[768];               /* the game's palette as g_pal has it now */
static SDL_Renderer *g_s2m_ren;
static SDL_Texture  *g_s2m_tex;
static int g_s2m_closed;
static uint8_t g_s2m_key[128];                 /* the keys down, by the PC's scancode (0x4cc25's table) */
static uint32_t g_s2m_ticks;
static float g_s2m_hundredths;                 /* (0x4ce0a: the clock the blinking goes by) */

static uint32_t s2m_rgb(const uint8_t *six) { return 0xFF000000u | (uint32_t)((six[0] << 2 | six[0] >> 4) & 255) << 16 | (uint32_t)((six[1] << 2 | six[1] >> 4) & 255) << 8 | (uint32_t)((six[2] << 2 | six[2] >> 4) & 255); }

void s2m_base(const uint8_t *six)              /* the game's palette, 0x1338cc (768 of 0..63) */
{
    if (!memcmp(six, g_s2m_base, 768)) return;
    memcpy(g_s2m_base, six, 768);
    for (int i = 0; i < 256; i++) g_pal[i] = s2m_rgb(six + 3 * i);
    g_s2_dark_ok = 0;                          /* (the darkening table is the palette's) */
}
void s2m_palette(const uint8_t *six)           /* 0x51664: the palette shown */
{
    for (int i = 0; i < 256; i++) g_s2m_hw[i] = s2m_rgb(six + 3 * i);
}
/* HIGH RESOLUTION (STARTH, docs/NOTES2.md S2-9): the menu at 640 x 400 with the H set; the game's
   routines then give every place in the screen's own pixels, and these draw them as given */
int s2_screen_hi(void) { return g_s == 2 && g_yd == 1; }
void s2m_fonts(void)                           /* 0x50dd3 / 0x50a9c: the menu's font and sprites */
{
    g_fnt_file = s2_screen_hi() ? "build/dec/GRAF1/MENUH.FNT" : "build/dec/GRAF1/MENUL.FNT";
    g_spr_file = s2_screen_hi() ? "build/dec/GRAF1/MENUH.SPR" : "build/dec/GRAF1/MENUL.SPR";
    hud_load();
}
void s2m_picture(int x, int y, int w, int h, const uint8_t *px)    /* 0x51b02 */
{
    const int sw = s2_screen_hi() ? FB_W : 320, sh = s2_screen_hi() ? FB_H : 200;
    for (int r = 0; r < h; r++)
        for (int c = 0; c < w; c++) {
            const int xx = x + c, yy = y + r;
            if (xx < 0 || xx >= sw || yy < 0 || yy >= sh) continue;
            fb[yy * FB_W + xx] = g_pal[px[r * w + c]];
            pxi[yy * FB_W + xx] = px[r * w + c];
        }
}
/* the menu's two clip boxes (0x50d6c sprites, 0x50fb9 text; the game keeps them in its screen's
   record and the drawers copy them in): x0 <= x < x1, y0 <= y < y1, in the 320 x 200 screen */
static int g_s2m_clip[2][4] = { { 0, 0, 320, 200 }, { 0, 0, 320, 200 } };
static void s2m_clip_start(void)               /* (the whole screen, as the menu starts) */
{
    for (int k = 0; k < 2; k++) { g_s2m_clip[k][0] = g_s2m_clip[k][1] = 0; g_s2m_clip[k][2] = s2_screen_hi() ? FB_W : 320; g_s2m_clip[k][3] = s2_screen_hi() ? FB_H : 200; }
}
void s2m_clip(int text, int x0, int y0, int x1, int y1)
{
    int *c = g_s2m_clip[text != 0];
    c[0] = x0; c[1] = y0; c[2] = x1; c[3] = y1;
}
static void s2m_clip_on(int text)
{
    const int *c = g_s2m_clip[text != 0];
    if (s2_screen_hi()) { g_clip[0] = c[0]; g_clip[1] = c[2]; g_clip[2] = c[1]; g_clip[3] = c[3]; }
    else { g_clip[0] = c[0] * g_s; g_clip[1] = c[2] * g_s; g_clip[2] = c[1] * SY + g_oy; g_clip[3] = c[3] * SY + g_oy; }
    if (g_clip[0] < 0) g_clip[0] = 0;
    if (g_clip[1] > FB_W) g_clip[1] = FB_W;
    if (g_clip[2] < 0) g_clip[2] = 0;
    if (g_clip[3] > FB_H) g_clip[3] = FB_H;
}
/* 0x504f2 in the menu (its rooms, the attract race's return): the see-through table from the
   palette now set -- routine 208's texels 32..47, the rooms' shadows, darken to black */
void s2m_blend(const int smoke[3], const int dust[3])
{
    s2_blend_make(smoke, dust); g_have_smg = 1;
    if (getenv("OS_S2MLOG")) printf("menu: see-through table made (asmoke %d %d %d): colour 20 in the shadow's rows 32..47 -> %d .. %d\n", smoke[0], smoke[1], smoke[2], g_smg[32 * 256 + 20], g_smg[47 * 256 + 20]);
}
static void s2m_clip_off(void) { g_clip[0] = 0; g_clip[1] = FB_W; g_clip[2] = 0; g_clip[3] = FB_H; }
int s2m_text(int x, int y, const char *str, int font, int colour, int space)
{
    s2m_clip_on(1); g_hud_native = s2_screen_hi();
    const int w = s2_text(x, y, str, font, colour, space);
    g_hud_native = 0; s2m_clip_off();
    return w;
}
int s2m_text_wide(const char *str, int font, int space) { g_hud_native = s2_screen_hi(); const int w = s2_text_wide(str, font, space); g_hud_native = 0; return w; }
void s2m_sprite(int x, int y, int n) { s2m_clip_on(0); if (s2_screen_hi()) hud_sprite_px(x, y, n); else hud_sprite_draw(x, y, n); s2m_clip_off(); }
void s2m_dark(int x, int y, int w, int h)      /* 0x51698, on the colours' numbers */
{
    s2_dark_make();
    x &= ~3; w &= ~3;
    const int sw = s2_screen_hi() ? FB_W : 320, sh = s2_screen_hi() ? FB_H : 200;
    for (int yy = y; yy < y + h && yy < sh; yy++)
        for (int xx = x; xx < x + w && xx < sw; xx++) {
            if (yy < 0 || xx < 0) continue;
            const int i = yy * FB_W + xx, idx = pxi[i] >= 0 ? pxi[i] : 0;
            pxi[i] = g_s2_dark[0][idx]; fb[i] = g_pal[pxi[i]];
        }
}
void s2m_clear(uint8_t idx) { for (int i = 0; i < FB_W * FB_H; i++) { fb[i] = g_pal[idx]; pxi[i] = idx; } }

/* the engine's objects in its draw list (0x753a0), as s2_menu_hooks.h finds them, drawn by the
   port's renderer over what is on the screen.  Meshes by container and name, loaded once;
   their materials are the library the container had when the object was loaded. */
#define S2M_MESHES 160
static struct { char key[64]; mesh m; int ok; } g_s2m_mesh[S2M_MESHES];
static int g_s2m_nmesh;
/* the material libraries, by "CONTAINER/NAME" (a car's container has one for every car:
   AUTO1.MLB .. AUTO5.MLB); an object's is the one that was current when it was loaded */
static struct { char lib[32]; int base; } g_s2m_mlb[48];
static int g_s2m_nmlb;
static char g_s2m_env[32];          /* "CONTAINER/NAME" of the reflections' map (0xae4e8), "" none */
void s2m_materials(const char *cont, const char *name, int flags)   /* 0x4feb4 */
{
    char file[160], lib[32];
    snprintf(lib, sizeof lib, "%s/%s", cont, name);
    if (flags & 4) snprintf(g_s2m_env, sizeof g_s2m_env, "%s", lib);
    for (int i = 0; i < g_s2m_nmlb; i++)
        if (!strcmp(g_s2m_mlb[i].lib, lib)) { if (flags & 4) g_env_mat = g_s2m_mlb[i].base; return; }
    if (g_s2m_nmlb >= 48) return;
    snprintf(file, sizeof file, "build/dec/%s/%s.MLB", cont, name);
    const int base = mlb_add(file);
    snprintf(g_s2m_mlb[g_s2m_nmlb].lib, sizeof g_s2m_mlb[0].lib, "%s", lib);
    g_s2m_mlb[g_s2m_nmlb++].base = base;
    if (flags & 4) g_env_mat = base;
}
void s2m_materials_go(const char *cont, const char *name)
{
    char lib[32];
    snprintf(lib, sizeof lib, "%s/%s", cont, name);
    if (!strcmp(lib, g_s2m_env)) { g_s2m_env[0] = 0; g_env_mat = -1; }
}
void s2m_materials_all_go(void) { g_s2m_env[0] = 0; g_env_mat = -1; }
static const mesh *s2m_mesh(const char *cont, const char *name, const char *lib)
{
    char key[64], file[160];
    snprintf(key, sizeof key, "%s/%s@%s", cont, name, lib);
    for (int i = 0; i < g_s2m_nmesh; i++) if (!strcmp(g_s2m_mesh[i].key, key)) return g_s2m_mesh[i].ok ? &g_s2m_mesh[i].m : NULL;
    if (g_s2m_nmesh >= S2M_MESHES) return NULL;
    const int k = g_s2m_nmesh++;
    snprintf(g_s2m_mesh[k].key, sizeof g_s2m_mesh[k].key, "%s", key);
    snprintf(file, sizeof file, "build/dec/%s/%s.3DX", cont, name);
    for (char *c = file + 10; *c; c++) *c = (char)toupper((unsigned char)*c);
    g_s2m_mesh[k].ok = mesh_load(&g_s2m_mesh[k].m, file) == 0;
    if (!g_s2m_mesh[k].ok) { fprintf(stderr, "menu: no %s\n", file); return NULL; }
    for (int i = 0; i < g_s2m_nmlb; i++) if (!strcmp(g_s2m_mlb[i].lib, lib) && g_s2m_mlb[i].base >= 0) { mesh_rebase(&g_s2m_mesh[k].m, g_s2m_mlb[i].base); break; }
    return &g_s2m_mesh[k].m;
}
/* the engine's turn of three angles (2048ths), 0x759b8: rows of the matrix */
static void s2m_turn(const int a[3], float m[3][3])
{
    const float k = 3.14159265f / 1024.0f;
    const float s0 = sinf(a[0] * k), c0 = cosf(a[0] * k), s1 = sinf(a[1] * k), c1 = cosf(a[1] * k), s2 = sinf(a[2] * k), c2 = cosf(a[2] * k);
    m[0][0] = c2 * c1 - s2 * s1 * s0; m[0][1] = s2 * c0; m[0][2] = s1 * c2 + s2 * c1 * s0;
    m[1][0] = -(c2 * s1 * s0 + s2 * c1); m[1][1] = c2 * c0; m[1][2] = c2 * c1 * s0 - s1 * s2;
    m[2][0] = -c0 * s1; m[2][1] = -s0; m[2][2] = c1 * c0;
}
/* A ROOM (0x4fca9 "map1" of the container: the options room, the car's garage ...): a scene
   built like a track's (MAPn.MMP / .UNR / .MLB, its backdrop MAPn.BCK), loaded as the race
   loads its track; its library is the materials' first (base 0), the objects' come after it */
static scene g_s2m_scene;
static sky   g_s2m_sky;
static int   g_s2m_have_scene, g_s2m_have_sky;
void s2m_scene_load(const char *cont, const char *name)
{
    char dir[64], file[160];
    const int n = atoi(name + 3) > 0 ? atoi(name + 3) : 1;
    snprintf(dir, sizeof dir, "build/dec/%s", cont);
    snprintf(file, sizeof file, "%s/MAP%d.MLB", dir, n);
    mlb_load(file);
    g_s2m_nmesh = 0;                                         /* (the objects' meshes again, on their libraries) */
    g_s2m_nmlb = 0;
    snprintf(g_s2m_mlb[0].lib, sizeof g_s2m_mlb[0].lib, "%s/MAP%d", cont, n); g_s2m_mlb[0].base = 0; g_s2m_nmlb = 1;
    if (g_s2m_env[0]) {                                      /* (the reflections' map stays what it was: again after the room's) */
        char c2[32], *slash;
        snprintf(c2, sizeof c2, "%s", g_s2m_env);
        if ((slash = strchr(c2, '/')) != NULL) { *slash = 0; s2m_materials(c2, slash + 1, 4); }
    }
    if (getenv("OS_S2MLOG")) {                               /* (a test: which of the engine's routines the room's materials are) */
        int count[256] = { 0 };
        for (int i = 0; i < g_nmat; i++) count[g_mat[i].type & 255]++;
        printf("menu: room's materials %d:", g_nmat);
        for (int t = 0; t < 256; t++) if (count[t]) printf(" routine %d x%d", t, count[t]);
        printf("\n");
    }
    g_s2m_have_scene = scene_load(&g_s2m_scene, dir, n) > 0;
    snprintf(file, sizeof file, "%s/MAP%d.BCK", dir, n);
    if (g_s2m_have_sky) { free(g_s2m_sky.pix); g_s2m_sky.pix = NULL; }
    g_s2m_have_sky = sky_load(&g_s2m_sky, file) == 0;
    if (getenv("OS_S2MLOG")) printf("menu: room %s/%s: %d objects, backdrop %s\n", cont, name, g_s2m_have_scene ? g_s2m_scene.count : 0, g_s2m_have_sky ? "yes" : "no");
}
void s2m_scene_free(void) { g_s2m_have_scene = 0; }

void s2m_draw(const s2m_item *it, int n, const s2m_cam *cm, int room)
{
    static scene_object obj[S2M_MESHES];
    static float mat[S2M_MESHES][9];
    camera cam;
    memset(&cam, 0, sizeof cam);
    /* the camera as the race's (s2_camera_show): the engine's (x, y up, z) is the port's (x, z, y) */
    const float k = 3.14159265f / 1024.0f, h = (float)cm->a[1] * k;
    cam.x = (float)cm->x; cam.y = (float)cm->z; cam.z = (float)cm->y;
    cam.yaw = atan2f(-cosf(h), sinf(h));
    cam.pitch = s2_half_turn((float)cm->a[0] * k);
    cam.roll = -s2_half_turn((float)cm->a[2] * k);
    if (getenv("OS_S2MCAM")) { float d[3] = { 0 }; sscanf(getenv("OS_S2MCAM"), "%f,%f,%f", &d[0], &d[1], &d[2]); cam.yaw += d[0]; cam.pitch += d[1]; cam.roll += d[2]; }  /* a test */
    /* the engine's matrix by its ROWS: with a heading alone an object's x goes to (cos h, sin h)
       and its z to (-sin h, cos h) of the port's world, as the race turns its cars (o->yaw = the
       heading); by its columns everything turned the other way and showed mirrored.
       OS_S2MTRANS=1: the columns, a test */
    const int tr = getenv("OS_S2MTRANS") == NULL;
    int m = 0;
    for (int i = 0; i < n && m < S2M_MESHES; i++) {
        const mesh *me = s2m_mesh(it[i].cont, it[i].name, it[i].lib);
        if (!me) continue;
        float r[3][3];
        s2m_turn(it[i].a, r);
        scene_object *o = &obj[m];
        memset(o, 0, sizeof *o);
        o->m = me; o->phase = it[i].frame; o->radius = me->bound_r + 100.0f;
        o->x = (float)it[i].x; o->y = (float)it[i].z; o->z = (float)it[i].y;
        /* the world directions of the object's own x, y (up), z, each swapped as the places are */
        for (int ax = 0; ax < 3; ax++) {
            const float e[3] = { tr ? r[ax][0] : r[0][ax], tr ? r[ax][1] : r[1][ax], tr ? r[ax][2] : r[2][ax] };
            mat[m][ax * 3 + 0] = e[0]; mat[m][ax * 3 + 1] = e[2]; mat[m][ax * 3 + 2] = e[1];
        }
        o->mat = mat[m];
        m++;
    }
    /* drawn over the picture as it is: the depth cleared, the colours kept */
    const float keep_cy = g_cy; const int keep_vh = g_vh;
    g_cy = s2_screen_hi() ? 200.0f : 100.0f; g_vh = s2_screen_hi() ? 400 : 200;      /* (the eye line in the middle) */
    for (int i = 0; i < FB_W * FB_H; i++) { zb[i] = 0.0f; idb[i] = -1; }
    g_lanes = 4; g_tie_rule = 1;
    /* with the room (0x753e8): its backdrop, then it, then the objects */
    const scene *keep_scene = g_scene;
    if (room && g_s2m_have_sky) {
        sky_paint(cam.yaw, cam.pitch, cam.roll, &g_s2m_sky, g_focal_x, g_focal_y);
        for (int i = 0; i < FB_W * FB_H; i++) pxi[i] = -1;
    }
    view_setup(&cam);
    g_tri_listing = 1;
    if (room && g_s2m_have_scene) {
        g_scene = &g_s2m_scene;
        for (int pass = 0; pass < 2; pass++)
            for (int o = 0; o < g_s2m_scene.count; o++) {
                const scene_object *so = &g_s2m_scene.obj[o];
                g_ground_now = (so->layer == 0);
                if (so->m) draw_object(so, o, pass, &cam);
                g_ground_now = 0;
            }
    }
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < m; i++) draw_object(&obj[i], (room && g_s2m_have_scene ? g_s2m_scene.count : 0) + i, pass, &cam);
    g_scene = keep_scene;
    raster_flush();
    g_cy = keep_cy; g_vh = keep_vh;
}

/* 0x7f5a2: the picture shown -- through the palette last set -- and the keys and the clock read */
int s2m_show(void)
{
    static uint32_t out[FB_MAXW * FB_MAXH];
    if (getenv("OS_SNDWAV")) snd_render(1.0f / 30.0f);  /* (a test: the menu's sound into the file, a frame's worth a frame) */
    for (int i = 0; i < FB_W * FB_H; i++) out[i] = pxi[i] >= 0 ? g_s2m_hw[pxi[i] & 255] : fb[i];
    {   /* the frames a second (F5) over what is shown (the game draws its whole screen again each frame) */
        memcpy(fb, out, sizeof(uint32_t) * (size_t)(FB_W * FB_H));
        fps_draw();
        memcpy(out, fb, sizeof(uint32_t) * (size_t)(FB_W * FB_H));
    }
    if (g_s2m_tex) {
        SDL_UpdateTexture(g_s2m_tex, NULL, out, FB_W * (int)sizeof(uint32_t));
        SDL_RenderClear(g_s2m_ren);
        SDL_RenderCopy(g_s2m_ren, g_s2m_tex, NULL, NULL);
        SDL_RenderPresent(g_s2m_ren);
        res_started();                       /* (a picture shown: this size starts -- the mark for the next start goes) */
    }
    if (getenv("OS_S2MSHOT")) {                       /* a test: OS_S2MSHOT=<frame>,<file>: that frame written, and the end */
        static int f;
        int at = 0; char name[256] = "menu.ppm";
        sscanf(getenv("OS_S2MSHOT"), "%d,%255s", &at, name);
        if (++f >= at) { memcpy(fb, out, sizeof(uint32_t) * (size_t)(FB_W * FB_H)); write_ppm(name); g_s2m_closed = 1; }
    }
    static const struct { SDL_Scancode sdl; uint8_t pc; } map[] = {
        { SDL_SCANCODE_ESCAPE, 0x01 }, { SDL_SCANCODE_RETURN, 0x1c }, { SDL_SCANCODE_KP_ENTER, 0x1c }, { SDL_SCANCODE_SPACE, 0x39 },
        { SDL_SCANCODE_BACKSPACE, 0x0e }, { SDL_SCANCODE_TAB, 0x0f }, { SDL_SCANCODE_UP, 0x48 }, { SDL_SCANCODE_DOWN, 0x50 },
        { SDL_SCANCODE_LEFT, 0x4b }, { SDL_SCANCODE_RIGHT, 0x4d }, { SDL_SCANCODE_LSHIFT, 0x2a }, { SDL_SCANCODE_RSHIFT, 0x36 },
        { SDL_SCANCODE_LCTRL, 0x1d }, { SDL_SCANCODE_RCTRL, 0x1d }, { SDL_SCANCODE_LALT, 0x38 },
        { SDL_SCANCODE_F1, 0x3b }, { SDL_SCANCODE_F2, 0x3c }, { SDL_SCANCODE_F3, 0x3d }, { SDL_SCANCODE_F4, 0x3e }, { SDL_SCANCODE_F5, 0x3f },
        { SDL_SCANCODE_F6, 0x40 }, { SDL_SCANCODE_F7, 0x41 }, { SDL_SCANCODE_F8, 0x42 }, { SDL_SCANCODE_F9, 0x43 }, { SDL_SCANCODE_F10, 0x44 },
        { SDL_SCANCODE_F11, 0x57 }, { SDL_SCANCODE_F12, 0x58 },
    };
    static const char rows[3][11] = { "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM" };
    static const uint8_t row_at[3] = { 0x10, 0x1e, 0x2c };
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) { g_s2m_closed = 1; g_window_closed = 1; }
        if (ev.type != SDL_KEYDOWN && ev.type != SDL_KEYUP) continue;
        const int down = ev.type == SDL_KEYDOWN;
        const SDL_Scancode sc = ev.key.keysym.scancode;
        if (down && sc == SDL_SCANCODE_F12) { write_ppm("menu_shot.ppm"); printf("saved menu_shot.ppm\n"); }
        for (unsigned k = 0; k < sizeof map / sizeof map[0]; k++) if (map[k].sdl == sc) g_s2m_key[map[k].pc] = (uint8_t)down;
        if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z) {
            const char c = (char)('A' + (sc - SDL_SCANCODE_A));
            for (int r = 0; r < 3; r++) { const char *p = strchr(rows[r], c); if (p) g_s2m_key[row_at[r] + (p - rows[r])] = (uint8_t)down; }
        }
        if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_0) g_s2m_key[2 + (sc - SDL_SCANCODE_1)] = (uint8_t)down;
    }
    if (getenv("OS_S2MKEYS")) {                       /* a test: OS_S2MKEYS="frame:key,frame:key..." -- a scancode pressed for a frame */
        static int f; f++;
        const char *p = getenv("OS_S2MKEYS");
        memset(g_s2m_key, 0, sizeof g_s2m_key);
        while (p && *p) { int at = 0, key = 0; if (sscanf(p, "%d:%x", &at, &key) == 2 && at == f && key > 0 && key < 128) g_s2m_key[key] = 1; p = strchr(p, ','); if (p) p++; }
    }
    return !g_s2m_closed;
}
const uint8_t *s2m_keys(void) { return g_s2m_key; }
/* the frame's time (0x4adf8 reads the 1/100 s clock): seconds since the last asking; with
   OS_S2MSHOT or OS_S2MKEYS a fixed 1/30 (the same pictures every time) */
float s2m_elapsed(void)
{
    const uint32_t now = SDL_GetTicks();
    float d = g_s2m_ticks ? (float)(now - g_s2m_ticks) / 1000.0f : 0.02f;
    g_s2m_ticks = now;
    if (getenv("OS_S2MSHOT") || getenv("OS_S2MKEYS")) d = getenv("OS_S2MSTEP") ? (float)atof(getenv("OS_S2MSTEP")) : 1.0f / 30.0f;
    if (d > 0.25f) d = 0.25f;
    g_s2m_hundredths += d * 100.0f;
    return d;
}
int s2m_hundredths(void) { return (int)g_s2m_hundredths; }

/* the menu's race (s2_menu_hooks.h s2m_race): the race part, as the plain menu starts it, and
   back to the menu when it is left */
void s2m_race_go(int track)
{
    char dir[64], num[16];
    if (track < 1 || track > 7) track = 1;
    snprintf(dir, sizeof dir, "build/dec/MAP%d", track);
    snprintf(num, sizeof num, "%d", track);
    setenv("OS_FROM_MENU", "1", 1);
    os_go(3, dir, num);
}

/* the game's own menu, from its loop (0x38540) -- the port's first steps with it: OS_S2MENU3D=1 */
static int s2_menu3d(void)
{
    setenv("OS_S2MENU3D", "1", 1);      /* (its races know they are its: their results go back to it) */
    g_focal_x = GAME_FOCAL * (float)g_s; g_focal_y = GAME_FOCAL * (float)SY;
    if (!fb) fb = malloc(sizeof(uint32_t) * FB_MAXW * (FB_MAXH + TALL_ROWS));
    if (!zb) zb = malloc(sizeof(float) * FB_MAXW * (FB_MAXH + TALL_ROWS));
    if (!idb) idb = malloc(sizeof(int16_t) * FB_MAXW * (FB_MAXH + TALL_ROWS));
    if (!pxi) pxi = malloc(sizeof(int16_t) * FB_MAXW * (FB_MAXH + TALL_ROWS));
    if (!fb || !zb || !idb || !pxi) return 0;
    static scene empty;                      /* (draw_object looks at the scene's count) */
    g_scene = &empty;
    SDL_Window *win = sys_window(&g_s2m_ren);
    if (!win || !g_s2m_ren) return 0;
    SDL_SetWindowTitle(win, "OpenScreamer");
    g_s2m_tex = sys_texture(FB_W, FB_H);
    g_s2m_closed = 0; g_s2m_ticks = 0; g_s2m_nmesh = 0; g_s2m_nmlb = 0;
    if (getenv("OS_CULL")) g_cull = atoi(getenv("OS_CULL"));     /* (a test, as in a race) */
    memset(g_s2m_base, 0xff, sizeof g_s2m_base);
    s2m_clip_start();
    for (int i = 0; i < FB_W * FB_H; i++) { fb[i] = 0xFF000000u; pxi[i] = 0; }
    const int r = s2_menu_run();
    const int res = s2_menu_res_change();
    if (res) {                               /* the options page's RESOLUTION line: kept, and the menu again in it, on that page */
        g_cfg.res = res == 2; g_cfg.starting = 0;
        cfg_save();
        setenv("OS_RES", res == 2 ? "high" : "low", 1);
        setenv("OS_S2MGOTO", "options", 1);
        unsetenv("OS_S2MKEYS");
        if (getenv("OS_S2MKEYSAFTER")) { setenv("OS_S2MKEYS", getenv("OS_S2MKEYSAFTER"), 1); unsetenv("OS_S2MKEYSAFTER"); }   /* (a test: the keys after the change) */
        os_go(1, NULL, NULL);
    }
    return r;
}

/*
 * LEAGUE REPORT (0x43914), after every race of a league: the points for the race, the four
 * by their points, how many races are done and to come; after the last, whether the league
 * is won and what that opens.  The game's own screen is its menu's (other fonts, the
 * teams' badges); this is the port's plain one with the same lines.
 * 1 = on to the next race, 0 = the league is over, -1 = the window was closed.
 */
static int s2_champ_report(s2_champ_t *c, SDL_Renderer *ren, SDL_Texture *tex, char str[][40], int nstr)
{
    static const int points_of[4] = { 10, 6, 3, 0 };
    int order[4] = { 0, 1, 2, 3 }, phase = 0, won = 0;
    const char *t_report = "LEAGUE REPORT", *t_done = "RACES COMPLETED:", *t_togo = "RACES TO GO:", *team[4] = { "CONDOR", "ANGELS", "WASP", "ZEUS" };
    const char *t_won[3] = { "CONGRATULATIONS !!!", "YOU'VE WON", "" }, *t_lost[2] = { "BETTER LUCK", "NEXT TIME !" }, *t_go = "ACCELERATE TO CONTINUE";
    for (int i = 0; i < nstr; i++) {
        if (!strcmp(str[i], "ACCELERATE TO CONTINUE")) t_go = str[i];
        if (!strcmp(str[i], "CONDOR") && i + 3 < nstr) for (int k = 0; k < 4; k++) team[k] = str[i + k];
        if (!strncmp(str[i], "CONGRATULATIONS", 15) && i + 5 < nstr) { t_won[0] = str[i]; t_won[1] = str[i + 1]; t_lost[0] = str[i + 4]; t_lost[1] = str[i + 5]; }
    }
    for (int p = 0; p < c->nres && p < 4; p++) if (c->res[p].car >= 0 && c->res[p].car < 4) c->points[c->res[p].car] += points_of[p];
    c->race++; c->report = 0;
    for (int a = 0; a < 4; a++)                                  /* by points, the most first */
        for (int b = a + 1; b < 4; b++)
            if (c->points[order[b]] > c->points[order[a]]) { const int k = order[a]; order[a] = order[b]; order[b] = k; }
    const int races = s2_league[c->league][0];
    pal_load("build/dec/MAP1H01/MAP1.COL");
    for (;;) {
        char line[80];
        for (int k = 0; k < FB_W * FB_H; k++) fb[k] = 0xff101418u;
        LAY_TOP();
        if (phase == 0) {
            s2_text_centred(14, t_report, 0, 1, 2);
            for (int k = 0; k < 4; k++) {               /* (0x43914: "n.", the team's badge, the name, the points) */
                const int y = 44 + k * 22, colour = 1;
                snprintf(line, sizeof line, "%d.", k + 1);
                s2_text(70, y, line, 0, colour, 2);
                hud_sprite_draw(104, y + 2, ((c->team + order[k]) & 3) + 4);
                s2_text(126, y, order[k] == 0 ? s2_name : "CPU", 0, colour, 2);
                snprintf(line, sizeof line, "%d", c->points[order[k]]);
                s2_text(250 - s2_text_wide(line, 0, 2), y, line, 0, colour, 2);
            }
            if (c->race < races) {
                snprintf(line, sizeof line, "%s %d", t_done, c->race); s2_text_centred(140, line, 3, 3, -1);
                snprintf(line, sizeof line, "%s %d", t_togo, races - c->race); s2_text_centred(152, line, 3, 3, -1);
            }
        } else {
            snprintf(line, sizeof line, "LEAGUE %d", c->league + 1);
            if (won) { s2_text_centred(60, t_won[0], 0, 1, 2); s2_text_centred(90, t_won[1], 0, 1, 2); s2_text_centred(120, line, 0, 1, 2); }
            else { s2_text_centred(70, t_lost[0], 0, 1, 2); s2_text_centred(100, t_lost[1], 0, 1, 2); }
        }
        s2_text_centred(178, t_go, 3, 1, -1);
        if (getenv("OS_S2MENU_SHOT")) { write_ppm(getenv("OS_S2MENU_SHOT")); printf("saved %s\n", getenv("OS_S2MENU_SHOT")); return -1; }
        s2_menu_show(ren, tex);
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) return -1;
            if (ev.type != SDL_KEYDOWN) continue;
            const SDL_Keycode k = ev.key.keysym.sym;
            if (k != SDLK_RETURN && k != SDLK_KP_ENTER && k != SDLK_SPACE && k != SDLK_UP) continue;   /* (the accelerator too) */
            if (phase == 0 && c->race < races) return 1;
            if (phase == 1) { c->on = 0; return 0; }
            /* the league is over (0x43914): won if the player is first in the list */
            won = order[0] == 0;
            if (won) {
                const int *l = s2_league[c->league];
                if (l[2]) c->fifth[c->team & 3] = 1;
                if (l[3] > c->open_league) c->open_league = l[3];
                if (l[1] == 0) { if (c->fifth[0] && c->fifth[1] && c->fifth[2] && c->fifth[3]) c->open_tracks = 4; }
                else if (l[1] > c->open_tracks) c->open_tracks = l[1];
                s2_champ_save(c);
            }
            phase = 1;
        }
        SDL_Delay(8);
    }
}

/*
 * A CAR'S SET-UPS (0x128aa4: for each team, each of its first four cars, four set-ups of six
 * numbers; 0x32d53 starts them all as 2 2 0 2 2 2).  The car screen (0x3d234) offers
 * Standard -- every car 2 2 -1 2 2 2, -1 = the track's own tyres -- or Custom 1-4 in the
 * championship, time attack and its free run (not arcade, not the fifth car); CUSTOMIZE
 * (0x3e161) sets one: HANDLING, BRAKES, TIRE TYPE (A - Dry, B - Wet, C - Snow), TIRE
 * PRESSURE, FRONT and REAR SUSPENSIONS, five steps each but the tyres' three, then SAVE or
 * EXIT.  The items are the numbers 0, 5, 2, 1, 3, 4 of the six.  The game keeps them in its
 * settings file; the port in openscreamer.s2setup.  (The port's plain page, not the game's
 * 3D one with the car turning.)
 */
static int g_s2_setup[4][4][4][6];
/* (the race screen's font the plain pages use has capitals only: "Standard", "A - Dry") */
static const char *s2_caps(const char *t)
{
    static char buf[4][40];
    static int at;
    char *o = buf[at++ & 3];
    size_t i = 0;
    for (; t[i] && i < sizeof buf[0] - 1; i++) o[i] = (char)toupper((unsigned char)t[i]);
    o[i] = 0;
    return o;
}
static void s2_setups_load(void)
{
    for (int t = 0; t < 4; t++) for (int c = 0; c < 4; c++) for (int u = 0; u < 4; u++) {
        static const int first[6] = { 2, 2, 0, 2, 2, 2 };
        memcpy(g_s2_setup[t][c][u], first, sizeof first);
    }
    FILE *f = os_fopen("openscreamer.s2setup", "r");
    if (!f) return;
    for (int t = 0; t < 4; t++) for (int c = 0; c < 4; c++) for (int u = 0; u < 4; u++) {
        int six[6];
        if (fscanf(f, "%d %d %d %d %d %d", &six[0], &six[1], &six[2], &six[3], &six[4], &six[5]) != 6) { fclose(f); return; }
        for (int k = 0; k < 6; k++) if (six[k] < 0 || six[k] > (k == 2 ? 2 : 4)) six[k] = k == 2 ? 0 : 2;
        memcpy(g_s2_setup[t][c][u], six, sizeof six);
    }
    fclose(f);
}
static void s2_setups_save(void)
{
    FILE *f = os_fopen("openscreamer.s2setup", "w");
    if (!f) return;
    for (int t = 0; t < 4; t++) for (int c = 0; c < 4; c++) for (int u = 0; u < 4; u++) {
        const int *six = g_s2_setup[t][c][u];
        fprintf(f, "%d %d %d %d %d %d\n", six[0], six[1], six[2], six[3], six[4], six[5]);
    }
    fclose(f);
}
/* CAR CUSTOMIZATION: 0 when left, -1 when the window is closed */
static int s2_customize(SDL_Renderer *ren, SDL_Texture *tex, char (*str)[40], int nstr, int team, int car, int which)
{
    static const int item_of[6] = { 0, 5, 2, 1, 3, 4 };       /* (0x3e161's items -> the six numbers) */
    const char *title = "CAR CUSTOMIZATION", *item[8] = { "HANDLING", "BRAKES", "TIRE TYPE", "TIRE PRESSURE", "FRONT SUSPENSIONS", "REAR SUSPENSIONS", "SAVE", "EXIT" };
    const char *tyre[3] = { "A - Dry", "B - Wet", "C - Snow" };
    for (int i = 0; i < nstr; i++) {
        if (!strcmp(str[i], "CAR CUSTOMIZATION") && i + 9 < nstr) { title = str[i]; for (int k = 0; k < 8; k++) item[k] = str[i + 1 + k]; }
        if (!strcmp(str[i], "A - Dry") && i + 2 < nstr) for (int k = 0; k < 3; k++) tyre[k] = str[i + k];
    }
    int six[6], sel = 0;
    memcpy(six, g_s2_setup[team][car][which], sizeof six);
    for (;;) {
        char line[64];
        for (int k = 0; k < FB_W * FB_H; k++) fb[k] = 0xff101418u;
        LAY_TOP();
        s2_text_centred(10, title, 0, 1, 2);
        snprintf(line, sizeof line, "CAR %d   CUSTOM %d", car + 1, which + 1);
        s2_text_centred(32, line, 3, 3, -1);
        for (int k = 0; k < 8; k++) {
            const int y = 46 + k * 16 + (k >= 6 ? 6 : 0);
            if (k < 6) {
                const int v = six[item_of[k]];
                if (k == 2) snprintf(line, sizeof line, "%s   %s", item[k], s2_caps(tyre[v]));
                else snprintf(line, sizeof line, "%s   %c%c%c%c%c", item[k], v == 0 ? '1' : '-', v == 1 ? '2' : '-', v == 2 ? '3' : '-', v == 3 ? '4' : '-', v == 4 ? '5' : '-');
            } else snprintf(line, sizeof line, "%s", item[k]);
            s2_text_centred(y, line, 3, k == sel ? 1 : 3, -1);
        }
        if (getenv("OS_S2MENU_SHOT")) { write_ppm(getenv("OS_S2MENU_SHOT")); printf("saved %s\n", getenv("OS_S2MENU_SHOT")); return -1; }
        s2_menu_show(ren, tex);
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) return -1;
            if (ev.type != SDL_KEYDOWN) continue;
            const SDL_Keycode k = ev.key.keysym.sym;
            const int step = k == SDLK_LEFT ? -1 : k == SDLK_RIGHT ? 1 : 0;
            if (k == SDLK_ESCAPE) return 0;
            if (k == SDLK_UP && sel > 0) sel--;
            if (k == SDLK_DOWN && sel < 7) sel++;
            if (step && sel < 6) {
                const int at = item_of[sel], top = sel == 2 ? 2 : 4;
                six[at] += step;
                if (six[at] < 0) six[at] = 0;
                if (six[at] > top) six[at] = top;
            }
            if ((k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) && sel >= 6) {
                if (sel == 6) { memcpy(g_s2_setup[team][car][which], six, sizeof six); s2_setups_save(); }
                return 0;
            }
        }
        SDL_Delay(8);
    }
}

static int s2_menu(void)
{
    static char str[260][40];
    static const int lap_of[4] = { 1, 3, 5, 10 };
    const int nstr = text_lines("build/dec/LOCAL/STRINGS.TXT", str, 260);
    int first_track = -1, first_laps = -1, rookie = -1;
    const char *team_name[4] = { "CONDOR", "ANGELS", "WASP", "ZEUS" };
    for (int i = 0; i < nstr; i++) {
        if (first_track < 0 && !strcmp(str[i], "ENGLAND")) first_track = i;
        if (!strcmp(str[i], "1 LAP")) first_laps = i;
        if (rookie < 0 && !strcmp(str[i], ":ROOKIE")) rookie = i;
        if (!strcmp(str[i], "CONDOR") && i + 3 < nstr) for (int k = 0; k < 4; k++) team_name[k] = str[i + k];
    }
    s2_champ_t *champ = s2_champ();
    /* the lines: the kind of game -- ARCADE, TIME ATTACK (three laps against the ghost of
       the best race so far, by day: the ghost on or off takes the skill's line), TIME
       ATTACK's FREE RUN, CHAMPIONSHIP (a league takes the track's line; its weather and the
       others' skill are the league's) -- then track, car, by day / night / in fog, the
       others' skill, the laps, the team */
    enum { NLINE = 8, L_TRACK = 0, L_CAR, L_COND, L_SKILL, L_LAPS, L_GAME, L_TEAM, L_SETUP };
    static const int game_of[4] = { 1, 4, 8, 2 };
    static const int show[NLINE] = { L_GAME, L_TEAM, L_CAR, L_SETUP, L_TRACK, L_COND, L_SKILL, L_LAPS };
    int v[NLINE] = { 0, getenv("OS_CAR") ? atoi(getenv("OS_CAR")) : 1, getenv("OS_COND") ? atoi(getenv("OS_COND")) : 0,
                     getenv("OS_DIFF") ? 2 - atoi(getenv("OS_DIFF")) : 0, 1, 0, getenv("OS_TEAM") ? atoi(getenv("OS_TEAM")) : 0,
                     getenv("OS_S2SETUPNO") ? atoi(getenv("OS_S2SETUPNO")) : 0 };
    static const int most[NLINE] = { 7, 5, 3, 3, 4, 4, 4, 5 };
    const char *setup_name[5] = { "Standard", "Custom 1", "Custom 2", "Custom 3", "Custom 4" }, *t_customize = "CUSTOMIZE";
    for (int i = 0; i < nstr; i++) {
        if (!strcmp(str[i], "Standard") && i + 4 < nstr) for (int k = 0; k < 5; k++) setup_name[k] = str[i + k];
        if (!strcmp(str[i], "CUSTOMIZE")) t_customize = str[i];
    }
    s2_setups_load();
    int ghost = getenv("OS_S2GHOST") ? atoi(getenv("OS_S2GHOST")) != 0 : 1;
    int league = champ ? champ->league : 0;
    if (getenv("OS_TRACK")) for (int k = 0; k < 7; k++) if (s2_track_order[k] == atoi(getenv("OS_TRACK")) - 1) v[L_TRACK] = k;   /* (the track's line counts in the menu's order) */
    if (getenv("OS_LAPS")) for (int k = 0; k < 4; k++) if (lap_of[k] == atoi(getenv("OS_LAPS"))) v[L_LAPS] = k;
    if (getenv("OS_S2GAME")) for (int k = 0; k < 4; k++) if (game_of[k] == atoi(getenv("OS_S2GAME"))) v[L_GAME] = k;
    for (int k = 0; k < NLINE; k++) v[k] = ((v[k] % most[k]) + most[k]) % most[k];
    int sel = 0, pal_of = -1;
    s2_splits_forget();                              /* (0x3f7a3: a free run's best lap is this visit's) */

    SDL_Renderer *ren = NULL;
    SDL_Window *win = sys_window(&ren);
    if (!win) return 0;
    SDL_SetWindowTitle(win, "OpenScreamer");
    SDL_Texture *tex = sys_texture(FB_W, FB_H);
    if (!fb) fb = malloc(sizeof(uint32_t) * FB_MAXW * (FB_MAXH + TALL_ROWS));
    if (!ren || !tex || !fb) return 0;
    g_fnt_file = "build/dec/GAME1/GAMEL.FNT";
    g_spr_file = "build/dec/GAME1/GAMEL.SPR";
    hud_load();
    if (champ && getenv("OS_S2REPORTTEST")) {        /* a test: the report after race n of league 1 with the player second, or (n = 9) after its last */
        const int n = atoi(getenv("OS_S2REPORTTEST"));
        memset(champ->points, 0, sizeof champ->points);
        champ->on = champ->report = 1; champ->league = 0; champ->race = n == 9 ? 3 : n - 1; champ->nres = 4;
        for (int k = 0; k < 4; k++) { champ->res[k].car = n == 9 ? k : (k + 1) & 3; champ->res[k].done = 1; }
    }
    /* back from a league's race: its report, and on to the next */
    if (champ && champ->on && champ->report) {
        const int go = s2_champ_report(champ, ren, tex, str, nstr);
        if (go < 0) return 0;
        if (go > 0) return s2_champ_race(champ);
        league = champ->league < champ->open_league ? champ->league + 1 : champ->league;
    }
    if (champ) champ->on = 0;
    for (;;) {
        char line[NLINE][64];
        static const char *game[4] = { "ARCADE", "TIME ATTACK", "TIME ATTACK   FREE RUN", "CHAMPIONSHIP" };
        static const char *cond[3] = { "DAY", "NIGHT", "FOG" };
        static const char *level[3] = { "ROOKIE", "NORMAL", "PRO" };
        const int open_tracks = s2_open_all() || !champ ? 7 : champ->open_tracks + 3;
        const int open_league = s2_open_all() || !champ ? 3 : champ->open_league;
        const int fifth = s2_open_all() || (champ && champ->fifth[v[L_TEAM]]);
        const int g = v[L_GAME];
        if (v[L_TRACK] >= open_tracks) v[L_TRACK] = 0;
        if (v[L_CAR] == 4 && !fifth) v[L_CAR] = 0;
        const int track = s2_track_order[v[L_TRACK]];
        if (pal_of != track) {                       /* the colours are the chosen track's */
            char file[96];
            snprintf(file, sizeof file, "build/dec/MAP%dH01/MAP%d.COL", track + 1, track + 1);
            if (!os_exists(file)) snprintf(file, sizeof file, "build/dec/MAP%dL01/MAP%d.COL", track + 1, track + 1);
            pal_load(file);
            pal_of = track;
        }
        snprintf(line[L_GAME], sizeof line[0], "%s", game[g]);
        snprintf(line[L_TEAM], sizeof line[0], "TEAM   %s", team_name[v[L_TEAM]]);
        const int own_setup = g != 0 && v[L_CAR] < 4;          /* (0x3d234: not arcade, not the fifth car) */
        if (own_setup) snprintf(line[L_SETUP], sizeof line[0], v[L_SETUP] && sel < NLINE && show[sel] == L_SETUP ? "%s   ENTER %s" : "%s", s2_caps(setup_name[v[L_SETUP]]), t_customize);
        else snprintf(line[L_SETUP], sizeof line[0], "%s", s2_caps(setup_name[0]));
        snprintf(line[L_CAR], sizeof line[0], "CAR   %d", v[L_CAR] + 1);
        snprintf(line[L_TRACK], sizeof line[0], "TRACK   %s", first_track >= 0 && first_track + track < nstr ? str[first_track + track] : "?");
        snprintf(line[L_COND], sizeof line[0], "%s", cond[g == 0 ? v[L_COND] : 0]);
        snprintf(line[L_SKILL], sizeof line[0], "OPPONENTS   %s", rookie >= 0 && rookie + v[L_SKILL] < nstr ? str[rookie + v[L_SKILL]] + 1 : level[v[L_SKILL]]);
        snprintf(line[L_LAPS], sizeof line[0], "%s", first_laps >= 0 && first_laps + v[L_LAPS] < nstr ? str[first_laps + v[L_LAPS]] : "3 LAPS");
        if (g == 1) { snprintf(line[L_SKILL], sizeof line[0], "GHOST CAR   %s", ghost ? "ON" : "OFF"); snprintf(line[L_LAPS], sizeof line[0], "%s", first_laps >= 0 && first_laps + 1 < nstr ? str[first_laps + 1] : "3 LAPS"); }
        if (g == 2) { snprintf(line[L_SKILL], sizeof line[0], "-"); snprintf(line[L_LAPS], sizeof line[0], "-"); }
        if (g == 3) {
            snprintf(line[L_TRACK], sizeof line[0], "LEAGUE %d   %d TRACKS", league + 1, s2_league[league][0]);
            snprintf(line[L_COND], sizeof line[0], "-");
            snprintf(line[L_SKILL], sizeof line[0], "OPPONENTS   %s", level[2 - s2_league[league][4]]);
        }
        for (int k = 0; k < FB_W * FB_H; k++) fb[k] = 0xff101418u;
        LAY_TOP();
        s2_text_centred(10, "SCREAMER 2", 0, 1, 2);
        for (int k = 0; k < NLINE; k++) s2_text_centred(40 + k * 18, line[show[k]], 3, k == sel ? 1 : 3, -1);
        s2_text_centred(40 + NLINE * 18 + 8, "ENTER   START        ESC   QUIT", 3, 3, -1);
        if (getenv("OS_S2MENU_SHOT")) { write_ppm(getenv("OS_S2MENU_SHOT")); printf("saved %s\n", getenv("OS_S2MENU_SHOT")); return 0; }
        s2_menu_show(ren, tex);
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) return 0;
            if (ev.type != SDL_KEYDOWN) continue;
            const SDL_Keycode k = ev.key.keysym.sym;
            const int at = show[sel], step = k == SDLK_LEFT ? -1 : k == SDLK_RIGHT ? 1 : 0;
            if (k == SDLK_ESCAPE) return 0;
            if (k == SDLK_UP) sel = (sel + NLINE - 1) % NLINE;
            if (k == SDLK_DOWN) sel = (sel + 1) % NLINE;
            if (step) {
                if (at == L_TRACK && g == 3) league = (league + step + open_league + 1) % (open_league + 1);
                else if (at == L_TRACK) v[at] = (v[at] + step + open_tracks) % open_tracks;
                else if (at == L_CAR) v[at] = (v[at] + step + (fifth ? 5 : 4)) % (fifth ? 5 : 4);
                else if (at == L_SKILL && g == 1) ghost = !ghost;
                else if ((at == L_COND && g != 0) || (at == L_SKILL && g >= 2) || (at == L_LAPS && (g == 1 || g == 2)) || (at == L_SETUP && !own_setup)) { }
                else v[at] = (v[at] + step + most[at]) % most[at];
            }
            if ((k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) && at == L_SETUP && own_setup && v[L_SETUP]) {
                if (s2_customize(ren, tex, str, nstr, v[L_TEAM], v[L_CAR], v[L_SETUP] - 1) < 0) return 0;
                continue;
            }
            if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
                char num[16];
                snprintf(num, sizeof num, "%d", v[L_SETUP]); setenv("OS_S2SETUPNO", num, 1);
                if (own_setup && v[L_SETUP]) {
                    const int *six = g_s2_setup[v[L_TEAM]][v[L_CAR]][v[L_SETUP] - 1];
                    char sx[64];
                    snprintf(sx, sizeof sx, "%d,%d,%d,%d,%d,%d", six[0], six[1], six[2], six[3], six[4], six[5]);
                    setenv("OS_S2SETUP", sx, 1);
                } else unsetenv("OS_S2SETUP");
                snprintf(num, sizeof num, "%d", track + 1); setenv("OS_TRACK", num, 1);
                snprintf(num, sizeof num, "%d", v[L_CAR]); setenv("OS_CAR", num, 1);
                snprintf(num, sizeof num, "%d", v[L_TEAM]); setenv("OS_TEAM", num, 1);
                snprintf(num, sizeof num, "%d", g == 0 ? v[L_COND] : 0); setenv("OS_COND", num, 1);
                snprintf(num, sizeof num, "%d", game_of[g]); setenv("OS_S2GAME", num, 1);
                snprintf(num, sizeof num, "%d", ghost); setenv("OS_S2GHOST", num, 1);
                snprintf(num, sizeof num, "%d", 2 - v[L_SKILL]); setenv("OS_DIFF", num, 1);
                snprintf(num, sizeof num, "%d", lap_of[v[L_LAPS]]); setenv("OS_LAPS", num, 1);
                unsetenv("OS_S2SEED");
                if (g == 3 && champ) {
                    /* a league begins (0x431ca): no points, its races not in daylight, a
                       number for the random draws */
                    memset(champ->points, 0, sizeof champ->points);
                    champ->on = 1; champ->league = league; champ->race = 0; champ->report = 0;
                    champ->left = league < 2 ? league : 2;
                    champ->number = (unsigned)SDL_GetTicks() | 1u;
                    champ->team = v[L_TEAM]; champ->model = v[L_CAR];
                    return s2_champ_race(champ);
                }
                return track + 1;
            }
        }
        SDL_Delay(8);
    }
}

static int session(int argc, char **argv)
{
    g_argv = argv; g_argc = argc;
    g_dev = os_dev();
    res_init();
    /*
     * With no arguments: the menu.  It hands over to the race by starting the
     * program again with the track on the command line and the car in OS_CAR
     * -- a clean slate for the track's own palette, materials and scene --
     * and the race comes back here the same way when it is left with ESC.
     */
    if (argc < 2 && os_game() == 2) {
        /* SCREAMER 2: the game's own menu (s2_menu3d, S2-5zq), which starts its races and has
           them come back to it.  A test switch that picks the race (OS_S2PHYS, OS_S2ROBOT,
           OS_NOMENU ...) goes straight to the track of OS_TRACK and leaves the program after it
           -- unless OS_S2MENU3D=1 asks for the game's menu all the same (the menu's tests);
           OS_S2PLAINMENU=1: the port's own plain menu (s2_menu), as before the game's was in */
        if (!s2_available()) {
            fprintf(stderr, "Screamer 2 is not in this build\n");
            return 0;
        }
        char dir[64], num[16];
        int track = getenv("OS_TRACK") ? atoi(getenv("OS_TRACK")) : 1;
        const int straight = getenv("OS_NOMENU") || getenv("OS_S2PHYS") || getenv("OS_S2ROBOT") || getenv("OS_S2SCRIPT") || getenv("OS_SNDWAV")
                          || getenv("OS_FRAMELOG") || getenv("OS_SNDLOG") || !strcmp(SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "", "dummy")
                          || (getenv("SDL_VIDEODRIVER") && !strcmp(getenv("SDL_VIDEODRIVER"), "dummy") && !getenv("OS_S2MENU_SHOT"));
        const char *m3 = getenv("OS_S2MENU3D");
        if ((m3 && atoi(m3) != 0) || (!straight && !getenv("OS_S2PLAINMENU") && !(m3 && atoi(m3) == 0))) { s2_menu3d(); return 0; }
        if (straight) { if (getenv("OS_FROM_RACE")) return 0; }
        else { track = s2_menu(); if (!track) return 0; setenv("OS_FROM_MENU", "1", 1); }
        if (track < 1 || track > 7) track = 1;
        snprintf(dir, sizeof dir, "build/dec/MAP%d", track);
        snprintf(num, sizeof num, "%d", track);
        os_next(3, dir, num);
        return 0;
    }
    if (argc < 2) {
        int track = 1, car = 0;
        int what = 0;
        if (getenv("OS_CYCLETEST")) {            /* a test: menu -> race -> menu ... n times, then how much memory is held */
            int n = getenv("OS_CYCLE_AT") ? atoi(getenv("OS_CYCLE_AT")) : 0;
            char v[16];
            FILE *f = fopen("/proc/self/statm", "r");
            long pages = 0, res = 0;
            if (f) { if (fscanf(f, "%ld %ld", &pages, &res) != 2) res = 0; fclose(f); }
            printf("cycle %d: %ld MB held\n", n, res * 4096 / 1048576);
            fflush(stdout);
            if (n >= atoi(getenv("OS_CYCLETEST"))) return 0;
            snprintf(v, sizeof v, "%d", n + 1);
            setenv("OS_CYCLE_AT", v, 1);
        }
        if (!getenv("OS_INTRO_DONE") && !getenv("OS_NOINTRO") && !getenv("OS_MENU_SHOT")
            && !getenv("OS_MENU_KEYS") && !getenv("OS_CHAMP") && !getenv("OS_FROM_RACE"))
            what = 2;                            /* 0x10092: the intro comes before the menu */
        else what = menu_run(&track, &car);
        if (!what) return 0;
        if (what == 2) {
            /* 0x277af: the track goes round the six, one on every time */
            save_load();
            g_save.intro = (g_save.intro + 1) % 6;
            save_write();
            track = g_save.intro + 1;
            setenv("OS_INTRO", "1", 1);
        } else unsetenv("OS_INTRO");
        setenv("OS_INTRO_DONE", "1", 1);
        char dir[64], num[16], carnum[16];
        /* 0x2a264: a track over 6 is that track less 6 with the reversed option on */
        snprintf(dir, sizeof dir, "build/dec/MAP%d%s", track > 6 ? track - 6 : track, track > 6 ? "I" : "");
        snprintf(num, sizeof num, "%d", track > 6 ? track - 6 : track);
        snprintf(carnum, sizeof carnum, "%d", car);
        setenv("OS_CAR", carnum, 1);
        setenv("OS_FROM_MENU", "1", 1);
        if (what != 2 && !loading_show(track > 6 ? track - 6 : track)) return 0;
        res_started();           /* handing over is not dying: the mark must not outlive this part */
        os_next(3, dir, num);
        return 0;
    }
    const char *data  = (argc > 1) ? argv[1] : "build/dec/MAP1";
    int track         = (argc > 2) ? atoi(argv[2]) : 1;
    if (getenv("OS_MODE"))                       /* cones | slalom */
        g_mode = !strcmp(getenv("OS_MODE"), "cones") ? MODE_CONES
               : !strcmp(getenv("OS_MODE"), "slalom") ? MODE_SLALOM
               : !strcmp(getenv("OS_MODE"), "attack") ? MODE_ATTACK : 1;
    /* 0x2a264: the lap to beat is the track's record -- in the normal game and
       the championship; the special modes get 0 */
    save_load();
    g_intro = getenv("OS_INTRO") != NULL;
    if (g_intro) {                               /* 0x27710: mode 0x100, no options, LOGOL.SPR for sprites */
        g_mode = 1;
        g_spr_file = "build/dec/GRAF/LOGOL.SPR";
        g_fnt_file = "build/dec/GRAF/ATTRL.FNT";      /* the credits have their own font file */
    }
    if (getenv("OS_CHEAT") && !g_intro) g_cheat = atoi(getenv("OS_CHEAT"));
    const int records_ok = (!getenv("OS_DRIVE_DEMO") || getenv("OS_RECORDTEST")) && !getenv("OS_REPLAYFILE") && !g_intro;   /* the robot's laps are not records */
    g_record_lap = g_record_shown = (g_mode == 1 && track >= 1 && track <= 6) ? g_save.t[track - 1].lap[0] : 0.0f;
    const char *shot  = (argc > 4 && strcmp(argv[3], "--shot") == 0)
                        ? argv[4] : NULL;
    float t0          = (argc > 5) ? (float)atof(argv[5]) : 0.0f;
    float cam_height  = (argc > 6) ? (float)atof(argv[6]) : 600.0f;
    if (argc > 7) g_layer = atoi(argv[7]);
    char gfxdir[512];
    snprintf(gfxdir, sizeof gfxdir, "build/dec/MAP%d%c", track, g_s == 2 && g_yd == 1 ? 'H' : 'L');     /* (a wide picture has the low set's tracks) */
    if (os_game() == 2) {
        /* SCREAMER 2: MAP<track><L|H><look><colours> (0x2e676).  The look is the track's
           condition & 1 (see g_s2_cond; tracks 5 and 7 have look 0 only); colours 1 = 256.
           OS_COND=0..2 until there is a menu to choose it. */
        g_s2_cond = getenv("OS_COND") ? atoi(getenv("OS_COND")) : 0;
        if (g_s2_cond < 0 || g_s2_cond > 2) g_s2_cond = 0;
        if (g_s2_cond == 1 && (track == 5 || track == 7)) g_s2_cond = 0;      /* (they have no look 1) */
        /* the track's set is the DETAIL's, not the picture's size (0x2e676): L only at detail 0,
           H otherwise -- and the race's detail is 2 (the options', 0x128a2c).  The two sets are
           different pieces: other objects on the road itself, so other ground under the car.
           OS_S2SET=L for the other; L too when there is no H container. */
        {
            char set = getenv("OS_S2SET") ? getenv("OS_S2SET")[0] : 'H';
            snprintf(gfxdir, sizeof gfxdir, "build/dec/MAP%d%c%d1/MAP%d.MMP", track, set, g_s2_cond & 1, track);
            if (set != 'L' && !os_exists(gfxdir)) set = 'L';
            snprintf(gfxdir, sizeof gfxdir, "build/dec/MAP%d%c%d1", track, set, g_s2_cond & 1);
        }
    }

    /* the DETAIL option; OS_DETAIL=0..3 for one run, OS_DRAWDIST=<units> the
       old test switch (no far plane, pieces out to that distance) */
    g_detail = getenv("OS_DETAIL") ? atoi(getenv("OS_DETAIL")) & 3 : g_cfg.detail & 3;
#ifdef MISTER_HYBRID
    if (g_detail == 3) g_detail = 2;
#endif
    if (g_detail == 3) { g_far_z = 0.0f; g_drawdist = DRAWDIST_FAR; }
    else { g_far_z = g_detail == 2 ? 43808.0f : 27808.0f; g_drawdist = DRAWDIST_ORIGINAL; }
    if (getenv("OS_DRAWDIST")) { g_far_z = 0.0f; g_drawdist = (float)atof(getenv("OS_DRAWDIST")); }
    printf("detail %d: drawn to a depth of %.0f, pieces within %.0f\n", g_detail, g_far_z, g_drawdist);
    if (getenv("OS_FOG"))      g_fog      = (float)atof(getenv("OS_FOG"));
    if (getenv("OS_CULL"))     g_cull     = atoi(getenv("OS_CULL"));
    if (getenv("OS_SHOWEDGES")) g_show_edges = 1;

    char path_[600];
    snprintf(path_, sizeof path_, "%s/MAP%dOPP.SPL", data, track);
    path *r = calloc(1, sizeof(path));
    if (path_build(r, path_) != 0) return 1;

    sky sk = { 0, 0, NULL };
    snprintf(path_, sizeof path_, "build/dec/MAP%dL/MAP%d.BCK", track, track);
    if (os_game() == 2) snprintf(path_, sizeof path_, "%s/MAP%d.BCK", gfxdir, track);
    if (os_game() == 2 && (g_s2_cond == 2 || track == 7)) { }           /* no backdrop there */
    else if (sky_load(&sk, path_) != 0)
        fprintf(stderr, "note: no backdrop at %s\n", path_);

    snprintf(path_, sizeof path_, "%s/MAP%d.COL", gfxdir, track);
    pal_load(path_);
    snprintf(path_, sizeof path_, "%s/MAP%d.MLB", gfxdir, track);
    mlb_load(path_);
    snprintf(path_, sizeof path_, "%s/MAP%d.SMG", gfxdir, track);
    {
        FILE *f = fopen(path_, "rb");
        size_t got = f ? fread(g_smg, 1, sizeof g_smg, f) : 0;
        /* Screamer 2's table has 48 rows where Screamer's has 64.  (Not `sizeof g_smg`: the array
           has 112 rows since Screamer 2's smoke, and Screamer's file was then taken for missing --
           its smoke, shadows and glass drawn as a dark half-mix, track 4's beams too.) */
        g_have_smg = got == 64 * 256 || (os_game() == 2 && got == 48 * 256);
        if (f) fclose(f);
        if (!g_have_smg && os_game() == 2) {
            /* SCREAMER 2's track 7 has no MAP7.SMG (and the program no name for such a file:
               it must make the table itself -- not read).  Until then: the rows the shadow
               uses, 32..47, as the other tracks' files have them near enough -- the palette's
               nearest colour to 1 - 0.045 a row of each colour; the rest left as it is. */
            for (int row = 0; row < 64; row++)
                for (int i = 0; i < 256; i++) {
                    const float k = row >= 32 && row < 48 ? 1.0f - 0.045f * (float)(row - 32) : 1.0f;
                    long least = 1L << 40; int best = i;
                    for (int q = 0; q < 256 && k < 1.0f; q++) {
                        const long dr = (long)((g_pal[q] >> 16) & 255) - (long)((float)((g_pal[i] >> 16) & 255) * k),
                                   dg = (long)((g_pal[q] >> 8) & 255) - (long)((float)((g_pal[i] >> 8) & 255) * k),
                                   db = (long)(g_pal[q] & 255) - (long)((float)(g_pal[i] & 255) * k);
                        if (dr * dr + dg * dg + db * db < least) { least = dr * dr + dg * dg + db * db; best = q; }
                    }
                    g_smg[row * 256 + i] = (uint8_t)best;
                }
            g_have_smg = 1;
        }
        if (!g_have_smg) fprintf(stderr, "note: no blend table at %s\n", path_);
    }

    if (os_game() == 2) {
        int td[11];
        snprintf(path_, sizeof path_, "%s/MAP%dDAT.DTR", data, track);
        if (s2_track_data(path_, td)) {
            /* how far: TRACK_DATA's depth at the top detail, 0.8 of it below (0x2efca) */
            if (g_detail != 3 && !getenv("OS_DRAWDIST")) {
                g_far_z = (float)(int)((float)-td[0] * (g_detail == 2 ? 1.0f : 0.8f));
                g_drawdist = g_far_z * 1.2f > DRAWDIST_ORIGINAL ? g_far_z * 1.2f : DRAWDIST_ORIGINAL;   /* (the port's: pieces looked at) */
                printf("Screamer 2 track %d: drawn to a depth of %.0f\n", track, g_far_z);
            }
            g_ground_colour = g_s2_cond == 0 ? td[4] & 255 : g_s2_cond == 1 ? 0 : 0x13;
            if (!getenv("OS_S2SMGFILE")) { s2_blend_make(td + 5, td + 8); g_have_smg = 1; }   /* (the test switch: the file's table, as before) */
        }
        g_fade_on = 0;
        if (track != 7 && (g_s2_cond == 2 || (g_s2_cond == 1 && track != 4))) {
            snprintf(path_, sizeof path_, "%s/DEPTH%d.DPT", gfxdir, g_s2_cond == 2 ? 2 : 1);
            FILE *f = fopen(path_, "rb");
            g_fade_on = f && fread(g_dpt, 1, sizeof g_dpt, f) == sizeof g_dpt && !getenv("OS_NOFADE");
            if (f) fclose(f);
            if (!g_fade_on) fprintf(stderr, "note: no distance table at %s\n", path_);
            g_fade_near = g_s2_cond == 2 ? 0.0f : 6000.0f;
            g_fade_far  = g_s2_cond == 2 ? 25000.0f : 35000.0f;
        }
    }

    static scene sc;
    int nobj = scene_load(&sc, gfxdir, track);
    g_scene = (nobj && g_layer != 2) ? &sc : NULL;
    /* THE REVERSED TRACKS (option bit 8 of 0x1d270).  All the game does:
       the track's data comes from DATA\MAPnI.DAT instead of MAPn.DAT
       (0x13225: its own opponent lines, grid, markers, props) and the road's
       direction is turned round (0x12329: + pi on the centre points' angle).
       The pictures, the walls and the records are the ordinary track's. */
    g_reverse = strlen(data) > 0 && data[strlen(data) - 1] == 'I';
    if (g_reverse && g_scene)
        for (int i = 0; i < sc.ndir; i++) sc.dir[i].angle += (float)M_PI;
    if (os_game() == 2 && g_scene) path_from_road(r, &sc);
    path_drape(r);
    if (g_scene && !getenv("OS_NOCARS") && os_game() != 2) cars_load(gfxdir);
    if (g_scene && !getenv("OS_NOCARS") && os_game() == 2 && !getenv("OS_S2PHYS")) { s2_race_begin(track, gfxdir); s2_race_show(1.0f); }
    if (g_scene && os_game() == 2 && getenv("OS_S2PHYS")) {
        /* a test of the game's own car routines: a car on the grid, the accelerator down
           (OS_S2KEYS: letters A B L R), OS_S2PHYS frames of s2_step() (OS_S2STEP=<seconds> another); a line every 25 */
        static const int team[4] = { 0, 1, 2, 3 };
        int model[4] = { 1, 2, 3, 0 };
        if (getenv("OS_CAR")) { model[0] = (atoi(getenv("OS_CAR")) % 5 + 5) % 5; for (int t = 1; t < 4; t++) model[t] = (model[0] + t) % 4; }
        const char *keys = getenv("OS_S2KEYS") ? getenv("OS_S2KEYS") : "A";
        const int frames = atoi(getenv("OS_S2PHYS"));
        const int ncars = getenv("OS_S2CARS") ? atoi(getenv("OS_S2CARS")) : 1;
        if (getenv("OS_DIFF")) s2_level = atoi(getenv("OS_DIFF")) < 0 ? 0 : atoi(getenv("OS_DIFF")) > 2 ? 2 : atoi(getenv("OS_DIFF"));   /* 0 pro, 1 normal, 2 rookie (the port starts in rookie) */
        if (!s2_race_start(track, g_s2_cond, ncars, team, model)) { fprintf(stderr, "screamer 2: the race did not start\n"); return 1; }
        if (getenv("OS_S2AT")) { int x = 0, y = 0; float h = 0; sscanf(getenv("OS_S2AT"), "%d,%d,%f", &x, &y, &h); s2_put(0, x, y, h); }   /* a test: the car put somewhere, facing h */
        if (getenv("OS_S2AIR")) {                    /* a test: "height,speed,gap,turn": car 0 in the air, car 1 behind it
                                                        by `gap`, turned by `turn`, thrown at it at `speed` units a second */
            int hgt = 2000; float v = 3000.0f, gap = 1700.0f, turn = 0.0f;
            const float st = getenv("OS_S2STEP") ? (float)atof(getenv("OS_S2STEP")) : s2_step();
            sscanf(getenv("OS_S2AIR"), "%d,%f,%f,%f", &hgt, &v, &gap, &turn);
            s2_put(0, 0, 0, 0.0f); s2_put(1, 150, (int)gap, turn);
            s2_throw(0, hgt, 0.0f, st); s2_throw(1, hgt, v, st);
        }
        if (getenv("OS_S2WHY")) {                    /* a test: every piece whose circle holds the point, and how its faces take it */
            int x = 0, y = 0;
            sscanf(getenv("OS_S2WHY"), "%d,%d", &x, &y);
            for (int o = 0; o < g_scene->count; o++) {
                const scene_object *so = &g_scene->obj[o];
                const mesh *mm = so->m;
                if (!mm) continue;
                const int ox = (int)so->x, oh = (int)so->z, oz = (int)so->y, dx = x - ox, dz = y - oz;
                const int bx = dx - (int)mm->bound_cx, bz = dz - (int)mm->bound_cz, br = (int)mm->bound_r;
                if (bz * bz + bx * bx > br * br) continue;
                const float turn = (float)so->t * 0.0009765625f * 3.14159274f;
                const int px = (int)((float)dx * cosf(turn) + (float)dz * sinf(turn)) + ox, pz = (int)((float)dz * cosf(turn) - (float)dx * sinf(turn)) + oz;
                printf("piece %d (object V%d) run %d, listed in cell %d,%d (its place's cell %d,%d; the point's %d,%d), %d faces in its list:", o, so->v, so->layer,
                       so->cell & 255, so->cell >> 8, (ox + 0x100000) >> 13, (oz + 0x100000) >> 13, (x + 0x100000) >> 13, (y + 0x100000) >> 13, mm->ncoll);
                for (int k = 0; k < mm->ncoll; k++) {
                    const int f = mm->coll[k];
                    if (f < 0) { printf(" -"); continue; }
                    int pos = 0, neg = 0;
                    for (int j = 0; j < mm->ncorner[f]; j++) {
                        const vec3 *a = &mm->vert[mm->idx[f][j]], *b = &mm->vert[mm->idx[f][(j + 1) % mm->ncorner[f]]];
                        const int va[3] = { (int)a->x + ox, 0, (int)a->z + oz }, vb[3] = { (int)b->x + ox, 0, (int)b->z + oz };
                        const int e = s2_edge(va, vb, px, pz);
                        if (e > 0) pos++; else if (e < 0) neg++;
                    }
                    printf(" %s", !neg ? "IN" : !pos ? "in-the-other-way-round" : "out");
                }
                printf("\n");
            }

            return 0;
        }
        if (!getenv("OS_S2HOLD")) s2_go();
        if (getenv("OS_S2DUMP")) s2_dump(getenv("OS_S2DUMP"));                 /* (OS_S2HOLD: the game's own three seconds on the grid) */
        if (getenv("OS_S2PEEK")) { unsigned from = 0; int n = 8; sscanf(getenv("OS_S2PEEK"), "%x,%d", &from, &n); s2_peek(0, from, n); }
        /* a test: OS_S2SCRIPT=<file>, a line a frame "step raw accelerate brake left right" (as read
           out of the original running: NOTES2 S2-5w), OS_S2CLOCK the clock to start from; every
           frame's cars are printed */
        if (getenv("OS_S2SCRIPT")) {
            FILE *sf = fopen(getenv("OS_S2SCRIPT"), "r");
            float step, raw; int a, b, l, r, f = 0;
            if (getenv("OS_S2CLOCK")) s2_clock_set((float)atof(getenv("OS_S2CLOCK")));
            if (getenv("OS_VIEW")) s2_view(g_s2_views[atoi(getenv("OS_VIEW")) % 5]);
            while (sf && fscanf(sf, "%f %f %d %d %d %d", &step, &raw, &a, &b, &l, &r) == 6) {
                s2_car_state c;
                s2_controls(a, b, l, r, 0, 0);
                s2_frame_times(raw);
                s2_frame(step);
                if (getenv("OS_S2ASKLOG")) s2_peek(0, 0x150, 5);
                if (getenv("OS_S2CAMLOG")) { s2_camera_state v; s2_camera(&v); printf("C %d camera at %d %d %d angles %.4f %.4f %.4f\n", f, v.x, v.height, v.y, (double)v.angle[0], (double)v.angle[1], (double)v.angle[2]); }
                printf("F %d", f++);
                for (int k = 0; k < ncars; k++) { s2_car(k, &c); printf(" | %d %d %d %.4f %.2f %.5f %.5f", c.x, c.height, c.y, (double)c.angle[1], (double)c.speed, (double)c.angle[0], (double)c.angle[2]); }
                printf("\n");
            }
            return 0;
        }
        for (int f = 0; f <= frames; f++) {
            s2_car_state c;
            if (f % (getenv("OS_S2EVERY") ? atoi(getenv("OS_S2EVERY")) : 25) == 0) {
                if (getenv("OS_S2STATE")) s2_state();
                s2_car(0, &c);
                printf("s2 %4d  at %7d %7d  height %6d  heading %6.3f  tilt %6.3f %6.3f  speed %8.2f  gear %d  rpm %6.0f  steer %5.2f  look %04x\n",
                       f, c.x, c.y, c.height, c.angle[1], c.angle[0], c.angle[2], c.speed, c.gear, c.rpm, c.steer, c.look);
                for (int k = 1; k < ncars; k++) {
                    s2_car(k, &c);
                    printf("   car %d  at %7d %7d  height %6d  heading %6.3f  speed %8.2f  gear %d\n", k, c.x, c.y, c.height, c.angle[1], c.speed, c.gear);
                }
                s2_car(0, &c);
                if (getenv("OS_S2WHEELS"))
                    printf("        wheels on the ground %d %d %d %d  ground at %d %d %d %d  kinds %d %d %d %d\n", c.wheel_down[0], c.wheel_down[1],
                           c.wheel_down[2], c.wheel_down[3], c.wheel_ground[0], c.wheel_ground[1], c.wheel_ground[2], c.wheel_ground[3],
                           c.wheel_kind[0], c.wheel_kind[1], c.wheel_kind[2], c.wheel_kind[3]);
            }
            if (getenv("OS_S2TRACE") && f < atoi(getenv("OS_S2TRACE"))) {
                unsigned from = 0x574; int n = 28;
                if (getenv("OS_S2PEEK")) sscanf(getenv("OS_S2PEEK"), "%x,%d", &from, &n);
                printf("frame %d\n", f); s2_peek(getenv("OS_S2PEEKCAR") ? atoi(getenv("OS_S2PEEKCAR")) : 0, from, n);
            }
            if (getenv("OS_S2PLAN")) {                   /* a test: "A:4,AL:0.6,A:3" = keys for so many seconds, one after another */
                static char now[16];
                const float tnow = (float)f * (getenv("OS_S2STEP") ? (float)atof(getenv("OS_S2STEP")) : s2_step());
                float from = 0.0f;
                for (const char *p = getenv("OS_S2PLAN"); *p; ) {
                    char k[16] = ""; float d = 0.0f; int n = 0;
                    if (sscanf(p, "%15[^:]:%f%n", k, &d, &n) < 2) break;
                    if (tnow >= from) snprintf(now, sizeof now, "%s", k);
                    from += d; p += n; if (*p == ',') p++;
                }
                keys = now;
            }
            if (getenv("OS_S2KEYS2") && (float)f * (getenv("OS_S2STEP") ? (float)atof(getenv("OS_S2STEP")) : s2_step()) >= (float)atof(getenv("OS_S2KEYS2AT") ? getenv("OS_S2KEYS2AT") : "0")) keys = getenv("OS_S2KEYS2");   /* a test: other keys from so many seconds on */
            s2_controls(strchr(keys, 'A') != NULL, strchr(keys, 'B') != NULL, strchr(keys, 'L') != NULL, strchr(keys, 'R') != NULL, 0, 0);
            s2_frame(getenv("OS_S2STEP") ? (float)atof(getenv("OS_S2STEP")) : s2_step());
            if (getenv("OS_S2LOG")) {                    /* every frame, every car: place, speed; "touched" */
                printf("f %d touch %d", f, s2_touched());
                for (int k = 0; k < ncars; k++) { s2_car(k, &c); printf(" | %d %d %.1f", c.x, c.y, (double)c.speed); }
                printf("\n");
            }
            if (getenv("OS_S2LOG")) {                    /* every frame, every car: place, speed, "touched" */
                printf("f %d touch %d", f, s2_touched());
                for (int k = 0; k < ncars; k++) { s2_car(k, &c); printf(" | %d %d %.1f", c.x, c.y, c.speed); }
                printf("\n");
            }
        }
        return 0;
    }
    if (g_scene && g_ncar >= 0 && car_mesh_ok[0]) {
        g_nspec = cars_dtr_load(g_spec, CAR_SPECS_MAX, "build/dec/GEN/CARS.DTR");
        if (g_nspec == 0) fprintf(stderr, "note: no build/dec/GEN/CARS.DTR -- no driving\n");
        guides_build(r);
        walls_prepare();
        snprintf(path_, sizeof path_, "%s/MAP%d.MRK", data, track);
        markers_load(path_);
        if (g_mode == MODE_SLALOM) gates_load(data, track);
        hud_load();
        sound_load(data, track);
        snprintf(path_, sizeof path_, "%s/MAP%d%c.DTR", data, track, 'A' + difficulty());
        track_data_load(path_);
        markers_reset();
        player_car(r, getenv("OS_CAR") ? atoi(getenv("OS_CAR")) : 0);
        opponents_init(r, data, track);
        smoke_init(gfxdir);
        props_init(r, data, gfxdir, track);
        attack_init(track);
        if (g_mode == MODE_ATTACK) race_restart(r);        /* the grid place goes by the recording */
        anims_init(data, gfxdir, track);
        trackside_init(data, track);
    }

    /* SCREAMER 2's lens is wider: 112 where Screamer's is 128 -- measured, not read: the port's
       picture of the very state of one of the original's (tools/s2replay.py's script, the same
       camera to the unit) was 1 / 0.871 the size about the same eye line (NOTES2 S2-5y) */
    g_focal_x = (getenv("OS_FOCAL") ? (float)atof(getenv("OS_FOCAL")) : os_game() == 2 ? 112.0f : GAME_FOCAL) * (float)g_s;
    g_focal_y = g_focal_x / (float)g_yd;
#ifdef MISTER_HYBRID
    g_in_race = 1;
#endif
    float base_pitch = -0.10f;

    fb  = malloc(sizeof(uint32_t) * FB_MAXW * (FB_MAXH + TALL_ROWS));     /* (the biggest: the size can change as it runs) */
    zb  = malloc(sizeof(float)    * FB_MAXW * (FB_MAXH + TALL_ROWS));
    idb = malloc(sizeof(int16_t)  * FB_MAXW * (FB_MAXH + TALL_ROWS));
    pxi = malloc(sizeof(int16_t)  * FB_MAXW * (FB_MAXH + TALL_ROWS));
    if (g_s == 2 && (!fb || !zb || !idb || !pxi)) res_fail("no memory for the bigger picture");

    /* OS_WALLCHECK=1: for every wall segment, is the track really there just
       inside it?  The collision only works if the drivable area reaches the
       wall line (to within the 30 units a corner is put back by). */
    if (getenv("OS_WALLCHECK") && g_scene) {
        int bad = 0;
        for (int i = 0; i < g_scene->nwall; i++) {
            const scene_wall *w = &g_scene->wall[i];
            float nx = cosf(w->angle), ny = -sinf(w->angle);
            int in30 = on_track(w->x - nx * 30.0f, w->y - ny * 30.0f);
            int out30 = on_track(w->x + nx * 30.0f, w->y + ny * 30.0f);
            int in200 = on_track(w->x - nx * 200.0f, w->y - ny * 200.0f);
            int out200 = on_track(w->x + nx * 200.0f, w->y + ny * 200.0f);
            if (!(in30 || out30) || in200 == out200) {
                if (bad < 12)
                    printf("wall %d at (%.0f, %.0f): track at -200 %d, -30 %d, +30 %d, +200 %d\n",
                           i, w->x, w->y, in200, in30, out30, out200);
                bad++;
            }
        }
        printf("wall check: %d of %d segments do not have the track edge on them\n",
               bad, g_scene->nwall);
    }

    /* OS_WHATAT=x,y: every piece with a floor over that point */
    if (getenv("OS_WHATAT") && g_scene) {
        float wx = 0, wy = 0;
        sscanf(getenv("OS_WHATAT"), "%f,%f", &wx, &wy);
        for (int o = 0; o < g_scene->count; o++) {
            const scene_object *so = &g_scene->obj[o];
            if (!so->m) continue;
            float dx = so->x - wx, dy = so->y - wy;
            if (dx * dx + dy * dy > so->radius * so->radius) continue;
            if (piece_covers(so, wx, wy))
                printf("at (%.0f, %.0f): V%d  run %d  pos (%.0f, %.0f) height %.0f  "
                       "%d faces, first material %d\n", wx, wy, so->v, so->layer,
                       so->x, so->y, so->z, so->m->nface, so->m->material[0] / 16);
        }
    }

    /* OS_MATDUMP=<routine>: every piece with a face of that material routine (160 = reflection) */
    if (getenv("OS_MATDUMP") && g_scene) {
        const int want = atoi(getenv("OS_MATDUMP"));
        for (int o = 0; o < g_scene->count; o++) {
            const scene_object *so = &g_scene->obj[o];
            if (!so->m) continue;
            int n = 0, mi = -1;
            for (int f = 0; f < so->m->nface; f++) {
                const int k = so->m->material[f] / 16;
                if (k >= 0 && k < MAT_MAX && g_mat[k].type == want) { n++; mi = k; }
            }
            if (n) printf("matdump: V%d run %d pos (%.0f, %.0f) height %.0f heading %d: %d of %d faces, material %d (%dx%d), flags 0x%x, normals %d\n",
                          so->v, so->layer, so->x, so->y, so->z, so->t, n, so->m->nface, mi, g_mat[mi].su, g_mat[mi].sv, so->m->nblock > 0 ? so->m->block[0].flags : 0, so->m->nblock > 0 ? so->m->block[0].has_vnormal : 0);
        }
    }
    /* OS_FLATDUMP=1: every small flat piece (four faces at most, all its corners at one height) */
    if (getenv("OS_FLATDUMP") && g_scene) {
        for (int o = 0; o < g_scene->count; o++) {
            const scene_object *so = &g_scene->obj[o];
            const mesh *mm = so->m;
            if (!mm || mm->nface > 4 || mm->nvert <= 0) continue;
            float lo = 1e9f, hi = -1e9f, w0 = 1e9f, w1 = -1e9f, l0 = 1e9f, l1 = -1e9f;
            for (int v = 0; v < mm->nvert; v++) {
                if (mm->vert[v].y < lo) lo = mm->vert[v].y; if (mm->vert[v].y > hi) hi = mm->vert[v].y;
                if (mm->vert[v].x < w0) w0 = mm->vert[v].x; if (mm->vert[v].x > w1) w1 = mm->vert[v].x;
                if (mm->vert[v].z < l0) l0 = mm->vert[v].z; if (mm->vert[v].z > l1) l1 = mm->vert[v].z;
            }
            if (hi - lo > 4.0f) continue;
            const int k = mm->material[0] / 16;
            printf("flatdump: V%d run %d pos (%.0f, %.0f) height %.0f+%.0f heading %d: %d faces, %.0f x %.0f, material %d routine %d (%dx%d)\n",
                   so->v, so->layer, so->x, so->y, so->z, lo, so->t, mm->nface, w1 - w0, l1 - l0, k,
                   k >= 0 && k < MAT_MAX ? g_mat[k].type : -1, k >= 0 && k < MAT_MAX ? g_mat[k].su : 0, k >= 0 && k < MAT_MAX ? g_mat[k].sv : 0);
        }
    }
    /* OS_HOLES=1: walk the whole lap looking for HOLES in the drivable area --
       points that are off the track but have track on both sides of them
       across the road.  A hole is an invisible wall. */
    /* OS_STACKED=1: where do drivable faces lie one over another?  There the
       game's "first one found" (track_under) and any other rule can disagree.
       Every drivable face over each point of a grid across the road is
       measured against the game's answer. */
    if (getenv("OS_STACKED") && g_scene) {
        int points = 0, stacked = 0, shown = 0;
        float worst = 0.0f;
        for (int i = 0; i < r->count; i++) {
            int j = (i + 1) % r->count;
            float hx = r->p[j][0] - r->p[i][0], hy = r->p[j][1] - r->p[i][1];
            float hl = sqrtf(hx * hx + hy * hy);
            if (hl < 1.0f) continue;
            float sx = hy / hl, sy = -hx / hl;
            for (float d = -1500.0f; d <= 1500.0f; d += 100.0f) {
                float x = r->p[i][0] + sx * d, y = r->p[i][1] + sy * d, first;
                if (!track_under(x, y, 1, &first)) continue;
                points++;
                float far = 0.0f;
                for (int o = 0; o < g_scene->count; o++) {
                    const scene_object *so = &g_scene->obj[o];
                    const mesh *mm = so->m;
                    if (so->layer != 0 || !mm) continue;
                    float c = cosf(so->yaw), sn = sinf(so->yaw);
                    float lx = (x - so->x) * c + (y - so->y) * sn, lz = -(x - so->x) * sn + (y - so->y) * c;
                    for (int f = 0; f < mm->nface; f++) {
                        int nc = mm->ncorner[f], pos = 0, neg = 0;
                        if (fabsf(mm->normal[f].y) < 0.05f) continue;
                        for (int k = 0; k < nc; k++) {
                            const vec3 *a = &mm->vert[mm->idx[f][k]], *b = &mm->vert[mm->idx[f][(k + 1) % nc]];
                            float cr = (b->x - a->x) * (lz - a->z) - (b->z - a->z) * (lx - a->x);
                            if (cr > 0) pos++; else if (cr < 0) neg++;
                        }
                        if (pos && neg) continue;
                        const vec3 *v0 = &mm->vert[mm->idx[f][0]];
                        float h = so->z + v0->y - ((lx - v0->x) * mm->normal[f].x + (lz - v0->z) * mm->normal[f].z) / mm->normal[f].y;
                        if (fabsf(h - first) > fabsf(far)) far = h - first;
                    }
                }
                if (fabsf(far) > 60.0f) {
                    stacked++;
                    if (shown++ < 12)
                        printf("stacked: t=%d  (%.0f, %.0f)  the game stands at %.0f, another drivable face is %+.0f from it\n",
                               i, x, y, first, far);
                }
                if (fabsf(far) > fabsf(worst)) worst = far;
            }
        }
        printf("drivable faces one over another: %d of %d points (more than 60 units apart); furthest %+.0f\n",
               stacked, points, worst);
    }

    if (getenv("OS_HOLES")) {
        int holes = 0;
        for (int i = 0; i < r->count; i++) {
            int j = (i + 1) % r->count;
            float hx = r->p[j][0] - r->p[i][0], hy = r->p[j][1] - r->p[i][1];
            float hl = sqrtf(hx * hx + hy * hy);
            if (hl < 1.0f) continue;
            float sx = hy / hl, sy = -hx / hl;
            for (int q = 0; q < 4; q++) {                 /* four lines per sample */
                float bx = r->p[i][0] + hx * q / 4.0f, by = r->p[i][1] + hy * q / 4.0f;
                int state = 0, first = 0;                  /* 0 before, 1 on, 2 gap */
                float gap_at = 0.0f;
                for (float d = -5000.0f; d <= 5000.0f; d += 20.0f) {
                    int on = on_track(bx + sx * d, by + sy * d);
                    if (on && state == 2 && !first) {
                        printf("hole: t=%d  at (%.0f, %.0f), %.0f units from the "
                               "driving line, %.0f wide\n", i, bx + sx * gap_at,
                               by + sy * gap_at, gap_at, d - gap_at);
                        holes++; first = 1;
                    }
                    if (on) state = 1;
                    else if (state == 1) { state = 2; gap_at = d; }
                }
            }
        }
        printf("holes in the drivable area: %d\n", holes);
    }

    /* OS_TRACKMASK=x,y,half: print a map of on_track() round a point, one
       character per 150 units ('#' on the track, '.' off, 'W' near a wall
       segment's middle).  For finding invisible walls and holes. */
    if (getenv("OS_TRACKMASK")) {
        float mx = 0, my = 0, half = 6000;
        sscanf(getenv("OS_TRACKMASK"), "%f,%f,%f", &mx, &my, &half);
        for (float y = my + half; y >= my - half; y -= half / 23.0f) {
            for (float x = mx - half; x <= mx + half; x += half / 47.0f) {
                char ch = on_track(x, y) ? '#' : '.';
                const scene_wall *w = nearest_wall(x, y);
                if (w && fabsf(w->x - x) < half / 47.0f && fabsf(w->y - y) < half / 23.0f) ch = 'W';
                if (fabsf(x - mx) < half / 94.0f && fabsf(y - my) < half / 46.0f) ch = '@';
                putchar(ch);
            }
            putchar('\n');
        }
    }

    if (shot) {
        camera cam;
        path_at(r, t0, cam_height, &cam);
        cam.pitch = base_pitch;
        /* OS_OVERHEAD=<h>: the same spot from h units up, looking down */
        if (getenv("OS_OVERHEAD")) {
            cam.z += (float)atof(getenv("OS_OVERHEAD")) - cam_height;
            cam.pitch = -1.45f;
        }
        /* OS_POS=x,y,z puts the shot camera at an exact spot (the numbers the
           `I` key prints), instead of on the lap */
        if (getenv("OS_POS"))
            sscanf(getenv("OS_POS"), "%f,%f,%f", &cam.x, &cam.y, &cam.z);
        if (g_s2_on && getenv("OS_S2RUN")) {
            /* SCREAMER 2: OS_S2RUN=<frames> of the race first (OS_S2KEYS: letters A B L R held),
               then the picture from behind the player's car (OS_S2LOOK=<car> another's) */
            const char *keys = getenv("OS_S2KEYS") ? getenv("OS_S2KEYS") : "A";
            s2_car_state c;
            if (getenv("OS_S2AT")) { int x = 0, y = 0; float hd = 0; sscanf(getenv("OS_S2AT"), "%d,%d,%f", &x, &y, &hd); s2_put(0, x, y, hd); }   /* (the player's car put there first) */
            if (getenv("OS_VIEW")) s2_view(g_s2_views[atoi(getenv("OS_VIEW")) % 5]);
            if (getenv("OS_S2SCRIPT")) {             /* a race recorded from the original (tools/s2replay.py), then the picture */
                FILE *sf = fopen(getenv("OS_S2SCRIPT"), "r");
                float step, raw; int ka, kb, kl, kr;
                if (getenv("OS_S2CLOCK")) s2_clock_set((float)atof(getenv("OS_S2CLOCK")));
                while (sf && fscanf(sf, "%f %f %d %d %d %d", &step, &raw, &ka, &kb, &kl, &kr) == 6) {
                    s2_controls(ka, kb, kl, kr, 0, 0);
                    s2_frame_times(raw); s2_frame(step); s2_race_read();
                }
                if (sf) fclose(sf);
            }
            else
            for (int f = atoi(getenv("OS_S2RUN")), f0 = f; f > 0; f--) {
                if (getenv("OS_S2PLAN")) {                   /* (as OS_S2PHYS takes it: "A:4,AR:2" -- keys for so many seconds) */
                    static char now[16];
                    const float tnow = (float)(f0 - f) * s2_step();
                    float from = 0.0f;
                    for (const char *p = getenv("OS_S2PLAN"); *p; ) {
                        char k[16] = ""; float d = 0.0f; int n = 0;
                        if (sscanf(p, "%15[^:]:%f%n", k, &d, &n) < 2) break;
                        if (tnow >= from) snprintf(now, sizeof now, "%s", k);
                        from += d; p += n; if (*p == ',') p++;
                    }
                    keys = now;
                }
                s2_controls(strchr(keys, 'A') != NULL, strchr(keys, 'B') != NULL, strchr(keys, 'L') != NULL, strchr(keys, 'R') != NULL, 0, 0);
                s2_race_step();
            }
            s2_race_show(1.0f);
            if (getenv("OS_S2GHOSTLOG")) s2_race_leave();        /* (a test of time attack's ghost) */
            s2_car(getenv("OS_S2LOOK") ? atoi(getenv("OS_S2LOOK")) % g_s2_on : 0, &c);
            s2_camera_show(&cam, 1.0f);
            if (getenv("OS_S2ORBIT") || getenv("OS_S2LOOK")) {
                cam.yaw = atan2f(-cosf(c.angle[1]), sinf(c.angle[1]));
                cam.x = (float)c.x - cosf(cam.yaw) * 1300.0f; cam.y = (float)c.y - sinf(cam.yaw) * 1300.0f;
                cam.z = (float)c.height + 550.0f;
                cam.pitch = -0.274f; cam.roll = 0.0f; g_hide = NULL;
            }
            if (getenv("OS_S2ANIMLOOK")) {       /* a test: OS_S2ANIMLOOK=<row>,<degrees>,<distance>,<height>: look at that animation's object */
                int row = 0; float deg = 0.0f, far_ = 3000.0f, up = 300.0f;
                sscanf(getenv("OS_S2ANIMLOOK"), "%d,%f,%f,%f", &row, &deg, &far_, &up);
                if (row >= 0 && row < 40 && g_s2_anim_is[row].on) {
                    const s2_anim *an = &g_s2_anim_is[row].s;
                    cam.yaw = deg * (float)M_PI / 180.0f;
                    cam.x = an->x - cosf(cam.yaw) * far_; cam.y = an->y - sinf(cam.yaw) * far_; cam.z = an->height + up;
                    cam.pitch = -atanf(up / far_); cam.roll = 0.0f; g_hide = NULL;
                    printf("animation %d \"%s\" at %.0f %.0f height %.0f, heading %.2f, climb %.3f\n", row, an->name, (double)an->x, (double)an->y, (double)an->height, (double)an->heading, (double)an->climb);
                } else { printf("animation %d is not to be seen now; those that are:", row); for (int k = 0; k < 40; k++) if (g_s2_anim_is[k].on) printf(" %d %s", k, g_s2_anim_is[k].s.name); printf("\n"); }
            }
            if (getenv("OS_S2ORBIT")) {          /* OS_S2ORBIT=<degrees round the car>,<distance>,<height> */
                float deg = 90.0f, far_ = 1500.0f, up = 200.0f;
                sscanf(getenv("OS_S2ORBIT"), "%f,%f,%f", &deg, &far_, &up);
                cam.yaw += deg * (float)M_PI / 180.0f;
                cam.x = (float)c.x - cosf(cam.yaw) * far_; cam.y = (float)c.y - sinf(cam.yaw) * far_;
                cam.z = (float)c.height + up;
                cam.pitch = -atan2f(up - 100.0f, far_);
            }
            printf("s2: car at %d %d height %d, speed %.1f, gear %d, road kinds under the wheels %d %d %d %d, look %04x\n", c.x, c.y, c.height, c.speed, c.gear,
                   c.wheel_kind[0], c.wheel_kind[1], c.wheel_kind[2], c.wheel_kind[3], c.look);
        }
        if (getenv("OS_HEADING"))
            cam.yaw = (float)atof(getenv("OS_HEADING")) * (float)M_PI / 180.0f;
        if (getenv("OS_YAW"))   cam.yaw   += (float)atof(getenv("OS_YAW")) * (float)M_PI / 180.0f;
        if (getenv("OS_PITCH")) cam.pitch  = (float)atof(getenv("OS_PITCH")) * (float)M_PI / 180.0f;
        s2_render(&cam, &sk);
        if (g_s2_on && getenv("OS_S2RUN") && !getenv("OS_NOHUD") && !getenv("OS_S2ORBIT")) {
            if (getenv("OS_S2SPRITES")) {             /* a test: the sprites from number n on, each with its number */
                int n = atoi(getenv("OS_S2SPRITES")), x = 4, y = 4, tall = 0;
                for (; n < g_nspr; n++) {
                    char num[8];
                    const int w = g_spr[n].w / g_s + 14, hh = g_spr[n].h / g_s + 2;
                    if (x + w > 316) { x = 4; y += tall + 2; tall = 0; }
                    if (y + hh > 196) break;
                    hud_sprite_draw(x - g_spr[n].ox / g_s, y - g_spr[n].oy / g_s, n);
                    snprintf(num, sizeof num, "%d", n);
                    hud_text(x + g_spr[n].w / g_s + 1, y, num, 3, 1, 1);
                    x += w + 8; if (hh > tall) tall = hh;
                }
            }
            else s2_hud_draw(10.0f);                  /* (long enough for the dial to have settled) */
        }
        if (getenv("OS_INSPECT")) describe_under_crosshair();
        write_ppm(shot);
        return 0;
    }

    SDL_Renderer *ren = NULL;
    SDL_Window *win = sys_window(&ren);
    if (!win || !ren) return 1;
    SDL_Texture *tex = sys_texture(FB_W, FB_H);
    (void)tex;

    if (os_once(3))
        printf("\nCONTROLS\n"
               "  %s / %s   accelerate / brake      %s / %s  steer\n"
               "  %s / %s   gear up / down (manual cars); A / Z too\n"
               "  game controller: stick or d-pad steer, triggers or A / B accelerate / brake,\n"
               "                   shoulders change gear, Y next view, START enter, BACK escape\n"
               "  C               next view;  F1 bumper cam, F2 cockpit, F3 near chase, F4 far chase\n"
               "  M               radar zoom              P   the CD player\n"
               "  BACKSPACE       back onto the road      ESC leave (asks first)\n"
               "  F5              FPS counter             F11 full screen    F12 screenshot\n\n",
               controls_key_name(0), controls_key_name(1), controls_key_name(2), controls_key_name(3),
               controls_key_name(4), controls_key_name(5));
    if (os_game() == 2 && os_once(5))
        printf("SCREAMER 2\n"
               "  M (or R)        the mirror\n"
               "  in a replay:    F9 the trackside cameras, F11 the next car, ESC its end\n\n");
    if (g_dev && os_once(4))
        printf("DEVELOPER KEYS (-d)\n"
               "  TAB           change mode:  DRIVE -> RIDE -> WALK -> FLY   (4 / 1 / 2 / 3)\n"
               "  RIDE   UP / DOWN faster / slower, P pause     WALK / FLY   W A S D, SHIFT, CTRL, SPACE, E / Q\n"
               "  mouse click   look around (ESC gives the mouse back)\n"
               "  [  /  ]       back / forward along the lap    HOME  the start\n"
               "  V  from above     I  what is in the middle of the screen     G  wall segments\n"
               "  K  back faces     L  record run 0 / 1 / both                 N  next car\n\n");
    fflush(stdout);

    /*
     * Scale: a car is 1476 units for 4.5 m, so 1 m is about 328 units.
     */
    const float M = 328.0f;
    const float EYE        = 1.7f * M;      /* eye height on foot */
    const float WALK_SPEED = 6.0f * M;      /* a brisk jog; a real 1.4 m/s walk
                                               takes ten minutes a lap */
    const float FLY_SPEED  = 30.0f * M;
    const float GRAVITY    = 9.8f * M * 2.0f;   /* a little heavy: feels better */
    const float STEP_UP    = 0.6f * M;      /* kerbs and steps climbed freely */
    const float JUMP       = 5.5f * M;

    enum { RIDE, WALK, FLY, DRIVE };
    static const char *mode_name[] = { "RIDE", "WALK", "FLY", "DRIVE" };
    const int can_drive = (g_pobj != NULL);
    int mode = can_drive ? DRIVE : RIDE;
    float phys_acc = 0.0f;
    int shift_key = 0;
    camera cam = { 0, 0, 0, 0, base_pitch, 0 };
    float t = t0;
    float ride_speed = 60.0f;               /* path samples per second */
    int paused = 0;
    float feet = 0.0f, vz = 0.0f;           /* WALK: height of the feet, fall speed */
    int on_ground = 1;
    int running = 1, mouse = 0, overhead = 0, shots = 0;
    /* a record's replay, chosen in VIEW RECORDS (0x291f7): the race is set up
       as it was and run from the file; when it runs out, back to the menu */
    if (g_intro) intro_init(data, track);
    if (!g_intro && getenv("OS_REPLAYFILE") && replay_read(getenv("OS_REPLAYFILE"))) {
        g_rpl_file = 1; g_replay = 1; g_rpl_view = -1;
        race_restart(r);
    }
    float overhead_height = 12000.0f;
    path_at(r, t, cam_height, &cam);

    Uint64 last = SDL_GetPerformanceCounter();
    const double tick = 1.0 / (double)SDL_GetPerformanceFrequency();
    while (running) {
        /* (the frame's time from the fine clock: in whole milliseconds a 180 Hz screen's
           frames are 5 and 6 by turns) */
        const Uint64 frame_began = SDL_GetPerformanceCounter();
        float dt = (float)((double)(frame_began - last) * tick);
        last = frame_began;
        if (dt > 0.1f) dt = 0.1f;

        int new_mode = -1, jump = 0, hop = 0;
        /*
         * OS_DEMO=<frames>: a headless self-test of WALK.  Walks forward from
         * the start for that many 1/60 s frames (jumping once), prints how far
         * the eyes are above the ground, saves demo.ppm and exits.  Run it
         * with SDL_VIDEODRIVER=dummy; it is how the walking code gets checked
         * without a screen.
         */
        static int demo_frame = 0;
        const int demo = getenv("OS_DEMO") ? atoi(getenv("OS_DEMO")) : 0;
        /* OS_DRIVE_DEMO=<frames>: the same idea for DRIVE -- a simple robot
           driver takes the car round and the console shows speed, gear, rpm
           and wall hits.  Also run with SDL_VIDEODRIVER=dummy. */
        const int ddemo = getenv("OS_DRIVE_DEMO") ? atoi(getenv("OS_DRIVE_DEMO")) : 0;
        const int autopilot = (ddemo != 0) && !getenv("OS_PADTEST");   /* OS_PADTEST: a pretended controller drives (controls.c) */
        {
            static int view_set = 0;                 /* once, not every frame */
            if (ddemo && getenv("OS_VIEW") && !view_set) {
                g_view = atoi(getenv("OS_VIEW")) % PLAYER_VIEWS;
                view_set = 1;
            }
        }
        /* OS_VIEWCHANGE="frame,view": change view at that frame, to test the fly-across */
        if (ddemo && getenv("OS_VIEWCHANGE")) {
            static int vc_frame = -1, vc_view = 0, vc_now = 0;
            if (vc_frame < 0) sscanf(getenv("OS_VIEWCHANGE"), "%d,%d", &vc_frame, &vc_view);
            if (vc_now++ == vc_frame) view_change(vc_view);
        }
        if (ddemo) dt = 1.0f / 50.0f;
        /* OS_BENCH=1: time stands still -- the same picture (the grid) over and
           over, for measuring how long a picture takes */
        { static int bench = -1; if (bench < 0) bench = getenv("OS_BENCH") != NULL; if (bench) { dt = 0.0f; g_fade_in = 1.0f; } }
        if (demo) {
            dt = 1.0f / 60.0f;
            if (demo_frame == 0) new_mode = WALK;
            if (demo_frame == demo / 2) jump = 1;
        }
        /* the controller's buttons are the car's while it is being driven, the menus' otherwise */
        controls_menu_mode(mode != DRIVE || g_dialog || g_replay || g_intro || g_cd.on || g_finished || g_game_over);
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) { running = 0; g_window_closed = 1; }
            if (ev.type == SDL_MOUSEBUTTONDOWN && !mouse && g_dev) {
                mouse = 1;
                SDL_SetRelativeMouseMode(SDL_TRUE);
                if (mode == RIDE) new_mode = WALK;
            }
            if (ev.type == SDL_MOUSEMOTION && mouse) {
                const float sens = 0.003f;
                cam.yaw   -= (float)ev.motion.xrel * sens;
                cam.pitch -= (float)ev.motion.yrel * sens;
            }
            if (ev.type != SDL_KEYDOWN) continue;
            if (g_s2_pz.on) {                            /* the race's pause screen (0x49f8e) takes the keys */
                const SDL_Keycode k = ev.key.keysym.sym;
                if (k != SDLK_F12 && k != SDLK_F11 && k != SDLK_F5) { if (s2_pause_key(k)) running = 0; continue; }
            }
            if (g_s2_on && (s2_mode & 0x80)) {           /* the attract race: ENTER or ESC ends it (0x37ea9), nothing else is taken */
                const SDL_Keycode k = ev.key.keysym.sym;
                if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE || k == SDLK_ESCAPE) { if (!g_s2_att.end) g_s2_att.end = 1; continue; }
                if (k != SDLK_F12 && k != SDLK_F11 && k != SDLK_F5) continue;      /* (the port's own: a picture, full screen, FPS) */
            }
            if (g_s2_on && g_s2_end_t >= 4.0f) {        /* SCREAMER 2, the race over: the choices */
                const SDL_Keycode k = ev.key.keysym.sym;
                const int replay = s2_replay_offered(), last = (s2_mode == 2 ? 0 : 1) + replay;
                if (k == SDLK_LEFT && g_s2_end_sel > 0) { g_s2_end_sel--; continue; }
                if (k == SDLK_RIGHT && g_s2_end_sel < last) { g_s2_end_sel++; continue; }
                if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
                    const int pick = g_s2_end_sel + !replay;        /* 0 REPLAY, 1 RACE AGAIN or CONTINUE, 2 EXIT */
                    if (s2_mode == 2 && !s2_replaying() && s2_champ() && s2_champ()->on) {    /* (0x47e75: a league race's places taken as ENTER is, not from its replay) */
                        s2_champ()->nres = s2_results(s2_champ()->res);
                        s2_champ()->report = 1;
                    }
                    if (!s2_replaying() && getenv("OS_S2MENU3D") && getenv("OS_FROM_MENU")) s2_race_results_keep();   /* (the game's menu: the race's result as ENTER is, 0x47e75) */
                    s2_replay_take(pick == 0);
                    if (pick == 0 || (pick == 1 && s2_mode != 2)) s2_race_again();     /* (a league's race: CONTINUE, to the report) */
                    running = 0;
                    continue;
                }
            }
            if (g_cd.on) { cd_key(ev.key.keysym.sym); if (ev.key.keysym.sym == SDLK_F5) fps_toggle(); continue; }
            if (g_intro) {                       /* 0x211d0: ESC, ENTER or SPACE ends it */
                int k = ev.key.keysym.sym;
                if (k == SDLK_ESCAPE || k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) g_in.st |= 2;
                if (k == SDLK_F12) { write_ppm("intro.ppm"); printf("saved intro.ppm\n"); }
                if (k == SDLK_F5) fps_toggle();
                continue;
            }
            /* the developer's keys are only there with -d */
            if (!g_dev)
                switch (ev.key.keysym.sym) {
                case SDLK_TAB: case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4: case SDLK_n:
                case SDLK_SPACE: case SDLK_LEFTBRACKET: case SDLK_RIGHTBRACKET: case SDLK_HOME:
                case SDLK_v: case SDLK_i: case SDLK_g: case SDLK_k: case SDLK_l:
                    continue;
                default: break;
                }
            switch (ev.key.keysym.sym) {
            case SDLK_ESCAPE:
                if (mouse) { mouse = 0; SDL_SetRelativeMouseMode(SDL_FALSE); }
                else if (g_replay) g_rpl_at = g_rpl_n;                /* skip the replay */
                else if (g_s2_on && s2_replaying()) { if (!g_s2_end_t) s2_replay_stop(); }     /* (Screamer 2's: 0x2d7c0, the rest not run) */
                else if (g_s2_on) { if (!s2_player_done() && !g_s2_end_t) s2_pause_open(); }  /* (0x2d7c0: the pause screen; nothing once the car has finished) */
                /* 0x1fe86: leaving a race is answered with PLAY AGAIN? over the
                   darkened picture, in every mode but the network game -- the
                   championship too, when the race was left or lost on time.
                   ESC on the question itself is NO. */
                else if (mode == DRIVE && !g_dialog && !getenv("OS_DRIVE_DEMO")) { g_dialog = 1; g_dialog_sel = 0; }
                else running = 0;
                break;
            case SDLK_TAB: new_mode = (mode + 1) % (can_drive ? 4 : 3); break;
            case SDLK_1:   new_mode = RIDE; break;
            case SDLK_2:   new_mode = WALK; break;
            case SDLK_3:   new_mode = FLY;  break;
            case SDLK_4:   if (can_drive) new_mode = DRIVE; break;
            /* 0x211d0: the radar's key -- 24, 48, 72, off, and round again */
            /* (Screamer 2's mirror: M, the game's 0x2d7c0 -- R as well, the port's first key for it) */
            case SDLK_r:   if (g_s2_on) { g_s2_mirror_on = !g_s2_mirror_on; s2_mirror_set(g_s2_mirror_on); } break;
            case SDLK_m:
                if (g_s2_on) { g_s2_mirror_on = !g_s2_mirror_on; s2_mirror_set(g_s2_mirror_on); }
                else g_radar_zoom = g_radar_zoom >= 72 ? 0 : g_radar_zoom + 24;
                break;
            case SDLK_F9:  if (g_s2_on && s2_replaying()) s2_view(1); break;          /* 0x132b4: a replay's trackside cameras */
            case SDLK_n:   if (mode == DRIVE) player_car(r, g_spec_now + 1); break;
            case SDLK_BACKSPACE:
                if (mode == DRIVE && !g_replay) g_reset_key = 1;     /* done in the next step, so the replay has it */
                break;
            case SDLK_RETURN:
                /* in the championship a race is run once: ENTER leaves it when it is over */
                if (g_dialog) {                  /* YES: the race again.  NO: leave */
                    if (g_dialog_sel == 0) {
                        if (records_ok) records_enter(track);        /* 0x1febf */
                        g_replay = 0; g_replay_seen = 0; race_restart(r);
                    }
                    else running = 0;
                }
                /* (a finished championship race leaves by itself after the end
                   screen's five seconds; one that ran out of time gets the question) */
                else if (g_replay) { g_rpl_at = g_rpl_n; }            /* skip the rest of the replay */
                /* ENTER does nothing else in a race.  It once restarted it (a
                   testing key of the port's), then skipped the end screen; in the
                   original the five seconds of the end screen cannot be cut short
                   (Danik, 2026-10-09; 0x21048 only counts them down). */
                break;
            case SDLK_p:
                if (mode == DRIVE && !g_dialog && !g_replay) cd_open();     /* 0x211d0 */
                else paused = !paused;
                break;
            case SDLK_SPACE: jump = 1; break;
            case SDLK_LEFTBRACKET:  hop = -25; break;
            case SDLK_RIGHTBRACKET: hop = +25; break;
            case SDLK_HOME: hop = -(int)t - 1000000; break;
            case SDLK_UP:
                if (g_dialog) { g_dialog_sel = 0; snd_play(SND_CLICK); }
                if (mode == RIDE) { ride_speed *= 1.25f; if (ride_speed > 600) ride_speed = 600; }
                break;
            case SDLK_DOWN:
                if (g_dialog) { g_dialog_sel = 1; snd_play(SND_CLICK); }
                if (mode == RIDE) { ride_speed *= 0.8f; if (ride_speed < 2) ride_speed = 2; }
                break;
            case SDLK_v:
                overhead = !overhead;
                if (!overhead) cam.pitch = base_pitch;
                break;
            case SDLK_i: describe_under_crosshair(); break;
            case SDLK_g: g_show_edges = !g_show_edges; break;
            case SDLK_c:
                if (g_replay) {                  /* trackside, then the four driving views */
                    g_rpl_view = (g_rpl_view + 2) % (PLAYER_VIEWS + 1) - 1;
                    if (g_rpl_view >= 0) view_change(g_rpl_view);
                }
                else if (mode == DRIVE) {
                    int to = (g_view + 1) % PLAYER_VIEWS;
                    if (to == 1 && (g_cheat & 0x20)) to = 2;         /* 0x19b9e: no cockpit upside down */
                    view_change(to);
                }
                else if (g_s2_on) s2_view(g_s2_views[g_s2_view = (g_s2_view + 1) & 3]);
                break;
            case SDLK_F5: fps_toggle(); break;
            case SDLK_F1: case SDLK_F2: case SDLK_F3: case SDLK_F4:
                if (mode == DRIVE) view_change((ev.key.keysym.sym - SDLK_F1) % PLAYER_VIEWS);
                if (g_s2_on) s2_view(g_s2_views[g_s2_view = (int)(ev.key.keysym.sym - SDLK_F1)]);
                break;
            case SDLK_k:
                g_cull = !g_cull;
                printf("back-face culling: %s\n", g_cull ? "on" : "off");
                fflush(stdout);
                break;
            case SDLK_l:
                g_layer = (g_layer == 3) ? 0 : (g_layer == 0) ? 1 : 3;
                printf("showing record run: %s\n",
                       g_layer == 3 ? "both" : g_layer == 0 ? "0" : "1");
                fflush(stdout);
                break;
            case SDLK_F11: {
                if (g_s2_on && s2_replaying()) { s2_replay_next_car(); break; }     /* (in Screamer 2's replay the game's: the next car, 0x2d7c0) */
                Uint32 fs = SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP;
                SDL_SetWindowFullscreen(win, fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                break;
            }
            case SDLK_F12: {
                char name[64];
                snprintf(name, sizeof name, "shot_%03d.ppm", shots++);
                write_ppm(name);
                shot_report(name, &cam);
                printf("saved %s  (t=%d)\n", name, path_nearest(r, cam.x, cam.y));
                fflush(stdout);
                break;
            }
            default: break;
            }
        }
        const Uint8 *keys = SDL_GetKeyboardState(NULL);

        /* [ ] HOME: move along the lap, in whatever mode */
        if (hop) {
            int at = (mode == RIDE) ? (int)t : path_nearest(r, cam.x, cam.y);
            at = (hop < -100000) ? 0 : ((at + hop) % r->count + r->count) % r->count;
            t = (float)at;
            float keep_pitch = cam.pitch;
            path_at(r, t, 0.0f, &cam);
            feet = cam.z; vz = 0.0f;
            cam.z = feet + ((mode == WALK) ? EYE : cam_height);
            cam.pitch = keep_pitch;
        }

        if (new_mode >= 0 && new_mode != mode) {
            cam.roll = 0.0f;
            g_hide = NULL;
            g_chase.init = 0;
            g_swoop.on = 0;
            g_vh = FB_H;
            g_cy = 0.65f * FB_H;
            if (new_mode == RIDE) {
                /* pick the lap up from wherever we are standing */
                t = (float)path_nearest(r, cam.x, cam.y);
                cam.pitch = base_pitch;
                overhead = 0;
            }
            if (new_mode == WALK) {
                /* stand on whatever is under the camera */
                feet = cam.z - EYE;
                float g;
                if (surface_pick(cam.x, cam.y, cam.z, 1, &g)) feet = g;
                vz = 0.0f;
            }
            mode = new_mode;
        }

        /* turning with the keyboard works everywhere except while riding */
        if (mode != RIDE && mode != DRIVE) {
            float turn = 1.8f * dt;
            if (keys[SDL_SCANCODE_LEFT])     cam.yaw   += turn;
            if (keys[SDL_SCANCODE_RIGHT])    cam.yaw   -= turn;
            if (keys[SDL_SCANCODE_PAGEUP])   cam.pitch += turn * 0.6f;
            if (keys[SDL_SCANCODE_PAGEDOWN]) cam.pitch -= turn * 0.6f;
        }
        if (cam.pitch >  1.50f) cam.pitch =  1.50f;
        if (cam.pitch < -1.50f) cam.pitch = -1.50f;

        float fwd = (float)(keys[SDL_SCANCODE_W] - keys[SDL_SCANCODE_S]);
        float strafe = (float)(keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A]);
        float lift = (float)(keys[SDL_SCANCODE_E] - keys[SDL_SCANCODE_Q]);
        float gear = 1.0f;
        if (demo) { fwd = 1.0f; gear = 4.0f; }
        if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]) gear = 4.0f;
        if (keys[SDL_SCANCODE_LCTRL]  || keys[SDL_SCANCODE_RCTRL])  gear = 0.25f;

        if (overhead) {
            overhead_height += lift * 20000.0f * gear * dt;
            if (overhead_height < 1500.0f) overhead_height = 1500.0f;
        }

        if (mode == RIDE && g_s2_on && !shot) {
            /* SCREAMER 2: the game's own race, 50 steps a second; the keys are the port's
               six controls; the view is the game's (F1-F4, C) */
            static float owed;
            controls_state ct;
            controls_read(&ct);
            owed += paused || g_s2_pz.on ? 0.0f : dt;
            if (owed > 0.25f) owed = 0.25f;
            while (owed >= s2_every()) {
                if (getenv("OS_S2REPLAYTEST") && !s2_replaying()) {    /* a test: a pattern of controls, then the race kept and its replay started */
                    static int f;
                    const int part = f / 23 % 4;
                    ct.throttle = 1.0f; ct.brake = (f / 61 % 5 == 4) ? 1.0f : 0.0f; ct.steer = part == 1 ? 1.0f : part == 3 ? -1.0f : 0.0f; ct.shift = 0; ct.up_held = ct.down_held = 0;
                    if (++f == atoi(getenv("OS_S2REPLAYTEST"))) { s2_replay_keep(); s2_replay_take(1); if (getenv("OS_S2REPLAYFILE")) s2_replay_from_file(); printf("s2: replay\n"); fflush(stdout); s2_race_again(); }
                }
                /* (the gear controls as HELD: the game changes one gear a press itself -- 0x1f756 reads
                   the key's word and waits for it to be let go, car +0x140 -- and a press handed on
                   for one picture only was lost whenever that picture ran no race step) */
                s2_controls(ct.throttle > 0.3f, ct.brake > 0.3f, ct.steer > 0.3f, ct.steer < -0.3f, ct.up_held, ct.down_held);
                s2_race_step();
                owed -= s2_every();
            }
            if (getenv("OS_S2AGAINTEST")) {              /* a test: RACE AGAIN taken by itself after so many pictures */
                static int n;
                if (++n == atoi(getenv("OS_S2AGAINTEST"))) { unsetenv("OS_S2AGAINTEST"); printf("s2: race again\n"); fflush(stdout); s2_race_again(); }
            }
            s2_race_show(owed / s2_every());
            s2_camera_show(&cam, owed / s2_every());
        } else if (mode == RIDE) {
            if (!paused) t += ride_speed * dt;
            while (t >= r->count) t -= r->count;
            float keep = cam.yaw;
            path_at(r, t, cam_height, &cam);
            /* ease the heading so the view does not snap at the joins */
            float d = cam.yaw - keep;
            while (d >  (float)M_PI) d -= 2 * (float)M_PI;
            while (d < -(float)M_PI) d += 2 * (float)M_PI;
            float ease = 8.0f * dt; if (ease > 1.0f) ease = 1.0f;
            cam.yaw = keep + d * ease;
            cam.pitch = base_pitch;
        } else if (mode == DRIVE) {
            drive_input in = { 0 };
            /* 0x24d40: the six controls, from the keys of CONTROLS.CFG and the game controller */
            controls_state ct;
            controls_read(&ct);
            in.throttle = ct.throttle;
            in.brake    = ct.brake;
            in.steer    = ct.steer;
            int in_analog = (ct.steer_analog ? 2 : 0) | (ct.brake_analog ? 4 : 0);
            if (ct.shift) shift_key = ct.shift;
            if (autopilot) in_analog = 0;
            if (g_cheat & 0x10) in.steer = -in.steer;                /* 0x1631c: mirrored picture, mirrored wheel */
            if (autopilot) {
                /* the headless test driver: aim at the road ahead, lift for bends */
                int a = (g_pat + 6) % r->count, b = (g_pat + 30) % r->count;
                float want = atan2f(r->p[a][0] - g_pc.x, -(r->p[a][1] - g_pc.y));
                float e = want - g_pc.yaw;
                while (e >  (float)M_PI) e -= 2 * (float)M_PI;
                while (e < -(float)M_PI) e += 2 * (float)M_PI;
                in.steer = e * 4.0f;
                if (in.steer > 1.0f) in.steer = 1.0f;
                if (in.steer < -1.0f) in.steer = -1.0f;
                float far = atan2f(r->p[b][0] - g_pc.x, -(r->p[b][1] - g_pc.y)) - g_pc.yaw;
                while (far >  (float)M_PI) far -= 2 * (float)M_PI;
                while (far < -(float)M_PI) far += 2 * (float)M_PI;
                float safe = 16000.0f - 30000.0f * fabsf(far);
                if (safe < 4000.0f) safe = 4000.0f;
                in.throttle = (g_pc.speed < safe) ? 1.0f : 0.0f;
                in.brake    = (g_pc.speed > safe + 1500.0f) ? 1.0f : 0.0f;
                /* OS_RAM="start,frames,steer": from sample `start`, hold the
                   wheel over and the pedal down for `frames` steps -- drive
                   into the wall on purpose -- then hand back to the robot.
                   The wall test: how deep does the car get, does it come out. */
                if (getenv("OS_RAM")) {
                    static int ram_left = -1, ram_start = 0;
                    static float ram_steer = 0;
                    if (ram_left < 0) {
                        int fr = 0;
                        sscanf(getenv("OS_RAM"), "%d,%d,%f", &ram_start, &fr, &ram_steer);
                        ram_left = fr;
                    }
                    if (ram_left > 0 && g_pat >= ram_start) {
                        in.steer = ram_steer; in.throttle = 1.0f; in.brake = 0.0f;
                        ram_left--;
                    }
                }
            }
            phys_acc += dt;
            if (g_dialog || g_cd.on) phys_acc = 0.0f;       /* the question, and the CD player, are over a still picture */
            while (phys_acc >= DRIVE_DT) {
                phys_acc -= DRIVE_DT;
                if (g_intro) {                   /* 0x2780c: only the others, the scenery and the credits */
                    g_race_clock += DRIVE_DT;
                    g_race_on = (g_race_clock >= RACE_HOLD);
                    opponents_step(r);
                    props_step();
                    anims_step();
                    intro_step(DRIVE_DT);
                    if (g_in.st == 4) running = 0;
                    continue;
                }
                in.shift = shift_key; shift_key = 0;
                if (!g_rec) g_rec = calloc(REPLAY_MAX, sizeof *g_rec);
                if (g_replay) {                  /* 0x22e74: the controls come from the recording */
                    if (g_rpl_at >= g_rpl_n) {   /* ... until it runs out: 0x22dd8 */
                        g_replay = 0; g_replay_seen = 1; g_dialog = 1; g_dialog_sel = 0;
                        if (g_rpl_file) { g_dialog = 0; running = 0; }
                        if (getenv("OS_REPLAYCHECK"))
                            printf("replay: %d steps, strayed at most %.1f units from the race\n", g_rpl_n, g_rpl_stray);
                        phys_acc = 0.0f;
                        break;
                    }
                    in = g_rec[g_rpl_at].in;
                    in_analog = g_rec[g_rpl_at].reset & 6;
                    if (g_rec[g_rpl_at].reset & 1) player_reset(r, g_pat);
                    float stray = hypotf(g_pc.x - g_rec[g_rpl_at].x, g_pc.y - g_rec[g_rpl_at].y);
                    if (stray > g_rpl_stray) g_rpl_stray = stray;
                    g_rpl_at++;
                } else if (g_rpl_n < REPLAY_MAX) {                         /* 0x22f34 */
                    g_rec[g_rpl_n].in = in;
                    g_rec[g_rpl_n].x = g_pc.x; g_rec[g_rpl_n].y = g_pc.y;
                    g_rec[g_rpl_n].reset = g_reset_key | in_analog;     /* bit 0 back on the road, bits 1-2 stick / trigger */
                    g_rpl_n++;
                    if (g_reset_key) player_reset(r, g_pat);
                    g_reset_key = 0;
                } else if (g_reset_key) { player_reset(r, g_pat); g_reset_key = 0; }
                /* 0x21048 + 0x1fd64: five seconds of the end screen, then the
                   replay of a finished normal race, then the question */
                if ((g_finished || g_game_over) && !g_replay
                    && (!autopilot || getenv("OS_REPLAYCHECK"))) {
                    g_end_clock += DRIVE_DT;
                    if (g_end_clock >= 5.0f && getenv("OS_CHAMP") && g_finished) {
                        running = 0;             /* 0x1fe86: the championship goes straight on to its standings */
                        phys_acc = 0.0f;
                        break;
                    }
                    if (g_end_clock >= 5.0f) {
                        if (g_mode == 1 && g_finished && !g_replay_seen && !getenv("OS_NOREPLAY")) {
                            g_replay = 1; g_rpl_view = -1;
                            race_restart(r);
                        } else { g_dialog = 1; g_dialog_sel = 0; }
                        phys_acc = 0.0f;
                        break;
                    }
                }
                g_race_clock += DRIVE_DT;
                g_race_on = (g_race_clock >= RACE_HOLD);
                g_pc.waiting = !g_race_on;       /* the pedal still works: it revs the engine (drive.c) */
                if (g_time_out && !getenv("OS_NOTIMER")) {
                    in.throttle = 0.0f;
                    /* 0x18a04: out of time AND stopped = the race is over */
                    if (g_pc.speed < 1.0f) g_game_over = 1;
                }
                if (g_finished || g_game_over) { in.throttle = 0.0f; in.brake = 0.4f; }
                if (getenv("OS_CRASHAT")) {     /* seconds into the race: crash on purpose */
                    static int done = 0;
                    if (!done && g_race_clock >= (float)atof(getenv("OS_CRASHAT"))) { done = 1; crash_start(); }
                }
                if (g_crash.on) { in.throttle = 0.0f; in.steer = 0.0f; }
                {
                    int was = g_crash.bounces;
                    crash_step(DRIVE_DT);
                    if (g_crash.bounces != was) g_puff_timer = 1.0f, g_crash_puff = 3;
                }
                g_pc.analog = in_analog;
                drive_step(&g_pc, &in);
                opponents_step(r);
                props_step();
                anims_step();
                player_env(r);
                chase_step();
                swoop_step();
                dash_step();
                smoke_step(DRIVE_DT);
                sound_step();
                rumble_step(DRIVE_DT);
                if (g_pobj && g_pobj->m && !getenv("OS_NOLEAN"))
                    suspension_step(&g_ppose, &g_pc, g_pobj->m, DRIVE_DT);
                if (getenv("OS_LEANDUMP") && fabsf(g_ppose.body_pitch) + fabsf(g_ppose.body_roll) > 0.0f) {
                    static int n = 0;
                    if (n++ % 10 == 0)
                        printf("  body  %.1fs  %3.0f km/h  pitch %+5.1f deg  roll %+5.1f deg  drop %+3.0f\n",
                               g_race_clock, drive_kmh(&g_pc), g_ppose.body_pitch * 57.3f,
                               g_ppose.body_roll * 57.3f, g_ppose.body_drop);
                }
                wheels_step(&g_ppose, g_pc.speed,
                            g_pc.steer_max > 0.0f ? g_pc.steer / g_pc.steer_max : 0.0f, DRIVE_DT);
            }
            chase_camera(&cam);
            if (g_intro && g_in.cam_ok) { cam = g_in.view; g_hide = g_pobj; g_vh = FB_H; }
            /* the race is over: the game's trackside camera, the car in view */
            if ((g_finished || g_game_over || (g_replay && g_rpl_view < 0)) && !getenv("OS_NOTRACKSIDE")
                && trackside_camera(&cam)) {
                g_hide = NULL;
                g_vh = FB_H;                    /* row 3 is a full-height view */
                g_cy = 0.65f * FB_H;
            }
            t = (float)g_pat;
        } else if (mode == WALK) {
            float sp = WALK_SPEED * gear * dt;
            float cy = cosf(cam.yaw), sy = sinf(cam.yaw);
            /* forward is where you face on the ground; right = (sin, -cos) */
            cam.x += (cy * fwd + sy * strafe) * sp;
            cam.y += (sy * fwd - cy * strafe) * sp;

            /*
             * GRAVITY.  The ground is the highest surface that is not above
             * knee height -- so a kerb or a step is simply climbed, and a
             * bridge overhead is ignored.  Above the ground you fall; below it
             * (a slope coming up) you are lifted.  With nothing underneath at
             * all -- off the edge of the modelled world -- you keep your
             * height rather than falling for ever.
             */
            float ground;
            if (surface_pick(cam.x, cam.y, feet + STEP_UP, 1, &ground)) {
                if (jump && on_ground) { vz = JUMP; on_ground = 0; }
                /* walking down a slope or off a kerb: stay on the ground, do
                   not float off it and drop back */
                if (on_ground && vz <= 0.0f && feet - ground < STEP_UP) feet = ground;
                if (feet > ground + 1.0f || vz > 0.0f) {
                    vz -= GRAVITY * dt;
                    feet += vz * dt;
                    on_ground = 0;
                }
                if (feet <= ground) { feet = ground; vz = 0.0f; on_ground = 1; }
            } else {
                vz = 0.0f; on_ground = 1;
            }
            /* the eyes follow the feet smoothly, so slopes and kerbs do not jolt */
            float want = feet + EYE;
            float ease = 14.0f * dt; if (ease > 1.0f) ease = 1.0f;
            cam.z += (want - cam.z) * ease;
        } else {
            float sp = FLY_SPEED * gear * dt;
            float cy = cosf(cam.yaw), sy = sinf(cam.yaw);
            float cp = cosf(cam.pitch), spi = sinf(cam.pitch);
            cam.x += (cy * cp * fwd + sy * strafe) * sp;
            cam.y += (sy * cp * fwd - cy * strafe) * sp;
            cam.z += spi * fwd * sp;
            if (!overhead) cam.z += lift * sp;
        }

        camera view = cam;
        if (overhead) {
            /* straight down on wherever we are, "up" = the way we are facing */
            view.z = cam.z + overhead_height;
            view.pitch = -1.50f;
        }

        {
            static int tick = 0;
            if ((tick++ % 6) == 0) {
                int at = (mode == RIDE) ? (int)t : path_nearest(r, cam.x, cam.y);
                char title[160];
                if (mode == DRIVE)
                    snprintf(title, sizeof title,
                             "OpenScreamer   %s   %3.0f km/h   gear %d   %4.0f rpm   POS %d/%d"
                             "   lap %d  %.1f s  (last %.2f, best %.2f)   %s%s   t=%d",
                             g_race_clock < RACE_HOLD ? "GET READY" : "DRIVE",
                             drive_kmh(&g_pc), g_pc.gear, g_pc.rpm,
                             g_player_rank, g_nopp + 1,
                             g_laps + 1, g_lap_time, g_last_lap, g_best_lap,
                             g_pc.spec.name,
                             g_pc.spin ? "  SPIN" : g_pc.slide > 0 ? "  skid" : "",
                             g_pat);
                else
                snprintf(title, sizeof title,
                         "OpenScreamer   %s%s   t=%d of %d   lap %.0f%%"
                         "      (TAB changes mode)",
                         mode_name[mode], overhead ? " + from above" : "",
                         at, r->count, 100.0f * (float)at / (float)r->count);
                SDL_SetWindowTitle(win, title);
            }
        }

        /* the animation clock (0x1fcc4): 0.3 a second, wrapping at 1 */
        g_anim_time += dt * 0.3f;
        if (g_anim_time >= 1.0f) g_anim_time -= 1.0f;
        g_anim_clock = (int)(g_anim_time * 65535.0f);
        mesh_animate_all(g_anim_clock);
        smoke_dump();
        s2_render(&view, &sk);
        /* the trackside view has no instruments (overlay 0 in the view table) */
        /* 0x1ffec: the whole picture is turned over before the instruments go on */
        if (g_cheat & 0x10)
            for (int y = 0; y < FB_H; y++)
                for (int x = 0; x < FB_W / 2; x++) {
                    uint32_t t = fb[y * FB_W + x];
                    fb[y * FB_W + x] = fb[y * FB_W + FB_W - 1 - x]; fb[y * FB_W + FB_W - 1 - x] = t;
                }
        if (g_cheat & 0x20)
            for (int y = 0; y < FB_H / 2; y++)
                for (int x = 0; x < FB_W; x++) {
                    uint32_t t = fb[y * FB_W + x];
                    fb[y * FB_W + x] = fb[(FB_H - 1 - y) * FB_W + x]; fb[(FB_H - 1 - y) * FB_W + x] = t;
                }
        if (g_intro) {
            intro_draw();
            if (getenv("OS_INTRO_TEST")) {       /* a picture every 2.5 s, eight of them, then stop */
                static int shot_n;
                if (g_in.clock >= 1.5f + 2.5f * (float)shot_n) {
                    char name[64];
                    snprintf(name, sizeof name, "intro_%d.ppm", shot_n++);
                    write_ppm(name);
                    if (shot_n >= 8) running = 0;
                }
            }
        }
        else if (mode == DRIVE && !overhead && !getenv("OS_NOHUD"))
            hud_draw(!g_swoop.on && !(g_finished || g_game_over));
        else if (mode == RIDE && g_s2_on && (s2_mode & 0x80))
            { s2_attract_draw(paused ? 0.0f : dt); if (g_s2_att.end == 2) running = 0; }   /* (the attract race: its own screen, 0x37ea9) */
        else if (mode == RIDE && g_s2_on && !overhead && !getenv("OS_NOHUD"))
            s2_hud_draw(paused || g_s2_pz.on ? 0.0f : dt);
        if (g_s2_pz.on) s2_pause_draw(dt);
        if (g_dialog) { static int dtick; dialog_draw(dtick++); }
        if (getenv("OS_CDTEST") && !g_cd.on && g_race_clock > 7.0f) {      /* a test: open it, cursor on button n */
            cd_open(); g_cd.sel = atoi(getenv("OS_CDTEST")) % 7; g_cd.track = 3;
        }
        if (g_cd.on) cd_draw(dt);
        if (getenv("OS_CYCLETEST")) { static int frames; if (++frames > 90) running = 0; }
        if (!g_intro && !g_cd.on && g_fade_in < 1.0f) {
            g_fade_in += dt * 2.0f;
            if (g_fade_in > 1.0f) g_fade_in = 1.0f;
            for (int k = 0; k < FB_W * FB_H; k++) {
                uint32_t c = fb[k];
                fb[k] = 0xff000000u | (uint32_t)((float)(c >> 16 & 255) * g_fade_in) << 16
                      | (uint32_t)((float)(c >> 8 & 255) * g_fade_in) << 8 | (uint32_t)((float)(c & 255) * g_fade_in);
            }
            volumes_apply(g_fade_in);
        }
        rumble_show(!g_replay && !g_intro && !g_finished && !g_game_over && !g_dialog && !g_cd.on);
        fps_draw();
        if (ddemo) {
            static int df = 0;
            if (df % 50 == 0 || df == ddemo)
                printf("drive %5.1fs  t=%4d  %3.0f km/h  gear %d  %4.0f rpm  steer %+5.1f deg"
                       "  z %6.0f  pitch %+4.1f  hits %d%s\n", df / 50.0f, g_pat,
                       drive_kmh(&g_pc), g_pc.gear, g_pc.rpm, g_pc.steer * 57.3f, g_pc.z,
                       g_pc.pitch * 57.3f, g_wall_hits,
                       g_pc.spin ? " SPIN" : g_pc.slide > 0 ? " skid" : "");
            if (getenv("OS_OPPDUMP") && df % 250 == 0) {
                printf("  %5.1fs  player POS %d  ", df / 50.0f, g_player_rank);
                for (int k = 0; k < g_nopp; k++)
                    printf(" [%d: P%d lap %d at %.2f %3.0fkm/h]", k, g_opp[k].rank, g_opp[k].mk_laps,
                           g_opp[k].progress, g_opp[k].speed * 340.0f / 21000.0f);
                printf("\n");
            }
            {
                /* how far inside the nearest wall the car's middle is */
                static float worst = 1e9f;
                const scene_wall *w = nearest_wall(g_pc.x, g_pc.y);
                if (w) {
                    float nx = cosf(w->angle), ny = -sinf(w->angle);
                    float depth = ((g_pc.x - w->x) * nx + (g_pc.y - w->y) * ny)
                                  * g_wall_in[w - g_scene->wall];
                    if (depth < worst) worst = depth;
                    if (df % 50 == 0 || df == ddemo)
                        printf("        marker %d  slip %.2f   car is %.0f inside the nearest "
                               "wall (least so far %.0f)\n", g_mark_at, g_pc.surface,
                               depth, worst);
                }
            }
            if (df++ >= ddemo) { write_ppm("demo.ppm"); break; }
        }
        if (demo) {
            float g = 0.0f;
            int hit = surface_pick(cam.x, cam.y, feet + STEP_UP, 1, &g);
            if (demo_frame % 20 == 0 || demo_frame == demo)
                printf("demo frame %3d  t=%4d  feet %7.0f  ground %7.0f%s  eyes %+5.0f"
                       " above feet\n", demo_frame,
                       path_nearest(r, cam.x, cam.y), feet, g, hit ? "" : " (none)",
                       cam.z - feet);
            if (demo_frame++ >= demo) { write_ppm("demo.ppm"); break; }
        }
#ifdef MISTER_HYBRID
        {
            /* MISTER: the helper thread turns the picture into the screen's and
               copies it; nothing draws on it until that thread has filled the
               next scene */
            tri_helper *h = tri_helper_get();
            if (!h) mister_show(os_slot(0), fb, FB_W, FB_H);
            else if (mister_show_later(os_slot(0), fb, FB_W, FB_H)) { SDL_AtomicSet(&h->show, 1); SDL_SemPost(h->wake); }
        }
#else
        SDL_UpdateTexture(tex, NULL, fb, FB_W * sizeof(uint32_t));
#endif
        const double worked = (double)(SDL_GetPerformanceCounter() - frame_began) * tick;
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren); res_started();
        /* no vsync (software renderer, some drivers)?  do not spin flat out.  (This was "under
           4 ms: wait 8", made with 60 Hz screens in mind: on a 180 Hz one a frame is 5.6 ms
           and any that came back a little early was made to miss the next one or two -- the
           rate swung between 180 and 90 with nothing going on.) */
        const double whole = (double)(SDL_GetPerformanceCounter() - frame_began) * tick;
        if (whole < 0.002) SDL_Delay(2);
        {   /* OS_FRAMELOG=1: once a second, how many frames, and how long the port's own work
               on one took (mean and longest), without the wait for the screen */
            static int on = -1, n, slow; static double sum, worst, since;
            if (on < 0) on = getenv("OS_FRAMELOG") != NULL;
            if (on) {
                n++; sum += worked; if (worked > worst) worst = worked;
                if (whole < 0.002) slow++;
                since += (double)(SDL_GetPerformanceCounter() - frame_began) * tick;
                if (since >= 1.0) {
                    printf("frames %d in %.2f s; the port's work on one: mean %.2f ms, longest %.2f ms; %d came back in under 2 ms\n", n, since, sum / n * 1000.0, worst * 1000.0, slow);
                    fflush(stdout);
                    n = slow = 0; sum = worst = since = 0.0;
                }
            }
        }
    }
#ifdef MISTER_HYBRID
    mister_show_wait();
#endif

    if (g_s2_on) s2_race_leave();                /* (0x2b2a8: time attack's ghost) */
    if (g_s2_on && getenv("OS_S2MENU3D") && getenv("OS_FROM_MENU") && !(s2_mode & 0x80)) s2_race_results_keep();   /* (for the game's menu: S2-5zq) */
    if (g_s2_on && s2_mode == 2 && s2_champ() && s2_champ()->on && !s2_replaying()) {      /* a league's race: how it ended, for the report (a replay's is the race's) */
        s2_champ_t *c = s2_champ();
        c->nres = s2_results(c->res);
        c->report = 1;
    }
    if (records_ok) records_enter(track);        /* 0x2a1a8, 0x28f42 */
    if (!g_window_closed && !g_intro && !getenv("OS_DRIVE_DEMO") && !getenv("OS_DEMO") && !shot) {      /* 0x21d8c */
        Uint32 t0 = SDL_GetTicks();
        uint32_t *keep = malloc(sizeof(uint32_t) * (size_t)(FB_W * FB_H));
        if (keep) {
            memcpy(keep, fb, sizeof(uint32_t) * (size_t)(FB_W * FB_H));
            for (;;) {
                float f = 1.0f - (float)(SDL_GetTicks() - t0) / 500.0f;
                if (f <= 0.0f) break;
                for (int k = 0; k < FB_W * FB_H; k++) {
                    uint32_t c = keep[k];
                    fb[k] = 0xff000000u | (uint32_t)((float)(c >> 16 & 255) * f) << 16
                          | (uint32_t)((float)(c >> 8 & 255) * f) << 8 | (uint32_t)((float)(c & 255) * f);
                }
                volumes_apply(f);
                SDL_UpdateTexture(tex, NULL, fb, FB_W * sizeof(uint32_t));
                SDL_RenderClear(ren);
                SDL_RenderCopy(ren, tex, NULL, NULL);
                SDL_RenderPresent(ren);
                SDL_PumpEvents();
            }
            free(keep);
        }
    }
    SDL_SetRelativeMouseMode(SDL_FALSE);
    /* (the window stays for the next part) */
    snd_close();
    /* started from the menu and left with ESC (not by closing the window): back to it */
    if (getenv("OS_FROM_MENU") && !g_window_closed && !getenv("OS_DRIVE_DEMO") && !getenv("OS_DEMO")) {
        unsetenv("OS_FROM_MENU");
        unsetenv("OS_INTRO");
        if (os_game() != 2) unsetenv("OS_CAR");      /* (Screamer 2's menu keeps what was chosen in these) */
        if (getenv("OS_CHAMP")) {                /* 0x29acc: nothing is recorded for a race that was left */
            char res[64];
            if (g_finished || g_game_over) {
                snprintf(res, sizeof res, "%d,%.2f,%d", g_final_rank ? g_final_rank : g_player_rank,
                         g_total_time, g_game_over && !g_finished);
                setenv("OS_CHAMP_RESULT", res, 1);
            } else unsetenv("OS_CHAMP_RESULT");
        }
        res_started();           /* handing over is not dying: the mark must not outlive this part */
        os_next(1, NULL, NULL);                  /* the menu next */
    }
    return 0;
}
