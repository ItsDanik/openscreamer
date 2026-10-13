/*
 * SCREAMER 2's routines, carried over as they are (tools/s2c.py), work on the game's own
 * memory layout: one block of bytes standing for the program's data object, 0x110000 up,
 * and after it room the port hands out for what the game kept in allocated memory
 * (splines).  An address is a NUMBER; MP(a) is the byte it names.  Nothing of the game's
 * own data is in the block to begin with: what the routines read is put there by the port
 * (s2_hooks.c) from the game's files, as the game's loaders did.
 */
#ifndef OS_S2_MEM_H
#define OS_S2_MEM_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define S2_BASE 0x110000u
#define S2_SIZE 0x100000u          /* the data object is 0x2a000 of it; the rest is s2_alloc()'s */
extern uint8_t s2_mem[S2_SIZE];
void s2_bad_address(uint32_t a);

/* the engine keeps a few variables in its CODE object (0x4cb98: what the ground query
   found; 0xb04a8); those pages are given out as they are asked for */
uint8_t *s2_code_var(uint32_t a);

static inline uint8_t *MP(uint32_t a)
{
    if (a - S2_BASE >= S2_SIZE - 16) return s2_code_var(a);
    return s2_mem + (a - S2_BASE);
}
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
typedef double   float10;
#ifndef __cplusplus
typedef uint8_t  bool;
#define true 1
#define false 0
#endif
#define fcos(x)  cos((double)(x))
#define fsin(x)  sin((double)(x))
#define fsqrt(x) sqrt((double)(x))
#define SQRT(x)  sqrt((double)(x))
/* the program's arc cosine (0x4ea98) is pi/2 - atan(x / sqrt(1 - x x)); when 1 - x x is
   negative its square root reports a domain error and the arc tangent is skipped: taken as 0
   here.  (With plain acos a cosine of 1.0000001 gave a NaN heading, and the loop that brings
   an angle under two pi, 0x2d59c, never ended: track 4, frame 548.) */
static inline double s2_acos(double x) { return x * x <= 1.0 ? acos(x) : 0.0; }
#define fpatan(y, x) atan2((double)(y), (double)(x))
/* the game's float-to-whole-number helper (0x4ea12) cuts the fraction off */
#define ROUND(x) trunc((double)(x))
#define ABS(x)   fabs((double)(x))
#define CONCAT44(hi, lo) (((int64_t)(int32_t)(hi) << 32) | (uint32_t)(lo))

#endif
