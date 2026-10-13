/*
 * SCREAMER 2: an interpreter for the game's race routines (s2_cpu.h).
 *
 * A 386 in flat 32-bit mode with a 387, as much of both as the routines use (the list was
 * taken from what they execute: docs/NOTES2.md S2-7); anything else stops the routine with
 * a message that says where.  No segments (the prefixes are skipped), no 16-bit addressing,
 * no interrupts, no ports.
 *
 * THE 387.  The game's runtime sets its control word to 0x127f: every sum is rounded to 53
 * bits, a double's.  So a register here IS a double and the host's own double arithmetic
 * gives the same bits (compiled without contraction of a * b + c: CMakeLists) -- on any
 * processor.  What is not the processor's to the last bit: sine, cosine and arc tangent,
 * which are the C library's here.  The rounding of a store to a whole number is the control
 * word's (the game switches to cutting off round its conversions).
 */
#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include "s2_cpu.h"

s2_cpu s2_c;
unsigned long long s2_cpu_count;

#define RET_MAGIC 0x0000fff0u        /* where the outermost routine returns to: below the code */
#define STACK_TOP 0x0020f000u        /* the routines' stack, downwards, at the top of the flat memory */

static jmp_buf s2_stop;
static int s2_depth;

static struct { uint32_t at; int pop; s2_hook_fn fn; } s2_hooks[64];
static int s2_nhooks;

void s2_hooks_clear(void) { s2_nhooks = 0; }
void s2_hook(uint32_t at, int pop, s2_hook_fn fn)
{
    if (s2_nhooks < 64) { s2_hooks[s2_nhooks].at = at; s2_hooks[s2_nhooks].pop = pop; s2_hooks[s2_nhooks].fn = fn; s2_nhooks++; }
}

static void s2_fail(const char *what)
{
    const uint32_t at = s2_c.eip;
    fprintf(stderr, "screamer 2: the game's routine stopped at 0x%x: %s", at, what);
    if (at < S2_FLAT - 8) fprintf(stderr, " (bytes %02x %02x %02x %02x %02x %02x)", s2_flat[at], s2_flat[at + 1], s2_flat[at + 2], s2_flat[at + 3], s2_flat[at + 4], s2_flat[at + 5]);
    fprintf(stderr, "\n");
    longjmp(s2_stop, 1);
}

/* ---- memory */
static inline uint8_t *M(uint32_t a)
{
    if (a > S2_FLAT - 16) { s2_fail("an address outside its memory"); }
    return s2_flat + a;
}
static inline uint32_t rd(uint32_t a, int size)
{
    const uint8_t *p = M(a);
    if (size == 4) { uint32_t v; memcpy(&v, p, 4); return v; }
    if (size == 2) { uint16_t v; memcpy(&v, p, 2); return v; }
    return *p;
}
static inline void wr(uint32_t a, int size, uint32_t v)
{
    uint8_t *p = M(a);
    if (size == 4) memcpy(p, &v, 4);
    else if (size == 2) { uint16_t w = (uint16_t)v; memcpy(p, &w, 2); }
    else *p = (uint8_t)v;
}
static inline uint32_t fetch8(s2_cpu *c) { return s2_flat[c->eip++]; }
static inline uint32_t fetch16(s2_cpu *c) { uint16_t v; memcpy(&v, s2_flat + c->eip, 2); c->eip += 2; return v; }
static inline uint32_t fetch32(s2_cpu *c) { uint32_t v; memcpy(&v, s2_flat + c->eip, 4); c->eip += 4; return v; }
static inline uint32_t fetch(s2_cpu *c, int size) { return size == 4 ? fetch32(c) : size == 2 ? fetch16(c) : fetch8(c); }

static inline void push32(s2_cpu *c, uint32_t v) { c->r[S2_ESP] -= 4; wr(c->r[S2_ESP], 4, v); }
static inline uint32_t pop32(s2_cpu *c) { const uint32_t v = rd(c->r[S2_ESP], 4); c->r[S2_ESP] += 4; return v; }

/* ---- operands */
typedef struct { int mod, reg, rm; uint32_t ea; } modrm;

