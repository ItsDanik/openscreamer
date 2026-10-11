/* mister.c - see mister.h */
#include <stdio.h>
#include <SDL.h>
#include <mister_hybrid.h>
#include "mister.h"

#define EXIT_TO_MENU  0
#define EXIT_RELOADED 43    /* danik_hybrid_launch.sh starts the game again once the core has settled */

int mister_on(void)
{
    return MH_Open();
}

/* "O[24],Resolution,..." in core/OpenScreamer.sv */
int mister_res_high(void)
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

void mister_frame_done(void)
{
    static uint32_t field0;
    static int frames, on = -1;
    if (on < 0) on = SDL_getenv("OS_FPSLOG") != NULL;
    if (!on || !MH_IsOpen()) return;
    uint32_t field = MH_FieldCounter();
    if (!frames) field0 = field;
    frames++;
    if (field - field0 >= 600) {
        printf("speed: %.1f pictures a second (%d in %u fields)\n", (double)(frames - 1) * 59.64 / (double)(field - field0), frames - 1, field - field0);
        fflush(stdout);
        frames = 0;
    }
}

void mister_hires_there(int there)
{
    if (MH_IsOpen()) MH_SetMenuMask(there ? 0 : 1);
}
