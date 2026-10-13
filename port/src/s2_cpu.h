/*
 * SCREAMER 2: the game's own race routines, run from the player's STARTL.EXE.
 *
 * Nothing of the program is in the port.  s2_exe_load() reads the player's own file into
 * the flat layout its addresses mean (the LE objects at their bases, its relocation records
 * applied -- what tools/unle.py --fixups does), and s2_call() runs a routine of it in the
 * interpreter of s2_cpu.c: the 386's whole numbers and the 387's floating point, as much of
 * both as those routines use.  Where a routine calls out of what the port lets run (the
 * track under a wheel, sound, the engine's lists) a HOOK of s2_hooks.c answers instead.
 * docs/NOTES2.md S2-7.
 */
#ifndef OS_S2_CPU_H
#define OS_S2_CPU_H

#include <stdint.h>

#define S2_FLAT 0x210000u            /* code 0x10000.., data 0x110000..0x139c90, then the port's heap and the stack */
extern uint8_t s2_flat[S2_FLAT];

typedef struct {
    uint32_t r[8];                   /* eax ecx edx ebx esp ebp esi edi */
    uint32_t eip;
    int      cf, zf, sf, of, pf, df;
    double   st[8];                  /* the 387's registers; st[top] is ST(0) */
    int      top;
    uint16_t cw, sw;
} s2_cpu;
enum { S2_EAX, S2_ECX, S2_EDX, S2_EBX, S2_ESP, S2_EBP, S2_ESI, S2_EDI };
extern s2_cpu s2_c;

/* the player's program: 1 if it is the one these addresses are of */
int  s2_exe_load(const char *path);
/* the image as it was read (the flat memory is given back to it by s2_exe_fresh) */
void s2_exe_fresh(void);

/* a hook: the routine at `at` is not run; `fn` answers (its result in eax, or on the 387's
   stack, as the routine would leave it) and the routine's own `ret pop` follows.  A hook
   that returns 0 lets the routine run after all. */
typedef int (*s2_hook_fn)(s2_cpu *c);
void s2_hook(uint32_t at, int pop, s2_hook_fn fn);
void s2_hooks_clear(void);

/* run the routine at `entry` (Watcom's register convention: eax, edx, ebx, ecx, then
   `nstack` words on the stack, the first lowest); what it leaves in eax */
uint32_t s2_call(uint32_t entry, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx, int nstack, const uint32_t *stack);
/* ST(0) as a call or a hook finds it, and the 387's stack for a hook that takes or gives a number there */
double s2_st0(void);
void   s2_st0_set(double v);
/* word n of the arguments on the stack, for a hook (0 = the first) */
uint32_t s2_arg(const s2_cpu *c, int n);

extern unsigned long long s2_cpu_count;      /* instructions run so far */

#endif