static inline void decode(s2_cpu *c, modrm *m)
{
    const uint32_t b = fetch8(c);
    m->mod = (int)(b >> 6); m->reg = (int)(b >> 3 & 7); m->rm = (int)(b & 7); m->ea = 0;
    if (m->mod == 3) return;
    uint32_t ea;
    if (m->rm == 4) {
        const uint32_t sib = fetch8(c), idx = sib >> 3 & 7, base = sib & 7;
        ea = idx != 4 ? c->r[idx] << (sib >> 6) : 0;
        if (base == 5 && m->mod == 0) ea += fetch32(c); else ea += c->r[base];
    } else if (m->rm == 5 && m->mod == 0) ea = fetch32(c);
    else ea = c->r[m->rm];
    if (m->mod == 1) ea += (uint32_t)(int32_t)(int8_t)fetch8(c);
    else if (m->mod == 2) ea += fetch32(c);
    m->ea = ea;
}
static inline uint32_t getr(const s2_cpu *c, int reg, int size)
{
    if (size == 4) return c->r[reg];
    if (size == 2) return c->r[reg] & 0xffffu;
    return reg < 4 ? c->r[reg] & 0xffu : c->r[reg - 4] >> 8 & 0xffu;
}
static inline void setr(s2_cpu *c, int reg, int size, uint32_t v)
{
    if (size == 4) c->r[reg] = v;
    else if (size == 2) c->r[reg] = (c->r[reg] & 0xffff0000u) | (v & 0xffffu);
    else if (reg < 4) c->r[reg] = (c->r[reg] & 0xffffff00u) | (v & 0xffu);
    else c->r[reg - 4] = (c->r[reg - 4] & 0xffff00ffu) | (v & 0xffu) << 8;
}
static inline uint32_t gete(s2_cpu *c, const modrm *m, int size) { return m->mod == 3 ? getr(c, m->rm, size) : rd(m->ea, size); }
static inline void sete(s2_cpu *c, const modrm *m, int size, uint32_t v) { if (m->mod == 3) setr(c, m->rm, size, v); else wr(m->ea, size, v); }

/* ---- flags */
static inline uint32_t mask_of(int size) { return size == 4 ? 0xffffffffu : size == 2 ? 0xffffu : 0xffu; }
static inline uint32_t sign_of(int size) { return size == 4 ? 0x80000000u : size == 2 ? 0x8000u : 0x80u; }
static inline void szp(s2_cpu *c, uint32_t res, int size)
{
    res &= mask_of(size);
    c->zf = res == 0; c->sf = (res & sign_of(size)) != 0; c->pf = !__builtin_parity(res & 0xffu);
}
static inline uint32_t f_add(s2_cpu *c, uint32_t a, uint32_t b, uint32_t carry, int size)
{
    const uint32_t m = mask_of(size), s = sign_of(size);
    const uint64_t wide = (uint64_t)(a & m) + (b & m) + carry;
    const uint32_t res = (uint32_t)wide & m;
    c->cf = (wide >> (size * 8)) & 1; c->of = ((a ^ res) & (b ^ res) & s) != 0;
    szp(c, res, size);
    return res;
}
static inline uint32_t f_sub(s2_cpu *c, uint32_t a, uint32_t b, uint32_t borrow, int size)
{
    const uint32_t m = mask_of(size), s = sign_of(size);
    const uint64_t wide = (uint64_t)(a & m) - (b & m) - borrow;
    const uint32_t res = (uint32_t)wide & m;
    c->cf = (uint64_t)(a & m) < (uint64_t)(b & m) + borrow; c->of = ((a ^ b) & (a ^ res) & s) != 0;
    szp(c, res, size);
    return res;
}
static inline uint32_t f_logic(s2_cpu *c, uint32_t res, int size) { c->cf = c->of = 0; szp(c, res, size); return res & mask_of(size); }

