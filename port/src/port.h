/*
 * What every source file of the port shares: where files are, memory that is
 * given back between the menu and a race, and starting the next part of the
 * program without starting the program again.
 */
#ifndef OS_PORT_H
#define OS_PORT_H

#include <stdio.h>
#include <stdlib.h>

/*
 * PATHS.  The code names its files as it always has -- "build/dec/..." for
 * the game's extracted data, "openscreamer.*" and "REPLAYS/..." for what the
 * port writes -- and os_path() turns those into real places:
 *   the DATA directory   -data DIR, or $OPENSCREAMER_DATA, or the first of
 *                        ./screamer, <program's directory>/screamer,
 *                        <program's directory>/../../screamer, ./build/dec,
 *                        <program's directory>/data,
 *                        <program's directory>/../../build/dec, ./data.
 *                        It is either a copy of the GAME (its DATA and GFX
 *                        folders: the containers are read as they are) or a
 *                        directory of files unpacked by tools/extract.sh.
 *   the HOME directory   -home DIR, or $OPENSCREAMER_HOME, or the current
 *                        directory when it already holds the port's files,
 *                        or ~/.config/openscreamer
 * Anything else is left as it is.
 */
const char *os_path(const char *path);
FILE *os_fopen(const char *path, const char *mode);
FILE *os_config_open(const char *name);  /* a file of the game's CONFIG folder, or NULL */
int   os_exists(const char *path);      /* can it be read?  (also inside the game's containers) */
const char *os_data_dir(void);
const char *os_home_dir(void);

/* memory that os_state_restore() frees: everything the port allocates */
void *os_malloc(size_t n);
void *os_calloc(size_t n, size_t each);
void *os_realloc(void *p, size_t n);
void  os_free(void *p);
/* memory that outlives the parts -- for what an os_slot keeps; never freed */
void *os_keep_calloc(size_t n, size_t each);

/*
 * ONE PROGRAM, SEVERAL PARTS.  The menu, a race and the intro each expect to
 * start with everything as the program first finds it.  They used to get
 * that by the program starting itself again; now the program remembers its
 * own untouched state once (os_state_save) and puts it back between parts
 * (os_state_restore), keeping its window, its arguments and its directories.
 */
void os_init(int argc, char **argv);    /* reads -d, -data, -home; the rest are the part's own arguments */
int  os_dev(void);                      /* -d: the developer's keys are on */
int  os_game(void);                     /* 1 = Screamer, 2 = Screamer 2 (-game N, or by the files found) */
int  os_both_games(void);               /* both games' files were found and -game was not given */
void os_select_game(int g);             /* both found: from now on game g (its folders, its check) */
int  os_argc(void);
char **os_argv(void);
void os_state_save(void);
void os_state_restore(void);
void os_next(int argc, const char *a1, const char *a2);   /* arguments of the part to run next */
int  os_has_next(void);
void os_next_done(void);                 /* the next part's arguments have been taken */
int  os_once(int id);                   /* 1 the first time it is asked about `id`, 0 after */
void *os_slot(int id);                  /* a pointer that outlives the parts (window, renderer, texture), ids 0..15; what it points at must too (os_keep_calloc) */
void os_slot_set(int id, void *p);

#ifdef MISTER_HYBRID
/* MISTER: time is counted in the core's fields (mister.c) */
#include <SDL.h>
#include "mister.h"
#define SDL_GetTicks() mister_ticks()
#endif

/*
 * The environment is asked a thousand times a picture (every OS_... switch,
 * where it is used), and each asking is a search through all of it: on a slow
 * processor that is a millisecond.  An answer is remembered per place that
 * asks -- by the address of the name it asks with, so only for names written
 * into the program -- until something is set or unset.
 */
const char *os_getenv(const char *name);
int  os_setenv(const char *name, const char *value, int overwrite);
int  os_unsetenv(const char *name);

#if defined(_WIN32) && !defined(OS_PORT_IMPL)
/* WINDOWS: its mkdir has no second argument */
#include <direct.h>
#undef mkdir
#define mkdir(path, mode) _mkdir(path)
#endif

#ifndef OS_PORT_IMPL
#define getenv(n)        ((char *)(__builtin_constant_p(n) ? os_getenv(n) : (getenv)(n)))
#define setenv(n, v, o)  os_setenv(n, v, o)
#define unsetenv(n)      os_unsetenv(n)
#define fopen(p, m)    os_fopen(p, m)
#define malloc(n)      os_malloc(n)
#define calloc(n, e)   os_calloc(n, e)
#define realloc(p, n)  os_realloc(p, n)
#define free(p)        os_free(p)
#endif

#endif
