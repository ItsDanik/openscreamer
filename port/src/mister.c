/* mister.c - see mister.h */
#include <stdio.h>
#include <SDL.h>
#include <mister_hybrid.h>
#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include "mister.h"

#define EXIT_TO_MENU  0
#define EXIT_RELOADED 43    /* danik_hybrid_launch.sh starts the game again once the core has settled */

int mister_on(void)
{
    return MH_Open();
}

/* "O[24],Resolution,..." in core/OpenScreamer.sv */
int mister_res_tall(void)
{
    return MH_Open() ? MH_OSD_GAME_BITS(MH_OSDStatus(), 24, 1) : 0;
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

int mister_exit_code(void)
{
    return MH_CoreReloaded() ? EXIT_RELOADED : EXIT_TO_MENU;
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
                     "leftstick:b28,rightstick:b29,dpup:h0.1,dpright:h0.2,dpdown:h0.4,dpleft:h0.8,leftx:a0,lefty:a1,",
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

/* the picture's 32 bits a pixel to the screen's 16 */
static void to_565(const uint32_t *src, uint8_t *to, int pitch, int w, int h)
{
    for (int y = 0; y < h; y++) {
        uint16_t *dst = (uint16_t *)(to + y * pitch);
        int x = 0;
#ifdef __ARM_NEON
        /* eight pixels at a time */
        for (; x + 8 <= w; x += 8, src += 8) {
            const uint8x8x4_t p = vld4_u8((const uint8_t *)src);          /* b, g, r, a */
            uint16x8_t o = vshll_n_u8(vshr_n_u8(p.val[2], 3), 8);
            o = vshlq_n_u16(o, 3);
            o = vorrq_u16(o, vshlq_n_u16(vmovl_u8(vshr_n_u8(p.val[1], 2)), 5));
            o = vorrq_u16(o, vmovl_u8(vshr_n_u8(p.val[0], 3)));
            vst1q_u16(dst + x, o);
        }
#endif
        for (; x < w; x++) {
            uint32_t p = *src++;
            dst[x] = (uint16_t)(((p >> 8) & 0xf800) | ((p >> 5) & 0x07e0) | ((p >> 3) & 0x001f));
        }
    }
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
static struct { const void *pixels; int w, h; } g_later;
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

void mister_show(void *window, const void *pixels, int w, int h)
{
    mister_show_wait();
    SDL_Surface *s = SDL_GetWindowSurface(window);
    if (!s) return;
    mister_mark(3);
    if (s->format->format == SDL_PIXELFORMAT_RGB565 && s->w >= w && s->h >= h) {
        to_565(pixels, s->pixels, s->pitch, w, h);
        g_shown_w = w; g_shown_h = h;
    } else {
        SDL_ConvertPixels(w < s->w ? w : s->w, h < s->h ? h : s->h, SDL_PIXELFORMAT_ARGB8888, pixels, w * 4,
                          s->format->format, s->pixels, s->pitch);
        g_shown_w = g_shown_h = 0;
    }
    /* what went before this is the making of the picture; the rest is the wait for the field */
    Uint64 now = SDL_GetPerformanceCounter();
    work_done(now);
    mister_mark(4);
    SDL_UpdateWindowSurface(window);
    g_shown_field = MH_FieldCounter();
    wait_done(now);
}

int mister_show_later(void *window, const void *pixels, int w, int h)
{
    static int never = -1;
    if (never < 0) never = SDL_getenv("OS_SHOWNOW") != NULL;
    if (never || !MH_IsOpen() || w != g_shown_w || h != g_shown_h || w * h > 640 * 400) {
        mister_show(window, pixels, w, h);
        return 0;
    }
    mister_show_wait();
    mister_mark(3);
    Uint64 now = SDL_GetPerformanceCounter();
    work_done(now);
    mister_mark(4);
    MH_WaitFrame(g_shown_field);                 /* one picture a field (two with an interlaced mode) */
    g_shown_field = MH_FieldCounter();
    wait_done(now);
    g_later.pixels = pixels; g_later.w = w; g_later.h = h;
    SDL_AtomicSet(&g_later_due, 1);
    return 1;
}

void mister_show_job(void)
{
    static uint16_t screen[640 * 400];
    if (!SDL_AtomicGet(&g_later_due)) return;
    to_565(g_later.pixels, (uint8_t *)screen, g_later.w * 2, g_later.w, g_later.h);
    MH_Present(screen, g_later.w * 2);
    SDL_AtomicSet(&g_later_due, 0);
}

void mister_second_cpu(void)
{
    MH_PinThreadToCPU1();
}