/* the eight of 00..3f, 80..83: add or adc sbb and sub xor cmp; cmp writes nothing */
static inline uint32_t alu(s2_cpu *c, int op, uint32_t a, uint32_t b, int size)
{
    switch (op) {
    case 0: return f_add(c, a, b, 0, size);
    case 1: return f_logic(c, a | b, size);
    case 2: return f_add(c, a, b, (uint32_t)c->cf, size);
    case 3: return f_sub(c, a, b, (uint32_t)c->cf, size);
    case 4: return f_logic(c, a & b, size);
    case 5: return f_sub(c, a, b, 0, size);
    case 6: return f_logic(c, a ^ b, size);
    default: return f_sub(c, a, b, 0, size);
    }
}
static inline int cond(const s2_cpu *c, int n)
{
    int v;
    switch (n >> 1) {
    case 0: v = c->of; break;
    case 1: v = c->cf; break;
    case 2: v = c->zf; break;
    case 3: v = c->cf || c->zf; break;
    case 4: v = c->sf; break;
    case 5: v = c->pf; break;
    case 6: v = c->sf != c->of; break;
    default: v = c->zf || c->sf != c->of; break;
    }
    return (n & 1) ? !v : v;
}
static uint32_t shift(s2_cpu *c, int op, uint32_t a, uint32_t n, int size)
{
    const uint32_t m = mask_of(size), s = sign_of(size);
    const int bits = size * 8;
    uint32_t res;
    a &= m; n &= 31;
    if (!n) return a;
    switch (op) {
    case 0: n %= (uint32_t)bits; res = n ? (a << n | a >> ((uint32_t)bits - n)) & m : a; c->cf = res & 1; c->of = ((res & s) != 0) != c->cf; return res;      /* rol */
    case 1: n %= (uint32_t)bits; res = n ? (a >> n | a << ((uint32_t)bits - n)) & m : a; c->cf = (res & s) != 0; c->of = ((res ^ res << 1) & s) != 0; return res; /* ror */
    case 4: case 6:                                                    /* shl */
        res = n < 32 ? (a << n) & m : 0;
        c->cf = n <= (uint32_t)bits ? (a >> ((uint32_t)bits - n)) & 1 : 0; c->of = ((res & s) != 0) != c->cf;
        szp(c, res, size); return res;
    case 5:                                                            /* shr */
        res = a >> n; c->cf = (a >> (n - 1)) & 1; c->of = (a & s) != 0;
        szp(c, res, size); return res;
    case 7: {                                                          /* sar */
        const int32_t sa = size == 4 ? (int32_t)a : size == 2 ? (int16_t)a : (int8_t)a;
        res = (uint32_t)(sa >> (n > 31 ? 31 : n)) & m; c->cf = (uint32_t)(sa >> (n - 1)) & 1; c->of = 0;
        szp(c, res, size); return res;
    }
    default: s2_fail("a rotate through carry"); return 0;
    }
}

/* ---- the 387 */
#define ST(i) c->st[(c->top + (i)) & 7]
static inline void fpush(s2_cpu *c, double v) { c->top = (c->top - 1) & 7; c->st[c->top] = v; }
static inline double fpop(s2_cpu *c) { const double v = c->st[c->top]; c->top = (c->top + 1) & 7; return v; }
static inline void fcom(s2_cpu *c, double a, double b)
{
    c->sw &= (uint16_t)~0x4500u;
    if (a != a || b != b) c->sw |= 0x4500u;
    else if (a < b) c->sw |= 0x0100u;
    else if (a == b) c->sw |= 0x4000u;
}
static double fround(const s2_cpu *c, double v)
{
    switch (c->cw >> 10 & 3) {
    case 0: return nearbyint(v);             /* (the host's mode is to nearest, ties to even, as the 387's) */
    case 1: return floor(v);
    case 2: return ceil(v);
    default: return trunc(v);
    }
}
static inline uint32_t fint32(const s2_cpu *c, double v)
{
    v = fround(c, v);
    if (!(v >= -2147483648.0 && v <= 2147483647.0)) return 0x80000000u;
    return (uint32_t)(int32_t)v;
}
static inline double f_arith(int op, double a, double b)
{
    switch (op) {
    case 0: return a + b;
    case 1: return a * b;
    case 4: return a - b;
    case 5: return b - a;
    case 6: return a / b;
    default: return b / a;
    }
}
static inline float  rdf(uint32_t a) { float v; memcpy(&v, M(a), 4); return v; }
static inline double rdd(uint32_t a) { double v; memcpy(&v, M(a), 8); return v; }
static inline void   wrf(uint32_t a, double v) { const float f = (float)v; memcpy(M(a), &f, 4); }
static inline void   wrd(uint32_t a, double v) { memcpy(M(a), &v, 8); }
/* ten bytes: sign and exponent, then 64 bits with the leading one written out */
static double rd80(uint32_t a)
{
    uint64_t man; uint16_t se;
    memcpy(&man, M(a), 8); memcpy(&se, M(a + 8), 2);
    const int e = se & 0x7fff;
    double v = e == 0 && man == 0 ? 0.0 : ldexp((double)man, e - 16383 - 63);
    return (se & 0x8000) ? -v : v;
}
static void wr80(uint32_t a, double v)
{
    uint64_t man = 0; uint16_t se = 0;
    if (v != 0.0 && v == v) {
        int e;
        const double f = frexp(fabs(v), &e);              /* 0.5 <= f < 1 */
        man = (uint64_t)ldexp(f, 64); se = (uint16_t)(e - 1 + 16383);
    }
    if (signbit(v)) se |= 0x8000;
    memcpy(M(a), &man, 8); memcpy(M(a + 8), &se, 2);
}

