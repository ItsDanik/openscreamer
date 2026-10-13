/*
 * SCREAMER 2's routines work on the game's own memory layout: the program's data object,
 * 0x110000 up, and after it room the port hands out for what the game kept in allocated
 * memory (splines).  An address is a NUMBER; MP(a) is the byte it names.  The data object
 * starts as the player's program has it (s2_cpu.h); what the game's loaders read from its
 * files is put there by the port (s2_hooks.c).
 */
#ifndef OS_S2_MEM_H
#define OS_S2_MEM_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define S2_BASE 0x110000u
#define S2_SIZE 0x100000u          /* the data object is 0x2a000 of it; the rest is s2_alloc()'s */
void s2_bad_address(uint32_t a);
#ifdef S2_CONVERTED
/* (the developer's build with the routines converted to C, tools/s2c.py: not in the
   repository -- see s2_cpu.h) */
extern uint8_t s2_mem[S2_SIZE];

/* the engine keeps a few variables in its CODE object (0x4cb98: what the ground query
   found; 0xb04a8); those pages are given out as they are asked for */
uint8_t *s2_code_var(uint32_t a);

static inline uint8_t *MP(uint32_t a)
{
    if (a - S2_BASE >= S2_SIZE - 16) return s2_code_var(a);
    return s2_mem + (a - S2_BASE);
}
#else
/* THE PLAYER'S PROGRAM, RUN AS IT IS (s2_cpu.h): the block is part of the flat memory the
   program's image is read into, so an address is simply an offset in it, code and all */
#include "s2_cpu.h"
#define s2_mem (s2_flat + S2_BASE)
static inline uint8_t *MP(uint32_t a)
{
    if (a > S2_FLAT - 16) { s2_bad_address(a); return s2_flat + S2_BASE; }
    return s2_flat + a;
}
#endif
/* the address of a byte of the block */
#define PM(p) ((uint32_t)((const uint8_t *)(p) - s2_mem) + S2_BASE)
/* a spare word in the block, for a routine's own local that has to be handed on by address */
#define S2_TMP 0x13f000u
/* word i of a record reached through a float pointer, as the whole number it is */
#define WI(p, i) (((int32_t *)(p))[i])
/* the routines' own stack, for locals whose address they hand on (it grows upwards) */
extern uint32_t s2_sp;
static inline uint32_t s2_push(uint32_t n) { uint32_t a = s2_sp; s2_sp += (n + 7u) & ~7u; memset(MP(a), 0, n); return a; }
/* a float's bits as a whole number (the game compares floats that way against constants) */
static inline int32_t s2_bits(float f) { int32_t i; memcpy(&i, &f, 4); return i; }
#define BITS(x) s2_bits((float)(x))
#define MP1 MP
uint32_t s2_alloc(uint32_t size);                 /* zeroed; an address */
void     s2_str(uint32_t a, const char *text);    /* put a name where the game has it */

/* the decompiler's words */
#define __cdecl
typedef uint8_t  undefined, undefined1, byte;
typedef uint16_t undefined2, ushort;
typedef uint32_t undefined4, uint;
typedef uint64_t undefined8, ulonglong;
typedef int64_t  longlong;
/*
 * THE GAME'S ARITHMETIC.  Its runtime loads the maths processor's control word 0x127f
 * (0xbb603 -> 0xd5d60): every step of a sum is worked out to DOUBLE precision (53 bits) on
 * the x87 stack, whatever the variables are, and only what is stored in a float is cut to
 * one.  Where a place is made a whole number by cutting the fraction off (0x1c7f8) that
 * decides which whole number.  So this file's users are compiled to use the x87 too
 * (CMakeLists: -mfpmath=387 -fexcess-precision=standard) and S2_FPU sets the same control
 * word round the game's routines.  (Other processors: not done -- there the sums would have
 * to be written out in doubles.)
 */
typedef long double float10;
#if defined(__i386__) || defined(__x86_64__)
#define S2_FPU_IN  unsigned short s2_cw_was_, s2_cw_ = 0x127f; __asm__ volatile("fnstcw %0" : "=m"(s2_cw_was_)); __asm__ volatile("fldcw %0" : : "m"(s2_cw_))
#define S2_FPU_OUT __asm__ volatile("fldcw %0" : : "m"(s2_cw_was_))
#else
#define S2_FPU_IN  (void)0
#define S2_FPU_OUT (void)0
#endif
#ifndef __cplusplus
typedef uint8_t  bool;
#define true 1
#define false 0
#endif
/* (the processor's own functions give their answers to the same 53 bits) */
#define fcos(x)  ((long double)(double)cosl((long double)(x)))
#define fsin(x)  ((long double)(double)sinl((long double)(x)))
#define fsqrt(x) ((long double)sqrt((double)(x)))
#define SQRT(x)  ((long double)sqrt((double)(x)))
/* the program's arc cosine (0x4ea98) is pi/2 - atan(x / sqrt(1 - x x)); when 1 - x x is
   negative its square root reports a domain error and the arc tangent is skipped: taken as 0
   here.  (With plain acos a cosine of 1.0000001 gave a NaN heading, and the loop that brings
   an angle under two pi, 0x2d59c, never ended: track 4, frame 548.) */
/* a float stored where the decompiler has no float (tools/s2c.py, "every store of a float is
   a rounding"): its bytes, not its value as a whole number */
#define S2_ASF(v) (*(float *)&(v))
#define S2_ASD(v) (*(double *)&(v))
#define S2_SETF(lv, e)  (*(float *)&(lv) = (float)(e))
#define S2_SETFD(lv, e) (*(double *)&(lv) = (double)(e))
static inline uint32_t s2_f32bits(float v) { uint32_t u; memcpy(&u, &v, 4); return u; }
static inline uint64_t s2_f64bits(double v) { uint64_t u; memcpy(&u, &v, 8); return u; }
static inline double s2_acos(double x) { return x * x <= 1.0 ? acos(x) : 0.0; }
#define fpatan(y, x) ((long double)(double)atan2l((long double)(y), (long double)(x)))
/* the game's float-to-whole-number helper (0x4ea12) cuts the fraction off */
#define ROUND(x) truncl((long double)(x))
#define ABS(x)   fabsl((long double)(x))
#define CONCAT44(hi, lo) (((int64_t)(int32_t)(hi) << 32) | (uint32_t)(lo))

#endif