static void x87(s2_cpu *c, uint32_t opc)
{
    modrm m;
    decode(c, &m);
    const int op = m.reg, i = m.rm;
    if (m.mod != 3) {
        const uint32_t ea = m.ea;
        switch (opc) {
        case 0xd8: case 0xdc: case 0xda: case 0xde: {
            const double v = opc == 0xd8 ? (double)rdf(ea) : opc == 0xdc ? rdd(ea) : opc == 0xda ? (double)(int32_t)rd(ea, 4) : (double)(int16_t)rd(ea, 2);
            if (op == 2) fcom(c, ST(0), v);
            else if (op == 3) { fcom(c, ST(0), v); fpop(c); }
            else ST(0) = f_arith(op, ST(0), v);
            return;
        }
        case 0xd9:
            switch (op) {
            case 0: fpush(c, (double)rdf(ea)); return;
            case 2: wrf(ea, ST(0)); return;
            case 3: wrf(ea, ST(0)); fpop(c); return;
            case 5: c->cw = (uint16_t)rd(ea, 2); return;
            case 7: wr(ea, 2, c->cw); return;
            }
            break;
        case 0xdb:
            switch (op) {
            case 0: fpush(c, (double)(int32_t)rd(ea, 4)); return;
            case 2: wr(ea, 4, fint32(c, ST(0))); return;
            case 3: wr(ea, 4, fint32(c, ST(0))); fpop(c); return;
            case 5: fpush(c, rd80(ea)); return;
            case 7: wr80(ea, ST(0)); fpop(c); return;
            }
            break;
        case 0xdd:
            switch (op) {
            case 0: fpush(c, rdd(ea)); return;
            case 2: wrd(ea, ST(0)); return;
            case 3: wrd(ea, ST(0)); fpop(c); return;
            case 7: wr(ea, 2, (uint32_t)((c->sw & ~0x3800u) | (uint32_t)c->top << 11)); return;
            }
            break;
        case 0xdf:
            switch (op) {
            case 0: fpush(c, (double)(int16_t)rd(ea, 2)); return;
            case 2: case 3: {
                const uint32_t v = fint32(c, ST(0));
                wr(ea, 2, (int32_t)v >= -32768 && (int32_t)v <= 32767 ? v : 0x8000u);
                if (op == 3) fpop(c);
                return;
            }
            case 5: { int64_t v; memcpy(&v, M(ea), 8); fpush(c, (double)v); return; }
            case 7: {
                const double v = fround(c, ST(0));
                int64_t w = v >= -9223372036854775808.0 && v < 9223372036854775808.0 ? (int64_t)v : INT64_MIN;
                memcpy(M(ea), &w, 8); fpop(c); return;
            }
            }
            break;
        }
        s2_fail("a floating-point instruction not known");
        return;
    }
    switch (opc) {
    case 0xd8:
        if (op == 2) fcom(c, ST(0), ST(i));
        else if (op == 3) { fcom(c, ST(0), ST(i)); fpop(c); }
        else ST(0) = f_arith(op, ST(0), ST(i));
        return;
    case 0xdc:                                   /* (to ST(i); subtract and divide are the other way round) */
        if (op == 2) { fcom(c, ST(0), ST(i)); return; }
        if (op == 3) { fcom(c, ST(0), ST(i)); fpop(c); return; }
        ST(i) = f_arith(op < 4 ? op : op ^ 1, ST(i), ST(0));
        return;
    case 0xde:
        if (op == 3 && i == 1) { fcom(c, ST(0), ST(1)); fpop(c); fpop(c); return; }
        if (op == 2 || op == 3) break;
        ST(i) = f_arith(op < 4 ? op : op ^ 1, ST(i), ST(0)); fpop(c);
        return;
    case 0xd9:
        switch (op) {
        case 0: { const double v = ST(i); fpush(c, v); return; }
        case 1: { const double v = ST(0); ST(0) = ST(i); ST(i) = v; return; }
        case 2: return;                          /* fnop */
        case 4:
            if (i == 0) { ST(0) = -ST(0); return; }
            if (i == 1) { ST(0) = fabs(ST(0)); return; }
            if (i == 4) { fcom(c, ST(0), 0.0); return; }
            break;
        case 5:
            switch (i) {
            case 0: fpush(c, 1.0); return;
            case 1: fpush(c, 3.3219280948873623479); return;       /* log2(10) */
            case 2: fpush(c, 1.4426950408889634074); return;       /* log2(e) */
            case 3: fpush(c, 3.14159265358979323846); return;
            case 4: fpush(c, 0.30102999566398119521); return;      /* log10(2) */
            case 5: fpush(c, 0.69314718055994530942); return;      /* ln(2) */
            case 6: fpush(c, 0.0); return;
            }
            break;
        case 6:
            switch (i) {
            case 0: ST(0) = exp2(ST(0)) - 1.0; return;                                   /* f2xm1 */
            case 1: { const double x = fpop(c); ST(0) = ST(0) * log2(x); return; }       /* fyl2x */
            case 2: ST(0) = tan(ST(0)); fpush(c, 1.0); c->sw &= (uint16_t)~0x0400u; return;   /* fptan */
            case 3: { const double x = fpop(c); ST(0) = atan2(ST(0), x); return; }       /* fpatan */
            }
            break;
        case 7:
            switch (i) {
            case 0: ST(0) = fmod(ST(0), ST(1)); c->sw &= (uint16_t)~0x0400u; return;     /* fprem: done in one go */
            case 2: ST(0) = sqrt(ST(0)); return;
            case 3: { const double x = ST(0); ST(0) = sin(x); fpush(c, cos(x)); c->sw &= (uint16_t)~0x0400u; return; }
            case 4: ST(0) = fround(c, ST(0)); return;
            case 5: ST(0) = ldexp(ST(0), (int)trunc(ST(1))); return;                     /* fscale */
            case 6: ST(0) = sin(ST(0)); c->sw &= (uint16_t)~0x0400u; return;
            case 7: ST(0) = cos(ST(0)); c->sw &= (uint16_t)~0x0400u; return;
            }
            break;
        }
        break;
    case 0xda:
        if (op == 5 && i == 1) { fcom(c, ST(0), ST(1)); fpop(c); fpop(c); return; }      /* fucompp */
        break;
    case 0xdb:
        if (op == 4 && (i == 2 || i == 3)) { if (i == 3) { c->cw = 0x037f; c->sw = 0; c->top = 0; } return; }   /* fclex, finit */
        break;
    case 0xdd:
        switch (op) {
        case 0: return;                          /* ffree */
        case 2: ST(i) = ST(0); return;
        case 3: ST(i) = ST(0); fpop(c); return;
        case 4: fcom(c, ST(0), ST(i)); return;
        case 5: fcom(c, ST(0), ST(i)); fpop(c); return;
        }
        break;
    case 0xdf:
        if (op == 4 && i == 0) { setr(c, S2_EAX, 2, (uint32_t)((c->sw & ~0x3800u) | (uint32_t)c->top << 11)); return; }   /* fnstsw ax */
        break;
    }
    s2_fail("a floating-point instruction not known");
}

/* ---- calls */
static int hooked(s2_cpu *c, uint32_t target)
{
    for (int k = 0; k < s2_nhooks; k++)
        if (s2_hooks[k].at == target) {
            /* (as the routine finds it: its return address on the stack, the arguments above) */
            push32(c, c->eip);
            const int done = s2_hooks[k].fn(c);
            c->r[S2_ESP] += 4;
            if (!done) return 0;
            c->r[S2_ESP] += (uint32_t)s2_hooks[k].pop;
            return 1;
        }
    return 0;
}
uint32_t s2_arg(const s2_cpu *c, int n) { return rd(c->r[S2_ESP] + 4 + 4 * (uint32_t)n, 4); }
double s2_st0(void) { return s2_c.st[s2_c.top & 7]; }
void   s2_st0_set(double v) { s2_c.st[s2_c.top & 7] = v; }

static void run(s2_cpu *c)
{
    for (;;) {
        if (c->eip == RET_MAGIC) return;
        if (c->eip < 0x10000u || c->eip > 0xe5000u) s2_fail("it left the program's code");
        s2_cpu_count++;
        int size = 4, rep = 0;
        uint32_t opc;
        for (;;) {
            opc = fetch8(c);
            if (opc == 0x66) size = 2;
            else if (opc == 0xf3) rep = 1;
            else if (opc == 0xf2) rep = 2;
            else if (opc == 0x26 || opc == 0x2e || opc == 0x36 || opc == 0x3e || opc == 0x64 || opc == 0x65) continue;   /* a segment: all the same here */
            else break;
        }
        modrm m;
        if (opc < 0x40 && (opc & 7) < 6) {                 /* the eight, in their six shapes */
            const int op = (int)(opc >> 3), shape = (int)(opc & 7), sz = (shape & 1) ? size : 1;
            if (shape < 4) {
                decode(c, &m);
                const uint32_t e = gete(c, &m, sz), g = getr(c, m.reg, sz);
                if (shape < 2) { const uint32_t r = alu(c, op, e, g, sz); if (op != 7) sete(c, &m, sz, r); }
                else { const uint32_t r = alu(c, op, g, e, sz); if (op != 7) setr(c, m.reg, sz, r); }
            } else {
                const uint32_t r = alu(c, op, getr(c, S2_EAX, sz), fetch(c, sz), sz);
                if (op != 7) setr(c, S2_EAX, sz, r);
            }
            continue;
        }
        switch (opc) {
        case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47: {
            const int cf = c->cf; setr(c, (int)opc & 7, size, f_add(c, getr(c, (int)opc & 7, size), 1, 0, size)); c->cf = cf; break;
        }
        case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f: {
            const int cf = c->cf; setr(c, (int)opc & 7, size, f_sub(c, getr(c, (int)opc & 7, size), 1, 0, size)); c->cf = cf; break;
        }
        case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
            if (size != 4) s2_fail("a 16-bit push");
            push32(c, c->r[opc & 7]); break;
        case 0x58: case 0x59: case 0x5a: case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f:
            if (size != 4) s2_fail("a 16-bit pop");
            c->r[opc & 7] = pop32(c); break;
        case 0x68: push32(c, fetch32(c)); break;
        case 0x6a: push32(c, (uint32_t)(int32_t)(int8_t)fetch8(c)); break;
        case 0x69: case 0x6b: {
            decode(c, &m);
            const int64_t a = size == 4 ? (int64_t)(int32_t)gete(c, &m, 4) : (int64_t)(int16_t)gete(c, &m, 2);
            const int64_t b = opc == 0x6b ? (int64_t)(int8_t)fetch8(c) : size == 4 ? (int64_t)(int32_t)fetch32(c) : (int64_t)(int16_t)fetch16(c);
            const int64_t p = a * b;
            setr(c, m.reg, size, (uint32_t)p);
            c->cf = c->of = size == 4 ? p != (int64_t)(int32_t)p : p != (int64_t)(int16_t)p;
            break;
        }
        case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
        case 0x78: case 0x79: case 0x7a: case 0x7b: case 0x7c: case 0x7d: case 0x7e: case 0x7f: {
            const int32_t d = (int8_t)fetch8(c);
            if (cond(c, (int)opc & 15)) c->eip += (uint32_t)d;
            break;
        }
        case 0x80: case 0x81: case 0x83: {
            const int sz = opc == 0x80 ? 1 : size;
            decode(c, &m);
            const uint32_t b = opc == 0x83 ? (uint32_t)(int32_t)(int8_t)fetch8(c) : fetch(c, sz);
            const uint32_t r = alu(c, m.reg, gete(c, &m, sz), b, sz);
            if (m.reg != 7) sete(c, &m, sz, r);
            break;
        }
        case 0x84: case 0x85: { const int sz = opc & 1 ? size : 1; decode(c, &m); f_logic(c, gete(c, &m, sz) & getr(c, m.reg, sz), sz); break; }
        case 0x86: case 0x87: {
            const int sz = opc & 1 ? size : 1;
            decode(c, &m);
            const uint32_t e = gete(c, &m, sz);
            sete(c, &m, sz, getr(c, m.reg, sz)); setr(c, m.reg, sz, e);
            break;
        }
        case 0x88: case 0x89: { const int sz = opc & 1 ? size : 1; decode(c, &m); sete(c, &m, sz, getr(c, m.reg, sz)); break; }
        case 0x8a: case 0x8b: { const int sz = opc & 1 ? size : 1; decode(c, &m); setr(c, m.reg, sz, gete(c, &m, sz)); break; }
        case 0x8d: decode(c, &m); if (m.mod == 3) s2_fail("lea of a register"); setr(c, m.reg, size, m.ea); break;
        case 0x8f: decode(c, &m); { const uint32_t v = pop32(c); sete(c, &m, 4, v); } break;
        case 0x90: break;
        case 0x98: if (size == 4) c->r[S2_EAX] = (uint32_t)(int32_t)(int16_t)c->r[S2_EAX]; else setr(c, S2_EAX, 2, (uint32_t)(int32_t)(int8_t)c->r[S2_EAX]); break;
        case 0x99: if (size == 4) c->r[S2_EDX] = (int32_t)c->r[S2_EAX] < 0 ? 0xffffffffu : 0; else setr(c, S2_EDX, 2, (int16_t)c->r[S2_EAX] < 0 ? 0xffffu : 0); break;
        case 0x9b: break;                                  /* wait */
        case 0x9e: { const uint32_t ah = c->r[S2_EAX] >> 8; c->cf = ah & 1; c->pf = ah >> 2 & 1; c->zf = ah >> 6 & 1; c->sf = ah >> 7 & 1; break; }
        case 0x9f: setr(c, 4, 1, (uint32_t)(c->cf | 2 | c->pf << 2 | c->zf << 6 | c->sf << 7)); break;
        case 0xa0: setr(c, S2_EAX, 1, rd(fetch32(c), 1)); break;
        case 0xa1: setr(c, S2_EAX, size, rd(fetch32(c), size)); break;
        case 0xa2: wr(fetch32(c), 1, c->r[S2_EAX]); break;
        case 0xa3: wr(fetch32(c), size, c->r[S2_EAX]); break;
        case 0xa4: case 0xa5: case 0xaa: case 0xab: case 0xac: case 0xad: {
            const int sz = opc & 1 ? size : 1;
            const uint32_t step = c->df ? (uint32_t)-sz : (uint32_t)sz;
            uint32_t n = rep ? c->r[S2_ECX] : 1;
            for (; n; n--) {
                if (opc <= 0xa5) { wr(c->r[S2_EDI], sz, rd(c->r[S2_ESI], sz)); c->r[S2_ESI] += step; c->r[S2_EDI] += step; }
                else if (opc <= 0xab) { wr(c->r[S2_EDI], sz, c->r[S2_EAX]); c->r[S2_EDI] += step; }
                else { setr(c, S2_EAX, sz, rd(c->r[S2_ESI], sz)); c->r[S2_ESI] += step; }
            }
            if (rep) c->r[S2_ECX] = 0;
            break;
        }
        case 0xa8: f_logic(c, c->r[S2_EAX] & fetch8(c), 1); break;
        case 0xa9: f_logic(c, getr(c, S2_EAX, size) & fetch(c, size), size); break;
        case 0xb0: case 0xb1: case 0xb2: case 0xb3: case 0xb4: case 0xb5: case 0xb6: case 0xb7: setr(c, (int)opc & 7, 1, fetch8(c)); break;
        case 0xb8: case 0xb9: case 0xba: case 0xbb: case 0xbc: case 0xbd: case 0xbe: case 0xbf: setr(c, (int)opc & 7, size, fetch(c, size)); break;
        case 0xc0: case 0xc1: case 0xd0: case 0xd1: case 0xd2: case 0xd3: {
            const int sz = opc & 1 ? size : 1;
            decode(c, &m);
            const uint32_t n = opc < 0xd0 ? fetch8(c) : opc < 0xd2 ? 1 : c->r[S2_ECX] & 0xff;
            sete(c, &m, sz, shift(c, m.reg, gete(c, &m, sz), n, sz));
            break;
        }
        case 0xc2: { const uint32_t n = fetch16(c); c->eip = pop32(c); c->r[S2_ESP] += n; break; }
        case 0xc3: c->eip = pop32(c); break;
        case 0xc6: decode(c, &m); sete(c, &m, 1, fetch8(c)); break;
        case 0xc7: decode(c, &m); sete(c, &m, size, fetch(c, size)); break;
        case 0xc9: c->r[S2_ESP] = c->r[S2_EBP]; c->r[S2_EBP] = pop32(c); break;
        case 0xd8: case 0xd9: case 0xda: case 0xdb: case 0xdc: case 0xdd: case 0xde: case 0xdf: x87(c, opc); break;
        case 0xe8: {
            const uint32_t d = fetch32(c), target = c->eip + d;
            if (!hooked(c, target)) { push32(c, c->eip); c->eip = target; }
            break;
        }
        case 0xe9: { const uint32_t d = fetch32(c); c->eip += d; break; }
        case 0xeb: { const int32_t d = (int8_t)fetch8(c); c->eip += (uint32_t)d; break; }
        case 0xf5: c->cf = !c->cf; break;
        case 0xf8: c->cf = 0; break;
        case 0xf9: c->cf = 1; break;
        case 0xfc: c->df = 0; break;
        case 0xfd: c->df = 1; break;
        case 0xf6: case 0xf7: {
            const int sz = opc & 1 ? size : 1;
            decode(c, &m);
            const uint32_t e = gete(c, &m, sz);
            switch (m.reg) {
            case 0: case 1: f_logic(c, e & fetch(c, sz), sz); break;
            case 2: sete(c, &m, sz, ~e); break;
            case 3: { const uint32_t r = f_sub(c, 0, e, 0, sz); sete(c, &m, sz, r); break; }
            case 4: case 5:
                if (sz != 4) s2_fail("a multiply of a width not known");
                if (m.reg == 4) { const uint64_t p = (uint64_t)c->r[S2_EAX] * e; c->r[S2_EAX] = (uint32_t)p; c->r[S2_EDX] = (uint32_t)(p >> 32); c->cf = c->of = c->r[S2_EDX] != 0; }
                else { const int64_t p = (int64_t)(int32_t)c->r[S2_EAX] * (int32_t)e; c->r[S2_EAX] = (uint32_t)p; c->r[S2_EDX] = (uint32_t)((uint64_t)p >> 32); c->cf = c->of = p != (int64_t)(int32_t)p; }
                break;
            default:
                if (sz != 4) s2_fail("a divide of a width not known");
                if (!e) s2_fail("a divide by nought");
                if (m.reg == 6) {
                    const uint64_t n = (uint64_t)c->r[S2_EDX] << 32 | c->r[S2_EAX];
                    if (n / e > 0xffffffffu) s2_fail("a divide that does not fit");
                    c->r[S2_EAX] = (uint32_t)(n / e); c->r[S2_EDX] = (uint32_t)(n % e);
                } else {
                    const int64_t n = (int64_t)((uint64_t)c->r[S2_EDX] << 32 | c->r[S2_EAX]), q = n / (int32_t)e;
                    if (q != (int64_t)(int32_t)q) s2_fail("a divide that does not fit");
                    c->r[S2_EAX] = (uint32_t)q; c->r[S2_EDX] = (uint32_t)(n % (int32_t)e);
                }
            }
            break;
        }
        case 0xfe: case 0xff: {
            const int sz = opc & 1 ? size : 1;
            decode(c, &m);
            switch (m.reg) {
            case 0: { const int cf = c->cf; sete(c, &m, sz, f_add(c, gete(c, &m, sz), 1, 0, sz)); c->cf = cf; break; }
            case 1: { const int cf = c->cf; sete(c, &m, sz, f_sub(c, gete(c, &m, sz), 1, 0, sz)); c->cf = cf; break; }
            case 2: { const uint32_t target = gete(c, &m, 4); if (!hooked(c, target)) { push32(c, c->eip); c->eip = target; } break; }
            case 4: c->eip = gete(c, &m, 4); break;
            case 6: push32(c, gete(c, &m, 4)); break;
            default: s2_fail("an instruction not known");
            }
            break;
        }
        case 0x0f: {
            const uint32_t o2 = fetch8(c);
            if (o2 >= 0x80 && o2 < 0x90) { const uint32_t d = fetch32(c); if (cond(c, (int)o2 & 15)) c->eip += d; break; }
            if (o2 >= 0x90 && o2 < 0xa0) { decode(c, &m); sete(c, &m, 1, (uint32_t)cond(c, (int)o2 & 15)); break; }
            switch (o2) {
            case 0xaf: {
                decode(c, &m);
                const int64_t p = size == 4 ? (int64_t)(int32_t)getr(c, m.reg, 4) * (int32_t)gete(c, &m, 4) : (int64_t)(int16_t)getr(c, m.reg, 2) * (int16_t)gete(c, &m, 2);
                setr(c, m.reg, size, (uint32_t)p);
                c->cf = c->of = size == 4 ? p != (int64_t)(int32_t)p : p != (int64_t)(int16_t)p;
                break;
            }
            case 0xb6: decode(c, &m); setr(c, m.reg, size, gete(c, &m, 1)); break;
            case 0xb7: decode(c, &m); setr(c, m.reg, 4, gete(c, &m, 2)); break;
            case 0xbe: decode(c, &m); setr(c, m.reg, size, (uint32_t)(int32_t)(int8_t)gete(c, &m, 1)); break;
            case 0xbf: decode(c, &m); setr(c, m.reg, 4, (uint32_t)(int32_t)(int16_t)gete(c, &m, 2)); break;
            default: c->eip -= 2; s2_fail("an instruction not known");
            }
            break;
        }
        default:
            c->eip -= 1;
            s2_fail("an instruction not known");
        }
    }
}

uint32_t s2_call(uint32_t entry, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx, int nstack, const uint32_t *stack)
{
    s2_cpu *c = &s2_c;
    if (s2_depth) { fprintf(stderr, "screamer 2: a routine called from inside another's hook (0x%x)\n", entry); return 0; }
    c->r[S2_ESP] = STACK_TOP; c->r[S2_EBP] = 0; c->r[S2_ESI] = 0; c->r[S2_EDI] = 0;
    c->r[S2_EAX] = eax; c->r[S2_EDX] = edx; c->r[S2_EBX] = ebx; c->r[S2_ECX] = ecx;
    c->top = 0; c->cw = 0x127f; c->sw = 0; c->df = 0;
    for (int k = nstack - 1; k >= 0; k--) push32(c, stack[k]);
    push32(c, RET_MAGIC);
    c->eip = entry;
    s2_depth = 1;
    if (!setjmp(s2_stop)) run(c);
    s2_depth = 0;
    return c->r[S2_EAX];
}
