/*
 * SCREAMER 2: THE CAR'S ROUTINES, WRITTEN BY HAND (the port's own text, from what docs/NOTES2.md
 * records of each and the order of its sums in the machine code -- not the decompiler's).  Each
 * takes the place of the converted routine of the same address (tools/s2_set.txt --hook) and
 * must give the same results to the last bit: the test races' traces before and after are
 * compared byte for byte (NOTES2 S2-8).
 *
 * Included by s2_hooks.c before the converted routines; I32 / U32 / F32 are the block's words.
 * The sums are the x87's at 53 bits (S2_FPU_IN), as the game's runtime sets it: a product or a
 * sum of floats is taken in long double and rounded to a double each step, a float only where
 * the game stores one -- and in the game's order, which decides the last bit.
 *
 * A CONSTANT THAT ITS TYPE CANNOT HOLD EXACTLY IS WRITTEN WITH A CAST: `(float)3.14159274f`,
 * `(double)0.8`.  Built so, C reads a bare `3.14159274f` or `0.8` at the processor's full width --
 * the digits, not the game's float or double (0x19810 was four float steps off).  The cast makes
 * it the number of its type.
 */
#ifndef OS_S2R_CAR_H
#define OS_S2R_CAR_H

/* (routines still converted that these call: declared by the converted file after this one) */
static void FUN_00029571(int car, int unused);
static void FUN_00028f96(int car, uint32_t unused);
static void FUN_0001f1ef(int car);
static void FUN_0002073c(int car, uint32_t a, uint32_t b, uint32_t c);
static void FUN_0001248d(int path);
static void FUN_0001257f(int path, int node, uint32_t a, uint32_t b, float t);
static void FUN_0001f3ce(int car);
static void FUN_000174ca(int car);
static void FUN_0001756f(int car);
static void FUN_00019409(int car);
static void FUN_0001f756(int car);
static void FUN_00014acf(int camera);
static void FUN_00013f38(int camera);
static void FUN_00014200(int camera);
static double FUN_00014dc0(float a, float b);
static void FUN_00012d00(int camera, uint32_t car);
static double FUN_00011253(double x);
static void FUN_00013514(int camera);
static void FUN_000135b6(int camera);
static void FUN_00013768(int camera);
static void FUN_000138e8(int camera, uint32_t a, uint32_t b, uint32_t c);
static void FUN_00013a8e(int camera, uint32_t a, uint32_t b, uint32_t c);
static void FUN_0004a831(int out, int handles, uint32_t r2, uint32_t r3, float t);
static void FUN_0001099a(int path, int state, uint32_t r2, uint32_t r3);
static void FUN_00010388(int out, int in, uint32_t a, uint32_t b, float angle);
static void FUN_0001030d(int out, int in, uint32_t a, uint32_t b, float angle);
static uint32_t FUN_0004c01c(int list, int a, int rec);
static uint64_t FUN_00012462(int path, uint32_t high);
static void FUN_00012512(uint32_t path, int node, int count);
static uint32_t FUN_0004bfbe(int list, int set, int rec);
static void FUN_000153c6(int car);
static void FUN_00017726(int car, uint32_t a, uint32_t b, int c);
static float10 FUN_00019cac(int car, int wheel, uint32_t a, uint32_t b, float angle);
static void FUN_0001c7f8(int car);
static void FUN_0004b016(void);
static uint32_t FUN_0004be04(int list, int set, int rec);
static void FUN_00014e20(int car, int set);
static double FUN_000245a3(void);
static void FUN_0004a488(void);
static void FUN_0004a59a(void);
static double FUN_000108f5(int h, int r1, int r2, int r3, float t);
static void FUN_0004af7d(void);
static uint64_t FUN_0002da06(uint32_t a, uint32_t b);
static void FUN_00016824(int car);
static void FUN_0001684e(int car);
static void FUN_000167e2(int car);
static void FUN_00016878(int car, float share);
static void FUN_0002d59c(int path, int state, int a, int b, float by);
static int FUN_00020970(int p, int d, int q, int e, int s, int u);
static void FUN_0001e529(int car);
static void FUN_00019810(int car, float angle);
static void FUN_0001bef1(int car);
static void FUN_0001c4fb(int car);
static void FUN_00020a18(int a, int b);
static double FUN_00019c61(float a, float b, float unused, float k);
static long double FUN_0001766c(int a, int b, int c);
static void FUN_000152f7(int path, int state, int out, uint32_t r3, float t);
static void FUN_0001d9fc(int car);
static void FUN_000204af(int car, int always);
static uint32_t FUN_0004beba(int list, int set, int rec);
static uint32_t FUN_0004c01c(int list, int set, int rec);
static int FUN_0004bd1b(int list, int set);
static uint32_t FUN_0004bda7(int list, int set);
static void FUN_00050b71(int s);
static void FUN_00050a9c(int s, int cont, int entry);
static void FUN_00050ef4(int s);
static void FUN_00050dd3(int s, int cont, int entry);
static void FUN_0005160b(int b, uint32_t r1, uint32_t r2);
static uint64_t FUN_000514fa(int b, uint32_t r1, uint32_t r2, uint32_t r3);
static int FUN_00051521(int b);
static void FUN_00051532(int b, int cont, int entry);
static void FUN_0001226b(int cont, int list);
static int FUN_0003248f(uint32_t name, int list);
static void FUN_00011298(uint32_t room, uint32_t r1, uint32_t r2, uint32_t r3);
static void FUN_00034ec6(void);
static int FUN_0003519f(int n, uint32_t name, uint32_t r2, uint32_t r3);
static void FUN_00035b86(int p, int n);
static uint64_t FUN_00036150(uint32_t r0, uint32_t r1);
static void FUN_0003661d(int letter, int from, int to);
static uint64_t FUN_00036845(uint32_t r0, uint32_t r1);
static void FUN_00037686(void);
static void FUN_00038d48(void);
static void FUN_00035cb0(int p);
static void FUN_00036245(int p, int was, int now, uint32_t r3, float t, int lift);
static void FUN_000366a8(void);
static int FUN_000368e2(int to_all, uint32_t message, uint32_t extra);
static bool FUN_00036d22(uint32_t message, int whole, int page);
static uint64_t FUN_000371fd(uint32_t r0, uint32_t r1);
static void FUN_0003770c(void);
static uint64_t FUN_000379bf(uint32_t r0, uint32_t r1);
static uint32_t FUN_00037b41(int message, int word, int page, uint32_t r3);
static uint64_t FUN_00038996(int enter, uint32_t r1);
static uint32_t FUN_00038b2c(int player, int every, int enter);
static void FUN_00041fcf(void);
static void FUN_0003c9ac(void);
static void FUN_000394f5(void);
static void FUN_00038d8d(void);
static void FUN_00038540(void);
static void FUN_0003b674(void);
static void FUN_00011d6f(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3, float t, int cam);
static void FUN_0003bbc4(void);
static void FUN_00035916(int n, int first, int field, uint8_t kind, int size, int way);
static void FUN_0003ff37(void);
static void FUN_000431b3(void);
static void FUN_0004189f(void);
static void FUN_000420c0(void);
static void FUN_00044f72(int list, int keep);
static void FUN_00044587(int list);
static void FUN_0004254f(void);
static void FUN_00043914(void);
static void FUN_0003edb4(void);
static void FUN_0003f72a(void);
static void FUN_0003bf06(void);
static void FUN_0003988d(void);
static void FUN_0003a870(void);
static void FUN_00040a67(void);
static void FUN_0003e161(void);
static void FUN_0003cbb8(void);
static void FUN_000339e7(void);
static int FUN_0001240d(uint32_t name, int table);
static void s2_dispatch(uint32_t at);
/* (and hand-written ones further down) */

/* THE NAMES THESE ROUTINES HAND ON, at the program's places for them (the converter places only
   those its own routines name; a race without the game's menu has no other copy of the program's
   data): file and block names, formats -- every one these routines name (made by the scratch
   helper names.py from the addresses in this file) */
static void s2r_strings(void)
{
    static const struct { uint32_t at; const char *text; } names[] = {
        { 0x110094u, "%s\\anims\\scene%02d%c.dat" }, { 0x1100adu, "rb" }, { 0x1100b0u, "splines.pth" },
        { 0x1100bcu, "SPLNAM" }, { 0x1100c3u, "map%c" }, { 0x1100c9u, "%s\\anims\\sccar%c%c.dat" },
        { 0x1100e0u, "%s\\anims\\team%c%c.dat" }, { 0x1100f6u, "team" },
        { 0x1100fbu, "%s\\anims\\scene%02d%c.dat" }, { 0x110114u, "map%02d" },
        { 0x11011cu, "%s\\anims\\scene%02d%c.dat" }, { 0x110135u, "env" }, { 0x110139u, "SPLNAM" },
        { 0x110158u, "MV_mauto" }, { 0x110179u, "rt" }, { 0x11017cu, "splines.txt" }, { 0x110d59u, "misc" },
        { 0x110d5eu, "ashadow" }, { 0x110d66u, "%s\\gfx\\cars%c%c%c.dat" }, { 0x110d7cu, "auto%c" },
        { 0x110d83u, "fauto" }, { 0x110d89u, "MV_mauto" }, { 0x110d92u, "rauto" }, { 0x110d98u, "fai" },
        { 0x110eacu, "%s\\data\\local.dat" }, { 0x110ebeu, "%s.txt" }, { 0x110ec5u, "rt" },
        { 0x110ec8u, "TEXT" }, { 0x110ecdu, "TEXT" }, { 0x110f66u, "%s\\data\\gen.dat" },
        { 0x110f76u, "rb" }, { 0x110f79u, "%s\\config\\choice.dat" }, { 0x110f8eu, "rb" },
        { 0x110f91u, "MIL" }, { 0x110f95u, "MIL" }, { 0x110f99u, "MIL" }, { 0x110f9du, "MIL" },
        { 0x110fa1u, "%s\\config\\choice.dat" }, { 0x110fb6u, "wb" }, { 0x110fc8u, "NET_SPACE" },
        { 0x110fd2u, "CPU" }, { 0x110fd6u, "CPU" }, { 0x110fdau, "CPU" }, { 0x110fdeu, "CPU" },
        { 0x110fe2u, "CPU" }, { 0x110fe6u, "CPU" }, { 0x110feau, "gfx\\graf%c.dat" },
        { 0x110ff9u, "load_%c%c.bkk" }, { 0x111007u, "%s %s" }, { 0x11100du, "%s %s" },
        { 0x111013u, "%s%c.pth" }, { 0x11101cu, "%s\\data\\gen.dat" }, { 0x11102cu, "rb" },
        { 0x11113au, "NET_SPACE" }, { 0x111144u, "NET_SPACE" }, { 0x11114eu, "\001%c.%s   " },
        { 0x111158u, "%s %s" }, { 0x11115eu, "%s %s" }, { 0x111164u, "NET_SPACE" },
        { 0x11116eu, "NET_SPACE" }, { 0x111178u, "\001%c.%s   " }, { 0x1111d0u, "NET_SPACE" },
        { 0x11120au, "leag_%c%c.bkk" }, { 0x11121cu, "bkg_4%c.bkk" }, { 0x11124cu, "%s %c" },
        { 0x111252u, "\001%s" }, { 0x111256u, "-" }, { 0x111260u, "bkg_5%c.bkk" }, { 0x111280u, "%s %c" },
        { 0x111292u, "arcade_%c.bkk" }, { 0x1112a0u, "scene" }, { 0x1112e2u, "bkg_6%c.bkk" },
        { 0x1112eeu, "NET_SPACE" }, { 0x1112f8u, "NET_SPACE" }, { 0x111302u, "NET_SPACE" },
        { 0x11130cu, "NET_SPACE" }, { 0x111316u, "NET_SPACE" }, { 0x111320u, "NET_SPACE" },
        { 0x11132eu, "attract" }, { 0x111396u, "\001%03d" }, { 0x1113acu, "\001%03d" },
        { 0x11144au, "tkg_%c%c.bkk" }, { 0x111467u, "leag_0%c.bkk" }, { 0x111488u, "logo%c.bkk" },
        { 0x1114afu, "\001%s" }, { 0x1114b3u, "-" }, { 0x1114bdu, "p_%c.mcm" }, { 0x1114c6u, "bkg_2%c.bkk" },
        { 0x1114f6u, "\001%03d" }, { 0x11150cu, "\001%03d" }, { 0x11151eu, "bkg_1%c.bkk" },
        { 0x111542u, "bkg_6%c.bkk" }, { 0x11154eu, "bkg_7%c.bkk" }, { 0x11155au, ":" },
        { 0x111568u, "hiscore%c.bkk" }, { 0x111576u, "REP%c%c%c%c" }, { 0x111592u, "\001 %s. %s      %s" },
        { 0x1115a3u, "\001%s. %s      %s" }, { 0x1115b3u, "\001 %s. %s      %s" },
        { 0x1115c4u, "\001%s. %s      %s" }, { 0x1115d8u, "bkg_3%c.bkk" }, { 0x1115e8u, "leag_%c%c.bkk" },
        { 0x1115f6u, "\001%c.  %s  %d" }, { 0x111603u, "\001%c.  %s %d" }, { 0x11160fu, "%s %d" },
        { 0x111615u, "%s %d" }, { 0x11161fu, "%s\\gfx\\graf%c.dat" }, { 0x111631u, "menu%c.fnt" },
        { 0x11163cu, "%s\\data\\sound" }, { 0x11164au, "menu" }, { 0x11164fu, "%s\\gfx\\graf%c.dat" },
        { 0x111661u, "menu%c.spr" }, { 0x11166cu, "strings" }, { 0x111674u, "gfx\\graf%c.dat" },
        { 0x111683u, "%s\\gfx\\menu%c%c.dat" }, { 0x111697u, "map%c" }, { 0x11169du, "ashadow" },
        { 0x1116a5u, "%s\\gfx\\cars%c0%c.dat" }, { 0x1116bau, "auto%c" },
        { 0x1116c1u, "%s\\gfx\\envm%c%c.dat" }, { 0x1116d5u, "env" },
        { 0x1116d9u, "%s\\gfx\\cars%c0%c.dat" }, { 0x1116eeu, "auto%c" },
        { 0x1116f5u, "%s\\gfx\\cars%c0%c.dat" }, { 0x11170au, "auto%c" },
        { 0x111711u, "%s\\gfx\\envm%c%c.dat" }, { 0x111725u, "env" }, { 0x111729u, "%s\\gfx\\logo%c.dat" },
        { 0x11173bu, "logo" }, { 0x111740u, "a%clog-00" }, { 0x11174au, "a%clog-00" },
        { 0x111754u, "ALETTER" }, { 0x11175cu, "logo" }, { 0x111761u, "%s\\gfx\\maps%c.dat" },
        { 0x111773u, "maps" }, { 0x111778u, "amap%c-vs" }, { 0x111782u, "maps" },
        { 0x111787u, "%s\\gfx\\plates%c.dat" }, { 0x11179bu, "plates" }, { 0x1117a2u, "a%cplate" },
        { 0x1117abu, "plates" }, { 0x1117b2u, "%s\\gfx\\menu%c%c.dat" }, { 0x1117c6u, "map%c" },
        { 0x1117ccu, "%s\\gfx\\cars%c0%c.dat" }, { 0x1117e1u, "auto%c" },
        { 0x1117e8u, "%s\\gfx\\envm%c%c.dat" }, { 0x1117fcu, "env" },
        { 0x111800u, "%s\\gfx\\cars%c0%c.dat" }, { 0x111815u, "auto%c" },
        { 0x11181cu, "%s\\gfx\\cars%c0%c.dat" }, { 0x111831u, "auto%c" },
        { 0x111838u, "%s\\gfx\\envm%c%c.dat" }, { 0x11184cu, "env" }, { 0x111850u, "%s\\gfx\\maps%c.dat" },
        { 0x111862u, "maps" }, { 0x111867u, "%s\\gfx\\plates%c.dat" }, { 0x11187bu, "plates" },
        { 0x1122ecu, "rb" }, { 0x1122efu, "SPRITES" }, { 0x1122f7u, "SPRITES" }, { 0x1122ffu, "rb" },
        { 0x112302u, "FONTS INFO" }, { 0x11230du, "CHAR INFO" }, { 0x112317u, "FONTS" }, { 0x112334u, "rb" },
        { 0x112353u, "BKK" }, { 0x112357u, "BKKPAL" },
    };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) s2_str(names[i].at, names[i].text);
}
static void FUN_00012dd8(int camera, uint32_t view);
static void FUN_00012d31(int camera, uint32_t view);
static void FUN_00013223(int camera);
static void FUN_00013bab(int camera);
static void FUN_00013e1a(int camera);
static void FUN_00013cbd(int camera);

/*
 * EASING (0x111d6, 0x11212): a value brought towards a target by the easing table's row -- the
 * table the frame's step picks (0x1240c0 -> rows of (0.5 + 0.005 i) ^ (50 x step), S2-5zq):
 *     value = (value - target) x row[i] + target
 * the first for a float, the second for a whole number (cut, not rounded: 0x4ea12)
 */
#define HOOK_111d6
static void FUN_000111d6(int at, float target, int i)
{
    const long double row = F32(U32(0x1240c0) + 4u * (uint32_t)i);
    F32(at) = (float)(((long double)F32(at) - target) * row + target);
}
#define HOOK_11212
static void FUN_00011212(int at, float target, int i)
{
    const long double row = F32(U32(0x1240c0) + 4u * (uint32_t)i);
    I32(at) = (int32_t)(((long double)I32(at) - target) * row + target);
}

/* AN ANGLE EASED towards a target (0x1016b) the short way round: as 0x111d6, but when the two are
   half a turn or more apart the difference is taken through the other side (one turn added or
   taken off it); the result brought back into 0 .. 2 pi (over by the float's bits as a whole
   number, as the game does) */
#define HOOK_1016b
static void FUN_0001016b(int at, float target, int i)
{
    const long double row = F32(U32(0x1240c0) + 4u * (uint32_t)i);
    const float a = F32(at);
    if (a != target) {
        if (a < target) {
            if ((long double)target - a >= (float)3.14159274f) F32(at) = (float)(-(((long double)target - a) + -(float)6.28318548f) * row + target);
            else F32(at) = (float)(((long double)a - target) * row + target);
        } else {
            if ((long double)a - target >= (float)3.14159274f) F32(at) = (float)(-(((long double)target - a) + (float)6.28318548f) * row + target);
            else F32(at) = (float)(((long double)a - target) * row + target);
        }
    }
    if (0.0f > F32(at)) F32(at) = (float)((long double)F32(at) + (float)6.28318548f);
    else if (I32(at) >= 0x40c90fdb) F32(at) = (float)((long double)F32(at) + -(float)6.28318548f);
}

/* the trackside cameras' and the scenery paths' tables emptied (0x11906): ten paths (their
   records at 0x118afc, 0x118c14 and three of each at 0x118b24, 0x118b9c, 0x118cb4, 0x118c3c) and
   forty (0x118d2c, three of each at 0x118dcc, 0x118fac) */
#define HOOK_11906
static void FUN_00011906(void)
{
    for (uint32_t i = 0; i < 10; i++) {
        U32(0x118afc + 4 * i) = 0; U32(0x118c14 + 4 * i) = 0;
        for (uint32_t k = 0; k < 3; k++) { U32(0x118b24 + 4 * k + 0xc * i) = 0; U32(0x118b9c + 4 * k + 0xc * i) = 0; U32(0x118cb4 + 4 * k + 0xc * i) = 0; U32(0x118c3c + 4 * k + 0xc * i) = 0; }
    }
    for (uint32_t i = 0; i < 40; i++) {
        U32(0x118d2c + 4 * i) = 0;
        for (uint32_t k = 0; k < 3; k++) { U32(0x118dcc + 4 * k + 0xc * i) = 0; U32(0x118fac + 0xc * i + 4 * k) = 0; }
    }
}

/* A NODE PUT INTO A PATH'S LIST, kept in order of its place t (node +8; the list's first and last
   at path +0x14 / +0x18, its count +0x1c; a node's previous / next at +0 / +4) -- 0x1257f */
#define HOOK_1257f
static void FUN_0001257f(int path, int node, uint32_t r2, uint32_t r3, float t)
{
    (void)r2; (void)r3;
    F32(node + 8) = t;
    if (t <= F32(U32(path + 0x14) + 8) || F32(U32(path + 0x18) + 8) <= t) {
        if (I32(path + 0x1c) == 0) { I32(node + 4) = 0; I32(node) = 0; I32(path + 0x14) = node; I32(path + 0x18) = node; }
        else if (0.0f <= t) { I32(U32(path + 0x18) + 4) = node; U32(node) = U32(path + 0x18); I32(path + 0x18) = node; I32(node + 4) = 0; }
        else { I32(U32(path + 0x14)) = node; U32(node + 4) = U32(path + 0x14); I32(path + 0x14) = node; I32(node) = 0; }
    } else {
        uint32_t at = U32(path + 0x14);
        while (F32(at + 8) <= t) at = U32(at + 4);
        U32(node + 4) = at; U32(node) = U32(at); I32(U32(at) + 4) = node; I32(at) = node;
    }
    I32(path + 0x1c) += 1;
}

/*
 * A KNOCK (0x19aaa, car, side 0-3, corner 0-3, -, share, force), from the walls and from other
 * cars; not in the attract race.  The side's count (car +0x1c + 4 side) grows by (1 - share) x
 * force / 20000, to 1 at most, and at 1 a knock over 2000 (the force's bits, as a whole number)
 * marks the side for the look (car +0xb9: sides 0-3 = 0x20, 8, 0x40, 0x10).  With DAMAGES on
 * (0x130d54) the force is first scaled by the level's row of MISC.DTR's DAMAGES (0x11cce0); the
 * corner's damage (+0xc + 4 corner, to 2) grows by share x force / 50000, the whole car's (+8, to 2)
 * by share x force / 100000 for a front corner (0, 2) and (1 - share) x force / 100000 for side 1;
 * the corner's spring then eases at the row (1 - d) x 55 + d x 99 (wheel +0x28, cut).
 */
#define HOOK_19aaa
static void FUN_00019aaa(int car, int side, int corner, uint32_t r3, float share, float force)
{
    (void)r3;
    if (U8(0x130d10) & 0x80) return;
    const uint32_t count = (uint32_t)car + 0x1c + 4u * (uint32_t)side;
    F32(count) = (float)((1.0L - share) * force / 20000.0f + F32(count));
    if (I32(count) >= 0x3f800000) {
        I32(count) = 0x3f800000;
        if (s2_bits(force) > 0x44fa0000 && side >= 0 && side < 4) {
            static const uint8_t mark[4] = { 0x20, 8, 0x40, 0x10 };
            U8(car + 0xb9) |= mark[side];
        }
    }
    if (!I32(0x130d54)) return;
    force = (float)((long double)force * F32(0x11cce0 + 4u * U32(0x130d4c)));
    const uint32_t hurt = (uint32_t)car + 0xc + 4u * (uint32_t)corner;
    F32(hurt) = (float)((long double)share * force / 50000.0f + F32(hurt));
    if (I32(hurt) > 0x40000000) I32(hurt) = 0x40000000;
    if (corner == 2 || corner == 0) F32(car + 8) = (float)((long double)share * force / 100000.0f + F32(car + 8));
    if (side == 1) F32(car + 8) = (float)((1.0L - share) * force / 100000.0f + F32(car + 8));
    if (I32(car + 8) > 0x40000000) I32(car + 8) = 0x40000000;
    I32(car + 0x59c + corner * 0x70) = (int32_t)((1.0L - F32(hurt)) * 55.0f + (long double)F32(hurt) * 99.0f);
}

/*
 * THE ROAD'S KIND under the car (+0x6b0), what it does to the look and the reflections (0x1f1ef):
 *   kind  7  flags 0x10, the reflections' picture 0x8000      34  flags 0x18, 0x8000
 *        13  flags 8,    0x5556                                42  flags 8,    0
 *        43  flags 8,    0xaaaa                                44  flags 0x18, 0
 * (car +0xba, its 0x18 cleared first; the picture car +0xc80); any other kind picture 0.  The
 * reflections slide 0.03 a unit on these (tunnels and the like) but kinds 42 and any other's 0.002
 * (0xb04a8) -- set for the car the camera follows (0x11f0c8) only.
 */
#define HOOK_1f1ef
static void FUN_0001f1ef(int car)
{
    static const struct { uint32_t kind; uint8_t flags; uint32_t picture; int fast; } road[] = {
        { 7, 0x10, 0x8000, 1 }, { 13, 8, 0x5556, 1 }, { 0x22, 0x18, 0x8000, 1 },
        { 0x2a, 8, 0, 0 }, { 0x2b, 8, 0xaaaa, 1 }, { 0x2c, 0x18, 0, 1 },
    };
    U8(car + 0xba) &= 0xe7;
    int fast = 0;
    U32(car + 0xc80) = 0;
    for (unsigned k = 0; k < sizeof road / sizeof road[0]; k++)
        if (U32(car + 0x6b0) == road[k].kind) { U8(car + 0xba) |= road[k].flags; U32(car + 0xc80) = road[k].picture; fast = road[k].fast; break; }
    if (I32(car) == I32(0x11f0c8)) I32(0xb04a8) = fast ? 0x3cf5c28f : 0x3b03126f;     /* 0.03 / 0.002 */
}

/* a camera of the second kind (0x135b6): it starts at its row's place (+0x60..) when it is newly
   chosen (+0x24 against +0x28), and while it stays it drifts with the car -- k = the row's +0x7c x
   the car object's +0x28 x 111 -- along the way it looks:
       x += cos(pitch) sin(heading) k step,  y += sin(pitch) k step,  z += -cos(pitch) cos(heading) k step
   each added as a double to the whole number and cut; looking at the car, the roll the row's +0x74 */
#define HOOK_135b6
static void FUN_000135b6(int camera)
{
    const uint32_t row = (uint32_t)camera + (uint32_t)I32(camera + 0x24) * 0x20u, obj = U32(U32(camera + 0xadc) + 0x990);
    if (I32(camera + 0x24) == I32(camera + 0x28)) {
        if (F32(row + 0x7c) != 0.0f) {
            float k = (float)((long double)F32(row + 0x7c) * F32(obj + 0x28));
            k = (float)((long double)k * 111.0f);
            const long double step = F32(0x133350), p = F32(camera + 0x54), h = F32(camera + 0x58);
            I32(camera + 0x48) = (int32_t)(double)((double)(cosl(p) * sinl(h) * k * step) + (double)I32(camera + 0x48));
            I32(camera + 0x4c) = (int32_t)(double)((double)(sinl(p) * k * step) + (double)I32(camera + 0x4c));
            I32(camera + 0x50) = (int32_t)(double)((double)(-cosl(p) * cosl(h) * k * step) + (double)I32(camera + 0x50));
        }
    } else
        for (uint32_t i = 0; i < 3; i++) U32(camera + 0x48 + 4 * i) = U32(row + 0x60 + 4 * i);
    FUN_000338ed((uint32_t)camera + 0x54, obj, (uint32_t)camera + 0x48);
    F32(camera + 0x5c) = F32(row + 0x74);
    I32(camera + 0x1c) = 200;
}

/* a camera of the fourth kind (0x138e8): circling the car 1100 out, rising and falling 440 +- 220
   (twice a round of its own +0x38), turned round by its +0x3c of a turn (0x10388); both go on by
   (the row's +0x7c x 0.5 + 0.1) x the step, round again past 1; looking at the car, no roll */
#define HOOK_138e8
static void FUN_000138e8(int camera, uint32_t r1, uint32_t r2, uint32_t r3)
{
    (void)r1;
    const uint32_t row = (uint32_t)camera + (uint32_t)I32(camera + 0x24) * 0x20u, obj = U32(U32(camera + 0xadc) + 0x990);
    I32(camera + 0x48) = 1100;
    I32(camera + 0x4c) = (int32_t)(sinl((long double)F32(camera + 0x38) * (float)12.566371f) * 220.0 + 440.0);
    I32(camera + 0x50) = 0;
    const uint32_t sp = s2_sp, turned = s2_push(12);
    FUN_00010388((int)turned, camera + 0x48, r2, r3, (float)((long double)F32(camera + 0x3c) * (float)6.28318548f));
    for (uint32_t k = 0; k < 3; k++) I32(camera + 0x48 + 4 * k) = I32(obj + 4 * k) + I32(turned + 4 * k);
    s2_sp = sp;
    for (uint32_t at = 0x3c; ; at = 0x38) {
        F32(camera + at) = (float)(double)((double)(((long double)F32(row + 0x7c) * 0.5 + (double)0.1) * F32(0x133350)) + (double)F32(camera + at));
        if (I32(camera + at) > 0x3f800000) F32(camera + at) = (float)((long double)F32(camera + at) + -1.0f);
        if (at == 0x38) break;
    }
    FUN_000338ed((uint32_t)camera + 0x54, obj, (uint32_t)camera + 0x48);
    I32(camera + 0x5c) = 0;
    I32(camera + 0x1c) = 200;
}

/* A PATH SET READ (0x11c0c, the menu's moving pictures: SCENEn.PTH): a 100-byte head copied
   from `head` into a new block (*out); then three paths from the file -- each a 0x30-byte record
   (reset by 0x12462, then read over), its count of 0x2c-byte nodes (+0x1c) read into a block of
   their own, the count set to 0 and the nodes put into the path in order (0x12512) -- their
   records into paths[3], their nodes into nodes[3], and the three into the head at +0x54.. */
#define HOOK_11c0c
static void FUN_00011c0c(int out, int paths, int nodes, uint32_t head, int file)
{
    U32(out) = s2m_malloc(100);
    memmove(MP(U32(out)), MP(head), 100);
    for (uint32_t i = 0; i < 3; i++) {
        uint32_t p = s2m_malloc(0x30);
        if (p) p = (uint32_t)FUN_00012462((int)p, 0);
        U32(paths + 4 * i) = p;
        FUN_0004ee3a((uint32_t)file, p, 0x30);
        const int n = I32(p + 0x1c);
        U32(nodes + 4 * i) = s2m_malloc((uint32_t)n * 0x2cu);
        FUN_0004ee3a((uint32_t)file, U32(nodes + 4 * i), n * 0x2c);
        I32(p + 0x1c) = 0;
        FUN_00012512(p, I32(nodes + 4 * i), n);
    }
    for (uint32_t i = 0; i < 3; i++) U32(U32(out) + 0x54 + 4 * i) = U32(paths + 4 * i);
}

/* THE SMOKE'S PUFFS, a frame (0x2073c): each of a car's sixteen puff records (0x28 bytes from car
   +0x228) that is on (+8) has its object (+0) put at its place (+0x18..) -- or, seen from the car
   itself in view 3, at its offset turned by the car's heading and pitch (0x10388, 0x1030d; into
   +0xc..) from the car's object -- and its animation run on by 1.5 x the step (0x4c0cf); one whose
   animation is over is switched off and taken out of the engine's list (0x4c01c) */
#define HOOK_2073c
static void FUN_0002073c(int car, uint32_t r1, uint32_t r2, uint32_t r3)
{
    (void)r1;
    const uint32_t obj = U32(car + 0x990);
    for (uint32_t rec = (uint32_t)car + 0x228; rec < (uint32_t)car + 0x4a8; rec += 0x28) {
        if (!I32(rec + 8)) continue;
        const uint32_t engine = U32(0x11da20u + U32(rec) * 0x1cu), camera = U32(0x1192f4);
        if (I32(camera + 0xae4) == 3 && I32(car) == I32(U32(camera + 0xadc))) {
            const uint32_t sp = s2_sp, turned = s2_push(12);
            FUN_00010388((int)turned, (int)rec + 0x18, r2, r3, -F32(obj + 0x10));
            FUN_0001030d((int)rec + 0xc, (int)turned, 0, 0, F32(obj + 0xc));
            s2_sp = sp;
            for (uint32_t k = 0; k < 3; k++) I32(engine + 8 + 4 * k) = I32(rec + 0xc + 4 * k) + I32(obj + 4 * k);
        } else
            for (uint32_t k = 0; k < 3; k++) U32(engine + 8 + 4 * k) = U32(rec + 0x18 + 4 * k);
        if (FUN_0004c0cf(0x11da14, rec, r2, r3, (float)((long double)F32(0x133350) * 1.5)) < 0) {
            I32(rec + 8) = 0;
            FUN_0004c01c(0x11da14, 0x11d6f4, (int)rec);
        }
    }
}

/*
 * A CAR'S PICTURES LET GO (0x11a28): the ten models' blocks (0x118afc, 0x118c14, and three each
 * at 0x118b24 0x118b9c 0x118cb4 0x118c3c) and the forty at 0x118d2c (three each at 0x118dcc and
 * 0x118fac), every one that is there.
 */
#define HOOK_11a28
static void FUN_00011a28(void)
{
    for (int i = 0; i < 10; i++) {
        if (U32(0x118afcu + 4u * i)) s2m_free(U32(0x118afcu + 4u * i));
        if (U32(0x118c14u + 4u * i)) s2m_free(U32(0x118c14u + 4u * i));
        for (int j = 0; j < 3; j++) {
            const uint32_t k = 4u * j + 0xcu * i;
            if (U32(0x118b24u + k)) s2m_free(U32(0x118b24u + k));
            if (U32(0x118b9cu + k)) s2m_free(U32(0x118b9cu + k));
            if (U32(0x118cb4u + k)) s2m_free(U32(0x118cb4u + k));
            if (U32(0x118c3cu + k)) s2m_free(U32(0x118c3cu + k));
        }
    }
    for (int i = 0; i < 0x28; i++) {
        if (U32(0x118d2cu + 4u * i)) s2m_free(U32(0x118d2cu + 4u * i));
        for (int j = 0; j < 3; j++) {
            const uint32_t k = 4u * j + 0xcu * i;
            if (U32(0x118dccu + k)) s2m_free(U32(0x118dccu + k));
            if (U32(0x118facu + k)) s2m_free(U32(0x118facu + k));
        }
    }
}

/*
 * THE WHEELS' PUSH SIDEWAYS (0x19810): the car's four wheel vectors (+0x544, 12 bytes each) cut
 * to their part along the ground's direction at the angle (+ a quarter turn, twice) -- and only
 * when the speed is over half the top speed (+0x500 / +0x5c - 0.5 > 0), that part times
 * (that * 0.8 + 0.6).  Below it they are left alone.
 */
#define HOOK_19810
static void FUN_00019810(int car, float angle)
{
    angle = (float)((long double)angle + (float)1.5707964f);
    const float c = (float)sinl((long double)angle + (float)1.5707964f);
    const float s = (float)-cosl((long double)angle + (float)1.5707964f);
    const float zero = 0.0f;
    for (int i = 0; i < 4; i++) {
        const uint32_t w = (uint32_t)car + 0x544u + (uint32_t)i * 0xcu;
        const float along = (float)((long double)F32(w + 4) * zero + (long double)F32(w) * c + (long double)F32(w + 8) * s);
        const float k = (float)((long double)F32(car + 0x500) / F32(car + 0x5c) + -0.5);
        if (0.0f < k) {
            const float m = (float)((long double)k * (double)0.8 + (double)0.6);
            const float x = (float)((long double)c * along), y = (float)((long double)zero * along), z = (float)((long double)s * along);
            F32(w) = (float)((long double)x * m); F32(w + 4) = (float)((long double)y * m); F32(w + 8) = (float)((long double)z * m);
        }
    }
}

/*
 * A POINT ON A CURVE OF VALUES (0x126ad): the record's list of keys (+0x14 the first, +0x18 the
 * last; a key: +0 the one before, +4 the next, +8 its time, +0xc its value, +0x10 the slope out,
 * +0x14 the slope in, +0x1c its kind -- 2 = a cubic, the only kind there is) read at the time
 * (held to the first key's time, and to just under the last's).  +8 = the value, +0xc = how fast
 * it changes.  Only when the record is in use (+0x1c).
 */
#define HOOK_126ad
static void FUN_000126ad(int rec, float t)
{
    if (!I32(rec + 0x1c)) return;
    const uint32_t first = U32(rec + 0x14), last = U32(rec + 0x18);
    if (!(t >= F32(first + 8))) t = F32(first + 8);
    else if (t >= F32(last + 8)) t = (float)((long double)F32(last + 8) + -(double)0.001);
    uint32_t key = U32(first + 4);
    while (t >= F32(key + 8)) key = U32(key + 4);
    const float t1 = F32(key + 8), v1 = F32(key + 0xc), in = F32(key + 0x14);
    const uint32_t prev = U32(key);
    const float t0 = F32(prev + 8), v0 = F32(prev + 0xc), out = F32(prev + 0x10);
    if (I32(prev + 0x1c) != 2) { fprintf(stderr, "\nIllegal interpolation"); FUN_000ae662(1); return; }
    float len = (float)((long double)t1 - t0);
    const float u = (float)(((long double)t - t0) / len);
    len = (float)(double)((double)len * (double)(1.0 / 3.0));
    const float a = (float)((long double)out * len + v0);
    const float b2 = (float)((long double)v1 - (long double)in * len);
    const float c3 = (float)((((long double)v1 - (long double)b2 * 3.0f) + (long double)a * 3.0f) - v0);
    const float c2 = (float)(((long double)b2 * 3.0f - (long double)a * 6.0f) + (long double)v0 * 3.0f);
    const float c1 = (float)((long double)a * 3.0f - (long double)v0 * 3.0f);
    F32(rec + 8) = (float)(((((long double)c3 * u + c2) * u + c1) * u) + v0);
    F32(rec + 0xc) = (float)(((((long double)c3 * 3.0f) * u + (long double)c2 * 2.0f) * u + c1) / len);
}

/*
 * THE CAMERA, EACH FRAME (0x12dff) -- not while the cameras are held (0x11f0a8 bit 8).  Its record
 * asks (+0xae0 bit 4) for +0x30; the view keys (0x132b4) are the port's own (s2_view).  The
 * dashboard view (6) of a car not the player's, when the game says so (0x11f14c bit 1), turns to
 * view 2.  When it is to move (the second word): the view's kind (+0xae4) places it -- 1 the fixed
 * place, 2 in the car, 3 the chase from above (0x13f38), 4 the trackside cameras (0x14200), 5 the
 * mirror's, 6 the dashboard's -- and tells the car whether it is seen from inside (+0xc74: 0 the
 * fixed place, else 2); the first time (+0x18 none yet) it is put where it is going at once
 * (+0x48.. the place, +0x54.. the angles), and the sounds forget how far they were (0xaf45c: the
 * listener jumped, no Doppler from it).  The
 * picture's size and eye line (0x4f6c4: 320 wide, +0x1c rows, the eye at 0.65 of them, of half of
 * them less 2 with +0xae0 bit 0x20) are the port's own.  Then the angles in 2048ths of a turn,
 * cut (+0xa64..), the place (+0xa70..); and the sounds' distances forgotten again when the trackside
 * camera (+0x24, with view 1) or the view or the car changed.
 */
#define HOOK_12dff
static void FUN_00012dff(int camera, int move)
{
    if (U8(0x11f0a8) & 8) return;
    if (U8(camera + 0xae0) & 4) I32(camera + 0x30) = 1;
    if ((U8(0x11f14c) & 1) && I32(camera + 0xae4) == 6 && I32(U32(camera + 0xadc)) != I32(0x11f0d0)) FUN_00012d31(camera, 2);
    if (move) {
        const uint32_t car = U32(camera + 0xadc);
        switch (I32(camera + 0xae4)) {
        case 1: FUN_00013223(camera); I32(car + 0xc74) = 0; break;
        case 2: FUN_00013bab(camera); I32(car + 0xc74) = 2; break;
        case 3: FUN_00013f38(camera); I32(car + 0xc74) = 2; break;
        case 4: FUN_00014200(camera); I32(car + 0xc74) = 2; break;
        case 5: FUN_00013e1a(camera); I32(car + 0xc74) = 2; break;
        case 6: FUN_00013cbd(camera); I32(car + 0xc74) = 2; break;
        default: break;
        }
        if (!I32(camera + 0x18)) {
            memmove(MP(camera), MP(camera + 0x48), 12);
            memmove(MP(camera + 0xc), MP(camera + 0x54), 12);
            FUN_000af45c(U32(0x124274));
            I32(camera + 0x18) = 1;
        }
    }
    for (uint32_t k = 0; k < 3; k++)
        U32(camera + 0xa64 + 4 * k) = (uint32_t)(int32_t)truncl((long double)F32(camera + 0x54 + 4 * k) * 1024.0f / (float)3.14159274f) & 0x7ffu;
    U32(camera + 0xa70) = U32(camera + 0x48); U32(camera + 0xa74) = U32(camera + 0x4c); U32(camera + 0xa78) = U32(camera + 0x50);
    if ((I32(camera + 0x24) != I32(camera + 0x28) && I32(camera + 0xae4) == 1) || I32(camera + 0xae4) != I32(camera + 0x2c)) {
        I32(camera + 0x28) = I32(camera + 0x24);
        FUN_000af45c(U32(0x124274));
    }
    if (U32(camera + 0xadc) != U32(camera + 0x20)) {
        U32(camera + 0x20) = U32(camera + 0xadc);
        FUN_000af45c(U32(0x124274));
    }
    I32(camera + 0x2c) = I32(camera + 0xae4);
}

/*
 * THE CHASE CAMERA (0x13f38, view kind 3): 1300 behind the car and 200 over it, looking down
 * 0.274 of a radian.  Its turn follows the car's heading (+0x10) led towards the way the car is
 * going (+0x30, first kept within 0..2 pi) by 0.3 of the angle between them times the share of the
 * top speed (+0x28 / the car's +0x5c); its easing goes 95 less ten times that share.  New to the
 * view: put there at once; else eased (0x111d6, 0x1016b, the height 0x11212).
 */
#define HOOK_13f38
static void FUN_00013f38(int camera)
{
    const uint32_t car = U32(camera + 0xadc), obj = U32(car + 0x990);
    if (I32(obj + 0x30) > 0x40c90fdb) F32(obj + 0x30) = (float)((long double)F32(obj + 0x30) + -(float)6.28318548f);
    else if (0.0f > F32(obj + 0x30)) F32(obj + 0x30) = (float)((long double)F32(obj + 0x30) + (float)6.28318548f);
    const int above = 200;
    const int ease = (int32_t)truncl(95.0f - (long double)F32(obj + 0x28) / F32(car + 0x5c) * 10.0f);
    const double share = (double)((long double)F32(obj + 0x28) / F32(car + 0x5c));
    const float lead = (float)((long double)FUN_00014dc0(F32(obj + 0x10), F32(obj + 0x30)) * (double)0.3 * share);
    const float yaw = (float)FUN_00014dc0(F32(obj + 0x10), lead);
    if (I32(camera + 0x2c) != I32(camera + 0xae4)) {
        F32(camera + 0x34) = 1300.0f;
        F32(camera + 0x5c) = 0.0f;
        F32(camera + 0x54) = (float)-0.274f;
        F32(camera + 0x58) = yaw;
        I32(camera + 0x4c) = (int32_t)truncl((long double)(I32(obj + 4) + above) - sinl((long double)F32(camera + 0x54)) * F32(camera + 0x34));
    } else {
        FUN_000111d6(camera + 0x34, 1300.0f, 0x50);
        FUN_0001016b(camera + 0x5c, 0.0f, ease);
        FUN_0001016b(camera + 0x54, (float)-0.274f, ease);
        FUN_0001016b(camera + 0x58, yaw, ease);
        const int height = (int32_t)truncl((long double)(I32(obj + 4) + above) - sinl((long double)F32(camera + 0x54)) * F32(camera + 0x34));
        FUN_00011212(camera + 0x4c, (float)height, 0x3c);
    }
    I32(camera + 0x48) = (int32_t)truncl((long double)I32(obj) - sinl((long double)F32(camera + 0x58)) * F32(camera + 0x34) * cosl((long double)F32(camera + 0x54)));
    I32(camera + 0x50) = (int32_t)truncl(cosl((long double)F32(camera + 0x58)) * F32(camera + 0x34) * cosl((long double)F32(camera + 0x54)) + (long double)I32(obj + 8));
    I32(camera + 0x1c) = 200;
}

/*
 * A POINT TURNED BY THREE ANGLES, the game's way (0x14200's, 0x1d9fc's): about the view's line
 * (roll), then up (pitch), then round (the turn) -- each sine and cosine kept as a float, each
 * step's three coordinates kept as floats.  A camera's: roll +0x5c, pitch -(+0x54), turn +0x58.
 */
static void s2r_turn3(float v[3], float roll, float pitch, float turn)
{
    float s = (float)sinl((long double)roll), c = (float)cosl((long double)roll);
    float x = (float)((long double)v[0] * c - (long double)v[1] * s), y = (float)((long double)v[1] * c + (long double)v[0] * s);
    v[0] = x; v[1] = y;
    s = (float)sinl((long double)pitch); c = (float)cosl((long double)pitch);
    y = (float)((long double)v[1] * c + (long double)v[2] * s);
    float z = (float)((long double)v[2] * c - (long double)v[1] * s);
    v[1] = y; v[2] = z;
    s = (float)sinl((long double)turn); c = (float)cosl((long double)turn);
    x = (float)((long double)v[0] * c - (long double)v[2] * s);
    z = (float)((long double)v[2] * c + (long double)v[0] * s);
    v[0] = x; v[2] = z;
}
static void s2r_turn_by_camera(int camera, float v[3]) { s2r_turn3(v, F32(camera + 0x5c), -F32(camera + 0x54), F32(camera + 0x58)); }

/*
 * THE CAR'S FOUR CORNERS (0x1d9fc): half its length (+0x4d0) straight ahead and half its width
 * (+0x4c8) to the side, turned by the car's roll (-(+0x4ec)), pitch (+0x4e4) and turn (+0x4e8),
 * from its middle (+0x4d8.., whole numbers): front-side, front-other side, back-side, back-other
 * (+0x52c, +0x514, +0x538, +0x520, three floats each); and the wheels'
 * rows (+0x5a0 / +0x5a8, 0x70 apart) take x and z of the four from +0x514 on, cut.
 */
#define HOOK_1d9fc
static void FUN_0001d9fc(int car)
{
    float ahead[3] = { 0.0f, 0.0f, -1.0f }, side[3] = { -1.0f, 0.0f, 0.0f };
    s2r_turn3(ahead, -F32(car + 0x4ec), F32(car + 0x4e4), F32(car + 0x4e8));
    const float half = (float)((long double)I32(car + 0x4d0) * (double)0.5);
    for (int k = 0; k < 3; k++) ahead[k] = (float)((long double)ahead[k] * half);
    s2r_turn3(side, -F32(car + 0x4ec), F32(car + 0x4e4), F32(car + 0x4e8));
    const float width = (float)((long double)I32(car + 0x4c8) * (double)0.5);
    for (int k = 0; k < 3; k++) side[k] = (float)((long double)side[k] * width);
    static const uint32_t at[4] = { 0x52c, 0x514, 0x538, 0x520 };
    static const int sa[4] = { 1, 1, -1, -1 }, ss[4] = { 1, -1, 1, -1 };
    for (int i = 0; i < 4; i++) {
        const uint32_t o = (uint32_t)car + at[i];
        F32(o)     = (float)(((long double)I32(car + 0x4d8) + sa[i] * (long double)ahead[0]) + ss[i] * (long double)side[0]);
        F32(o + 8) = (float)(((long double)I32(car + 0x4e0) + sa[i] * (long double)ahead[2]) + ss[i] * (long double)side[2]);
        F32(o + 4) = (float)(((long double)I32(car + 0x4dc) + sa[i] * (long double)ahead[1]) + ss[i] * (long double)side[1]);
    }
    for (uint32_t i = 0; i < 4; i++) {
        I32(car + 0x5a0 + i * 0x70) = (int32_t)truncl((long double)F32(car + 0x514 + i * 0xc));
        I32(car + 0x5a8 + i * 0x70) = (int32_t)truncl((long double)F32(car + 0x51c + i * 0xc));
    }
}

/*
 * THE CAMERA FIXED TO THE CAR (0x14200, view kind 4): at the car's camera offset (+0xac along,
 * +0xb0 up, whole numbers), turned with the car.  Its pitch is half the car's pitch and the
 * offset's (+0xc18) less 0.3, its roll half the car's and +0xc20's, its turn the car's and +0xc1c's
 * -- new to the view (and not held, +0x30) at once, else eased (0x1016b: the angle between them,
 * by 60; the turn by 80).  Placed at the car (and its height +0x1c and +0x218 over it) plus the
 * two offsets turned the camera's way (0x14200 turns them one by one, the along one first).
 * (+0x44: when it is set the game turns whatever its stack held; nothing in a race sets it.)
 */
#define HOOK_14200
static void FUN_00014200(int camera)
{
    const uint32_t car = U32(camera + 0xadc), obj = U32(car + 0x990);
    const int now = I32(camera + 0x30) == 0 && I32(camera + 0x2c) != I32(camera + 0xae4);
    if (now) F32(camera + 0x54) = (float)(((long double)F32(obj + 0xc) + F32(car + 0xc18)) * (double)0.5 + -(double)0.3);
    else FUN_0001016b(camera + 0x54, (float)((long double)FUN_00014dc0(F32(obj + 0xc), -F32(car + 0xc18)) * (double)0.5 + -(double)0.3), 0x3c);
    if (now) F32(camera + 0x5c) = (float)(((long double)F32(obj + 0x14) + F32(car + 0xc20)) * (double)0.5);
    else FUN_0001016b(camera + 0x5c, (float)((long double)FUN_00014dc0(F32(obj + 0x14), -F32(car + 0xc20)) * (double)0.5), 0x3c);
    float along[3] = { 0.0f, 0.0f, 0.0f }, up[3] = { 0.0f, 0.0f, 0.0f };
    if (I32(camera + 0x44) == 0) {
        along[2] = (float)I32(car + 0xac);
        up[1] = (float)I32(car + 0xb0);
        if (I32(camera + 0x2c) != I32(camera + 0xae4)) F32(camera + 0x58) = (float)((long double)F32(obj + 0x10) + F32(car + 0xc1c));
        else FUN_0001016b(camera + 0x58, (float)((long double)F32(obj + 0x10) + F32(car + 0xc1c)), 0x50);
    }
    s2r_turn_by_camera(camera, along);
    s2r_turn_by_camera(camera, up);
    I32(camera + 0x48) = (int32_t)truncl((long double)I32(obj) + along[0] + up[0]);
    I32(camera + 0x50) = (int32_t)truncl((long double)I32(obj + 8) + along[2] + up[2]);
    I32(camera + 0x4c) = (int32_t)truncl((long double)(I32(obj + 4) + I32(obj + 0x1c) + I32(car + 0x218)) + along[1] + up[1]);
    I32(camera + 0x1c) = 200;
}

/*
 * A MOVING PART'S PLACE AT A TIME (0x129d5): the part's list of stretches (+4 the next; +0x28 /
 * +0x2c its first and last time, 0..1) at the time held to 0..1; the stretch's three value curves
 * (+0x54 +0x58 +0x5c, read by 0x126ad at the share of the stretch gone) give the place (+0x30..)
 * and, made one long, the direction it moves in (+0x3c..; none if it does not move).  Gives the
 * stretch back (and the second word as it came: the game's EDX, which the callers read on).
 */
#define HOOK_129d5
static uint64_t FUN_000129d5(int part, uint32_t edx, uint32_t ebx, uint32_t ecx, float t)
{
    (void)ebx; (void)ecx;
    if (0.0f > t) t = 0.0f;
    else if (!(1.0f >= t)) t = 1.0f;
    int seg = part;
    while (U32(seg + 4) != 0 && t >= F32(seg + 0x2c)) seg = I32(seg + 4);
    t = (float)(((long double)t - F32(seg + 0x28)) / ((long double)F32(seg + 0x2c) - F32(seg + 0x28)));
    FUN_000126ad(I32(seg + 0x54), t);
    FUN_000126ad(I32(seg + 0x58), t);
    FUN_000126ad(I32(seg + 0x5c), t);
    for (uint32_t k = 0; k < 3; k++) F32(seg + 0x30 + 4 * k) = F32(U32(seg + 0x54 + 4 * k) + 8);
    for (uint32_t k = 0; k < 3; k++) F32(seg + 0x3c + 4 * k) = F32(U32(seg + 0x54 + 4 * k) + 0xc);
    const uint32_t d = (uint32_t)seg + 0x3c;
    float len = (float)sqrt((double)((long double)F32(d + 4) * F32(d + 4) + (long double)F32(d) * F32(d) + (long double)F32(d + 8) * F32(d + 8)));
    float v[3] = { 0.0f, 0.0f, 0.0f };
    if (s2_bits(len) & 0x7fffffff) {
        len = (float)(1.0L / len);
        for (int k = 0; k < 3; k++) v[k] = (float)((long double)F32(d + 4u * (uint32_t)k) * len);
    }
    for (int k = 0; k < 3; k++) F32(d + 4u * (uint32_t)k) = v[k];
    return (uint64_t)edx << 32 | (uint32_t)seg;
}

/*
 * A LAP DONE (0x201ee): the car's laps (+0x4a8) go on by the number given; only a lap not done
 * before (over +0x4ac) counts.  Not in the attract race (mode 0x80): a car whose times are kept
 * (+0xb4) with a lap (+0xc3c) under its best (+0xc40) has a new best, and under the track's
 * (0x130d18, not when that is not to be kept, 0x130d11 bit 1) a new record, written out (0x132fa5)
 * and, for the player's car, shown 4 s (0x13307c).  A race (+0xc54) under the car's best (+0xc68)
 * keeps the five split times (+0xc44 -> +0xc58), and under the track's best (0x130d2c) they are the
 * track's (0x130d1c); the lap is shown 4 s (0x13304c).  The last lap (0x130d48) of a race that has
 * laps (not the free run, mode 8): the car has finished (+0x4c bits 1, 2), +4 = a fifth of the
 * places behind it (the cars, 0x130c48, less its place +0xc30); for a car whose times are kept a
 * race time under the track's best (0x130d14) is the record (0x132fb1), and the camera turns to
 * the finish: in a two-player game that car's own camera, view 1 with flags 0x2b; else, for the
 * player's car, the camera (0x1192f4) -- its view kept for after (0x130d68) unless view 1 or the
 * bit -- follows it in view 1 with flags 0xb.  Then the lap's clock (+0xc3c) starts again.
 * (Each "under" is the x87's: not over or the same, so a time that is no number counts.)
 */
#define HOOK_201ee
static void FUN_000201ee(int car, int by)
{
    I32(car + 0x4a8) += by;
    if (I32(car + 0x4a8) <= I32(car + 0x4ac)) return;
    I32(car + 0x4ac) = I32(car + 0x4a8);
    if (!(U8(0x130d10) & 0x80)) {
        if (I32(car + 0xb4) && !(F32(car + 0xc3c) >= F32(car + 0xc40))) {
            F32(car + 0xc40) = F32(car + 0xc3c);
            if (!(F32(car + 0xc40) >= F32(0x130d18)) && !(U8(0x130d11) & 1)) {
                F32(0x130d18) = F32(car + 0xc40);
                FUN_00037e06(0x132fa5, 0, 0, 0, F32(0x130d18));
                if (I32(car) == I32(0x11f0d0)) F32(0x13307c) = 4.0f;
            }
        }
        if (!(F32(car + 0xc54) >= F32(car + 0xc68))) {
            for (uint32_t i = 0; i < 5; i++) F32(car + 0xc58 + 4 * i) = F32(car + 0xc44 + 4 * i);
            if (!(F32(car + 0xc54) >= F32(0x130d2c)))
                for (uint32_t i = 0; i < 5; i++) F32(0x130d1c + 4 * i) = F32(car + 0xc44 + 4 * i);
        }
        F32(0x13304c) = 4.0f;
        if (I32(car + 0x4a8) == I32(0x130d48) && !(U8(0x130d10) & 8)) {
            U8(car + 0x4c) |= 3;
            F32(car + 4) = (float)((long double)(I32(0x130c48) - I32(car + 0xc30)) * (double)0.2);
            if (I32(car + 0xb4)) {
                if (!(F32(car + 0xc38) >= F32(0x130d14)) && !(U8(0x130d11) & 1)) {
                    F32(0x130d14) = F32(car + 0xc38);
                    FUN_00037e06(0x132fb1, 0, 0, 0, F32(0x130d14));
                }
                if (U8(0x130d10) & 0x40) {
                    const int cam = I32(0x1192f4 + 4 * (uint32_t)I32(car));
                    FUN_00012d31(cam, 1);
                    FUN_00012dd8(cam, 0x2b);
                } else if (I32(car) == I32(0x11f0d0)) {
                    const int cam = I32(0x1192f4);
                    if (!(U8(0x130d11) & 1) && I32(cam + 0xae4) != 1) I32(0x130d68) = I32(cam + 0xae4);
                    FUN_00012d00(cam, (uint32_t)car);
                    FUN_00012d31(cam, 1);
                    FUN_00012dd8(cam, 0xb);
                }
            }
        }
    }
    I32(car + 0xc3c) = 0;
}

/*
 * DUST FROM THE WHEELS (0x204af): at the highest detail (0x130c54 = 2), every 0.05 s of race
 * clock (+0x224 counts it up by the step 0x133354).  For the two front wheels (+0x5d0, 0x70 apart:
 * the ground kind under each), when asked to (the second word) or when the ground throws dust
 * (the kind's record at 0x11ce00, 0x24 bytes: bit 4) and the car does over 20 (+0x500), one puff
 * from the set the kind says (its low two bits: 1 the car's set at +0x228, 2 the one at +0x368;
 * eight puffs of 0x28 bytes each): the first one not in the air (+8) goes up -- into the drawing
 * list (0x4c15f, 0x4bfbe) -- at the wheel (the car's frame's wheel row +0xc8.., 0x70 each, the
 * first after the body), scattered by the kind's spread (+4) times the car's place's low three
 * bits, signed by the hundredths clock (0x4ce0a).  Seen from the chase camera of this car, the
 * wheel's other place (+0xd4, +0xdc) and the puff's height left as it was.
 */
#define HOOK_204af
static void FUN_000204af(int car, int always)
{
    if (I32(0x130c54) != 2) return;
    if (!(F32(car + 0x224) >= (double)0.05)) { F32(car + 0x224) = (float)((long double)F32(0x133354) + F32(car + 0x224)); return; }
    I32(car + 0x224) = 0;
    for (int wheel = 0; wheel < 4; wheel += 2) {
        const uint32_t kind = 0x11ce00u + (uint32_t)I32(car + 0x5d0 + wheel * 0x70) * 0x24u;
        if (!always && !((U8(kind) & 4) && I32(car + 0x500) > 0x41a00000)) continue;
        uint32_t puff;
        switch (U32(kind) & 3) {
        case 1: puff = (uint32_t)car + 0x228u; break;
        case 2: puff = (uint32_t)car + 0x368u; break;
        default: puff = 0; break;
        }
        if (!puff) continue;
        for (const uint32_t end = puff + 0x140; puff < end; puff += 0x28) {
            if (I32(puff + 8)) continue;
            I32(puff + 8) = 1;
            FUN_0004c15f(0x11da14, puff, 0, 0, 0.0f);
            FUN_0004bfbe(0x11da14, 0x11d6f4, (int)puff);
            const int32_t spread = I32(kind + 4), sign = (int32_t)((uint32_t)(spread + spread) & U32(0x4ce0a)) - spread;
            const uint32_t obj = U32(car + 0x990), row = obj + (uint32_t)(wheel + 1) * 0x70u;
            const int32_t dx = sign * (I32(obj) & 7), dz = sign * (I32(obj + 8) & 7);
            const int cam = I32(0x1192f4);
            if (I32(cam + 0xae4) == 3 && I32(car) == I32(U32(cam + 0xadc))) {
                I32(puff + 0x18) = I32(row + 0xd4) + dx;
                I32(puff + 0x20) = I32(row + 0xdc) + dz;
            } else {
                I32(puff + 0x18) = I32(row + 0xc8) + dx;
                I32(puff + 0x20) = I32(row + 0xd0) + dz;
                I32(puff + 0x1c) = I32(row + 0xcc);
            }
            break;
        }
    }
}

/*
 * SPARKS (0x2aeb9): a knock at a place (the first word, three floats) with the car's speed along
 * (the second, three floats) throws sparks -- the hit's speed x 3600 / 400000 x 30 / 60 of them
 * (cut), at most 30, into the first or (set not 1) the
 * second thirty of the sparks' records (0x125dd4, 0x3c bytes each).  The random numbers start
 * from the hundredths clock (0x4ce0a).  A spark: alive 2 s (+0x24), at the place 100 higher, flying
 * at 0.9 of the car's speed across and along plus up to 1500 either way, 1000..2999 up, spinning
 * by up to 500 x 0.0125663709 a turn each way (+0xc, +0x14, +0x10), drawn (+0x30, +0x34; its
 * drawing record +0x28 into the list, 0x4bfbe).
 */
#define HOOK_2aeb9
static void FUN_0002aeb9(int at, int speed, uint32_t ebx, uint32_t ecx, float hit, int set)
{
    (void)ebx; (void)ecx;
    FUN_000b3c2d(U32(0x4ce0a));
    int n = (int32_t)truncl((long double)hit * 3600.0f / 400000.0f * 30.0f / 60.0f);
    if (n > 30) n = 30;
    for (int i = 0; i < n; i++) {
        const uint32_t sp = 0x125dd4u + (uint32_t)(set == 1 ? i : i + 30) * 0x3cu;
        U32(sp + 0x24) = 0x40000000u;   /* 2.0 */
        F32(sp) = F32(at); F32(sp + 4) = F32(at + 4); F32(sp + 8) = F32(at + 8);
        F32(sp + 4) = (float)((long double)F32(sp + 4) + 100.0f);
        F32(sp + 0x18) = (float)((long double)(s2_rand() % 3000) + (long double)F32(speed) * (double)0.9 + -1500.0);
        const double along = (double)((long double)F32(speed + 8) * (double)0.9);
        F32(sp + 0x20) = (float)((long double)(s2_rand() % 3000) + along + -1500.0);
        F32(sp + 0x1c) = (float)(s2_rand() % 2000 + 1000);
        F32(sp + 0xc) = (float)((long double)(s2_rand() % 500) * (float)0.0125663709f);
        F32(sp + 0x14) = (float)((long double)(s2_rand() % 500) * (float)0.0125663709f);
        F32(sp + 0x10) = (float)((long double)(s2_rand() % 500) * (float)0.0125663709f);
        I32(sp + 0x30) = 1;
        I32(sp + 0x34) = I32(sp + 0x30);
        FUN_0004bfbe(0x11da14, 0x11d6f4, (int)(sp + 0x28));
    }
}

/*
 * A SPLIT PASSED (0x1ffcc), not in the attract race: the car's splits (+0x4b0) go on by the number
 * given; only one not passed before (over +0x4b4) counts.  The leader (place +0xc30 = 1) writes
 * its race time, cut, into the split's slot (0x11ccec); the player's car (or any, two players)
 * shows how far behind it is (0x132f9a, "+", 4 s: 0x133078).  In the free run (mode 8) with a
 * best time for this split of the five (0x130d1c; 3599.999 = none): the difference, "-" if ahead,
 * is shown (0x133001.., 11 bytes each, copied to 0x132f99) and the next split's best is put up.
 * The split's time is the PLAYER's lap clock (car 0x11f0d0's +0xc3c), whichever car passed --
 * the game's way -- kept (+0xc44..) and written out (0x132fc9..).
 */
#define HOOK_1ffcc
static void FUN_0001ffcc(int car, int by)
{
    if (U8(0x130d10) & 0x80) return;
    I32(car + 0x4b0) += by;
    if (I32(car + 0x4b0) <= I32(car + 0x4b4)) return;
    if (I32(car + 0xc30) == 1) I32(0x11ccec + 4 * (uint32_t)I32(car + 0x4b0)) = (int32_t)truncl((long double)F32(car + 0xc38));
    else if (I32(car) == I32(0x11f0d0) || (U8(0x130d10) & 0x40)) {
        FUN_00037e06(0x132f9a, 0, 0, 0, (float)fabsl((long double)I32(0x11ccec + 4 * (uint32_t)I32(car + 0x4b0)) - F32(car + 0xc38)));
        U8(0x132f99) = '+';
        F32(0x133078) = 4.0f;
    }
    I32(car + 0x4b4) = I32(car + 0x4b0);
    const int k = (I32(car + 0x4b0) - by) % 5;
    const uint32_t best = 0x130d1cu + 4u * (uint32_t)k, mine = (uint32_t)car + 0xc44u + 4u * (uint32_t)k, text = 0x133001u + 0xbu * (uint32_t)k;
    if ((U8(0x130d10) & 8) && U32(best) != 0x4560fffcu) {
        FUN_00037e06(text + 1, 0, 0, 0, (float)fabsl((long double)F32(best) - F32(mine)));
        U8(text) = 0.0L > (long double)F32(mine) - F32(best) ? '-' : '+';
        FUN_000b3be4(0x132f99, text);
        F32(0x133078) = 4.0f;
        if (k + 1 < 5) {
            U8(text + 0xb) = ' ';
            FUN_00037e06(text + 0xb + 1, 0, 0, 0, F32(best + 4));
        }
    }
    F32(mine) = F32(0x119f3cu + 0xcc0u * (uint32_t)I32(0x11f0d0));
    FUN_00037e06(0x132fc9u + 0xbu * (uint32_t)k, 0, 0, 0, F32(mine));
}

/*
 * THREE ANGLES TURNED INTO THREE OTHERS (0x105cd): from the angles at the second word (their
 * sines and cosines, each a float), five terms of the turn they make and a sixth, the measure
 * whether it can be undone (`det`); under 0.002 the angles are taken 0.002 further and all made
 * again.  Then q = the square root of det over the sixth term; the three new angles are the arc
 * cosines of q and of two terms over q (each held to -1..1), and each is two pi less itself
 * where its sine's term is below nought.  Into the first word, floats.
 * (The converted text had lost the arc cosines' arguments and gave 0 for each.)
 */
static void s2r_105cd_terms(int angles, long double by, float t[7])
{
    const float s0 = (float)sinl((long double)F32(angles) + by), s1 = (float)sinl((long double)F32(angles + 4) + by), s2 = (float)sinl((long double)F32(angles + 8) + by);
    const float c0 = (float)cosl((long double)F32(angles) + by), c1 = (float)cosl((long double)F32(angles + 4) + by), c2 = (float)cosl((long double)F32(angles + 8) + by);
    t[0] = (float)((long double)-c0 * s1 * s2 - (long double)s0 * c2);   /* -0x2c */
    t[1] = (float)((long double)c0 * c1);                                  /* -0x10 */
    t[2] = (float)((long double)-c0 * s1 * c2 + (long double)s0 * s2);   /* -0x0c */
    t[3] = (float)((long double)-s0 * s1 * s2 + (long double)c0 * c2);   /* -0x30 */
    t[4] = (float)((long double)c1 * s2);                                  /* -0x34 */
    t[5] = (float)((long double)-s0 * s1 * c2 - (long double)c0 * s2);   /* -0x38 */
    t[6] = (float)((long double)-t[4] * t[1] - (long double)t[3] * t[0] * t[2]);   /* det */
}
static float s2r_hold_1(float x) { return s2_bits(x) > 0x3f800000 ? 1.0f : !(x >= -1.0f) ? -1.0f : x; }
#define HOOK_105cd
static void FUN_000105cd(int out, int angles)
{
    float t[7];
    s2r_105cd_terms(angles, 0.0L, t);
    if (!(fabsl((long double)t[6]) >= (double)0.002)) s2r_105cd_terms(angles, (double)0.002, t);
    const float q = (float)sqrt((double)((long double)t[6] / t[5]));
    const float neg = -t[0];
    const float inv = (float)(1.0L / q);
    const float a = (float)((long double)t[1] * inv), sb = (float)((long double)-t[2] * inv);
    const float b = (float)((long double)t[3] * inv), sc = (float)((long double)t[4] * inv);
    F32(out) = (float)s2_acos((double)s2r_hold_1(q));
    if (0.0f > neg) F32(out) = (float)((long double)(float)6.28318548f - F32(out));
    F32(out + 4) = (float)s2_acos((double)s2r_hold_1(a));
    if (0.0f > sb) F32(out + 4) = (float)((long double)(float)6.28318548f - F32(out + 4));
    F32(out + 8) = (float)s2_acos((double)s2r_hold_1(b));
    if (0.0f > sc) F32(out + 8) = (float)((long double)(float)6.28318548f - F32(out + 8));
}

/*
 * THE CAR MADE OF ITS SET-UP (0x14e20; NOTES2 S2-5zn): the set-up's six (+0 handling, +4 tyre
 * pressure, +8 tyres -- -1 the track's own, by its kind 0x130c4c: 1 1 2 0 2 0 0 --, +0xc / +0x10
 * springs front / rear, +0x14 brakes; 2 the middle of each).  A computer's car (no controls,
 * +0xb4) whose level (+0x94) is over 0.8 gets handling 1; and unless the mode keeps them (bit 1),
 * a computer's car gets the tyres' best pressure (0x124124), springs 4 -- the rear 3 when its
 * level is over 0.4 from the player's (+0x90) -- softer by the track's kind (1, 2: one; 4: two),
 * handling 4.  Then: grip (+0x74) less 0.05 for each step of pressure off the tyre's best; the
 * springs' stiffness (+0x78 / +0x7c, 0.01 a step, 0.005 more than 0.4 off the player's level) and
 * the wheels' heights (+0xa0 / +0xa8 and 5 a step, front +0x59c.., rear +0x60c..); the most revs
 * (+0x5c) x 1.12, and the pressure off the best less the tyre's own (0x112e38); the steering
 * (+0x6c) x (handling - 2) x 0.15 + 0.7, and x 1.3 for a car without the extra (its controls'
 * +0x40); the brakes (+0x9c) a quarter of the brakes' number; the knocks (+8, +0xc.., +0x1c..)
 * none; +0x2c half the two springs' sum.  Each "over" is the x87's.
 */
#define HOOK_14e20
static void FUN_00014e20(int car, int set)
{
    I32(car + 0x50) = I32(set + 8);
    if (I32(car + 0x50) == -1) {
        static const int by_kind[7] = { 1, 1, 2, 0, 2, 0, 0 };
        if (U32(0x130c4c) <= 6) I32(car + 0x50) = by_kind[U32(0x130c4c)];
    }
    if (F32(car + 0x94) > (double)0.8 && I32(car + 0xb4) == 0) I32(set) = 1;
    if (I32(car + 0xb4) == 0 && !(U8(0x130d10) & 1)) {
        I32(set + 4) = I32(0x124124 + 4 * (uint32_t)I32(car + 0x50));
        I32(set + 0xc) = 4;
        I32(set + 0x10) = fabsl((long double)F32(car + 0x94) - F32(car + 0x90)) > (double)0.4 ? 3 : 4;
        const uint32_t kind = U32(0x130c4c);
        const int softer = kind >= 1 && kind <= 2 ? 1 : kind == 4 ? 2 : 0;
        I32(set + 0xc) -= softer;
        I32(set + 0x10) -= softer;
        I32(set) = 4;
    }
    const float step = fabsl((long double)F32(car + 0x94) - F32(car + 0x90)) > (double)0.4 ? (float)0.005f : (float)0.01f;
    F32(car + 0x70) = 1.0f;
    const int off = I32(0x124124 + 4 * (uint32_t)I32(car + 0x50)) - I32(set + 4);
    F32(car + 0x74) = (float)(double)(((double)1.1 - fabsl((long double)off) * (double)0.05) * (double)F32(car + 0x74));
    const int front = 2 - I32(set + 0xc), rear = 2 - I32(set + 0x10);
    F32(car + 0x78) = (float)((long double)front * step + F32(car + 0x78));
    F32(car + 0x80) = F32(car + 0x78);
    I32(car + 0x59c) = I32(car + 0xa0) + front * 5;
    I32(car + 0x67c) = I32(car + 0x59c);
    F32(car + 0x594) = (float)(I32(car + 0xa8) + front * 5);
    F32(car + 0x674) = F32(car + 0x594); F32(car + 0x58c) = F32(car + 0x674); F32(car + 0x66c) = F32(car + 0x58c);
    F32(car + 0x7c) = (float)((long double)rear * step + F32(car + 0x7c));
    F32(car + 0x84) = F32(car + 0x7c);
    I32(car + 0x60c) = I32(car + 0xa0) + rear * 5;
    I32(car + 0x6ec) = I32(car + 0x60c);
    F32(car + 0x604) = (float)(I32(car + 0xa8) + rear * 5);
    F32(car + 0x6e4) = F32(car + 0x604); F32(car + 0x5fc) = F32(car + 0x6e4); F32(car + 0x6dc) = F32(car + 0x5fc);
    F32(car + 0x5c) = (float)(double)((double)F32(car + 0x5c) * (double)1.12);
    F32(car + 0x5c) = (float)((long double)(I32(set + 4) - 2 - I32(0x112e38 + 4 * (uint32_t)I32(car + 0x50))) + F32(car + 0x5c));
    F32(car + 0x6c) = (float)(double)(((long double)(I32(set) - 2) * (double)0.15 + (double)0.7) * (double)F32(car + 0x6c));
    if (I32(car + 0xb4) == 0 || I32(I32(car + 0xb4) + 0x40) == 0) F32(car + 0x6c) = (float)(double)((double)F32(car + 0x6c) * (double)1.3);
    F32(car + 0x9c) = (float)((long double)I32(set + 0x14) * (double)0.25);
    I32(car + 8) = 0;
    for (uint32_t i = 0; i < 4; i++) { I32(car + 0xc + 4 * i) = 0; I32(car + 0x1c + 4 * i) = 0; }
    F32(car + 0x2c) = (float)((long double)(I32(set + 0xc) + I32(set + 0x10)) * (double)0.5);
}

/*
 * WHERE A STEP CROSSES A GATE (0x2348b), on the ground (x and z): the step from p1 to p2 and the
 * gate of four corners p3 p4 (one side) p5 p6 (the other); the gate's line is its middle, the
 * average of the four corners, and runs the mean of p5-p3 and p6-p4 across; its length is the
 * longer of its two sides (p4-p3, p6-p5) along their mean direction -- each step a float.  Gives
 * the share of the step at the crossing, or 2 when the step does not cross it (either share out
 * of 0..1, or the lines run together).
 */
static int s2r_zero(float x) { return (s2_bits(x) & 0x7fffffff) == 0; }
#define HOOK_2348b
static float10 FUN_0002348b(int p1, int p2, int p3, int p4, int p5, int p6)
{
    float A[3], D1[3], D2[3], M[3], E[3], F[3], G[3], N[3], P[3], Q[3], S[3], R[3];
    for (uint32_t k = 0; k < 3; k++) A[k] = (float)((long double)F32(p2 + 4 * k) - F32(p1 + 4 * k));
    for (uint32_t k = 0; k < 3; k++) D1[k] = (float)((long double)F32(p6 + 4 * k) - F32(p4 + 4 * k));
    for (uint32_t k = 0; k < 3; k++) D2[k] = (float)((long double)F32(p5 + 4 * k) - F32(p3 + 4 * k));
    for (int k = 0; k < 3; k++) M[k] = (float)((long double)D2[k] + D1[k]);
    for (int k = 0; k < 3; k++) M[k] = (float)((long double)M[k] * 0.5f);
    for (uint32_t k = 0; k < 3; k++) E[k] = (float)((long double)F32(p4 + 4 * k) - F32(p3 + 4 * k));
    for (uint32_t k = 0; k < 3; k++) F[k] = (float)((long double)F32(p6 + 4 * k) - F32(p5 + 4 * k));
    for (int k = 0; k < 3; k++) G[k] = (float)((long double)E[k] + F[k]);
    const float le = (float)sqrt((double)((long double)E[1] * E[1] + (long double)E[0] * E[0] + (long double)E[2] * E[2]));
    const float lf = (float)sqrt((double)((long double)F[1] * F[1] + (long double)F[0] * F[0] + (long double)F[2] * F[2]));
    const float len = lf >= le ? lf : le;
    const float lg = (float)sqrt((double)((long double)G[1] * G[1] + (long double)G[0] * G[0] + (long double)G[2] * G[2]));
    if (s2r_zero(lg)) N[0] = N[1] = N[2] = 0.0f;
    else { const float inv = (float)(1.0L / lg); for (int k = 0; k < 3; k++) N[k] = (float)((long double)G[k] * inv); }
    for (int k = 0; k < 3; k++) P[k] = (float)((long double)N[k] * len);
    for (int k = 0; k < 3; k++) Q[k] = (float)((long double)M[k] + P[k]);
    for (int k = 0; k < 3; k++) Q[k] = (float)((long double)Q[k] * 0.5f);
    for (uint32_t k = 0; k < 3; k++) S[k] = (float)((long double)F32(p3 + 4 * k) + F32(p5 + 4 * k));
    for (uint32_t k = 0; k < 3; k++) S[k] = (float)((long double)S[k] + F32(p4 + 4 * k));
    for (uint32_t k = 0; k < 3; k++) S[k] = (float)((long double)S[k] + F32(p6 + 4 * k));
    for (int k = 0; k < 3; k++) S[k] = (float)((long double)S[k] * 0.25f);
    for (int k = 0; k < 3; k++) R[k] = (float)((long double)S[k] - Q[k]);
    const long double x1 = F32(p1), z1 = F32(p1 + 8);
    float u;
    const float den = (float)(((long double)A[2] - P[2]) * M[0] + ((long double)P[0] - A[0]) * M[2]);
    if (!s2r_zero(den)) u = (float)((((long double)R[2] - z1) * M[0] + (x1 - R[0]) * M[2]) / den);
    else {
        const float den2 = (float)(((long double)P[2] - A[2]) * M[0] + ((long double)A[0] - P[0]) * M[2]);
        if (s2r_zero(den2)) return 2.0f;
        u = (float)(((z1 - R[2]) * M[0] + ((long double)R[0] - x1) * M[2]) / den2);
    }
    if (0.0f > u || s2_bits(u) > 0x3f800000) return 2.0f;
    float v;
    if (!s2r_zero(M[0])) v = (float)(((((long double)A[0] - P[0]) * u + x1) - R[0]) / M[0]);
    else if (!s2r_zero(M[2])) v = (float)(((((long double)A[2] - P[2]) * u + z1) - R[2]) / M[2]);
    else return 2.0f;
    if (0.0f > v || s2_bits(v) > 0x3f800000) return 2.0f;
    return u;
}

/*
 * THE BODY LEANING ON ITS SPRINGS (0x1c4fb), not while the car is down (+0x4c bit 8) or out of
 * the race (+0x150): the push of speeding up (the change of speed, +0x75c - +0x500, over the step)
 * and of turning (the speed by the sine of the steering, +0x4d4), each times 0.12 over the
 * springs' strength (+0x674), make four loads round 1 at the wheels (front-left, front-right,
 * back-left, back-right); each wheel's spring (+0x574.., 0x70 apart) goes to its rest (+0x18)
 * moved by its travel (+0x20) x |1 - load| / 0.6, and then is held within its travel of rest.
 * The body's pitch (+0xc18) and roll (+0xc20) are eased (0x111d6, faster with +0x30) to twice the
 * angle whose tangent is the mean difference of the springs over the car's length (+0x4d0) and
 * its width (+0x4c8).
 */
#define HOOK_1c4fb
static void FUN_0001c4fb(int car)
{
    if ((U8(car + 0x4c) & 8) || I32(car + 0x150)) { I32(car + 0xc18) = 0; I32(car + 0xc20) = 0; return; }
    const float pitch = (float)(((long double)(double)0.12 / F32(car + 0x674) * cosl((long double)F32(car + 0x4d4))
                                 * ((long double)F32(car + 0x75c) - F32(car + 0x500))) / F32(0x133350));
    const float roll = (float)(-((long double)(double)0.12 / F32(car + 0x674)) * sinl((long double)F32(car + 0x4d4)) * F32(car + 0x500));
    float w[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    w[2] = (float)((long double)w[2] - pitch); w[0] = w[2];
    w[3] = (float)((long double)w[3] + pitch); w[1] = w[3];
    w[0] = (float)((long double)w[0] + roll); w[1] = (float)((long double)w[1] + roll);
    w[2] = (float)((long double)w[2] - roll); w[3] = (float)((long double)w[3] - roll);
    for (uint32_t i = 0; i < 4; i++) {
        const uint32_t sp = (uint32_t)car + 0x574u + i * 0x70u;
        if (s2_bits(w[i]) == 0x3f800000) continue;
        const float by = (float)(fabsl(1.0L - w[i]) / (double)0.6 * F32(sp + 0x20));
        F32(sp + 0x1c) = s2_bits(w[i]) < 0x3f800000 ? (float)((long double)F32(sp + 0x18) - by) : (float)((long double)F32(sp + 0x18) + by);
    }
    for (uint32_t i = 0; i < 4; i++) {
        const uint32_t sp = (uint32_t)car + 0x574u + i * 0x70u;
        const float d = (float)((long double)F32(sp + 0x1c) - F32(sp + 0x18));
        if (F32(sp + 0x20) >= fabsf(d)) w[i] = F32(sp + 0x1c);
        else w[i] = (float)((long double)(0.0f > d ? -1 : 1) * F32(sp + 0x20) + F32(sp + 0x18));
    }
    const int rate = (int32_t)truncl((long double)F32(car + 0x30) * 68.0f + 30.0f);
    float t = (float)((((long double)w[2] - w[3]) + ((long double)w[0] - w[1])) * (double)0.5);
    FUN_000111d6(car + 0xc18, (float)(atanl((long double)t / I32(car + 0x4d0)) * (double)2.0), rate);
    const int rate2 = (int32_t)truncl((long double)F32(car + 0x30) * 68.0f + 30.0f);
    t = (float)((((long double)w[1] - w[3]) + ((long double)w[0] - w[2])) * (double)0.5);
    FUN_000111d6(car + 0xc20, (float)(atanl((long double)t / I32(car + 0x4c8)) * (double)2.0), rate2);
}

/*
 * THE STEERING (0x19409).  First the ground under the wheels (0x153c6).  A car driven by the game
 * (no controls, +0xb4) or finished (+0x4c bit 1): the game's driver (0x17726), and a finished
 * car standing still (+4 nought) lets go of the throttle (+0x1ec 0, +0x1e8 1).  Else from the
 * controls: the steering's word (-(+0xb0) in 1024ths, and +0xb4 added for a stick, +0x40 = 1)
 * makes the wheels' angle (+0x660) of the car's most (+0x6c) -- for keys (+0x40 = 0) taken over
 * the steering's full travel (0x112e20) and held to -1..1: half of it below 0.6, beyond that
 * (|r| - 0.6) x 1.7 + 0.3, held to the most; a stick (1): the most, to the side it points; a
 * button pair (2): the most from half way, else none.  None at all when the word is nought.
 * Then the angle the wheels are put to (+0xbc) is that over 4 x speed / top speed + 2 (less at
 * speed), times 1.6 less 0.6 of the throttle (+0x1ec), times |cos(the wheels' angle +0x4d4)|^4,
 * and when it turns the same way as they already are, 1 + 1.5 |sin| times that; the game's
 * driver puts it at once, a player's eases (0x111d6) by 21 less 5 x +0x2c.  A crashed car
 * (+0x4c bit 4) has no steering or throttle.  +0x580 = the wheels' angle.
 */
#define HOOK_19409
static void FUN_00019409(int car)
{
    FUN_000153c6(car);
    const int ctrl = I32(car + 0xb4);
    if (ctrl == 0 || (U8(car + 0x4c) & 1)) {
        FUN_00017726(car, 0, 0, 0);
        if ((U8(car + 0x4c) & 1) && (U32(car + 4) & 0x7fffffffu) == 0) { I32(car + 0x1ec) = 0; F32(car + 0x1e8) = 1.0f; }
    } else {
        float in = (float)((long double)(-I32(ctrl + 0xb0)) * 0.0009765625f);
        if (I32(ctrl + 0x40) == 1) in = (float)((long double)I32(ctrl + 0xb4) * 0.0009765625f + in);
        if ((s2_bits(in) & 0x7fffffff) == 0) F32(car + 0x660) = 0.0f;
        else switch (U32(ctrl + 0x40)) {
        case 0: {
            const long double x = (long double)in / F32(0x112e20);
            float r;
            if (!(x > -1.0f)) r = -1.0f;
            else if (!(1.0L > x)) r = 1.0f;
            else r = (float)x;
            if (!(fabsf(r) >= (double)0.6)) F32(car + 0x660) = (float)((long double)F32(car + 0x6c) * ((long double)r * (double)0.5));
            else {
                const int sign = 0.0f > r ? -1 : 1;
                F32(car + 0x660) = (float)((long double)sign * ((fabsl((long double)r) + -(double)0.6) * (double)1.7 + (double)0.3) * F32(car + 0x6c));
            }
            if (F32(car + 0x660) > F32(car + 0x6c)) F32(car + 0x660) = F32(car + 0x6c);
            else if (-F32(car + 0x6c) > F32(car + 0x660)) F32(car + 0x660) = -F32(car + 0x6c);
            break;
        }
        case 1: {
            const float r = -in;
            F32(car + 0x660) = (float)((long double)(0.0f > r ? -1 : 1) * F32(car + 0x6c));
            break;
        }
        case 2:
            if (!(fabsf(in) >= (double)0.5)) F32(car + 0x660) = 0.0f;
            else F32(car + 0x660) = (float)((long double)(0.0f > in ? -1 : 1) * F32(car + 0x6c));
            break;
        default: break;
        }
    }
    const float grip = (float)pow(fabs(cos((double)F32(car + 0x4d4))), 4.0);
    float steer = (float)(double)(((long double)F32(car + 0x660) / ((long double)F32(car + 0x500) * 4.0f / F32(car + 0x5c) + 2.0f))
                                  * ((double)1.6 - (long double)F32(car + 0x1ec) * (double)0.6) * (double)grip);
    if (!(0.0L >= (long double)F32(car + 0x4d4) * steer))
        steer = (float)(double)((fabsl(sinl((long double)F32(car + 0x4d4))) * (double)1.5 + 1.0L) * (double)steer);
    if (ctrl == 0) F32(car + 0xbc) = steer;
    else FUN_000111d6(car + 0xbc, steer, (int32_t)truncl(21.0f - (long double)F32(car + 0x2c) * 5.0f));
    if (U8(car + 0x4c) & 4) { I32(car + 0x1ec) = 0; I32(car + 0xbc) = 0; F32(car + 0x1e8) = 1.0f; I32(car + 0x1e4) = 0; I32(car + 0x660) = 0; }
    F32(car + 0x580) = F32(car + 0x660);
}

/*
 * THE GROUND UNDER THE FOUR WHEELS (0x28f96): for each corner (+0x514.., x and z cut) the ground
 * there (0x28ce6): found, the wheel (+0x574.., 0x70 apart) takes its slope (+0x64.., floats of
 * 0x4cba0..), its height (+0x44, 0x4cb98) and kind (+0x5c, 0x4cbec), and is on it (+8).  With one
 * or two found, a wheel not on the ground already (+8) is put at their mean height; with three,
 * the first missing one is put on the plane through the three: its height is the first found
 * one's less (dz x n.z + dx x n.x) / n.y, n the plane's direction made one long.
 */
#define HOOK_28f96
static void FUN_00028f96(int car, uint32_t edx)
{
    float sum = 0.0f;
    int found = 0;
    for (uint32_t i = 0; i < 4; i++) {
        const uint32_t w = (uint32_t)car + 0x574u + i * 0x70u;
        I32(0x11cdcc) = (int32_t)truncl((long double)F32(car + 0x514 + i * 0xc));
        I32(0x11cdd4) = (int32_t)truncl((long double)F32(car + 0x51c + i * 0xc));
        const uint64_t r = FUN_00028ce6(0, edx);
        edx = (uint32_t)(r >> 32);
        if ((uint32_t)r == 0) { I32(w + 8) = 0; continue; }
        F32(w + 0x64) = (float)I32(0x4cba0); F32(w + 0x68) = (float)I32(0x4cba4); F32(w + 0x6c) = (float)I32(0x4cba8);
        I32(w + 0x44) = I32(0x4cb98);
        sum = (float)((long double)I32(w + 0x44) + sum);
        I32(w + 0x5c) = I32(0x4cbec);
        I32(w + 8) = 1;
        found++;
    }
#define ON(j) I32(car + 0x57c + (uint32_t)(j) * 0x70u)
#define HEIGHT(j) I32(car + 0x5b8 + (uint32_t)(j) * 0x70u)
#define CX(j) F32(car + 0x514 + (uint32_t)(j) * 0xcu)
#define CZ(j) F32(car + 0x51c + (uint32_t)(j) * 0xcu)
    if (found == 1 || found == 2) {
        const float mean = (float)((long double)sum / found);
        for (int j = 0; j < 4; j++) if (!ON(j)) { ON(j) = 1; HEIGHT(j) = (int32_t)truncl((long double)mean); }
        return;
    }
    if (found != 3) return;
    float n[3] = { 0.0f, 0.0f, 0.0f };
    int i = 0;
    for (i = 0; i < 4; i++) {
        if (!ON(i)) continue;
        int done = 0;
        for (int j = i + 1; j < 4 && !done; j++) {
            if (!ON(j)) continue;
            for (int k = j + 1; k < 4; k++) {
                if (!ON(k)) continue;
                const float a = (float)((long double)CX(j) - CX(i)), c = (float)((long double)CZ(j) - CZ(i)), b = (float)(HEIGHT(j) - HEIGHT(i));
                const float d = (float)((long double)CX(k) - CX(i)), f = (float)((long double)CZ(k) - CZ(i)), e = (float)(HEIGHT(k) - HEIGHT(i));
                n[0] = (float)((long double)b * f - (long double)c * e);
                n[1] = (float)((long double)c * d - (long double)a * f);
                n[2] = (float)((long double)a * e - (long double)b * d);
                break;
            }
            done = 1;
        }
        break;
    }
    for (int m = 0; m < 4; m++) {
        if (ON(m)) continue;
        const float len = (float)sqrt((double)((long double)n[1] * n[1] + (long double)n[0] * n[0] + (long double)n[2] * n[2]));
        if ((s2_bits(len) & 0x7fffffff) == 0) n[0] = n[1] = n[2] = 0.0f;
        else { const float inv = (float)(1.0L / len); for (int k = 0; k < 3; k++) n[k] = (float)((long double)n[k] * inv); }
        const float dx = (float)((long double)CX(m) - CX(i)), dz = (float)((long double)CZ(m) - CZ(i));
        HEIGHT(m) = (int32_t)truncl((long double)HEIGHT(i) - ((long double)dz * n[2] + (long double)dx * n[0]) / n[1]);
        ON(m) = 1;
        break;
    }
#undef ON
#undef HEIGHT
#undef CX
#undef CZ
}

/*
 * THE SPRINGS (0x1bef1), each wheel (+0x574.., 0x70 apart; w+0 its speed up, +0x14 the spring's
 * length now, +0x18 at rest, +0x20 its travel, +0x28 its easing, +0x30 the wheel's height, +0x44
 * the ground's, +4 on the ground, +8 over ground at all, +0x48 a bump).  A wheel over ground: the
 * ground's roughness (its kind's mask, 0x112e50 by 0x11cdfc, on +0x2c + +0x34; bit 2 the sign)
 * times the share of top speed makes a bump, of which (1 - +0x2c / 10) the wheel takes and the
 * rest lifts the ground for this frame.  The wheel falls (5886 a second squared) and moves; never
 * lower than the ground plus rest less travel.  Over the ground plus rest and a fifth of travel:
 * off the ground, the spring eases out to that.  On it: pressed shorter than rest, the spring is
 * the gap and eases to rest (and the wheel follows); else it eases out and is never longer than
 * the gap.  The bump (+0x48) the wheel's rise against the corner's last height (+0x518..) plus half
 * the old one, held within the travel; the corner takes the wheel's height (and its +0x60 when the
 * wheels are straight, under 0.01).  The car's height (+0x4dc), spring (+0x4f4) and wheels on the
 * ground (+0x510) are the four's means; the wheel's speed up is its rise over the step (not when
 * the car is down or out: then only never upwards), none when a wheel on the ground is not on the
 * road's surface (+0x7d8.. not 1).  Landing (no wheels down before, +0x76c) on the followed car:
 * the thud (sample 0x12).  +0x4f8 the mean of the four ground heights.
 */
#define HOOK_1bef1
static void FUN_0001bef1(int car)
{
    I32(car + 0x4f4) = 0; I32(car + 0x4dc) = 0; I32(car + 0x510) = 0;
    const float dt = F32(0x133350);
    for (uint32_t i = 0; i < 4; i++) {
        const uint32_t w = (uint32_t)car + 0x574u + i * 0x70u, corner = (uint32_t)car + 0x518u + i * 0xcu;
        const int32_t was = I32(w + 0x30);
        int32_t bump = 0;
        if (I32(w + 8) == 0) I32(w + 0x48) = 0;
        else {
            const int32_t b = I32(w + 0x2c) + I32(w + 0x34);
            const int32_t rough = (int32_t)((uint32_t)b & U32(0x112e50u + 4u * (uint32_t)I32(0x11cdfcu + (uint32_t)I32(w + 0x5c) * 0x24u)));
            const float v = (float)(rough * ((b & 2) ? -1 : 1));
            const float f = (float)((long double)F32(car + 0x500) / F32(car + 0x5c) * v);
            bump = (int32_t)truncl((long double)f);
            I32(w + 0x48) = (int32_t)truncl((1.0L - (long double)F32(car + 0x2c) / 10.0f) * bump);
            bump -= I32(w + 0x48);
        }
        I32(w + 0x44) += bump;
        F32(w) = (float)(double)((double)F32(w) - (long double)dt * (double)5886.0);
        const long double low = ((long double)I32(w + 0x44) + F32(w + 0x18)) - F32(w + 0x20);
        const float y = (float)((long double)F32(w) * dt + (float)I32(w + 0x30));
        I32(w + 0x30) = (int32_t)truncl((long double)y);
        if (!((long double)I32(w + 0x30) >= low)) I32(w + 0x30) = (int32_t)truncl(low);
        const long double high = ((long double)I32(w + 0x44) + F32(w + 0x18)) + (long double)F32(w + 0x20) * (double)0.2;
        if ((long double)I32(w + 0x30) >= high) {
            I32(w + 4) = 0;
            FUN_000111d6(w + 0x14, (float)((long double)F32(w + 0x20) * (double)0.2 + F32(w + 0x18)), 0x50);
        } else {
            I32(w + 4) = 1;
            if (!((long double)(I32(w + 0x30) - I32(w + 0x44)) >= F32(w + 0x18))) {
                F32(w + 0x14) = (float)(I32(w + 0x30) - I32(w + 0x44));
                FUN_000111d6(w + 0x14, F32(w + 0x18), I32(w + 0x28));
                I32(w + 0x30) = (int32_t)truncl((long double)I32(w + 0x44) + F32(w + 0x14));
            } else {
                FUN_000111d6(w + 0x14, (float)((long double)F32(w + 0x20) * (double)0.2 + F32(w + 0x18)), 0x50);
                if (!((long double)(I32(w + 0x30) - I32(w + 0x44)) >= F32(w + 0x14))) F32(w + 0x14) = (float)(I32(w + 0x30) - I32(w + 0x44));
            }
        }
        F32(car + 0x510) = (float)((long double)I32(w + 4) + F32(car + 0x510));
        const int32_t half = (I32(w + 0x48) - (I32(w + 0x48) >> 31)) >> 1;
        const long double rise = ((long double)F32(corner) - I32(w + 0x30)) + (long double)half;
        float held;
        if (-F32(w + 0x20) >= rise) held = -F32(w + 0x20);
        else if (rise >= F32(w + 0x20)) held = F32(w + 0x20);
        else held = (float)rise;
        I32(w + 0x48) = (int32_t)truncl((long double)held);
        F32(corner) = (float)I32(w + 0x30);
        if (!(fabsf(F32(car + 0x4d4)) >= (double)0.01)) F32(corner) = (float)((long double)I32(w + 0x60) + F32(corner));
        const float h = (float)(((long double)I32(w + 0x30) - F32(w + 0x14)) + (float)I32(car + 0x4dc));
        I32(car + 0x4dc) = (int32_t)truncl((long double)h);
        const float sp = (float)((long double)(float)I32(car + 0x4f4) + F32(w + 0x14));
        I32(car + 0x4f4) = (int32_t)truncl((long double)sp);
        if (!(U8(car + 0x4c) & 8) && I32(car + 0x150) == 0) {
            if (!(0.0f >= dt)) F32(w) = (float)((long double)(I32(w + 0x30) - was) / dt);
            else F32(w) = 0.0f;
        } else if (!(0.0f >= F32(w))) F32(w) = 0.0f;
        if (I32(w + 8) != 0 && I32(car + 0x7d8 + i * 0x70) != 1) F32(w) = 0.0f;
        I32(w + 0x44) -= bump;
    }
    I32(car + 0x4f4) = (int32_t)truncl((long double)(double)((long double)I32(car + 0x4f4) * (double)0.25));
    F32(car + 0x510) = (float)(double)((double)F32(car + 0x510) * (double)0.25);
    I32(car + 0x4dc) = (int32_t)truncl((long double)(double)((long double)I32(car + 0x4dc) * (double)0.25));
    if ((U32(car + 0x76c) & 0x7fffffff) == 0 && (U32(car + 0x510) & 0x7fffffff) != 0 && I32(car) == I32(0x11f0c8) && !(U8(0x130d10) & 0x80))
        FUN_000aea54(U32(0x124274), 0x12, 4, 0, 0, 0);
    I32(car + 0x4f8) = (int32_t)truncl((long double)(I32(car + 0x708) + (I32(car + 0x5b8) + I32(car + 0x698) + I32(car + 0x628))) * (double)0.25);
}

/*
 * THE TYRES ON THE GROUND (0x1b36f).  The grip (the mean of the four wheels' grounds for these
 * tyres, 0x11cdf0 by kind 0x24 apart and tyre +0x50) changed by how rough the grounds are (their
 * mean roughness 0x11cdfc less 4 less the springs' softness +0x2c): rough takes away (a player's
 * 0.2, the game's driver's 0.1 of it), smooth adds (0.1, 0.05).  The record so far is kept
 * (+0x4d8.. to +0x734.., 0x25c bytes).  The sideways slide (+0xc0): none when the wheels are turned
 * against the way the car turns, or nearly straight, with more than a tenth of the most steering
 * put on; none with not all four wheels down (+0x510 under 1); else |cos(wheels' angle)| - 0.7, no
 * less than nought, times the grip, the car's +0x98 and the share of top speed (held to 1).  How
 * much the four tyres hold (0x19cac each: the front two at the turned angle) makes +0xc34 = 1 less
 * their mean over 111 x top speed; on the ground under 0.93 the wheels throw dust (0x204af) and
 * squeal, louder (to 29952) the lower.  The squeal and the rolling sound are the followed car's
 * (or each car's, two players: slot car + 5): the ground's squeal (0x11ce0c, sample + 7) at that
 * loudness, or its rolling sound (0x11ce08, sample + 0x12) at 655 x the speed held to 50, at most
 * 32767, or quiet.  Then the springs' corners (0x1c7f8), the four corners (0x1d9fc), and the ground's
 * height under the car's middle (0x2a371: +0x4c4).
 */
#define HOOK_1b36f
static void FUN_0001b36f(int car)
{
    const uint32_t tyre = 4u * (uint32_t)I32(car + 0x50);
#define KIND(off) (0x24u * (uint32_t)I32(car + (off)))
    const float base = (float)(((((long double)F32(0x11cdf0u + KIND(0x6b0) + tyre) + F32(0x11cdf0u + tyre + KIND(0x5d0)))
                                 + F32(0x11cdf0u + tyre + KIND(0x720))) + F32(0x11cdf0u + tyre + KIND(0x640))) * (double)0.25);
    const float rough = (float)((long double)(I32(0x11cdfcu + KIND(0x6b0)) + I32(0x11cdfcu + KIND(0x5d0)) + I32(0x11cdfcu + KIND(0x720)) + I32(0x11cdfcu + KIND(0x640)))
                                * (double)0.25 - (4.0f - (long double)F32(car + 0x2c)));
    float grip;
    if (I32(car + 0xb4)) grip = 0.0f >= rough ? (float)(double)((long double)rough * (double)0.1 + (double)base) : (float)(double)((double)base - (long double)rough * (double)0.2);
    else grip = 0.0f >= rough ? (float)(double)((long double)rough * (double)0.05 + (double)base) : (float)(double)((double)base - (long double)rough * (double)0.1);
    memmove(MP(car + 0x734), MP(car + 0x4d8), 0x25c);
    const int against = 0.0L > (long double)F32(car + 0x4d4) * F32(car + 0x660) || !(fabsf(F32(car + 0x4d4)) >= (double)0.01);
    if ((against && fabsf(F32(car + 0x660)) > (long double)F32(car + 0x6c) * (double)0.1) || I32(car + 0x510) < 0x3f800000) I32(car + 0xc0) = 0;
    else {
        F32(car + 0xc0) = (float)fabsl(cosl((long double)F32(car + 0x4d4)));
        F32(car + 0xc0) = (float)((long double)F32(car + 0xc0) + -(float)0.7f);
        if (0.0f >= F32(car + 0xc0)) I32(car + 0xc0) = 0;
        else {
            const long double ratio = (long double)F32(car + 0x500) / F32(car + 0x5c);
            const float share = !(1.0L > ratio) ? 1.0f : (float)ratio;
            F32(car + 0xc0) = (float)((long double)grip * F32(car + 0x98) * share * F32(car + 0xc0));
        }
    }
    const float turned = (float)((long double)F32(car + 0x4e8) + F32(car + 0xbc));
    float sum = 0.0f;
    sum = (float)(FUN_00019cac(car, 2, 0, 0, turned) + sum);
    sum = (float)(FUN_00019cac(car, 0, 0, 0, (float)((long double)F32(car + 0x4e8) + F32(car + 0xbc))) + sum);
    sum = (float)(FUN_00019cac(car, 3, 0, 0, F32(car + 0x4e8)) + sum);
    sum = (float)(FUN_00019cac(car, 1, 0, 0, F32(car + 0x4e8)) + sum);
    F32(car + 0xc34) = (float)(1.0L - ((long double)sum * (double)0.25) / ((long double)F32(car + 0x5c) * 111.0f));
    int squeal = 0;
    if (!(0.0f >= F32(car + 0x510))) {
        if (F32(car + 0xc34) >= (double)0.93) FUN_000204af(car, 0);
        else {
            FUN_000204af(car, 1);
            const long double q = ((double)0.93 - (long double)F32(car + 0xc34)) / (double)0.1;
            const double share = 1.0L >= q ? (double)q : 1.0;
            squeal = (int32_t)truncl((long double)share * (double)29952.0);
        }
    }
    if ((I32(car) == I32(0x11f0c8) && !(U8(0x130d10) & 0x80)) || (U8(0x130d10) & 0x40)) {
        const int slot = ((U8(0x130d10) & 0x40) ? I32(car) + 1 : 1) + 4;
        const uint32_t ground = 0x11ce00u + KIND(0x6b0);
        if (squeal == 0 || I32(ground + 0xc) == 0) {
            if (I32(ground + 8) == 0) FUN_000aee98(U32(0x124274), slot);
            else {
                FUN_000aebf0(U32(0x124274), I32(ground + 8) + 0x12, slot, 0, 1);
                const float v = I32(car + 0x500) < 0x42480000 ? F32(car + 0x500) : 50.0f;
                const float vol = (long double)v * 655.0f >= 32767.0f ? 32767.0f : (float)((long double)v * 655.0f);
                FUN_000aee18(U32(0x124274), slot, (int32_t)(int64_t)truncl((long double)vol));
            }
        } else {
            FUN_000aebf0(U32(0x124274), I32(ground + 0xc) + 7, slot, 0, 1);
            FUN_000aee18(U32(0x124274), slot, squeal);
        }
    }
#undef KIND
    FUN_0001c7f8(car);
    FUN_0001d9fc(car);
    I32(0x11cdcc) = I32(car + 0x4d8);
    I32(0x11cdd4) = I32(car + 0x4e0);
    FUN_0002a371(0, 0);
    F32(car + 0x4c4) = F32(0x11cddc);
}

/*
 * WHERE THE CAR IS ALONG THE ROAD (0x153c6).  The road is the car's path (+0xc4: a row of 0x24
 * bytes at 0x127350; +0x1c its nodes, +0 their records, 0x38 bytes, +0x34 the distance at each),
 * followed with a copy of the car's place on it (+0xc4.., 0x78 bytes).  From the car's last
 * distance (+0x4b8 less the laps done, in half nodes) four points of the road half a node apart
 * (0x152f7) and, for the three stretches between them, how far along each the car is (0x1766c,
 * held to 0..1) and how far it is from it; the middle one should be the nearest -- else the four
 * points step half a node towards the nearer side and it is looked at again (the game keeps the
 * stretch's share it had, for the new stretch at the end, from the first look: so does this).
 * Round the end of the road a lap is done (or undone).  Then: the node (+0xc8), the share of it in
 * two halves (+0xcc, +0xd0), the road's point there (0x1099a: +0xd4 / +0xdc), the distance
 * (+0x4b8), the car's distance from the road's middle, signed by the side (+0x4c0), the distance
 * in the road's own measure (+0x4bc), and the splits (fifths of a lap, 0x1ffcc) and laps (0x201ee).
 */
#define HOOK_153c6
static void FUN_000153c6(int car)
{
    const uint32_t sp0 = s2_sp, state = s2_push(0x78 + 12 + 4 * 12), P = state + 0x78, Q = P + 12;
    memmove(MP(state), MP(car + 0xc4), 0x78);
    F32(P) = (float)I32(car + 0x4d8); F32(P + 4) = (float)I32(car + 0x4dc); F32(P + 8) = (float)I32(car + 0x4e0);
    const uint32_t path = U32(0x127350) + 0x24u * (uint32_t)I32(car + 0xc4);
    const int nodes = I32(path + 0x1c);
#define QP(i) (Q + 12u * (uint32_t)(i))
    const int k = (int32_t)truncl(((long double)F32(car + 0x4b8) - (long double)(I32(car + 0x4a8) * nodes)) / (double)0.5);
    float u = (float)((long double)k * (double)0.5);
    int lap = 0;
    FUN_000152f7((int)path, (int)state, (int)QP(0), 0, (float)((long double)u + -(double)0.5));
    FUN_000152f7((int)path, (int)state, (int)QP(1), 0, u);
    FUN_000152f7((int)path, (int)state, (int)QP(2), 0, (float)((long double)u + (double)0.5));
    FUN_000152f7((int)path, (int)state, (int)QP(3), 0, (float)(((long double)u + (double)0.5) + (double)0.5));
    float sh[5] = { 0 }, dist[3] = { 0 };
    for (int i = 0; i < 3; i++) {
        const long double v = FUN_0001766c((int)P, (int)QP(i), (int)QP(i + 1));
        if (0.0L >= v) sh[3] = 0.0f;
        else { sh[4] = !(1.0L > v) ? 1.0f : (float)v; sh[3] = sh[4]; }
        sh[i] = sh[3];
    }
    /* how far the car is from stretch i, the share along it being sh[s] */
    #define S2R_DIST(i, s) do { \
        float A_[3], B_[3], D_[3]; const float om_ = (float)(1.0L - sh[s]); \
        for (uint32_t c_ = 0; c_ < 3; c_++) A_[c_] = (float)((long double)F32(QP((i) + 1) + 4 * c_) * sh[s]); \
        for (uint32_t c_ = 0; c_ < 3; c_++) B_[c_] = (float)((long double)F32(QP(i) + 4 * c_) * om_); \
        for (uint32_t c_ = 0; c_ < 3; c_++) D_[c_] = (float)((long double)F32(P + 4 * c_) - B_[c_]); \
        for (uint32_t c_ = 0; c_ < 3; c_++) D_[c_] = (float)((long double)D_[c_] - A_[c_]); \
        dist[i] = (float)sqrt((double)((long double)D_[1] * D_[1] + (long double)D_[0] * D_[0] + (long double)D_[2] * D_[2])); \
    } while (0)
    for (int i = 0; i < 3; i++) S2R_DIST(i, i);
    float dir;
    do {
        dir = !(dist[0] >= dist[1]) ? -1.0f : dist[2] >= dist[1] ? 0.0f : 1.0f;
        if (dir == 0.0f) break;
        u = (float)(double)((long double)dir * (double)0.5 + (double)u);
        if (dir == 1.0f) {
            for (int i = 0; i < 3; i++) { memmove(MP(QP(i)), MP(QP(i + 1)), 12); sh[i] = sh[i + 1]; }
            FUN_000152f7((int)path, (int)state, (int)QP(3), 0, (float)(((long double)u + (double)0.5) + (double)0.5));
            S2R_DIST(2, 2);
        } else {
            for (int i = 3; i > 0; i--) { memmove(MP(QP(i)), MP(QP(i - 1)), 12); sh[i] = sh[i - 1]; }
            FUN_000152f7((int)path, (int)state, (int)QP(0), 0, (float)((long double)u + -(double)0.5));
            S2R_DIST(0, 0);
        }
        if (0.0f > u) { lap = (int32_t)truncl((long double)dir); u = (float)((long double)nodes + u); }
        else if (!((long double)nodes > u)) { lap = (int32_t)truncl((long double)dir); u = (float)((long double)u - nodes); }
    } while (1);
    #undef S2R_DIST
    if (sh[1] > (double)0.99) sh[1] = (float)0.99f;
    u = (float)(double)((long double)sh[1] * (double)0.5 + (double)u);
    I32(car + 0xc8) = (int32_t)truncl((long double)u);
    const float frac = (float)((long double)u - I32(car + 0xc8));
    F32(car + 0xcc) = (float)((long double)frac * 2.0f);
    if (I32(car + 0xcc) < 0x3f800000) I32(car + 0xd0) = 0;
    else { F32(car + 0xcc) = (float)((long double)F32(car + 0xcc) + -1.0f); I32(car + 0xd0) = 1; }
    I32(car + 0xc8) = (I32(car + 0xc8) + nodes) % nodes;
    FUN_0001099a((int)U32(0x127350), car + 0xc4, (uint32_t)nodes, 0);
    I32(car + 0xd4) = (int32_t)truncl((long double)F32(car + 0x124));
    I32(car + 0xdc) = (int32_t)truncl((long double)F32(car + 0x128));
    F32(car + 0x4b8) = (float)((long double)(nodes * (I32(car + 0x4a8) + lap)) + ((long double)I32(car + 0xc8) + frac));
    const float x = (float)((long double)F32(P) - I32(car + 0xd4)), z = (float)((long double)F32(P + 8) - I32(car + 0xdc));
    float ang = (s2_bits(z) & 0x7fffffff) ? (float)atanl(fabsl((long double)x / z)) : (float)1.5707964f;
    if (0.0f < z) ang = (float)((long double)(float)3.14159274f - ang);
    if (0.0f > x) ang = (float)((long double)(float)6.28318548f - ang);
    const double side = 0.0 > FUN_00014dc0(ang, F32(car + 0x4c4)) ? -1.0 : 1.0;
    F32(car + 0x4c0) = (float)((long double)FUN_00011253((double)((long double)z * z + (long double)x * x)) * side);
    const uint32_t rows = U32(path);
#define ALONG(n) I32(rows + 0x38u * (uint32_t)(n) + 0x34u)
    if (I32(car + 0xc8) > 0)
        F32(car + 0x4bc) = (float)((long double)ALONG(I32(car + 0xc8) - 1) * (1.0L - frac) + (long double)ALONG(I32(car + 0xc8)) * frac);
    else F32(car + 0x4bc) = (float)((long double)ALONG(0) * frac);
    F32(car + 0x4bc) = (float)((long double)(ALONG(nodes - 1) * (I32(car + 0x4a8) + lap)) + F32(car + 0x4bc));
#undef ALONG
#undef QP
    s2_sp = sp0;
    const int split = (int32_t)truncl((long double)F32(car + 0x4b8) * 5.0f / nodes);
    if (split != I32(car + 0x4b0)) FUN_0001ffcc(car, split - I32(car + 0x4b0));
    if (lap) FUN_000201ee(car, lap);
}

/*
 * AN ANGLE ON THE GROUND (the game's way, written out in 0x19cac and others): of a vector's x and
 * z -- the arc tangent of |x / z| (a quarter turn when z is nought), from the other side when z is
 * over nought, and the whole turn less it when x is under nought.  A float.
 */
static float s2r_ground_angle(float x, float z)
{
    float a = (s2_bits(z) & 0x7fffffff) ? (float)atanl(fabsl((long double)x / z)) : (float)1.5707964f;
    if (0.0f < z) a = (float)((long double)(float)3.14159274f - a);
    if (0.0f > x) a = (float)((long double)(float)6.28318548f - a);
    return a;
}

/*
 * ONE TYRE ON THE GROUND (0x19cac), the wheel given (0, 2 the front) turned to the angle given.
 * The wheel's push (+0x544..) gets the slope's pull (the ground's direction under it, +0x64..,
 * made one long: 0.8 x 9.81 x 400 x the step, by how much the wheel faces across it); then it is
 * split into its part along the wheel (A) and across it (B, kept level).  The wheel's speed along
 * (signed: backwards when the wheel faces away from the car's way) goes towards what the drive
 * gives (the engine's revs x 111 in gear, +0x4fc; else its own) by the tyre's hold (0x19c61 of the
 * ground's grip for these tyres, the tyres' +0x74, and the drive / brake on this wheel: the
 * throttle +0x1ec x the brake +0x1e4 x 0.3 less the slide +0xc0, with the spring +0x78.. less a
 * fifth of the knocks over 1, +0xc..), less the handbrake (+0x1e8); held to the grip x 1500 x +0x74
 * (x 1 + handbrake) from where it was, past which the wheel skids (+0x5d4.., a sound picked from
 * its place); it moves there at the step x 5 x +0x88.. x (0.6 revs share + 0.7) of the way.  Across
 * it keeps the hold's share.  In gear, when the across push keeps its way, the along push grows by
 * its change x 0.3 (+100 = 1) or x (0.3 revs share + 0.15) over itself.  The new push, by how many
 * wheels are down (+0x510), plus the old push for the rest, under 300 is nought and under 1000
 * dies away (twice the step a second) when off the throttle (under 0.1) or out of gear; times the
 * step it moves the wheel's corner (+0x770.. -> +0x514..).  Gives how far the speed along was off.
 */
#define HOOK_19cac
static float10 FUN_00019cac(int car, int wheel, uint32_t ebx, uint32_t ecx, float angle)
{
    (void)ebx; (void)ecx;
    typedef long double ld;
    const float s = (float)sinl((ld)angle), c = (float)-cosl((ld)angle), y0 = 0.0f;
    const uint32_t push = (uint32_t)car + 0x544u + (uint32_t)wheel * 0xcu, w = (uint32_t)car + 0x574u + (uint32_t)wheel * 0x70u;
    const float dt = F32(0x133350);
    float V[3] = { F32(push), F32(push + 4), F32(push + 8) }, N[3] = { 0.0f, 0.0f, 0.0f };
    {
        const float len = (float)sqrt((double)((ld)F32(w + 0x68) * F32(w + 0x68) + (ld)F32(w + 0x64) * F32(w + 0x64) + (ld)F32(w + 0x6c) * F32(w + 0x6c)));
        if (!s2r_zero(len)) { const float inv = (float)(1.0L / len); for (uint32_t k = 0; k < 3; k++) N[k] = (float)((ld)F32(w + 0x64 + 4 * k) * inv); }
    }
    V[0] = (float)(double)((((((ld)fabsf(s) * N[0]) * (double)0.8) * (double)9.81) * (double)400.0) * dt + (double)V[0]);
    V[2] = (float)(double)((((((ld)fabsf(c) * N[2]) * (double)0.8) * (double)9.81) * (double)400.0) * dt + (double)V[2]);
    float A[3] = { 0.0f, 0.0f, 0.0f }, B[3] = { 0.0f, 0.0f, 0.0f };
    const float lenV = (float)sqrt((double)((ld)V[1] * V[1] + (ld)V[0] * V[0] + (ld)V[2] * V[2]));
    if (!(0.0f >= lenV)) {
        const float dot = (float)((ld)V[1] * y0 + (ld)V[0] * s + (ld)V[2] * c);
        A[0] = (float)((ld)s * dot); A[1] = (float)((ld)y0 * dot); A[2] = (float)((ld)c * dot);
        B[0] = (float)((ld)V[0] - A[0]); B[2] = (float)((ld)V[2] - A[2]);
    }
    float lenA = (float)sqrt((double)((ld)A[1] * A[1] + (ld)A[0] * A[0] + (ld)A[2] * A[2]));
    const float top = I32(car + 0xc7c) > 0 ? (float)((ld)F32(car + 0x4fc) * 111.0f) : lenA;
    const float rel = (float)FUN_00014dc0(s2r_ground_angle(A[0], A[2]), F32(car + 0x4e8));
    if (!(rel >= -(float)1.57079637f) || s2_bits(rel) > 0x3fc90fdb) lenA = -lenA;
    const float thr = I32(car + 0xc7c) == 0 ? (float)((ld)F32(car + 0x1e4) * (double)0.3 - F32(car + 0xc0))
                                             : (float)((ld)F32(car + 0x1ec) * F32(car + 0x1e4) * (double)0.3 - F32(car + 0xc0));
    const float knock = 0.0L >= (ld)F32(car + 0xc + 4 * (uint32_t)wheel) + -1.0f ? 0.0f : (float)((ld)F32(car + 0xc + 4 * (uint32_t)wheel) + -1.0f);
    const ld spring = (1.0L - (ld)knock * (double)0.2) * F32(car + 0x78 + 4 * (uint32_t)wheel);
    const float x = wheel == 2 || wheel == 0 ? (float)(spring + thr) : (float)(spring - thr);
    const uint32_t ground = 0x11cdf0u + 4u * (uint32_t)I32(car + 0x50) + 0x24u * (uint32_t)I32(w + 0x5c);
    const float g = (float)FUN_00019c61(F32(ground), F32(car + 0x74), lenA, x);
    if (0.0f > lenA) {
        const uint32_t sp0 = s2_sp, at = s2_push(4);
        F32(at) = lenA; FUN_000111d6((int)at, 0.0f, 10); lenA = F32(at);
        s2_sp = sp0;
    }
    float v = (float)((1.0L - g) * lenA + (ld)top * g);
    v = (float)((1.0L - F32(car + 0x1e8)) * v);
    const float diff = (float)fabsl((ld)v - lenA);
    float lim = (float)((ld)F32(ground) * 1500.0f * F32(car + 0x74));
    lim = (float)((1.0L + F32(car + 0x1e8)) * lim);
    if (lim >= fabsl((ld)v - lenA)) I32(w + 0x60) = 0;
    else {
        lim = (float)(double)((double)lim * (double)0.5);
        v = (float)((ld)(0.0L > (ld)v - lenA ? -1 : 1) * lim + lenA);
        I32(w + 0x60) = (int32_t)truncl((ld)F32(car + 0x514 + 0xc * (uint32_t)wheel) + F32(car + 0x518 + 0xc * (uint32_t)wheel)) & 0xf;
    }
    float r = (float)((ld)dt * 5.0f * F32(car + 0x88 + 4 * (uint32_t)wheel));
    r = (float)(double)((((ld)F32(car + 0xc78) * (double)0.6) / F32(car + 0x60) + (double)0.7) * (double)r);
    lenA = s2_bits(r) < 0x3f800000 ? (float)(((ld)v - lenA) * r + lenA) : v;
    float nB[3];
    A[0] = (float)((ld)s * lenA); A[1] = (float)((ld)y0 * lenA); A[2] = (float)((ld)c * lenA);
    for (int k = 0; k < 3; k++) nB[k] = (float)((ld)B[k] * g);
    if (I32(car + 0xc7c) != 0) {
        const float m = (float)sqrt((double)((ld)A[1] * A[1] + (ld)A[0] * A[0] + (ld)A[2] * A[2]));
        if (0.0f < m && 1.0L > fabsl((ld)FUN_00014dc0(s2r_ground_angle(nB[0], nB[2]), s2r_ground_angle(B[0], B[2])))) {
            float d[3];
            for (int k = 0; k < 3; k++) d[k] = (float)((ld)nB[k] - B[k]);
            const float ld_ = (float)sqrt((double)((ld)d[1] * d[1] + (ld)d[0] * d[0] + (ld)d[2] * d[2]));
            const double q = I32(car + 100) == 1 ? (double)((ld)ld_ * (double)0.3)
                : (double)((ld)ld_ * (double)(((ld)F32(car + 0xc78) / F32(car + 0x60)) * (double)0.3 + (double)0.15));
            const float la = (float)sqrt((double)((ld)A[1] * A[1] + (ld)A[0] * A[0] + (ld)A[2] * A[2]));
            const float factor = (float)((ld)q / la + 1.0L);
            for (int k = 0; k < 3; k++) A[k] = (float)((ld)A[k] * factor);
        }
    }
    const float om = (float)(1.0L - F32(car + 0x510));
    float W[3];
    for (int k = 0; k < 3; k++) W[k] = (float)((ld)V[k] * om);
    for (int k = 0; k < 3; k++) A[k] = (float)((ld)A[k] + nB[k]);
    for (int k = 0; k < 3; k++) A[k] = (float)((ld)A[k] * F32(car + 0x510));
    for (int k = 0; k < 3; k++) A[k] = (float)((ld)A[k] + W[k]);
    if (!(F32(car + 0x1ec) >= (double)0.1) || I32(car + 0xc7c) == 0) {
        const float len = (float)sqrt((double)((ld)A[1] * A[1] + (ld)A[0] * A[0] + (ld)A[2] * A[2]));
        if (s2_bits(len) < 0x43960000) A[0] = A[1] = A[2] = 0.0f;
        else if (s2_bits(len) < 0x447a0000) {
            const float f = !(1.0L >= (ld)dt * 2.0f) ? 1.0f : (float)((ld)dt * 2.0f);
            const float om2 = (float)(1.0L - f);
            for (int k = 0; k < 3; k++) A[k] = (float)((ld)A[k] * om2);
        }
    }
    for (uint32_t k = 0; k < 3; k++) {
        const float delta = (float)((ld)A[k] * dt);
        F32(car + 0x514 + 0xc * (uint32_t)wheel + 4 * k) = (float)((ld)F32(car + 0x770 + 0xc * (uint32_t)wheel + 4 * k) + delta);
    }
    return diff;
}

/*
 * THE CAR'S PICTURE, EACH FRAME (0x16878).  Between two race frames (the share given, under 1) the
 * car's record is the last frame's (+0x734..) moved the share of the way to this one's (+0x4d8..):
 * place, angles (by the angle between, 0x14dc0), heights; at 1, this frame's itself (+0x990 points
 * at the one drawn).  Into its drawing record (0x11da20 by +0xbf0: place, angles in 2048ths of a
 * turn; the shadow's (+0xbf8) turn the same) and the parts' records (0x11da24, 0x20 bytes: the
 * body's pitch +0xc18 and roll +0xc20 on the four body parts, the body parts' heights; each wheel
 * at its place and height over the ground); the front wheels steered (+0x13c, eased to pi / (4 x
 * the most steering) x the wheels' angle); each wheel turned on as it rolls (the speed over top
 * speed, by the cosine of its angle for a car below 0.2 -- front by +0x90, back by +0x94 -- times
 * pi a frame); the wheels' three angles made the drawing's (0x105cd).  Its flags (+0x20): the
 * wheels blurred when spinning over 0.29 (0x400 the front, 0x8000 the back, as the level says),
 * the brake lights on (0x200) with brake or handbrake, and the car's own (+0xb8); +0x3c = +0xc80.
 */
#define HOOK_16878
static void FUN_00016878(int car, float share)
{
    typedef long double ld;
    if (share == 1.0f) U32(car + 0x990) = (uint32_t)car + 0x4d8u;
    else {
        U32(car + 0x990) = (uint32_t)car + 0x994u;
        memmove(MP(car + 0x994), MP(car + 0x4d8), 0x25c);
#define MIX(to, now, was) I32(car + (to)) = (int32_t)truncl((ld)(I32(car + (now)) - I32(car + (was))) * share + (ld)I32(car + (was)))
        MIX(0x994, 0x4d8, 0x734); MIX(0x99c, 0x4e0, 0x73c); MIX(0x998, 0x4dc, 0x738);
        F32(car + 0x9a0) = (float)((ld)FUN_00014dc0(F32(car + 0x4e4), F32(car + 0x740)) * share + F32(car + 0x740));
        F32(car + 0x9a8) = (float)((ld)FUN_00014dc0(F32(car + 0x4ec), F32(car + 0x748)) * share + F32(car + 0x748));
        F32(car + 0x9a4) = (float)((ld)FUN_00014dc0(F32(car + 0x4e8), F32(car + 0x744)) * share + F32(car + 0x744));
        MIX(0x9b0, 0x4f4, 0x750); MIX(0x9b4, 0x4f8, 0x754);
#undef MIX
    }
#define TURNS(f) ((uint32_t)(int32_t)truncl((ld)(f) * 1024.0f / (float)3.14159274f) & 0x7ffu)
    const uint32_t obj = U32(car + 0x990);
    const uint32_t parts = U32(0x11da24u + 0x1cu * (uint32_t)I32(car + 0xbf0)), rec = U32(0x11da20u + 0x1cu * (uint32_t)I32(car + 0xbf0));
    const uint32_t shadow = U32(0x11da20u + 0x1cu * (uint32_t)I32(car + 0xbf8));
#define PART(i) (parts + 0x20u * (uint32_t)(i))
    U32(rec + 8) = U32(obj); U32(rec + 0xc) = U32(obj + 4); U32(rec + 0x10) = U32(obj + 8);
    U32(rec + 0x14) = TURNS(F32(obj + 0xc));
    U32(rec + 0x18) = TURNS(F32(obj + 0x10));
    U32(shadow + 0x18) = U32(rec + 0x18);
    U32(rec + 0x1c) = TURNS(F32(obj + 0x14));
    U32(PART(I32(car + 0x208)) + 8) = TURNS(F32(car + 0xc18));
    U32(PART(I32(car + 0x200)) + 8) = U32(PART(I32(car + 0x208)) + 8);
    U32(PART(I32(car + 0x1f8)) + 8) = U32(PART(I32(car + 0x200)) + 8);
    U32(PART(I32(car + 0xbf4)) + 8) = U32(PART(I32(car + 0x1f8)) + 8);
    U32(PART(I32(car + 0x208)) + 0x10) = TURNS(F32(car + 0xc20));
    U32(PART(I32(car + 0x200)) + 0x10) = U32(PART(I32(car + 0x208)) + 0x10);
    U32(PART(I32(car + 0x1f8)) + 0x10) = U32(PART(I32(car + 0x200)) + 0x10);
    U32(PART(I32(car + 0xbf4)) + 0x10) = U32(PART(I32(car + 0x1f8)) + 0x10);
    I32(PART(I32(car + 0x1f8)) + 0x18) = I32(car + 0x214) + I32(obj + 0x1c);
    I32(PART(I32(car + 0xbf4)) + 0x18) = I32(car + 0x218) + I32(obj + 0x1c);
    I32(PART(I32(car + 0x200)) + 0x18) = I32(car + 0x21c) + I32(obj + 0x1c);
    I32(PART(I32(car + 0x208)) + 0x18) = I32(car + 0x220) + I32(obj + 0x1c);
    for (uint32_t i = 0; i < 4; i++) {
        const uint32_t w = obj + i * 0x70u, hub = PART(I32(w + 0xf4)), tyre = PART(I32(w + 0xec));
        const uint32_t x = U32(w + 0xd4), z = U32(w + 0xdc);
        const int32_t y = I32(w + 0xd8);
        U32(hub + 0x10) = 0; U32(tyre + 0x10) = U32(hub + 0x10);
        U32(hub + 0x14) = x; U32(tyre + 0x14) = U32(hub + 0x14);
        I32(hub + 0x18) = y - I32(w + 0xe4); U32(tyre + 0x18) = U32(hub + 0x18);
        U32(hub + 0x1c) = z; U32(tyre + 0x1c) = U32(hub + 0x1c);
    }
    FUN_0001016b(car + 0x13c, (float)((ld)(float)3.14159274f / ((ld)F32(car + 0x6c) * 4.0f) * F32(obj + 0x188)), 0x50);
    U32(PART(I32(obj + 0x1d4)) + 0xc) = TURNS(F32(car + 0x13c));
    U32(PART(I32(obj + 0x1cc)) + 0xc) = U32(PART(I32(obj + 0x1d4)) + 0xc);
    U32(PART(I32(obj + 0xf4)) + 0xc) = U32(PART(I32(obj + 0x1cc)) + 0xc);
    U32(PART(I32(obj + 0xec)) + 0xc) = U32(PART(I32(obj + 0xf4)) + 0xc);
    U32(PART(I32(obj + 0x244)) + 0xc) = 0;
    U32(PART(I32(obj + 0x23c)) + 0xc) = U32(PART(I32(obj + 0x244)) + 0xc);
    U32(PART(I32(obj + 0x164)) + 0xc) = U32(PART(I32(obj + 0x23c)) + 0xc);
    U32(PART(I32(obj + 0x15c)) + 0xc) = U32(PART(I32(obj + 0x164)) + 0xc);
    const float front = !(F32(car + 0x90) >= (double)0.2) ? (float)((ld)F32(obj + 0x28) * cosl((ld)F32(car + 0x4d4)) / F32(car + 0x5c))
                                                          : (float)((ld)F32(obj + 0x24) / F32(car + 0x5c));
    const float back = !(F32(car + 0x94) >= (double)0.2) ? (float)((ld)F32(obj + 0x28) * cosl((ld)F32(car + 0x4d4)) / F32(car + 0x5c))
                                                         : (float)((ld)F32(obj + 0x24) / F32(car + 0x5c));
    F32(car + 0x584) = (float)((ld)F32(car + 0x584) - (ld)front * (float)3.14159274f);
    F32(car + 0x664) = (float)((ld)F32(car + 0x664) - (ld)front * (float)3.14159274f);
    F32(car + 0x5f4) = (float)((ld)F32(car + 0x5f4) - (ld)back * (float)3.14159274f);
    F32(car + 0x6d4) = (float)((ld)F32(car + 0x6d4) - (ld)back * (float)3.14159274f);
    U32(PART(I32(obj + 0xec)) + 8) = TURNS(F32(car + 0x584));
    U32(PART(I32(obj + 0x1cc)) + 8) = TURNS(F32(car + 0x664));
    U32(PART(I32(obj + 0x15c)) + 8) = TURNS(F32(car + 0x5f4));
    U32(PART(I32(obj + 0x23c)) + 8) = TURNS(F32(car + 0x6d4));
    U32(PART(I32(obj + 0xf4)) + 8) = 0; U32(PART(I32(obj + 0x1d4)) + 8) = 0;
    U32(PART(I32(obj + 0x164)) + 8) = 0; U32(PART(I32(obj + 0x244)) + 8) = 0;
    const uint32_t sp0 = s2_sp, angles = s2_push(24), out = angles + 12;
    for (uint32_t i = 0; i < 4; i++) {
        const uint32_t tyre = PART(I32(obj + i * 0x70u + 0xec));
        for (uint32_t k = 0; k < 3; k++) F32(angles + 4 * k) = (float)((ld)I32(tyre + 8 + 4 * k) * 0.0009765625f * (float)3.14159274f);
        FUN_000105cd((int)out, (int)angles);
        for (uint32_t k = 0; k < 3; k++) U32(tyre + 8 + 4 * k) = TURNS(F32(out + 4 * k));
    }
    s2_sp = sp0;
    U32(rec + 0x3c) = U32(car + 0xc80);
    const int fast_front = fabsf(front) > (double)0.29, fast_back = fabsf(back) > (double)0.29;
    if (F32(car + 0x90) > (double)0.8) U32(rec + 0x20) = (fast_front ? 0x400u : 0u) | (fast_back ? 0x8000u : 0u);
    else if (F32(car + 0x94) > (double)0.8) U32(rec + 0x20) = (fast_back ? 0x400u : 0u) | (fast_front ? 0x8000u : 0u);
    else U32(rec + 0x20) = fast_back ? 0x400u : 0u;
    const ld brake = (ld)F32(car + 0x1e8) + F32(car + 0x1e4);
    U32(rec + 0x20) |= U32(car + 0xb8) | ((brake == 0.0L || brake != brake) ? 0u : 0x200u);
#undef PART
#undef TURNS
}

/*
 * WHICH CARS MAY TOUCH (0x22d35): every pair of cars whose middles (the mean of the four corners,
 * +0x514 +0x52c +0x520 +0x538 summed in that order) are nearer than their two lengths (+0x4d0)
 * goes to the collision (0x20a18).  The game measures that distance in x and in HEIGHT, not z.
 */
#define HOOK_22d35
static void FUN_00022d35(void)
{
    typedef long double ld;
    const int n = I32(0x130c48);
    for (int i = 0; i < n; i++) {
        float ci[3], cj[3];
        const uint32_t a = 0x119300u + 0xcc0u * (uint32_t)i;
        for (uint32_t k = 0; k < 3; k++)
            ci[k] = (float)((float)((float)((float)((ld)F32(a + 0x514 + 4 * k) + F32(a + 0x52c + 4 * k)) + (ld)F32(a + 0x520 + 4 * k)) + (ld)F32(a + 0x538 + 4 * k)) * 0.25f);
        for (int j = i + 1; j < n; j++) {
            const uint32_t b = 0x119300u + 0xcc0u * (uint32_t)j;
            for (uint32_t k = 0; k < 3; k++)
                cj[k] = (float)((float)((float)((float)((ld)F32(b + 0x514 + 4 * k) + F32(b + 0x52c + 4 * k)) + (ld)F32(b + 0x520 + 4 * k)) + (ld)F32(b + 0x538 + 4 * k)) * 0.25f);
            const double reach = (double)(I32(a + 0x4d0) + I32(b + 0x4d0));
            const double d = FUN_00011253((double)(((ld)ci[1] - cj[1]) * ((ld)ci[1] - cj[1]) + ((ld)ci[0] - cj[0]) * ((ld)ci[0] - cj[0])));
            if (!(d >= reach)) FUN_00020a18(i, j);
        }
    }
}

/*
 * THE CAR'S STEP ON THE GROUND (0x1b92b).  A spin (+0x58, its way as a whole number) goes on only
 * while it turns the car further the way it already faces off its travel (+0x54 against the turn
 * +0x4e8) and the wheels do not steer against it: the turn eases towards +0x54 + a tenth of it;
 * else it stops.  Then the corners (0x1d9fc), the springs (0x1bef1), and 0x1e529; a car out of the
 * race slides (0x19810).  Spinning, the four wheels' pushes are made their mean.  Facing more
 * than a quarter turn off the road's way (+0x4c4) the "wrong way" clock (+0x1f0) runs down to -1,
 * else up to 1, by the step (0x133354).  Then the body's lean (0x1c4fb).
 */
#define HOOK_1b92b
static void FUN_0001b92b(int car)
{
    typedef long double ld;
    if (I32(car + 0x58) != 0) {
        const float spin = (float)I32(car + 0x58);
        if (0.0L > (ld)FUN_00014dc0(F32(car + 0x54), F32(car + 0x4e8)) * spin) I32(car + 0x58) = 0;
        else if (!(0.0L <= (ld)I32(car + 0x58) * F32(car + 0x660))) I32(car + 0x58) = 0;
        else FUN_0001016b(car + 0x4e8, (float)((ld)I32(car + 0x58) * (double)0.1 + F32(car + 0x54)), 0x57);
    }
    FUN_0001d9fc(car);
    FUN_0001bef1(car);
    FUN_0001e529(car);
    if (I32(car + 0x150) != 0) FUN_00019810(car, F32(car + 0x160));
    if (I32(car + 0x58) != 0) {
        float m[3];
        for (uint32_t k = 0; k < 3; k++)
            m[k] = (float)((float)((float)((float)((ld)F32(car + 0x544 + 4 * k) + F32(car + 0x550 + 4 * k)) + (ld)F32(car + 0x55c + 4 * k)) + (ld)F32(car + 0x568 + 4 * k)) * 0.25f);
        for (uint32_t w = 0; w < 4; w++) for (uint32_t k = 0; k < 3; k++) F32(car + 0x544 + 0xc * w + 4 * k) = m[k];
    }
    if (fabsl((ld)FUN_00014dc0(F32(car + 0x4e8), F32(car + 0x4c4))) >= (double)1.570796370506286) {
        F32(car + 0x1f0) = (float)((ld)F32(car + 0x1f0) - F32(0x133354));
        if (!(F32(car + 0x1f0) >= -1.0f)) F32(car + 0x1f0) = -1.0f;
    } else {
        F32(car + 0x1f0) = (float)((ld)F32(0x133354) + F32(car + 0x1f0));
        if (I32(car + 0x1f0) > 0x3f800000) F32(car + 0x1f0) = 1.0f;
    }
    FUN_0001c4fb(car);
}

/*
 * THE CAR AGAINST A WALL (0x29571).  A corner (+0x514..) off the drivable ground (0x28ce6 with 1)
 * is off (+0x170.., 0x20 apart).  For each corner off, the nearest wall (0x2a371; its record at
 * 0x11cdd8: +0 its angle, +4 / +8 a point) gives the way out: the corner turned into the wall's
 * frame, put 2 units out of it, and turned back -- the move (+0x174.. x, +0x17c.. z) and the wall's
 * angle less pi (+0x180..).  The biggest move (|x| + |z|) is taken (+0x154 / +0x15c, the wall's
 * way +0x160, that corner).  If the first corner still on the ground looks along the move (its
 * way to the wall point and the move less than a quarter turn apart), the car is not against the
 * wall after all.  Else the car is moved out; the wall's way is turned to face the car's; sparks
 * fly from the corner at the speed in km/h-ish (x 400000 / 3600) with the wheel's push but none
 * across.  Slow for its steering (|+0x4d4| x speed / top under 0.2 pi): where the car's line meets
 * the wall's (0x20970), ahead of it, the car spins (+0x58) towards the wall's way (picked by speed,
 * computer or not, how much it steers and which way the road goes), +0x54 that plus 0.06 pi; and
 * the knock (0x19aaa) on the side the wall is (front 1, left 0, right 2, back 3) by how square it
 * hit, as hard as the wheel's push across the wall; not meeting, the spin is just the wall's way.
 * Moving into the wall the four wheels' upward speeds are made their mean; else not against it.
 * The followed car's thud by how hard (over 500, 4000, 10000: samples 0xc 0xd 0xe); +0x30 1, +0x34 0.3.
 */
#define HOOK_29571
static void FUN_00029571(int car, int edx_in)
{
    typedef long double ld;
    uint32_t edx = (uint32_t)edx_in;
    float hard = 0.0f;
    int hit = 0;
    I32(car + 0x150) = 0;
    for (uint32_t i = 0; i < 4; i++) {
        I32(0x11cdcc) = (int32_t)truncl((ld)F32(car + 0x514 + 0xc * i));
        I32(0x11cdd4) = (int32_t)truncl((ld)F32(car + 0x51c + 0xc * i));
        const uint64_t r = FUN_00028ce6(1, edx);
        I32(car + 0x170 + 0x20 * i) = (uint32_t)r == 0;
        if (I32(car + 0x170 + 0x20 * i)) I32(car + 0x150) = 1;
    }
    if (I32(car + 0x150) == 0) return;
    const uint32_t sp0 = s2_sp, v = s2_push(12 * 8 + 8), o1 = v + 12, o2 = v + 24, P = v + 36, D = v + 48, Q = v + 60, E = v + 72, T = v + 84;
    for (uint32_t i = 0; i < 4; i++) {
        const uint32_t row = (uint32_t)car + 0x164u + 0x20u * i;
        if (I32(row + 0xc) == 0) { I32(row + 0x18) = 0; I32(row + 0x10) = I32(row + 0x18); I32(row + 0x1c) = 0; continue; }
        I32(0x11cdcc) = (int32_t)truncl((ld)F32(car + 0x514 + 0xc * i));
        I32(0x11cdd4) = (int32_t)truncl((ld)F32(car + 0x51c + 0xc * i));
        const uint64_t r = FUN_0002a371(1, edx);
        edx = (uint32_t)(r >> 32);
        if ((uint32_t)r == 0) continue;
        const uint32_t wall = U32(0x11cdd8);
        F32(row + 0x1c) = (float)FUN_00014dc0((float)3.14159274f, F32(wall));
        I32(v) = I32(0x11cdcc) - I32(wall + 4);
        I32(v + 8) = I32(0x11cdd4) - I32(wall + 8);
        FUN_00010388((int)o1, (int)v, 0, 0, F32(row + 0x1c));
        I32(o1) = 2;
        FUN_00010388((int)o2, (int)o1, 0, 0, -F32(row + 0x1c));
        I32(row + 0x10) = I32(o2) - I32(v);
        I32(row + 0x18) = I32(o2 + 8) - I32(v + 8);
    }
    I32(car + 0x15c) = 0;
    I32(car + 0x154) = I32(car + 0x15c);
    for (uint32_t i = 0; i < 4; i++) {
        const uint32_t row = (uint32_t)car + 0x164u + 0x20u * i;
        if (I32(row + 0xc) == 0) continue;
        if (s2_abs(I32(car + 0x154)) + s2_abs(I32(car + 0x15c)) <= s2_abs(I32(row + 0x10)) + s2_abs(I32(row + 0x18))) {
            I32(car + 0x154) = I32(row + 0x10); I32(car + 0x15c) = I32(row + 0x18);
            F32(car + 0x160) = F32(row + 0x1c);
            hit = (int)i;
        }
    }
    for (uint32_t i = 0; i < 4; i++) {
        if (I32(car + 0x170 + 0x20 * i) != 0) continue;
        const float mx = (float)I32(car + 0x154), mz = (float)I32(car + 0x15c);
        const uint32_t wall = U32(0x11cdd8);
        const float wx = (float)((ld)I32(wall + 4) - F32(car + 0x514 + 0xc * i)), wz = (float)((ld)I32(wall + 8) - F32(car + 0x51c + 0xc * i));
        const float to_wall = s2r_ground_angle(wx, wz), move = s2r_ground_angle(mx, mz);
        if (!(fabsl((ld)FUN_00014dc0(move, to_wall)) >= (double)1.570796370506286)) { I32(car + 0x150) = 0; s2_sp = sp0; return; }
        break;
    }
    I32(car + 0x4d8) += I32(car + 0x154);
    I32(car + 0x4e0) += I32(car + 0x15c);
    if (fabsl((ld)FUN_00014dc0(F32(car + 0x160), F32(car + 0x4e8))) > (double)1.570796370506286)
        F32(car + 0x160) = (float)FUN_00014dc0(F32(car + 0x160), (float)3.14159274f);
    const uint32_t push = (uint32_t)car + 0x544u + 0xcu * (uint32_t)hit;
    F32(v) = 0.0f; F32(v + 4) = F32(push + 4); F32(v + 8) = F32(push + 8);
    FUN_0002aeb9(car + 0x514 + 0xc * hit, (int)v, 0, 0, (float)((ld)F32(car + 0x500) * 400000.0f / 3600.0f), 0);
    F32(P) = (float)I32(car + 0x4d8); F32(P + 8) = (float)I32(car + 0x4e0);
    F32(D) = (float)sinl((ld)F32(car + 0x4e8)); F32(D + 8) = (float)-cosl((ld)F32(car + 0x4e8));
    { const uint32_t wall = U32(0x11cdd8);
      F32(Q) = (float)I32(wall + 4); F32(Q + 8) = (float)I32(wall + 8); F32(Q + 4) = (float)I32(car + 0x4dc); }
    F32(E) = (float)sinl((ld)F32(car + 0x160)); F32(E + 8) = (float)-cosl((ld)F32(car + 0x160)); F32(E + 4) = 0.0f;
    if (!((ld)fabsf(F32(car + 0x4d4)) * F32(car + 0x500) / F32(car + 0x5c) >= (double)0.6283185482025146)) {
        if (FUN_00020970((int)P, (int)D, (int)Q, (int)E, (int)T, (int)(T + 4)) == 0) {
            F32(car + 0x54) = F32(car + 0x160);
            I32(car + 0x58) = 0.0L > (ld)FUN_00014dc0(F32(car + 0x54), F32(car + 0x4e8)) ? -1 : 1;
        } else {
            if (!(0.0f >= F32(T))) {
                float a;
                if (I32(car + 0x500) < 0x41f00000 && I32(car + 0xb4) == 0) {
                    a = fabsl((ld)FUN_00014dc0(F32(car + 0x160), F32(car + 0x4c4))) > (double)1.570796370506286
                        ? (float)FUN_00014dc0(F32(car + 0x160), (float)3.14159274f) : F32(car + 0x160);
                } else if (I32(car + 0x500) >= 0x41a00000 || !(fabsf(F32(car + 0x660)) > (ld)F32(car + 0x6c) * (double)0.2)) {
                    const float off = (float)fabsl((ld)FUN_00014dc0(F32(car + 0x4e8), F32(car + 0x4c4)));
                    if (!(0.0L > ((ld)(float)1.57079637f - off) * F32(car + 0x1f0))) a = F32(car + 0x160);
                    else a = fabsl((ld)FUN_00014dc0(F32(car + 0x4c4), F32(car + 0x160))) > (double)1.570796370506286
                        ? (float)FUN_00014dc0(F32(car + 0x160), -(float)3.14159274f) : F32(car + 0x160);
                } else {
                    const int sg = 0.0f > F32(car + 0x660) ? -1 : 1;
                    const float r = (float)FUN_00014dc0((float)((ld)sg * (float)1.57079637f + F32(car + 0x4e8)), F32(car + 0x160));
                    a = fabsf(r) > (double)1.570796370506286 ? (float)FUN_00014dc0(F32(car + 0x160), -(float)3.14159274f) : F32(car + 0x160);
                }
                I32(car + 0x58) = 0.0L > (ld)FUN_00014dc0(a, F32(car + 0x4e8)) ? -1 : 1;
                F32(car + 0x54) = (float)((ld)I32(car + 0x58) * (float)3.14159274f * (double)0.06 + a);
            }
            const float mx = (float)I32(car + 0x154), mz = (float)I32(car + 0x15c);
            const float side = (float)FUN_00014dc0((float)((ld)s2r_ground_angle(mx, mz) + (float)3.14159274f), F32(car + 0x4e8));
            const double across = (double)fabsl(sinl((ld)FUN_00014dc0(s2r_ground_angle(F32(push), F32(push + 8)), F32(car + 0x160))));
            const float len = (float)sqrt((double)((ld)F32(push + 4) * F32(push + 4) + (ld)F32(push) * F32(push) + (ld)F32(push + 8) * F32(push + 8)));
            hard = (float)((ld)len * across);
            if (!(fabsf(side) >= (double)0.7853981852531433)) FUN_00019aaa(car, 1, hit, 0, (float)(fabsl((ld)side) / (double)0.7853981852531433), hard);
            else if (fabsf(side) > (double)2.35619455575943) FUN_00019aaa(car, 3, hit, 0, (float)(fabsl((ld)(float)3.14159274f - side) / (double)0.7853981852531433), hard);
            else if (0.0f > side) FUN_00019aaa(car, 0, hit, 0, (float)(fabsl(-(ld)side + -(float)1.57079637f) / (double)0.7853981852531433), hard);
            else FUN_00019aaa(car, 2, hit, 0, (float)(fabsl((ld)side + -(float)1.57079637f) / (double)0.7853981852531433), hard);
        }
    }
    s2_sp = sp0;
    const ld into = -cosl((ld)F32(car + 0x508)) * I32(car + 0x15c) + sinl((ld)F32(car + 0x508)) * I32(car + 0x154);
    if (0.0L >= into) {
        const float mean = (float)((((ld)F32(car + 0x574) + F32(car + 0x5e4)) + F32(car + 0x654) + F32(car + 0x6c4)) * (double)0.25);
        for (uint32_t i = 0; i < 4; i++) F32(car + 0x574 + 0x70 * i) = mean;
    } else I32(car + 0x150) = 0;
    if (I32(car) == I32(0x11f0c8) && !(U8(0x130d10) & 0x80)) {
        if (s2_bits(hard) > 0x461c4000) FUN_000aea54(U32(0x124274), 0xe, 4, 0, 0, 0);
        else if (s2_bits(hard) > 0x457a0000) FUN_000aea54(U32(0x124274), 0xd, 4, 0, 0, 0);
        else if (s2_bits(hard) > 0x43fa0000) FUN_000aea54(U32(0x124274), 0xc, 4, 0, 0, 0);
    }
    F32(car + 0x30) = 1.0f;
    F32(car + 0x34) = (float)0.3f;
}

/*
 * THE CAR AND THE CONES (0x2a639), every car unless time attack (mode 4), then only the player's.
 * Each cone (0x124664.., 0x3c bytes, 0x126d4c of them) within 200 in height and 2000 on the ground
 * (x, z from the car's middle +0x4d8 / +0x4e0): the car's nose point (half its length +0x4d0 along
 * its last step's way, at the last and this frame's places +0x734.. / +0x4d8..) swept between the
 * two (0x1766c: how far along the cone is), and nearer the cone than the car's reach (its width and
 * length by the wheels' angle +0x4d4): the cone flies off along the car's travel (+0x508) turned by
 * (its place's low four bits - 7) x 0.0314, up by the sine of the car's pitch +0x50c + 0.26, at the
 * speed x 1.4 x 111; a cone not hit before counts (+0xc88), and it is hit (+0x38, +0x34); the
 * followed car's knock (sample 0x10).
 */
#define HOOK_2a639
static void FUN_0002a639(int car)
{
    typedef long double ld;
    if ((U8(0x130d10) & 4) && I32(car) != I32(0x11f0d0)) return;
    const uint32_t end = 0x124664u + 0x3cu * (uint32_t)I32(0x126d4c);
    const float reach = (float)(fabsl(cosl((ld)F32(car + 0x4d4)) * I32(car + 0x4c8)) + fabsl(sinl((ld)F32(car + 0x4d4)) * I32(car + 0x4d0)));
    const uint32_t sp0 = s2_sp, A = s2_push(24), B = A + 12;
    for (uint32_t cone = 0x124664u; cone < end; cone += 0x3c) {
        if (!(fabsl((ld)I32(car + 0x4dc) - F32(cone + 4)) < (double)200.0)) continue;
        const float d = (float)FUN_00011253((double)(((ld)I32(car + 0x4e0) - F32(cone + 8)) * ((ld)I32(car + 0x4e0) - F32(cone + 8))
                                                     + ((ld)I32(car + 0x4d8) - F32(cone)) * ((ld)I32(car + 0x4d8) - F32(cone))));
        if (!(s2_bits(d) < 0x44fa0000)) continue;
        const float cx = (float)I32(car + 0x4d8), cz = (float)I32(car + 0x4e0), px = (float)I32(car + 0x734), pz = (float)I32(car + 0x73c);
        const float dx = (float)((ld)cx - px), dz = (float)((ld)cz - pz);
        const float half = (float)((ld)I32(car + 0x4d0) * (double)0.5);
        const float len = (float)sqrt((double)((ld)0.0f * 0.0f + (ld)dx * dx + (ld)dz * dz));
        float u[3] = { 0.0f, 0.0f, 0.0f };
        if (!s2r_zero(len)) { const float inv = (float)(1.0L / len); u[0] = (float)((ld)dx * inv); u[1] = (float)((ld)0.0f * inv); u[2] = (float)((ld)dz * inv); }
        const float h[3] = { (float)((ld)u[0] * half), (float)((ld)u[1] * half), (float)((ld)u[2] * half) };
        F32(A) = (float)((ld)px + h[0]); F32(A + 4) = h[1]; F32(A + 8) = (float)((ld)pz + h[2]);
        F32(B) = (float)((ld)cx + h[0]); F32(B + 4) = h[1]; F32(B + 8) = (float)((ld)cz + h[2]);
        const float s = (float)FUN_0001766c((int)cone, (int)A, (int)B);
        if (0.0f > s || s2_bits(s) > 0x3f800000) continue;
        const float om = (float)(1.0L - s);
        float D[3];
        for (uint32_t k = 0; k < 3; k++) {
            const float c = (float)((ld)(float)((ld)F32(B + 4 * k) * s) + (float)((ld)F32(A + 4 * k) * om));
            D[k] = (float)((ld)c - F32(cone + 4 * k));
        }
        D[1] = 0.0f;
        const float dist = (float)sqrt((double)((ld)D[1] * D[1] + (ld)D[0] * D[0] + (ld)D[2] * D[2]));
        if (!(dist < reach)) continue;
        const int32_t r = (int32_t)truncl((ld)F32(cone) + F32(cone + 8));
        const float a = (float)((ld)(int32_t)(((uint32_t)r & 0xfu) - 7u) * (float)0.0314159282f + F32(car + 0x508));
        F32(cone + 0x18) = (float)sinl((ld)a);
        F32(cone + 0x20) = (float)-cosl((ld)a);
        F32(cone + 0x1c) = (float)sinl((ld)F32(car + 0x50c) + (float)0.261799395f);
        const float sp = (float)((ld)F32(car + 0x500) * (double)1.4 * (double)111.0);
        for (uint32_t k = 0; k < 3; k++) F32(cone + 0x18 + 4 * k) = (float)((ld)F32(cone + 0x18 + 4 * k) * sp);
        if (I32(cone + 0x38) == 0) I32(car + 0xc88) += 1;
        I32(cone + 0x38) = 1;
        I32(cone + 0x34) = I32(cone + 0x38);
        if (I32(car) == I32(0x11f0c8) && !(U8(0x130d10) & 0x80)) FUN_000aea54(U32(0x124274), 0x10, 4, 0, 0, 0);
    }
    s2_sp = sp0;
}

/*
 * THE CAR'S SPEED AND WAY (0x1e529), from where its middle went since the last frame (+0x4d8..
 * against +0x734..): the way on the ground (+0x508; the car's turn when it did not move), the speed
 * (+0x500: the move over the step, x 3600 / 400000).  Each wheel's push (+0x544..) is its corner's
 * move (+0x514.. from +0x770..) over the step; after a knock (+0x34 over nought) a push faster than
 * its cap (the knock's own push +0x7a0.., at least 3330) is cut to it and the corner put where that
 * takes it, and the car moves by the biggest such change (x and z); the knock is then over.  The
 * pitch of the move (+0x50c), the turn's speed (+0x44: the turn since the last frame over the step),
 * and the wheels' angle to the way (+0x4d4).
 */
#define HOOK_1e529
static void FUN_0001e529(int car)
{
    typedef long double ld;
    const float dx = (float)(I32(car + 0x4d8) - I32(car + 0x734)), dz = (float)(I32(car + 0x4e0) - I32(car + 0x73c)), dy = (float)(I32(car + 0x4dc) - I32(car + 0x738));
    if (s2r_zero(dx) && s2r_zero(dz)) F32(car + 0x508) = F32(car + 0x4e8);
    else F32(car + 0x508) = s2r_ground_angle(dx, dz);
    const float dt = F32(0x133350);
    if (s2r_zero(dt)) I32(car + 0x500) = 0;
    else {
        const float len = (float)sqrt((double)((ld)dy * dy + (ld)dx * dx + (ld)dz * dz));
        F32(car + 0x500) = (float)((ld)len / dt * 3600.0f / 400000.0f);
    }
    float best[3] = { 0.0f, 0.0f, 0.0f };
    for (uint32_t i = 0; i < 4; i++) {
        const uint32_t push = (uint32_t)car + 0x544u + 0xcu * i, corner = (uint32_t)car + 0x514u + 0xcu * i, was = (uint32_t)car + 0x770u + 0xcu * i;
        if (s2r_zero(dt)) { I32(push + 8) = 0; F32(push + 4) = F32(push + 8); F32(push) = F32(push + 4); continue; }
        float v[3];
        for (uint32_t k = 0; k < 3; k++) v[k] = (float)((ld)F32(corner + 4 * k) - F32(was + 4 * k));
        for (uint32_t k = 0; k < 3; k++) F32(push + 4 * k) = (float)((ld)v[k] / dt);
        if (!(0.0f >= F32(car + 0x34))) {
            const uint32_t k7 = (uint32_t)car + 0x7a0u + 0xcu * i;
            const float own = (float)sqrt((double)((ld)F32(k7 + 4) * F32(k7 + 4) + (ld)F32(k7) * F32(k7) + (ld)F32(k7 + 8) * F32(k7 + 8)));
            const float cap = own > 3330.0f ? own : 3330.0f;
            const float m = (float)sqrt((double)((ld)F32(push + 4) * F32(push + 4) + (ld)F32(push) * F32(push) + (ld)F32(push + 8) * F32(push + 8)));
            if (m > cap) {
                float u[3] = { 0.0f, 0.0f, 0.0f };
                if (!s2r_zero(m)) { const float inv = (float)(1.0L / m); for (uint32_t k = 0; k < 3; k++) u[k] = (float)((ld)F32(push + 4 * k) * inv); }
                for (uint32_t k = 0; k < 3; k++) F32(push + 4 * k) = (float)((ld)u[k] * cap);
                const float old[3] = { F32(corner), F32(corner + 4), F32(corner + 8) };
                for (uint32_t k = 0; k < 3; k++) F32(corner + 4 * k) = (float)((ld)F32(was + 4 * k) + (float)((ld)F32(push + 4 * k) * dt));
                float d[3];
                for (uint32_t k = 0; k < 3; k++) d[k] = (float)((ld)F32(corner + 4 * k) - old[k]);
                const float dl = (float)sqrt((double)((ld)d[1] * d[1] + (ld)d[0] * d[0] + (ld)d[2] * d[2]));
                const float bl = (float)sqrt((double)((ld)best[1] * best[1] + (ld)best[0] * best[0] + (ld)best[2] * best[2]));
                if (bl < dl) { best[0] = d[0]; best[1] = d[1]; best[2] = d[2]; }
            }
        }
    }
    I32(car + 0x34) = 0;
    I32(car + 0x4d8) = (int32_t)truncl((ld)(float)((ld)(float)I32(car + 0x4d8) + best[0]));
    I32(car + 0x4e0) = (int32_t)truncl((ld)(float)((ld)(float)I32(car + 0x4e0) + best[2]));
    const float h = (float)-sqrt((double)((ld)dz * dz + (ld)dx * dx));
    if (s2r_zero(h)) I32(car + 0x50c) = 0;
    else F32(car + 0x50c) = s2r_ground_angle(dy, h);
    if (s2r_zero(dt)) I32(car + 0x44) = 0;
    else F32(car + 0x44) = (float)((ld)FUN_00014dc0(F32(car + 0x4e8), F32(car + 0x744)) / dt);
    F32(car + 0x4d4) = (float)FUN_00014dc0(F32(car + 0x508), F32(car + 0x4e8));
}

/*
 * THE CAR'S ANGLES FROM ITS CORNERS (0x1c7f8).  Ahead = the mean of the two sides' front-less-back
 * (+0x514 - +0x520, +0x52c - +0x538), across = the mean of the two ends' side-to-side; the turn
 * (+0x4e8) is ahead's on the ground; ahead turned back by it gives the pitch (+0x4e4); across turned
 * back by both gives the roll (+0x4ec, the other way).  Each kept within a half turn either way
 * (0x14dc0 against nought); pitch and roll over 0.45 pi are put at 0.45 pi, over 0.3 pi eased back
 * to 0.3 pi (0x1016b, by 5).  The middle (+0x4d8, +0x4e0) is the corners' mean; each corner's height
 * the wheel's (+0x5a4..); +0xc14 / +0xc0c the slopes across and along of the springs' heights
 * (+0x5b8 +0x698 +0x708) over the width and length.
 */
static void s2r_hold_angle(int car, uint32_t at)
{
    typedef long double ld;
    if (fabsf(F32(car + at)) > (double)1.413716733455658)
        F32(car + at) = (float)((ld)(0.0f > F32(car + at) ? -1 : 1) * (float)3.14159274f * (double)0.45);
    if (fabsf(F32(car + at)) > (double)0.942477822303772)
        FUN_0001016b(car + (int)at, (float)((ld)(0.0f > F32(car + at) ? -1 : 1) * (float)3.14159274f * (double)0.3), 5);
}
#define HOOK_1c7f8
static void FUN_0001c7f8(int car)
{
    typedef long double ld;
    float A[3], B[3];
    for (uint32_t k = 0; k < 3; k++) {
        const float l = (float)((ld)F32(car + 0x514 + 4 * k) - F32(car + 0x520 + 4 * k)), r = (float)((ld)F32(car + 0x52c + 4 * k) - F32(car + 0x538 + 4 * k));
        A[k] = (float)((ld)(float)((ld)r + l) * 0.5f);
    }
    for (uint32_t k = 0; k < 3; k++) {
        const float b = (float)((ld)F32(car + 0x538 + 4 * k) - F32(car + 0x520 + 4 * k)), f = (float)((ld)F32(car + 0x52c + 4 * k) - F32(car + 0x514 + 4 * k));
        B[k] = (float)((ld)(float)((ld)f + b) * 0.5f);
    }
    F32(car + 0x4e8) = s2r_ground_angle(A[0], A[2]);
    float a = -F32(car + 0x4e8);
    float sn = (float)sinl((ld)a), cs = (float)cosl((ld)a);
    const float az = (float)((ld)A[2] * cs + (ld)A[0] * sn);
    F32(car + 0x4e4) = s2r_ground_angle(A[1], az);
    a = -F32(car + 0x4e8);
    sn = (float)sinl((ld)a); cs = (float)cosl((ld)a);
    const float bx = (float)((ld)B[0] * cs - (ld)B[2] * sn), bz = (float)((ld)B[2] * cs + (ld)B[0] * sn);
    a = -F32(car + 0x4e4);
    sn = (float)sinl((ld)a); cs = (float)cosl((ld)a);
    const float by = (float)((ld)B[1] * cs + (ld)bz * sn);
    F32(car + 0x4ec) = -s2r_ground_angle(by, bx);
    F32(car + 0x4e4) = (float)FUN_00014dc0(F32(car + 0x4e4), 0.0f);
    F32(car + 0x4e8) = (float)FUN_00014dc0(F32(car + 0x4e8), 0.0f);
    F32(car + 0x4ec) = (float)FUN_00014dc0(F32(car + 0x4ec), 0.0f);
    s2r_hold_angle(car, 0x4e4);
    s2r_hold_angle(car, 0x4ec);
    float m[3];
    for (uint32_t k = 0; k < 3; k++)
        m[k] = (float)((float)((float)((float)((ld)F32(car + 0x52c + 4 * k) + F32(car + 0x514 + 4 * k)) + (ld)F32(car + 0x538 + 4 * k)) + (ld)F32(car + 0x520 + 4 * k)) * 0.25f);
    I32(car + 0x4d8) = (int32_t)truncl((ld)m[0]);
    I32(car + 0x4e0) = (int32_t)truncl((ld)m[2]);
    for (uint32_t i = 0; i < 4; i++) F32(car + 0x518 + 0xc * i) = (float)I32(car + 0x5a4 + 0x70 * i);
    F32(car + 0xc14) = (float)atanl((ld)(float)(I32(car + 0x5b8) - I32(car + 0x698)) / I32(car + 0x4c8));
    F32(car + 0xc0c) = (float)atanl((ld)(float)(I32(car + 0x698) - I32(car + 0x708)) / I32(car + 0x4d0));
}

/*
 * THE SPARKS FLYING (0x2cc74): each spark in the air (+0x34; 60 of 0x3c bytes from 0x125dd4) falls
 * (7848 a second squared on +0x1c), moves by its speed (+0x18..) a step, spins against its speed
 * (0.0008, 0.0004, 0.00015 of it on +0xc..), and its life (+0x24) runs down by the step (0x133354):
 * gone, it leaves the drawing list (0x4c01c).  One drawn (+0x30) is put in its drawing record at
 * its place and angles, and into the list (0x4bfbe).  (0x11cdcc / 0x11cdd4: its x, z, as the game
 * leaves them.)
 */
#define HOOK_2cc74
static void FUN_0002cc74(void)
{
    typedef long double ld;
    for (uint32_t sp = 0x125dd4u; sp < 0x126be4u; sp += 0x3c) {
        if (I32(sp + 0x34)) {
            const float dt = F32(0x133350);
            F32(sp + 0x1c) = (float)(double)((double)F32(sp + 0x1c) - (ld)dt * (double)7848.0);
            F32(sp) = (float)((ld)F32(sp + 0x18) * dt + F32(sp));
            F32(sp + 8) = (float)((ld)F32(sp + 0x20) * dt + F32(sp + 8));
            F32(sp + 4) = (float)((ld)F32(sp + 0x1c) * dt + F32(sp + 4));
            F32(sp + 0xc) = (float)(double)((double)F32(sp + 0xc) - (ld)F32(sp + 0x18) * dt * (double)0.0008);
            F32(sp + 0x10) = (float)(double)((double)F32(sp + 0x10) - (ld)F32(sp + 0x1c) * dt * (double)0.0004);
            F32(sp + 0x14) = (float)(double)((double)F32(sp + 0x14) - (ld)F32(sp + 0x20) * dt * (double)0.00015);
            I32(0x11cdcc) = (int32_t)truncl((ld)F32(sp));
            I32(0x11cdd4) = (int32_t)truncl((ld)F32(sp + 8));
            F32(sp + 0x24) = (float)((ld)F32(sp + 0x24) - F32(0x133354));
            if (!(0.0f < F32(sp + 0x24))) {
                FUN_0004c01c(0x11da14, 0x11d6f4, (int)(sp + 0x28));
                I32(sp + 0x30) = 0;
                I32(sp + 0x34) = I32(sp + 0x30);
            }
        }
        if (I32(sp + 0x30)) {
            const uint32_t rec = U32(0x11da20u + 0x1cu * (uint32_t)I32(sp + 0x28));
            I32(rec + 8) = (int32_t)truncl((ld)F32(sp));
            I32(rec + 0xc) = (int32_t)truncl((ld)F32(sp + 4));
            I32(rec + 0x10) = (int32_t)truncl((ld)F32(sp + 8));
            for (uint32_t k = 0; k < 3; k++) U32(rec + 0x14 + 4 * k) = (uint32_t)(int32_t)truncl((ld)F32(sp + 0xc + 4 * k) * 1024.0f / (float)3.14159274f) & 0x7ffu;
            FUN_0004bfbe(0x11da14, 0x11d6f4, (int)(sp + 0x28));
        }
    }
}

/*
 * THE ROAD MEASURED (0x309b0): a fresh place on the path (0x1099a from nothing) stepped along it 100
 * at a time (0x2d59c); each node passed takes the distance so far (its record's +0x34), until the
 * road comes round to node 0 again.
 */
#define HOOK_309b0
static void FUN_000309b0(int path)
{
    const uint32_t sp0 = s2_sp, state = s2_push(0x78);
    FUN_0001099a(path, (int)state, 0, 0);
    float dist = 0.0f;
    int prev;
    do {
        prev = I32(state + 4);
        FUN_0002d59c(path, (int)state, 0, 0, 100.0f);
        dist = (float)((long double)dist + 100.0f);
        if (prev != I32(state + 4)) I32(U32(path) + 0x38u * (uint32_t)prev + 0x34u) = (int32_t)truncl((long double)dist);
    } while (prev == 0 || I32(state + 4) != 0);
    s2_sp = sp0;
}

/*
 * THE RACE FRAME (0x2baa4).  The lights and the ghost's frame (0x4a455), the network's (none here),
 * the clocks (0x4af7d), the computers' skills (0x2da06); after 3 s of race (0x13335c) the cameras
 * may move (0x11f0a8 bit 0x20).  Every car: the tyres (0x1b36f), the walls and ground (0x16824), the
 * step (0x1b92b); the solver of touching cars (0x245a3) gives the share of the step at which two
 * cars first touch (2: none) -- then the rest of the step (at least 0.005) is run again, once, for
 * the cars it flags (+0x4c bit 8).  Then which cars touch (0x22d35),
 * and every car's look (0x1684e), picture (0x16878) and drive (0x167e2).
 */
#define HOOK_2baa4
static void FUN_0002baa4(void)
{
    FUN_0004a455();
    if (!(U8(0x130d10) & 0x20)) { if (U8(0x130d10) & 0x10) FUN_00031b00(0x14, 0x7531u); }
    else FUN_00031c8c(8, 0x7531u);
    FUN_0004af7d();
    FUN_0002da06(0, 0);
    const float step = F32(0x133350);
    if (!(U8(0x11f0a8) & 0x20) && I32(0x13335c) > 0x40400000) U8(0x11f0a8) |= 0x20;
    I32(0x11cdb4) = 0;
    const int n = I32(0x130c48);
#define CAR(i) ((int)(0x119300u + 0xcc0u * (uint32_t)(i)))
    for (int i = 0; i < n; i++) U8(CAR(i) + 0x4c) &= 0xf7;
    float done;
    do {
        if (I32(0x11cdb4) == 0) {
            for (int i = 0; i < n; i++) FUN_0001b36f(CAR(i));
            for (int i = 0; i < n; i++) FUN_00016824(CAR(i));
            for (int i = 0; i < n; i++) FUN_0001b92b(CAR(i));
        } else
            for (int i = 0; i < n; i++)
                if (U8(CAR(i) + 0x4c) & 8) { FUN_0001b36f(CAR(i)); FUN_00016824(CAR(i)); FUN_0001b92b(CAR(i)); }
        if (I32(0x11cdb4) < 1) {
            done = (float)FUN_000245a3();
            if (s2_bits(done) < 0x40000000) {
                I32(0x11cdb4) += 1;
                F32(0x133350) = (float)((1.0L - done) * F32(0x133350));
                if (!(F32(0x133350) > (double)0.005)) done = 2.0f;
                else { FUN_0004b016(); for (int i = 0; i < n; i++) FUN_00016824(CAR(i)); }
            }
        } else done = 2.0f;
    } while (s2_bits(done) < 0x40000000);
    FUN_00022d35();
    F32(0x133350) = step;
    if (I32(0x11cdb4) != 0) FUN_0004b016();
    for (int i = 0; i < n; i++) FUN_0001684e(CAR(i));
    for (int i = 0; i < n; i++) FUN_00016878(CAR(i), 1.0f);
    for (int i = 0; i < n; i++) FUN_000167e2(CAR(i));
#undef CAR
}

/*
 * A STEP ALONG A PATH (0x2d59c): the place on it (the state: +0 the path, +4 the node, +8 the share
 * of the half-node, +0xc which half) moves on by the distance given -- by the share it makes of the
 * curve's length there (0x108f5); past a half-node's end it steps to the next half (or the next
 * node, +0x10 of its record) and what is left of the distance goes on from there, less the way from
 * the last point to the new one.  Then the point (0x1099a: +0x10.. whole numbers), the last way
 * (+0x28 / +0x30 / +0x38 from +0x1c / +0x24 / +0x34), the new way (+0x1c / +0x24 = +0x6c / +0x70)
 * and its angle (+0x34: from the arc cosine, a quarter turn on, within one turn).
 */
#define HOOK_2d59c
static void FUN_0002d59c(int path, int state, int a, int b, float by)
{
    typedef long double ld;
    for (;;) {
        const double len = FUN_000108f5(state + 0x3c, state, a, b, F32(state + 8));
        F32(state + 8) = (float)((ld)by / len + F32(state + 8));
        if (I32(state + 8) < 0x3f800000) break;
        I32(state + 8) = 0;
        if (I32(state + 0xc) == 0) I32(state + 0xc) += 1;
        else { I32(state + 0xc) = 0; I32(state + 4) = I32(U32((uint32_t)path + 0x24u * (uint32_t)I32(state)) + 0x38u * (uint32_t)I32(state + 4) + 0x10u); }
        FUN_0001099a(path, state, 0, 0);
        const double d = FUN_00011253((double)(((ld)F32(state + 0x60) - I32(state + 0x10)) * ((ld)F32(state + 0x60) - I32(state + 0x10))
                                               + ((ld)F32(state + 0x64) - I32(state + 0x18)) * ((ld)F32(state + 0x64) - I32(state + 0x18))));
        by = (float)(double)((double)by - (ld)d);
        I32(state + 0x10) = (int32_t)truncl((ld)F32(state + 0x60));
        I32(state + 0x18) = (int32_t)truncl((ld)F32(state + 0x64));
    }
    FUN_0001099a(path, state, 0, 0);
    I32(state + 0x10) = (int32_t)truncl((ld)F32(state + 0x60));
    I32(state + 0x14) = (int32_t)truncl((ld)F32(state + 0x68));
    I32(state + 0x18) = (int32_t)truncl((ld)F32(state + 0x64));
    F32(state + 0x28) = F32(state + 0x1c);
    F32(state + 0x30) = F32(state + 0x24);
    F32(state + 0x1c) = F32(state + 0x6c);
    F32(state + 0x24) = F32(state + 0x70);
    F32(state + 0x38) = F32(state + 0x34);
    F32(state + 0x34) = (float)fabsl((ld)s2_acos((double)F32(state + 0x1c)));
    if (0.0f >= F32(state + 0x24)) F32(state + 0x34) = (float)((ld)(float)7.85398197f - F32(state + 0x34));
    else F32(state + 0x34) = (float)((ld)F32(state + 0x34) + (float)1.57079637f);
    while (0.0f > F32(state + 0x34)) F32(state + 0x34) = (float)((ld)F32(state + 0x34) + (float)6.28318548f);
    while (I32(state + 0x34) >= 0x40c90fdb) F32(state + 0x34) = (float)((ld)F32(state + 0x34) + -(float)6.28318548f);
}

/*
 * THE TIME ATTACK FRAME (0x2c132): the clocks (0x4af7d), the skills (0x2da06), the player's controls
 * kept (0x4a59a).  The ghost (car 0x11f0cc) runs on its own clocks (0x133364, 0x11f0ac) up to this
 * frame's race clock -- each of its frames read from the recording (0x4a488), stepped like a race
 * frame for it alone (tyres, walls, step, drive) -- while there is a recording (0x1330b8).  Then the
 * race's clocks put back, the cameras may move after 3 s; the ghost drawn or not (0x11f0a4: its
 * two drawing records); the player's car steps; every car's look; the player's picture, and the
 * ghost's at the share of its last frame the race clock has reached.
 */
#define HOOK_2c132
static void FUN_0002c132(void)
{
    typedef long double ld;
    FUN_0004af7d();
    FUN_0002da06(0, 0);
    FUN_0004a59a();
    const float c60 = F32(0x133360), c5c = F32(0x13335c), c54 = F32(0x133354), c50 = F32(0x133350);
    const uint32_t flags = U32(0x11f0a8);
#define CAR(i) ((int)(0x119300u + 0xcc0u * (uint32_t)(i)))
    if (I32(0x1330b8) != 0) {
        F32(0x13335c) = F32(0x133364);
        U32(0x11f0a8) = U32(0x11f0ac);
        while (F32(0x13335c) < c5c && I32(0x1330b8) != 0) {
            FUN_0004a488();
            F32(0x13335c) = (float)((ld)F32(0x133354) + F32(0x13335c));
            F32(0x133360) = (float)((ld)F32(0x133350) + F32(0x133360));
            F32(0x13336c) = F32(0x133350);
            F32(0x133370) = F32(0x133354);
            FUN_0004b016();
            if (!(U32(0x11f0a8) & 0x20) && I32(0x13335c) > 0x40400000) U32(0x11f0a8) |= 0x20;
            FUN_0001b36f(CAR(I32(0x11f0cc)));
            FUN_00016824(CAR(I32(0x11f0cc)));
            FUN_0001b92b(CAR(I32(0x11f0cc)));
            FUN_000167e2(CAR(I32(0x11f0cc)));
        }
        F32(0x133364) = F32(0x13335c);
        U32(0x11f0ac) = U32(0x11f0a8);
    }
    F32(0x133350) = c50; F32(0x133354) = c54; F32(0x13335c) = c5c; F32(0x133360) = c60;
    U32(0x11f0a8) = flags;
    FUN_0004b016();
    if (!(U32(0x11f0a8) & 0x20) && I32(0x13335c) > 0x40400000) U32(0x11f0a8) |= 0x20;
    const int ghost = (int)(0x119ef0u + 0xcc0u * (uint32_t)I32(0x11f0cc));
    if (I32(0x11f0a4) == 0) { FUN_0004c01c(0x11da14, 0x11d6f4, ghost); FUN_0004c01c(0x11da14, 0x11d6f4, ghost + 8); }
    else { FUN_0004bfbe(0x11da14, 0x11d6f4, ghost); FUN_0004bfbe(0x11da14, 0x11d6f4, ghost + 8); }
    FUN_0001b36f(CAR(I32(0x11f0d0)));
    FUN_00016824(CAR(I32(0x11f0d0)));
    FUN_0001b92b(CAR(I32(0x11f0d0)));
    FUN_000167e2(CAR(I32(0x11f0d0)));
    for (int i = 0; i < I32(0x130c48); i++) FUN_0001684e(CAR(i));
    FUN_00016878(CAR(I32(0x11f0d0)), 1.0f);
    const float share = !(F32(0x13335c) > F32(0x133364)) ? (float)(((ld)F32(0x13335c) - ((ld)F32(0x133364) - F32(0x133370))) / F32(0x133370)) : 1.0f;
    FUN_00016878(CAR(I32(0x11f0cc)), share);
#undef CAR
}

/*
 * PLACES AND THE COMPUTERS' PACE (0x2da06).  Each car still racing (+0x4c bit 2 clear) is placed
 * (+0xc30) by how many cars are no further along (+0x4b8) or out, ties broken by order.  In a plain
 * race (mode bits 0x70 clear), in each first second of eight: the players' mean distance (+0x4bc of
 * the cars with controls, +0xb4); with no players every car gets the level's pace (0x11f100 by the
 * level 0x130d4c); else after 6 s each computer's pace (+0x48) is read off the level's table
 * (0x11f0f4, seven entries from 0.01 to 5.99, between two by the share) at 3 + 3 x (its distance -
 * the mean) / 200000 -- behind, it goes faster; never below nought.
 */
#define HOOK_2da06
static uint64_t FUN_0002da06(uint32_t a, uint32_t b)
{
    typedef long double ld;
    (void)a; (void)b;
    const int n = I32(0x130c48);
#define CAR(i) (0x119300u + 0xcc0u * (uint32_t)(i))
    for (int i = 0; i < n; i++) {
        if (U8(CAR(i) + 0x4c) & 2) continue;
        I32(CAR(i) + 0xc30) = 0;
        for (int j = 0; j < n; j++)
            if (!(F32(CAR(i) + 0x4b8) > F32(CAR(j) + 0x4b8)) || (U8(CAR(j) + 0x4c) & 2)) I32(CAR(i) + 0xc30) += 1;
        for (int j = 0; j < i; j++)
            if (I32(CAR(i) + 0xc30) == I32(CAR(j) + 0xc30)) I32(CAR(i) + 0xc30) += 1;
    }
    if ((U8(0x130d10) & 0x70) == 0 && (int32_t)truncl((ld)F32(0x13335c)) % 8 == 0) {
        float sum = 0.0f;
        int players = 0;
        for (int i = 0; i < n; i++) if (U32(CAR(i) + 0xb4)) { sum = (float)((ld)sum + F32(CAR(i) + 0x4bc)); players++; }
        const uint32_t row = 0x1cu * (uint32_t)I32(0x130d4c);
        if (players < 1) { for (int i = 0; i < n; i++) F32(CAR(i) + 0x48) = F32(0x11f100u + row); }
        else {
            const float mean = (float)((ld)sum / players);
            for (int i = 0; i < n; i++) {
                if (U32(CAR(i) + 0xb4)) continue;
                if (I32(0x13335c) < 0x40c00000) { I32(CAR(i) + 0x48) = 0; continue; }
                const ld x = ((ld)F32(CAR(i) + 0x4bc) - mean) * 3.0f / (double)200000.0 + (double)3.0;
                const double q = !(x > (double)0.01) ? (double)0.01 : x >= (double)5.99 ? (double)5.99 : (double)x;
                float f = (float)q;
                const int k = (int32_t)truncl((ld)f);
                f = (float)((ld)f - k);
                const ld v = (1.0L - f) * F32(0x11f0f4u + row + 4u * (uint32_t)k) + (ld)F32(0x11f0f8u + row + 4u * (uint32_t)k) * f;
                F32(CAR(i) + 0x48) = !(0.0L > v) ? (float)v : 0.0f;
            }
        }
    }
#undef CAR
    return 0;
}

/*
 * THE CONES PUT OUT (0x30b29), when the track has them (0x12465c; the layout 0x124658: +0x1c how
 * many, +0 their records of 0x38 bytes: +4 / +8 the place, +0xc the kind).  Each (0x124664.., 0x3c
 * bytes, the count at 0x126d4c): its model made (0x4b843, when asked: the first word; a failure is
 * given back), placed on the ground (0x28ce6: the height; 0x2a371: the nearest wall's way +0x10),
 * and leaned with the ground: the ground's slope (0x4cba0..) turned by the wall's way less a
 * quarter turn gives its two angles (+0xc and +0x14, each a quarter turn on); standing (+0x30), not
 * hit (+0x34, +0x38).
 */
#define HOOK_30b29
static uint64_t FUN_00030b29(int models, uint32_t edx)
{
    typedef long double ld;
    if (I32(0x12465c) == 0) { I32(0x126d4c) = 0; return (uint64_t)edx << 32; }
    const uint32_t layout = U32(0x124658);
    I32(0x126d4c) = I32(layout + 0x1c);
    const uint32_t sp0 = s2_sp, in = s2_push(24), out = in + 12;
    for (int i = 0; i < I32(layout + 0x1c); i++) {
        const uint32_t cone = 0x124664u + 0x3cu * (uint32_t)i, rec = U32(layout) + 0x38u * (uint32_t)i;
        if (models) {
            const int r = FUN_0004b843(0x11da14, 0x126be4u + 0x24u * (uint32_t)I32(rec + 0xc), cone + 0x28);
            if (r != 0) { s2_sp = sp0; return (uint64_t)edx << 32 | (uint32_t)r; }
        }
        F32(cone) = (float)I32(rec + 4);
        I32(0x11cdcc) = (int32_t)truncl((ld)F32(cone));
        F32(cone + 8) = (float)I32(rec + 8);
        I32(0x11cdd4) = (int32_t)truncl((ld)F32(cone + 8));
        FUN_00028ce6(0, 0x38u * (uint32_t)i);
        F32(cone + 4) = (float)I32(0x4cb98);
        FUN_0002a371(0, 0);
        F32(cone + 0x10) = F32(0x11cddc);
        I32(in) = I32(0x4cba0); I32(in + 4) = I32(0x4cba4); I32(in + 8) = I32(0x4cba8);
        FUN_00010388((int)out, (int)in, 0, 0, (float)((ld)F32(cone + 0x10) + -(float)1.57079637f));
        const float x = (float)I32(out), y = (float)I32(out + 4), z = (float)I32(out + 8);
        F32(cone + 0xc) = (float)((ld)s2r_ground_angle(y, z) + (float)1.57079637f);
        F32(cone + 0x14) = (float)((ld)s2r_ground_angle(y, x) + (float)1.57079637f);
        I32(cone + 0x30) = 1;
        I32(cone + 0x34) = 0;
        I32(cone + 0x38) = I32(cone + 0x34);
    }
    s2_sp = sp0;
    return (uint64_t)edx << 32;
}

/*
 * THE CONTROLS READ (0x31888): six records of 0x1c bytes (+0 in use, +4 a stick's axis or a button,
 * +8 the kind: 1 a key, 0 / 2 the stick, +0xc the axis, +0x10 the stick, +0x18 the key or button's
 * bit) give six words after them (+0xa8.., 0..0x3ff).  A key: down in the key table (0x100002).  A
 * button: its bit in the stick's buttons (0x4cd69).  An axis read before by another record is the
 * same word; else the stick's reading (0x4cd77, 12 bits, over the calibration's step +0xc, from its
 * middle +4) times the calibration's scale (+0x14 one way, +0x18 the other), held to +-0x3ff -- kind
 * 2 is all or nothing past half -- with no movement within 0x66 of the middle.
 */
#define HOOK_31888
static void FUN_00031888(int rec)
{
    for (uint32_t r = (uint32_t)rec, out = (uint32_t)rec + 0xa8u; r < (uint32_t)rec + 0xa8u; r += 0x1c, out += 4) {
        if (I32(r) != 1) continue;
        if (I32(r + 8) != 0 && I32(r + 8) != 2) { I32(out) = U8(0x100002u + U32(r + 0x18)) ? 0x3ff : 0; continue; }
        const int stick = I32(r + 0x10), axis = I32(r + 0xc);
        if (I32(r + 4) != 1) { I32(out) = ((uint32_t)FUN_0004cd69(I32(0x127e68u + 4u * (uint32_t)axis)) & U32(r + 0x18)) ? 0x3ff : 0; continue; }
        int found = 0;
        for (uint32_t q = (uint32_t)rec, qo = (uint32_t)rec + 0xa8u; q < r; q += 0x1c, qo += 4)
            if (I32(q) == 1 && I32(q + 4) == 1 && (I32(q + 8) == 0 || I32(q + 8) == 2) && I32(q + 0x10) == stick && I32(q + 0xc) == axis) {
                I32(out) = I32(qo); found = 1; break;
            }
        if (found) continue;
        const uint32_t cal = 0x127a08u + 0x118u * (uint32_t)stick + 0x1cu * (uint32_t)axis;
        const int32_t raw = (int32_t)((uint32_t)FUN_0004cd77(1 << stick, I32(cal + 0x10)) & 0xfffu);
        const int32_t v = raw / I32(cal + 0xc), d = v - I32(cal + 4);
        const float k = d > 0 ? F32(cal + 0x14) : F32(cal + 0x18);
        int32_t o = (int32_t)truncl((long double)(float)((long double)d * k));
        if (o > 0x3ff) o = 0x3ff;
        if (I32(r + 8) == 2 && o > 0x1ff) o = 0x3ff;
        if (o < -0x3ff) o = -0x3ff;
        if (I32(r + 8) == 2 && o < -0x1ff) o = -0x3ff;
        if (s2_abs(o) < 0x66) o = 0;
        I32(out) = o;
    }
}

/*
 * THE CONES FLYING (0x2c475): a cone hit (+0x34) is drawn again (+0x30), falls (7848), moves and
 * spins like a spark.  Off the ground (0x28ce6) it meets the nearest wall (0x2a371) if there is one:
 * put back out of it (into the wall's frame, its across part nought, back), its speed turned about
 * the wall by twice the angle between the wall and its way (that angle kept within a quarter turn)
 * and slowed to half its cosine across, half up; still off the ground, or with no wall, the ground
 * is at its own height.  At or below the ground it stands on it, its speed x 0.7 and the up speed
 * turned upwards; under 300 it stops, and if it is on the ground it leans with it as when it was
 * put out.  A drawn cone's record takes its place and angles, and goes into the drawing list.
 */
#define HOOK_2c475
static void FUN_0002c475(void)
{
    typedef long double ld;
    const uint32_t end = 0x124664u + 0x3cu * (uint32_t)I32(0x126d4c), sp0 = s2_sp, v = s2_push(36), o1 = v + 12, o2 = v + 24;
    for (uint32_t c = 0x124664u; c < end; c += 0x3c) {
        if (I32(c + 0x34)) {
            I32(c + 0x30) = 1;
            const float dt = F32(0x133350);
            F32(c + 0x1c) = (float)(double)((double)F32(c + 0x1c) - (ld)dt * (double)7848.0);
            F32(c) = (float)((ld)F32(c + 0x18) * dt + F32(c));
            F32(c + 8) = (float)((ld)F32(c + 0x20) * dt + F32(c + 8));
            F32(c + 4) = (float)((ld)F32(c + 0x1c) * dt + F32(c + 4));
            F32(c + 0xc) = (float)(double)((double)F32(c + 0xc) - (ld)F32(c + 0x18) * dt * (double)0.0008);
            F32(c + 0x10) = (float)(double)((double)F32(c + 0x10) - (ld)F32(c + 0x1c) * dt * (double)0.0004);
            F32(c + 0x14) = (float)(double)((double)F32(c + 0x14) - (ld)F32(c + 0x20) * dt * (double)0.00015);
            I32(0x11cdcc) = (int32_t)truncl((ld)F32(c));
            I32(0x11cdd4) = (int32_t)truncl((ld)F32(c + 8));
            int off = (uint32_t)FUN_00028ce6(0, 0) == 0;
            if (off) {
                if ((uint32_t)FUN_0002a371(1, 0) == 0) I32(0x4cb98) = (int32_t)truncl((ld)F32(c + 4));
                else {
                    const uint32_t wall = U32(0x11cdd8);
                    float a = (float)FUN_00014dc0((float)3.14159274f, F32(wall));
                    I32(v) = I32(0x11cdcc) - I32(wall + 4);
                    I32(v + 8) = I32(0x11cdd4) - I32(wall + 8);
                    FUN_00010388((int)o1, (int)v, 0, 0, a);
                    I32(o1) = 0;
                    FUN_00010388((int)o2, (int)o1, 0, 0, -a);
                    F32(c) = (float)((ld)(I32(o2) - I32(v)) + F32(c));
                    I32(0x11cdcc) = (int32_t)truncl((ld)F32(c));
                    F32(c + 8) = (float)((ld)(I32(o2 + 8) - I32(v + 8)) + F32(c + 8));
                    I32(0x11cdd4) = (int32_t)truncl((ld)F32(c + 8));
                    off = (uint32_t)FUN_00028ce6(0, 0) == 0;
                    if (off) I32(0x4cb98) = (int32_t)truncl((ld)F32(c + 4));
                    a = (float)FUN_00014dc0(a, s2r_ground_angle(F32(c + 0x18), F32(c + 0x20)));
                    if (fabsf(a) > (double)1.570796370506286) a = (float)FUN_00014dc0(a, (float)3.14159274f);
                    const float b = (float)((ld)a * 2.0f);
                    const float sn = (float)sinl((ld)b), cs = (float)cosl((ld)b);
                    const float x = (float)((ld)F32(c + 0x18) * cs - (ld)F32(c + 0x20) * sn), z = (float)((ld)F32(c + 0x20) * cs + (ld)F32(c + 0x18) * sn);
                    F32(c + 0x18) = x; F32(c + 0x20) = z;
                    const float k = (float)(cosl((ld)a) * (double)0.5);
                    F32(c + 0x18) = (float)((ld)F32(c + 0x18) * k);
                    F32(c + 0x20) = (float)((ld)F32(c + 0x20) * k);
                    F32(c + 0x1c) = (float)(double)((double)F32(c + 0x1c) * (double)0.5);
                }
            }
            if ((ld)I32(0x4cb98) >= F32(c + 4)) {
                F32(c + 4) = (float)I32(0x4cb98);
                F32(c + 0x18) = (float)(double)((double)F32(c + 0x18) * (double)0.7);
                F32(c + 0x20) = (float)(double)((double)F32(c + 0x20) * (double)0.7);
                F32(c + 0x1c) = (float)fabsl((ld)F32(c + 0x1c) * (double)0.7);
                const float len = (float)sqrt((double)((ld)F32(c + 0x1c) * F32(c + 0x1c) + (ld)F32(c + 0x18) * F32(c + 0x18) + (ld)F32(c + 0x20) * F32(c + 0x20)));
                if (len < 300.0f) {
                    I32(c + 0x34) = 0;
                    if (!off) {
                        I32(v) = I32(0x4cba0); I32(v + 4) = I32(0x4cba4); I32(v + 8) = I32(0x4cba8);
                        FUN_00010388((int)o1, (int)v, 0, 0, (float)((ld)F32(c + 0x10) + -(float)1.57079637f));
                        const float x = (float)I32(o1), y = (float)I32(o1 + 4), z = (float)I32(o1 + 8);
                        F32(c + 0xc) = (float)((ld)s2r_ground_angle(y, z) + (float)1.57079637f);
                        F32(c + 0x14) = (float)((ld)s2r_ground_angle(y, x) + (float)1.57079637f);
                    }
                }
            }
        }
        if (I32(c + 0x30)) {
            I32(c + 0x30) = 0;
            const uint32_t rec = U32(0x11da20u + 0x1cu * (uint32_t)I32(c + 0x28));
            I32(rec + 8) = (int32_t)truncl((ld)F32(c));
            I32(rec + 0xc) = (int32_t)truncl((ld)F32(c + 4));
            I32(rec + 0x10) = (int32_t)truncl((ld)F32(c + 8));
            for (uint32_t k = 0; k < 3; k++) U32(rec + 0x14 + 4 * k) = (uint32_t)(int32_t)truncl((ld)F32(c + 0xc + 4 * k) * 1024.0f / (float)3.14159274f) & 0x7ffu;
            FUN_0004bfbe(0x11da14, 0x11d6f4, (int)(c + 0x28));
        }
    }
    s2_sp = sp0;
}

/*
 * SMALL ONES: the file group of the race's texts let go (0x326be); a sprite of the race screen
 * (0x12420c, its sizes at 0x124210, 0x14 bytes each: +0 width, +8 the other) drawn in the middle of
 * the 320-wide screen (0x333c1) or in the middle of a box (0x3343d: from x, w wide); a point eased
 * towards another by a share from the table at 0x1240c0 (0x33635) or by one given (0x3385b) --
 * whole numbers, the move a float sum, cut.
 */
#define HOOK_326be
static void FUN_000326be(void) { FUN_0004f3c8(U32(0x11d6ec), 0x110ecdu); }
#define HOOK_333c1
static void FUN_000333c1(int n, uint32_t y)
{
    const uint32_t rec = U32(0x124210) + 0x14u * (uint32_t)n;
    const int32_t w = I32(rec + 8) + I32(rec), half = (I32(rec) - (I32(rec) >> 31)) >> 1, mid = (0x140 - w) >> 1;
    FUN_00050c0a(0x12420c, mid - half, (int)y, n, 0);
}
#define HOOK_3343d
static void FUN_0003343d(int n, uint32_t y, int w, int x)
{
    FUN_00050c0a(0x12420c, x + ((w - I32(U32(0x124210) + 0x14u * (uint32_t)n + 8)) >> 1), (int)y, n, 0);
}
#define HOOK_33635
static void FUN_00033635(int p, int to, int k)
{
    const float share = F32(U32(0x1240c0) + 4u * (uint32_t)k);
    for (uint32_t c = 0; c < 3; c++)
        I32(p + 4 * c) = (int32_t)truncl((long double)(I32(p + 4 * c) - I32(to + 4 * c)) * share + (long double)I32(to + 4 * c));
}
#define HOOK_3385b
static void FUN_0003385b(int out, int a, int b, uint32_t ecx, float share)
{
    (void)ecx;
    for (uint32_t c = 0; c < 3; c++)
        I32(out + 4 * c) = (int32_t)truncl((long double)(I32(a + 4 * c) - I32(b + 4 * c)) * share + (long double)I32(b + 4 * c));
}

/*
 * THE SETTINGS SAVED (0x331dc): CHOICE.DAT (its path made from 0x110fa1 and the game's folder
 * 0x11eff8, opened as 0x110fb6 says) gets the options block (0x1289ec, 0x2077 words), each word
 * (w + 0x1050c x i) ^ i.  And text on the race screen's font (0x12420c): one line in the middle of
 * the screen (0x3327c: text, y, font, colour, gap), in the middle of a box (0x332dd: text, y, box
 * width, x, font, colour; gap 2); the widest of n of the game's strings (0x3348f: 0x127ed8 from the
 * first, gap 2).
 */
#define HOOK_331dc
static void FUN_000331dc(void)
{
    const uint32_t sp0 = s2_sp, buf = s2_push(0x40 + 8), word = buf + 0x40;
    FUN_0004eaed(buf, 0x110fa1u, 0x11eff8u);
    const uint32_t f = FUN_000b6aac(buf, 0x110fb6u);
    if (f) {
        for (uint32_t i = 0; i < 0x2077u; i++) {
            U32(word) = (U32(0x1289ecu + 4u * i) + i * 0x1050cu) ^ i;
            FUN_000b6e97(word, 4, 1, f);
        }
        FUN_000b6d3b(f);
    }
    s2_sp = sp0;
}
#define HOOK_3327c
static void FUN_0003327c(uint32_t text, uint32_t y, uint32_t font, uint32_t colour, uint32_t gap)
{
    const uint32_t sp0 = s2_sp, w = s2_push(8), h = w + 4;
    FUN_00051020(0x12420c, w, h, text, (int)font, (int)gap);
    FUN_00050f45(0x12420c, (0x140 - I32(w)) >> 1, (int)y, text, (int)font, (int)colour, (int)gap);
    s2_sp = sp0;
}
#define HOOK_332dd
static void FUN_000332dd(uint32_t text, uint32_t y, int box, int x, uint32_t font, uint32_t colour)
{
    const uint32_t sp0 = s2_sp, w = s2_push(8), h = w + 4;
    FUN_00051020(0x12420c, w, h, text, (int)font, 2);
    FUN_00050f45(0x12420c, x + ((box - I32(w)) >> 1), (int)y, text, (int)font, (int)colour, 2);
    s2_sp = sp0;
}
#define HOOK_3348f
static int FUN_0003348f(int first, int n, uint32_t font)
{
    const uint32_t sp0 = s2_sp, w = s2_push(8), h = w + 4;
    int most = 0;
    for (int i = 0; i < n; i++) {
        FUN_00051020(0x12420c, w, h, U32(0x127ed8u + 4u * (uint32_t)(first + i)), (int)font, 2);
        if (most < I32(w)) most = I32(w);
    }
    s2_sp = sp0;
    return most;
}

/*
 * THE MENU'S CAMERA PATHS READ (0x32c48): from the container (its name made from 0x110f66 and the
 * game's folder) the file named, for each of the paths asked (0x16c bytes each at 0x131f34): how many
 * points, then that many points and as many aims (12 bytes each, +4 and +0xb8).  A point list turned
 * round by one (0x336eb): forwards (first word 0: each takes the next, the first goes last) or back.
 */
#define HOOK_32c48
static void FUN_00032c48(int count, int name)
{
    const uint32_t sp0 = s2_sp, buf = s2_push(0x90 + 0xb8), file = buf + 0x90;
    FUN_0004eaed(buf, 0x110f66u, 0x11eff8u);
    if (FUN_0004eba5(file, buf, (uint32_t)name, 0x110f76u) != 0) FUN_0004eb34(file);
    for (int i = 0; i < count; i++) {
        const uint32_t path = 0x131f34u + 0x16cu * (uint32_t)i;
        FUN_0004ee3a(file, path, 4);
        if (I32(path) != 0) {
            FUN_0004ee3a(file, path + 4, I32(path) * 0xc);
            FUN_0004ee3a(file, path + 0xb8, I32(path) * 0xc);
        }
    }
    FUN_0004ed79(file, 0);
    s2_sp = sp0;
}
#define HOOK_336eb
static void FUN_000336eb(int back, int list, int stride, int n)
{
    uint32_t keep[3];
    if (back == 0) {
        memcpy(keep, MP(list), 12);
        for (int i = 0; i < n - 1; i++) memmove(MP(list + i * stride), MP(list + (i + 1) * stride), 12);
        memcpy(MP(list + (n - 1) * stride), keep, 12);
    } else {
        memcpy(keep, MP(list + (n - 1) * stride), 12);
        for (int i = n - 2; i >= 0; i--) memmove(MP(list + (i + 1) * stride), MP(list + i * stride), 12);
        memcpy(MP(list), keep, 12);
    }
}

/*
 * THE SETTINGS READ (0x32d53): CHOICE.DAT (0x110f79 / 0x110f8e) into the options block (0x1289ec,
 * 0x2077 words, each (w ^ i) - 0x1050c x i).  With no file, the first values: all nought, then
 * +0x48 1, +0x40 2, three volumes 0x7fff 0x7fff 0x5fff, +0x7c 2; every best-times table (3 x 7 x 4
 * x 15 at 0x1290bc, and 3 x 7 x 15 at 0x12f32c) with no time (3599.999), the name 0x110f91 /
 * 0x110f95, no car, slot i & 3; +0x64 and +0x44 1; the two names 0x110f99 / 0x110f9d; the four
 * players' cars allowed (+0xb0..) and their set-ups 2 2 0 2 2 2.
 */
#define HOOK_32d53
static void FUN_00032d53(void)
{
    const uint32_t sp0 = s2_sp, buf = s2_push(0x50 + 8), word = buf + 0x50;
    FUN_0004eaed(buf, 0x110f79u, 0x11eff8u);
    const uint32_t f = FUN_000b6aac(buf, 0x110f8eu);
    if (f == 0) {
        FUN_000ae8b0(0x1289ecu, 0x81dcu, 0);
        U32(0x128a34) = 1; U32(0x128a2c) = 2;
        U32(0x128a60) = 0x7fff; U32(0x128a5c) = U32(0x128a60); U32(0x128a64) = 0x5fff; U32(0x128a68) = 2;
        for (uint32_t a = 0; a < 3; a++) for (uint32_t t = 0; t < 7; t++) for (uint32_t k = 0; k < 4; k++) for (uint32_t i = 0; i < 0xf; i++) {
            const uint32_t at = a * 0x20d0u + t * 0x4b0u + k * 0x12cu + i * 4u;
            U32(0x1290bc + at) = 0x4560fffcu;
            FUN_000b3be4(0x1290f8 + at, 0x110f91u);
            U32(0x129134 + at) = 0;
            U32(0x1291ac + at) = 0;
            U32(0x129170 + at) = i & 3;
        }
        for (uint32_t a = 0; a < 3; a++) for (uint32_t t = 0; t < 7; t++) for (uint32_t i = 0; i < 0xf; i++) {
            const uint32_t at = a * 0x834u + t * 0x12cu + i * 4u;
            U32(0x12f32c + at) = 0x4560fffcu;
            FUN_000b3be4(0x12f368 + at, 0x110f95u);
            U32(0x12f3a4 + at) = 0;
            U32(0x12f41c + at) = 0;
            U32(0x12f3e0 + at) = i & 3;
        }
        U32(0x128a50) = 1; U32(0x128a30) = U32(0x128a50);
        FUN_000b3be4(0x1289f4u, 0x110f99u);
        FUN_000b3be4(0x1289f8u, 0x110f9du);
        for (uint32_t p = 0; p < 4; p++) {
            U32(0x128a9c + p * 0x188u) = 1;
            for (uint32_t c = 0; c < 4; c++) for (uint32_t k = 0; k < 4; k++) {
                const uint32_t at = p * 0x188u + c * 0x60u + k * 0x18u;
                U32(0x128aa4 + at) = 2; U32(0x128aa8 + at) = 2; U32(0x128aac + at) = 0;
                U32(0x128ab0 + at) = 2; U32(0x128ab4 + at) = 2; U32(0x128ab8 + at) = 2;
            }
        }
    } else {
        for (uint32_t i = 0; i < 0x2077u; i++) {
            FUN_000b6b60(word, 4, 1, f);
            U32(0x1289ecu + 4u * i) = (U32(word) ^ i) - i * 0x1050cu;
        }
        FUN_000b6d3b(f);
    }
    s2_sp = sp0;
}

/*
 * A CAR PUT ON THE GRID (0x2f519: the car's record, the folder's name, whether to load its models,
 * its number).  The record is cleared -- the sound slots' part (+0x224, 0x284 bytes) kept, and when
 * the models are kept (no loading, not time attack) the drawing records and wheel places too.  No
 * times yet (+0xc44.., the track's best splits into +0xc58..); its number, the flags 0x80, its place
 * last; who drives it (+0xb4: the controls' record 0x127708 + 0xc0 x n for the players -- car 0, or
 * every car in time attack, car 1 too with two players -- none for the computers; in a network game
 * by the network's numbers); its data (0x11c600: model x 5 + colour, 0x58 bytes, into +0x5c).
 * Loading: the shadow ("ashadow"), the body (the model and team's file name, kept at 0x12735c /
 * 0x12755c, in the high set with 0x11f14c bit 1), its four parts ("fauto", "MV_mauto", "rauto", ...)
 * and their heights.  Into the drawing list; the wheels' blocks and places, the corners' rows; its
 * length and width from the wheels; the road's start (path 0, its first point), its height far
 * below, the road's way; the grid: two cars a row 2000 apart, 1000 to each side (time attack: only
 * to the side); then the ground, the springs settled, the speed and way, its engine sound (sample
 * by model / colour), and its set-up (0x14e20).
 */
static int s2r_grid_car(int car, uint32_t folder, int load, int n, uint32_t buf);
#define HOOK_2f519
static int FUN_0002f519(int car, uint32_t folder, int load, int n)
{
    /* the body's file name stays in the routine's frame while it runs: the library keeps a pointer to it */
    const uint32_t sp0 = s2_sp;
    const int r = s2r_grid_car(car, folder, load, n, s2_push(0x80));
    s2_sp = sp0;
    return r;
}
static int s2r_grid_car(int car, uint32_t folder, int load, int n, uint32_t buf)
{
    typedef long double ld;
    uint8_t sounds[0x284], keep[9][8];
    int32_t wheels[4][3];
    static const uint32_t kept[9] = { 0x1f4, 0x1fc, 0x204, 0xbf0, 0xbf8, 0x5c0, 0x630, 0x6a0, 0x710 };
    memcpy(sounds, MP(car + 0x224), 0x284);
    if (load == 0 && !(U8(0x130d10) & 4)) {
        for (int i = 0; i < 4; i++) { wheels[i][0] = I32(car + 0x5ac + 0x70 * i); wheels[i][2] = I32(car + 0x5b4 + 0x70 * i); wheels[i][1] = I32(car + 0x5b0 + 0x70 * i); }
        for (int k = 0; k < 9; k++) memcpy(keep[k], MP(car + kept[k]), 8);
        memset(MP(car), 0, 0xcc0);
        for (int k = 0; k < 9; k++) memcpy(MP(car + kept[k]), keep[k], 8);
    } else memset(MP(car), 0, 0xcc0);
    for (uint32_t i = 0; i < 5; i++) { U32(car + 0xc44 + 4 * i) = 0x4560fffcu; F32(car + 0xc58 + 4 * i) = F32(0x130d1cu + 4 * i); }
    U32(car + 0xc40) = 0x4560fffcu;
    memcpy(MP(car + 0x224), sounds, 0x284);
    I32(car) = n;
    I32(car + 0x4c) = 0x80;
    I32(car + 0xc30) = I32(0x130c48) - I32(car);
    U32(car + 0x990) = (uint32_t)car + 0x4d8u;
    const uint32_t mode = U32(0x130d10);
    if (mode & 0x80) U32(car + 0xb4) = 0;
    else if (!(mode & 0x30)) {
        if (I32(car) == 0 || (mode & 4) || (I32(car) == 1 && (mode & 0x40))) U32(car + 0xb4) = 0x127708u + 0xc0u * (uint32_t)I32(car);
        else U32(car + 0xb4) = 0;
    } else {
        memset(MP(car + 0xc90), 0, 0x30);
        if (I32(car) == I32(0x130d08)) { I32(car + 0xc94) = 0xd5f; U32(car + 0xb4) = 0x127708u; }
        else {
            I32(car + 0xc94) = 0;
            if (I32(car) < I32(0x130d0c)) U32(car + 0xb4) = 0x127708u + 0xc0u * (uint32_t)(I32(car) < I32(0x130d08) ? I32(car) + 1 : I32(car));
            else U32(car + 0xb4) = 0;
        }
    }
    const uint32_t entry = 0x130c58u + 0x2cu * (uint32_t)I32(car);   /* (model at +0, colour at +4) */
    memmove(MP(car + 0x5c), MP(0x11c600u + (U32(entry) * 5u + U32(entry + 4)) * 0x58u), 0x58);
    if (load != 0 || (mode & 4)) {
        FUN_0004b49f(folder, folder);
        FUN_0004feb4(0x110d59u, 1);
        int r = FUN_0004b843(0x11da14, 0x110d5eu, car + 0xbf8);
        if (r != 0) return r;
        FUN_0004eaed(buf, 0x110d66u, 0x11eff8u, U32(entry) + 0x31u, (U32(0x130c50) & 1u) + 0x30u, 0x31u);
        FUN_000b3be4(0x12735cu + 0x80u * (uint32_t)I32(car), buf);
        FUN_0004b49f(buf, buf);
        FUN_0004eaed(buf, 0x110d7cu, U32(entry + 4) + 0x31u);
        FUN_000b3be4(0x12755cu + 10u * (uint32_t)I32(car), buf);
        FUN_0004feb4(buf, (U8(0x11f14c) & 1) ? 9 : 1);
        FUN_0004bc94(0x11da14, buf, car + 0xbf0);
        r = FUN_0004b4d4(0x11da14, U32(car + 0xbf0), 1);
        if (r != 0) return r;
        U32(car + 0x204) = U32(car + 0xbf0); U32(car + 0x1fc) = U32(car + 0x204); U32(car + 0x1f4) = U32(car + 0x1fc);
        FUN_0004bc03(0x110d83u, car + 0x1f4, 0x11da14);
        FUN_0004bc03(0x110d89u, car + 0xbf0, 0x11da14);
        FUN_0004bc03(0x110d92u, car + 0x1fc, 0x11da14);
        FUN_0004bc03(0x110d98u, car + 0x204, 0x11da14);
        const uint32_t parts = U32(0x11da24u + 0x1cu * (uint32_t)I32(car + 0xbf0));
        U32(car + 0x214) = U32(parts + 0x20u * (uint32_t)I32(car + 0x1f8) + 0x18);
        U32(car + 0x218) = U32(parts + 0x20u * (uint32_t)I32(car + 0xbf4) + 0x18);
        U32(car + 0x21c) = U32(parts + 0x20u * (uint32_t)I32(car + 0x200) + 0x18);
        U32(car + 0x220) = U32(parts + 0x20u * (uint32_t)I32(car + 0x208) + 0x18);
    }
    FUN_0004bfbe(0x11da14, 0x11d6f4, car + 0xbf0);
    FUN_0004bfbe(0x11da14, 0x11d6f4, car + 0xbf8);
    I32(car + 100) = U32(car + 0xb4) == 0 ? 1 : I32(0x130c64u + 0x2cu * (uint32_t)I32(car));
    I32(car + 0x13c) = 0;
    I32(car + 0x20c) = 0;
    memset(MP(car + 0x4d8), 0, 0x25c);
    memset(MP(car + 0x734), 0, 0x25c);
    F32(car + 0x76c) = 1.0f;
    F32(car + 0x510) = F32(car + 0x76c);
    for (uint32_t i = 0; i < 4; i++) {
        const uint32_t w = (uint32_t)car + 0x70u * i;
        U32(w + 0x5c8) = U32(car + 0xbf0);
        U32(w + 0x5c0) = U32(w + 0x5c8);
        FUN_0004bc03(0x112f00u + 8u * i, (int)(w + 0x5c0), 0x11da14);
        FUN_0004bc03(0x112f04u + 8u * i, (int)(w + 0x5c8), 0x11da14);
        if (load == 0 && !(mode & 4)) { I32(w + 0x5ac) = wheels[i][0]; I32(w + 0x5b4) = wheels[i][2]; I32(w + 0x5b0) = wheels[i][1]; }
        else {
            const uint32_t part = U32(0x11da24u + 0x1cu * (uint32_t)I32(w + 0x5c0)) + 0x20u * (uint32_t)I32(w + 0x5c4);
            U32(w + 0x5ac) = U32(part + 0x14); U32(w + 0x5b0) = U32(part + 0x18); U32(w + 0x5b4) = U32(part + 0x1c);
        }
        U32(car + 0x164 + 0x20 * i) = U32(w + 0x5ac);
        I32(car + 0x16c + 0x20 * i) = I32(w + 0x5b4) + ((i & 1) == 0 ? -0xfa : 0x96);
        F32(w + 0x58c) = (float)I32(car + 0xa4);
        F32(w + 0x594) = (float)I32(car + 0xa8);
    }
    I32(car + 0x4d0) = (int32_t)truncl(fabsl((ld)(I32(car + 0x5b4) - I32(car + 0x624))));
    I32(car + 0x4c8) = (int32_t)truncl(fabsl((ld)(I32(car + 0x5ac) - I32(car + 0x68c))));
    I32(car + 0x44) = 0;
    I32(car + 0x58) = (int32_t)truncl((ld)F32(car + 0x44));
    F32(car + 0x54) = (float)I32(car + 0x58);
    I32(car + 0x4b0) = 0; I32(car + 0x4b4) = I32(car + 0x4b0); I32(car + 0x4a8) = I32(car + 0x4b4); I32(car + 0x4ac) = I32(car + 0x4a8);
    I32(car + 0x4b8) = 0; F32(car + 0x4c0) = F32(car + 0x4b8); F32(car + 0x4bc) = F32(car + 0x4c0);
    I32(car + 0x140) = 0; I32(car + 0x1f0) = 0; I32(car + 0xb8) = 0; I32(car + 0xc0) = 0;
    F32(car + 0xc34) = 1.0f;
    memset(MP(car + 0xc4), 0, 0x78);
    FUN_0001099a((int)U32(0x127350), car + 0xc4, 0, 0);
    const uint32_t start = U32(U32(0x127350));
    const float x0 = (float)I32(start + 4), z0 = (float)I32(start + 8);
    I32(car + 0x4d8) = (int32_t)truncl((ld)x0); U32(0x11cdcc) = U32(car + 0x4d8); U32(car + 0xd4) = U32(0x11cdcc);
    I32(car + 0x4e0) = (int32_t)truncl((ld)z0); U32(0x11cdd4) = U32(car + 0x4e0); U32(car + 0xdc) = U32(0x11cdd4);
    I32(car + 0x4dc) = (int32_t)0xfffe7960; I32(0x11cdd0) = I32(car + 0x4dc); I32(car + 0xd8) = I32(0x11cdd0);
    for (uint32_t i = 0; i < 4; i++) {
        const uint32_t w = (uint32_t)car + 0x70u * i;
        I32(w + 0x5a8) = 0; I32(w + 0x5a0) = I32(w + 0x5a8); I32(w + 0x5a4) = (int32_t)0xfffe7960;
        I32(w + 0x578) = 0; I32(w + 0x574) = 0; I32(w + 0x590) = 0; I32(w + 0x5bc) = 0; I32(w + 0x584) = 0; I32(w + 0x580) = 0; I32(w + 0x598) = 0;
        F32(w + 0x588) = F32(w + 0x58c);
        I32(w + 0x5d0) = 0; I32(w + 0x57c) = 1;
    }
    FUN_0002a371(0, 0);
    F32(car + 0x4c4) = F32(0x11cddc); F32(car + 0x4e8) = F32(car + 0x4c4); F32(car + 0x508) = F32(car + 0x4e8);
    I32(car + 0x4d4) = 0;
    F32(car + 0xc20) = F32(car + 0x4d4); F32(car + 0xc1c) = F32(car + 0xc20); F32(car + 0xc18) = F32(car + 0xc1c);
    F32(car + 0x4ec) = F32(car + 0xc18); F32(car + 0x4e4) = F32(car + 0x4ec); F32(car + 0x50c) = F32(car + 0x4e4); F32(car + 0x504) = F32(car + 0x50c);
    const uint32_t grid = 0x130c68u + 0x2cu * (uint32_t)n;
    if (!(mode & 0x30)) I32(grid) = I32(0x130c48) - 1 - n;
    const int k = (I32(grid) % 2) * 2 - 1;
    const ld a = F32(car + 0x4e8);
    float x, z;
    if (!(mode & 4)) {
        x = (float)(double)(((ld)k * sinl(a + (float)1.57079637f) * (double)1000.0 + (ld)I32(grid) * sinl(a + (float)3.14159274f) * (double)2000.0) + (double)x0);
        z = (float)(double)((double)z0 - ((ld)k * cosl(a + (float)1.57079637f) * (double)1000.0 + (ld)I32(grid) * cosl(a + (float)3.14159274f) * (double)2000.0));
    } else {
        x = (float)(double)((ld)k * sinl(a + (float)1.57079637f) * (double)1000.0 + (double)x0);
        z = (float)(double)((double)z0 - (ld)k * cosl(a + (float)1.57079637f) * (double)1000.0);
    }
    I32(car + 0x4d8) = (int32_t)truncl((ld)x); U32(0x11cdcc) = U32(car + 0x4d8); U32(car + 0xd4) = U32(0x11cdcc);
    I32(car + 0x4e0) = (int32_t)truncl((ld)z); U32(0x11cdd4) = U32(car + 0x4e0); U32(car + 0xdc) = U32(0x11cdd4);
    I32(car + 0x4fc) = 0; F32(car + 0x500) = F32(car + 0x4fc);
    I32(car + 0x1e8) = 0; F32(car + 0x1e4) = F32(car + 0x1e8); F32(car + 0xbc) = F32(car + 0x1e4); F32(car + 0x1ec) = F32(car + 0xbc); F32(car + 0xc78) = F32(car + 0x1ec);
    I32(car + 0xc7c) = 0; I32(car + 0xc70) = 0; I32(car + 0xc80) = 0; I32(car + 0xc8c) = 0; I32(car + 0xc88) = I32(car + 0xc8c);
    I32(car + 0x150) = 0;
    I32(car + 0xc3c) = 0; F32(car + 0xc38) = F32(car + 0xc3c);
    I32(car + 0x48) = 0;
    for (uint32_t i = 0; i < 8; i++) { I32(car + 0x230 + 0x28 * i) = 0; I32(car + 0x370 + 0x28 * i) = 0; }
    I32(car + 0x224) = 0;
    memmove(MP(car + 0x734), MP(car + 0x4d8), 0x25c);
    FUN_0001d9fc(car);
    FUN_00016824(car);
    for (uint32_t i = 0; i < 4; i++) {
        const uint32_t w = (uint32_t)car + 0x70u * i;
        I32(w + 0x574) = 0;
        I32(w + 0x5a4) = (int32_t)truncl((ld)I32(w + 0x5b8) + F32(w + 0x58c));
        F32(car + 0x518 + 0xc * i) = (float)I32(w + 0x5a4);
    }
    FUN_0001bef1(car);
    for (uint32_t i = 0; i < 4; i++) I32(car + 0x574 + 0x70 * i) = 0;
    memmove(MP(car + 0x734), MP(car + 0x4d8), 0x25c);
    FUN_0001e529(car);
    FUN_00016824(car);
    int sample = I32(entry + 4);
    if (sample == 4) sample = I32(entry) + 4;
    FUN_000aea54(U32(0x124274), sample, I32(car), 0, 1, 1);
    FUN_00014e20(car, (int)(0x130c6cu + 0x2cu * (uint32_t)I32(car)));
    return 0;
}

/*
 * SMALL MENU ONES: the screen cleared (64000 bytes) and the frame shown (0x358b9); a menu object
 * (0x361dc: its picture n, no animation -- out of the drawing list); a set-up's five numbers back to
 * the middle and the tyres to the track's (0x36cc5); the keyboard's waiting keys thrown away
 * (0x37dcf: the port keeps none); nothing (0x38462).
 */
#define HOOK_358b9
static void FUN_000358b9(void)
{
    FUN_00051aa4(U32(0x134794), 64000u, 0);
    FUN_000512f1(1.0f, 0, 0, 0);
    FUN_0007f5a2(0, 0);
}
#define HOOK_361dc
static void FUN_000361dc(int obj, int n)
{
    I32(obj) = n;
    I32(obj + 8) = -1;
    FUN_0004be04(0x11da14, 0x11d6f4, (int)(0x131090u + 8u * (uint32_t)I32(obj)));
    FUN_0004c01c(0x11da14, 0x11d6f4, (int)(0x131090u + 8u * (uint32_t)I32(obj)));
}
#define HOOK_36cc5
static void FUN_00036cc5(int set)
{
    I32(set + 0x14) = 2; I32(set + 0x10) = I32(set + 0x14); I32(set + 0xc) = I32(set + 0x10); I32(set + 4) = I32(set + 0xc); I32(set) = I32(set + 4);
    I32(set + 8) = 0;
}
#define HOOK_37dcf
static void FUN_00037dcf(void) { while (FUN_000b5d84(0, 0xc, 0) != 0) FUN_000b5fa4(); }
#define HOOK_38462
static void FUN_00038462(void) { }

/*
 * A COMPUTER CAR'S DRIVING (0x17726): throttle (+0x1ec), steering (+0x660) and handbrake (+0x1e8).
 * - Its grip: 3 less a quarter of the ground's grip for these tyres under the four wheels (0x11cdf0
 *   rows by the wheels' ground kinds +0x5d0 / +0x640 / +0x6b0 / +0x720, column the tyres +0x50).
 * - Slow (under 30) with the car's flag +0x58: full throttle, wheel straight.
 * - Its pace along its line: 22100 in the first six seconds of the race (0x13335c), else 170 x its
 *   speed held to 10 .. 130; the line (+0xc4, the path's state) moves on by it (0x2d59c, x 0.95 out
 *   of arcade).  The line's point's speed share (path 0x127350 +0xc8 node's +0xc, a hundredth off
 *   100; out of arcade 0.7 of it) says whether to lift: at or over it the throttle is nought.
 * - Without the game's flag 0x11f0a8 bit 0x20: the throttle on or off by the clock and the car's
 *   number (a quarter of the time on), the handbrake off.
 * - Else it aims at the line's point (B, +0xd4 / +0xdc) from a point ahead of it (A: speed x step x
 *   1.5 x 111 along its travel +0x508); the steering towards it is the angle (0x14dc0) from where
 *   the wheels should point: its heading +0x4e8 less step x +0x44 x 1 (fast on the front, +0x90 over
 *   0.8) or 2.
 * - Out of the first six seconds and when not far off its line (+0x4c4 against +0xf8 within 0.9) it
 *   looks for slower cars on the way from A to B (0x1766c: how far along, -0.2 .. 1) nearer the line
 *   than 1.2 x the two widths (+0x4c8): the first along.  Found: that car is told (+0x4c bit 0x10,
 *   a computer's), this one marks itself (bit 0x20); a car behind A, or one further than twice its
 *   length (+0x4d0) away, is taken as if there; it steers to whichever side of it (its width off the
 *   line through it) turns the less from the line, unless that is over an eighth of a turn: then at
 *   the line, and off the throttle when the car is more than a tenth along.
 * - Otherwise it steers at the line, and lifts off when a car is in front within its length x grip,
 *   alongside within 3 x grip x its width, and it is going over 0.4 of its top speed (+0x5c).
 * - Not avoiding: the steering x 5 (wheels at the turn's side, fast on the back +0x94), x 3 or x 2.
 * - The throttle: 1 less the turn left (to 1) x speed share x +0x48 x 0.3; over the wheel's lock
 *   (+0x6c) the steering is held to it and the throttle cut by speed share x +0x48 x 1.5 (to 1); over
 *   0.7 it is full; x the lift.  Under a half, the handbrake is the rest; with no travel (+0x510) the
 *   brake +0x1e4 and the handbrake are off.  The handbrake moves towards it at 0x28 (0x111d6).
 */
#define HOOK_17726
static void FUN_00017726(int car, uint32_t r1, uint32_t r2, int r3)
{
    typedef long double ld;
    (void)r1; (void)r2;
    const uint32_t g = 0x11cdf0u + 4u * (uint32_t)I32(car + 0x50);
    const float grip = (float)(3.0L - ((((ld)F32(g + 0x24u * (uint32_t)I32(car + 0x6b0)) + F32(g + 0x24u * (uint32_t)I32(car + 0x5d0)))
                                       + F32(g + 0x24u * (uint32_t)I32(car + 0x720))) + F32(g + 0x24u * (uint32_t)I32(car + 0x640))) * 0.25L);
    U8(car + 0x4c) &= 0xcf;
    if (I32(car + 0x500) < 0x41f00000 && I32(car + 0x58) != 0) { F32(car + 0x1ec) = 1.0f; I32(car + 0x660) = 0; }
    float pace;
    if (I32(0x13335c) < 0x40c00000) pace = 22100.0f;
    else {
        const float v = 10.0f >= F32(car + 0x500) ? 10.0f : I32(car + 0x500) < 0x43020000 ? F32(car + 0x500) : 130.0f;
        pace = (float)((ld)v * 170.0f);
    }
    const int32_t share = I32(U32(U32(0x127350) + 0x24u * (uint32_t)I32(car + 0xc4)) + 0x38u * (uint32_t)I32(car + 0xc8) + 0xc);
    float lift;
    if (U8(0x130d10) & 1) lift = (float)((ld)(100 - share) * (double)0.01);
    else {
        pace = (float)(double)((ld)(double)pace * (double)0.95);
        lift = (float)(((double)100.0 - (ld)share * (double)0.7) * (double)0.01);
    }
    lift = (ld)F32(car + 0x500) / F32(car + 0x5c) >= lift ? 0.0f : 1.0f;
    FUN_0002d59c((int)(U32(0x127350) + 0x24u * (uint32_t)I32(car + 0xc4)), car + 0xc4, 0x24 * I32(car + 0xc4), r3, pace);
    if (!(U8(0x11f0a8) & 0x20)) {
        const int32_t k = (int32_t)truncl(((ld)I32(car) * (double)0.4 + (double)3.0) * F32(0x13335c) + (ld)I32(car) * (double)0.4);
        F32(car + 0x1ec) = (k & 3) ? 0.0f : 1.0f;
        FUN_000111d6(car + 0x1e8, 0.0f, 0x28);
        return;
    }
    const float off = (float)fabs(FUN_00014dc0(F32(car + 0x4c4), F32(car + 0xf8)));
    const float ahead = (float)((ld)F32(car + 0x500) * ((ld)F32(0x133350) * (double)1.5) * (double)111.0);
    const uint32_t sp0 = s2_sp, P = s2_push(36), A = P + 12, B = P + 24;   /* (as 0x1766c reads them) */
    F32(A) = (float)(sinl((ld)F32(car + 0x508)) * ahead + (ld)I32(car + 0x4d8));
    F32(A + 4) = 0.0f;
    F32(A + 8) = (float)((ld)I32(car + 0x4e0) - cosl((ld)F32(car + 0x508)) * ahead);
    F32(B) = (float)I32(car + 0xd4); F32(B + 4) = 0.0f; F32(B + 8) = (float)I32(car + 0xdc);
    /* (the game first takes the angle from the travel +0x508 when fast on the back, and drops it) */
    const float aim = (float)FUN_00014dc0(F32(car + 0x4e8), (float)((ld)F32(car + 0x44) * (F32(car + 0x90) > (double)0.8 ? -1.0f : -2.0f) * F32(0x133350)));
    const float ex = (float)((ld)F32(B) - F32(A)), ey = (float)((ld)F32(B + 4) - F32(A + 4)), ez = (float)((ld)F32(B + 8) - F32(A + 8));
    const float head = s2r_ground_angle(ex, ez);
    int avoided = 0, lifted = 0;
    if (!(off >= (double)0.9) && I32(0x13335c) > 0x40c00000) {
        float best = 2.0f, reach = 0.0f, D[3] = { 0 }, Q[3] = { 0 };
        int who = 0;
        for (int i = 0; i < I32(0x130c48); i++) {
            const uint32_t o = 0x119300u + 0xcc0u * (uint32_t)i;
            if (i == I32(car) || F32(o + 0x500) >= F32(car + 0x500)) continue;
            F32(P) = (float)I32(o + 0x4d8); F32(P + 4) = 0.0f; F32(P + 8) = (float)I32(o + 0x4e0);
            const float s = (float)FUN_0001766c((int)P, (int)A, (int)B);
            if (!(s >= (double)-0.2) || s2_bits(s) > 0x3f800000 || s >= best) continue;
            const float om = (float)(1.0L - s);
            float d[3];
            for (uint32_t k = 0; k < 3; k++) {
                const float c = (float)((ld)(float)((ld)F32(A + 4 * k) * om) + (float)((ld)F32(B + 4 * k) * s));
                d[k] = (float)((ld)c - F32(P + 4 * k));
            }
            const float r = (float)((ld)(I32(o + 0x4c8) + I32(car + 0x4c8)) * (double)1.2);
            const float dist = (float)sqrt((double)((ld)d[1] * d[1] + (ld)d[0] * d[0] + (ld)d[2] * d[2]));
            if (dist >= r) continue;
            who = i; best = s; reach = r;
            for (uint32_t k = 0; k < 3; k++) { D[k] = d[k]; Q[k] = F32(P + 4 * k); }
        }
        if (s2_bits(best) < 0x40000000) {
            const uint32_t o = 0x119300u + 0xcc0u * (uint32_t)who;
            if (U32(o + 0xb4) == 0) U8(o + 0x4c) |= 0x10;
            U8(car + 0x4c) |= 0x20;
            const float lim = (float)(int32_t)(2u * U32(car + 0x4d0));
            const float len = (float)sqrt((double)((ld)ey * ey + (ld)ex * ex + (ld)ez * ez));
            if (0.0f > best || !((ld)len * best >= lim)) {
                best = (float)((ld)lim / len);
                const float om = (float)(1.0L - best);
                for (uint32_t k = 0; k < 3; k++) {
                    const float c = (float)((ld)(float)((ld)F32(A + 4 * k) * om) + (float)((ld)F32(B + 4 * k) * best));
                    Q[k] = (float)((ld)c - D[k]);
                }
            }
            const float dl = (float)sqrt((double)((ld)D[1] * D[1] + (ld)D[0] * D[0] + (ld)D[2] * D[2]));
            float n[3] = { 0.0f, 0.0f, 0.0f };
            if (!s2r_zero(dl)) { const float inv = (float)(1.0L / dl); for (uint32_t k = 0; k < 3; k++) n[k] = (float)((ld)D[k] * inv); }
            float L[3], R[3];
            for (uint32_t k = 0; k < 3; k++) {
                const float w = (float)((ld)n[k] * reach);
                L[k] = (float)((ld)(float)((ld)Q[k] - w) - F32(A + 4 * k));
                R[k] = (float)((ld)(float)((ld)Q[k] + w) - F32(A + 4 * k));
            }
            const float left = s2r_ground_angle(L[0], L[2]), right = s2r_ground_angle(R[0], R[2]);
            const double turn_left = fabs(FUN_00014dc0(left, head));
            if (fabs(FUN_00014dc0(right, head)) > turn_left) F32(car + 0x660) = (float)FUN_00014dc0(left, aim);
            else F32(car + 0x660) = (float)FUN_00014dc0(right, aim);
            if (fabsf(F32(car + 0x660)) > (double)0x1.921fb6p-1) {
                F32(car + 0x660) = (float)FUN_00014dc0(head, aim);
                if (best > (double)0.1) lifted = 1;
            }
            avoided = 1;
        } else F32(car + 0x660) = (float)FUN_00014dc0(head, aim);
    } else {
        F32(car + 0x660) = (float)FUN_00014dc0(head, aim);
        for (int i = 0; i < I32(0x130c48); i++) {
            const uint32_t o = 0x119300u + 0xcc0u * (uint32_t)i;
            if (i == I32(car)) continue;
            const ld along = (ld)F32(o + 0x4bc) - F32(car + 0x4bc);
            if (!((ld)I32(car + 0x4d0) * grip > along) || 0.0L >= along) continue;
            if (!((ld)I32(car + 0x4c8) * ((ld)grip * 3.0f) > fabsl((ld)F32(o + 0x4c0) - F32(car + 0x4c0)))) continue;
            if (F32(car + 0x500) > (ld)F32(car + 0x5c) * (double)0.4) lifted = 1;
        }
    }
    s2_sp = sp0;
    if (!avoided) {
        float by;
        if (F32(car + 0x94) > (double)0.8) by = 0.0L >= (ld)F32(car + 0x660) * F32(car + 0x4d4) ? 3.0f : 5.0f;
        else by = F32(car + 0x90) > (double)0.8 ? 2.0f : 3.0f;
        F32(car + 0x660) = (float)((ld)F32(car + 0x660) * by);
    }
    if (lifted) I32(car + 0x1ec) = 0;
    else {
        const double t = (double)(((ld)F32(car + 0x500) / F32(car + 0x5c)) * ((ld)F32(car + 0x48) * (double)0.3));
        const float c = s2_bits(off) < 0x3f800000 ? off : 1.0f;
        F32(car + 0x1ec) = (float)(1.0L - (ld)c * t);
    }
    if (!(F32(car + 0x6c) >= fabsf(F32(car + 0x660)))) {
        F32(car + 0x660) = (float)((ld)(0.0f > F32(car + 0x660) ? -1 : 1) * F32(car + 0x6c));
        const ld q = (ld)F32(car + 0x48) * (double)1.5 * F32(car + 0x500) / F32(car + 0x5c);
        const double k = 1.0L >= q ? (double)q : 1.0;
        F32(car + 0x1ec) = (float)(double)((1.0L - k) * (double)F32(car + 0x1ec));
    }
    if (F32(car + 0x1ec) > (double)0.7) F32(car + 0x1ec) = 1.0f;
    F32(car + 0x1ec) = (float)((ld)F32(car + 0x1ec) * lift);
    float hand = F32(car + 0x1ec) > (double)0.5 ? 0.0f : (float)(1.0L - F32(car + 0x1ec));
    if ((U32(car + 0x510) & 0x7fffffffu) == 0) { I32(car + 0x1e4) = 0; hand = 0.0f; }
    FUN_000111d6(car + 0x1e8, hand, 0x28);
}

/*
 * TWO CARS PUSHED APART (0x20a18, cars a and b by number).  Each car's outline is its four wheel
 * corners (+0x514.., 0xc apart) made 1.4 times as large about their mean (summed corners 2, 0, 3,
 * 1, a quarter of it).  A corner of one inside the other's outline (on the same side of all four
 * edges -- the edges as table 0x112e64's pairs 3-2, 2-0, 0-1, 1-3, each one's side by its square
 * turned a quarter about the height: x cos - z sin, z cos + x sin of the float quarter turn) is
 * taken back along the line between the two centres to that edge (0x20970: how far along, the
 * edge's share 0 .. 1): the deepest of these, a's corners first (their distance turned over),
 * pushes a back and b on by that share of the line between the centres, in x and z, both sets of
 * corners (+0x514.. and +0x770..).  Two cars both under 5 (+0x500) and neither already turned
 * (+0x58), nearer each other nose to tail than three of a's width (+0x4c8), are both turned:
 * +0x54 = their heading less a fifth of a half turn, +0x58 = which way that is (-1 / 1).
 */
#define HOOK_20a18
static void FUN_00020a18(int a, int b)
{
    typedef long double ld;
    static const int edge[4][2] = { { 3, 2 }, { 2, 0 }, { 0, 1 }, { 1, 3 } };   /* (0x112e64) */
    const uint32_t A = 0x119300u + 0xcc0u * (uint32_t)a, B = 0x119300u + 0xcc0u * (uint32_t)b;
    /* (the routine's own frame: 0x20970 reads and writes these, and keeps its last answers there
       when the two lines run side by side, as the game's does) */
    const uint32_t sp0 = s2_sp, fp = s2_push(0x850) + 0x850;
    const uint32_t C = fp - 0x384, dir = fp - 0x284, E = fp - 0x5cc, T = fp - 0x5dc, U = fp - 0x170;
    float ca[3], cb[3];
    for (uint32_t k = 0; k < 3; k++) {
        const uint32_t pa = A + 0x514u + 4 * k, pb = B + 0x514u + 4 * k;
        ca[k] = (float)((ld)(float)((ld)(float)((ld)(float)((ld)F32(pa + 0x18) + F32(pa)) + F32(pa + 0x24)) + F32(pa + 0xc)) * 0.25f);
        cb[k] = (float)((ld)(float)((ld)(float)((ld)(float)((ld)F32(pb + 0x18) + F32(pb)) + F32(pb + 0x24)) + F32(pb + 0xc)) * 0.25f);
        F32(dir + 4 * k) = (float)((ld)cb[k] - ca[k]);
    }
    for (uint32_t c = 0; c < 4; c++) for (uint32_t k = 0; k < 3; k++) {   /* (C: a's corner at +0, b's at +0xc, 0x18 a corner) */
        F32(C + 0x18 * c + 4 * k) = (float)((ld)(float)((ld)(float)((ld)F32(A + 0x514 + 0xc * c + 4 * k) - ca[k]) * (float)1.39999998f) + ca[k]);
        F32(C + 0x18 * c + 0xc + 4 * k) = (float)((ld)(float)((ld)(float)((ld)F32(B + 0x514 + 0xc * c + 4 * k) - cb[k]) * (float)1.39999998f) + cb[k]);
    }
    const float sn = (float)sinl((ld)(float)1.57079637f), cs = (float)cosl((ld)(float)1.57079637f);
    float deepest = 0.0f;
    for (int pass = 0; pass < 2; pass++) {
        const uint32_t mine = pass ? 0xc : 0, theirs = pass ? 0 : 0xc;   /* (a's corners in b's outline, then b's in a's) */
        for (uint32_t c = 0; c < 4; c++) {
            const uint32_t P = C + 0x18 * c + mine;
            int inside = 1;
            float first = 0.0f;
            for (int e = 0; e < 4; e++) {
                const uint32_t Q = C + 0x18u * (uint32_t)edge[e][0] + theirs, R = C + 0x18u * (uint32_t)edge[e][1] + theirs;
                const float ex = (float)((ld)F32(R) - F32(Q)), ey = (float)((ld)F32(R + 4) - F32(Q + 4)), ez = (float)((ld)F32(R + 8) - F32(Q + 8));
                const float nx = (float)((ld)ex * cs - (ld)ez * sn), nz = (float)((ld)ez * cs + (ld)ex * sn);
                const float vx = (float)((ld)F32(P) - F32(Q)), vy = (float)((ld)F32(P + 4) - F32(Q + 4)), vz = (float)((ld)F32(P + 8) - F32(Q + 8));
                const float side = (float)((ld)vy * ey + (ld)vx * nx + (ld)vz * nz);
                if (e == 0) first = side;
                else if (0.0L > (ld)side * first) inside = 0;
            }
            if (!inside) continue;
            for (int e = 0; e < 4; e++) {
                const uint32_t Q = C + 0x18u * (uint32_t)edge[e][0] + theirs, R = C + 0x18u * (uint32_t)edge[e][1] + theirs;
                for (uint32_t k = 0; k < 3; k++) F32(E + 4 * k) = (float)((ld)F32(R + 4 * k) - F32(Q + 4 * k));
                FUN_00020970((int)P, (int)dir, (int)Q, (int)E, (int)T, (int)U);
                if (!pass) F32(T) = -F32(T);
                if (0.0f > F32(U) || s2_bits(F32(U)) > 0x3f800000) continue;
                if (F32(T) > deepest) deepest = F32(T);
            }
        }
    }
    if (!(0.0f >= deepest)) {
        const float px = (float)((ld)F32(dir) * deepest), pz = (float)((ld)F32(dir + 8) * deepest);
        for (uint32_t c = 0; c < 4; c++) {
            const uint32_t at = 0x514u + 0xc * c, was = 0x770u + 0xc * c;
            F32(A + at) = (float)((ld)F32(A + at) - px); F32(A + at + 8) = (float)((ld)F32(A + at + 8) - pz);
            F32(A + was) = (float)((ld)F32(A + was) - px); F32(A + was + 8) = (float)((ld)F32(A + was + 8) - pz);
            F32(B + at) = (float)((ld)F32(B + at) + px); F32(B + at + 8) = (float)((ld)F32(B + at + 8) + pz);
            F32(B + was) = (float)((ld)F32(B + was) + px); F32(B + was + 8) = (float)((ld)F32(B + was + 8) + pz);
        }
        if (I32(A + 0x500) < 0x40a00000 && I32(B + 0x500) < 0x40a00000 && I32(A + 0x58) == 0 && I32(B + 0x58) == 0) {
            const float x1 = (float)((ld)F32(A + 0x52c) - F32(B + 0x514)), z1 = (float)((ld)F32(A + 0x534) - F32(B + 0x51c));
            const float x2 = (float)((ld)F32(A + 0x514) - F32(B + 0x52c)), z2 = (float)((ld)F32(A + 0x51c) - F32(B + 0x534));
            const float l1 = (float)sqrt((double)((ld)0.0f * 0.0f + (ld)x1 * x1 + (ld)z1 * z1));
            const float l2 = (float)sqrt((double)((ld)0.0f * 0.0f + (ld)x2 * x2 + (ld)z2 * z2));
            if ((ld)(I32(A + 0x4c8) * 3) > (ld)(float)((ld)l2 + l1)) {
                const uint32_t car[2] = { A, B };
                for (int i = 0; i < 2; i++) {
                    F32(car[i] + 0x54) = (float)FUN_00014dc0(F32(car[i] + 0x4e8), (float)0.628318548f);
                    I32(car[i] + 0x58) = 0.0 > FUN_00014dc0(F32(car[i] + 0x54), F32(car[i] + 0x4e8)) ? -1 : 1;
                }
            }
        }
    }
    s2_sp = sp0;
}

/*
 * A REPLAY'S FRAME PLAYED (0x4a488): at the recording's end (frame 0x1330c0 one short of its count
 * 0x1330bc, or of 0x1330c8) the followed car 0x11f0cc is done (+0x4c bit 1, +4 nought) and its
 * controls let go (its controls' record +0xa8.., six words); with 0x11f0a0 set, 0x11f0a8 gets bits
 * 1 and 2.  Otherwise the frame (0x1330ac, 0x20 bytes each) gives the step (0x133350), the clock's
 * step (0x133354) and the six controls, and the frame counts on.
 */
#define HOOK_4a488
static void FUN_0004a488(void)
{
    const uint32_t car = 0x119300u + 0xcc0u * (uint32_t)I32(0x11f0cc);
    if (I32(0x1330bc) - 1 == I32(0x1330c0) || I32(0x1330c8) - 1 == I32(0x1330c0)) {
        U8(car + 0x4c) |= 1;
        I32(car + 4) = 0;
        for (uint32_t i = 0; i < 6; i++) I32(U32(car + 0xb4) + 0xa8 + 4 * i) = 0;
        if (I32(0x11f0a0) != 0) U8(0x11f0a8) |= 3;
    } else {
        const uint32_t f = U32(0x1330ac) + 0x20u * (uint32_t)I32(0x1330c0);
        U32(0x133350) = U32(f); U32(0x133354) = U32(f + 4);
        for (uint32_t i = 0; i < 6; i++) U32(U32(car + 0xb4) + 0xa8 + 4 * i) = U32(f + 8 + 4 * i);
        I32(0x1330c0) += 1;
    }
}

/*
 * A FRAME RECORDED (0x4a59a), while recording (0x1330b4): into frame 0x1330c4 of the recording
 * (0x1330ac; in time attack 0x1330b0) the step, the clock's step and the player's (0x11f0d0) six
 * controls -- which are then taken back from the frame as written.  The frame counts on; at the
 * recording's end (its count 0x1330bc, half of it in time attack) recording stops (0x1330d0) and
 * the count stays.  0x1330cc follows it.
 */
#define HOOK_4a59a
static void FUN_0004a59a(void)
{
    if (I32(0x1330b4) == 0) return;
    const int attack = (U8(0x130d10) & 4) != 0;
    const uint32_t f = U32(attack ? 0x1330b0 : 0x1330ac) + 0x20u * (uint32_t)I32(0x1330c4);
    U32(f) = U32(0x133350); U32(f + 4) = U32(0x133354);
    const int32_t was = I32(0x1330c4);
    for (uint32_t i = 0; i < 6; i++) {
        const uint32_t c = U32(0x119300u + 0xcc0u * (uint32_t)I32(0x11f0d0) + 0xb4) + 0xa8 + 4 * i;
        U32(f + 8 + 4 * i) = U32(c);
        U32(c) = U32(f + 8 + 4 * i);
    }
    I32(0x1330c4) += 1;
    if ((attack ? I32(0x1330bc) / 2 : I32(0x1330bc)) == I32(0x1330c4)) { U32(0x1330d0) = 0; I32(0x1330c4) = was; }
    I32(0x1330cc) = I32(0x1330c4);
}

/*
 * A POINT OF A CURVE AND ITS WAY (0x4a831): the quadratic B-spline over three handles (h, h + 0xc,
 * h + 0x18; x and z at +0, +4) at t: weights (1 - t)^2 / 2, t^2 / 2 and the rest; the point to out,
 * and the way to it from there at t + 0.002, made one long, to out + 0xc / + 0x10.
 */
static void s2r_4a831_at(int h, float t, float *x, float *z)
{
    typedef long double ld;
    const float a = (float)((1.0L - t) * (1.0L - t) * 0.5L), c = (float)((ld)t * t * 0.5L), b = (float)(1.0L - ((ld)a + c));
    *x = (float)((ld)F32(h + 0xc) * b + (ld)F32(h) * a + (ld)F32(h + 0x18) * c);
    *z = (float)((ld)F32(h + 4) * a + (ld)F32(h + 0x10) * b + (ld)F32(h + 0x1c) * c);
}
#define HOOK_4a831
static void FUN_0004a831(int out, int handles, uint32_t r2, uint32_t r3, float t)
{
    typedef long double ld;
    (void)r2; (void)r3;
    float x, z, x2, z2;
    s2r_4a831_at(handles, t, &x, &z);
    F32(out) = x; F32(out + 4) = z;
    s2r_4a831_at(handles, (float)((ld)t + (float)0.00200000009f), &x2, &z2);
    const float dx = (float)((ld)x2 - x), dz = (float)((ld)z2 - z);
    const float inv = (float)(1.0L / (ld)sqrt((double)((ld)dz * dz + (ld)dx * dx)));
    F32(out + 0xc) = (float)((ld)dx * inv);
    F32(out + 0x10) = (float)((ld)dz * inv);
}

/* THE CLOCKS (0x4af7d): the race's (0x13335c) by the clock's step, the other (0x133360) by the
   step; the step's table (0x4b016); the animations' clock (0x133358) on by 0.3 of the step, round
   one, and as 0 .. 65535 (0x1347a0) */
#define HOOK_4af7d
static void FUN_0004af7d(void)
{
    typedef long double ld;
    F32(0x13335c) = (float)((ld)F32(0x133354) + F32(0x13335c));
    F32(0x133360) = (float)((ld)F32(0x133350) + F32(0x133360));
    FUN_0004b016();
    F32(0x133358) = (float)(double)((ld)F32(0x133350) * (double)0.3 + (double)F32(0x133358));
    if (I32(0x133358) >= 0x3f800000) F32(0x133358) = (float)((ld)F32(0x133358) + -1.0f);
    I32(0x1347a0) = (int32_t)truncl((ld)F32(0x133358) * 65535.0f);
}

/* WHICH ROW OF THE STEP'S TABLE (0x4b016): under 50 steps a second, the row of that many (0x11f2a0,
   400 bytes a row), else the last one (0x123f30) */
#define HOOK_4b016
static void FUN_0004b016(void)
{
    const long double rate = 1.0L / F32(0x133350);
    if (!(rate >= 50.0f)) I32(0x1240c0) = (int32_t)truncl(rate) * 400 + 0x11f2a0;
    else I32(0x1240c0) = 0x123f30;
}

/*
 * THE DRAWING LIST (0x4be04 .. 0x4c01c).  A library of models (0x1c bytes each, 200 of them: +0xc
 * the model, +0x14 loaded, +0x18 its slot in the list or -1) and a list of words (the models to
 * draw, -1 a free slot, 0 the end).  `rec` holds the model's number.
 *   0x4be04  takes a slot: the first free one or the end (which moves one on); the model goes in.
 *            0xd when it has one already.
 *   0x4beba  gives it back: the slot freed (-1), or the end when it was the last; 0xc without one.
 *   0x4bfbe  shows it (its model into the slot), 0x4c01c hides it (the model with bits 30 and 31
 *            turned); 0xe without a slot.
 *   0x4bd1b  every loaded model takes a slot, hidden when its model's byte 5 has bit 7;
 *   0x4bda7  every loaded model gives its slot back.
 */
#define HOOK_4be04
static uint32_t FUN_0004be04(int list, int set, int rec)
{
    const uint32_t m = (uint32_t)list + 0x1cu * U32(rec), slot = m + 0x18;
    if (I32(slot) != -1) return 0xd;
    I32(slot) = 0;
    while (I32((uint32_t)set + 4u * U32(slot)) != -1 && I32((uint32_t)set + 4u * U32(slot)) != 0) I32(slot) += 1;
    if (I32((uint32_t)set + 4u * U32(slot)) == 0) I32((uint32_t)set + 4u * U32(slot) + 4) = 0;
    U32((uint32_t)set + 4u * U32(slot)) = U32(m + 0xc);
    return 0;
}
#define HOOK_4beba
static uint32_t FUN_0004beba(int list, int set, int rec)
{
    const uint32_t slot = (uint32_t)list + 0x1cu * U32(rec) + 0x18;
    if (I32(slot) == -1) return 0xc;
    I32((uint32_t)set + 4u * U32(slot)) = I32((uint32_t)set + 4u * U32(slot) + 4) == 0 ? 0 : -1;
    I32(slot) = -1;
    return 0;
}
#define HOOK_4bfbe
static uint32_t FUN_0004bfbe(int list, int set, int rec)
{
    const uint32_t m = (uint32_t)list + 0x1cu * U32(rec);
    if (I32(m + 0x18) == -1) return 0xe;
    U32((uint32_t)set + 4u * U32(m + 0x18)) = U32(m + 0xc);
    return 0;
}
#define HOOK_4c01c
static uint32_t FUN_0004c01c(int list, int set, int rec)
{
    const uint32_t m = (uint32_t)list + 0x1cu * U32(rec);
    if (I32(m + 0x18) == -1) return 0xe;
    U32((uint32_t)set + 4u * U32(m + 0x18)) = U32(m + 0xc) ^ 0xc0000000u;
    return 0;
}
#define HOOK_4bd1b
static int FUN_0004bd1b(int list, int set)
{
    const uint32_t sp0 = s2_sp, rec = s2_push(4);
    int r = 0;
    for (uint32_t i = 0; i < 200 && r == 0; i++) {
        if (I32((uint32_t)list + 0x1cu * i + 0x14) == 0) continue;
        U32(rec) = i;
        FUN_0004be04(list, set, (int)rec);
        if (U8(U32((uint32_t)list + 0x1cu * i + 0xc) + 5) & 0x80) r = (int)FUN_0004c01c(list, set, (int)rec);
    }
    s2_sp = sp0;
    return r;
}
#define HOOK_4bda7
static uint32_t FUN_0004bda7(int list, int set)
{
    const uint32_t sp0 = s2_sp, rec = s2_push(4);
    for (uint32_t i = 0; i < 200; i++) {
        if (I32((uint32_t)list + 0x1cu * i + 0x14) == 0) continue;
        U32(rec) = i;
        FUN_0004beba(list, set, (int)rec);
    }
    s2_sp = sp0;
    return 0;
}

/*
 * THE MENU'S PICTURE FILES (0x50a9c .. 0x5160b): read from a container's entry (0x4eba5 opens it
 * into a file record in the routine's frame, 0x4ee3a reads, 0x4ed79 closes; a file that will not
 * open stops the game), into blocks of the menu's memory (0x4f0c6, named; 0x4f331 frees).
 *   sprites (0x50a9c; 0x50b71 lets them go): +0x1c their count, +0x18 the pixels' size; +4 the
 *     count's 0x14-byte records, +0 the pixels.
 *   fonts (0x50dd3; 0x50ef4): +0x3c the count (one byte); +0x44 the fonts' 0x20-byte records, +0x40
 *     each font's characters (0x500 a font: 8 bytes each of the record's +8 of them), +0x48 the
 *     pixels' size, +0x38 the pixels.
 *   a backdrop (0x51532; 0x5160b, 0x51521 when loaded; 0x514fa makes an empty one): +8 its 0x18-
 *     byte header, which must start "V652" (else the game stops), +0x18 the pixels' size; +4 the
 *     palette (0x300), +0 the pixels.
 */
static uint32_t s2r_menu_open(int cont, int entry, uint32_t file)
{
    if (FUN_0004eba5(file, (uint32_t)cont, (uint32_t)entry, 0x1122ecu) != 0) { FUN_0004eb34(file); FUN_000ae662(0); }
    return file;
}
#define HOOK_50b71
static void FUN_00050b71(int s)
{
    (void)FUN_0004f331(U32(0x1338c8), U32(s + 4));
    (void)FUN_0004f331(U32(0x1338c8), U32(s));
    U32(s + 4) = 0; U32(s) = 0; U32(s + 0x1c) = 0; U32(s + 0x18) = 0;
}
#define HOOK_50a9c
static void FUN_00050a9c(int s, int cont, int entry)
{
    if (I32(s + 0x1c) != 0) FUN_00050b71(s);
    const uint32_t sp0 = s2_sp, f = s2r_menu_open(cont, entry, s2_push(0xc0));
    FUN_0004ee3a(f, (uint32_t)s + 0x1c, 4);
    FUN_0004ee3a(f, (uint32_t)s + 0x18, 4);
    U32(s + 4) = FUN_0004f0c6(U32(0x1338c8), 0x1122efu, 0x14u * U32(s + 0x1c));
    if (U32(s + 4) == 0) (void)FUN_0004f033(U32(0x1338c8));
    FUN_0004ee3a(f, U32(s + 4), I32(s + 0x1c) * 0x14);
    U32(s) = FUN_0004f0c6(U32(0x1338c8), 0x1122f7u, U32(s + 0x18));
    if (U32(s) == 0) (void)FUN_0004f033(U32(0x1338c8));
    FUN_0004ee3a(f, U32(s), I32(s + 0x18));
    FUN_0004ed79(f, 0);
    s2_sp = sp0;
}
#define HOOK_50ef4
static void FUN_00050ef4(int s)
{
    (void)FUN_0004f331(U32(0x1338c8), U32(s + 0x44));
    (void)FUN_0004f331(U32(0x1338c8), U32(s + 0x40));
    (void)FUN_0004f331(U32(0x1338c8), U32(s + 0x38));
    U32(s + 0x44) = 0; U32(s + 0x40) = 0; U32(s + 0x38) = 0; U32(s + 0x3c) = 0; U32(s + 0x48) = 0;
}
#define HOOK_50dd3
static void FUN_00050dd3(int s, int cont, int entry)
{
    if (I32(s + 0x3c) != 0) FUN_00050ef4(s);
    const uint32_t sp0 = s2_sp, f = s2_push(0xc0);
    if (FUN_0004eba5(f, (uint32_t)cont, (uint32_t)entry, 0x1122ffu) != 0) { FUN_0004eb34(f); FUN_000ae662(0); }
    FUN_0004ee3a(f, (uint32_t)s + 0x3c, 1);
    U32(s + 0x44) = FUN_0004f0c6(U32(0x1338c8), 0x112302u, U32(s + 0x3c) << 5);
    if (U32(s + 0x44) == 0) (void)FUN_0004f033(U32(0x1338c8));
    U32(s + 0x40) = FUN_0004f0c6(U32(0x1338c8), 0x11230du, U32(s + 0x3c) * 0x500u);
    if (U32(s + 0x40) == 0) (void)FUN_0004f033(U32(0x1338c8));
    for (int i = 0; i < I32(s + 0x3c); i++) FUN_0004ee3a(f, U32(s + 0x44) + 0x20u * (uint32_t)i, 0x20);
    for (int i = 0; i < I32(s + 0x3c); i++) FUN_0004ee3a(f, U32(s + 0x40) + 0x500u * (uint32_t)i, I32(U32(s + 0x44) + 8 + 0x20u * (uint32_t)i) << 3);
    FUN_0004ee3a(f, (uint32_t)s + 0x48, 4);
    U32(s + 0x38) = FUN_0004f0c6(U32(0x1338c8), 0x112317u, U32(s + 0x48));
    if (U32(s + 0x38) == 0) (void)FUN_0004f033(U32(0x1338c8));
    FUN_0004ee3a(f, U32(s + 0x38), I32(s + 0x48));
    FUN_0004ed79(f, 0);
    s2_sp = sp0;
}
#define HOOK_5160b
static void FUN_0005160b(int b, uint32_t r1, uint32_t r2)
{
    (void)r1; (void)r2;
    (void)FUN_0004f331(U32(0x1338c8), U32(b));
    (void)FUN_0004f331(U32(0x1338c8), U32(b + 4));
    U32(b) = 0; U32(b + 4) = 0;
    FUN_00051aa4((uint32_t)b + 8, 0x18, 0);
}
#define HOOK_514fa
static uint64_t FUN_000514fa(int b, uint32_t r1, uint32_t r2, uint32_t r3)
{
    (void)r1; (void)r2; (void)r3;
    U32(b) = 0; U32(b + 4) = 0;
    FUN_00051aa4((uint32_t)b + 8, 0x18, 0);
    return (uint32_t)b;
}
#define HOOK_51521
static int FUN_00051521(int b)
{
    if (I32(b) != 0) FUN_0005160b(b, 0, (uint32_t)b);
    return b;
}
#define HOOK_51532
static void FUN_00051532(int b, int cont, int entry)
{
    if (I32(b) != 0) FUN_0005160b(b, (uint32_t)cont, (uint32_t)entry);
    const uint32_t sp0 = s2_sp, f = s2_push(0xc0);
    if (FUN_0004eba5(f, (uint32_t)cont, (uint32_t)entry, 0x112334u) != 0) { FUN_0004eb34(f); FUN_000ae662(0); }
    FUN_0004ee3a(f, (uint32_t)b + 8, 0x18);
    if (I32(b + 8) != 0x32353656) { FUN_0004ed79(f, 0); (void)FUN_000ae88d(); FUN_000ae662(0); }
    U32(b) = FUN_0004f0c6(U32(0x1338c8), 0x112353u, U32(b + 0x18));
    if (U32(b) == 0) (void)FUN_0004f033(U32(0x1338c8));
    U32(b + 4) = FUN_0004f0c6(U32(0x1338c8), 0x112357u, 0x300);
    if (U32(b + 4) == 0) (void)FUN_0004f033(U32(0x1338c8));
    FUN_0004ee3a(f, U32(b + 4), 0x300);
    FUN_0004ee3a(f, U32(b), I32(b + 0x18));
    FUN_0004ed79(f, 0);
    s2_sp = sp0;
}

/*
 * THE TRACK'S PATHS' NAMES (0x1226b: the container, the list -- 0x30 bytes a path): "splines.txt",
 * a line a word.  Its first two lines are two floats (0x1192d8, 0x1192dc); then three lines a path:
 * its name (+0), its file's name (+0x14), and a letter and a number -- the number to +0x28, the
 * letter its kind (+0x2c): C 1, T 2, N 4 (either case).  The list ends with +0x28 = -1.
 */
#define HOOK_1226b
static void FUN_0001226b(int cont, int list)
{
    const uint32_t sp0 = s2_sp, fp = s2_push(0x1d0) + 0x1d0, file = fp - 0xd0, line = fp - 0x1d0;
    if (FUN_0004eba5(file, (uint32_t)cont, 0x11017cu, 0x110179u) != 0) FUN_0004eb34(file);
    FUN_0004ef0a(file, line, 0); F32(0x1192d8) = (float)FUN_000ae758(line);
    FUN_0004ef0a(file, line, 0); F32(0x1192dc) = (float)FUN_000ae758(line);
    for (uint32_t p = (uint32_t)list;; p += 0x30) {
        if (FUN_0004ef0a(file, p, 0) != 0) { I32(p + 0x28) = -1; FUN_0004ed79(file, 0); break; }
        FUN_0004ef0a(file, p + 0x14, 0);
        FUN_0004ef0a(file, line, 0);
        U32(p + 0x28) = FUN_000ae762(line + 1);
        switch (U8(line)) {
        case 'C': case 'c': I32(p + 0x2c) = 1; break;
        case 'T': case 't': I32(p + 0x2c) = 2; break;
        case 'N': case 'n': I32(p + 0x2c) = 4; break;
        default: break;
        }
    }
    s2_sp = sp0;
}

/*
 * A TEXT OF THE GAME'S (0x3248f: its name, the list to fill): "<name>.txt" of "<game>\data\
 * local.dat" ("rt"), a line a text; lines starting '*' are notes.  The texts go one after another
 * into one block ("TEXT") and the list gets each one's place, then -1.  Gives the block.
 */
#define HOOK_3248f
static int FUN_0003248f(uint32_t name, int list)
{
    const uint32_t sp0 = s2_sp, fp = s2_push(0x2e0) + 0x2e0;
    const uint32_t cont = fp - 0xac, entry = fp - 0x12c, file = fp - 0x1e0, line = fp - 0x2e0;
    FUN_0004eaed(cont, 0x110eacu, 0x11eff8u);
    FUN_0004eaed(entry, 0x110ebeu, name);
    if (FUN_0004eba5(file, cont, entry, 0x110ec5u) != 0) FUN_0004eb34(file);
    uint32_t size = 0;
    while (FUN_0004ef0a(file, line, 1) == 0) if (U8(line) != '*') size += FUN_000b6e7e(line) + 1;
    const uint32_t block = FUN_0004f0c6(U32(0x11d6ec), 0x110ec8u, size);
    if (block == 0) (void)FUN_0004f033(U32(0x11d6ec));
    FUN_0004eea5(file, 0, 0);
    uint32_t at = 0, n = 0;
    while (FUN_0004ef0a(file, line, 1) == 0) {
        if (U8(line) == '*') continue;
        FUN_000b3be4(block + at, line);
        U32((uint32_t)list + 4 * n++) = block + at;
        at += FUN_000b6e7e(line) + 1;
    }
    I32((uint32_t)list + 4 * n) = -1;
    FUN_0004ed79(file, 0);
    s2_sp = sp0;
    return (int)block;
}

/*
 * A MENU ROOM LOADED (0x11298: the room's number).  Its container "<game>\anims\scene<nn>1.dat";
 * its paths' names (0x1226b, into a "SPLNAM" block at 0x1192cc, 0x960 bytes); the room's set
 * ("map<league + 1>": materials, the room, its palette 0x1338cc); then "splines.pth": a word, then
 * records of 100 bytes, each a path (0x11c0c) whose kind (+0x60, low three bits) says where:
 *   1  the camera's paths (number +0xe, from 1: 0x118afc / 0x118b24 / 0x118b9c), counted;
 *   2  the cameras' targets' (0x118c14 / 0x118c3c / 0x118cb4);
 *   4  a moving model's (in turn: 0x118d2c / 0x118dcc / 0x118fac), named +8 among the paths'
 *      names (0x1240d; not there stops the game), whose model by that name's kind: 1 a car
 *      ("sccar<league + 1>1.dat", its colour the league's car's +0x18), 2 the team's ("team<league
 *      + 1>1.dat", library "team"), 4 the room's own ("map<nn>"), into 0x11918c (8 bytes each).
 * Then the room's reflections ("env", flag 4).  The counts (0x1192d0 models, 0x1192d4 camera
 * paths); every model takes a slot in the drawing list; each camera path and target path runs on
 * into the next; the models shown, their animation (+0x20, +0x3c) at the start.  The SPLNAM block
 * is let go; the clock's step 0.08, 0x4ce06 = 8, 0x1192e4 = 0, the room 0x1192e0.
 */
#define HOOK_11298
static void FUN_00011298(uint32_t room, uint32_t r1, uint32_t r2, uint32_t r3)
{
    (void)r1; (void)r2; (void)r3;
    const uint32_t sp0 = s2_sp, fp = s2_push(0x1e0) + 0x1e0;
    const uint32_t buf = fp - 0xbc, file = fp - 0x170, rec = fp - 0x1d8, head = fp - 0x20;
    const uint32_t league = U32(0x1289ec);
    FUN_0004eaed(buf, 0x110094u, 0x11f060u, room, 0x31u);
    FUN_0004b49f(buf, buf);
    if (FUN_0004eba5(file, buf, 0x1100b0u, 0x1100adu) != 0) FUN_0004eb34(file);
    U32(0x1192cc) = FUN_0004f0c6(U32(0x11d6ec), 0x1100bcu, 0x960);
    if (U32(0x1192cc) == 0) (void)FUN_0004f033(U32(0x11d6ec));
    FUN_0001226b((int)buf, I32(0x1192cc));
    FUN_0004eaed(buf, 0x1100c3u, league + 0x31u);
    FUN_0004feb4(buf, 3);
    FUN_0004fca9(buf);
    FUN_0004fe21(buf, 0x1338ccu, 0, 0);
    FUN_00051664(0x1338ccu, 0);
    (void)FUN_0007f504(-0xab20, 0);
    (void)FUN_0004f6c4(0x140, 200, 100);
    FUN_00011906();
    int paths = 0, models = 0;
    FUN_0004ee3a(file, head, 4);
    while (FUN_0004ee3a(file, rec, 100) == 100) {
        const uint32_t kind = U32(rec + 0x60) & 7;
        if (kind == 1 || kind == 2) {
            const uint32_t i = FUN_000ae5f4(rec + 0xe) - 1u, d = kind == 1 ? 0 : 0x118u;
            FUN_00011c0c((int)(0x118afcu + d + 4 * i), (int)(0x118b24u + d + 0xc * i), (int)(0x118b9cu + d + 0xc * i), rec, (int)file);
            if (kind == 1) paths++;
        } else if (kind == 4) {
            const int j = FUN_0001240d(rec + 8, I32(0x1192cc));
            if (j == -1) FUN_00030e75();
            const uint32_t m = 0x30u * (uint32_t)j + U32(0x1192cc), to = 0x11918cu + 8u * (uint32_t)models;
            FUN_00011c0c((int)(0x118d2cu + 4u * (uint32_t)models), (int)(0x118dccu + 0xcu * (uint32_t)models), (int)(0x118facu + 0xcu * (uint32_t)models), rec, (int)file);
            int r = 0;
            switch (U32(m + 0x2c)) {
            case 1:
                FUN_0004eaed(buf, 0x1100c9u, 0x11f060u, league + 0x31u, 0x31u);
                FUN_0004b49f(buf, buf);
                U8(m + 0x18) = (uint8_t)(U8(0x128a9cu + 0x188u * league) + '1');
                FUN_0004feb4(m + 0x14, 1);
                if (FUN_0004bc94(0x11da14, m + 0x14, to) != 0) FUN_000ae662(0);
                (void)FUN_0004b4d4(0x11da14, U32(to), 1);
                break;
            case 2:
                FUN_0004eaed(buf, 0x1100e0u, 0x11f060u, league + 0x31u, 0x31u);
                FUN_0004b49f(buf, buf);
                FUN_0004feb4(0x1100f6u, 1);
                r = FUN_0004b843(0x11da14, m + 0x14, to);
                break;
            case 4:
                FUN_0004eaed(buf, 0x1100fbu, 0x11f060u, room, 0x31u);
                FUN_0004b49f(buf, buf);
                FUN_0004eaed(buf, 0x110114u, room);
                FUN_0004feb4(buf, 1);
                r = FUN_0004b843(0x11da14, m + 0x14, to);
                break;
            default: break;
            }
            if (r != 0) { (void)FUN_0004b169(3); FUN_0004b1a4(r); FUN_000ae662((uint32_t)r); }
            models++;
        }
    }
    FUN_0004eaed(buf, 0x11011cu, 0x11f060u, room, 0x31u);
    FUN_0004b49f(buf, buf);
    FUN_0004feb4(0x110135u, 4);
    FUN_0004ed79(file, 0);
    I32(0x1192d0) = models;
    I32(0x1192d4) = paths;
    FUN_0004bd1b(0x11da14, 0x11d6f4);
    for (int i = 0; i < paths - 1; i++) {
        U32(U32(0x118afcu + 4u * (uint32_t)i) + 4) = U32(0x118b00u + 4u * (uint32_t)i);
        U32(U32(0x118c14u + 4u * (uint32_t)i) + 4) = U32(0x118c18u + 4u * (uint32_t)i);
    }
    for (int i = 0; i < I32(0x1192d0); i++) {
        const uint32_t to = 0x11918cu + 8u * (uint32_t)i;
        FUN_0004bfbe(0x11da14, 0x11d6f4, (int)to);
        U32(U32(0x11da20u + 0x1cu * U32(to)) + 0x20) = 0;
        U32(U32(0x11da20u + 0x1cu * U32(to)) + 0x3c) = 0;
    }
    (void)FUN_0004f3c8(U32(0x11d6ec), 0x110139u);
    U32(0x133354) = 0x3da3d70au;
    U32(0x4ce06) = 8;
    F32(0x1192e4) = 0.0f;
    U32(0x1192e0) = room;
    s2_sp = sp0;
}

/*
 * THE LOADING PAGE (0x34ec6), for the track 0x130c4c: its backdrop ("load_<track>L.bkk" of
 * "gfx\graf1.dat") with its palette; the heading; unless the
 * kind of table (0x1324dc) is -1, 3 or 4, the best lap (kind 2: 0x12d388, else 0x1290bc by the
 * level 0x128a30) with its driver's name (0x1290f8 ..), and the best race (0x12f32c / 0x12f368);
 * the track's name; shown.
 */
#define HOOK_34ec6
static void FUN_00034ec6(void)
{
    const uint32_t sp0 = s2_sp, fp = s2_push(0x140) + 0x140, cont = fp - 0x134, entry = fp - 0x34, text = fp - 0xb4;
    const uint32_t track = U32(0x130c4c), kind = U32(0x1324dc), bk = U32(0x1323c8);
    FUN_00044587(0x11343c);
    FUN_0004eaed(cont, 0x110feau, 0x31u);
    FUN_0004eaed(entry, 0x110ff9u, track + 0x31u, 0x4cu);
    FUN_00051532((int)bk, (int)cont, (int)entry);
    FUN_00051b02(0, 0, bk + 8, U32(bk));
    FUN_000ae6e3(0x1338ccu, U32(bk + 4), 0x300);
    FUN_00051664(U32(bk + 4), 0);
    (void)FUN_0005105e(0x12420cu, U32(bk + 4));
    FUN_0003327c(U32(0x127f24), 0xaa, 1, 1, 2);
    if (I32(0x1324dc) != -1 && kind != 4 && kind != 3) {
        FUN_0003327c(U32(0x127f28), 0x48, 2, 1, 2);
        const uint32_t row = kind * 0x20d0u + track * 0x4b0u;
        if (kind == 2) FUN_00037e06(cont, track * 0x4b0u, 0, 0, F32(0x12d388u + track * 0x4b0u));
        else FUN_00037e06(cont, row, 0, 0, F32(0x1290bcu + U32(0x128a30) * 300u + row));
        FUN_0004eaed(text, 0x111007u, cont, U32(0x128a30) * 300u + row + 0x1290f8u);
        FUN_0003327c(text, 0x54, 1, 1, 2);
        FUN_0003327c(U32(0x127f2c), 0x70, 2, 1, 2);
        FUN_00037e06(cont, track * 300u, 0, 0, F32(0x12f32cu + kind * 0x834u + track * 300u));
        FUN_0004eaed(text, 0x11100du, cont, track * 300u + kind * 0x834u + 0x12f368u);
        FUN_0003327c(text, 0x7c, 1, 1, 2);
    }
    FUN_000333c1((int)track + 0x13, 4);
    FUN_0007f5a2(0, 0);
    s2_sp = sp0;
}

/*
 * A SCENE'S PATHS (0x3519f: how many scenes, the name): for each, "<name><n>.pth" of
 * "<game>\data\gen.dat": camera paths (kind 1) and targets (2) by the scene's
 * number, moving models' paths (4) at 4 x the scene + their own number.  Gives the last of these.
 */
#define HOOK_3519f
static int FUN_0003519f(int n, uint32_t name, uint32_t r2, uint32_t r3)
{
    (void)r2; (void)r3;
    const uint32_t sp0 = s2_sp, fp = s2_push(0x250) + 0x250;
    const uint32_t entry = fp - 0xac, cont = fp - 0x12c, file = fp - 0x1e0, head = fp - 0x28, rec = fp - 0x248;
    int last = 0;
    for (int i = 0; i < n; i++) {
        FUN_0004eaed(entry, 0x111013u, name, (uint32_t)i + 0x30u);
        FUN_0004eaed(cont, 0x11101cu, 0x11eff8u);
        if (FUN_0004eba5(file, cont, entry, 0x11102cu) != 0) FUN_0004eb34(file);
        FUN_0004ee3a(file, head, 4);
        while (FUN_0004ee3a(file, rec, 100) == 100) {
            const uint32_t kind = U32(rec + 0x60) & 7;
            if (kind == 1 || kind == 2) {
                const uint32_t d = kind == 1 ? 0 : 0x118u, k = (uint32_t)i;
                FUN_00011c0c((int)(0x118afcu + d + 4 * k), (int)(0x118b24u + d + 0xc * k), (int)(0x118b9cu + d + 0xc * k), rec, (int)file);
            } else if (kind == 4) {
                last = (int)FUN_000ae5f4(rec + 0xe) + i * 4;
                const uint32_t k = (uint32_t)last;
                FUN_00011c0c((int)(0x118d2cu + 4 * k), (int)(0x118dccu + 0xc * k), (int)(0x118facu + 0xc * k), rec, (int)file);
            }
        }
        FUN_0004ed79(file, 0);
    }
    s2_sp = sp0;
    return last;
}

/*
 * A CAR OF THE LEAGUE'S PAGE SET OFF (0x35b86: its record, its number): size 1 + 0.6 x n; it starts
 * 2000 to one side and 2500 back, the sides taking turns (the game's power: -1 to the n + 2), 2000
 * up, turned -2200 about two axes, not moving; +0x2c 0x200.
 */
#define HOOK_35b86
static void FUN_00035b86(int p, int n)
{
    I32(p + 0x44) = n;
    F32(p + 0x3c) = (float)((long double)n * (double)0.6 + 1.0L);
    I32(p + 0x20) = -2200; I32(p + 0x1c) = I32(p + 0x20); I32(p + 0x18) = I32(p + 0x1c);
    I32(p + 0x38) = 0; I32(p + 0x34) = 0; I32(p + 0x30) = 0; I32(p + 0x14) = 0; I32(p + 0x10) = 0; I32(p + 0xc) = 0;
    const long double side = ((n + 2) & 1) ? -1.0L : 1.0L;       /* (-1)^(n + 2), as 0xafed2 gives it */
    I32(p) = (int32_t)truncl(side * (double)-2000.0);
    I32(p + 4) = (int32_t)truncl(side * (double)-2500.0);
    I32(p + 8) = 2000;
    I32(p + 0x24) = 0; I32(p + 0x28) = 0; I32(p + 0x2c) = 0x200; I32(p + 0x40) = 0; I32(p + 0x48) = 0;
}

/* ALL THE LEAGUE'S CARS ARRIVED? (0x36150): none but the last still moving (+0x40 under 0), and the
   last one placed (+0x48) and under 0.4 along (+0x3c) -- 1; else 0.  The high word is the second
   argument, as the game leaves it. */
#define HOOK_36150
static uint64_t FUN_00036150(uint32_t r0, uint32_t r1)
{
    (void)r0;
    const int last = I32(0x1130b0u + 0x1cu * U32(0x128a10)) - 1;
    uint32_t v = 0;
    for (int i = 0;; i++) {
        const uint32_t c = 0x131290u + 0x4cu * (uint32_t)i;
        if (last <= i) { v = !((double)0.4 <= F32(c + 0x3c)) && I32(c + 0x48) != 0; break; }
        if (0.0 <= F32(c + 0x40)) { v = 0; break; }
    }
    return (uint64_t)r1 << 32 | v;
}

/* THE LETTERS OF THE NAME PAGE (0x3661d: the letter, the first and the end of the row): the one
   given lit (+8 of its 0x28-byte record = 0), the others not (1) */
#define HOOK_3661d
static void FUN_0003661d(int letter, int from, int to)
{
    for (int i = from; i < to; i++) {
        const uint32_t r = 0x1314a8u + 0x28u * (uint32_t)i;
        if (I32(r) == letter && I32(r + 8) != 0) I32(r + 8) = 0;
        else if (I32(r) != letter && I32(r + 8) != 1) I32(r + 8) = 1;
    }
}

/* A KEY LET GO (0x36845): of the keys of the table 0x112f50 (to its first one under 0), the first
   that was down (0x1288ec) and is up now, as the table's value; 0 for none.  Keys down are marked. */
#define HOOK_36845
static uint64_t FUN_00036845(uint32_t r0, uint32_t r1)
{
    (void)r0;
    uint32_t v = 0;
    for (uint32_t i = 0; I32(0x112f50u + 4 * i) >= 0; i++) {
        if (I32(0x112f50u + 4 * i) == 0 || U8(0x100002u + i) == 0) {
            if (I32(0x1288ecu + 4 * i) == 1) { I32(0x1288ecu + 4 * i) = 0; v = U32(0x112f50u + 4 * i); break; }
        } else I32(0x1288ecu + 4 * i) = 1;
    }
    return (uint64_t)r1 << 32 | v;
}

/* THE NETWORK GAME'S START (0x370ed host / 0x37167 join): the network's own (not carried over),
   a "NET_SPACE" block, the page's fade and step at 1, the next page 0x3bf06 */
static void s2r_net_page(uint32_t name)
{
    if (FUN_0004f0c6(U32(0x11d6ec), name, 0x4000) == 0) (void)FUN_0004f033(U32(0x11d6ec));
    U32(0x1324a0) = 0x3f800000u; U32(0x132498) = 0x3f800000u; I32(0x132494) = 0x3bf06;
    U32(0x1324e4) = 0; U32(0x1324e0) = 0; U32(0x131f28) = 0;
}
#define HOOK_370ed
static void FUN_000370ed(void) { (void)FUN_000b4d73(); s2r_net_page(0x11113au); }
#define HOOK_37167
static void FUN_00037167(void) { FUN_000368e2(0, 0x5678u, 0); FUN_000368e2(0, 0x5678u, 0); (void)FUN_000b5ae0(); s2r_net_page(0x111144u); }

/* THE NETWORK GAME'S LINE (0x37686): the other player's name (0x132bc0 to the host, 0x132508 to the
   one who joined) and 0x128068 */
#define HOOK_37686
static void FUN_00037686(void)
{
    const uint32_t sp0 = s2_sp, buf = s2_push(0xa0) + 0xa0 - 0x98;
    if (I32(0x1324e4) == 2) FUN_0004eaed(buf, 0x111158u, 0x132bc0u, U32(0x128068));
    else FUN_0004eaed(buf, 0x11115eu, 0x132508u, U32(0x128068));
    FUN_0003327c(buf, 0x8c, 1, 1, 2);
    s2_sp = sp0;
}

/* A MENU FRAME'S INPUT (0x38d48): the CD's track looked at (unless 0x128a44), the keys read, the
   player's controller when there is one (0x11f298), the clocks (0x4adf8), and 0x38462 */
#define HOOK_38d48
static void FUN_00038d48(void)
{
    if (I32(0x128a44) == 0) (void)FUN_0004d630(0, 0);
    FUN_0004cca5(0, 0);
    if (I32(0x11f298) != 0) FUN_00031888(0x127708);
    FUN_0004adf8();
    FUN_00038462();
}

/*
 * A CAR OF THE LEAGUE'S PAGE MOVED (0x35cb0: its record; t = +0x3c, from 1 down to 0 at 0.55 a
 * second).  Not yet placed (+0x48 = 0): it comes in -- its turn (+0x24.. from +0x30..: t of the
 * way), its place from the start (+0..) to the end (+0xc..) by 1 - cos(t a quarter turn), x and z
 * swinging by +0x18 / +0x1c x t x cos / sin(t a whole turn).  Placed: it goes off again, by
 * 1 - sin(t a quarter turn) from where it was, x and z swinging by 1 - cos(t a whole turn), turning
 * on (0.15 and 0.05 of 2048 x the same, round the turn).  At t under 0.05: placed and t from 1
 * again, its end now where it is, the turn back to the start's, +0x18.. 1500, off to x 7000 (odd
 * cars -6500), y -1000, z -2000; or, placed already, done (+0x40 = -1).  Not when done.
 */
#define HOOK_35cb0
static void FUN_00035cb0(int p)
{
    typedef long double ld;
    if (0.0f > F32(p + 0x40)) return;
    const uint32_t m = U32(0x11da14u + 0x1cu * U32(0x130fa8u + 8u * U32(p + 0x44)) + 0xc);
    const ld t = F32(p + 0x3c);
    if (I32(p + 0x48) == 0) {
        for (uint32_t k = 0; k < 3; k++)
            I32(m + 0x14 + 4 * k) = (int32_t)truncl((ld)(I32(p + 0x24 + 4 * k) - I32(p + 0x30 + 4 * k)) * t + (ld)I32(p + 0x30 + 4 * k));
        const ld in = 1.0L - cosl(t * (float)1.57079637f);
        I32(m + 8) = (int32_t)truncl((in * (ld)(I32(p) - I32(p + 0xc)) + (ld)I32(p + 0xc)) + ((ld)I32(p + 0x18) * t) * cosl(t * (float)6.28318548f));
        I32(m + 0xc) = (int32_t)truncl(((ld)(I32(p + 4) - I32(p + 0x10)) * in + (ld)I32(p + 0x10)) + ((ld)I32(p + 0x1c) * t) * sinl(t * (float)6.28318548f));
        I32(m + 0x10) = (int32_t)truncl((ld)(I32(p + 8) - I32(p + 0x14)) * in + (ld)I32(p + 0x14));
    } else {
        const ld out = 1.0L - sinl(t * (float)1.57079637f), swing = 1.0L - cosl(t * (float)6.28318548f);
        I32(m + 8) = (int32_t)truncl(((ld)I32(p) - out * (ld)(I32(p) - I32(p + 0xc))) + ((ld)I32(p + 0x18) * t) * swing);
        I32(m + 0xc) = (int32_t)truncl(((ld)I32(p + 4) - (ld)(I32(p + 4) - I32(p + 0x10)) * out) + ((ld)I32(p + 0x1c) * t) * swing);
        I32(m + 0x10) = (int32_t)truncl((ld)I32(p + 8) - (ld)(I32(p + 8) - I32(p + 0x14)) * out);
        I32(m + 0x1c) = (int32_t)truncl((double)(out * (double)2048.0 * (double)0.15 + (double)I32(m + 0x1c)));
        U32(m + 0x1c) &= 0x7ff;
        I32(m + 0x14) = (int32_t)truncl((double)(out * (double)2048.0 * (double)0.05 + (double)I32(m + 0x14)));
        U32(m + 0x14) &= 0x7ff;
    }
    F32(p + 0x3c) = (float)(double)((double)F32(p + 0x3c) - (ld)F32(0x133350) * (double)0.55);
    if (F32(p + 0x3c) >= (double)0.05) return;
    if (0.0f > F32(p + 0x3c)) I32(p + 0x3c) = 0;
    if (I32(p + 0x48) == 0) {
        I32(p + 0x48) += 1;
        F32(p + 0x3c) = (float)(1.0L - F32(p + 0x3c));
        U32(p) = U32(m + 8); U32(p + 4) = U32(m + 0xc); U32(p + 8) = U32(m + 0x10);
        U32(p + 0x24) = U32(p + 0x30); U32(p + 0x28) = U32(p + 0x34); U32(p + 0x2c) = U32(p + 0x38);
        I32(p + 0x20) = 1500; I32(p + 0x1c) = 1500; I32(p + 0x18) = 1500;
        I32(p + 0xc) = (U8(p + 0x44) & 1) ? -6500 : 7000;
        I32(p + 0x10) = -1000; I32(p + 0x14) = -2000;
    } else F32(p + 0x40) = -1.0f;
}

/*
 * A LETTER OF THE NAME PAGE'S WHEEL (0x36245: its record, the letter chosen before and now, the
 * share of the turn between them t, a lift).  A letter not in use (+8 = -1) is hidden.  Else after
 * its wait (+0x24) it shows (its animation frame by +8: 0x9d8 x +8 + 0x226), its growth (+0x1c)
 * falls at 2 a second; its place round the wheel: radius 3500 + 5000 x growth^2, the angle from
 * the letter's distance (of 26, the turn's way) behind each of the two -- each letter 0.2417 of a
 * turn -- the shorter way round, t of the way (0x10388 turns (0, lift ? 200 : 0, r) by it), raised by
 * the lift.  Its own turn follows the angle (0x1016b at 0x37 while t is under 1), its tilt (+0x10)
 * swings to 2.83 (+0xc set) or -0.31 at 4 pi a second.
 */
#define HOOK_36245
static void FUN_00036245(int p, int was, int now, uint32_t r3, float t, int lift)
{
    typedef long double ld;
    (void)r3;
    const uint32_t slot = 0x131090u + 8u * U32(p);
    if (I32(p + 8) == -1) { FUN_0004c01c(0x11da14, 0x11d6f4, (int)slot); return; }
    F32(p + 0x24) = (float)((ld)F32(p + 0x24) - F32(0x133350));
    if (!(0.0f > F32(p + 0x24))) return;
    const uint32_t sp0 = s2_sp, fp = s2_push(0x60) + 0x60, out = fp - 0x54, in = fp - 0x44, turn = fp - 0x20;
    const uint32_t k = U32(slot);
    I32(p + 0x24) = 0;
    FUN_0004bfbe(0x11da14, 0x11d6f4, (int)slot);
    const uint32_t m = U32(0x11da20u + 0x1cu * k);
    I32(m + 0x20) = I32(p + 8) * 0x9d8 + 0x226;
    F32(p + 0x1c) = (float)((ld)F32(p + 0x1c) - (ld)F32(0x133350) * 2.0f);
    if (0.0f > F32(p + 0x1c)) I32(p + 0x1c) = 0;
    float a = (float)((ld)(float)(I32(p + 4) - 0x41) - (ld)was);
    if (0.0f > a) a = (float)((ld)a + 26.0f);
    float b = (float)((ld)(float)(I32(p + 4) - 0x41) - (ld)now);
    if (0.0f > b) b = (float)((ld)b + 26.0f);
    I32(in) = 0; I32(in + 4) = lift != 0 ? 200 : 0;
    I32(in + 8) = (int32_t)truncl((ld)F32(p + 0x1c) * F32(p + 0x1c) * 5000.0f + 3500.0f);
    float aa = (float)((ld)a * -(float)0x1.eeebf4p-3f), bb = (float)((ld)b * -(float)0x1.eeebf4p-3f);
    if (fabsl((ld)aa - bb) > (double)0x1.921fb6p+0) {
        if (aa > bb) aa = (float)((ld)aa + -(float)6.28318548f);
        else bb = (float)((ld)bb + -(float)6.28318548f);
    }
    FUN_00010388((int)out, (int)in, 0, 0, (float)((ld)bb - ((ld)bb - aa) * t));
    for (uint32_t c = 0; c < 3; c++) I32(m + 8 + 4 * c) = I32(out + 4 * c) + lift;
    F32(turn) = (float)((ld)I32(m + 0x18) * 0.0009765625f * (float)3.14159274f);
    if (s2_bits(t) < 0x3f800000) FUN_0001016b((int)turn, (float)((ld)a * (float)0x1.eeebf4p-3f), 0x37);
    else F32(turn) = (float)((ld)a * (float)0x1.eeebf4p-3f);
    U32(m + 0x18) = (uint32_t)(int32_t)truncl((ld)F32(turn) * 1024.0f / (float)3.14159274f) & 0x7ff;
    I32(p + 0x18) = 0;
    U32(m + 0x14) = (uint32_t)(int32_t)truncl((ld)F32(p + 0x10) * 1024.0f / (float)3.14159274f) & 0x7ff;
    U32(m + 0x1c) = (uint32_t)(int32_t)truncl((ld)F32(p + 0x18) * 1024.0f / (float)3.14159274f) & 0x7ff;
    if (I32(p + 0xc) == 0) {
        if (F32(p + 0x10) != (double)-0x1.41b2f8p-2) {
            F32(p + 0x10) = (float)((ld)F32(p + 0x10) - (ld)F32(0x133350) * (float)0x1.921fb6p+3f);
            if (!(F32(p + 0x10) >= (double)-0x1.41b2f8p-2)) U32(p + 0x10) = 0xbea0d97cu;
        }
    } else if (F32(p + 0x10) != (double)0x1.69e957p+1) {
        F32(p + 0x10) = (float)((ld)F32(0x133350) * (float)0x1.921fb6p+3f + F32(p + 0x10));
        if (F32(p + 0x10) > (double)0x1.69e957p+1) U32(p + 0x10) = 0x4034f4acu;
    }
    s2_sp = sp0;
}

/*
 * THE CHEAT WORDS (0x366a8): each key of 0x112f50 let go goes into the last five typed (0x128898,
 * a word each, ended by 0; 0x1288e8 how many).  Matched against the six words of 0x113020 (0x18
 * bytes each: five letters, +0xc the word it sets, +0x10 the value, +0x14 whether it is turned
 * rather than set): all five letters the same, the word is set and the typing starts again; a
 * typing that matches none so far starts again too.
 */
#define HOOK_366a8
static void FUN_000366a8(void)
{
    for (uint32_t i = 0; I32(0x112f50u + 4 * i) >= 0; i++) {
        if (I32(0x112f50u + 4 * i) == 0 || U8(0x100002u + i) == 0) {
            if (I32(0x1288ecu + 4 * i) == 1) {
                I32(0x1288ecu + 4 * i) = 0;
                U32(0x128898u + 4u * U32(0x1288e8)) = U32(0x112f50u + 4 * i);
                U32(0x12889cu + 4u * U32(0x1288e8)) = 0;
                if (I32(0x1288e8) < 4) I32(0x1288e8) += 1;
            }
        } else I32(0x1288ecu + 4 * i) = 1;
    }
    int missed = 0;
    for (uint32_t w = 0; w < 6; w++) {
        const uint32_t word = 0x113020u + 0x18u * w;
        for (uint32_t k = 0; U32(0x128898u + 4 * k) != 0; k++) {
            if (U8(word + k) != U8(0x128898u + 4 * k)) { missed++; break; }
            if (k == 4) {
                if (I32(word + 0x14) == 0) U32(U32(word + 0xc)) = U32(word + 0x10);
                else U32(U32(word + 0xc)) ^= U32(word + 0x10);
                I32(0x1288e8) = 0; U32(0x128898) = 0;
                break;
            }
        }
    }
    if (missed == 6) { I32(0x1288e8) = 0; U32(0x128898) = 0; }
}

/*
 * THE NETWORK GAME'S PAGES (0x368e2 .. 0x3770c).  The port has no network game: the network's own
 * routines (0xb5xxx) answer nothing, so these pages wait for no one; they are carried over as the
 * game has them.  A player's record (0xd8 bytes at 0x1324f0 + 0xd8 x n; a link game's two of 0x74 at
 * 0x132bb0): +4 the message (0x1234 joining, 0x5678 leaving, 0x9abc starting), the number, the
 * name (0x1289f4), team, car, the level's and mode's words, the controls' six keys (0x127590..),
 * the set-up (0x128aa4.. or 0x36cc5's), the players' network numbers.
 */
static uint32_t s2r_net_ready(void) { return I32(0x1324dc) == 1 ? I32(0x1289fc) == 0 : I32(0x128a28) == 0; }
#define HOOK_368e2
static int FUN_000368e2(int to_all, uint32_t message, uint32_t extra)
{
    const int me = FUN_000b5b11();
    const uint32_t rec = 0x1324f0u + 0xd8u * (uint32_t)me;
    I32(rec + 0x54) = I32(0x131f30);
    U32(rec + 4) = message;
    I32(rec + 8) = me;
    FUN_000b3be4(rec + 0x18, 0x1289f4u);
    I32(rec + 0x10) = I32(0x1289ec);
    U32(rec + 0xc) = U32(0x128a9cu + 0x188u * U32(0x1289ec));
    U32(0x13250cu + 0xd8u * U32(0x131f28)) = s2r_net_ready();     /* (the game's: by 0x131f28, not this player's number) */
    I32(rec + 0x24) = 1;
    U32(rec + 0x48) = extra;
    U32(rec + 0x44) = U32(0x131f1c);
    U32(rec + 0x40) = U32(0x131f20);
    U32(rec + 0x20) = U32(0x1324ec);
    U32(rec + 0x4c) = U32(0x128a38) | U32(0x128a34) << 8;
    U32(rec + 0x14) = U32(0x128a30) | U32(0x128a14) << 8;
    I32(rec + 0x50) = I32(0x131f2c);
    for (uint32_t k = 0; k < 6; k++) { U32(rec + 0x98 + 8 * k) = U32(0x127590u + 0x1c * k); U32(rec + 0x9c + 8 * k) = U32(0x12759cu + 0x1c * k); }
    if (I32(0x1324e8) < 0) { FUN_00036cc5((int)(rec + 0x28)); I32(rec + 0x30) = -1; }
    else FUN_000ae6e3(rec + 0x28, 0x128aa4u + 0x18u * U32(0x1324e8) + 0x60u * U32(rec + 0xc) + 0x188u * U32(0x1289ec), 0x18);
    for (uint32_t k = 0; k < 4; k++) FUN_000ae8b0(rec + 0x58 + 0x10 * k, 0, 0x10);
    (void)FUN_000b5bc4();
    for (int k = 0; k < I32(0x131f2c); k++) FUN_000ae6e3(rec + 0x58 + 0x10u * (uint32_t)k, 0x1325b8u + 0xd8u * (uint32_t)k, 0x10);
    if (I32(0x131f30) == 0) FUN_000ae6e3(0x132850u + 0xd8u * (uint32_t)me, rec, 0xd8);
    if (to_all == 0) { for (int k = 0; k < I32(0x131f2c); k++) if (k != me) (void)FUN_000b5e08(); }
    else (void)FUN_000b5e08();
    return me;
}

/* THE LINK GAME'S HANDSHAKE (0x36d22: the message, whole record or not, a "waiting" page first):
   this player's record sent and the other's taken, again until it comes or ESC (0x131f20 = 0x20) */
#define HOOK_36d22
static bool FUN_00036d22(uint32_t message, int whole, int page)
{
    if (page != 0) {
        FUN_00051aa4(U32(0x134794), 64000u, 0);
        FUN_0003327c(U32(0x128070), 100, 1, 1, 2);
        FUN_0007f5a2(0, 0); FUN_0007f5a2(0, 0);
    }
    int failed, stop;
    do {
        (void)FUN_000b52f1(2);
        const uint32_t me = 0x132bb0u + 0x74u * U32(0x131f28), other = 0x132bb0u + 0x74u * (1u - U32(0x131f28));
        U32(me) = message;
        FUN_000b3be4(me + 0x10, 0x1289f4u);
        I32(me + 8) = I32(0x1289ec);
        U32(me + 4) = U32(0x128a9cu + 0x188u * U32(0x1289ec));
        U32(me + 0x14) = s2r_net_ready();
        U32(me + 0x1c) = 1;
        U32(me + 0x24) = U32(0x131f1c);
        U32(me + 0x20) = U32(0x131f20);
        U32(me + 0x18) = U32(0x1324ec);
        U32(me + 0x28) = U32(0x128a38) | U32(0x128a34) << 8;
        U32(me + 0xc) = U32(0x128a30) | U32(0x128a14) << 8;
        for (uint32_t k = 0; k < 6; k++) { U32(me + 0x44 + 8 * k) = U32(0x127590u + 0x1c * k); U32(me + 0x48 + 8 * k) = U32(0x12759cu + 0x1c * k); }
        if (I32(0x1324e8) < 0) { FUN_00036cc5((int)(me + 0x2c)); I32(me + 0x34) = -1; }
        else FUN_000ae6e3(me + 0x2c, 0x128aa4u + 0x18u * U32(0x1324e8) + 0x60u * U32(me + 4) + 0x188u * U32(0x1289ec), 0x18);
        const uint32_t n = whole ? 0x74 : 0x2c;
        (void)n;                                 /* (the network's routines, answering nothing, take no notice) */
        (void)FUN_000b5383(me, n);
        const int got = FUN_000b5827(other, me, n);
        failed = got == 0 || got < 0;
        const uint32_t said = U32(other);
        if (said == 0xace0) failed = 1;
        if (!failed) stop = 1;
        else {
            FUN_0004cca5(said == 0xace0, 0);
            if (U8(0x100003) != 0) U32(0x131f20) = 0x20;
            stop = U8(0x100003) != 0 || said == 0xace0;
        }
    } while (failed && !stop);
    return failed;
}

/*
 * THE NETWORK GAME'S WAITING PAGE (0x371fd): this player joins (0x368e2, 0x1234); the messages that
 * come are taken (a player joining, leaving, the game starting -- 0x1324c8); then the players (up
 * to four) listed with their teams' badges, and, with more than one and someone ready, the "start"
 * line; the host starting (0x131f1c with 0x131f20 = 0x10) starts it for all.  1 when the game is
 * full or this player was dropped.
 */
#define HOOK_371fd
static uint64_t FUN_000371fd(uint32_t r0, uint32_t r1)
{
    (void)r0;
    const uint32_t sp0 = s2_sp, fp = s2_push(0x190) + 0x190, rec = fp - 0x190, len = fp - 0x30, text = fp - 0xb8;
    (void)len;
    uint32_t result = 0;
    FUN_0003327c(U32(0x128060), 4, 0, 1, 2);
    I32(0x131f2c) = FUN_000b5b0b();
    I32(0x131f28) = FUN_000368e2(1, 0x1234u, 0);
    int waiting = 1;
    while (FUN_000b5d84(rec, 0xc, len) != 0 && waiting) {
        (void)FUN_000b68c5(I32(len), rec + 0xc, 0xcc);
        const uint32_t message = U32(rec + 4);
        if (message == 0x1234) {
            if (I32(rec + 0x50) > 3) {
                int known = 0;
                (void)FUN_000b5bc4();
                for (uint32_t k = 0; k < 4; k++) if (FUN_000b7356(0x1325b8u + 0xd8u * U32(0x131f28), rec + 0x58 + 0x10 * k, 0x10) == 0) known = 1;
                if (!known) { result = 1; goto done; }
            }
            if (I32(rec + 0x50) != 1 || I32(0x131f2c) != 4) {
                const int was = I32(0x131f2c);
                (void)FUN_000b5bdf(); (void)FUN_000b5b17();
                I32(0x131f2c) = FUN_000b5b0b();
                const int k = FUN_000b5ca0();
                if (k > 3) { result = 1; goto done; }
                if (k < was && was != I32(0x131f2c)) (void)FUN_000b7374(0x1324f0u + 0xd8u * (uint32_t)(k + 1), 0x1324f0u + 0xd8u * (uint32_t)k, 0xd8u * (uint32_t)(was - k));
                I32(0x131f28) = FUN_000b5b11();
                if (FUN_000b7356(0x1325b8u + 0xd8u * U32(0x131f28), rec + 0xc8, 0x10) != 0) FUN_000ae6e3(0x1324f0u + 0xd8u * (uint32_t)k, rec, 0xd8);
                (void)FUN_000b5fa4();
            }
        } else if (message == 0x5678) {
            if (FUN_000b5ca0() != -1) { (void)FUN_000b5c63(); (void)FUN_000b5b17(); (void)FUN_000b5fa4(); }
        } else if (message == 0x9abc) {
            U32(0x1324c8) = 1;
            FUN_000368e2(0, 0x9abcu, 0);
            waiting = 0;
        } else (void)FUN_000b5fa4();
    }
    I32(0x131f2c) = FUN_000b5b0b();
    I32(0x131f28) = FUN_000b5b11();
    {
        const int n = I32(0x131f2c) > 4 ? 4 : I32(0x131f2c);
        uint32_t y = 0x37, ready = 0;
        for (int k = 0; k < n; k++, y += 0x16) {
            FUN_0004eaed(text, 0x11114eu, (uint32_t)k + 0x31u, 0x132508u + 0xd8u * (uint32_t)k);
            FUN_0003327c(text, y, 1, 1, 2);
            FUN_00050c0a(0x12420c, 0xbe, (int)y, I32(0x132500u + 0xd8u * (uint32_t)k) + 0x51, 0);
        }
        FUN_0003327c(U32(0x128064), 0xa8, 2, 1, 2);
        for (int k = 0; k < n; k++) ready |= U32(0x132514u + 0xd8u * (uint32_t)k);
        if (I32(0x131f2c) > 1 && ready != 0) FUN_0003327c(U32(0x127f14), 0xb8, 3, 3, 2);
        if (I32(0x131f1c) != 0 && I32(0x131f20) == 0x10 && I32(0x131f2c) > 1 && ready != 0) {
            U32(0x1324c8) = 1; I32(0x131f1c) = 0;
            FUN_000368e2(0, 0x9abcu, 0);
        }
    }
done:
    s2_sp = sp0;
    return (uint64_t)r1 << 32 | result;
}

/* THE LINK GAME'S PAGE (0x3770c): the handshake (starting, leaving or joining), what the other
   said done (starting; leaving: the network's game set up again with a "NET_SPACE" block); the two
   players listed with their badges and the "start" line */
#define HOOK_3770c
static void FUN_0003770c(void)
{
    const uint32_t sp0 = s2_sp, text = s2_push(0x120) + 0x120 - 0x120;
    FUN_0003327c(U32(0x128060), 4, 0, 1, 2);
    if (I32(0x131f1c) != 0 && I32(0x131f20) == 0x10) {
        U32(0x1324c8) = 1; I32(0x131f1c) = 0;
        FUN_00036d22(0x9abcu, 0, 0);
    } else if (I32(0x131f1c) != 0 && I32(0x131f20) == 0x20) {
        U32(0x1324c8) = 0; U32(0x1324c4) = 0;
        FUN_00036d22(0x5678u, 0, 0);
        (void)FUN_000b4d73();
        if (FUN_0004f0c6(U32(0x11d6ec), 0x111164u, 0x4000) == 0) (void)FUN_0004f033(U32(0x11d6ec));
        s2_sp = sp0;
        return;
    } else FUN_00036d22(0x1234u, 0, 0);
    const uint32_t said = U32(0x132bb0u + 0x74u * (1u - U32(0x131f28)));
    if (said == 0x9abc) { U32(0x1324c8) = 1; I32(0x131f1c) = 0; }
    else if (said == 0x5678) {
        U32(0x1324c8) = 0; U32(0x1324c4) = 0;
        (void)FUN_000b4d73();
        if (FUN_0004f0c6(U32(0x11d6ec), 0x11116eu, 0x4000) == 0) (void)FUN_0004f033(U32(0x11d6ec));
    }
    uint32_t y = 0x37;
    for (int k = 0; k < I32(0x131f2c); k++, y += 0x16) {
        FUN_0004eaed(text, 0x111178u, (uint32_t)k + 0x31u, 0x132bc0u + 0x74u * (uint32_t)k);
        FUN_0003327c(text, y, 1, 1, 2);
        FUN_00050c0a(0x12420c, 0xbe, (int)y, I32(0x132bb8u + 0x74u * (uint32_t)k) + 0x51, 0);
    }
    FUN_0003327c(U32(0x127f14), 0xb8, 3, 3, 2);
    s2_sp = sp0;
}

/* A NETWORK MESSAGE IN THE RACE'S START (0x379bf): a player leaving (0x5678) -- dropped, 1; a
   player's record of this start (0x131f30) from another: taken (0x2468, its keys 0; 0xdef0) */
#define HOOK_379bf
static uint64_t FUN_000379bf(uint32_t r0, uint32_t r1)
{
    (void)r0;
    const uint32_t sp0 = s2_sp, fp = s2_push(0x100) + 0x100, rec = fp - 0xfc, len = fp - 0x20;
    (void)len;
    uint32_t result = 0;
    if (FUN_000b5d84(rec, 0xc, len) != 0) {
        (void)FUN_000b68c5(I32(len), rec + 0xc, 0xcc);
        const uint32_t message = U32(rec + 4);
        if (message == 0x5678) {
            if (FUN_000b5ca0() != -1) { (void)FUN_000b5c63(); (void)FUN_000b5b17(); }
            I32(0x131f2c) = FUN_000b5b0b();
            result = 1;
        } else if (I32(0x131f30) == I32(rec + 0x54) && I32(rec + 8) != I32(0x131f28)) {
            if (message == 0x2468 || message == 0xdef0) {
                const uint32_t k = (uint32_t)FUN_000b5ca0();
                FUN_000ae6e3(0x1324f0u + 0xd8u * k, rec, 0xd8);
                if (message == 0x2468) { U32(0x132534u + 0xd8u * k) = 0; U32(0x132530u + 0xd8u * k) = 0; }
            } else (void)FUN_000b5fa4();
        }
    }
    s2_sp = sp0;
    return (uint64_t)r1 << 32 | result;
}

/* THE NETWORK RACE'S START (0x37b41: the message, its word, a "waiting" page first): this player's
   record sent again every ten ticks or so until every other one has sent the same for this start;
   ESC or a player dropped gives 1 */
#define HOOK_37b41
static uint32_t FUN_00037b41(int message, int word, int page, uint32_t r3)
{
    (void)r3;
    if (page != 0) {
        FUN_00051aa4(U32(0x134794), 64000u, 0);
        FUN_0003327c(U32(0x128074), 100, 1, 1, 2);
        FUN_000ae6e3(0x1338ccu, 0x132c98u, 0x300);
        FUN_00051664(0x1338ccu, 0);
        FUN_0007f5a2(0, 0); FUN_0007f5a2(0, 0);
    }
    for (int k = 0; k < I32(0x131f2c); k++) { const uint32_t rec = 0x1324f0u + 0xd8u * (uint32_t)k; U32(rec + 4) = 0; U32(rec + 0x48) = 0; U32(rec + 0x54) = 0; }
    FUN_000368e2(0, (uint32_t)message, (uint32_t)word);
    uint32_t since = U32(0x4ce0a);
    (void)since;
    (void)FUN_0004ccfd(U32(0x4ce0a));
    for (;;) {
        const uint32_t gap = (U32(0x4ce0a) & 3) + 10;
        const uint64_t ticks = FUN_0004abf5(since, U32(0x4ce0a), gap);
        if (gap < (uint32_t)(ticks & 0xffff)) {
            since = U32(0x4ce0a);
            for (int k = 0; k < I32(0x131f2c); k++) if (k != I32(0x131f28)) (void)FUN_000b5e08();
            FUN_000368e2(0, (uint32_t)message, (uint32_t)word);
            if (U8(0x100003) != 0) { U32(0x131f20) = 0x20; return 1; }
        }
        FUN_0004cca5(0, 0);
        if ((uint32_t)FUN_000379bf(0, 0) != 0) return 1;
        int waiting = 0;
        for (int k = 0; k < I32(0x131f2c); k++) {
            const uint32_t rec = 0x1324f0u + 0xd8u * (uint32_t)k;
            if (k != I32(0x131f28) && (I32(rec + 4) != message || I32(rec + 0x48) != word || I32(0x131f30) != I32(rec + 0x54))) { waiting = 1; break; }
        }
        if (!waiting) {
            FUN_000ae6e3(0x132850u + 0xd8u * U32(0x131f28), 0x1324f0u + 0xd8u * U32(0x131f28), 0xd8);
            I32(0x131f30) += 1;
            return 0;
        }
    }
}

/*
 * THE MENU'S KEYS (0x38996: whether ENTER counts from the controller): the game controller first,
 * when there is one (0x38b2c: a new press 1, held 0); then the keyboard -- ESC, ENTER, down, up,
 * left, right in that order (0x20 0x10 2 1 4 8 into 0x131f20): 1 for a key newly down, 0 when it is
 * still held or none is (then 0x131f20 = 0).
 */
#define HOOK_38996
static uint64_t FUN_00038996(int enter, uint32_t r1)
{
    static const struct { uint32_t key, bit; } keys[6] = { { 0x01, 0x20 }, { 0x1c, 0x10 }, { 0x50, 2 }, { 0x48, 1 }, { 0x4b, 4 }, { 0x4d, 8 } };
    uint32_t result = 0;
    const int pad = I32(0x11f298) != 0 ? (int)FUN_00038b2c(0, 0, enter) : 0;
    if (pad == 1) result = 1;
    else if (pad == -1) result = 0;
    else {
        int i = 0;
        while (i < 6 && U8(0x100002u + keys[i].key) == 0) i++;
        if (i == 6) { I32(0x131f20) = 0; result = 0; }
        else if (U32(0x131f20) == keys[i].bit) result = 0;
        else { U32(0x131f20) = keys[i].bit; result = 1; }
    }
    return (uint64_t)r1 << 32 | result;
}

/*
 * THE CONTROLLER AS THE MENU'S KEYS (0x38b2c: the player, whether every control counts, whether
 * ENTER does): the player's six controls (their records at 0x127708 + 0xc0 x player, 0x1c each:
 * +4 a button 0 / an axis 1, +8 kept for the race (1), +0x14 the axis' way; their values +0xa8..)
 * as ENTER, ESC, left, right, down, up.  A button down, or an axis over 0x300 its way: that key
 * into 0x131f20 + 4 x player -- 1 when new, -1 when held.  0 for none.
 */
#define HOOK_38b2c
static uint32_t FUN_00038b2c(int player, int every, int enter)
{
    static const uint32_t bit[6] = { 0x10, 0x20, 4, 8, 2, 1 };
    const uint32_t rec = 0x127708u + 0xc0u * (uint32_t)player, held = 0x131f20u + 4u * (uint32_t)player;
    for (uint32_t i = 0; i < 6; i++) {
        const uint32_t c = rec + 4 + 0x1c * i;
        const int32_t v = I32(rec + 0xa8 + 4 * i);
        if (!(I32(c + 4) != 1 || every != 0 || (bit[i] == 0x10 && enter != 0))) continue;
        int on = 0;
        if (I32(c) == 0 && v != 0) on = 1;
        else if (I32(c) == 1 && (int32_t)FUN_000b73c7((uint32_t)v) > 0x300 && (I32(c + 0x10) < 0 ? -1 : 1) == (v < 0 ? -1 : 1)) on = 1;
        if (on) {
            if (U32(held) != bit[i]) { U32(held) = bit[i]; return 1; }
            return 0xffffffffu;
        }
    }
    return 0;
}

/* THE NETWORK'S "NO CONNECTION" PAGE (0x41fcf): backdrop "bkg_6L.bkk"; while the network cannot be
   opened (0x3162c) its two lines are shown until ESC (then back to the start: 0x132494, 0x1324d8,
   0x1324e0 nought); the page's end (0x358b9, 0x44f72) */
#define HOOK_41fcf
static void FUN_00041fcf(void)
{
    FUN_0004eaed(0x130bc8u, 0x111542u, 0x4cu);
    FUN_00044587(0x113374);
    for (;;) {
        if (FUN_0003162c(0, 0) == 0) goto end;
        FUN_00051b02(0, 0, U32(0x1323c8) + 8, U32(U32(0x1323c8)));
        FUN_0003327c(U32(0x12815c), 0x50, 1, 1, 2);
        FUN_0003327c(U32(0x128160), 100, 1, 1, 2);
        FUN_0007f5a2(0, 0);
        FUN_0004cca5(0, 0);
        if (U8(0x100003) != 0) break;
    }
    I32(0x132494) = 0; U32(0x1324d8) = 0; U32(0x1324e0) = 0;
end:
    FUN_000358b9();
    FUN_00044f72(0x113374, 0);
}

/*
 * THE ATTRACT RACE STARTED FROM THE MENU (0x3c9ac): the page ended, the CD stopped, the texts of
 * "attract" read (into 0x1286a8); mode 0x80, four cars on the track of the turn (0x113224 by
 * 0x128a58, which moves on round six), the level 0, the sound's and CD's settings from the menu's;
 * car n = model n, colour n, set-ups all 2 (-1 the third); the fades at 0.  The race (0x2b2a8; an
 * error but 0x11d7 stops the game); back to the title page (0x3ff37); the texts let go; the CD
 * started again (track 2) unless the menu's music is off.
 */
#define HOOK_3c9ac
static void FUN_0003c9ac(void)
{
    FUN_00044f72(0x11343c, 0);
    (void)FUN_0004d738(0, 0);
    FUN_0003248f(0x11132eu, 0x1286a8);
    U32(0x130d10) = 0x80;
    U8(0x130d34) = 0;
    U32(0x130c50) = 0;
    U32(0x130c48) = 4;
    U32(0x130c4c) = U32(0x113224u + 4u * U32(0x128a58));
    U32(0x130d4c) = 0;
    I32(0x130d5c) = I32(0x128a5c);
    U32(0x130d60) = U32(0x128a60);
    U32(0x130d58) = U32(0x128a64);
    I32(0x130c54) = I32(0x128a2c);
    U32(0x130d64) = 1;
    for (uint32_t i = 0; i < 4; i++) {
        const uint32_t e = 0x130c58u + 0x2cu * i;
        U32(e) = i; U32(e + 4) = i;
        I32(e + 0x14) = 2; I32(e + 0x18) = 2; I32(e + 0x1c) = -1; I32(e + 0x20) = 2; I32(e + 0x24) = 2; I32(e + 0x28) = 2;
    }
    F32(0x127ed4) = 0.0f; U32(0x127ed0) = 0; U32(0x127ecc) = 0;
    U32(0x13249c) = 0; U32(0x132498) = 0;
    const int r = (int)FUN_0002b2a8(0, 0);
    if (r != 0 && r != 0x11d7) { (void)FUN_0004b169(3); FUN_0004b1a4(r); FUN_000ae662((uint32_t)r); }
    I32(0x128a58) += 1;
    if (I32(0x128a58) > 5) I32(0x128a58) = 0;
    I32(0x132494) = 0x3ff37;
    (void)FUN_0004f331(U32(0x11d6ec), 0x1286a8);
    (void)FUN_0004d064(0, 0); (void)FUN_0004d064(0, 0);
    if (I32(0x4d900) == 0) {
        FUN_0004d7c0(I32(0x130d5c) >> 7, I32(0x130d5c) >> 7);
        if (I32(0x128a44) == 0) { (void)FUN_0004d738(0, 0); FUN_0004d5b4(2, 1); }
    }
}

/*
 * THE LEAGUE'S PAGE (0x394f5): the drawing list emptied; backdrop "leag_<league>L.bkk"; the seven
 * league models taken and hidden, their animations at the start; the league's cars (0x1130b0 of
 * them) set off (0x35b86).  A frame: input, backdrop, each car shown and moved (0x35cb0); ENTER
 * (once the fade-in is done) goes on to 0x3edb4, ESC back (0x431b3); every car arrived goes on too.
 * The 3D view from the camera record 0x131ea4 (y 100, z 1300), the list drawn, the top band darkened
 * with the heading and the league's name; the fade-in at 2 a second.  Until the fade's end
 * (0x1324a0), then the page's end.
 */
#define HOOK_394f5
static void FUN_000394f5(void)
{
    FUN_0004bda7(0x11da14, 0x11d6f4);
    I32(0x132494) = 0;
    FUN_0004eaed(0x130bc8u, 0x11120au, U32(0x128a10) + 0x31u, 0x4cu);
    FUN_00044587(0x113410);
    for (uint32_t i = 0; i < 7; i++) {
        const uint32_t slot = 0x130fa8u + 8 * i, rec = 0x11da14u + 0x1cu * (U32(0x130fa8) + i);
        FUN_0004be04(0x11da14, 0x11d6f4, (int)slot);
        U32(U32(rec + 0xc) + 0x3c) = 0;
        U32(U32(rec + 0xc) + 0x20) = 0;
        FUN_0004c01c(0x11da14, 0x11d6f4, (int)slot);
    }
    const uint32_t cars = 0x1130b0u + 0x1cu * U32(0x128a10);
    for (int i = 0; i < I32(cars); i++) FUN_00035b86((int)(0x131290u + 0x4cu * (uint32_t)i), i);
    do {
        FUN_00038d48();
        FUN_00051b02(0, 0, U32(0x1323c8) + 8, U32(U32(0x1323c8)));
        for (int i = 0; i < I32(cars); i++) {
            FUN_0004bfbe(0x11da14, 0x11d6f4, (int)(0x130fa8u + 8u * (uint32_t)i));
            FUN_00035cb0((int)(0x131290u + 0x4cu * (uint32_t)i));
        }
        const int key = (int)FUN_00038996(1, U32(0x128a10) * 0x1c) != 0 && (U32(0x132498) & 0x7fffffff) == 0;
        I32(0x131f1c) = key;
        if (key && U32(0x131f20) > 0xf) {
            if (U32(0x131f20) < 0x11) { U32(0x132498) = 0x3f800000u; I32(0x132494) = 0x3edb4; }
            else if (U32(0x131f20) == 0x20) { U32(0x1324a0) = 0x3f800000u; U32(0x132498) = 0x3f800000u; I32(0x132494) = 0x431b3; }
        }
        if ((uint32_t)FUN_00036150(0, 0x131290u) != 0) { U32(0x132498) = 0x3f800000u; I32(0x132494) = 0x3edb4; }
        U32(0x131eb0) = 0; U32(0x131eb4) = 100; I32(0x131eb8) = 0x514;
        U32(0x131eac) = 0; U32(0x131ea8) = 0; U32(0x131ea4) = 0;
        FUN_0007e8b4(0x131ea4, 0);
        FUN_000753a0(0x11d6f4u, 0);
        FUN_00051698(0, 0, 0x140, 0x2a);
        FUN_0003327c(U32(0x127f9c), 2, 1, 1, 2);
        FUN_0003327c(U32(0x127f3cu + 8u * U32(0x128a10)), 0x14, 1, 1, 2);
        if ((U32(0x132498) & 0x7fffffff) != 0) F32(0x13249c) = (float)((long double)F32(0x133350) * 2.0f + F32(0x13249c));
        if (I32(0x13249c) >= 0x3f800000) { F32(0x13249c) = 1.0f; U32(0x1324a0) = 0x3f800000u; }
        if (fabsf(F32(0x13249c)) != 0.0f) FUN_000512f1(F32(0x13249c), 0, 0, 0);
        FUN_0007f5a2(0, 0);
    } while ((U32(0x1324a0) & 0x7fffffff) == 0);
    FUN_000358b9();
    FUN_00044f72(0x113410, 0);
}

/*
 * THE MAIN MENU (0x38d8d): five items in the room (0x132378: arcade, championship, time attack,
 * network, options), each with a camera place (0x131f38.., 0xc each) and what it looks at
 * (0x131fec..).  The camera starts four times as far out again and flies in (0x33635 at 0x5f); the
 * room's palette, the "see-through" table.  A frame: input, the room's animation clock (0.2 a
 * second, round one); left / right move along the items (a sound, the camera at 0x55 to the new
 * item's place, turned by 0x1324a4 of a turn about what it looks at); ENTER picks (a sound; the
 * page and the kind of game: 0x3b674 arcade, 0x431b3 championship, 0x3f72a time attack, 0x3bf06
 * network, 0x420c0 options), ESC goes back to the title (0x3ff37).  The view turns smoothly
 * (0x1016b) to where it looks; the room drawn, the bottom band darkened with the heading and the
 * item's name, the arrows blinking; the fade (out at 2 a second once picked).  The page's end.
 */
#define HOOK_38d8d
static void FUN_00038d8d(void)
{
    typedef long double ld;
    const uint32_t sp0 = s2_sp, fp = s2_push(0x60) + 0x60;
    const uint32_t turn_y = fp - 0x24, turn_x = fp - 0x20, in = fp - 0x3c, out = fp - 0x48;
    int speed = 0x5f;
    float clock = 0.0f;
    FUN_0004bda7(0x11da14, 0x11d6f4);
    FUN_00044587(0x113288);
    I32(0x132494) = 0;
    U32(0x1324c0) = (uint32_t)FUN_0003348f(6, 6, 0) + 4;
    {
        const uint32_t at = 0x131f38u + 0xcu * U32(0x132378), to = 0x131fecu + 0xcu * U32(0x132378);
        for (uint32_t k = 0; k < 3; k++) I32(0x131eb0 + 4 * k) = I32(at + 4 * k) + (I32(at + 4 * k) - I32(to + 4 * k)) * 4;
        FUN_000338ed(0x1324a8, to, 0x131eb0);
    }
    F32(turn_y) = F32(0x1324ac); F32(turn_x) = F32(0x1324a8);
    U32(0x131ea4) = 0;
    U32(0x131ea8) = (uint32_t)(int32_t)truncl((ld)F32(0x1324ac) * 1024.0f / (float)3.14159274f) & 0x7ff;
    U32(0x131eac) = (uint32_t)(int32_t)truncl((ld)F32(0x1324a8) * 1024.0f / (float)3.14159274f) & 0x7ff;
    FUN_000ae6e3(0x1338ccu, 0x132c98u, 0x300);
    FUN_00051664(0x1338ccu, 0);
    (void)FUN_0005105e(0x12420cu, 0x1338cc);
    FUN_000504f2(0x1338cc, -1, -1, -1, -1, -1, -1);
    do {
        FUN_00038d48();
        clock = (float)(double)((ld)F32(0x133350) * (double)0.2 + (double)clock);
        if (s2_bits(clock) >= 0x3f800000) clock = (float)((ld)clock + -1.0f);
        I32(0x1347a0) = (int32_t)truncl((ld)clock * 65535.0f);
        const int key = (int)FUN_00038996(0, 0) != 0 && (U32(0x132498) & 0x7fffffff) == 0;
        I32(0x131f1c) = key;
        if (key) {
            const uint32_t k = U32(0x131f20);
            if (k == 4 && I32(0x132378) != 0) { FUN_000aea54(U32(0x124274), 5, 0, 0, 0, 1); I32(0x132378) -= 1; U32(0x4ce0a) = 0; speed = 0x55; }
            else if (k == 8 && I32(0x132378) < 4) { FUN_000aea54(U32(0x124274), 5, 0, 0, 0, 1); I32(0x132378) += 1; U32(0x4ce0a) = 0; speed = 0x55; }
            else if (k == 0x10) {
                FUN_000aea54(U32(0x124274), 7, 0, 0x5622, 0, 1);
                switch (I32(0x132378)) {
                case 0: I32(0x132494) = 0x3b674; U32(0x132498) = 0x3f800000u; U32(0x1324dc) = 1; U32(0x1324e4) = 0; break;
                case 1: I32(0x132494) = 0x431b3; U32(0x1324d4) = 0; U32(0x132498) = 0x3f800000u; U32(0x1324dc) = 0; U32(0x1324e4) = 0; break;
                case 2: I32(0x132494) = 0x3f72a; U32(0x132498) = 0x3f800000u; U32(0x1324dc) = 2; U32(0x1324e4) = 0; break;
                case 3: I32(0x132494) = 0x3bf06; U32(0x132498) = 0x3f800000u; break;
                case 4: I32(0x132494) = 0x420c0; U32(0x132498) = 0x3f800000u; break;
                default: break;
                }
            } else if (k == 0x20) { U32(0x1324a0) = 0x3f800000u; I32(0x132494) = 0x3ff37; }
        }
        const uint32_t at = 0x131f38u + 0xcu * U32(0x132378), to = 0x131fecu + 0xcu * U32(0x132378);
        if (fabsf(F32(0x13249c)) == 0.0f) {
            I32(in) = I32(at) - I32(to); U32(in + 4) = U32(at + 4); I32(in + 8) = I32(at + 8) - I32(to + 8);
            FUN_00010388((int)out, (int)in, 0, 0, (float)((ld)F32(0x1324a4) * (float)6.28318548f));
            I32(in) = I32(to) + I32(out); I32(in + 8) = I32(to + 8) + I32(out + 8);
            FUN_00033635(0x131eb0, (int)in, speed);
            FUN_000338ed(0x1324a8, to, at);
        } else {
            FUN_00033635(0x131eb0, (int)to, 0x62);
            FUN_000338ed(0x1324a8, to, 0x131eb0);
        }
        FUN_000366a8();
        FUN_0001016b((int)turn_y, F32(0x1324ac), (int32_t)truncl((ld)F32(0x13249c) * 12.0f + 85.0f));
        FUN_0001016b((int)turn_x, F32(0x1324a8), (int32_t)truncl((ld)F32(0x13249c) * 12.0f + 85.0f));
        U32(0x131ea4) = (uint32_t)(int32_t)truncl((ld)F32(turn_x) * 1024.0f / (float)3.14159274f) & 0x7ff;
        U32(0x131ea8) = (uint32_t)(int32_t)truncl((ld)F32(turn_y) * 1024.0f / (float)3.14159274f) & 0x7ff;
        U32(0x131eac) = 0;
        FUN_0007e8b4(0x131ea4, 0);
        FUN_000753e8(0x11d6f4u, 0);
        FUN_00051698(0, 0xaa, 0x140, 0x1e);
        FUN_0003327c(U32(0x127eec), 4, 0, 1, 2);
        FUN_0003327c(U32(0x127ef4u + 4u * U32(0x132378)), 0xaf, 1, 1, 2);
        if ((U32(0x4ce0a) & 0x7f) < 0x40) {
            if (I32(0x132378) != 0) FUN_00050c0a(0x12420c, 10, 0xac, 1, 0);
            if (I32(0x132378) < 4) FUN_00050c0a(0x12420c, 0x12a, 0xac, 2, 0);
        }
        if (fabsf(F32(0x13249c)) != 0.0f) FUN_000512f1(F32(0x13249c), 0, 0, 0);
        if ((U32(0x132498) & 0x7fffffff) != 0) {
            F32(0x13249c) = (float)((ld)F32(0x133350) * 2.0f + F32(0x13249c));
            if (I32(0x13249c) >= 0x3f800000) U32(0x1324a0) = 0x3f800000u;
        }
        FUN_0007f5a2(0, 0);
    } while ((U32(0x1324a0) & 0x7fffffff) == 0);
    FUN_000358b9();
    FUN_00044f72(0x113288, 0);
    s2_sp = sp0;
}

/*
 * THE GAME'S MENU (0x38540).  From the title (0x3c9ac first: the attract race), a page at a time:
 * each page's routine is the next one's address (0x132494), called in turn until one sets 0.  The
 * page's backdrop record (0x1323c8, a block of 0x20) and the texts' table (0x1323cc..) made fresh,
 * the settings read (0x32d53).  A race asked for (0x1324e0): the 3D's fade steps a third as long
 * meanwhile (0x7e428's +8 / +0xc); the replay of a best time (-1) or the race of the choices made
 * (0x339e7) -- with the loading page (0x34ec6) -- then on to the page after it: the results
 * (0x43914; arcade games counted 0x128a1c), arcade's (0x3b674) or the network's pages, the time
 * attack's (0x3cbb8), the championship's (0x3988d); the sound's settings kept.  The CD's music
 * started again (track 2), the settings saved (0x331dc), the backdrop let go -- until leaving the
 * game (0x1324d8 = 0).
 * The port's: the race and the attract race are run by the port's other part; the menu comes back
 * here after them (s2m_resume: 1 a race, 2 the attract race, 3 a replay).
 */
#define HOOK_38540
static void FUN_00038540(void)
{
    const uint32_t fade = U32(0x7e428);
    int r = 0;
    if (s2m_resume == 2) { s2m_resume = 0; s2m_attract_done(); goto after_attract; }
    if (s2m_resume == 3) { s2m_resume = 0; r = 0; goto after_replay; }
    if (s2m_resume) { s2m_resume = 0; r = 0; goto after_race; }
    I32(0x1324d8) = 1;
    I32(0x132494) = 0x3c9ac;
    for (uint32_t k = 0; k < 3; k++) U32(0x132378 + 4 * k) = 0;
    U32(0x1324e8) = 0; I32(0x1324e0) = 0; U32(0x1324dc) = 0;
    I32(fade + 8) *= 3; I32(fade + 0xc) *= 3;
    U32(0x131f28) = 0;
    do {
        U32(0x113740) = 0; U32(0x113744) = 1;
        {
            const uint32_t b = (uint32_t)FUN_000ae698(0x20);
            I32(0x1323c8) = b == 0 ? 0 : (int)(uint32_t)FUN_000514fa((int)b, 0, 0, 0);
        }
        for (uint32_t k = 0; k < 0x32; k++) U32(0x1323cc + 4 * k) = 0;
        FUN_00032d53();
        if (I32(0x132494) == 0x3c9ac) FUN_00041fcf();
after_attract:
        while (I32(0x132494) != 0) s2_dispatch(U32(0x132494));
        if (I32(0x1324e0) == 0) FUN_00044f72(0x11343c, 0);
        else {
            I32(fade + 8) /= 3; I32(fade + 0xc) /= 3;
            (void)FUN_0004d738(0, 0);
            if (I32(0x1324d8) == 0) FUN_00044f72(0x11343c, 0);
            else if (I32(0x1324e0) == -1) {
                FUN_00041fcf();
                U32(0x130d10) = 1;
                I32(0x1324dc) = -1;
                U32(0x130c4c) = U32(0x113224u + 4u * U32(0x128a54));
                FUN_00034ec6();
                FUN_00044f72(0x11343c, 0);
                r = (int)FUN_0002b2a8(0, 0);
after_replay:
                if (r != 0 && r != 0x11d7) { (void)FUN_0004b169(3); FUN_0004b1a4(r); FUN_000ae662((uint32_t)r); }
                I32(0x132494) = 0x4254f;
                U32(0x1324dc) = 0;
            } else {
                FUN_000339e7();
                FUN_00041fcf();
                FUN_00034ec6();
                FUN_00044f72(0x11343c, 0);
                r = (int)FUN_0002b2a8(0, 0);
after_race:
                if (r != 0 && r != 0x11d7) { (void)FUN_0004b169(3); FUN_0004b1a4(r); FUN_000ae662((uint32_t)r); }
                U32(0x128a68) = U32(0x130d68);
                I32(0x128a5c) = I32(0x130d5c);
                U32(0x128a60) = U32(0x130d60);
                U32(0x128a64) = U32(0x130d58);
                switch (U32(0x1324dc)) {
                case 0:
                    if (I32(0x1324e4) != 0) {
                        if (I32(0x1324e4) == 1) { (void)FUN_000b5fa4(); (void)FUN_000b5fa4(); }
                        I32(0x128a1c) += 1;
                        I32(0x132494) = 0x43914;
                        FUN_000ae8b0(0x132bb0u, 0, 0xe8);
                    } else { I32(0x128a1c) += 1; I32(0x132494) = 0x43914; }
                    break;
                case 1:
                    if (I32(0x1324e4) == 0) I32(0x132494) = 0x3b674;
                    else if (I32(0x1324e4) == 1) FUN_00037167();
                    else if (I32(0x1324e4) == 2) FUN_000370ed();
                    break;
                case 2: case 4: U32(0x1324d4) = 0; I32(0x132494) = 0x3cbb8; U32(0x1324dc) = 2; break;
                case 3: I32(0x132494) = 0x3988d; break;
                default: break;
                }
            }
            I32(0x1324e0) = 0;
            I32(fade + 8) *= 3; I32(fade + 0xc) *= 3;
        }
        (void)FUN_0004d064(0, 0); (void)FUN_0004d064(0, 0);
        if (I32(0x1324d8) != 0 && I32(0x4d900) == 0) {
            FUN_0004d7c0(I32(0x130d5c) >> 7, I32(0x130d5c) >> 7);
            if (I32(0x128a44) == 0) { (void)FUN_0004d738(0, 0); FUN_0004d5b4(2, 1); }
        }
        FUN_000331dc();
        if (I32(0x1323c8) != 0) (void)FUN_000b3d04((uint32_t)FUN_00051521(I32(0x1323c8)));   /* (its pictures, then the block) */
    } while (I32(0x1324d8) != 0);
    (void)FUN_0004d738(0, 0);
    (void)FUN_0004f3c8(U32(0x11d6ec), 0x1111d0u);
}

/*
 * THE ARCADE PAGE (0x3b674): backdrop "arcade_L.bkk", four scenes of the room
 * ("scene0..3.pth", 0x3519f), each a quarter of a turn of the clock (0.25 a second): the camera on
 * its scene's path (0x118b9c.., x, then z and y as the paths have them) looking at its target's
 * (0x118cb4..); the four cars on their paths (0x129d5 at the clock), facing along them.  ENTER goes
 * on to the arcade's choices (0x4189f, a sound), ESC back to the main menu (0x38d8d); "PRESS
 * ENTER" blinking; the fade.  The page's end: the scenes' paths let go (0x11a28).
 */
#define HOOK_3b674
static void FUN_0003b674(void)
{
    typedef long double ld;
    const uint32_t sp0 = s2_sp, fp = s2_push(0x80) + 0x80;
    const uint32_t look = fp - 0x74, cam = fp - 0x5c, target = fp - 0x50;
    FUN_0004bda7(0x11da14, 0x11d6f4);
    I32(0x132494) = 0;
    FUN_0004eaed(0x130bc8u, 0x111292u, 0x4cu);
    FUN_00044587(0x113340);
    FUN_00011906();
    FUN_0003519f(4, 0x1112a0u, 0, 0);
    for (uint32_t i = 0; i < 4; i++) {
        const uint32_t slot = 0x130fe0u + 8 * i, m = U32(0x11da14u + 0x1cu * (U32(0x130fe0) + i) + 0xc);
        FUN_0004be04(0x11da14, 0x11d6f4, (int)slot);
        U32(m + 0x3c) = 0; U32(m + 0x20) = 0;
        FUN_0004bfbe(0x11da14, 0x11d6f4, (int)slot);
    }
    float clock = 0.0f;
    uint32_t scene = 0;
    do {
        FUN_00038d48();
        FUN_00051b02(0, 0, U32(0x1323c8) + 8, U32(U32(0x1323c8)));
        I32(0x131eb0) = (int32_t)truncl((ld)F32(U32(0x118b9cu + 0xcu * scene) + 0xc));
        I32(0x131eb4) = (int32_t)truncl((ld)F32(U32(0x118ba4u + 0xcu * scene) + 0xc));
        I32(0x131eb8) = (int32_t)truncl((ld)F32(U32(0x118ba0u + 0xcu * scene) + 0xc));
        for (uint32_t k = 0; k < 3; k++) I32(cam + 4 * k) = I32(0x131eb0 + 4 * k);
        I32(target) = (int32_t)truncl((ld)F32(U32(0x118cb4u + 0xcu * scene) + 0xc));
        I32(target + 4) = (int32_t)truncl((ld)F32(U32(0x118cbcu + 0xcu * scene) + 0xc));
        I32(target + 8) = (int32_t)truncl((ld)F32(U32(0x118cb8u + 0xcu * scene) + 0xc));
        FUN_000338ed(look, target, cam);
        for (uint32_t k = 0; k < 3; k++) U32(0x131ea4 + 4 * k) = (uint32_t)(int32_t)truncl((ld)F32(look + 4 * k) * 1024.0f / (float)3.14159274f) & 0x7ff;
        FUN_0007e8b4(0x131ea4, 0);
        for (uint32_t i = 0; i < 4; i++) {
            const uint32_t path = U32(0x118d2cu + 4 * (scene * 4 + i)), m = U32(0x11da14u + 0x1cu * (U32(0x130fe0) + i) + 0xc);
            const uint32_t st = (uint32_t)FUN_000129d5((int)path, 0, 0, 0, clock);
            I32(m + 8) = (int32_t)truncl((ld)F32(st + 0x30));
            I32(m + 0xc) = (int32_t)truncl((ld)F32(st + 0x38));
            I32(m + 0x10) = (int32_t)truncl((ld)F32(st + 0x34));
            I32(m + 0x14) = 0;
            const float h = s2r_ground_angle(F32(path + 0x3c), F32(path + 0x40));
            F32(look) = F32(path + 0x3c); F32(look + 4) = h; F32(look + 8) = F32(path + 0x40);
            U32(m + 0x18) = (uint32_t)(int32_t)truncl((ld)h * 1024.0f / (float)3.14159274f) & 0x7ff;
            I32(m + 0x1c) = 0;
        }
        FUN_000753a0(0x11d6f4u, 0);
        const int key = (int)FUN_00038996(1, 0) != 0 && (U32(0x132498) & 0x7fffffff) == 0;
        I32(0x131f1c) = key;
        if (key) {
            if (I32(0x131f20) == 0x10) { U32(0x132498) = 0x3f800000u; I32(0x132494) = 0x4189f; FUN_000aea54(U32(0x124274), 1, 0, 0, 0, 0); }
            else if (I32(0x131f20) == 0x20) { U32(0x1324a0) = 0x3f800000u; I32(0x132494) = 0x38d8d; }
        }
        clock = (float)(double)((ld)F32(0x133350) * (double)0.25 + (double)clock);
        if (s2_bits(clock) > 0x3f800000) { clock = 0.0f; if (++scene > 3) scene = 0; }
        if ((U32(0x4ce0a) & 0x7f) < 0x40) FUN_0003327c(U32(0x127f14), 0xb8, 2, 1, 2);
        if ((U32(0x132498) & 0x7fffffff) != 0) F32(0x13249c) = (float)((ld)F32(0x133350) * 2.0f + F32(0x13249c));
        if (I32(0x13249c) >= 0x3f800000) { F32(0x13249c) = 1.0f; U32(0x1324a0) = 0x3f800000u; }
        if (fabsf(F32(0x13249c)) != 0.0f) FUN_000512f1(F32(0x13249c), 0, 0, 0);
        FUN_0007f5a2(0, 0);
    } while ((U32(0x1324a0) & 0x7fffffff) == 0);
    FUN_000358b9();
    FUN_00011a28();
    FUN_00044f72(0x113340, 0);
    s2_sp = sp0;
}

/*
 * A MENU ROOM'S FRAME (0x11d6f: the share t of its animation, the camera record).  The camera on
 * its path (0x118afc) looking at its target's (0x118c14), both at t (0x129d5); its turn from where
 * it looks (0x338ed).  The car's wheels' spin (0x1192e4) turns back at 0.7 of a turn a second, for
 * each model; in the car's room (0x1192e0 = 4) a car on a path (not +0x60 bit 8) has its wheels
 * ("flx" / "fli" .. of the first four models, 0x112d34) spun and their hubs' outer parts still;
 * each model on its path at t (or, held, at its path's first point: 0x118fac..), turned as the
 * path's record says (+0x48, +0x50, +0x4c).
 */
#define HOOK_11d6f
static void FUN_00011d6f(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3, float t, int cam)
{
    typedef long double ld;
    (void)r0; (void)r1; (void)r2; (void)r3;
    const uint32_t sp0 = s2_sp, fp = s2_push(0xa0) + 0xa0;
    const uint32_t look = fp - 0x44, target = fp - 0x38, at = fp - 0x2c, outer = fp - 0x80, inner = fp - 0xa0;
    const uint32_t c = (uint32_t)FUN_000129d5(I32(0x118afc), 0, 0, 0, t), g = (uint32_t)FUN_000129d5(I32(0x118c14), 0, 0, 0, t);
    static const uint32_t xyz[3] = { 0x30, 0x38, 0x34 };
    for (uint32_t k = 0; k < 3; k++) { I32(cam + 0xc + 4 * k) = (int32_t)truncl((ld)F32(c + xyz[k])); U32(at + 4 * k) = U32(cam + 0xc + 4 * k); }
    for (uint32_t k = 0; k < 3; k++) I32(target + 4 * k) = (int32_t)truncl((ld)F32(g + xyz[k]));
    FUN_000338ed(look, target, at);
    for (uint32_t k = 0; k < 3; k++) U32(cam + 4 * k) = (uint32_t)(int32_t)truncl((ld)F32(look + 4 * k) * 1024.0f / (float)3.14159274f) & 0x7ff;
    for (uint32_t i = 0; (int)i < I32(0x1192d0); i++) {
        F32(0x1192e4) = (float)(double)((double)F32(0x1192e4) - (ld)F32(0x133350) * (float)6.28318548f * (double)0.7);
        if (0.0f > F32(0x1192e4)) F32(0x1192e4) = (float)((ld)F32(0x1192e4) + (float)6.28318548f);
        const uint32_t slot = 0x11918cu + 8 * i, path = U32(0x118d2cu + 4 * i), m = U32(0x11da14u + 0x1cu * U32(slot) + 0xc);
        const int moving = !(U8(path + 0x60) & 8);
        if (I32(0x1192e0) == 4 && moving) {
            FUN_0004bc03(0x110158u, slot, 0x11da14);
            const uint32_t parts = U32(0x11da24u + 0x1cu * U32(slot));
            for (uint32_t k = 0; k < 4; k++) {
                I32(outer + 8 * k) = I32(0x11918cu + 8 * k);
                I32(inner + 8 * k) = I32(outer + 8 * k);
                FUN_0004bc03(0x112d34u + 8 * k, inner + 8 * k, 0x11da14);
                FUN_0004bc03(0x112d38u + 8 * k, outer + 8 * k, 0x11da14);
                U32(parts + 0x20u * U32(inner + 8 * k + 4) + 8) = (uint32_t)(int32_t)truncl((ld)F32(0x1192e4) * 1024.0f / (float)3.14159274f) & 0x7ff;
                U32(parts + 0x20u * U32(outer + 8 * k + 4) + 8) = 0;
            }
        }
        if (moving) {
            const uint32_t st = (uint32_t)FUN_000129d5((int)path, 0, 0, 0, t);
            for (uint32_t k = 0; k < 3; k++) I32(m + 8 + 4 * k) = (int32_t)truncl((ld)F32(st + xyz[k]));
        } else {
            I32(m + 8) = (int32_t)truncl((ld)F32(U32(0x118facu + 0xcu * i) + 0xc));
            I32(m + 0xc) = (int32_t)truncl((ld)F32(U32(0x118fb4u + 0xcu * i) + 0xc));
            I32(m + 0x10) = (int32_t)truncl((ld)F32(U32(0x118fb0u + 0xcu * i) + 0xc));
        }
        U32(m + 0x14) = (uint32_t)(int32_t)truncl((ld)F32(path + 0x48) * 1024.0f / (float)3.14159274f) & 0x7ff;
        U32(m + 0x18) = (uint32_t)(int32_t)truncl((ld)F32(path + 0x50) * 1024.0f / (float)3.14159274f) & 0x7ff;
        U32(m + 0x1c) = (uint32_t)(int32_t)truncl((ld)F32(path + 0x4c) * 1024.0f / (float)3.14159274f) & 0x7ff;
    }
    s2_sp = sp0;
}

/*
 * A NEW LEAGUE OPENED (0x3bbc4): the CD stopped; the room of the league (0x128a10: 3 and 4 share
 * the room 3, the last league has room 4) loaded (0x11298) and played through once along its
 * paths (0x11d6f at 1 / its length 0x1192d8 a second; the room's animation clock by 0x1192dc) with
 * the league's sound (not in the last room) and the CD's music; ENTER cuts it short.  The fade
 * out lowers the sound.  Then the room let go, on to the championship page (0x431b3) -- after the
 * third league, once, to the fourth's room first.
 */
#define HOOK_3bbc4
static void FUN_0003bbc4(void)
{
    typedef long double ld;
    (void)FUN_0004d738(0, 0);
    (void)FUN_0007f504(-0xab20, 0);
    (void)FUN_0004f6c4(0x140, 200, 100);
    FUN_00051664(0x1338ccu, 0);
    (void)FUN_0005105e(0x12420cu, 0x1338cc);
    FUN_00050d6c(0x12420c, 0, 0, 0x140, 200);
    FUN_00050fb9(0x12420c, 0, 0, 0x140, 200);
    const int room = I32(0x128a10) == 3 ? 2 : I32(0x128a10) == 4 ? 3 : I32(0x128a10);
    FUN_00011298((uint32_t)room + 1, 0, 0, 0);
    float t = 0.0f, anim = 0.0f;
    FUN_000ae8b0(0x132498u, 0, 0x34);
    if (room != 3) FUN_000aea54(U32(0x124274), 0xe, 0, 0, 0, 1);
    if (I32(0x128a44) == 0) FUN_0004d5b4(2, 1);
    U32(0x4ce0a) = 0;
    U32(0x133354) = 0x3ca3d70au;
    U32(0x133350) = 0x3ca3d70au;
    U32(0x4ce06) = 2;
    do {
        FUN_00038d48();
        anim = (float)((ld)F32(0x133350) / F32(0x1192dc) + anim);
        if (s2_bits(anim) >= 0x3f800000) anim = (float)((ld)anim + -1.0f);
        I32(0x1347a0) = (int32_t)truncl((ld)anim * 65535.0f);
        FUN_00011d6f(0x131ea4u, 0, 0, 0, t, 0x131ea4);
        FUN_0007e8b4(0x131ea4, 0);
        FUN_000753e8(0x11d6f4u, 0);
        t = (float)(1.0L / F32(0x1192d8) * F32(0x133350) + t);
        if (s2_bits(t) > 0x3f800000) t = 1.0f;
        if (t >= (double)0.98) U32(0x132498) = 0x3f800000u;
        FUN_0007f5a2(0, 0);
        if ((int)FUN_00038996(1, 0) != 0 && (U32(0x132498) & 0x7fffffff) == 0 && I32(0x131f20) == 0x10) U32(0x132498) = 0x3f800000u;
        if ((U32(0x132498) & 0x7fffffff) != 0) F32(0x13249c) = (float)((ld)F32(0x133350) * 2.0f + F32(0x13249c));
        if (I32(0x13249c) >= 0x3f800000) { F32(0x13249c) = 1.0f; U32(0x1324a0) = 0x3f800000u; }
        if (fabsf(F32(0x13249c)) != 0.0f) {
            FUN_000aec90(U32(0x124274), 0, (int)(uint32_t)(int64_t)truncl((ld)F32(0x133350) * 28671.0f));
            FUN_000512f1(F32(0x13249c), 0, 0, 0);
        }
    } while ((U32(0x1324a0) & 0x7fffffff) == 0);
    FUN_000aeef8(U32(0x124274));
    FUN_0004bda7(0x11da14, 0x11d6f4);
    (void)FUN_0004baf6(0x11da14, 0);
    (void)FUN_00050247();
    (void)FUN_000502d4();
    FUN_00011a28();
    FUN_000358b9();
    I32(0x132494) = 0x431b3;
    if (I32(0x1324e4) == 0 && I32(0x128a10) == 3) {
        if (I32(0x128a9cu + 0x188u * U32(0x1289ec)) != 4) { I32(0x132494) = 0x3bbc4; I32(0x128a10) = 4; I32(0x128a9cu + 0x188u * U32(0x1289ec)) = 4; }
    } else if (I32(0x1324e4) == 0 && I32(0x128a10) > 3) I32(0x128a10) = 3;
}

/* A TABLE SORTED (0x35916: how many rows, the first, the field, its kind 'c' byte / 'f' float /
   'l' whole number -- the field counted in bytes for 'c', in words otherwise -- the row's size, the
   way: rows whose difference has the way's sign are swapped, a bubble sort) */
#define HOOK_35916
static void FUN_00035916(int n, int first, int field, uint8_t kind, int size, int way)
{
    if (n <= 1) return;
    const uint32_t sp0 = s2_sp, tmp = s2_push(0x250) + 0x250 - 0x238;
    const int w = way < 0 ? -1 : 1;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n - i - 1; j++) {
            const uint32_t a = (uint32_t)(first + size * j), b = (uint32_t)(first + (j + 1) * size);
            int d;
            if (kind == 'c') d = (int)((uint32_t)U8(a + (uint32_t)field) - (uint32_t)U8(b + (uint32_t)field)) < 0 ? -1 : 1;
            else if (kind == 'f') d = (long double)F32(a + 4u * (uint32_t)field) - F32(b + 4u * (uint32_t)field) < 0.0L ? -1 : 1;
            else if (kind == 'l') d = (int32_t)((uint32_t)I32(a + 4u * (uint32_t)field) - (uint32_t)I32(b + 4u * (uint32_t)field)) < 0 ? -1 : 1;
            else continue;
            if (d == w) { FUN_000ae6e3(tmp, a, (uint32_t)size); FUN_000ae6e3(a, b, (uint32_t)size); FUN_000ae6e3(b, tmp, (uint32_t)size); }
        }
    s2_sp = sp0;
}

/*
 * THE TITLE PAGE (0x3ff37): backdrop "logo1.bkk"; the four teams' cars in the room, the chosen
 * team (0x1289ec) in front (its places 0x130e90.. turned round to it, 0x336eb), each spinning
 * (the chosen one four times as fast); the 64 letter models (0x361dc) and the 26 of the name wheel.
 * Left / right change the team (a sound); ENTER goes to the name (0x1324c4 = 1: the cars move off,
 * the letter wheel turns -- up / down the letter's place, left / right or a key the letter, held
 * keys repeating -- and ENTER again goes on to the main menu, ESC back to the team); ESC on the
 * team leaves the game (0x1324d8 = 0).  Thirty seconds untouched starts the attract race
 * (0x3c9ac).  The fade to white at 4 a second, then from black at 3; the team's music file
 * "p_<team>.mcm" (0x32c48).
 */
#define HOOK_3ff37
static void FUN_0003ff37(void)
{
    typedef long double ld;
    const uint32_t sp0 = s2_sp, fp = s2_push(0x80) + 0x80, line = fp - 0x60, music = fp - 0x50;
    int cursor = 0;
    float blink = 1.0f, idle;
    FUN_0004bda7(0x11da14, 0x11d6f4);
    I32(0x132494) = 0;
    FUN_0004eaed(0x130bc8u, 0x111488u, 0x4cu);
    FUN_00044587(0x113240);
    int letter = (int)U8(0x1289f4) - 0x41, was = letter;
    for (uint32_t k = 0; k < 4; k++) {
        const uint32_t slot = 0x130f68u + 8 * k, m = U32(0x11da14u + 0x1cu * (U32(0x130f68) + k) + 0xc), e = 0x130e90u + 0xc * k;
        FUN_0004be04(0x11da14, 0x11d6f4, (int)slot);
        I32(e) = k == 0 ? 0 : ((int)k - 1) * 3000 - 3000;
        I32(e + 4) = k == 2 ? 2000 : 0;
        I32(e + 8) = k == 0 ? 3500 : 1000;
        if (k == 0) U32(0x130e94) = 300;
        if (k == 2) U32(0x130ea8) = 0;
        I32(m + 0x10) = 0; I32(m + 0xc) = 0; I32(m + 8) = 0;
        I32(m + 0x1c) = 0; I32(m + 0x14) = 0; I32(m + 0x18) = 0x200;
        I32(m + 0x3c) = 0; I32(m + 0x20) = 0;
        FUN_0004bfbe(0x11da14, 0x11d6f4, (int)slot);
    }
    for (uint32_t u = U32(0x1289ec); u != 0; u--) FUN_000336eb(1, 0x130e90, 0xc, 4);
    for (int i = 0; i < 0x40; i++) FUN_000361dc((int)(0x1314a4u + 0x28u * (uint32_t)i), i);
    for (uint32_t u = 0; u < 0x1a; u++) {
        const uint32_t r = 0x1314a4u + 0x28u * u;
        U32(r + 8) = u; U32(r + 4) = u + 0x41; I32(r + 0x24) = 0; I32(r + 0xc) = 1; U32(r + 0x1c) = 0x3f800000u; U32(r + 0x20) = 0x3f800000u;
    }
    idle = 30.0f;
    do {
        FUN_00038d48();
        FUN_00051b02(0, 0, U32(0x1323c8) + 8, U32(U32(0x1323c8)));
        U32(0x131eb0) = 0; U32(0x131eb4) = 500; I32(0x131eb8) = 0x12c0;
        U32(0x131eac) = 0; U32(0x131ea8) = 0; U32(0x131ea4) = 0;
        FUN_0007e8b4(0x131ea4, 0);
        for (uint32_t k = 0; k < 4; k++) {
            const uint32_t m = U32(0x11da14u + 0x1cu * (U32(0x130f68) + k) + 0xc), e = 0x130e90u + 0xc * k;
            const int mine = k == U32(0x1289ec);
            I32(m + 0x18) = s2_turn_by(m + 0x18, mine ? 1024.0f : 256.0f);
            U32(m + 0x18) &= 0x7ff;
            if (I32(0x1324c4) == 0) FUN_00033635((int)(m + 8), (int)e, 0x50);
            else {
                FUN_00011212((int)(m + 8), (float)I32(e), 0x50);
                FUN_00011212((int)(m + 0xc), (float)(I32(e + 4) + (mine ? 500 : -500)), 0x50);
                FUN_00011212((int)(m + 0x10), (float)(I32(e + 8) - (mine ? 500 : 25000)), 0x50);
            }
        }
        I32(0x131f1c) = (int)FUN_00038996(0, 0);
        if ((U32(0x132498) & 0x7fffffff) == 0) {
            if (I32(0x131f1c) != 0) {
                const uint32_t key = U32(0x131f20);
                if (key == 1) { if (I32(0x1324c4) != 0 && cursor != 0) cursor--; idle = 30.0f; }
                else if (key == 2) { if (I32(0x1324c4) != 0 && cursor < 2) cursor++; idle = 30.0f; }
                else if (key == 0x10) {
                    if (I32(0x1324c4) == 0) { I32(0x1324c4) = 1; FUN_000aea54(U32(0x124274), 6, 0, 0x2b11, 0, 1); }
                    else { FUN_000aea54(U32(0x124274), 1, 0, 0, 0, 1); U32(0x132498) = 0x3f800000u; I32(0x132494) = 0x38d8d; }
                } else if (key == 0x20) {
                    if (I32(0x1324c4) == 0) { F32(0x13249c) = 1.0f; U32(0x1324a0) = 0x3f800000u; U32(0x132498) = U32(0x1324a0); U32(0x1324d8) = 0; }
                    else {
                        letter = (int)U8(0x1289f4u + (uint32_t)cursor) - 0x41; was = letter;
                        for (uint32_t u = 0; u < 0x1a; u++) {
                            FUN_0004c01c(0x11da14, 0x11d6f4, (int)(0x131090u + 8 * u));
                            const uint32_t r = 0x1314a4u + 0x28u * u;
                            U32(r + 8) = u; U32(r + 4) = u + 0x41; I32(r + 0x24) = 0; I32(r + 0xc) = 1; U32(r + 0x1c) = 0x3f800000u; U32(r + 0x20) = 0x3f800000u;
                        }
                        I32(0x1324c4) -= 1;
                    }
                }
            }
            const uint32_t key = U32(0x131f20);
            if (key == 4 || key == 8) {
                const int step = key == 4 ? -1 : 1;
                idle = 30.0f;
                if (I32(0x1324c4) == 0 && I32(0x131f1c) != 0) {
                    U32(0x1289ec) = (U32(0x1289ec) + (uint32_t)step) & 3;
                    FUN_000336eb(key == 4 ? 0 : 1, 0x130e90, 0xc, 4);
                    FUN_000aea54(U32(0x124274), 4, 0, 0, 0, 0);
                } else if (blink > (double)0.95 && I32(0x1324c4) != 0) {
                    FUN_000aea54(U32(0x124274), 9, 0, 0, 0, 0);
                    was = letter;
                    letter += step;
                    if (letter < 0) letter = 0x19;
                    else if (letter > 0x19) letter = 0;
                    blink = 0.0f;
                }
            }
        }
        if (I32(0x1324c4) != 0) {
            const int c = (int)(uint32_t)FUN_00036845(0, 0);
            if (c != 0) {
                was = letter; letter = c - 0x41;
                U8(0x1289f4u + (uint32_t)cursor) = (uint8_t)c;
                if (cursor < 2) cursor++;
                blink = 0.0f;
            }
        }
        if ((U32(0x132498) & 0x7fffffff) == 0 && I32(0x1324c4) == 0) {
            idle = (float)((ld)idle - F32(0x133350));
            if (0.0f > idle) { F32(0x13249c) = 1.0f; U32(0x1324a0) = 0x3f800000u; U32(0x132498) = U32(0x1324a0); I32(0x132494) = 0x3c9ac; }
        }
        blink = (float)((ld)F32(0x133350) * 8.0f + blink);
        if (s2_bits(blink) > 0x3f800000) blink = 1.0f;
        if (I32(0x1324c4) != 0) {
            FUN_0003661d(letter + 0x41, 0, 0x1a);
            for (uint32_t u = 0; u < 0x1a; u++) FUN_00036245((int)(0x1314a4u + 0x28u * u), letter, was, u * 0x28u, blink, 0);
        }
        FUN_000753a0(0x11d6f4u, 0);
        if (I32(0x1324c4) == 0) {
            FUN_0003327c(U32(0x127ed8), 4, 0, 1, 2);
            FUN_0003327c(U32(0x127edcu + 4u * U32(0x1289ec)), 0xaf, 1, 1, 2);
        } else {
            FUN_0003327c(U32(0x128044), 4, 0, 1, 2);
            U8(0x1289f4u + (uint32_t)cursor) = (uint8_t)(letter + 'A');
            FUN_0004eaed(line, 0x1114afu, 0x1289f4u);
            FUN_0003327c(line, 0xb0, 1, 1, 2);
            FUN_00050f45(0x12420c, cursor * 0x13 + 0x88, 0xba, 0x1114b3u, 1, 1, 2);
        }
        if ((U32(0x132498) & 0x7fffffff) != 0) F32(0x13249c) = (float)((ld)F32(0x133350) * 4.0f + F32(0x13249c));
        if (I32(0x13249c) >= 0x3f800000) { F32(0x13249c) = 1.0f; U32(0x1324a0) = 0x3f800000u; }
        if (fabsf(F32(0x13249c)) != 0.0f) FUN_000512f1(F32(0x13249c), 0xff, 0xff, 0xff);
        FUN_0007f5a2(0, 0);
    } while ((U32(0x1324a0) & 0x7fffffff) == 0);
    FUN_00051aa4(0x1338ccu, 0x300, 0x3f);
    FUN_00051664(0x1338ccu, 0);
    for (F32(0x13249c) = 0.0f; I32(0x13249c) < 0x3f800000; F32(0x13249c) = (float)((ld)F32(0x133350) * 3.0f + F32(0x13249c))) {
        FUN_00038d48();
        FUN_000512f1(F32(0x13249c), 0, 0, 0);
        FUN_0007f5a2(0, 0);
    }
    FUN_000358b9();
    FUN_00044f72(0x113240, 0);
    FUN_0004eaed(music, 0x1114bdu, U32(0x1289ec) + 0x31u);
    FUN_00032c48(3, (int)music);
    s2_sp = sp0;
}

/*
 * THE CHAMPIONSHIP PAGE (0x431b3): backdrop "bkg_3L.bkk"; the four leagues two at a time (their
 * badges and names, the closed ones marked), the chosen one (0x128a10) blinking and the other one
 * shaded.  Left / right move (a sound); ENTER on an open league (up to 0x128a18) picks it -- the
 * league's level 0x128a24 (0, 1, then 2), a sound -- on to the league's page (0x394f5) or, in a
 * network game, to its tracks (0x3edb4, the players' cars summed into 0x1324cc); ESC back to the
 * main menu (or leaves the network game).  A new championship: its counts cleared (0x128a1c,
 * 0x128a74 ..), four players' places (0x128a20).  In a network game the other machine's keys are
 * taken (0x37b41 / 0x36d22).  The fade.
 */
#define HOOK_431b3
static void FUN_000431b3(void)
{
    typedef long double ld;
    int first = 0, open;
    U32(0x1324dc) = 0;
    FUN_0004bda7(0x11da14, 0x11d6f4);
    I32(0x132494) = 0;
    FUN_0004eaed(0x130bc8u, 0x1115d8u, 0x4cu);
    FUN_00044587(0x113374);
    if (U32(0x1324e4) == 1) {
        if (FUN_00037b41(0x2468, 0x4287, 0, 0) != 0) { FUN_00037167(); FUN_00044f72(0x113374, 0); return; }
        I32(0x128a10) = 0; open = 2;
    } else if (U32(0x1324e4) == 2) {
        if (FUN_00036d22(0x2468u, 0, 0)) { FUN_000370ed(); FUN_00044f72(0x113374, 0); return; }
        I32(0x128a10) = 0; open = 2;
    } else open = I32(0x128a18);
    U32(0x4ce0a) = 0;
    U32(0x133354) = 0x3ca3d70au; U32(0x133350) = 0x3ca3d70au;
    U32(0x4ce06) = 2;
    U32(0x128a1c) = 0; U32(0x128a98) = 0; U32(0x128a8c) = 0; U32(0x128a80) = 0; U32(0x128a74) = 0;
    if (U32(0x1324e4) == 0) U32(0x128a20) = 4;
    do {
        FUN_00038d48();
        FUN_00051b02(0, 0, U32(0x1323c8) + 8, U32(U32(0x1323c8)));
        I32(0x131f1c) = (int)FUN_00038996(0, 0);
        const int fading = (U32(0x132498) & 0x7fffffff) != 0;
        if (U32(0x1324e4) == 1 && !fading) {
            if (FUN_00037b41(0xdef0, 0x2266, 0, 0) != 0) FUN_00037167();
            if (I32(0x131f28) != 0) { I32(0x131f1c) = I32(0x132534); U32(0x131f20) = U32(0x132530); }
        } else if (U32(0x1324e4) == 2 && !fading) {
            if (FUN_00036d22(0xdef0u, 0, 0)) FUN_000370ed();
            if (I32(0x131f28) != 0) { I32(0x131f1c) = I32(0x132bd4); U32(0x131f20) = U32(0x132bd0); }
        }
        if (I32(0x131f1c) != 0 && (U32(0x132498) & 0x7fffffff) == 0) {
            const uint32_t key = U32(0x131f20);
            if (key == 4 && I32(0x128a10) != 0) { I32(0x128a10) -= 1; U32(0x4ce0a) = 0; FUN_000aea54(U32(0x124274), 0, 0, 0, 0, 0); }
            else if (key == 8 && I32(0x128a10) < 3) { I32(0x128a10) += 1; U32(0x4ce0a) = 0; FUN_000aea54(U32(0x124274), 0, 0, 0, 0, 0); }
            else if (key == 0x10) {
                if (I32(0x128a10) <= open) {
                    U32(0x128a24) = I32(0x128a10) == 0 ? 0 : I32(0x128a10) == 1 ? 1 : 2;
                    FUN_000aea54(U32(0x124274), 6, 0, 0, 0, 0);
                    U32(0x132498) = 0x3f800000u;
                    if ((U32(0x1324e4) & 3) == 0) { I32(0x132494) = 0x394f5; U32(0x1324cc) = U32(0x4ce0a); }
                    else {
                        I32(0x132494) = 0x3edb4;
                        uint32_t sum;
                        if (U32(0x1324e4) == 1) { sum = U32(0x131f2c); for (int i = 0; i < I32(0x131f2c); i++) sum += U32(0x132500u + 0xd8u * (uint32_t)i); }
                        else { sum = 0; for (int i = 0; i < I32(0x131f2c); i++) sum += U32(0x132bb8u + 0x74u * (uint32_t)i); }
                        U32(0x1324cc) = sum;
                    }
                }
            } else if (key == 0x20) {
                if (U32(0x1324e4) == 1) FUN_00037167();
                else if (U32(0x1324e4) == 2) FUN_000370ed();
                else { U32(0x1324a0) = 0x3f800000u; U32(0x132498) = 0x3f800000u; I32(0x132494) = 0x38d8d; }
            }
        }
        while (I32(0x128a10) - first > 1) first++;
        while (I32(0x128a10) - first < 0) first--;
        const int at = I32(0x128a10) - first;
        FUN_00050c0a(0x12420c, 0x13, 0x27, first + 0x2c, 0);
        FUN_00050c0a(0x12420c, 0xb0, 0x27, first + 0x2d, 0);
        if ((U32(0x4ce0a) & 0x3f) < 0x20) FUN_00050c0a(0x12420c, at * 0x9d + 0xc, 0x20, 0x30, 0);
        for (int i = 0; i < 2; i++) {
            if (open < first + i) {
                FUN_000332dd(U32(0x127f34), 100, 0xa0, (uint32_t)(i * 0xa0), 2, 1);
                FUN_000332dd(U32(0x127f38), 0x70, 0xa0, (uint32_t)(i * 0xa0), 2, 1);
            }
            FUN_000332dd(U32(0x127f3cu + 8u * (uint32_t)(first + i)), 0xa0, 0xa0, (uint32_t)(i * 0xa0), 2, 1);
            FUN_000332dd(U32(0x127f40u + 8u * (uint32_t)(first + i)), 0xac, 0xa0, (uint32_t)(i * 0xa0), 2, 1);
        }
        FUN_00051698((int)FUN_000b73c7((uint32_t)(at - 1)) * 0x9d + 0x12, 0x23, 0x86, 0x9c);
        if ((U32(0x4ce0a) & 0x7f) < 0x40) {
            if (first != 0) FUN_00050c0a(0x12420c, 10, 0xac, 3, 0);
            if (first < 2) FUN_00050c0a(0x12420c, 0x12a, 0xac, 4, 0);
        }
        FUN_0003327c(U32(0x127f30), 2, 0, 1, 2);
        if ((U32(0x1324e4) & 3) != 0 && I32(0x131f28) != 0) {
            FUN_00051698(0, 0, 0x140, 200);
            if (U32(0x4ce0a) & 0x40) FUN_00037686();
        }
        if ((U32(0x132498) & 0x7fffffff) != 0) F32(0x13249c) = (float)((ld)F32(0x133350) * 2.0f + F32(0x13249c));
        if (I32(0x13249c) >= 0x3f800000) { F32(0x13249c) = 1.0f; U32(0x1324a0) = 0x3f800000u; }
        if (fabsf(F32(0x13249c)) != 0.0f) FUN_000512f1(F32(0x13249c), 0, 0, 0);
        FUN_0007f5a2(0, 0);
    } while ((U32(0x1324a0) & 0x7fffffff) == 0);
    FUN_000358b9();
    FUN_00044f72(0x113374, 0);
}

/*
 * THE ARCADE'S TRACKS (0x4189f): backdrop "bkg_1L.bkk"; the open tracks (the level 0x128a14 + 3;
 * in a network game 3, or 4 when a player has the last league) as a strip of pictures (0x113224's
 * tracks, the chosen one 0x128a0c lit) three at a time, the strip sliding (0x111d6 at 0x32, put
 * in place within half a pixel) and the frame round the chosen one sliding too, blinking.
 * Left / right choose (a sound); ENTER picks it (a sound) -- on to the cars (0x40a67), or in the
 * championship's replay mode (0x1324dc = 3) straight to the race (0x1324e0); ESC back to the main
 * menu (or leaves the network game).  The other machine's keys in a network game; the fade.
 */
#define HOOK_4189f
static void FUN_0004189f(void)
{
    typedef long double ld;
    const uint32_t sp0 = s2_sp, fp = s2_push(0x70) + 0x70, strip = fp - 0x20, frame = fp - 0x1c;
    float spin = 0.0f;
    F32(strip) = 0.0f; F32(frame) = 0.0f;
    int open, first = I32(fp - 0x34);
    if (I32(0x1324e4) == 1) {
        if (FUN_00037b41(0x2468, 0x1122, 0, 0) != 0) { FUN_00037167(); s2_sp = sp0; return; }
        I32(0x128a0c) = 0; open = 3;
        for (int i = 0; i < I32(0x131f2c); i++) if (U32(0x132504u + 0xd8u * (uint32_t)i) >> 8 == 4) open = 4;
    } else if (I32(0x1324e4) == 2) {
        if (FUN_00036d22(0x2468u, 0, 0)) { FUN_000370ed(); s2_sp = sp0; return; }
        I32(0x128a0c) = 0; open = 3;
        for (int i = 0; i < I32(0x131f2c); i++) if (U32(0x132bbcu + 0x74u * (uint32_t)i) >> 8 == 4) open = 4;
    } else open = I32(0x128a14);
    if (I32(0x128a14) + 2 < I32(0x128a0c)) I32(0x128a0c) = 0;
    FUN_0004bda7(0x11da14, 0x11d6f4);
    I32(0x132494) = 0;
    FUN_0004eaed(0x130bc8u, 0x11151eu, 0x4cu);
    FUN_00044587(0x113374);
    do {
        FUN_00038d48();
        FUN_00051b02(0, 0, U32(0x1323c8) + 8, U32(U32(0x1323c8)));
        I32(0x131f1c) = (int)FUN_00038996(0, 0);
        const int fading = (U32(0x132498) & 0x7fffffff) != 0;
        if (I32(0x1324e4) == 1 && !fading) {
            if (FUN_00037b41(0xdef0, 0x5566, 0, 0) != 0) FUN_00037167();
            if (I32(0x131f28) != 0) { I32(0x131f1c) = I32(0x132534); U32(0x131f20) = U32(0x132530); }
        } else if (I32(0x1324e4) == 2 && !fading) {
            if (FUN_00036d22(0xdef0u, 0, 0)) FUN_000370ed();
            if (I32(0x131f28) != 0) { I32(0x131f1c) = I32(0x132bd4); U32(0x131f20) = U32(0x132bd0); }
        }
        if (I32(0x131f1c) != 0 && (U32(0x132498) & 0x7fffffff) == 0) {
            const uint32_t key = U32(0x131f20);
            if (key == 4 && I32(0x128a0c) != 0) { FUN_000aea54(U32(0x124274), 0, 0, 0, 0, 0); I32(0x128a0c) -= 1; U32(0x4ce0a) = 0; }
            else if (key == 8 && I32(0x128a0c) < open + 2) { FUN_000aea54(U32(0x124274), 0, 0, 0, 0, 0); I32(0x128a0c) += 1; U32(0x4ce0a) = 0; }
            else if (key == 0x10) {
                U32(0x132498) = 0x3f800000u;
                FUN_000aea54(U32(0x124274), 2, 0, 0, 0, 0);
                if (I32(0x1324dc) == 3) U32(0x1324e0) = 1;
                else I32(0x132494) = 0x40a67;
            } else if (key == 0x20) {
                if (I32(0x1324e4) == 1) FUN_00037167();
                else if (I32(0x1324e4) == 2) FUN_000370ed();
                else { U32(0x1324a0) = 0x3f800000u; U32(0x132498) = 0x3f800000u; I32(0x132494) = 0x38d8d; }
            }
        }
        FUN_00050c0a(0x12420c, 0x1c, 0xf, 0x69, 0);
        const int last = open + 2, sel = I32(0x128a0c);
        if (sel == 0) first = 0;
        else if (last - sel < 1) { if (last == sel) first = sel - 2; }
        else first = sel - 1;
        if (fabsf(F32(strip)) == 0.0f) F32(strip) = (float)(first * -0x69 + 8);
        else FUN_000111d6((int)strip, (float)(first * -0x69 + 8), 0x32);
        if (!(fabsl((ld)F32(strip) - (ld)(first * -0x69 + 8)) > (double)0.5)) F32(strip) = (float)(first * -0x69 + 8);
        for (int i = 0; i < open + 3; i++)
            FUN_00050c0a(0x12420c, (int32_t)truncl((ld)(i * 0x69) + F32(strip)), 0x40, I32(0x113224u + 4u * (uint32_t)i) + 5 + (i == I32(0x128a0c) ? 0 : 7), 0);
        if ((U32(0x4ce0a) & 0x7f) < 0x40) {
            if (first != 0) FUN_00050c0a(0x12420c, 10, 0xaa, 1, 0);
            if (first + 2 < last) FUN_00050c0a(0x12420c, 0x12a, 0xaa, 2, 0);
        }
        first = I32(0x128a0c) == 0 ? 0 : I32(0x128a0c) == last ? 2 : 1;
        if (fabsf(F32(frame)) == 0.0f) F32(frame) = (float)(first * 0x69 + 2);
        else FUN_000111d6((int)frame, (float)(first * 0x69 + 2), 0x32);
        if (fabsl((ld)F32(frame) - (ld)(first * 0x69 + 2)) < (double)0.5) F32(frame) = (float)(first * 0x69 + 2);
        if ((U32(0x4ce0a) & 0x3f) < 0x20) FUN_00050c0a(0x12420c, (int32_t)truncl((ld)F32(frame)), 0x3c, 0, 0);
        if (I32(0x1324e4) != 0 && I32(0x131f28) != 0) {
            FUN_00051698(0, 0, 0x140, 200);
            if (U32(0x4ce0a) & 0x40) FUN_00037686();
        }
        if (fabsf(spin) != 0.0f) spin = (float)((ld)F32(0x133350) * 4.0f + spin);
        if (s2_bits(spin) >= 0x3f800000) spin = 0.0f;
        if ((U32(0x132498) & 0x7fffffff) != 0) F32(0x13249c) = (float)((ld)F32(0x133350) * 2.0f + F32(0x13249c));
        if (I32(0x13249c) >= 0x3f800000) { F32(0x13249c) = 1.0f; U32(0x1324a0) = 0x3f800000u; }
        if (fabsf(F32(0x13249c)) != 0.0f) FUN_000512f1(F32(0x13249c), 0, 0, 0);
        FUN_0007f5a2(0, 0);
    } while ((U32(0x1324a0) & 0x7fffffff) == 0);
    FUN_000358b9();
    FUN_00044f72(0x113374, 0);
    s2_sp = sp0;
}

/*
 * THE OPTIONS PAGE (0x420c0): backdrop "bkg_7L.bkk"; eight settings (0x128a28.., each with its
 * count of values 0x1131c4.. and its names from 0x127ed8 by the row 0x1131c0..) and "done", one
 * under the other (1.2 lines of the font apart), the chosen one blinking.  Up / down choose (a
 * sound); ENTER steps the setting round (the eighth -- the CD's music -- restarts it or stops it)
 * or, on "done", goes to the controls' page (0x4254f); ESC back to the main menu.  The fade.
 */
#define HOOK_420c0
static void FUN_000420c0(void)
{
    typedef long double ld;
    int row = 0;
    FUN_0004bda7(0x11da14, 0x11d6f4);
    I32(0x132494) = 0;
    FUN_0004eaed(0x130bc8u, 0x11154eu, 0x4cu);
    FUN_00044587(0x113374);
    do {
        FUN_00038d48();
        FUN_00051b02(0, 0, U32(0x1323c8) + 8, U32(U32(0x1323c8)));
        I32(0x131f1c) = (int)FUN_00038996(0, 0);
        if (I32(0x131f1c) != 0 && (U32(0x132498) & 0x7fffffff) == 0) {
            const uint32_t key = U32(0x131f20);
            if (key == 1 && row != 0) { row--; U32(0x4ce0a) = 0; FUN_000aea54(U32(0x124274), 10, 0, 0, 0, 0); }
            else if (key == 2 && row < 8) { row++; U32(0x4ce0a) = 0; FUN_000aea54(U32(0x124274), 10, 0, 0, 0, 0); }
            else if (key == 0x10) {
                if (row == 8) { U32(0x132498) = 0x3f800000u; I32(0x132494) = 0x4254f; FUN_000aea54(U32(0x124274), 0xd, 0, 0, 0, 0); }
                else {
                    const uint32_t s = 0x128a28u + 4u * (uint32_t)row;
                    FUN_000aea54(U32(0x124274), 0xc, 0, 0, 0, 0);
                    I32(s) += 1;
                    if (I32(0x1131c4u + 4u * (uint32_t)row) <= I32(s)) I32(s) = 0;
                    if (row == 7) {
                        (void)FUN_0004d064(0, 0x1c); (void)FUN_0004d064(0, 0);
                        if (I32(0x128a44) == 0) FUN_0004d5b4(2, 1);
                        else (void)FUN_0004d738(0x1c, 0);
                    }
                }
            } else if (key == 0x20) { U32(0x1324a0) = 0x3f800000u; U32(0x132498) = 0x3f800000u; I32(0x132494) = 0x38d8d; }
        }
        FUN_0003327c(U32(0x127f04), 4, 0, 1, 2);
        int y = 0x30, names = 0x79;
        for (int i = 0; i < 9; i++) {
            names += I32(0x1131c0u + 4u * (uint32_t)i);
            const int lit = (U32(0x4ce0a) & 0x20) == 0 && row == i;
            if (i == 8) FUN_00050f45(0x12420c, 10, y, U32(0x1280b8), lit ? 3 : 2, lit ? 3 : 1, 2);
            else {
                const uint32_t value = U32(0x127ed8u + 4u * ((uint32_t)names + U32(0x128a28u + 4u * (uint32_t)i)));
                FUN_00050f45(0x12420c, 10, y, U32(0x128098u + 4u * (uint32_t)i), 2, 1, 2);
                if (lit) { FUN_00050f45(0x12420c, 200, y, value, 3, 3, 2); FUN_00050f45(0x12420c, 200, y, 0x11155au, 2, 1, 2); }
                else FUN_00050f45(0x12420c, 200, y, value, 2, 1, 2);
            }
            y = (int32_t)truncl((double)((ld)I32(U32(0x124250) + 0x60) * (double)1.2 / 1.0L + (double)y));
        }
        if ((U32(0x132498) & 0x7fffffff) != 0) F32(0x13249c) = (float)((ld)F32(0x133350) * 2.0f + F32(0x13249c));
        if (I32(0x13249c) >= 0x3f800000) { F32(0x13249c) = 1.0f; U32(0x1324a0) = 0x3f800000u; }
        if (fabsf(F32(0x13249c)) != 0.0f) FUN_000512f1(F32(0x13249c), 0, 0, 0);
        FUN_0007f5a2(0, 0);
    } while ((U32(0x1324a0) & 0x7fffffff) == 0);
    FUN_000358b9();
    FUN_00044f72(0x113374, 0);
}

/*
 * A PAGE'S THINGS LET GO (0x44f72: the page's list, whether to keep its first two parts).  The
 * list is three runs of numbers each ended by -1 (what the page needs, ..., what it lets go at its
 * end); without "keep" the third run is gone through, else the list from its start.  Each thing
 * loaded (0x1323cc..) is let go: 1 the fonts, 2 the sprites, 3 the texts, 4 the backdrop, 5 the
 * room's set (menu<team>1.dat: "map<team>", its five models), 6 the team's cars (cars<team>01.dat:
 * "auto1".."auto5") and the reflections, 7 every model and the 3D's things, 9 the tracks' maps
 * (maps1.dat, seven models), 10 the leagues' plates (plates1.dat, seven), 11 the two teams' cars
 * (the second 0x1289f0) and reflections, 12 the sounds.
 */
#define HOOK_44f72
static void FUN_00044f72(int list, int keep)
{
    const uint32_t sp0 = s2_sp, buf = s2_push(0xb0) + 0xb0 - 0xa8;
    uint32_t p = (uint32_t)list;
    if (keep == 0) { while (I32(p) != -1) p += 4; p += 4; while (I32(p) != -1) p += 4; p += 4; }
    for (; I32(p) != -1; p += 4) {
        const uint32_t k = U32(p);
        if (I32(0x1323ccu + 4 * k) == 0) continue;
        I32(0x1323ccu + 4 * k) = 0;
        const uint32_t team = U32(0x1289ec) + 0x31u;
        switch (k) {
        case 1: FUN_00050ef4(0x12420c); break;
        case 2: FUN_00050b71(0x12420c); break;
        case 3: FUN_000326be(); break;
        case 4: FUN_0005160b(I32(0x1323c8), 0, 0); break;
        case 5:
            FUN_0004eaed(buf, 0x1117b2u, 0x11eff8u, team, 0x31u);
            FUN_0004b49f(buf, buf);
            FUN_0004eaed(buf, 0x1117c6u, team);
            for (uint32_t i = 0; i < 5; i++) { FUN_0004beba(0x11da14, 0x11d6f4, (int)(0x131030u + 8 * i)); FUN_0004ba39(0x11da14, 0x131030u + 8 * i); }
            (void)FUN_00050165(buf);
            (void)FUN_000502d4();
            break;
        case 6:
            FUN_0004eaed(buf, 0x1117ccu, 0x11eff8u, team, 0x31u);
            FUN_0004b49f(buf, buf);
            for (uint32_t i = 0; i < 5; i++) {
                FUN_0004eaed(buf, 0x1117e1u, i + 0x31u);
                (void)FUN_00050165(buf);
                FUN_0004beba(0x11da14, 0x11d6f4, (int)(0x130fe0u + 8 * i)); FUN_0004ba39(0x11da14, 0x130fe0u + 8 * i);
            }
            FUN_0004eaed(buf, 0x1117e8u, 0x11eff8u, team, 0x31u);
            FUN_0004b49f(buf, buf);
            (void)FUN_00050165(0x1117fcu);
            break;
        case 7:
            FUN_0004bda7(0x11da14, 0x11d6f4);
            (void)FUN_0004baf6(0x11da14, 0);
            (void)FUN_0004c21a();
            (void)FUN_00050247();
            break;
        case 9:
            FUN_0004eaed(buf, 0x111850u, 0x11eff8u, 0x31u);
            FUN_0004b49f(buf, buf);
            (void)FUN_00050165(0x111862u);
            for (uint32_t i = 0; i < 7; i++) { FUN_0004beba(0x11da14, 0x11d6f4, (int)(0x131058u + 8 * i)); FUN_0004ba39(0x11da14, 0x131058u + 8 * i); }
            break;
        case 10:
            FUN_0004eaed(buf, 0x111867u, 0x11eff8u, 0x31u);
            FUN_0004b49f(buf, buf);
            (void)FUN_00050165(0x11187bu);
            for (uint32_t i = 0; i < 7; i++) { FUN_0004beba(0x11da14, 0x11d6f4, (int)(0x130fa8u + 8 * i)); FUN_0004ba39(0x11da14, 0x130fa8u + 8 * i); }
            break;
        case 0xb:
            FUN_0004eaed(buf, 0x111800u, 0x11eff8u, team, 0x31u);
            FUN_0004b49f(buf, buf);
            for (uint32_t i = 0; i < 5; i++) {
                FUN_0004eaed(buf, 0x111815u, i + 0x31u);
                (void)FUN_00050165(buf);
                FUN_0004beba(0x11da14, 0x11d6f4, (int)(0x130fe0u + 8 * i)); FUN_0004ba39(0x11da14, 0x130fe0u + 8 * i);
            }
            FUN_0004eaed(buf, 0x11181cu, 0x11eff8u, U32(0x1289f0) + 0x31u, 0x31u);
            FUN_0004b49f(buf, buf);
            for (uint32_t i = 0; i < 5; i++) {
                FUN_0004eaed(buf, 0x111831u, i + 0x31u);
                (void)FUN_00050165(buf);
                FUN_0004beba(0x11da14, 0x11d6f4, (int)(0x131008u + 8 * i)); FUN_0004ba39(0x11da14, 0x131008u + 8 * i);
            }
            FUN_0004eaed(buf, 0x111838u, 0x11eff8u, team, 0x31u);
            FUN_0004b49f(buf, buf);
            (void)FUN_00050165(0x11184cu);
            break;
        case 0xc: FUN_000aeef8(U32(0x124274)); FUN_000af120(U32(0x124274)); break;
        default: break;
        }
    }
    s2_sp = sp0;
}

/* (a model of the library by name, into its slot; failing stops the game) */
static void s2r_menu_model(uint32_t name, uint32_t slot)
{
    const int r = FUN_0004b843(0x11da14, name, slot);
    if (r != 0) { (void)FUN_0004b169(3); FUN_0004b1a4(r); FUN_000ae662((uint32_t)r); }
}

/*
 * A PAGE'S THINGS LOADED (0x44587: the page's list).  What the last page had and this one does not
 * is let go first (0x44f72 from the list's second run); then each thing of the first run not yet
 * loaded (0x1323cc..): 1 the fonts ("menuL.fnt" of graf1.dat), 2 the sprites ("menuL.spr"), 3 the
 * texts ("strings"), 4 the page's backdrop (its name at 0x130bc8, of gfx\graf1.dat) with its
 * palette, 5 the team's room (menu<team>1.dat, "map<team>", five shadows) and its palette (kept at
 * 0x132c98), 6 the team's five cars ("auto1".."auto5" of cars<team>01.dat) and the reflections,
 * 7 the title's models (logo1.dat: the four teams' "a<n>log-00" twice, 64 letters), 8 the 3D
 * view's size and clip, 9 the tracks' maps ("amap<track>-vs" of maps1.dat), 10 the leagues'
 * plates, 11 the two teams' cars (the second 0x1289f0) and reflections, 12 the menu's sounds
 * ("menu" of data\sound).  The page's fades and clocks reset (0x132498.., 0x4ce0a, the steps 0.02).
 */
#define HOOK_44587
static void FUN_00044587(int list)
{
    const uint32_t sp0 = s2_sp, fp = s2_push(0x130) + 0x130, buf = fp - 0xb0, entry = fp - 0x130;
    uint32_t p = (uint32_t)list;
    while (I32(p) != -1) p += 4;
    FUN_00044f72((int)(p + 4), 1);
    for (p = (uint32_t)list; I32(p) != -1; p += 4) {
        const uint32_t k = U32(p);
        if (I32(0x1323ccu + 4 * k) != 0) continue;
        I32(0x1323ccu + 4 * k) = 1;
        const uint32_t team = U32(0x1289ec) + 0x31u;
        switch (k) {
        case 1:
            FUN_0004eaed(buf, 0x11161fu, 0x11eff8u, 0x31u);
            FUN_0004eaed(entry, 0x111631u, 0x4cu);
            FUN_00050dd3(0x12420c, (int)buf, (int)entry);
            break;
        case 2:
            FUN_0004eaed(buf, 0x11164fu, 0x11eff8u, 0x31u);
            FUN_0004eaed(entry, 0x111661u, 0x4cu);
            FUN_00050a9c(0x12420c, (int)buf, (int)entry);
            break;
        case 3: FUN_0003248f(0x11166cu, 0x127ed8); break;
        case 4:
            FUN_0004eaed(buf, 0x111674u, 0x31u);
            FUN_00051532(I32(0x1323c8), (int)buf, 0x130bc8);
            FUN_000358b9();
            FUN_000ae6e3(0x1338ccu, U32(U32(0x1323c8) + 4), 0x300);
            FUN_00051664(U32(U32(0x1323c8) + 4), 0);
            (void)FUN_0005105e(0x12420cu, U32(U32(0x1323c8) + 4));
            break;
        case 5:
            FUN_0004eaed(buf, 0x111683u, 0x11eff8u, team, 0x31u);
            FUN_0004b49f(buf, buf);
            FUN_0004eaed(buf, 0x111697u, team);
            FUN_0004feb4(buf, 3);
            FUN_0004fca9(buf);
            for (uint32_t i = 0; i < 5; i++) s2r_menu_model(0x11169du, 0x131030u + 8 * i);
            FUN_0004fe21(buf, 0x1338ccu, 0, 0);
            FUN_000ae6e3(0x132c98u, 0x1338ccu, 0x300);
            break;
        case 6:
            FUN_0004eaed(buf, 0x1116a5u, 0x11eff8u, team, 0x31u);
            FUN_0004b49f(buf, buf);
            if (I32(0x1323e0) == 0) (void)FUN_0004c1c3();
            for (uint32_t i = 0; i < 5; i++) {
                FUN_0004eaed(buf, 0x1116bau, i + 0x31u);
                FUN_0004feb4(buf, 9);
                s2r_menu_model(buf, 0x130fe0u + 8 * i);
            }
            FUN_0004eaed(buf, 0x1116c1u, 0x11eff8u, team, 0x31u);
            FUN_0004b49f(buf, buf);
            FUN_0004feb4(0x1116d5u, 4);
            break;
        case 7:
            (void)FUN_0004c1c3();
            FUN_0004eaed(buf, 0x111729u, 0x11eff8u, 0x31u);
            FUN_0004b49f(buf, buf);
            FUN_0004feb4(0x11173bu, 1);
            for (uint32_t i = 0; i < 4; i++) { FUN_0004eaed(buf, 0x111740u, i + 0x31u); s2r_menu_model(buf, 0x130f68u + 8 * i); }
            for (uint32_t i = 0; i < 4; i++) { FUN_0004eaed(buf, 0x11174au, i + 0x31u); s2r_menu_model(buf, 0x130f88u + 8 * i); }
            for (uint32_t i = 0; i < 0x40; i++) s2r_menu_model(0x111754u, 0x131090u + 8 * i);
            FUN_0004fe21(0x11175cu, 0x1338ccu, 0, 0);
            break;
        case 8:
            (void)FUN_0007f504(-0xab20, 0);
            (void)FUN_0004f6c4(0x140, 200, 100);
            FUN_00050d6c(0x12420c, 0, 0, 0x140, 200);
            FUN_00050fb9(0x12420c, 0, 0, 0x140, 200);
            break;
        case 9:
            FUN_0004eaed(buf, 0x111761u, 0x11eff8u, 0x31u);
            FUN_0004b49f(buf, buf);
            FUN_0004feb4(0x111773u, 1);
            if (I32(0x1323e0) == 0) (void)FUN_0004c1c3();
            for (uint32_t i = 0; i < 7; i++) { FUN_0004eaed(buf, 0x111778u, U32(0x113224u + 4 * i) + 0x31u); s2r_menu_model(buf, 0x131058u + 8 * i); }
            FUN_0004fe21(0x111782u, 0x1338ccu, 0, 0);
            break;
        case 10:
            if (I32(0x1323e0) == 0) (void)FUN_0004c1c3();
            FUN_0004eaed(buf, 0x111787u, 0x11eff8u, 0x31u);
            FUN_0004b49f(buf, buf);
            FUN_0004feb4(0x11179bu, 1);
            for (uint32_t i = 0; i < 7; i++) { FUN_0004eaed(buf, 0x1117a2u, U32(0x113224u + 4 * i) + 0x31u); s2r_menu_model(buf, 0x130fa8u + 8 * i); }
            FUN_0004fe21(0x1117abu, 0x1338ccu, 0, 0);
            break;
        case 0xb: {
            if (I32(0x1323e0) == 0) (void)FUN_0004c1c3();
            static const uint32_t form[2] = { 0x1116d9u, 0x1116f5u }, car[2] = { 0x1116eeu, 0x11170au }, slots[2] = { 0x130fe0u, 0x131008u };
            for (int t = 0; t < 2; t++) {
                FUN_0004eaed(buf, form[t], 0x11eff8u, (t == 0 ? U32(0x1289ec) : U32(0x1289f0)) + 0x31u, 0x31u);
                FUN_0004b49f(buf, buf);
                for (uint32_t i = 0; i < 5; i++) {
                    FUN_0004eaed(buf, car[t], i + 0x31u);
                    FUN_0004feb4(buf, 9);
                    if (FUN_0004bc94(0x11da14, buf, slots[t] + 8 * i) != 0) FUN_000ae662(0);
                    (void)FUN_0004b4d4(0x11da14, U32(slots[t] + 8 * i), 1);
                }
            }
            FUN_0004eaed(buf, 0x111711u, 0x11eff8u, team, 0x31u);
            FUN_0004b49f(buf, buf);
            FUN_0004feb4(0x111725u, 4);
            break;
        }
        case 0xc:
            FUN_0004eaed(buf, 0x11163cu, 0x11eff8u);
            FUN_000afc70(U32(0x124274), buf, 0x11164au);
            break;
        default: break;
        }
    }
    FUN_000ae8b0(0x132498u, 0, 0x34);
    U32(0x4ce0a) = 0;
    U32(0x133354) = 0x3ca3d70au; U32(0x133350) = 0x3ca3d70au;
    U32(0x4ce06) = 2;
    s2_sp = sp0;
}

/*
 * THE BEST TIMES PAGE (0x4254f): backdrop "hiscoreL.bkk"; four choices along the top (laps or
 * races 0x128a48, the level 0x128a4c, the laps 0x128a50 -- one for the hardest level --, the track
 * 0x128a54 of the open ones) and "view replay"; left / right move between them, ENTER steps the
 * chosen one round (the counts 0x1131ec..), up / down move along the fifteen times (six shown):
 * each its place, driver and time (0x37e06), its team's badge.  ENTER on "view replay" with a time
 * there names the replay ("REP<laps><level><track><place>", 0x130d34) and plays it (0x1324e0 =
 * -1); ESC back to the options.  An empty time is shaded.  The fade.
 */
#define HOOK_4254f
static void FUN_0004254f(void)
{
    typedef long double ld;
    const uint32_t sp0 = s2_sp, fp = s2_push(0x130) + 0x130;
    const uint32_t place = fp - 0x60, time = fp - 0x80, line = fp - 0x100, chosen = fp - 0x20;
    int item = 0, row = 0, level = I32(fp - 0x2c);
    float scroll = 0.0f;
    I32(chosen) = 0;
    FUN_0004bda7(0x11da14, 0x11d6f4);
    I32(0x132494) = 0;
    FUN_0004eaed(0x130bc8u, 0x111568u, 0x4cu);
    FUN_00044587(0x113374);
    do {
        FUN_00038d48();
        FUN_00051b02(0, 0, U32(0x1323c8) + 8, U32(U32(0x1323c8)));
        I32(0x131f1c) = (int)FUN_00038996(0, 0);
        if (I32(0x131f1c) != 0 && (U32(0x132498) & 0x7fffffff) == 0) {
            const uint32_t key = U32(0x131f20);
            if (key == 1) { if (row != 0) row--; }
            else if (key == 2) { if (row < 0xe) row++; }
            else if (key == 4) { if (item != 0) { item--; U32(0x4ce0a) = 0x20; } }
            else if (key == 8) { if (item < 4) { item++; U32(0x4ce0a) = 0x20; } }
            else if (key == 0x10) {
                if (item == 4) {
                    if (I32(I32(chosen)) != 0) {
                        level = U32(0x128a4c) == 0 ? 1 : U32(0x128a4c) == 1 ? 0 : 2;
                        FUN_0004eaed(0x130d34u, 0x111576u, U32(0x128a50) + 0x30u, (uint32_t)level + 0x30u, U32(0x113224u + 4u * U32(0x128a54)) + 0x30u, U32(I32(chosen)) + 0x41u);
                        U32(0x132498) = 0x3f800000u;
                        I32(0x1324e0) = -1;
                    }
                } else if (item != 2 || (U32(0x128a4c) < 2 && U32(0x128a48) == 0)) {
                    const uint32_t s = 0x128a48u + 4u * (uint32_t)item;
                    I32(s) += 1;
                    if (item == 3) { if (I32(0x128a54) >= (I32(0x128a14) == 4 ? I32(0x1131f8) : I32(0x1131f8) - 1)) I32(0x128a54) = 0; }
                    else if (I32(0x1131ecu + 4u * (uint32_t)item) <= I32(s)) I32(s) = 0;
                }
            } else if (key == 0x20) { U32(0x1324a0) = 0x3f800000u; U32(0x132498) = 0x3f800000u; I32(0x132494) = 0x420c0; }
        }
        if (U32(0x128a4c) > 1) I32(0x128a50) = 1;
        FUN_0003327c(U32(0x12810c), 4, 0, 1, 2);
        const int dim = (U32(0x4ce0a) & 0x20) == 0;
        int names = 0;
        FUN_000332dd(U32(0x128110u + 4u * U32(0x128a48)), 0x2c, 0xa0, 0, dim || item != 0 ? 2 : 3, dim || item != 0 ? 1 : 3);
        names += I32(0x1131ec);
        FUN_000332dd(U32(0x128110u + 4u * (U32(0x128a4c) + (uint32_t)names)), 0x2c, 0xa0, 0xa0, dim || item != 1 ? 2 : 3, dim || item != 1 ? 1 : 3);
        names += I32(0x1131f0);
        FUN_000332dd(U32(0x128110u + 4u * (uint32_t)(I32(0x128a50) + names)), 0x41, 0xa0, 0, dim || item != 2 ? 2 : 3, dim || item != 2 ? 1 : 3);
        names += I32(0x1131f4);
        FUN_000332dd(U32(0x128110u + 4u * (uint32_t)(I32(0x113224u + 4u * U32(0x128a54)) + names)), 0x41, 0xa0, 0xa0, dim || item != 3 ? 2 : 3, dim || item != 3 ? 1 : 3);
        names += I32(0x1131f8);
        FUN_0003327c(U32(0x128110u + 4u * (uint32_t)names), 0x55, dim || item != 4 ? 2 : 3, dim || item != 4 ? 1 : 3, 2);
        FUN_00050fb9(0x12420c, 0, 0x79, 0x140, 0xb4);
        FUN_00050d6c(0x12420c, 0, 0x77, 0x140, 0xb4);
        int y = (int32_t)truncl((double)120.0 - (ld)scroll * (double)12.0);
        const int top = row < 0xc ? (row < 3 ? 0 : row - 2) : 10;
        const uint32_t track = U32(0x113224u + 4u * U32(0x128a54));
        if (U32(0x128a48) == 0) {
            level = U32(0x128a4c) == 1 ? 0 : U32(0x128a4c) == 2 ? 2 : 1;
            const uint32_t t = U32(0x128a50) * 300u + track * 0x4b0u + (uint32_t)level * 0x20d0u;
            I32(chosen) = (int)(0x129134u + 4u * (uint32_t)row + t);
            for (int i = top; i < top + 6 && i < 0xf; i++, y += 0xc) {
                (void)FUN_000b743d((uint32_t)i + 1, place, 10);
                FUN_00037e06(time, t, 0, 0, F32(0x1290bcu + 4u * (uint32_t)i + t));
                FUN_0004eaed(line, i < 9 ? 0x111592u : 0x1115a3u, place, t + 0x1290f8u + 4u * (uint32_t)i, time);
                FUN_00050f45(0x12420c, 0x32, y, line, i == row ? 3 : 2, i == row ? 3 : 1, 2);
                FUN_00050c0a(0x12420c, 0x86, y - 1, I32(0x129170u + 4u * (uint32_t)i + t) * 5 + 0x55 + I32(0x1291acu + 4u * (uint32_t)i + t), 0);
            }
        } else {
            const uint32_t t = (uint32_t)level * 0x834u + track * 300u;
            I32(chosen) = (int)(0x12f3a4u + 4u * (uint32_t)row + t);
            for (int i = top; i < top + 6 && i < 0xf; i++, y += 0xc) {
                (void)FUN_000b743d((uint32_t)i + 1, place, 10);
                FUN_00037e06(time, t, 0, 0, F32(0x12f32cu + 4u * (uint32_t)i + t));
                FUN_0004eaed(line, i < 9 ? 0x1115b3u : 0x1115c4u, place, t + 0x12f368u + 4u * (uint32_t)i, time);
                FUN_00050f45(0x12420c, 0x32, y, line, i == row ? 3 : 2, i == row ? 3 : 1, 2);
                FUN_00050c0a(0x12420c, 0x86, y - 1, I32(0x12f3e0u + t + 4u * (uint32_t)i) * 5 + 0x55 + I32(0x12f41cu + 4u * (uint32_t)i + t), 0);
            }
        }
        if (I32(I32(chosen)) == 0) { FUN_00051698(0x21, 0x51, 0x102, 0x12); FUN_00051698(0x21, 0x51, 0x102, 0x12); FUN_00051698(0x21, 0x51, 0x102, 0x12); }
        FUN_00050fb9(0x12420c, 0, 0, 0x140, 200);
        FUN_00050d6c(0x12420c, 0, 0, 0x140, 200);
        if ((U32(0x132498) & 0x7fffffff) != 0) F32(0x13249c) = (float)((ld)F32(0x133350) * 2.0f + F32(0x13249c));
        if (I32(0x13249c) >= 0x3f800000) { F32(0x13249c) = 1.0f; U32(0x1324a0) = 0x3f800000u; }
        if (fabsf(F32(0x13249c)) != 0.0f) FUN_000512f1(F32(0x13249c), 0, 0, 0);
        FUN_0007f5a2(0, 0);
    } while ((U32(0x1324a0) & 0x7fffffff) == 0);
    FUN_000358b9();
    FUN_00044f72(0x113374, 0);
    s2_sp = sp0;
}

/*
 * THE LEAGUE'S REPORT (0x43914), after a championship race.  The race's order (each car's place
 * +0xc30, from 1): team, colour, time (+0xc38), car into 0x132388.. / 0x132398.. / 0x1323b8.. /
 * 0x1323a8.. (a place a word); the player not finished is put last (time 0x4560fffc).  In a network
 * game a car out of the race (+0x4c bit 2 without bit 1) gets that time; a player gone is dropped.
 * Points by place, fewer when cars of the same team share the race (0x113180, a row of four by how
 * many teammates), added to each car's total (0x128a74.., 0xc a car, with its team and name).
 * Backdrop "leag_<league>L.bkk"; the totals sorted (0x35916) and listed with the teams' badges and
 * the races done and to go.  ENTER: the next race (0x3edb4) while the league has races left; else
 * the result -- the winner (the player: the league's prize car 0x128aa0.., the next league open
 * 0x128a18, more tracks 0x128a14 -- all four teams' prize cars for the last tracks) shown as won
 * (1) or lost (2) -- and then on to the new league's scene (0x3bbc4) or the championship page.
 * At the end each car's grid place for the next race (0x130c68: 3 - its place in the table).
 */
#define HOOK_43914
static void FUN_00043914(void)
{
    typedef long double ld;
    const uint32_t sp0 = s2_sp, fp = s2_push(0xb0) + 0xb0, table = fp - 0x54, line = fp - 0x94, spare = fp - 0x34;
    const uint32_t league = U32(0x128a10);
    FUN_0004bda7(0x11da14, 0x11d6f4);
    I32(0x132494) = 0;
    if (I32(0x1324e4) == 0) {
        if (!(U32(0x11934c) & 2)) {
            U32(0x132384) = U32(0x130c48);
            for (int i = 0; i < I32(0x130c48); i++) {
                const uint32_t car = 0x119300u + 0xcc0u * (uint32_t)i, at = 4u * U32(car + 0xc30), e = 0x130c58u + 0x2cu * (uint32_t)i;
                U32(0x132384u + at) = U32(e); U32(0x132394u + at) = U32(e + 4);
                F32(0x1323b4u + at) = F32(car + 0xc38); I32(0x1323a4u + at) = i;
            }
            uint32_t i = 0;
            while (U32(0x132388u + 4 * i) != U32(0x1289ec)) i++;
            if (i != 3) {
                U32(0x132388u + 4 * i) = U32(0x132394); U32(0x132398u + 4 * i) = U32(0x1323a4);
                F32(0x1323b8u + 4 * i) = F32(0x1323c4); U32(0x1323a8u + 4 * i) = U32(0x1323b4);
                U32(0x132394) = U32(0x130c58); U32(0x1323a4) = U32(0x130c5c); U32(0x1323c4) = 0x4560fffcu; U32(0x1323b4) = 0;
            }
        }
    } else {
        U32(0x132384) = U32(0x130c48);
        for (int i = 0; i < I32(0x130c48); i++) {
            const uint32_t car = 0x119300u + 0xcc0u * (uint32_t)i, at = 4u * U32(car + 0xc30), e = 0x130c58u + 0x2cu * (uint32_t)i;
            if (!(U8(car + 0x4c) & 2) && (U8(car + 0x4c) & 4)) U32(0x1323b4u + at) = 0x4560fffcu;
            else F32(0x1323b4u + at) = F32(car + 0xc38);
            U32(0x132384u + at) = U32(e); U32(0x132394u + at) = U32(e + 4); I32(0x1323a4u + at) = i;
        }
    }
    for (uint32_t i = 0; i < U32(0x132384); i++) {
        const uint32_t car = U32(0x1323a8u + 4 * i);
        int mates = 0;
        for (uint32_t j = 0; j < U32(0x132384); j++) if (U32(0x130c58u + 0x2cu * car) == U32(0x130c58u + 0x2cu * j)) mates++;
        mates--;
        I32(0x128a74u + 0xcu * car) += I32(0x113180u + 4 * i + 0x10u * (uint32_t)mates);
        U32(0x128a6cu + 0xcu * car) = U32(0x130c58u + 0x2cu * car);
        FUN_000b3be4(0x128a70u + 0xcu * car, 0x130c60u + 0x2cu * car);
    }
    if (I32(0x1324e4) == 1) {
        if (!(U8(0x11934cu + 0xcc0u * U32(0x130d08)) & 2)) { FUN_00037167(); I32(0x132494) = 0x3bf06; s2_sp = sp0; return; }
        for (int i = 0; i < I32(0x131f2c); i++) {
            const uint32_t flags = U8(0x11934cu + 0xcc0u * (uint32_t)i);
            if (!(flags & 2) && (flags & 4)) {
                i = FUN_000b5ca0();
                if (i != -1) { (void)FUN_000b5c63(); (void)FUN_000b5b17(); (void)FUN_000b5fa4(); }
                I32(0x131f2c) = FUN_000b5b0b();
                I32(0x131f28) = FUN_000b5b11();
                if (i < I32(0x130c48) - 1) {
                    int j;
                    FUN_000ae6e3(spare, 0x128a6cu + 0xcu * (uint32_t)i, 0xc);
                    for (j = i; j < I32(0x130c48) - 1; j++) FUN_000ae6e3(0x128a6cu + 0xcu * (uint32_t)j, 0x128a6cu + 0xcu * (uint32_t)(j + 1), 0xc);
                    FUN_000ae6e3(0x128a6cu + 0xcu * (uint32_t)j, spare, 0xc);
                }
                I32(0x130c48) -= 1;
            }
        }
        if (I32(0x131f2c) < 2) { FUN_00037167(); I32(0x132494) = 0x3bf06; s2_sp = sp0; return; }
    } else if (I32(0x1324e4) == 2 && (!(U8(0x11934cu + 0xcc0u * U32(0x130d08)) & 2) || !(U8(0x11934cu + 0xcc0u * (1u - U32(0x130d08))) & 2))) {
        FUN_000370ed(); I32(0x132494) = 0x3bf06; s2_sp = sp0; return;
    }
    FUN_0004eaed(0x130bc8u, 0x1115e8u, league + 0x31u, 0x4cu);
    FUN_00044587(0x113374);
    for (int i = 0; i < I32(0x130c48); i++) { I32(table + 8u * (uint32_t)i) = I32(0x128a74u + 0xcu * (uint32_t)i); I32(table + 8u * (uint32_t)i + 4) = i; }
    FUN_00035916(I32(0x130c48), (int)table, 0, 'l', 8, -1);
    FUN_000ae6e3(0x1338ccu, U32(U32(0x1323c8) + 4), 0x300);
    FUN_00051664(U32(U32(0x1323c8) + 4), 0);
    (void)FUN_0005105e(0x12420cu, U32(U32(0x1323c8) + 4));
    const uint32_t races = 0x1130b0u + 0x1cu * U32(0x128a10);
    do {
        FUN_00038d48();
        FUN_00051b02(0, 0, U32(0x1323c8) + 8, U32(U32(0x1323c8)));
        const int key = (int)FUN_00038996(1, 0) != 0 && (U32(0x132498) & 0x7fffffff) == 0;
        I32(0x131f1c) = key;
        if (key && I32(0x131f20) == 0x10) {
            if (I32(0x1324c4) == 0) {
                if (I32(0x128a1c) < I32(races)) { U32(0x132498) = 0x3f800000u; I32(0x132494) = 0x3edb4; }
                else if (I32(0x1324e4) == 0) {
                    if (I32(table + 4) == I32(0x131f28)) {
                        if (I32(races + 8) != 0) U32(0x128aa0u + 0x188u * U32(0x1289ec)) = 1;
                        if (I32(races + 0xc) != 0) I32(0x128a18) = I32(0x128a18) < I32(races + 0xc) ? I32(races + 0xc) : I32(0x128a18);
                        if (I32(races + 4) == 0) { if (U32(0x128aa0) != 0 && I32(0x128c28) != 0 && I32(0x128db0) != 0 && I32(0x128f38) != 0) I32(0x128a14) = 4; }
                        else I32(0x128a14) = I32(0x128a14) < I32(races + 4) ? I32(races + 4) : I32(0x128a14);
                        I32(0x1324c4) = 1;
                    } else I32(0x1324c4) = 2;
                } else if (I32(0x1324e4) == 1 || I32(0x1324e4) == 2)
                    I32(0x1324c4) = I32(0x128a6cu + 0xcu * U32(table + 4)) == I32(0x1289ec) ? 1 : 2;
            } else {
                if (I32(0x1324e4) == 0) I32(0x132494) = I32(0x1324c4) == 2 ? 0x431b3 : 0x3bbc4;
                else if (I32(0x1324e4) == 1) { FUN_00037167(); I32(0x132494) = 0x3bf06; }
                else { FUN_000370ed(); I32(0x132494) = 0x3bf06; }
                U32(0x132498) = 0x3f800000u;
            }
        }
        if (I32(0x1324c4) == 0) {
            FUN_0003327c(U32(0x12802c), 4, 0, 1, 2);
            int y = 0x38;
            for (int i = 0; i < I32(0x130c48); i++, y += 0x18) {
                const uint32_t pts = U32(table + 8u * (uint32_t)i), car = U32(table + 8u * (uint32_t)i + 4);
                FUN_0004eaed(line, (int32_t)pts < 10 ? 0x1115f6u : 0x111603u, (uint32_t)i + 0x31u, 0x128a70u + 0xcu * car, pts);
                FUN_00050c0a(0x12420c, 0x68, y, I32(0x128a6cu + 0xcu * car) + 0x51, 0);
                FUN_0003327c(line, (uint32_t)y, 1, 1, 2);
            }
            if (I32(races) != I32(0x128a1c)) {
                FUN_0004eaed(line, 0x11160fu, U32(0x128154), U32(0x128a1c));
                FUN_0003327c(line, (uint32_t)y, 2, 1, 2);
                y += 0xc;
                FUN_0004eaed(line, 0x111615u, U32(0x128158), (uint32_t)(I32(races) - I32(0x128a1c)));
                FUN_0003327c(line, (uint32_t)y, 2, 1, 2);
            }
        } else if (I32(0x1324c4) == 1) {
            uint32_t y = 0x42;
            for (uint32_t i = 0; i < 2; i++, y += 0x18) FUN_0003327c(U32(0x128078u + 4 * i), y, 1, 1, 2);
            FUN_0003327c(U32(0x127f3cu + 8u * U32(0x128a10)), y, 1, 1, 2);
        } else if (I32(0x1324c4) == 2) {
            uint32_t y = 0x42;
            for (uint32_t i = 0; i < 2; i++, y += 0x18) FUN_0003327c(U32(0x128088u + 4 * i), y, 1, 1, 2);
        }
        if ((U32(0x4ce0a) & 0x7f) < 0x40) FUN_0003327c(U32(0x127f14), 0xb8, 2, 1, 2);
        if ((U32(0x132498) & 0x7fffffff) != 0) F32(0x13249c) = (float)((ld)F32(0x133350) * 2.0f + F32(0x13249c));
        if (I32(0x13249c) >= 0x3f800000) { F32(0x13249c) = 1.0f; U32(0x1324a0) = 0x3f800000u; }
        if (fabsf(F32(0x13249c)) != 0.0f) FUN_000512f1(F32(0x13249c), 0, 0, 0);
        FUN_0007f5a2(0, 0);
    } while ((U32(0x1324a0) & 0x7fffffff) == 0);
    for (int i = 0; i < I32(0x130c48); i++) {
        int k = 0;
        while (i != I32(table + 8u * (uint32_t)k + 4)) k++;
        I32(0x130c68u + 0x2cu * (uint32_t)i) = 3 - k;
    }
    FUN_000358b9();
    FUN_00044f72(0x113374, 0);
    s2_sp = sp0;
}

/*
 * A CHAMPIONSHIP RACE'S TRACK (0x3edb4): backdrop "tkg_<track>L.bkk"; the track's map model
 * (of the seven, 0x131058) spinning slowly in the view; the league's name, the track's, its facts
 * box (length, laps, record: 0x127fb0..).  ENTER goes on to the race's choices (0x3cbb8, a sound);
 * ESC before the league's first race goes back to the championship page, later it asks whether to
 * leave the league (0x1324c4: two lines and YES / NO, left / right, ENTER; leaving goes back to the
 * championship page).  Network games take the other machine's keys.  The fade at 4 a second.
 */
#define HOOK_3edb4
static void FUN_0003edb4(void)
{
    typedef long double ld;
    const uint32_t sp0 = s2_sp, fp = s2_push(0x40) + 0x40, look = fp - 0x38;
    int no = 1;
    const uint32_t track = U32(0x113224u + 4u * U32(0x128a1c));
    FUN_0004bda7(0x11da14, 0x11d6f4);
    I32(0x132494) = 0;
    FUN_0004eaed(0x130bc8u, 0x11144au, track + 0x31u, 0x4cu);
    FUN_00044587(0x113308);
    for (uint32_t i = 0; i < 7; i++) { FUN_0004be04(0x11da14, 0x11d6f4, (int)(0x131058u + 8 * i)); FUN_0004c01c(0x11da14, 0x11d6f4, (int)(0x131058u + 8 * i)); }
    {
        const uint32_t m = U32(0x11da20u + 0x1cu * U32(0x131058u + 8u * U32(0x128a1c)));
        I32(m + 0x10) = 0; I32(m + 0xc) = 0; I32(m + 8) = 0;
        I32(m + 0x18) = 0; I32(m + 0x1c) = 0; I32(m + 0x14) = 0;
        I32(m + 0x3c) = 0; I32(m + 0x20) = 0;
        FUN_0004bfbe(0x11da14, 0x11d6f4, (int)(0x131058u + 8u * U32(0x128a1c)));
    }
    if (U32(0x1324e4) == 1) {
        if (FUN_00037b41(0x2468, 0x2457, 1, 0) != 0) { FUN_00037167(); FUN_00044f72(0x113308, 0); s2_sp = sp0; return; }
    } else if (U32(0x1324e4) == 2 && FUN_00036d22(0x2468u, 0, 1)) { FUN_000370ed(); FUN_00044f72(0x113308, 0); s2_sp = sp0; return; }
    U32(0x4ce0a) = 0;
    U32(0x133354) = 0x3ca3d70au; U32(0x133350) = 0x3ca3d70au;
    U32(0x4ce06) = 2;
    FUN_000ae6e3(0x1338ccu, U32(U32(0x1323c8) + 4), 0x300);
    FUN_00051664(U32(U32(0x1323c8) + 4), 0);
    (void)FUN_0005105e(0x12420cu, U32(U32(0x1323c8) + 4));
    do {
        FUN_00038d48();
        FUN_00051b02(0, 0, U32(0x1323c8) + 8, U32(U32(0x1323c8)));
        const uint32_t m = U32(0x11da14u + 0x1cu * U32(0x131058u + 8u * U32(0x128a1c)) + 0xc);
        I32(look) = 0x45c; I32(look + 4) = 0x4b; I32(look + 8) = 0x24c;
        U32(0x131eb0) = 0x450; U32(0x131eb4) = 0xce4; I32(0x131eb8) = -0x594;
        FUN_000338ed(0x1324a8, look, 0x131eb0);
        U32(0x131ea4) = (uint32_t)(int32_t)truncl((ld)F32(0x1324a8) * 1024.0f / (float)3.14159274f) & 0x7ff;
        U32(0x131ea8) = (uint32_t)(int32_t)truncl((ld)F32(0x1324ac) * 1024.0f / (float)3.14159274f) & 0x7ff;
        U32(0x131eac) = 0;
        FUN_0007e8b4(0x131ea4, 0);
        I32(m + 0x18) = s2_turn_by(m + 0x18, 256.0f);
        U32(m + 0x18) &= 0x7ff;
        FUN_000753a0(0x11d6f4u, 0);
        FUN_0003327c(U32(0x127f3cu + 8u * U32(0x128a10)), 4, 0, 1, 2);
        FUN_0003343d((int)track + 0x13, 0x2c, 0xa0, 0xa0);
        FUN_00051698(0xa8, 0x5e, 0x92, 0x56);
        FUN_00050f45(0x12420c, 0xad, 0x60, U32(0x127fb0), 2, 1, 2);
        FUN_00050f45(0x12420c, 0xb9, 0x6c, U32(0x127fb8u + 0xcu * track), 2, 1, 2);
        FUN_00050f45(0x12420c, 0xb9, 0x78, U32(0x127fbcu + 0xcu * track), 2, 1, 2);
        FUN_00050f45(0x12420c, 0xad, 0x90, U32(0x127fb4), 2, 1, 2);
        FUN_00050f45(0x12420c, 0xb9, 0x9c, U32(0x127fc0u + 0xcu * track), 2, 1, 2);
        I32(0x131f1c) = (int)FUN_00038996(1, 0);
        const int fading = (U32(0x132498) & 0x7fffffff) != 0;
        if (U32(0x1324e4) == 1 && !fading) {
            if (FUN_00037b41(0xdef0, 0x7137, 0, 0) != 0) FUN_00037167();
            if (I32(0x131f28) != 0) { I32(0x131f1c) = I32(0x132534); U32(0x131f20) = U32(0x132530); }
        } else if (U32(0x1324e4) == 2 && !fading) {
            if (FUN_00036d22(0xdef0u, 0x7137, 0)) FUN_000370ed();
            if (I32(0x131f28) != 0) { I32(0x131f1c) = I32(0x132bd4); U32(0x131f20) = U32(0x132bd0); }
        }
        if (I32(0x131f1c) != 0 && (U32(0x132498) & 0x7fffffff) == 0) {
            const uint32_t key = U32(0x131f20);
            if (key == 4) { if (no != 0 && I32(0x1324c4) != 0) no = 0; }
            else if (key == 8) { if (no == 0 && I32(0x1324c4) != 0) no = 1; }
            else if (key == 0x10) {
                if (I32(0x1324c4) == 0) { U32(0x132498) = 0x3f800000u; U32(0x1324d4) = 0; I32(0x132494) = 0x3cbb8; FUN_000aea54(U32(0x124274), 0xd, 0, 0, 0, 0); }
                else if (no == 0) {
                    if (U32(0x1324e4) == 1) FUN_00037167();
                    else if (U32(0x1324e4) == 2) FUN_000370ed();
                    else { U32(0x1324a0) = 0x3f800000u; U32(0x132498) = 0x3f800000u; I32(0x132494) = 0x431b3; }
                } else I32(0x1324c4) = 0;
            } else if (key == 0x20) {
                if (I32(0x128a1c) == 0) {
                    if (U32(0x1324e4) == 1) FUN_00037167();
                    else if (U32(0x1324e4) == 2) FUN_000370ed();
                    else { U32(0x1324a0) = 0x3f800000u; U32(0x132498) = 0x3f800000u; I32(0x132494) = 0x431b3; }
                } else I32(0x1324c4) = I32(0x1324c4) == 0 ? 1 : 0;
            }
        }
        if ((U32(0x132498) & 0x7fffffff) != 0) F32(0x13249c) = (float)((ld)F32(0x133350) * 4.0f + F32(0x13249c));
        if (I32(0x13249c) >= 0x3f800000) { F32(0x13249c) = 1.0f; U32(0x1324a0) = 0x3f800000u; }
        if (fabsf(F32(0x13249c)) != 0.0f) FUN_000512f1(F32(0x13249c), 0, 0, 0);
        if (I32(0x1324c4) == 0) {
            if ((U32(0x4ce0a) & 0x7f) < 0x40 && (U32(0x1324e4) == 0 || I32(0x131f28) == 0)) FUN_0003327c(U32(0x127f14), 0xb8, 2, 1, 2);
        } else {
            FUN_00051698(0, 0, 0x140, 200);
            FUN_000332dd(U32(0x127fa0), 0x50, 0xa0, 0, 2, 1);
            FUN_000332dd(U32(0x127fa4), 0x5f, 0xa0, 0, 2, 1);
            const int lit_yes = no == 0 && (U32(0x4ce0a) & 0x3f) < 0x21, lit_no = !(no == 0 || (U32(0x4ce0a) & 0x3f) > 0x20);
            FUN_000332dd(U32(0x127fa8), 0x73, 0x50, 0, lit_yes ? 3 : 2, lit_yes ? 3 : 1);
            FUN_000332dd(U32(0x127fac), 0x73, 0x50, 0x50, lit_no ? 3 : 2, lit_no ? 3 : 1);
        }
        if ((U32(0x1324e4) & 3) != 0 && I32(0x131f28) != 0) {
            FUN_00051698(0, 0, 0x140, 200);
            if (U32(0x4ce0a) & 0x40) FUN_00037686();
        }
        FUN_0007f5a2(0, 0);
    } while ((U32(0x1324a0) & 0x7fffffff) == 0);
    FUN_000358b9();
    FUN_00044f72(0x113308, 0);
    s2_sp = sp0;
}

/*
 * THE TIME ATTACK PAGE (0x3f72a): backdrop "leag_0L.bkk"; no best laps yet (0x130d18..); the open
 * tracks' map models (the first one's place cleared, as the game does), the chosen one (0x128a0c)
 * spinning in view.  Left / right (when the last move is done) slide the old map out and the new
 * one in over a quarter second (0x3385b from / to, a sound); its name and facts box; ENTER goes on
 * to the race's choices (0x3cbb8, a sound); ESC back to the main menu.  The fade at 4 a second.
 */
#define HOOK_3f72a
static void FUN_0003f72a(void)
{
    typedef long double ld;
    const uint32_t sp0 = s2_sp, fp = s2_push(0x90) + 0x90, to = fp - 0x3c, mid = fp - 0x48, from = fp - 0x54;
    float slide = 1.0f;
    FUN_0004bda7(0x11da14, 0x11d6f4);
    I32(0x132494) = 0;
    FUN_0004eaed(0x130bc8u, 0x111467u, 0x4cu);
    FUN_00044587(0x113308);
    U32(0x1324d4) = 0;
    for (uint32_t i = 0; i < 5; i++) U32(0x130d1cu + 4 * i) = 0x4560fffcu;
    U32(0x130d18) = 0x4560fffcu;
    if (I32(0x128a14) + 2 < I32(0x128a0c)) I32(0x128a0c) = 0;
    {
        const uint32_t m = U32(0x11da14u + 0x1cu * U32(0x131058) + 0xc);
        for (uint32_t i = 0; i < 7; i++) {
            I32(m + 0x10) = 0; I32(m + 0xc) = 0; I32(m + 8) = 0;
            I32(m + 0x18) = 0; I32(m + 0x1c) = 0; I32(m + 0x14) = 0;
            I32(m + 0x3c) = 0; I32(m + 0x20) = 0;
            FUN_0004be04(0x11da14, 0x11d6f4, (int)(0x131058u + 8 * i));
            FUN_0004bfbe(0x11da14, 0x11d6f4, (int)(0x131058u + 8 * i));
        }
    }
    int was = I32(0x128a0c);
    do {
        FUN_00038d48();
        I32(mid + 8) = 0; I32(mid + 4) = 0; I32(mid) = 0;
        const int key = (int)FUN_00038996(0, 0) != 0 && (U32(0x132498) & 0x7fffffff) == 0;
        I32(0x131f1c) = key;
        if (key) {
            const uint32_t k = U32(0x131f20);
            if (k == 4 && I32(0x128a0c) != 0 && slide == 1.0f) {
                was = I32(0x128a0c); I32(0x128a0c) -= 1;
                I32(to) = -0x3c00; I32(to + 8) = 0; I32(to + 4) = -0xa00;
                U32(0x4ce0a) = 0;
                I32(from) = -0x3200; I32(from + 4) = -0xc800; I32(from + 8) = -0x3200;
                slide = 0.0f;
                FUN_000aea54(U32(0x124274), 0xc, 0, 0, 0, 0);
            } else if (k == 8 && I32(0x128a0c) < I32(0x128a14) + 2 && slide == 1.0f) {
                was = I32(0x128a0c); I32(0x128a0c) += 1;
                I32(to) = -0x3200; I32(to + 4) = -0xc800; I32(to + 8) = -0x3200;
                U32(0x4ce0a) = 0;
                I32(from) = -0x3c00; I32(from + 4) = -0xa00; I32(from + 8) = 0;
                slide = 0.0f;
                FUN_000aea54(U32(0x124274), 0xc, 0, 0, 0, 0);
            } else if (k == 0x10) { U32(0x132498) = 0x3f800000u; U32(0x1324d4) = 0; I32(0x132494) = 0x3cbb8; FUN_000aea54(U32(0x124274), 0xd, 0, 0, 0, 0); }
            else if (k == 0x20) { U32(0x1324a0) = 0x3f800000u; U32(0x132498) = 0x3f800000u; I32(0x132494) = 0x38d8d; }
        }
        for (int i = 0; i < 7; i++) {
            if (i != I32(0x128a0c) && i != was) FUN_0004c01c(0x11da14, 0x11d6f4, (int)(0x131058u + 8u * (uint32_t)i));
            const uint32_t m = U32(0x11da20u + 0x1cu * U32(0x131058u + 8u * (uint32_t)i));
            I32(m + 0x18) = s2_turn_by(m + 0x18, 256.0f);
            U32(m + 0x18) &= 0x7ff;
        }
        FUN_0004bfbe(0x11da14, 0x11d6f4, (int)(0x131058u + 8u * U32(0x128a0c)));
        FUN_00051b02(0, 0, U32(0x1323c8) + 8, U32(U32(0x1323c8)));
        if (was != I32(0x128a0c)) {
            FUN_0003385b((int)(U32(0x11da20u + 0x1cu * U32(0x131058u + 8u * (uint32_t)was)) + 8), (int)from, (int)mid, 0, slide);
            FUN_0003385b((int)(U32(0x11da20u + 0x1cu * U32(0x131058u + 8u * U32(0x128a0c))) + 8), (int)mid, (int)to, 0, slide);
        }
        I32(mid) = 0x45c; I32(mid + 4) = 0x4b; I32(mid + 8) = 0x24c;
        U32(0x131eb0) = 0x450; U32(0x131eb4) = 0xce4; I32(0x131eb8) = -0x594;
        FUN_000338ed(0x1324a8, mid, 0x131eb0);
        U32(0x131ea4) = (uint32_t)(int32_t)truncl((ld)F32(0x1324a8) * 1024.0f / (float)3.14159274f) & 0x7ff;
        U32(0x131ea8) = (uint32_t)(int32_t)truncl((ld)F32(0x1324ac) * 1024.0f / (float)3.14159274f) & 0x7ff;
        U32(0x131eac) = 0;
        FUN_0007e8b4(0x131ea4, 0);
        FUN_000753a0(0x11d6f4u, 0);
        FUN_0003327c(U32(0x127f10), 4, 0, 1, 2);
        const uint32_t track = U32(0x113224u + 4u * (uint32_t)(slide == 1.0f ? I32(0x128a0c) : was));
        FUN_0003343d((int)track + 0x13, 0x2c, 0xa0, 0xa0);
        FUN_00051698(0xa8, 0x5e, 0x92, 0x56);
        FUN_00050f45(0x12420c, 0xad, 0x60, U32(0x127fb0), 2, 1, 2);
        FUN_00050f45(0x12420c, 0xb9, 0x6c, U32(0x127fb8u + 0xcu * track), 2, 1, 2);
        FUN_00050f45(0x12420c, 0xb9, 0x78, U32(0x127fbcu + 0xcu * track), 2, 1, 2);
        FUN_00050f45(0x12420c, 0xad, 0x90, U32(0x127fb4), 2, 1, 2);
        FUN_00050f45(0x12420c, 0xb9, 0x9c, U32(0x127fc0u + 0xcu * track), 2, 1, 2);
        if ((U32(0x4ce0a) & 0x7f) < 0x40) {
            if (I32(0x128a0c) != 0) FUN_00050c0a(0x12420c, 10, 0xac, 1, 0);
            if (I32(0x128a0c) < I32(0x128a14) + 2) FUN_00050c0a(0x12420c, 0x12a, 0xac, 2, 0);
        }
        slide = (float)((ld)F32(0x133350) * 4.0f + slide);
        if (s2_bits(slide) > 0x3f800000) slide = 1.0f;
        if ((U32(0x132498) & 0x7fffffff) != 0) F32(0x13249c) = (float)((ld)F32(0x133350) * 4.0f + F32(0x13249c));
        if (I32(0x13249c) >= 0x3f800000) { F32(0x13249c) = 1.0f; U32(0x1324a0) = 0x3f800000u; }
        if (fabsf(F32(0x13249c)) != 0.0f) FUN_000512f1(F32(0x13249c), 0, 0, 0);
        FUN_0007f5a2(0, 0);
    } while ((U32(0x1324a0) & 0x7fffffff) == 0);
    FUN_000358b9();
    FUN_00044f72(0x113308, 0);
    s2_sp = sp0;
}

/*
 * THE NETWORK PAGE (0x3bf06): backdrop "bkg_6L.bkk"; three ways (0x1324c4 = 0: up / down blink):
 * one machine (both players' controls into 0x127708.. and on to the two players' page 0x3988d),
 * a network game (the network opened: waiting for players, 0x371fd; not opened: a message for
 * five seconds) or a link game (the line opened: waiting for the other machine; ESC gives up).
 * Once started (0x1324c8): arcade or championship (up / down) for all the players -- their grid
 * places 0..3 -- on to the tracks (0x4189f) or the leagues (0x431b3).  ESC goes back a step: to the
 * main menu, or leaves the game (the network's own, which the port has not, answers nothing).
 */
#define HOOK_3bf06
static void FUN_0003bf06(void)
{
    typedef long double ld;
    const uint32_t sp0 = s2_sp, fp = s2_push(0x30) + 0x30;
    int row = 0, kind = 0, message = I32(fp - 0x2c);
    float shown = 0.0f;
    FUN_0004bda7(0x11da14, 0x11d6f4);
    I32(0x132494) = 0;
    FUN_0004eaed(0x130bc8u, 0x1112e2u, 0x4cu);
    FUN_00044587(0x113374);
    U32(0x131f30) = 0;
    do {
        FUN_00038d48();
        FUN_00051b02(0, 0, U32(0x1323c8) + 8, U32(U32(0x1323c8)));
        I32(0x131f1c) = (int)FUN_00038996(I32(0x1324e4) == 0 || I32(0x1324c8) != 0 || I32(0x1324c4) == 0 ? 0 : 1, 0);
        if (I32(0x1324c8) != 0 && (U32(0x132498) & 0x7fffffff) == 0) {
            if (I32(0x1324e4) == 1) {
                if (FUN_00037b41(0xdef0, 0x3344, 0, 0) != 0) FUN_00037167();
                if (I32(0x131f28) != 0) { I32(0x131f1c) = I32(0x132534); U32(0x131f20) = U32(0x132530); }
            } else if (I32(0x1324e4) == 2) {
                if (FUN_00036d22(0xdef0u, 0, 0)) FUN_000370ed();
                else if (I32(0x132bcc) == 0 && I32(0x132c40) == 0) FUN_000370ed();
                if (I32(0x131f28) != 0) { I32(0x131f1c) = I32(0x132bd4); U32(0x131f20) = U32(0x132bd0); }
            }
        }
        if (I32(0x131f1c) != 0 && (U32(0x132498) & 0x7fffffff) == 0) {
            const uint32_t key = U32(0x131f20);
            if (key == 1) {
                if (row == 0 || I32(0x1324c4) != 0) { if (I32(0x1324c8) != 0 && kind != 0) kind--; }
                else { row--; U32(0x4ce0a) = 0; }
            } else if (key == 2) {
                if (row < 2 && I32(0x1324c4) == 0) { row++; U32(0x4ce0a) = 0; }
                else if (I32(0x1324c8) != 0 && kind == 0) kind = 1;
            } else if (key == 0x10) {
                if (I32(0x1324c4) == 0) {
                    if (row == 0) {
                        U32(0x132498) = 0x3f800000u; I32(0x132494) = 0x3988d;
                        FUN_000ae6e3(0x127708u, 0x127588u, 0xa8);
                        FUN_000ae6e3(0x1277c8u, 0x127648u, 0xa8);
                    } else if (row == 1) {
                        (void)FUN_0004f3c8(U32(0x11d6ec), 0x1112f8u);
                        if (FUN_000b66e0() == 0) {
                            I32(0x1324c4) = 1; I32(0x1324e4) = 1;
                            FUN_000ae8b0(0x1324f0u, 0, 0x360);
                            I32(0x131f1c) = 0;
                            (void)FUN_000b5fa4();
                        } else {
                            if (FUN_0004f0c6(U32(0x11d6ec), 0x111302u, 0x4000) == 0) (void)FUN_0004f033(U32(0x11d6ec));
                            message = 0; shown = 5.0f;
                        }
                    } else if (row == 2) {
                        (void)FUN_0004f3c8(U32(0x11d6ec), 0x11130cu);
                        if (FUN_000b4980(U32(0x11f238), U32(0x11f23c), 0, 1, 8) == 0) {
                            if (FUN_0004f0c6(U32(0x11d6ec), 0x111316u, 0x4000) == 0) (void)FUN_0004f033(U32(0x11d6ec));
                            shown = 5.0f; message = 5;
                        } else {
                            if (I32(0x11f240) != 0) (void)FUN_000b4c46(U32(0x1347a4), 8);
                            FUN_0003327c(U32(0x128048), 4, 0, 1, 2);
                            FUN_0003327c(U32(0x128070), 100, 1, 1, 2);
                            FUN_0007f5a2(0, 0);
                            int linked;
                            while ((linked = FUN_000b5727()) == 0 && U8(0x100003) == 0) FUN_0004cca5(0, 0);
                            if (linked == 0) {
                                U32(0x131f20) = 0x20;
                                (void)FUN_000b57e9(); (void)FUN_000b4d73();
                                shown = 5.0f; message = 5;
                                if (FUN_0004f0c6(U32(0x11d6ec), 0x111320u, 0x4000) == 0) (void)FUN_0004f033(U32(0x11d6ec));
                            } else {
                                I32(0x131f28) = FUN_000b58b6();
                                I32(0x131f2c) = 2; I32(0x1324c4) = 2; I32(0x1324e4) = 2;
                                FUN_000ae8b0(0x132bb0u, 0, 0xe8);
                                I32(0x131f1c) = 0;
                            }
                        }
                    }
                } else if (I32(0x1324c8) != 0) {
                    U32(0x1324a0) = 0x3f800000u; U32(0x132498) = 0x3f800000u;
                    if (kind == 0) { U32(0x1324dc) = 1; I32(0x132494) = 0x4189f; }
                    else { U32(0x1324dc) = 0; I32(0x132494) = 0x431b3; }
                    for (uint32_t i = 0; i < 4; i++) U32(0x130c68u + 0x2cu * i) = i;
                }
            } else if (key == 0x20) {
                if (I32(0x1324c4) == 0) { U32(0x1324a0) = 0x3f800000u; U32(0x132498) = 0x3f800000u; I32(0x132494) = 0x38d8d; }
                else if (I32(0x1324c4) == 1) {
                    if (I32(0x1324c8) == 0) { I32(0x1324c4) = 0; FUN_00037167(); F32(0x1324a0) = 0.0f; U32(0x132498) = 0; }
                    else I32(0x1324c8) = 0;
                } else if (I32(0x1324c4) == 2 && I32(0x1324c8) != 0) {
                    I32(0x1324c8) = 0; I32(0x1324c4) = 0;
                    (void)FUN_000b4d73();
                    if (FUN_0004f0c6(U32(0x11d6ec), 0x1112eeu, 0x4000) == 0) (void)FUN_0004f033(U32(0x11d6ec));
                }
            }
        }
        if (I32(0x1324c4) == 0) {
            FUN_0003327c(U32(0x128048), 4, 0, 1, 2);
            if ((U32(0x4ce0a) & 0x20) != 0 || row != 0) FUN_0003327c(U32(0x12804c), 0x46, 1, 1, 2);
            if ((U32(0x4ce0a) & 0x20) != 0 || row != 1) FUN_0003327c(U32(0x128050), 0x5a, 1, 1, 2);
            if ((U32(0x4ce0a) & 0x20) != 0 || row != 2) FUN_0003327c(U32(0x128054), 0x6e, 1, 1, 2);
            if (fabsf(shown) != 0.0f) {
                FUN_0003327c(message == 0 ? U32(0x128058) : U32(0x12806c), 0xb4, 3, 3, 2);
                shown = (float)((ld)shown - F32(0x133350));
                if (0.0f > shown) shown = 0.0f;
            }
        } else if (I32(0x1324c4) == 1 && I32(0x1324c8) == 0) {
            if ((uint32_t)FUN_000371fd(0, 0) == 0) {
                if (I32(0x131f2c) > 4) { I32(0x1324c4) = 0; FUN_00037167(); F32(0x1324a0) = 0.0f; U32(0x132498) = 0; }
            } else { FUN_00037167(); F32(0x1324a0) = 0.0f; U32(0x132498) = 0; I32(0x1324c4) = 0; shown = 5.0f; }
        } else if (I32(0x1324c4) == 2 && I32(0x1324c8) == 0) FUN_0003770c();
        else if (I32(0x1324c4) == 1 || I32(0x1324c4) == 2) {
            FUN_0003327c(U32(0x128048), 4, 0, 1, 2);
            if ((U32(0x4ce0a) & 0x20) != 0 || kind != 0) FUN_0003327c(U32(0x127ef4), 0x46, 1, 1, 2);
            if ((U32(0x4ce0a) & 0x20) != 0 || kind == 0) FUN_0003327c(U32(0x127ef8), 0x5a, 1, 1, 2);
            if (I32(0x131f28) != 0) { FUN_00051698(0, 0, 0x140, 200); if (U32(0x4ce0a) & 0x40) FUN_00037686(); }
        }
        if ((U32(0x132498) & 0x7fffffff) != 0) F32(0x13249c) = (float)((ld)F32(0x133350) * 2.0f + F32(0x13249c));
        if (I32(0x13249c) >= 0x3f800000) { F32(0x13249c) = 1.0f; U32(0x1324a0) = 0x3f800000u; }
        if (fabsf(F32(0x13249c)) != 0.0f) FUN_000512f1(F32(0x13249c), 0, 0, 0);
        FUN_0007f5a2(0, 0);
    } while ((U32(0x1324a0) & 0x7fffffff) == 0);
    FUN_000358b9();
    FUN_00044f72(0x113374, 0);
    s2_sp = sp0;
}

/*
 * TWO PLAYERS ON ONE MACHINE: TEAMS AND NAMES (0x3988d) -- the title page twice, the screen in two
 * halves (the second player's room 100000 away, drawn 0x7f80 bytes lower down).  Each player with
 * a controller (0x38b2c) chooses a team (left / right), ENTER to the name (the letter wheel, the
 * second at 200 up), ENTER again when ready -- the second one ready goes on to the cars (0x3a870);
 * a ready player's half shows "waiting for player n"; ESC on a team, or the keyboard's ESC, back to
 * the network page.  The fade to white, then from black.  (The game's page addresses the players'
 * model slots 8 bytes apart; the converted text had 2: its pages were wrong, not the game's.)
 */
#define HOOK_3988d
static void FUN_0003988d(void)
{
    typedef long double ld;
    const uint32_t sp0 = s2_sp, fp = s2_push(0xf0) + 0xf0, line = fp - 0x8c;
    int stage[2] = { 0, 0 }, ready[2] = { 0, 0 }, prev[2], cursor[2] = { 0, 0 }, letter[2];
    float blink[2] = { 1.0f, 1.0f };
    U32(0x1324dc) = 3;
    FUN_0004bda7(0x11da14, 0x11d6f4);
    I32(0x132494) = 0;
    FUN_0004eaed(0x130bc8u, 0x11121cu, 0x4cu);
    FUN_00044587(0x113240);
    prev[0] = (int)U8(0x1289f4) - 0x41; prev[1] = (int)U8(0x1289f8) - 0x41;
    letter[0] = prev[0]; letter[1] = prev[1];
    for (int i = 0; i < 0x40; i++) FUN_000361dc((int)(0x1314a4u + 0x28u * (uint32_t)i), i);
    for (uint32_t p = 0; p < 2; p++) {
        const int base = p == 0 ? 0 : 100000;
        for (uint32_t k = 0; k < 4; k++) {
            const uint32_t slot = 0x130f68u + 8u * (p * 4 + k), e = 0x130e90u + 0xcu * (p * 4 + k);
            const uint32_t m = U32(0x11da14u + 0x1cu * (U32(0x130f68u + 0x20u * p) + k) + 0xc);
            FUN_0004be04(0x11da14, 0x11d6f4, (int)slot);
            I32(e) = k == 0 ? base : base + ((int)k - 1) * 3000 - 3000;
            I32(e + 4) = k == 2 ? base + 2000 : base;
            I32(e + 8) = k == 0 ? base + 3500 : base + 1000;
            if (k == 0) I32(0x130e94u + 0x30u * p) = base + 300;
            if (k == 2) I32(0x130e90u + 0xcu * (p * 4 + 2)) = base;
            I32(m + 0x10) = base; I32(m + 0xc) = base; I32(m + 8) = base;
            I32(m + 0x1c) = 0; I32(m + 0x14) = 0; I32(m + 0x18) = 0x200;
            I32(m + 0x3c) = 0; I32(m + 0x20) = 0;
            FUN_0004bfbe(0x11da14, 0x11d6f4, (int)slot);
        }
        for (uint32_t k = 0; k < 0x1a; k++) {
            const uint32_t r = 0x1314a4u + 0x28u * (p * 0x1a + k);
            U32(r + 8) = k; U32(r + 4) = k + 0x41; I32(r + 0x24) = 0; I32(r + 0xc) = 1; U32(r + 0x1c) = 0x3f800000u; U32(r + 0x20) = 0x3f800000u;
        }
    }
    for (uint32_t k = U32(0x1289ec); k != 0; k--) FUN_000336eb(1, 0x130e90, 0xc, 4);
    for (int k = I32(0x1289f0); k != 0; k--) FUN_000336eb(1, 0x130ec0, 0xc, 4);
    do {
        FUN_00038d48();
        FUN_00031888(0x1277c8);
        FUN_00051b02(0, 0, U32(0x1323c8) + 8, U32(U32(0x1323c8)));
        for (uint32_t p = 0; p < 2; p++) {
            const uint32_t held = 0x131f20u + 4 * p, team = 0x1289ecu + 4 * p;
            const int r = (int)FUN_00038b2c((int)p, 1, 0);
            const int press = r == 1;
            if (r != 1 && r != -1) U32(held) = 0;
            if (U8(0x100003) != 0) { F32(0x13249c) = 1.0f; U32(0x1324a0) = 0x3f800000u; U32(0x132498) = U32(0x1324a0); I32(0x132494) = 0x3bf06; }
            if ((U32(0x132498) & 0x7fffffff) == 0) {
                if (press) {
                    const uint32_t key = U32(held);
                    if (key == 1) { if (stage[p] != 0 && cursor[p] != 0) cursor[p]--; }
                    else if (key == 2) { if (stage[p] != 0 && cursor[p] < 2) cursor[p]++; }
                    else if (key == 0x10) {
                        if (stage[p] == 0) { stage[p]++; FUN_000aea54(U32(0x124274), 6, (int)p, 0x2b11, 0, 1); }
                        else if (ready[p == 0] == 0) ready[p] = 1;
                        else { U32(0x132498) = 0x3f800000u; I32(0x132494) = 0x3a870; FUN_000aea54(U32(0x124274), 1, (int)p, 0, 0, 1); }
                    } else if (key == 0x20 && ready[p] == 0) {
                        if (stage[p] == 0) { F32(0x13249c) = 1.0f; U32(0x1324a0) = 0x3f800000u; U32(0x132498) = U32(0x1324a0); I32(0x132494) = 0x3bf06; }
                        else {
                            letter[p] = (int)U8(0x1289f4u + 4 * p + (uint32_t)cursor[p]) - 0x41;
                            prev[p] = letter[p];
                            for (uint32_t k = 0; k < 0x1a; k++) {
                                FUN_0004c01c(0x11da14, 0x11d6f4, (int)(0x131090u + 8u * (p * 0x1a + k)));
                                const uint32_t q = 0x1314a4u + 0x28u * (p * 0x1a + k);
                                U32(q + 8) = k; U32(q + 4) = k + 0x41; I32(q + 0x24) = 0; I32(q + 0xc) = 1; U32(q + 0x1c) = 0x3f800000u; U32(q + 0x20) = 0x3f800000u;
                            }
                            stage[p]--;
                        }
                    }
                }
                const uint32_t key = U32(held);
                if (key == 4 || key == 8) {
                    const int step = key == 4 ? -1 : 1;
                    if (stage[p] == 0 && press) {
                        U32(team) = (U32(team) + (uint32_t)step) & 3;
                        FUN_000336eb(key == 4 ? 0 : 1, (int)(0x130e90u + 0x30u * p), 0xc, 4);
                        FUN_000aea54(U32(0x124274), 4, (int)p, 0, 0, 0);
                    } else if (blink[p] > (double)0.95 && stage[p] != 0) {
                        FUN_000aea54(U32(0x124274), 9, (int)p, 0, 0, 0);
                        prev[p] = letter[p];
                        letter[p] += step;
                        if (letter[p] < 0) letter[p] = 0x19;
                        else if (letter[p] > 0x19) letter[p] = 0;
                        blink[p] = 0.0f;
                    }
                }
            }
            const int base = p == 0 ? 0 : 100000;
            (void)FUN_0004f6c4(0x140, 0x60, 0x31);
            I32(0x131eb0) = base; I32(0x131eb4) = base + 500; I32(0x131eb8) = (int32_t)truncl((double)base + 5520.0);
            U32(0x131eac) = 0; U32(0x131ea8) = 0; U32(0x131ea4) = 0;
            FUN_0007e8b4(0x131ea4, 0);
            for (uint32_t k = 0; k < 4; k++) {
                const uint32_t m = U32(0x11da14u + 0x1cu * (U32(0x130f68u + 0x20u * p) + k) + 0xc), e = 0x130e90u + 0xcu * (p * 4 + k);
                const int mine = k == U32(team);
                I32(m + 0x18) = s2_turn_by(m + 0x18, mine ? 1024.0f : 256.0f);
                U32(m + 0x18) &= 0x7ff;
                if (stage[p] == 0) FUN_00033635((int)(m + 8), (int)e, 0x50);
                else {
                    FUN_00011212((int)(m + 8), (float)I32(e), 0x50);
                    FUN_00011212((int)(m + 0xc), (float)(I32(e + 4) + (mine ? 500 : 0)), 0x50);
                    FUN_00011212((int)(m + 0x10), (float)(I32(e + 8) - (mine ? 500 : 25000)), 0x50);
                }
            }
            blink[p] = (float)((ld)F32(0x133350) * 8.0f + blink[p]);
            if (s2_bits(blink[p]) > 0x3f800000) blink[p] = 1.0f;
            if (stage[p] != 0) {
                FUN_0003661d(letter[p] + 0x41, (int)p * 0x1a, (int)p * 0x1a + 0x1a);
                for (uint32_t k = p * 0x1a; k < p * 0x1a + 0x1a; k++) FUN_00036245((int)(0x1314a4u + 0x28u * k), letter[p], prev[p], k * 0x28u, blink[p], base + 1);
            }
            if (ready[p] == 0) {
                if (p != 0) I32(0x134794) += 0x7f80;
                FUN_000753a0(0x11d6f4u, 0);
                if (p != 0) I32(0x134794) -= 0x7f80;
                if (stage[p] == 0) {
                    FUN_0003327c(U32(0x127ed8), p * 0x66, 1, 1, 2);
                    FUN_0003327c(U32(0x127edcu + 4u * U32(team)), p * 0x66 + 0x56, 2, 1, 2);
                } else {
                    FUN_0003327c(U32(0x128044), p * 0x66, 1, 1, 2);
                    U8(0x1289f4u + 4 * p + (uint32_t)cursor[p]) = (uint8_t)(letter[p] + 'A');
                    FUN_0004eaed(line, 0x111252u, p == 0 ? 0x1289f4u : 0x1289f8u);
                    FUN_0003327c(line, p * 0x66 + 0x55, 2, 1, 2);
                    FUN_00050f45(0x12420c, cursor[p] * 0xc + 0x91, (int)p * 0x66 + 0x5b, 0x111256u, 2, 1, 2);
                }
            } else {
                if (p != 0) I32(0x134794) += 0x7f80;
                FUN_00051aa4(U32(0x134794), 0x7a80, 0);
                if (p != 0) I32(0x134794) -= 0x7f80;
                FUN_0004eaed(line, 0x11124cu, U32(0x12805c), (uint32_t)(p == 0) + 0x31u);
                FUN_0003327c(line, p * 0x66 + 0x32, 1, 1, 2);
            }
        }
        if ((U32(0x132498) & 0x7fffffff) != 0) F32(0x13249c) = (float)((ld)F32(0x133350) * 4.0f + F32(0x13249c));
        if (I32(0x13249c) >= 0x3f800000) { F32(0x13249c) = 1.0f; U32(0x1324a0) = 0x3f800000u; }
        if (fabsf(F32(0x13249c)) != 0.0f) FUN_000512f1(F32(0x13249c), 0xff, 0xff, 0xff);
        FUN_0007f5a2(0, 0);
    } while ((U32(0x1324a0) & 0x7fffffff) == 0);
    FUN_00051aa4(0x1338ccu, 0x300, 0x3f);
    FUN_00051664(0x1338ccu, 0);
    for (F32(0x13249c) = 0.0f; I32(0x13249c) < 0x3f800000; F32(0x13249c) = (float)((ld)F32(0x133350) * 3.0f + F32(0x13249c))) {
        FUN_00038d48();
        FUN_000512f1(F32(0x13249c), 0, 0, 0);
        FUN_0007f5a2(0, 0);
    }
    FUN_000358b9();
    FUN_00044f72(0x113240, 0);
    s2_sp = sp0;
}

/*
 * TWO PLAYERS ON ONE MACHINE: CARS (0x3a870): backdrop "bkg_5L.bkk"; each player's team's cars
 * (four, five with the team's prize car 0x128aa0..) in a ring 3000 round (the first 2800), turned
 * so the chosen one (0x128a04..) is in front, the chosen one spinning and lifted, the others aside.
 * Left / right choose (when the ring has stopped turning: within 150 of its place), ENTER to the
 * gearbox (0x1289fc.., left / right), ENTER again ready; both ready go on to the tracks (0x4189f).
 * ESC back to the teams.  Each half's camera behind the chosen car; a ready player's half waits.
 * (Its model slots 8 bytes apart, as 0x3988d.)
 */
#define HOOK_3a870
static void FUN_0003a870(void)
{
    typedef long double ld;
    const uint32_t sp0 = s2_sp, fp = s2_push(0xd0) + 0xd0, line = fp - 0x84;
    int moving[2] = { 0, 0 }, gear[2] = { 0, 0 }, n[2], ready[2] = { 0, 0 };
    FUN_0004bda7(0x11da14, 0x11d6f4);
    I32(0x132494) = 0;
    FUN_0004eaed(0x130bc8u, 0x111260u, 0x4cu);
    FUN_00044587(0x1133d4);
    FUN_000ae6e3(0x1338ccu, U32(U32(0x1323c8) + 4), 0x300);
    FUN_00051664(U32(U32(0x1323c8) + 4), 0);
    (void)FUN_0005105e(0x12420cu, U32(U32(0x1323c8) + 4));
    n[0] = I32(0x128aa0u + 0x188u * U32(0x1289ec)) + 4;
    n[1] = I32(0x128aa0u + 0x188u * U32(0x1289f0)) + 4;
    for (uint32_t p = 0; p < 2; p++) {
        const int base = p == 0 ? 0 : 100000;
        for (int k = 0; k < n[p]; k++) {
            const uint32_t slot = 0x130fe0u + 8u * (p * 5 + (uint32_t)k), e = 0x130ef0u + 0xcu * (p * 5 + (uint32_t)k);
            const uint32_t m = U32(0x11da14u + 0x1cu * (U32(0x130fe0u + 0x28u * p) + (uint32_t)k) + 0xc);
            FUN_0004be04(0x11da14, 0x11d6f4, (int)slot);
            I32(e) = 0; I32(e + 4) = 0; I32(e + 8) = k == 0 ? 0xaf0 : 3000;
            FUN_00010388(0x1324b4, (int)e, 0, 0, (float)(-((ld)(float)6.28318548f / (ld)n[p]) * (ld)k));
            I32(m + 8) = I32(0x1324b4) + base; I32(e) = I32(m + 8);
            I32(m + 0xc) = I32(0x1324b8) + base; I32(e + 4) = I32(m + 0xc);
            I32(m + 0x10) = I32(0x1324bc) + base; I32(e + 8) = I32(m + 0x10);
            I32(m + 0x1c) = 0; I32(m + 0x14) = 0; I32(m + 0x18) = 0;
            I32(m + 0x3c) = 0; I32(m + 0x20) = 0;
            FUN_0004bfbe(0x11da14, 0x11d6f4, (int)slot);
        }
        for (uint32_t k = U32(0x128a04u + 4 * p); k != 0; k--) FUN_000336eb(1, (int)(0x130ef0u + 0x3cu * p), 0xc, n[p]);
        for (int k = 0; k < n[p]; k++) {
            const uint32_t e = 0x130ef0u + 0xcu * (p * 5 + (uint32_t)k), m = U32(0x11da14u + 0x1cu * (U32(0x130fe0u + 0x28u * p) + (uint32_t)k) + 0xc);
            U32(m + 8) = U32(e); U32(m + 0xc) = U32(e + 4); U32(m + 0x10) = U32(e + 8);
        }
    }
    do {
        FUN_00038d48();
        FUN_00051b02(0, 0, U32(0x1323c8) + 8, U32(U32(0x1323c8)));
        FUN_00031888(0x1277c8);
        if (U8(0x100003) != 0) { U32(0x1324a0) = 0x3f800000u; U32(0x132498) = 0x3f800000u; I32(0x132494) = 0x3988d; }
        for (uint32_t p = 0; p < 2; p++) {
            const uint32_t held = 0x131f20u + 4 * p, chosen = 0x128a04u + 4 * p, box = 0x1289fcu + 4 * p;
            int r = (int)FUN_00038b2c((int)p, 1, 0);
            if (r == -1) r = 0;
            else if (r == 0) U32(held) = 0;
            if (r != 0 && (U32(0x132498) & 0x7fffffff) == 0) {
                const uint32_t key = U32(held);
                if (key == 4) {
                    if (gear[p] == 0) {
                        if (moving[p] == 0) {
                            I32(chosen) -= 1;
                            if (I32(chosen) < 0) I32(chosen) = n[p] - 1;
                            FUN_000336eb(0, (int)(0x130ef0u + 0x3cu * p), 0xc, n[p]);
                            FUN_000aea54(U32(0x124274), 4, (int)p, 0, 0, 0);
                        }
                    } else if (U32(box) != 0) { U32(box) -= 1; FUN_000aea54(U32(0x124274), 0, (int)p, 0, 0, 0); }
                } else if (key == 8) {
                    if (gear[p] == 0) {
                        if (moving[p] == 0) {
                            I32(chosen) += 1;
                            if (n[p] <= I32(chosen)) I32(chosen) = 0;
                            FUN_000336eb(1, (int)(0x130ef0u + 0x3cu * p), 0xc, n[p]);
                            FUN_000aea54(U32(0x124274), 4, (int)p, 0, 0, 0);
                        }
                    } else if (U32(box) == 0) { U32(box) += 1; FUN_000aea54(U32(0x124274), 0, (int)p, 0, 0, 0); }
                } else if (key == 0x10) {
                    if (gear[p] == 0) gear[p] = 1;
                    else if (ready[p == 0] == 0) ready[p] = 1;
                    else { U32(0x132498) = 0x3f800000u; I32(0x132494) = 0x4189f; FUN_000aea54(U32(0x124274), 3, (int)p, 0, 0, 0); }
                } else if (key == 0x20 && ready[p] == 0) {
                    if (gear[p] == 0) { U32(0x1324a0) = 0x3f800000u; U32(0x132498) = 0x3f800000u; I32(0x132494) = 0x3988d; }
                    else gear[p] = 0;
                }
            }
            const int base = p == 0 ? 0 : 100000;
            for (int k = 0; k < n[p]; k++) {
                const uint32_t e = 0x130ef0u + 0xcu * (p * 5 + (uint32_t)k), m = U32(0x11da14u + 0x1cu * (U32(0x130fe0u + 0x28u * p) + (uint32_t)k) + 0xc);
                if (k == I32(chosen)) {
                    I32(m + 0x18) = s2_turn_by(m + 0x18, 512.0f);
                    U32(m + 0x18) &= 0x7ff;
                    FUN_00011212((int)(m + 8), (float)I32(e), 0x46);
                    FUN_00011212((int)(m + 0xc), (float)(I32(e + 4) + gear[p] * 200), gear[p] * 0x14 + 0x46);
                    FUN_00011212((int)(m + 0x10), (float)(I32(e + 8) + gear[p] * 200), gear[p] * 0x14 + 0x46);
                } else {
                    I32(m + 0x18) = s2_turn_by(m + 0x18, 128.0f);
                    U32(m + 0x18) &= 0x7ff;
                    const int aside = gear[p] == 0;
                    FUN_00011212((int)(m + 8), (float)(I32(e) * aside + (aside ? 0 : base)), 0x46);
                    FUN_00011212((int)(m + 0xc), (float)(I32(e + 4) - (aside ? 0 : 0x4b0)), 0x46);
                    FUN_00011212((int)(m + 0x10), (float)(I32(e + 8) - (aside ? 0 : 4000)), 0x46);
                }
                moving[p] = !(fabs((double)(I32(m + 8) - I32(e))) <= 150.0);
            }
            (void)FUN_0004f6c4(0x140, 0x60, 0x31);
            const uint32_t e = 0x130ef0u + 0xcu * (p * 5 + U32(chosen));
            U32(0x131eb0) = U32(e); I32(0x131eb4) = I32(e + 4) + 600; I32(0x131eb8) = I32(e + 8) + 0x4b0;
            U32(0x131ea4) = 0x780; U32(0x131ea8) = 0; U32(0x131eac) = 0;
            FUN_0007e8b4(0x131ea4, 0);
            if (ready[p] == 0) {
                if (p != 0) I32(0x134794) += 0x7f80;
                FUN_000753a0(0x11d6f4u, 0);
                if (p != 0) I32(0x134794) -= 0x7f80;
                if (gear[p] == 0) {
                    FUN_0003327c(U32(0x127f08), p * 0x66, 1, 1, 2);
                    if (U32(chosen) == 4) FUN_0003327c(U32(0x12801cu + 4u * U32(0x1289ecu + 4 * p)), p * 0x66 + 0x52, 2, 1, 2);
                    else FUN_0003327c(U32(0x12800cu + 4u * U32(chosen)), p * 0x66 + 0x52, 2, 1, 2);
                } else {
                    FUN_0003327c(U32(0x127f18), p * 0x66, 1, 1, 2);
                    if ((U32(0x4ce0a) & 0x3f) < 0x20 || U32(box) != 0) FUN_000332dd(U32(0x127f1c), p * 0x66 + 0x49, 0xa0, 0, 1, 1);
                    if ((U32(0x4ce0a) & 0x3f) < 0x20 || U32(box) == 0) FUN_000332dd(U32(0x127f20), p * 0x66 + 0x49, 0xa0, 0xa0, 1, 1);
                }
            } else {
                if (p != 0) I32(0x134794) += 0x7f80;
                FUN_00051aa4(U32(0x134794), 0x7a80, 0);
                if (p != 0) I32(0x134794) -= 0x7f80;
                FUN_0004eaed(line, 0x111280u, U32(0x12805c), (uint32_t)(p == 0) + 0x31u);
                FUN_0003327c(line, p * 0x66 + 0x32, 1, 1, 2);
            }
        }
        if ((U32(0x132498) & 0x7fffffff) != 0) F32(0x13249c) = (float)((ld)F32(0x133350) * 2.0f + F32(0x13249c));
        if (I32(0x13249c) >= 0x3f800000) { F32(0x13249c) = 1.0f; U32(0x1324a0) = 0x3f800000u; }
        if (fabsf(F32(0x13249c)) != 0.0f) FUN_000512f1(F32(0x13249c), 0, 0, 0);
        FUN_0007f5a2(0, 0);
    } while ((U32(0x1324a0) & 0x7fffffff) == 0);
    FUN_000358b9();
    FUN_00044f72(0x1133d4, 0);
    s2_sp = sp0;
}

/*
 * THE ARCADE'S CARS (0x40a67): backdrop "bkg_2L.bkk"; the team's cars (four, five with its prize
 * car) in a ring, as on the two players' page; the chosen car (0x128a9c.. of the team) is set up
 * on car 0's record every frame (0x36cc5, its data 0x11c600.., 0x14e20) to show its top speed (km/h
 * x 1.188, or mph / 1.6 by 0x124654) and its drive (front / back / four).  Left / right choose
 * (once the ring has stopped), ENTER to the gearbox (0x1289fc, left / right), ENTER again starts
 * the race (0x1324e0, a sound).  ESC back to the tracks (or out of the network game).  In a network
 * game, at the end, waiting for every player to be ready.
 */
#define HOOK_40a67
static void FUN_00040a67(void)
{
    typedef long double ld;
    const uint32_t sp0 = s2_sp, fp = s2_push(0xd0) + 0xd0, setup = fp - 0x58, line = fp - 0x98;
    int moving = 0;
    const uint32_t chosen = 0x128a9cu + 0x188u * U32(0x1289ec);
    FUN_0004bda7(0x11da14, 0x11d6f4);
    I32(0x132494) = 0;
    FUN_0004eaed(0x130bc8u, 0x1114c6u, 0x4cu);
    FUN_00044587(0x1133a4);
    FUN_00011906();
    U32(0x1324dc) = 1;
    const int n = I32(0x128aa0u + 0x188u * U32(0x1289ec)) + 4;
    for (int k = 0; k < n; k++) {
        const uint32_t e = 0x130ef0u + 0xcu * (uint32_t)k, m = U32(0x11da14u + 0x1cu * (U32(0x130fe0) + (uint32_t)k) + 0xc);
        FUN_0004be04(0x11da14, 0x11d6f4, (int)(0x130fe0u + 8u * (uint32_t)k));
        I32(e) = 0; I32(e + 4) = 0; I32(e + 8) = k == 0 ? 0xaf0 : 3000;
        FUN_00010388(0x1324b4, (int)e, 0, 0, (float)(-((ld)(float)6.28318548f / (ld)n) * (ld)k));
        U32(m + 8) = U32(0x1324b4); U32(m + 0xc) = U32(0x1324b8); U32(m + 0x10) = U32(0x1324bc);
        U32(e) = U32(m + 8); U32(e + 4) = U32(m + 0xc); U32(e + 8) = U32(m + 0x10);
        I32(m + 0x1c) = 0; I32(m + 0x14) = 0; I32(m + 0x18) = 0;
        I32(m + 0x3c) = 0; I32(m + 0x20) = 0;
        FUN_0004bfbe(0x11da14, 0x11d6f4, (int)(0x130fe0u + 8u * (uint32_t)k));
    }
    for (int k = I32(chosen); k != 0; k--) FUN_000336eb(1, 0x130ef0, 0xc, n);
    for (int k = 0; k < n; k++) {
        const uint32_t e = 0x130ef0u + 0xcu * (uint32_t)k, m = U32(0x11da14u + 0x1cu * (U32(0x130fe0) + (uint32_t)k) + 0xc);
        U32(m + 8) = U32(e); U32(m + 0xc) = U32(e + 4); U32(m + 0x10) = U32(e + 8);
    }
    U32(0x1324ec) = 0;
    if (I32(0x1324e4) == 1) {
        if (FUN_00037b41(0x2468, 0x1234, 1, 0) != 0) { FUN_00037167(); FUN_00044f72(0x1133a4, 0); s2_sp = sp0; return; }
    } else if (I32(0x1324e4) == 2 && FUN_00036d22(0x2468u, 0, 1)) { FUN_000370ed(); FUN_00044f72(0x1133a4, 0); s2_sp = sp0; return; }
    U32(0x4ce0a) = 0;
    U32(0x133354) = 0x3ca3d70au; U32(0x133350) = 0x3ca3d70au;
    U32(0x4ce06) = 2;
    FUN_000ae6e3(0x1338ccu, U32(U32(0x1323c8) + 4), 0x300);
    FUN_00051664(U32(U32(0x1323c8) + 4), 0);
    (void)FUN_0005105e(0x12420cu, U32(U32(0x1323c8) + 4));
    do {
        FUN_00038d48();
        FUN_00036cc5((int)setup);
        FUN_000ae6e3(0x11935cu, 0x11c600u + 0x58u * (U32(chosen) + 5u * U32(0x1289ec)), 0x58);
        I32(0x1193b4) = 0x127708;
        FUN_00014e20(0x119300, (int)setup);
        FUN_00051b02(0, 0, U32(0x1323c8) + 8, U32(U32(0x1323c8)));
        if (I32(0x1324e4) == 1) { if (FUN_00037b41(0x2468, 0x3456, 0, 0) != 0) FUN_00037167(); }
        else if (I32(0x1324e4) == 2 && FUN_00036d22(0x2468u, 0, 0)) FUN_000370ed();
        for (int k = 0; k < n; k++) {
            const uint32_t e = 0x130ef0u + 0xcu * (uint32_t)k, m = U32(0x11da14u + 0x1cu * (U32(0x130fe0) + (uint32_t)k) + 0xc);
            if (k == I32(chosen)) {
                I32(m + 0x18) = s2_turn_by(m + 0x18, 512.0f);
                U32(m + 0x18) &= 0x7ff;
                FUN_00011212((int)(m + 8), (float)I32(e), 0x46);
                FUN_00011212((int)(m + 0xc), (float)(I32(e + 4) + I32(0x1324c4) * 200), I32(0x1324c4) * 0x14 + 0x46);
                FUN_00011212((int)(m + 0x10), (float)(I32(e + 8) + I32(0x1324c4) * 200), I32(0x1324c4) * 0x14 + 0x46);
            } else {
                I32(m + 0x18) = s2_turn_by(m + 0x18, 128.0f);
                U32(m + 0x18) &= 0x7ff;
                const int aside = I32(0x1324c4) == 0;
                FUN_00011212((int)(m + 8), (float)(I32(e) * aside), 0x46);
                FUN_00011212((int)(m + 0xc), (float)(I32(e + 4) - (aside ? 0 : 0x4b0)), 0x46);
                FUN_00011212((int)(m + 0x10), (float)(I32(e + 8) - (aside ? 0 : 4000)), 0x46);
            }
            moving = 150.0 < fabs((double)(I32(m + 8) - I32(e)));
        }
        const int key = (int)FUN_00038996(0, 0) != 0 && (U32(0x132498) & 0x7fffffff) == 0;
        I32(0x131f1c) = key;
        if (key) {
            const uint32_t k = U32(0x131f20);
            if (k == 4) {
                if (I32(0x1324c4) == 0) {
                    if (!moving) {
                        I32(chosen) -= 1;
                        if (I32(chosen) < 0) I32(chosen) = n - 1;
                        FUN_000336eb(0, 0x130ef0, 0xc, n);
                        FUN_000aea54(U32(0x124274), 4, 0, 0, 0, 0);
                    }
                } else if (I32(0x1289fc) != 0) { I32(0x1289fc) -= 1; U32(0x4ce0a) = 0; FUN_000aea54(U32(0x124274), 0, 0, 0, 0, 0); }
            } else if (k == 8) {
                if (I32(0x1324c4) == 0) {
                    if (!moving) {
                        I32(chosen) += 1;
                        if (n <= I32(chosen)) I32(chosen) = 0;
                        FUN_000336eb(1, 0x130ef0, 0xc, n);
                        FUN_000aea54(U32(0x124274), 4, 0, 0, 0, 0);
                    }
                } else if (I32(0x1289fc) == 0) { I32(0x1289fc) = 1; U32(0x4ce0a) = 0; FUN_000aea54(U32(0x124274), 0, 0, 0, 0, 0); }
            } else if (k == 0x10) {
                if (I32(0x1324c4) == 0) I32(0x1324c4) = 1;
                else { U32(0x1324e0) = 1; U32(0x132498) = 0x3f800000u; FUN_000aea54(U32(0x124274), 3, 0, 0, 0, 0); }
            } else if (k == 0x20) {
                if (I32(0x1324c4) == 0) {
                    if (I32(0x1324e4) == 1) FUN_00037167();
                    else if (I32(0x1324e4) == 2) { FUN_00036d22(0xace0u, 0, 0); FUN_000370ed(); }
                    else { U32(0x1324a0) = 0x3f800000u; U32(0x132498) = 0x3f800000u; I32(0x132494) = 0x4189f; }
                } else I32(0x1324c4) = 0;
            }
        }
        const uint32_t e = 0x130ef0u + 0xcu * U32(chosen);
        U32(0x131eb0) = U32(e); I32(0x131eb4) = I32(e + 4) + 600; I32(0x131eb8) = I32(e + 8) + 0x4b0;
        U32(0x131ea4) = 0x780; U32(0x131ea8) = 0; U32(0x131eac) = 0;
        FUN_0007e8b4(0x131ea4, 0);
        FUN_000753a0(0x11d6f4u, 0);
        if (I32(0x1324c4) == 0) {
            FUN_00050c0a(0x12420c, 0x1c, 0xf, 0x22, 0);
            FUN_00050c0a(0x12420c, 0xe2, 1, I32(0x1289ec) + 0x23, 0);
            FUN_00050c0a(0x12420c, 0x12, 0x9b, I32(chosen) == 4 ? I32(0x1289ec) + 0x1e : I32(chosen) + 0x1a, 0);
            FUN_00050c0a(0x12420c, 0xa6, 0xaa, 0x6a, 0);
            FUN_00050c0a(0x12420c, 0xe7, 0xab, 0x6e, 0);
            FUN_00050c0a(0x12420c, 0xfb, 0xa3, F32(0x119394) > (double)0.8 ? 0x6c : F32(0x119390) > (double)0.8 ? 0x6b : 0x6d, 0);
            if (I32(0x124654) == 0) FUN_0004eaed(line, 0x11150cu, (uint32_t)(int32_t)truncl((ld)F32(0x11935c) * (double)1.188));
            else FUN_0004eaed(line, 0x1114f6u, (uint32_t)(int32_t)truncl((ld)F32(0x11935c) * (double)1.188 / (double)1.6));
            FUN_00050f45(0x12420c, 0xd5, 0xa9, line, 5, 5, 1);
        } else {
            FUN_00050c0a(0x12420c, 0x1c, 0xf, 0x27, 0);
            FUN_00050c0a(0x12420c, 0xe2, 1, I32(0x1289ec) + 0x23, 0);
            FUN_00050c0a(0x12420c, 0x12, 0x9b, (U32(0x4ce0a) & 0x20) == 0 && I32(0x1289fc) == 0 ? 0x29 : 0x28, 0);
            FUN_00050c0a(0x12420c, 0x9f, 0x9b, (U32(0x4ce0a) & 0x20) == 0 && I32(0x1289fc) != 0 ? 0x2b : 0x2a, 0);
        }
        if ((U32(0x132498) & 0x7fffffff) != 0) F32(0x13249c) = (float)((ld)F32(0x133350) * 2.0f + F32(0x13249c));
        if (I32(0x13249c) >= 0x3f800000) { F32(0x13249c) = 1.0f; U32(0x1324a0) = 0x3f800000u; }
        if (fabsf(F32(0x13249c)) != 0.0f) FUN_000512f1(F32(0x13249c), 0, 0, 0);
        FUN_0007f5a2(0, 0);
    } while ((U32(0x1324a0) & 0x7fffffff) == 0);
    FUN_000358b9();
    if (I32(0x1324e4) == 1 && I32(0x132494) == 0) {
        U32(0x1324ec) = 1;
        for (int waiting = 1; waiting != 0;) {
            if (FUN_00037b41(0x2468, 0x3456, 1, 0) != 0) { FUN_00037167(); FUN_00044f72(0x1133a4, 0); s2_sp = sp0; return; }
            waiting = 0;
            for (int i = 0; i < I32(0x131f2c); i++) if (I32(0x132510u + 0xd8u * (uint32_t)i) != 1) waiting = 1;
        }
    } else if (I32(0x1324e4) == 2 && I32(0x132494) == 0) {
        U32(0x1324ec) = 1;
        for (int waiting = 1; waiting != 0;) {
            if (FUN_00036d22(0x2468u, 0, 1)) { FUN_000370ed(); FUN_00044f72(0x1133a4, 0); s2_sp = sp0; return; }
            waiting = I32(0x132bc8u + 0x74u * (1u - U32(0x131f28))) != 1;
        }
    }
    FUN_00044f72(0x1133a4, 0);
    s2_sp = sp0;
}

/*
 * THE CAR'S SET-UP (0x3e161): the chosen car in the garage (the camera on its place 0x132264..,
 * looking at 0x1322c4..); the set-up (six values, 0x128aa4.. of the team, car and set 0x1324e8)
 * shown as eight items -- the six (the gearbox a different panel), "save", "done" -- the camera
 * flying to each (0x33635, its turn following at 0x5d).  Up / down / left / right move between
 * them; ENTER edits one (a panel slides up; left / right change it, 0..4, the gearbox 0..2, ENTER
 * keeps it, ESC drops it) or on "save" keeps the set-up and on "done" leaves (to the race's
 * choices 0x3cbb8, 0x1324d4 = 1); ESC goes to "done".  The fade at 4 a second.  (The car's slot 8
 * bytes a car, as the game has it; the converted text had 2.)
 */
#define HOOK_3e161
static void FUN_0003e161(void)
{
    typedef long double ld;
    const uint32_t sp0 = s2_sp, fp = s2_push(0x90) + 0x90, rec = fp - 0x64, turn_y = fp - 0x4c, turn_x = fp - 0x48;
    static const int field[6] = { 0, 5, 2, 1, 3, 4 };      /* (each item's word of the set-up) */
    float slide = 0.0f, left = 0.0f, right = 0.0f;
    int value = 0, row = 0;
    FUN_0004bda7(0x11da14, 0x11d6f4);
    I32(0x132494) = 0;
    const uint32_t team = U32(0x1289ec);
    FUN_00044587(0x1132e0);
    const uint32_t set = 0x128aa4u + 0x188u * team + 0x60u * U32(0x128a9cu + 0x188u * U32(0x1289ec)) + 0x18u * U32(0x1324e8);
    for (uint32_t i = 0; i < 6; i++) U32(rec + 4 * i) = U32(set + 4 * i);
    for (uint32_t k = 0; k < 3; k++) { U32(0x131eb0 + 4 * k) = U32(0x132264 + 4 * k); U32(0x1324b4 + 4 * k) = U32(0x1322c4 + 4 * k); }
    FUN_000338ed(0x1324a8, 0x1324b4, 0x131eb0);
    U32(0x131ea4) = (uint32_t)(int32_t)truncl((ld)F32(0x1324a8) * 1024.0f / (float)3.14159274f) & 0x7ff;
    U32(0x131ea8) = (uint32_t)(int32_t)truncl((ld)F32(0x1324ac) * 1024.0f / (float)3.14159274f) & 0x7ff;
    U32(0x131eac) = 0;
    {
        const uint32_t slot = 0x130fe0u + 8u * U32(0x128a9cu + 0x188u * team), m = U32(0x11da14u + 0x1cu * U32(slot) + 0xc);
        FUN_0004be04(0x11da14, 0x11d6f4, (int)slot);
        U32(m + 8) = U32(0x1324b4); U32(m + 0xc) = U32(0x1324b8); U32(m + 0x10) = U32(0x1324bc);
        I32(m + 0x1c) = 0; I32(m + 0x14) = 0; I32(m + 0x18) = 0x200;
        I32(m + 0x3c) = 0; I32(m + 0x20) = 0;
        FUN_0004bfbe(0x11da14, 0x11d6f4, (int)slot);
    }
    FUN_000ae6e3(0x1338ccu, 0x132c98u, 0x300);
    FUN_00051664(0x1338ccu, 0);
    (void)FUN_0005105e(0x12420cu, 0x1338cc);
    do {
        FUN_00038d48();
        if (I32(0x1324e4) == 1) { if (FUN_00037b41(0x2468, 0x12bb, 0, 0) != 0) FUN_00037167(); }
        else if (I32(0x1324e4) == 2 && FUN_00036d22(0x2468u, 0, 0)) FUN_000370ed();
        const int key = (int)FUN_00038996(0, 0) != 0 && (U32(0x132498) & 0x7fffffff) == 0;
        I32(0x131f1c) = key;
        if (key) {
            const uint32_t k = U32(0x131f20);
            const int still = fabsf(slide) == 0.0f || slide == 1.0f, editing = I32(0x1324c4) != 0;
            if (k == 1) { if (still && !editing) { if (row < 6) { if (row > 1) row -= 2; } else row--; } }
            else if (k == 2) { if (still && !editing) { if (row < 5) row += 2; else if (row < 7) row++; } }
            else if (k == 4) {
                if (still) {
                    if (!editing) { if (row != 0) { row--; FUN_000aea54(U32(0x124274), 0xc, 0, 0, 0, 0); } }
                    else { if (value != 0) value--; FUN_000aea54(U32(0x124274), 0, 0, 0, 0, 0); U32(0x4ce0a) = 0; left = 1.0f; }
                }
            } else if (k == 8) {
                if (still) {
                    if (!editing) { row++; if (row < 8) FUN_000aea54(U32(0x124274), 0xc, 0, 0, 0, 0); else row = 7; }
                    else {
                        if (row == 2 && value < 2) { value++; FUN_000aea54(U32(0x124274), 0, 0, 0, 0, 0); }
                        if (row != 2 && value < 4) { value++; FUN_000aea54(U32(0x124274), 0, 0, 0, 0, 0); }
                        right = 1.0f; U32(0x4ce0a) = 0;
                    }
                }
            } else if (k == 0x10) {
                if (row < 6) {
                    if (!editing) { I32(0x1324c4) = 1; FUN_000aea54(U32(0x124274), 4, 0, 13000, 0, 1); value = I32(rec + 4u * (uint32_t)field[row]); slide = 0.0f; }
                    else { I32(0x1324c4) -= 1; FUN_000aea54(U32(0x124274), 4, 0, 0, 0, 1); I32(rec + 4u * (uint32_t)field[row]) = value; }
                } else {
                    if (row == 6) {
                        const uint32_t to = 0x128aa4u + 0x18u * U32(0x1324e8) + 0x60u * U32(0x128a9cu + 0x188u * U32(0x1289ec)) + 0x188u * U32(0x1289ec);
                        for (uint32_t i = 0; i < 6; i++) U32(to + 4 * i) = U32(rec + 4 * i);
                        FUN_000aea54(U32(0x124274), 7, 0, 0, 0, 0);
                    } else FUN_000aea54(U32(0x124274), 0xd, 0, 0, 0, 0);
                    U32(0x132498) = 0x3f800000u; U32(0x1324d4) = 1; I32(0x132494) = 0x3cbb8;
                }
            } else if (k == 0x20) {
                if (!editing) row = 7;
                else { FUN_000aea54(U32(0x124274), 4, 0, 0, 0, 1); I32(0x1324c4) -= 1; }
            }
        }
        const uint32_t at = (uint32_t)(row + 1 > 6 ? 7 : row + 1);
        for (uint32_t k = 0; k < 3; k++) U32(0x1324b4 + 4 * k) = U32(0x1322c4u + 0xcu * at + 4 * k);
        FUN_00033635(0x131eb0, (int)(0x132210u + 0xcu * at), 0x5a);
        FUN_000338ed(0x1324a8, 0x1324b4, 0x131eb0);
        F32(turn_y) = (float)((ld)I32(0x131ea4) * 0.0009765625f * (float)3.14159274f);
        F32(turn_x) = (float)((ld)I32(0x131ea8) * 0.0009765625f * (float)3.14159274f);
        FUN_0001016b((int)turn_y, F32(0x1324a8), 0x5d);
        FUN_0001016b((int)turn_x, F32(0x1324ac), 0x5d);
        U32(0x131ea4) = (uint32_t)(int32_t)truncl((ld)F32(turn_y) * 1024.0f / (float)3.14159274f) & 0x7ff;
        U32(0x131ea8) = (uint32_t)(int32_t)truncl((ld)F32(turn_x) * 1024.0f / (float)3.14159274f) & 0x7ff;
        U32(0x131eac) = 0;
        FUN_0007e8b4(0x131ea4, 0);
        FUN_000753e8(0x11d6f4u, 0);
        FUN_00050c0a(0x12420c, 0xfa, 0x1e, 0x31, 0);
        FUN_00050c0a(0x12420c, 0xfa, 0x80, 0x32, 0);
        FUN_0003327c(U32(0x127f78), 0, 0, 1, 2);
        FUN_00050c0a(0x12420c, I32(0x113120u + 0xcu * (uint32_t)row), I32(0x113124u + 0xcu * (uint32_t)row), I32(0x113128u + 0xcu * (uint32_t)row), 0);
        if (I32(0x1324c4) == 0) { slide = (float)((ld)slide - (ld)F32(0x133350) * 8.0f); if (0.0f > slide) slide = 0.0f; }
        else { slide = (float)((ld)F32(0x133350) * 8.0f + slide); if (s2_bits(slide) > 0x3f800000) slide = 1.0f; }
        const int y = (int32_t)truncl(200.0f - (ld)slide * 50.0f);
        if (row == 2) {
            FUN_00050c0a(0x12420c, (int32_t)truncl((ld)slide * (double)107.0 + (double)-45.0), 0x5a, 0x3f, 0);
            FUN_00050c0a(0x12420c, 8, y, 0x43, 0);
            FUN_00050c0a(0x12420c, 0x3c, y, 0x44, 0);
            FUN_00050c0a(0x12420c, 0x70, y, 0x45, 0);
            FUN_00050f45(0x12420c, 0xaa, (int32_t)truncl(200.0f - (ld)slide * 42.0f), U32(0x128030u + 4u * (uint32_t)value), 2, 1, 2);
            if ((U32(0x4ce0a) & 0x3f) < 0x20) FUN_00050c0a(0x12420c, value * 0x34 + 8, y, 0x46, 0);
        } else {
            FUN_00050c0a(0x12420c, (int32_t)truncl((ld)slide * (double)87.0 + (double)-45.0), 0x5a, (row < 6 ? row : 5) + 0x3d, 0);
            FUN_00050c0a(0x12420c, 0x11, y, 0x35, 0);
            FUN_00050c0a(0x12420c, 5, (int32_t)truncl(210.0f - (ld)slide * 50.0f), fabsf(left) == 0.0f ? 0x48 : 0x3c, 0);
            FUN_00050c0a(0x12420c, 0x68, (int32_t)truncl(206.0f - (ld)slide * 50.0f), fabsf(right) == 0.0f ? 0x47 : 0x3b, 0);
            if ((U32(0x4ce0a) & 0x3f) < 0x20) FUN_00050c0a(0x12420c, value * 0x10 + 0x11, y, value + 0x36, 0);
        }
        left = (float)((ld)left - (ld)F32(0x133350) * 15.0f); if (0.0f > left) left = 0.0f;
        right = (float)((ld)right - (ld)F32(0x133350) * 15.0f); if (0.0f > right) right = 0.0f;
        FUN_0003327c(U32(0x127f7cu + 4u * (uint32_t)row), 0xb4, 1, 1, 2);
        if (fabsf(F32(0x13249c)) != 0.0f) FUN_000512f1(F32(0x13249c), 0, 0, 0);
        if ((U32(0x132498) & 0x7fffffff) != 0) {
            F32(0x13249c) = (float)((ld)F32(0x133350) * 4.0f + F32(0x13249c));
            if (I32(0x13249c) >= 0x3f800000) U32(0x1324a0) = 0x3f800000u;
        }
        FUN_0007f5a2(0, 0);
    } while ((U32(0x1324a0) & 0x7fffffff) == 0);
    FUN_000358b9();
    FUN_00044f72(0x1132e0, 0);
    s2_sp = sp0;
}

/*
 * THE RACE'S CHOICES (0x3cbb8: championship and time attack): the team's garage, its five cars in
 * their places (0x132158..) with their shadows (0x131030 slots, 30 behind each car from the
 * camera), the chosen one (0x128a9c..) set up on car 0's record (0x36cc5, 0x14e20) for its top
 * speed and drive.  Three steps (0x1324d4): 0 the car (left / right, the camera swinging round it),
 * 1 its set-up slot (up / down: "default" and four kept, 0x13237c; the kept one's six values
 * shown), 2 what next (championship: race / set-up page; time attack: race, ghost, or set-up).
 * ENTER moves on, starting the race (0x1324e0, the set-up 0x1324e8; time attack's kind 2 or 4 in
 * 0x1324dc) or going to the set-up page (0x3e161); ESC steps back, from the car to the track page
 * (0x3edb4 / 0x3f72a).  The fade at 4 a second; in a network game waiting for every player.
 */
#define HOOK_3cbb8
static void FUN_0003cbb8(void)
{
    typedef long double ld;
    const uint32_t sp0 = s2_sp, fp = s2_push(0xf0) + 0xf0;
    const uint32_t setup = fp - 0x88, line = fp - 0xc8, cam = fp - 0x64, out = fp - 0x58, turn_y = fp - 0x70, turn_x = fp - 0x6c;
    const uint32_t team = U32(0x1289ec), chosen = 0x128a9cu + 0x188u * team;
    int next = 0;
    float swing;
    FUN_0004bda7(0x11da14, 0x11d6f4);
    I32(0x132494) = 0;
    I32(0x1324e8) = -1;
    swing = (float)0;
    FUN_00044587(0x1132b0);
    {
        const uint32_t p = 0x132158u + 0xcu * U32(chosen);
        I32(0x131eb8) = I32(p + 8); I32(0x131eb0) = I32(p) + 900; I32(0x131eb4) = I32(p + 4) + 0x168;
    }
    for (uint32_t i = 0; i < 5; i++) {
        const uint32_t p = 0x132158u + 0xcu * i;
        const uint32_t sets[2] = { 0x130fe0u, 0x131030u };
        for (int s = 0; s < 2; s++) {
            const uint32_t slot = sets[s] + 8 * i, m = U32(0x11da14u + 0x1cu * (U32(sets[s]) + i) + 0xc);
            FUN_0004be04(0x11da14, 0x11d6f4, (int)slot);
            U32(m + 8) = U32(p); U32(m + 0xc) = U32(p + 4); U32(m + 0x10) = U32(p + 8);
            I32(m + 0x1c) = 0; I32(m + 0x18) = 0; I32(m + 0x14) = 0;
            I32(m + 0x3c) = 0; I32(m + 0x20) = 0;
            FUN_0004c01c(0x11da14, 0x11d6f4, (int)slot);
        }
    }
    for (uint32_t i = 0; i < U32(0x128aa0u + 0x188u * U32(0x1289ec)) + 4; i++) {
        FUN_0004bfbe(0x11da14, 0x11d6f4, (int)(0x130fe0u + 8 * i));
        FUN_0004bfbe(0x11da14, 0x11d6f4, (int)(0x131030u + 8 * i));
    }
    FUN_000ae6e3(0x1338ccu, 0x132c98u, 0x300);
    FUN_00051664(0x1338ccu, 0);
    (void)FUN_0005105e(0x12420cu, 0x1338cc);
    FUN_000504f2(0x1338cc, -1, -1, -1, -1, -1, -1);
    FUN_000338ed(0x1324a8, 0x132158u + 0xcu * U32(chosen), 0x131eb0);
    U32(0x131ea4) = (uint32_t)(int32_t)truncl((ld)F32(0x1324a8) * 1024.0f / (float)3.14159274f) & 0x7ff;
    U32(0x131ea8) = (uint32_t)(int32_t)truncl((ld)F32(0x1324ac) * 1024.0f / (float)3.14159274f) & 0x7ff;
    U32(0x131eac) = 0;
    U32(0x1324ec) = 0;
    if (I32(0x1324e4) == 1) {
        if (FUN_00037b41(0x2468, 0x12bb, 1, 0) != 0) { FUN_00037167(); FUN_00044f72(0x1132b0, 0); s2_sp = sp0; return; }
    } else if (I32(0x1324e4) == 2 && FUN_00036d22(0x2468u, 0, 1)) { FUN_000370ed(); FUN_00044f72(0x1132b0, 0); s2_sp = sp0; return; }
    U32(0x4ce0a) = 0;
    U32(0x133354) = 0x3ca3d70au; U32(0x133350) = 0x3ca3d70au;
    U32(0x4ce06) = 2;
    do {
        FUN_00038d48();
        for (uint32_t i = 0; i < 5; i++) {
            const uint32_t m = U32(0x11da14u + 0x1cu * (U32(0x130fe0) + i) + 0xc), sh = U32(0x11da14u + 0x1cu * (U32(0x131030) + i) + 0xc);
            const int dx = I32(0x131eb0) - I32(m + 8), dz = I32(0x131eb8) - I32(m + 0x10);
            const float d = (float)FUN_00011253((double)(dx * dx + dz * dz));
            const float ux = (float)((ld)dx / d), uz = (float)((ld)dz / d);
            I32(sh + 8) = (int32_t)truncl((ld)I32(m + 8) - (ld)ux * 30.0f);
            I32(sh + 0xc) = I32(m + 0xc) - 0x1e;
            I32(sh + 0x10) = (int32_t)truncl((ld)I32(m + 0x10) - (ld)uz * 30.0f);
        }
        FUN_00036cc5((int)setup);
        FUN_000ae6e3(0x11935cu, 0x11c600u + 0x58u * (U32(chosen) + 5u * U32(0x1289ec)), 0x58);
        I32(0x1193b4) = 0x127708;
        FUN_00014e20(0x119300, (int)setup);
        if (I32(0x1324e4) == 1) { if (FUN_00037b41(0x2468, 0x12bb, 0, 0) != 0) FUN_00037167(); }
        else if (I32(0x1324e4) == 2 && FUN_00036d22(0x2468u, 0, 0)) FUN_000370ed();
        const int key = (int)FUN_00038996(0, 0) != 0 && (U32(0x132498) & 0x7fffffff) == 0;
        I32(0x131f1c) = key;
        if (key) {
            const uint32_t k = U32(0x131f20), step = U32(0x1324d4);
            if (k == 1) { if (U32(0x13237c) != 0 && step == 1) { FUN_000aea54(U32(0x124274), 8, 0, 0, 0, 0); U32(0x13237c) -= 1; U32(0x4ce0a) = 0; } }
            else if (k == 2) { if (U32(0x13237c) < 4 && step == 1) { FUN_000aea54(U32(0x124274), 8, 0, 0, 0, 0); U32(0x13237c) += 1; U32(0x4ce0a) = 0; } }
            else if (k == 4) {
                if (step == 0) {
                    FUN_000aea54(U32(0x124274), 0xb, 0, 0, 0, 0);
                    I32(chosen) -= 1;
                    if (I32(chosen) < 0) I32(chosen) = I32(0x128aa0u + 0x188u * U32(0x1289ec)) + 3;
                    U32(0x4ce0a) = 0; swing = 0.125f;
                } else if (step == 2 && next != 0) { FUN_000aea54(U32(0x124274), 8, 0, 0, 0, 0); next--; U32(0x4ce0a) = 0; }
            } else if (k == 8) {
                if (step == 0) {
                    FUN_000aea54(U32(0x124274), 0xb, 0, 0, 0, 0);
                    I32(chosen) += 1;
                    if (I32(0x128aa0u + 0x188u * U32(0x1289ec)) + 3 < I32(chosen)) I32(chosen) = 0;
                    U32(0x4ce0a) = 0; swing = 0.125f;
                } else if (step == 2) {
                    if ((I32(0x1324dc) == 0 || (I32(0x1324dc) == 2 && I32(chosen) > 3) || U32(0x13237c) == 0) && next == 0) {
                        next = 1; FUN_000aea54(U32(0x124274), 8, 0, 0, 0, 0); U32(0x4ce0a) = 0;
                    }
                    if (I32(0x1324dc) == 2 && U32(0x13237c) != 0 && I32(chosen) < 4 && next < 2) {
                        next++; FUN_000aea54(U32(0x124274), 8, 0, 0, 0, 0); U32(0x4ce0a) = 0;
                    }
                }
            } else if (k == 0x10) {
                if (I32(chosen) < 4) {
                    FUN_000aea54(U32(0x124274), 6, 0, 0, 0, 1);
                    if (step == 0) U32(0x1324d4) = 1;
                    else if (step == 1 && (U32(0x13237c) != 0 || I32(0x1324dc) != 0)) U32(0x1324d4) = 2;
                    else {
                        U32(0x132498) = 0x3f800000u;
                        I32(0x1324e8) = (int)U32(0x13237c) - 1;
                        if (I32(0x1324dc) == 0) {
                            if (U32(0x13237c) == 0 || next == 0) U32(0x1324e0) = 1;
                            else I32(0x132494) = 0x3e161;
                        } else if (I32(0x1324dc) == 2) {
                            if (U32(0x13237c) == 0 || next < 2) { I32(0x1324dc) = next == 0 ? 2 : 4; U32(0x1324e0) = 1; }
                            else I32(0x132494) = 0x3e161;
                        }
                    }
                } else {
                    FUN_000aea54(U32(0x124274), 6, 0, 0, 0, 0);
                    if (I32(0x1324dc) == 0) { U32(0x132498) = 0x3f800000u; I32(0x1324e8) = (int)U32(0x13237c) - 1; U32(0x1324e0) = 1; }
                    else if (I32(0x1324dc) == 2) {
                        if (U32(0x1324d4) == 0) U32(0x1324d4) = 2;
                        else { U32(0x132498) = 0x3f800000u; I32(0x1324dc) = next == 0 ? 2 : 4; U32(0x1324e0) = 1; }
                    }
                }
            } else if (k == 0x20) {
                if (step == 0 || I32(chosen) > 3) {
                    if (step == 0) {
                        if (I32(0x1324e4) == 0) {
                            U32(0x132498) = 0x3f800000u;
                            if (I32(0x1324dc) == 0) I32(0x132494) = 0x3edb4;
                            else if (I32(0x1324dc) == 2) I32(0x132494) = 0x3f72a;
                        }
                    } else U32(0x1324d4) = 0;
                } else U32(0x1324d4) -= 1;
            }
        }
        I32(cam) = 900;
        I32(cam + 4) = U32(0x1324d4) == 0 ? (int32_t)truncl(sinl((ld)swing * (float)0x1.921fb6p+3f) * (double)180.0 + (double)360.0) : 0x1c2;
        I32(cam + 8) = 0;
        FUN_00010388((int)out, (int)cam, 0, 0, (float)((ld)F32(0x1324a4) * (float)6.28318548f));
        {
            const uint32_t p = 0x132158u + 0xcu * U32(chosen);
            I32(cam) = I32(p) + I32(out); I32(cam + 4) = I32(p + 4) + I32(out + 4); I32(cam + 8) = I32(p + 8) + I32(out + 8);
            FUN_00033635(0x131eb0, (int)cam, 0x5a);
            FUN_000338ed(0x1324a8, p, cam);
        }
        F32(0x1324a4) = (float)(double)((ld)F32(0x133350) * (double)0.05 + (double)F32(0x1324a4));
        if (I32(0x1324a4) > 0x3f800000) F32(0x1324a4) = (float)((ld)F32(0x1324a4) + -1.0f);
        swing = (float)(double)((ld)F32(0x133350) * (double)0.05 + (double)swing);
        if (s2_bits(swing) > 0x3f800000) swing = (float)((ld)swing + -1.0f);
        F32(turn_y) = (float)((ld)I32(0x131ea4) * 0.0009765625f * (float)3.14159274f);
        F32(turn_x) = (float)((ld)I32(0x131ea8) * 0.0009765625f * (float)3.14159274f);
        FUN_0001016b((int)turn_y, F32(0x1324a8), 0x5a);
        FUN_0001016b((int)turn_x, F32(0x1324ac), 0x5a);
        U32(0x131ea4) = (uint32_t)(int32_t)truncl((ld)F32(turn_y) * 1024.0f / (float)3.14159274f) & 0x7ff;
        U32(0x131ea8) = (uint32_t)(int32_t)truncl((ld)F32(turn_x) * 1024.0f / (float)3.14159274f) & 0x7ff;
        U32(0x131eac) = 0;
        FUN_0007e8b4(0x131ea4, 0);
        FUN_000753e8(0x11d6f4u, 0);
        FUN_0003327c(U32(0x127f08), 4, 0, 1, 2);
        FUN_00050c0a(0x12420c, 0x12, 0xa5, I32(chosen) == 4 ? I32(0x1289ec) + 0x1e : I32(chosen) + 0x1a, 0);
        FUN_00050c0a(0x12420c, 0xa6, 0xb4, 0x6a, 0);
        FUN_00050c0a(0x12420c, 0xe7, 0xb5, 0x6e, 0);
        FUN_00050c0a(0x12420c, 0xfb, 0xad, F32(0x119394) > (double)0.8 ? 0x6c : F32(0x119390) > (double)0.8 ? 0x6b : 0x6d, 0);
        if (I32(0x124654) == 0) FUN_0004eaed(line, 0x1113acu, (uint32_t)(int32_t)truncl((ld)F32(0x11935c) * (double)1.188));
        else FUN_0004eaed(line, 0x111396u, (uint32_t)(int32_t)truncl((ld)F32(0x11935c) * (double)1.188 / (double)1.6));
        FUN_00050f45(0x12420c, 0xd5, 0xb3, line, 5, 5, 1);
        if (U32(0x1324d4) != 0) {
            if (I32(chosen) < 4) {
                FUN_00051698(100, 0x54, 0x78, 0x44);
                int y = 0x58;
                for (uint32_t i = 0; i < 5; i++, y += I32(U32(0x124250) + 0x60)) {
                    const int lit = i == U32(0x13237c) && ((U32(0x4ce0a) & 0x3f) < 0x20 || U32(0x1324d4) == 2);
                    FUN_0003327c(U32(0x127f5cu + 4 * i), (uint32_t)y, lit ? 3 : 2, lit ? 3 : 1, 2);
                }
                if (U32(0x13237c) != 0) {
                    const uint32_t s = 0x128aa4u + 0x18u * (U32(0x13237c) - 1) + 0x60u * U32(chosen) + 0x188u * team;
                    FUN_00050c0a(0x12420c, 0xff, 0x35, 0x31, 0);
                    FUN_00050c0a(0x12420c, 0x111, 0x4c, I32(s) + 0x49, 0);
                    FUN_00050c0a(0x12420c, 0x12d, 0x4c, I32(s + 0x14) + 0x49, 0);
                    FUN_00050c0a(0x12420c, 0x111, 0x6c, I32(s + 8) + 0x4e, 0);
                    FUN_00050c0a(0x12420c, 0x12d, 0x6c, I32(s + 4) + 0x49, 0);
                    FUN_00050c0a(0x12420c, 0x111, 0x8c, I32(s + 0xc) + 0x49, 0);
                    FUN_00050c0a(0x12420c, 0x12d, 0x8c, I32(s + 0x10) + 0x49, 0);
                }
            }
            if (U32(0x1324d4) == 2) {
                const int blink = (U32(0x4ce0a) & 0x20) == 0;
                if (I32(0x1324dc) == 0) {
                    FUN_000332dd(U32(0x127f70), 0x9b, 0xa0, 0, blink && next == 0 ? 3 : 2, blink && next == 0 ? 3 : 1);
                    FUN_000332dd(U32(0x127f74), 0x9b, 0xa0, 0xa0, blink && next != 0 ? 3 : 2, blink && next != 0 ? 3 : 1);
                } else if (I32(0x1324dc) == 2) {
                    if (U32(0x13237c) == 0 || I32(chosen) > 3) {
                        FUN_000332dd(U32(0x12803c), 0x9b, 0xa0, 0, blink && next == 0 ? 3 : 2, blink && next == 0 ? 3 : 1);
                        FUN_000332dd(U32(0x128040), 0x9b, 0xa0, 0xa0, blink && next != 0 ? 3 : 2, blink && next != 0 ? 3 : 1);
                    } else {
                        FUN_000332dd(U32(0x12803c), 0x9b, 0x6a, 0, blink && next == 0 ? 3 : 2, blink && next == 0 ? 3 : 1);
                        FUN_000332dd(U32(0x128040), 0x9b, 0x6a, 0x6a, blink && next == 1 ? 3 : 2, blink && next == 1 ? 3 : 1);
                        FUN_000332dd(U32(0x127f74), 0x9b, 0x6a, 0xd4, blink && next == 2 ? 3 : 2, blink && next == 2 ? 3 : 1);
                    }
                }
            }
        }
        if (fabsf(F32(0x13249c)) != 0.0f) FUN_000512f1(F32(0x13249c), 0, 0, 0);
        if ((U32(0x132498) & 0x7fffffff) != 0) {
            F32(0x13249c) = (float)((ld)F32(0x133350) * 4.0f + F32(0x13249c));
            if (I32(0x13249c) >= 0x3f800000) U32(0x1324a0) = 0x3f800000u;
        }
        FUN_0007f5a2(0, 0);
    } while ((U32(0x1324a0) & 0x7fffffff) == 0);
    FUN_000358b9();
    if (I32(0x1324e4) == 1 && I32(0x132494) == 0) {
        U32(0x1324ec) = 1;
        for (int waiting = 1; waiting != 0;) {
            if (FUN_00037b41(0x2468, 0x12bb, 1, 0) != 0) { FUN_00037167(); FUN_00044f72(0x1132b0, 0); s2_sp = sp0; return; }
            waiting = 0;
            for (int i = 0; i < I32(0x131f2c); i++) if (I32(0x132510u + 0xd8u * (uint32_t)i) != 1) waiting = 1;
        }
    } else if (I32(0x1324e4) == 2 && I32(0x132494) == 0) {
        U32(0x1324ec) = 1;
        for (int waiting = 1; waiting != 0;) {
            if (FUN_00036d22(0x2468u, 0, 1)) { FUN_000370ed(); FUN_00044f72(0x1132b0, 0); s2_sp = sp0; return; }
            waiting = 0;
            for (int i = 0; i < I32(0x131f2c); i++) if (I32(0x132bc8u + 0x74u * (uint32_t)i) != 1) waiting = 1;
        }
    }
    FUN_00044f72(0x1132b0, 0);
    s2_sp = sp0;
}

/* (every car's set-up the default: 2, 2, -1, 2, 2, 2) */
static void s2r_default_setups(void)
{
    for (uint32_t i = 0; i < 4; i++) {
        const uint32_t e = 0x130c6cu + 0x2cu * i;
        I32(e) = 2; I32(e + 4) = 2; I32(e + 8) = -1; I32(e + 0xc) = 2; I32(e + 0x10) = 2; I32(e + 0x14) = 2;
    }
}
/* (the first free numbers, of those the players' cars do not have, for the computer's cars from
   `from` on: field 0 the team, 4 the car -- or car 4 for all in the last league) */
static void s2r_fill_free(int from, uint32_t field, int last_league)
{
    int k = from, v = 0;
    do {
        int taken = 0;
        for (int i = 0; i < I32(0x131f2c); i++) if (I32(0x130c58u + field + 0x2cu * (uint32_t)i) == v) taken = 1;
        if (!taken) { I32(0x130c58u + field + 0x2cu * (uint32_t)k) = last_league && I32(0x128a10) == 3 ? 4 : v; k++; }
        v++;
    } while (k < 4);
}
/*
 * A NETWORK GAME'S PLAYERS INTO THE RACE (from their records: the network's at 0x1324f0, 0xd8 each,
 * or the link's at 0x132bb0, 0x74 each -- `o` the offsets of name, car, team, ready, set-up, keys):
 * the level from the first one's, each player's name, car, team and gearbox, the set-up when
 * given, the other players' six controls into the controls' records 1.. (0x127708 + 0xc0 n); with
 * the first one's flag 1 the computer's cars fill the race to four.
 */
static void s2r_net_roster(uint32_t base, uint32_t stride, const uint32_t o[6], uint32_t level, uint32_t flags, int setups, int champ)
{
    U32(0x130d48) = U32(0x1131fcu + 4u * (U32(level) & 3));
    U32(0x128a30) = U32(level) & 3;
    if (!champ) I32(0x130d4c) = 2 - (int)(U32(flags) >> 8);
    int other = 1;
    for (int i = 0; i < I32(0x131f2c); i++) {
        const uint32_t r = base + stride * (uint32_t)i, e = 0x130c58u + 0x2cu * (uint32_t)i;
        FUN_000b3be4(e + 8, r + o[0]);
        U32(e + 4) = U32(r + o[1]); U32(e) = U32(r + o[2]); U32(e + 0xc) = U32(r + o[3]);
        if (setups) FUN_000ae6e3(e + 0x14, r + o[4], 0x18);
        if (i != I32(0x131f28)) {
            for (uint32_t k = 0; k < 6; k++) {
                U32(0x127710u + 0x1cu * k + 0xc0u * (uint32_t)other) = U32(r + o[5] + 8 * k);
                U32(0x12771cu + 0x1cu * k + 0xc0u * (uint32_t)other) = U32(r + o[5] + 4 + 8 * k);
            }
            other++;
        }
    }
    if ((U32(flags) & 1) == 0) { if (champ) I32(0x128a20) = I32(0x131f2c); I32(0x130c48) = I32(0x131f2c); }
    else {
        if (champ) I32(0x128a20) = 4;
        I32(0x130c48) = 4;
        s2r_fill_free(I32(0x131f2c), 0, 0);
        s2r_fill_free(I32(0x131f2c), 4, !champ);
    }
    FUN_000ae6e3(0x127708u, 0x127588u, 0xa8);
    I32(0x130d08) = I32(0x131f28);
    I32(0x130d0c) = I32(0x131f2c);
}

/*
 * THE RACE FROM THE MENU'S CHOICES (0x339e7): the sound's settings, the teams (the player's, the
 * others in turn) and cars (the player's chosen; the others in turn, all the prize car in the last
 * league), and by the kind of game (0x1324dc):
 *   0 championship -- the track of the league's race, the weather drawn (seeded by the races done,
 *     the leagues' "rain" counts 0x128a24: wet or by the track's kind 0x11320c), mode 2, the
 *     league's cars and level, the set-up chosen, the computers "CPU", the best lap and race;
 *   1 arcade -- mode 1, four cars, the track chosen, its best lap and race, level 2 - 0x128a34;
 *   2 time attack -- mode 4, two cars (the ghost), the ghost on or off (0x128a3c);
 *   3 two players -- mode 0x40, both players' names, teams, cars, gearboxes and controls;
 *   4 time attack's free run -- mode 8, one car.
 * Network games take every player from the network's records (0x1324e4: 1 network, 2 link, the
 * modes 0x20 / 0x10); a network game's handshake first (0x37b41 / 0x36d22) -- failing, back to the
 * network page -- and 0x37dcf after.  The gearbox (+0xc of each car's entry) from the options.
 */
#define HOOK_339e7
static void FUN_000339e7(void)
{
    static const uint32_t net[6] = { 0x18, 0xc, 0x10, 0x1c, 0x28, 0x98 }, link[6] = { 0x10, 4, 8, 0x14, 0x2c, 0x44 };
    FUN_00044587(0x11343c);
    U32(0x130d5c) = U32(0x128a5c); U32(0x130d60) = U32(0x128a60); U32(0x130d58) = U32(0x128a64);
    U32(0x130d64) = I32(0x128a44) == 0;
    U32(0x130c54) = U32(0x128a2c);
    if (I32(0x1324e4) == 1) {
        if (FUN_00037b41(0x2468, 0xefdc, 1, 0) != 0) { FUN_00037167(); I32(0x132494) = 0x3bf06; return; }
        U32(0x130d54) = 0;
    } else if (I32(0x1324e4) == 2) {
        if (FUN_00036d22(0x2468u, 1, 1)) {
            (void)FUN_000b4d73();
            if (FUN_0004f0c6(U32(0x11d6ec), 0x110fc8u, 0x4000) == 0) (void)FUN_0004f033(U32(0x11d6ec));
            I32(0x132494) = 0x3bf06;
            return;
        }
        U32(0x130d54) = 0;
    } else U32(0x130d54) = U32(0x128a40);
    U32(0x130c58) = U32(0x1289ec);
    for (uint32_t i = 1; i < 4; i++) U32(0x130c58u + 0x2cu * i) = (U32(0x130c58) + i) % 4;
    U32(0x130c5c) = U32(0x128a9cu + 0x188u * U32(0x1289ec));
    if (I32(0x1324dc) == 0 && I32(0x128a10) == 3) { for (uint32_t i = 1; i < 4; i++) U32(0x130c5cu + 0x2cu * i) = 4; }
    else for (uint32_t i = 1; i < 4; i++) U32(0x130c5cu + 0x2cu * i) = (U32(0x130c5c) + i) % 4;
    U8(0x130d34) = 0;
    I32(0x130c50) = 0;
    U32(0x130d68) = U32(0x128a68);
    const uint32_t league = 0x1130b0u + 0x1cu * U32(0x128a10);
    switch (I32(0x1324dc)) {
    case 0: {
        I32(0x130c4c) = I32(0x113224u + 4u * U32(0x128a1c));
        int sum = 0;
        if (I32(0x1324e4) == 1) for (int i = 0; i < I32(0x131f2c); i++) sum += I32(0x1324fcu + 0xd8u * (uint32_t)i);
        else if (I32(0x1324e4) == 2) for (uint32_t i = 0; i < 2; i++) sum += I32(0x132bb4u + 0x74u * i);
        FUN_000b3c2d((uint32_t)((I32(0x128a1c) + 1) * I32(0x1324cc) + sum));
        if (I32(0x128a24) != 0 && ((FUN_000b3c09() & 0x3f) < 0x20 || (I32(league) - I32(0x128a1c)) - I32(league + 0x14) <= I32(0x128a24))) {
            I32(0x128a24) -= 1;
            const int kind = I32(0x11320cu + 4u * U32(0x130c4c));
            if (kind == 0) I32(0x130c50) = 1;
            else if (kind == 1) I32(0x130c50) = 2;
            else I32(0x130c50) = (int)(FUN_000b3c09() & 1) + 1;
        }
        U32(0x130d10) = 2;
        I32(0x130c48) = I32(0x128a20);
        U32(0x130c64) = I32(0x128a28) == 0;
        I32(0x130d4c) = I32(league + 0x10);
        if (I32(0x1324e8) < 0) s2r_default_setups();
        else FUN_000ae6e3(0x130c6cu, 0x128aa4u + 0x18u * U32(0x1324e8) + 0x60u * U32(0x130c5c) + 0x188u * U32(0x1289ec), 0x18);
        FUN_000b3be4(0x130c60u, 0x1289f4u);
        FUN_000b3be4(0x130c8cu, 0x110fdeu); FUN_000b3be4(0x130cb8u, 0x110fe2u); FUN_000b3be4(0x130ce4u, 0x110fe6u);
        U32(0x130d14) = U32(0x1290bcu + 300u * U32(0x128a30) + 0x20d0u * U32(0x1324dc) + 0x4b0u * U32(0x130c4c));
        U32(0x130d18) = U32(0x12f32cu + 300u * U32(0x130c4c) + 0x834u * U32(0x1324dc));
        FUN_000ae6e3(0x127708u, 0x127588u, 0xa8);
        if (I32(0x1324e4) == 1) { U32(0x130d10) |= 0x20; s2r_net_roster(0x1324f0u, 0xd8, net, 0x132504u, 0x13253cu, 1, 1); }
        else if (I32(0x1324e4) == 2) { U32(0x130d10) |= 0x10; s2r_net_roster(0x132bb0u, 0x74, link, 0x132bbcu, 0x132bd8u, 1, 1); }
        else U32(0x130d48) = U32(0x1131fcu + 4u * U32(0x128a30));
        break;
    }
    case 1:
        U32(0x130d10) = 1;
        I32(0x130c48) = 4;
        U32(0x130c64) = I32(0x1289fc) == 0;
        I32(0x130c4c) = I32(0x113224u + 4u * U32(0x128a0c));
        U32(0x130d14) = U32(0x12b18cu + 300u * U32(0x128a30) + 0x4b0u * U32(0x113224u + 4u * U32(0x128a0c)));
        U32(0x130d18) = U32(0x12f32cu + 300u * U32(0x113224u + 4u * U32(0x128a0c)) + 0x834u * U32(0x1324dc));
        I32(0x130d4c) = 2 - I32(0x128a34);
        s2r_default_setups();
        FUN_000b3be4(0x130c60u, 0x1289f4u);
        FUN_000b3be4(0x130c8cu, 0x110fd2u); FUN_000b3be4(0x130cb8u, 0x110fd6u); FUN_000b3be4(0x130ce4u, 0x110fdau);
        FUN_000ae6e3(0x127708u, 0x127588u, 0xa8);
        if (I32(0x1324e4) == 1) { U32(0x130d10) |= 0x20; s2r_net_roster(0x1324f0u, 0xd8, net, 0x132504u, 0x13253cu, 0, 0); }
        else if (I32(0x1324e4) == 2) { U32(0x130d10) |= 0x10; s2r_net_roster(0x132bb0u, 0x74, link, 0x132bbcu, 0x132bd8u, 0, 0); }
        else U32(0x130d48) = U32(0x1131fcu + 4u * U32(0x128a30));
        break;
    case 2:
        U32(0x130d10) = 4;
        I32(0x130c48) = 2;
        U32(0x130d48) = 3;
        U32(0x130c64) = I32(0x128a28) == 0;
        I32(0x130c4c) = I32(0x113224u + 4u * U32(0x128a0c));
        U32(0x130d50) = I32(0x128a3c) == 0;
        U32(0x130d14) = U32(0x1290bcu + 300u * U32(0x128a30) + 0x4b0u * U32(0x130c4c) + 0x20d0u * U32(0x1324dc));
        U32(0x130d18) = U32(0x12f32cu + 300u * U32(0x130c4c) + 0x834u * U32(0x1324dc));
        if (I32(0x1324e8) < 0) s2r_default_setups();
        else FUN_000ae6e3(0x130c6cu, 0x128aa4u + 0x18u * U32(0x1324e8) + 0x60u * U32(0x130c5c) + 0x188u * U32(0x1289ec), 0x18);
        FUN_000b3be4(0x130c60u, 0x1289f4u);
        FUN_000ae6e3(0x127708u, 0x127588u, 0xa8);
        FUN_000ae6e3(0x1277c8u, 0x127588u, 0xa8);
        U32(0x130d14) = U32(0x1290bcu + 300u * U32(0x128a30) + 0x20d0u * U32(0x1324dc) + 0x4b0u * U32(0x130c4c));
        U32(0x130d18) = U32(0x12f32cu + 300u * U32(0x130c4c) + 0x834u * U32(0x1324dc));
        break;
    case 3:
        U32(0x130d10) = 0x40;
        I32(0x130c48) = 2;
        U32(0x130d48) = U32(0x1131fcu + 4u * U32(0x128a30));
        I32(0x130c4c) = I32(0x113224u + 4u * U32(0x128a0c));
        U32(0x130c64) = I32(0x1289fc) == 0;
        U32(0x130c90) = I32(0x128a00) == 0;
        FUN_000b3be4(0x130c60u, 0x1289f4u);
        FUN_000b3be4(0x130c8cu, 0x1289f8u);
        U32(0x130c58) = U32(0x1289ec); U32(0x130c84) = U32(0x1289f0);
        U32(0x130c5c) = U32(0x128a04); U32(0x130c88) = U32(0x128a08);
        FUN_000ae6e3(0x127708u, 0x127588u, 0xa8);
        FUN_000ae6e3(0x1277c8u, 0x127648u, 0xa8);
        s2r_default_setups();
        break;
    case 4:
        I32(0x130c48) = 1;
        U32(0x130d48) = 1;
        U32(0x130d10) = 8;
        U32(0x130c64) = I32(0x128a28) == 0;
        I32(0x130c4c) = I32(0x113224u + 4u * U32(0x128a0c));
        if (I32(0x1324e8) < 0) s2r_default_setups();
        else FUN_000ae6e3(0x130c6cu, 0x128aa4u + 0x18u * U32(0x1324e8) + 0x60u * U32(0x130c5c) + 0x188u * U32(0x1289ec), 0x18);
        FUN_000ae6e3(0x127708u, 0x127588u, 0xa8);
        break;
    default: break;
    }
    if (I32(0x1324e4) == 1) FUN_00037dcf();
}

/* (the mean of a car's four wheel corners -- at +0x514.. this frame, +0x770.. the last -- summed
   corners 0, 2, 1, 3 in turn, a quarter of it) */
static void s2r_corner_mean(uint32_t corners, float c[3])
{
    typedef long double ld;
    for (uint32_t k = 0; k < 3; k++) {
        const uint32_t p = corners + 4 * k;
        c[k] = (float)((ld)(float)((ld)(float)((ld)(float)((ld)F32(p) + F32(p + 0x18)) + F32(p + 0xc)) + F32(p + 0x24)) * 0.25f);
    }
}
/* (a corner made 1.4 times as far from the mean, into `to`) */
static void s2r_corner_out(uint32_t to, uint32_t corner, const float c[3])
{
    typedef long double ld;
    for (uint32_t k = 0; k < 3; k++)
        F32(to + 4 * k) = (float)((ld)(float)((ld)(float)((ld)F32(corner + 4 * k) - c[k]) * (float)1.39999998f) + c[k]);
}

/*
 * HOW MUCH OF THE STEP THE CARS CAN TAKE (0x245a3, with 0x2348b): for each two cars nearer than
 * twice their lengths (+0x4d0) apart, their outlines (the wheel corners 1.4 times as wide, last
 * frame's and this one's) are swept: each corner of one against each edge of the other (the edges
 * 0x112e64's pairs); the least share of the step at which one touches is kept, with the two cars,
 * the corner and the edge.  Under 2 (a touch):
 *  - both cars flagged (+0x4c bit 8, +0x30 = 1) and every flagged car put back to that share of
 *    its step: place, turns (0x14dc0), corners, wheels' places and +0x588;
 *  - the touch: the corner's point, the edge's two ends' distances from it (its share s along
 *    the edge), the corner's push less the edge's at that share (the knock), the mean of the
 *    pushes; sparks there (0x2aeb9) as hard as the knock; the followed car's knock heard (by its
 *    size: 0xf, 0x10 over 5000, 0x11 over 10000; not in the attract race); both cars' springs
 *    knocked (0x19aaa: the corner's wheel; the edge's nearer end by 2 |s - 0.5|);
 *  - each car's corners pushed: the knock (turned over for the first car) split into its part
 *    along the line from the car's middle to the point, given to every corner, and the rest,
 *    turned for each corner by its angle from that line and weighted 0.3 x how near it faces it.
 * Gives the share (2 for none).
 */
#define HOOK_245a3
static double FUN_000245a3(void)
{
    typedef long double ld;
    const uint32_t sp0 = s2_sp, fp = s2_push(0x1120) + 0x1120;
    const uint32_t oldA = fp - 0x1f0, newA = fp - 0x1c0, oldBi = fp - 0x190, newBi = fp - 0x220, oldBj = fp - 0x160, newBj = fp - 0x130;
    const uint32_t point = fp - 0xa0, mid = fp - 0x88;
    float best = 2.0f;
    int ca = 0, cb = 0, corner = 0, edge = 0;
    for (int a = 0; a < I32(0x130c48); a++) {
        int scaled = 0;
        const uint32_t A = 0x119300u + 0xcc0u * (uint32_t)a;
        float cA[3], cAo[3];
        s2r_corner_mean(A + 0x514, cA);
        for (int b = 0; b < I32(0x130c48); b++) {
            if (a == b) continue;
            const uint32_t B = 0x119300u + 0xcc0u * (uint32_t)b;
            float cB[3], cBo[3];
            s2r_corner_mean(B + 0x514, cB);
            const float dx = (float)((ld)cB[0] - cA[0]), dy = (float)((ld)cB[1] - cA[1]), dz = (float)((ld)cB[2] - cA[2]);
            const float reach = (float)((I32(A + 0x4d0) + I32(B + 0x4d0)) * 2);
            const float dist = (float)sqrt((double)((ld)dy * dy + (ld)dx * dx + (ld)dz * dz));
            if (dist >= reach) continue;
            s2r_corner_mean(B + 0x770, cBo);
            if (!scaled) {
                s2r_corner_mean(A + 0x770, cAo);
                for (uint32_t k = 0; k < 4; k++) {
                    s2r_corner_out(oldA + 0xc * k, A + 0x770 + 0xc * k, cAo);
                    s2r_corner_out(newA + 0xc * k, A + 0x514 + 0xc * k, cA);
                }
                scaled = 1;
            }
            for (uint32_t e = 0; e < 4; e++) {
                const uint32_t i = U32(0x112e64u + 8 * e), j = U32(0x112e68u + 8 * e);
                s2r_corner_out(oldBi + 0xc * e, B + 0x770 + 0xc * i, cBo);
                s2r_corner_out(newBi + 0xc * e, B + 0x514 + 0xc * i, cB);
                s2r_corner_out(oldBj + 0xc * e, B + 0x770 + 0xc * j, cBo);
                s2r_corner_out(newBj + 0xc * e, B + 0x514 + 0xc * j, cB);
            }
            for (int c = 0; c < 4; c++)
                for (int e = 0; e < 4; e++) {
                    const float t = (float)FUN_0002348b((int)(oldA + 0xcu * (uint32_t)c), (int)(newA + 0xcu * (uint32_t)c),
                                                        (int)(oldBi + 0xcu * (uint32_t)e), (int)(newBi + 0xcu * (uint32_t)e),
                                                        (int)(oldBj + 0xcu * (uint32_t)e), (int)(newBj + 0xcu * (uint32_t)e));
                    if (!(t >= best)) { ca = a; cb = b; corner = c; edge = e; best = t; }
                }
        }
    }
    if (s2_bits(best) < 0x40000000) {
        const uint32_t A = 0x119300u + 0xcc0u * (uint32_t)ca, B = 0x119300u + 0xcc0u * (uint32_t)cb;
        U8(A + 0x4c) |= 8; U32(A + 0x30) = 0x3f800000u;
        U8(B + 0x4c) |= 8; U32(B + 0x30) = 0x3f800000u;
        for (int n = 0; n < I32(0x130c48); n++) {
            const uint32_t C = 0x119300u + 0xcc0u * (uint32_t)n;
            if (!(U8(C + 0x4c) & 8)) continue;
            const ld back = 1.0L - best;
            static const uint32_t xyz[3] = { 0, 8, 4 };
            for (uint32_t k = 0; k < 3; k++)
                I32(C + 0x4d8 + xyz[k]) = (int32_t)truncl((ld)I32(C + 0x734 + xyz[k]) * back + (ld)I32(C + 0x4d8 + xyz[k]) * best);
            static const uint32_t turn[3] = { 0x4e4, 0x4ec, 0x4e8 };
            for (uint32_t k = 0; k < 3; k++)
                F32(C + turn[k]) = (float)FUN_00014dc0((float)((ld)F32(C + turn[k]) * best), (float)(-(ld)F32(C + turn[k] + 0x25c) * back));
            for (uint32_t k = 0; k < 4; k++) {
                const float keep = (float)(1.0L - best);
                const uint32_t now = C + 0x514 + 0xc * k, was = C + 0x770 + 0xc * k, w = C + 0x5a0 + 0x70 * k;
                float o[3], m[3];
                for (uint32_t q = 0; q < 3; q++) o[q] = (float)((ld)F32(was + 4 * q) * keep);
                for (uint32_t q = 0; q < 3; q++) m[q] = (float)((ld)F32(now + 4 * q) * best);
                for (uint32_t q = 0; q < 3; q++) F32(now + 4 * q) = (float)((ld)m[q] + o[q]);
                for (uint32_t q = 0; q < 3; q++)
                    I32(w + xyz[q]) = (int32_t)truncl((ld)I32(w + 0x25c + xyz[q]) * back + (ld)I32(w + xyz[q]) * best);
                F32(C + 0x588 + 0x70 * k) = (float)(back * F32(C + 0x7e4 + 0x70 * k) + (ld)F32(C + 0x588 + 0x70 * k) * best);
            }
        }
        const uint32_t i = U32(0x112e64u + 8u * (uint32_t)edge), j = U32(0x112e68u + 8u * (uint32_t)edge);
        for (uint32_t q = 0; q < 3; q++) U32(point + 4 * q) = U32(A + 0x514 + 0xc * (uint32_t)corner + 4 * q);
        float d0, d1;
        {
            const uint32_t p = B + 0x514 + 0xc * i, r = B + 0x514 + 0xc * j;
            const float x0 = (float)((ld)F32(p) - F32(point)), y0 = (float)((ld)F32(p + 4) - F32(point + 4)), z0 = (float)((ld)F32(p + 8) - F32(point + 8));
            d0 = (float)sqrt((double)((ld)y0 * y0 + (ld)x0 * x0 + (ld)z0 * z0));
            const float x1 = (float)((ld)F32(r) - F32(point)), y1 = (float)((ld)F32(r + 4) - F32(point + 4)), z1 = (float)((ld)F32(r + 8) - F32(point + 8));
            d1 = (float)sqrt((double)((ld)y1 * y1 + (ld)x1 * x1 + (ld)z1 * z1));
        }
        const float s = (float)((ld)d0 / ((ld)d0 + d1));
        float rel[3];
        {
            const uint32_t pi = B + 0x544 + 0xc * i, pj = B + 0x544 + 0xc * j, pa = A + 0x544 + 0xc * (uint32_t)corner;
            const float om = (float)(1.0L - s);
            for (uint32_t q = 0; q < 3; q++) {
                const float bj = (float)((ld)F32(pj + 4 * q) * s), bi = (float)((ld)F32(pi + 4 * q) * om);
                rel[q] = (float)((ld)(float)((ld)F32(pa + 4 * q) - bi) - bj);
            }
            const float sj = (float)((ld)d0 / ((ld)d0 + d1)), si = (float)((ld)d1 / ((ld)d0 + d1));
            for (uint32_t q = 0; q < 3; q++) {
                const float bj = (float)((ld)F32(pj + 4 * q) * sj), bi = (float)((ld)F32(pi + 4 * q) * si);
                F32(mid + 4 * q) = (float)((ld)(float)((ld)(float)((ld)F32(pa + 4 * q) + bi) + bj) * 0.5f);
            }
        }
        const float knock = (float)sqrt((double)((ld)rel[1] * rel[1] + (ld)rel[0] * rel[0] + (ld)rel[2] * rel[2]));
        FUN_0002aeb9((int)point, (int)mid, 0, 0, knock, 1);
        if ((ca == I32(0x11f0c8) || cb == I32(0x11f0c8)) && I32(0x11cdb4) == 0 && !(U8(0x130d10) & 0x80)) {
            const int k = (int32_t)truncl((ld)knock);
            FUN_000aea54(U32(0x124274), k > 0x2710 ? 0x11 : k > 0x1388 ? 0x10 : 0xf, 4, 0, 0, 0);
        }
        const float sign[2] = { -1.0f, 1.0f };
        const int side = corner == 1 || corner == 3 ? 3 : 1;
        FUN_00019aaa((int)A, side, corner, A - 0x119300u, 1.0f, knock);
        FUN_00019aaa((int)B, edge, I32(0x112e64u + 8u * (uint32_t)edge + 4u * (uint32_t)((float)0.5f <= s)), B - 0x119300u,
                     (float)(fabsl((ld)s + (double)-0.5) * (double)2.0), knock);
        float centre[2][3], facing[2], along[2][3], across[2][3];
        for (int n = 0; n < 2; n++) {
            const uint32_t C = n == 0 ? A : B;
            s2r_corner_mean(C + 0x514, centre[n]);
            const float vx = (float)((ld)F32(point) - centre[n][0]), vy = (float)((ld)F32(point + 4) - centre[n][1]), vz = (float)((ld)F32(point + 8) - centre[n][2]);
            const float len = (float)sqrt((double)((ld)vy * vy + (ld)vx * vx + (ld)vz * vz));
            float u[3] = { 0.0f, 0.0f, 0.0f };
            if (!s2r_zero(len)) { const float inv = (float)(1.0L / len); u[0] = (float)((ld)vx * inv); u[1] = (float)((ld)vy * inv); u[2] = (float)((ld)vz * inv); }
            facing[n] = s2r_ground_angle(u[0], u[2]);
            float r[3];
            for (uint32_t q = 0; q < 3; q++) r[q] = (float)((ld)rel[q] * sign[n]);
            const float dot = (float)((ld)r[1] * u[1] + (ld)r[0] * u[0] + (ld)r[2] * u[2]);
            for (uint32_t q = 0; q < 3; q++) along[n][q] = (float)((ld)u[q] * dot);
            for (uint32_t q = 0; q < 3; q++) across[n][q] = (float)((ld)(float)((ld)rel[q] * sign[n]) - along[n][q]);
        }
        for (int n = 0; n < 2; n++) {
            const uint32_t C = n == 0 ? A : B;
            for (uint32_t k = 0; k < 4; k++) {
                const uint32_t c = C + 0x514 + 0xc * k, push = C + 0x544 + 0xc * k;
                const float vx = (float)((ld)F32(c) - centre[n][0]), vz = (float)((ld)F32(c + 8) - centre[n][2]);
                const uint32_t keep = U32(push + 4);
                const float ang = s2r_ground_angle(vx, vz);
                const float w = (float)((1.0L - fabsl((ld)FUN_00014dc0(ang, facing[n]) / (float)3.14159274f)) * (double)0.3);
                const float rot = (float)((ld)s2r_ground_angle(vx, vz) - facing[n]);
                const float sn = (float)sinl((ld)rot), cs = (float)cosl((ld)rot);
                const float px = (float)((ld)across[n][0] * cs - (ld)across[n][2] * sn), pz = (float)((ld)across[n][2] * cs + (ld)across[n][0] * sn);
                const float add[3] = { (float)((ld)along[n][0] + (float)((ld)px * w)), (float)((ld)along[n][1] + (float)((ld)across[n][1] * w)),
                                       (float)((ld)along[n][2] + (float)((ld)pz * w)) };
                for (uint32_t q = 0; q < 3; q++) F32(push + 4 * q) = (float)((ld)add[q] + F32(push + 4 * q));
                U32(push + 4) = keep;
            }
        }
    }
    s2_sp = sp0;
    return (double)best;
}

/*
 * THE GAME'S NUMBERS THESE ROUTINES READ FROM ITS PROGRAM (s2r_tables, called at start-up with the
 * names): the steering's range (0x112e20); the gears' ratios (0x112e24: 2.5 2.5 1.6 1.2 1);
 * the tyres' counts and factors (0x112e3c / 0x112e44); the masks (0x112e54: 15 31 63 127); the
 * outline's four edges as corner pairs (0x112e64: 3-2, 2-0, 0-1, 1-3); the wheels' block names
 * (0x112f00); the 3D's fade distances and flags as the program starts (0x113738).
 */
static void s2r_tables(void)
{
    static const struct { uint32_t at, v; } words[] = {
        { 0x112e20u, 0x3f800000u }, { 0x112e24u, 0x40200000u }, { 0x112e28u, 0x40200000u }, { 0x112e2cu, 0x3fcccccdu },
        { 0x112e30u, 0x3f99999au }, { 0x112e34u, 0x3f800000u }, { 0x112e3cu, 3 }, { 0x112e40u, 6 },
        { 0x112e44u, 0x40000000u }, { 0x112e48u, 0x3fc00000u }, { 0x112e4cu, 0x3f800000u },
        { 0x112e54u, 0xf }, { 0x112e58u, 0x1f }, { 0x112e5cu, 0x3f }, { 0x112e60u, 0x7f },
        { 0x112e64u, 3 }, { 0x112e68u, 2 }, { 0x112e6cu, 2 }, { 0x112e78u, 1 }, { 0x112e7cu, 1 }, { 0x112e80u, 3 },
        { 0x113738u, 0xffff5038u }, { 0x11373cu, 0xfffff060u }, { 0x113744u, 1 },
    };
    static const char wheel[8][4] = { "flx", "fli", "blx", "rli", "frx", "fri", "brx", "rri" };
    for (size_t i = 0; i < sizeof words / sizeof words[0]; i++) U32(words[i].at) = words[i].v;
    for (uint32_t i = 0; i < 8; i++) memcpy(MP(0x112f00u + 4 * i), wheel[i], 4);
}

/*
 * THE MENU'S TABLES OF THE GAME (s2r_menu_tables, as the menu starts): what its routines read of the
 * program's data, besides the names (s2r_strings):
 *  - the letters of the keys, by scan code (0x112f50, to -1): the name page and the cheat words;
 *  - the cheat words (0x113020, 0x18 each: five letters, the word they set, its value, turned or
 *    set): MRTRK all the tracks, TACAR .. TDCAR each team's prize car, CHMPA every league;
 *  - the leagues (0x1130b0, seven numbers each): races, the tracks winning it opens, its prize car
 *    for the team, the next league open, the computers' level, ... , the dry races;
 *  - the set-up page's items (0x113120: x, y, sprite);
 *  - the points by place, by how many of the same team are in the race (0x113180);
 *  - the options' rows of names and counts of values, the best times' choices, the levels' numbers
 *    (0x1131c0 .. 0x113208);
 *  - each track's weather (0x11320c: -1 dry, 0 wet, 1 either) and the tracks in order (0x113224);
 *  - each page's things to load and to let go (0x113240 ..: three runs ended by -1 a page);
 *  - a room's car's wheels' block names (0x112d34);
 *  - in the code object: where the 3D's fade distances are (0x7e428 -> 0x7e3d8: -2000, -6000).
 */
static void s2r_menu_tables(void)
{
    static const int32_t keys[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 0, 0, 0, 0,
        'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 0, 0, 0, 0, 0, 'Z', 'X', 'C', 'V', 'B', 'N', 'M', -1 };
    static const struct { char word[12]; uint32_t sets, value, turned; } cheats[6] = {
        { "MRTRK", 0x128a14u, 4, 0 }, { "TACAR", 0x128aa0u, 1, 0 }, { "TBCAR", 0x128c28u, 1, 0 },
        { "TCCAR", 0x128db0u, 1, 0 }, { "TDCAR", 0x128f38u, 1, 0 }, { "CHMPA", 0x128a18u, 3, 0 } };
    static const int32_t leagues[4][7] = { { 4, 1, 0, 1, 2, 1, 3 }, { 5, 2, 0, 2, 1, 1, 2 }, { 6, 3, 0, 3, 0, 1, 1 }, { 6, 0, 1, 0, 1, 0, 1 } };
    static const int32_t setup_items[8][3] = { { 250, 30, 51 }, { 277, 30, 51 }, { 250, 62, 51 }, { 277, 62, 51 },
                                               { 250, 94, 51 }, { 277, 94, 51 }, { 250, 126, 52 }, { 250, 153, 52 } };
    static const int32_t counts[19] = { 0, 2, 3, 4, 3, 2, 2, 2, 2, 2, 2, 2, 3, 4, 7, 1, 3, 5, 10 };
    static const int32_t weather[7] = { -1, 0, 0, 0, 1, 0, 0 }, order[7] = { 0, 2, 3, 4, 5, 1, 6 };
    static const int32_t pages[] = {
        12, 1, 2, 3, 7, 4, 8, -1, 5, 6, 11, 9, 10, -1, 7, 4, 8, -1,                  /* 0x113240 the title */
        12, 1, 2, 3, 5, 8, -1, 7, -1, -1,                                            /* 0x113288 the main menu */
        12, 1, 2, 3, 5, 6, 8, -1, 10, 9, -1, -1,                                     /* 0x1132b0 the race's choices */
        12, 1, 2, 3, 5, 6, 8, -1, -1, -1,                                            /* 0x1132e0 the set-up */
        12, 1, 2, 3, 5, 9, 4, 8, -1, 10, 6, -1, 4, -1,                               /* 0x113308 a race's track, time attack */
        12, 1, 3, 5, 4, 6, 8, -1, 9, 10, -1, 4, -1,                                  /* 0x113340 arcade */
        12, 1, 2, 3, 4, 8, -1, 9, 10, -1, 4, -1,                                     /* 0x113374 the flat pages */
        12, 1, 2, 3, 4, 5, 6, 8, -1, -1, 4, -1,                                      /* 0x1133a4 the cars */
        12, 1, 2, 3, 4, 5, 11, 8, -1, 6, -1, 4, 11, 8, -1,                           /* 0x1133d4 two players' cars */
        12, 1, 3, 4, 5, 10, 8, -1, -1, 4, -1,                                        /* 0x113410 the league */
        1, 2, 3, 4, -1, -1, 12, 1, 2, 3, 4, 6, 9, 10, 5, 8, -1 };                    /* 0x11343c the loading page */
    static const char wheel[8][4] = { "flx", "fli", "blx", "rli", "frx", "fri", "brx", "rri" };   /* (0x112d34: a room's car's wheels, 0x11d6f) */
    for (uint32_t i = 0; i < 8; i++) memcpy(MP(0x112d34u + 4 * i), wheel[i], 4);
    for (uint32_t i = 0; i < sizeof keys / sizeof keys[0]; i++) I32(0x112f50u + 4 * i) = keys[i];
    for (uint32_t i = 0; i < 6; i++) {
        const uint32_t c = 0x113020u + 0x18 * i;
        memcpy(MP(c), cheats[i].word, 12);
        U32(c + 0xc) = cheats[i].sets; U32(c + 0x10) = cheats[i].value; U32(c + 0x14) = cheats[i].turned;
    }
    for (uint32_t i = 0; i < 4; i++) for (uint32_t k = 0; k < 7; k++) I32(0x1130b0u + 0x1c * i + 4 * k) = leagues[i][k];
    for (uint32_t i = 0; i < 8; i++) for (uint32_t k = 0; k < 3; k++) I32(0x113120u + 0xc * i + 4 * k) = setup_items[i][k];
    for (uint32_t i = 0; i < 16; i++) I32(0x113180u + 4 * i) = (int32_t[]){ 10, 6, 3, 0 }[i & 3];
    for (uint32_t i = 0; i < 19; i++) I32(0x1131c0u + 4 * i) = counts[i];
    for (uint32_t i = 0; i < 7; i++) { I32(0x11320cu + 4 * i) = weather[i]; I32(0x113224u + 4 * i) = order[i]; }
    for (uint32_t i = 0; i < sizeof pages / sizeof pages[0]; i++) I32(0x113240u + 4 * i) = pages[i];
    U32(0x7e428) = 0x7e3d8u; I32(0x7e3e0) = -2000; I32(0x7e3e4) = -6000;
}

/* A MENU PAGE BY ITS ADDRESS (the game keeps the next page as its routine's address, 0x132494) */
static void s2_dispatch(uint32_t at)
{
    switch (at) {
    case 0x38d8du: FUN_00038d8d(); break;
    case 0x394f5u: FUN_000394f5(); break;
    case 0x3988du: FUN_0003988d(); break;
    case 0x3a870u: FUN_0003a870(); break;
    case 0x3b674u: FUN_0003b674(); break;
    case 0x3bbc4u: FUN_0003bbc4(); break;
    case 0x3bf06u: FUN_0003bf06(); break;
    case 0x3c9acu: FUN_0003c9ac(); break;
    case 0x3cbb8u: FUN_0003cbb8(); break;
    case 0x3e161u: FUN_0003e161(); break;
    case 0x3edb4u: FUN_0003edb4(); break;
    case 0x3f72au: FUN_0003f72a(); break;
    case 0x3ff37u: FUN_0003ff37(); break;
    case 0x40a67u: FUN_00040a67(); break;
    case 0x4189fu: FUN_0004189f(); break;
    case 0x420c0u: FUN_000420c0(); break;
    case 0x4254fu: FUN_0004254f(); break;
    case 0x431b3u: FUN_000431b3(); break;
    case 0x43914u: FUN_00043914(); break;
    default: s2_dispatch_other(at); break;
    }
}

/*
 * WHICH TRACKSIDE CAMERA (0x14acf): the camera record's rows (+0xa60 of them, 0x20 bytes from
 * +0x60) -- the nearest to the car on the ground (x and z in 64ths, as whole numbers; the length a
 * double, cut to a whole number when kept), not one at no distance at all.  It is taken only when
 * it is the next or the one before (round the end), or none was chosen yet, and it is not the one
 * in use.  When it changes and the record asks for it (+0xae8, every 10 s of the attract race), the
 * cameras follow the next car (round the cars, 0x12d00), which becomes the one the camera follows
 * (0x11f0c8), and the choice is made again for it.
 */
#define HOOK_14acf
static void FUN_00014acf(int camera)
{
    const uint32_t obj = U32(U32(camera + 0xadc) + 0x990);
    const int now = I32(camera + 0x24), next = now + 1, before = now - 1, rows = I32(camera + 0xa60);
    int best = -1, pick = now;
    for (int i = 0; i < rows; i++) {
        const uint32_t row = (uint32_t)camera + 0x60u + (uint32_t)i * 0x20u;
        const long double dx = (long double)(I32(row) >> 6) - (long double)(I32(obj) >> 6);
        const long double dz = (long double)(I32(row + 8) >> 6) - (long double)(I32(obj + 8) >> 6);
        const double d = FUN_00011253((double)((double)(dx * dx) + (double)(dz * dz)));
        if (d != 0.0 && ((long double)best > d || best < 0)) { best = (int32_t)d; pick = i; }
    }
    if ((pick == next || now == -1 || (next == rows && pick == 0) || pick == before || (before < 0 && rows - 1 == pick)) && pick != now) {
        I32(camera + 0x24) = pick;
        if (I32(camera + 0xae8)) {
            FUN_00012d00(camera, 0x119300u + (uint32_t)((I32(U32(camera + 0xadc)) + 1) % I32(0x130c48)) * 0xcc0u);
            I32(0x11f0c8) = I32(U32(camera + 0xadc));
            I32(camera + 0xae8) = 0;
            FUN_00014acf(camera);
        }
    }
}

/* a camera record's car to follow (+0xadc; +0x24 -1: no trackside camera chosen yet) and its
   view (+0xae0): 0x12d00, 0x12dd8 */
#define HOOK_12d00
static void FUN_00012d00(int camera, uint32_t car) { U32(camera + 0xadc) = car; I32(camera + 0x24) = -1; }
#define HOOK_12dd8
static void FUN_00012dd8(int camera, uint32_t view) { U32(camera + 0xae0) = view; }

/* a car against the walls, then against the ground (0x16824); its road's kind's effects, then its
   smoke (0x1684e) */
#define HOOK_16824
static void FUN_00016824(int car) { FUN_00029571(car, 0); FUN_00028f96(car, 0); }
#define HOOK_1684e
static void FUN_0001684e(int car) { FUN_0001f1ef(car); FUN_0002073c(car, 0, 0, 0); }

/* a path from its start (0x12462: its curve made, 0x1248d); a point along a path's node at the
   node's own place (0x12553: 0x1257f with t = the node's +8) */
#define HOOK_12462
static uint64_t FUN_00012462(int path, uint32_t high) { FUN_0001248d(path); return (uint64_t)high << 32 | (uint32_t)path; }
#define HOOK_12553
static void FUN_00012553(int path, int node, uint32_t a, uint32_t b) { FUN_0001257f(path, node, a, b, F32(node + 8)); }
/* every node of a path at its own place (0x12512: count nodes of 0x2c bytes) */
#define HOOK_12512
static void FUN_00012512(uint32_t path, int node, int count) { for (int i = 0; i < count; i++, node += 0x2c) FUN_00012553((int)path, node, 0, 0); }

/* THE CAR'S ENGINE AND WHEELS, a frame (0x167e2): the throttle and brake (0x1f3ce), the engine and
   gearbox (0x1f756), then 0x174ca, 0x1756f, 0x19409 */
#define HOOK_167e2
static void FUN_000167e2(int car) { FUN_0001f3ce(car); FUN_0001f756(car); FUN_000174ca(car); FUN_0001756f(car); FUN_00019409(car); }

/* the square root, nought for a negative (0x11253) */
#define HOOK_11253
static double FUN_00011253(double x) { return 0.0 <= x ? (double)sqrtl((long double)x) : 0.0; }

/* what is left of a speed after a frame's drag (0x19c61): 1 - k x a x b x the step, no less than
   nought (a, b, k = the first, second and fourth argument; the third is not used) */
#define HOOK_19c61
static double FUN_00019c61(float a, float b, float unused, float k)
{
    (void)unused;
    float left = (float)(1.0L - (long double)k * a * b * F32(0x133350));
    if (0.0f > left) left = 0.0f;
    return (double)left;
}

/* the first record of a table of 0x30-byte records (a name at its start, -1 at +0x28 after the
   last) whose name holds `name` (0x1240d, through the library's strstr 0xae7cf); -1: none */
#define HOOK_1240d
static int FUN_0001240d(uint32_t name, int table)
{
    for (int i = 0; ; i++, table += 0x30) {
        if (I32(table + 0x28) == -1) return -1;
        if (FUN_000ae7cf((uint32_t)table, name) != 0) return i;
    }
}

/*
 * A POINT TURNED (whole numbers x, y, z; angle a): 0x10388 about the up axis (y kept),
 *     x' = x cos a + z sin a,   z' = z cos a - x sin a
 * 0x1030d about the x axis (x kept),
 *     y' = y cos a - z sin a,   z' = z cos a + y sin a
 * the cosine and sine kept as floats, each result cut to a whole number (0x4ea12)
 */
#define HOOK_10388
static void FUN_00010388(int out, int in, uint32_t a, uint32_t b, float angle)
{
    (void)a; (void)b;
    const float c = (float)cosl((long double)angle), s = (float)sinl((long double)angle);
    const long double x = I32(in), z = I32(in + 8);
    I32(out) = (int32_t)(z * s + x * c);
    I32(out + 4) = I32(in + 4);
    I32(out + 8) = (int32_t)(z * c - x * s);
}
#define HOOK_1030d
static void FUN_0001030d(int out, int in, uint32_t a, uint32_t b, float angle)
{
    (void)a; (void)b;
    const float c = (float)cosl((long double)angle), s = (float)sinl((long double)angle);
    const long double y = I32(in + 4), z = I32(in + 8);
    I32(out) = I32(in);
    I32(out + 4) = (int32_t)(y * c - z * s);
    I32(out + 8) = (int32_t)(z * c + y * s);
}

/* a path's reading from its start (0x1248d): at node 0, t 0, two pieces a node */
#define HOOK_1248d
static void FUN_0001248d(int path)
{
    I32(path + 0x28) = 0; I32(path + 0x1c) = 0; I32(path + 4) = 0; I32(path) = 0;
    I32(path + 0x18) = 0; I32(path + 0x14) = 0; I32(path + 0x10) = 0;
    I32(path + 0x24) = 1; I32(path + 0x20) = 1; I32(path + 0x2c) = 2;
}

/* THE CAMERA, a frame (0x13223): which car and trackside camera (0x14acf), then by the kind of the
   camera in use (its row of 0x20 bytes from +0x78, by +0x24): 0 0x13514, 1 0x135b6, 2 0x13768,
   3 0x138e8, 4 0x13a8e */
#define HOOK_13223
static void FUN_00013223(int camera)
{
    FUN_00014acf(camera);
    switch (I32(camera + 0x78 + I32(camera + 0x24) * 0x20)) {
    case 0: FUN_00013514(camera); break;
    case 1: FUN_000135b6(camera); break;
    case 2: FUN_00013768(camera); break;
    case 3: FUN_000138e8(camera, 0, 0, 0); break;
    case 4: FUN_00013a8e(camera, 0, 0, 0); break;
    default: break;
    }
}

/* a camera of the first kind (0x13514): it stands where its row says (+0x60.., the angles
   +0x6c..) -- copied to the camera's place (+0x48..) and angles (+0x54..) -- 200 rows */
#define HOOK_13514
static void FUN_00013514(int camera)
{
    const uint32_t row = (uint32_t)camera + (uint32_t)I32(camera + 0x24) * 0x20u;
    for (uint32_t k = 0; k < 6; k++) U32(camera + 0x48 + 4 * k) = U32(row + 0x60 + 4 * k);
    I32(camera + 0x1c) = 200;
}

/* how fast a path's curve goes at t of a piece (0x108f5): its slope from the three handles h0,
   h1, h2 (x, z at +0/+4, +0xc/+0x10, +0x18/+0x1c), ((h0 - 2 h1) + h2) t + h1 - h0, its length;
   1 where it is nought */
#define HOOK_108f5
static double FUN_000108f5(int h, int r1, int r2, int r3, float t)
{
    (void)r1; (void)r2; (void)r3;
    const float dx = (float)((((long double)F32(h) - (long double)F32(h + 0xc) * 2.0f) + F32(h + 0x18)) * t + F32(h + 0xc) - F32(h));
    const float dz = (float)((((long double)F32(h + 4) - (long double)F32(h + 0x10) * 2.0f) + F32(h + 0x1c)) * t + F32(h + 0x10) - F32(h + 4));
    float len = (float)FUN_00011253((double)((long double)dx * dx + (long double)dz * dz));
    if (!(s2_bits(len) & 0x7fffffff)) len = 1.0f;
    return (double)len;
}

/* THE THROTTLE from the player's accelerate control (0x174ca): its share of the most (1023) while
   held the way it was pressed (its entry's +0x14); nought when let go -- not for the computer's
   cars (no controls' record) */
#define HOOK_174ca
static void FUN_000174ca(int car)
{
    const uint32_t keys = U32(car + 0xb4);
    if (!keys) return;
    const int32_t v = I32(keys + 0xa8);
    if (0.0f > F32(car + 0x1ec) || I32(keys + 0x14) != (v < 0 ? -1 : 1)) { F32(car + 0x1ec) = 0.0f; return; }
    F32(car + 0x1ec) = (float)((long double)(v < 0 ? -v : v) / 1023.0f);
}

/* a camera's view (0x12d31, +0xae4) and, at night or in fog, how far it sees (0x113738 / 0x11373c:
   the fade's far and near): night -27000 (view 1, the one from the car) or -35000 / -6000; fog
   -25000; not for view 5 (the mirror's) */
#define HOOK_12d31
static void FUN_00012d31(int camera, uint32_t view)
{
    U32(camera + 0xae4) = view;
    if (I32(camera + 0xae4) == 5 || U32(0x130c50) == 0) return;
    if (U32(0x130c50) < 2) {
        if (I32(camera + 0xae4) == 1) { I32(0x113738) = -27000; I32(0x11373c) = 0; }
        else { I32(0x113738) = -35000; I32(0x11373c) = -6000; }
    } else if (U32(0x130c50) == 2) { I32(0x113738) = -25000; I32(0x11373c) = 0; }
}

/*
 * WHERE TWO LINES MEET (0x20970): the line through p in direction d and the one through q in
 * direction e (x at +0, z at +8 of each); the two distances along them, s along d and u along e,
 * into *s and *u; 0 when they run side by side.
 *     n = e.z d.x - d.z e.x;  a = q.x - p.x;  b = p.z - q.z;
 *     s = (a e.z + b e.x) / n;  u = (a d.z + b d.x) / n
 */
#define HOOK_20970
static int FUN_00020970(int p, int d, int q, int e, int s, int u)
{
    const float n = (float)((long double)F32(e + 8) * F32(d) - (long double)F32(d + 8) * F32(e));
    if (!(s2_bits(n) & 0x7fffffff)) return 0;
    const float a = (float)((long double)F32(q) - F32(p)), b = (float)((long double)F32(p + 8) - F32(q + 8));
    F32(s) = (float)(((long double)a * F32(e + 8) + (long double)b * F32(e)) / n);
    F32(u) = (float)(((long double)a * F32(d + 8) + (long double)b * F32(d)) / n);
    return 1;
}

/*
 * HOW FAR ALONG THE LINE FROM c TO b THE POINT a FALLS (0x1766c), as a share of its length:
 *     (((b.z - c.z) - a.z) b.z + ((b.x - c.x) - a.x) b.x + c.x a.x + c.z a.z) / |b - c|^2
 * summed in that order (the decompiler's text had another); 0 for a line of no length
 */
#define HOOK_1766c
static long double FUN_0001766c(int a, int b, int c)
{
    const float len2 = (float)(((long double)F32(b + 8) - F32(c + 8)) * ((long double)F32(b + 8) - F32(c + 8))
                             + ((long double)F32(b) - F32(c)) * ((long double)F32(b) - F32(c)));
    if (!(s2_bits(len2) & 0x7fffffff)) return 0.0L;
    long double num = ((long double)F32(b + 8) - F32(c + 8) - F32(a + 8)) * F32(b + 8);
    num += ((long double)F32(b) - F32(c) - F32(a)) * F32(b);
    num += (long double)F32(c) * F32(a);
    num += (long double)F32(c + 8) * F32(a + 8);
    return (long double)(float)((long double)(float)num / len2);
}

/* A POINT OF A PATH at t nodes from its start (0x152f7): the node (whole part, round the path's
   +0x1c nodes), the share of it as two halves (+8, the second half +0xc 1), the curve's point
   worked out (0x1099a) and copied out as x, z, y (+0x60, +0x68, +0x64) */
#define HOOK_152f7
static void FUN_000152f7(int path, int state, int out, uint32_t r3, float t)
{
    I32(state + 4) = (int32_t)((long double)I32(path + 0x1c) + t);
    F32(state + 8) = (float)(((long double)t - (long double)(I32(state + 4) - I32(path + 0x1c))) * 2.0f);
    const int nodes = I32(path + 0x1c);
    I32(state + 4) = I32(state + 4) % nodes;
    if (I32(state + 8) < 0x3f800000) I32(state + 0xc) = 0;
    else { F32(state + 8) = (float)((long double)F32(state + 8) + -1.0f); I32(state + 0xc) = 1; }
    FUN_0001099a(path, state, (uint32_t)nodes, r3);
    F32(out) = F32(state + 0x60); F32(out + 4) = F32(state + 0x68); F32(out + 8) = F32(state + 0x64);
}

/* THE CAR'S CLOCKS (0x1f3ce), by the raw time: while the race runs and the car has not finished,
   the race's time (+0xc38), the lap's (+0xc3c) and the lap's split (+0xc44 + 4 x (laps % 5));
   always two counters down to nought (+4, +0x30) */
#define HOOK_1f3ce
static void FUN_0001f3ce(int car)
{
    const float raw = F32(0x133354);
    if ((U8(0x11f0a8) & 0x20) && !(U8(car + 0x4c) & 2)) {
        F32(car + 0xc38) = (float)((long double)raw + F32(car + 0xc38));
        F32(car + 0xc3c) = (float)((long double)raw + F32(car + 0xc3c));
        const uint32_t at = (uint32_t)car + 0xc44u + 4u * (uint32_t)(I32(car + 0x4b4) % 5);
        F32(at) = (float)((long double)raw + F32(at));
    }
    F32(car + 4) = (float)((long double)F32(car + 4) - raw);
    if (0.0f > F32(car + 4)) F32(car + 4) = 0.0f;
    F32(car + 0x30) = (float)((long double)F32(car + 0x30) - raw);
    if (0.0f > F32(car + 0x30)) F32(car + 0x30) = 0.0f;
}

/* THE BRAKE from the player's brake control (0x1756f): its share of the most (1023) while held
   the way it was pressed (its entry's +0x30), split by the car's balance (+0x9c) into the front's
   (+0x1e4) and the rest (+0x1e8); none for the computer's cars */
#define HOOK_1756f
static void FUN_0001756f(int car)
{
    const uint32_t keys = U32(car + 0xb4);
    if (!keys) { F32(car + 0x1e8) = 0.0f; F32(car + 0x1e4) = 0.0f; return; }
    const int32_t v = I32(keys + 0xac);
    if (I32(keys + 0x30) == (v < 0 ? -1 : 1) && v != 0) F32(car + 0x1e8) = (float)((long double)(v < 0 ? -v : v) / 1023.0f);
    else F32(car + 0x1e8) = 0.0f;
    F32(car + 0x1e4) = (float)((long double)F32(car + 0x1e8) * F32(car + 0x9c));
    F32(car + 0x1e8) = (float)((long double)F32(car + 0x1e8) - F32(car + 0x1e4));
}

/* A PATH'S POINT (0x1099a): the path's nodes (0x38 bytes: +0x10 the next node's number, +0xc
   the height, +0x14 / +0x18 and +0x20 / +0x24 the two handles' x, z) from the table entry of the
   path (+0x24 a path) at state +0; the node state +4, its half state +0xc, the share state +8.
   The curve through three handles -- the node's two and the next's first (first half), or the
   node's second and the next's two (second half) -- as floats at state +0x3c, its point by 0x4a831
   into +0x60; the height straight between the two nodes' over the whole node:
       height = h0 + ((half + share) x (h1 - h0)) x 0.5 */
#define HOOK_1099a
static void FUN_0001099a(int path, int state, uint32_t r2, uint32_t r3)
{
    const uint32_t nodes = U32((uint32_t)path + (uint32_t)I32(state) * 0x24u);
    const uint32_t a = nodes + (uint32_t)I32(state + 4) * 0x38u, b = nodes + (uint32_t)I32(a + 0x10) * 0x38u;
    const uint32_t h[3] = { I32(state + 0xc) == 0 ? a + 0x14 : a + 0x20, I32(state + 0xc) == 0 ? a + 0x20 : b + 0x14, I32(state + 0xc) == 0 ? b + 0x14 : b + 0x20 };
    for (uint32_t k = 0; k < 3; k++) { F32(state + 0x3c + 0xc * k) = (float)I32(h[k]); F32(state + 0x40 + 0xc * k) = (float)I32(h[k] + 4); }
    FUN_0004a831(state + 0x60, state + 0x3c, r2, r3, F32(state + 8));
    F32(state + 0x68) = (float)((long double)(I32(b + 0xc) - I32(a + 0xc)) * ((long double)I32(state + 0xc) + F32(state + 8)) * 0.5 + I32(a + 0xc));
}

/*
 * THE CAMERAS IN THE CAR: the camera record's car (+0xadc), the car's object (car +0x990: x y z,
 * three angles at +0xc..+0x14, +0x1c its height's offset).  The camera stands at the object's
 * place, moved by the car's own +0xc00 (x) and up by its +0x218 and 400; it leans with the body on
 * its springs (car +0xc18 pitch, half of +0xc20 the roll).
 * 0x13bab looks ahead (200 rows); 0x13e1a, the mirror's, looks back (heading + pi, pitch and roll
 * turned round; 35 rows)
 */
static void s2r_camera_in_car(int camera, int back)
{
    const uint32_t car = U32(camera + 0xadc), obj = U32(car + 0x990);
    if (!back) {
        F32(camera + 0x54) = (float)((long double)F32(obj + 0xc) + F32(car + 0xc18));
        F32(camera + 0x58) = F32(obj + 0x10);
        F32(camera + 0x5c) = (float)((long double)F32(obj + 0x14) + (long double)F32(car + 0xc20) * 0.5);
    } else {
        F32(camera + 0x54) = (float)(-(long double)F32(obj + 0xc) - F32(car + 0xc18));
        F32(camera + 0x58) = (float)((long double)F32(obj + 0x10) + (float)3.14159274f);
        F32(camera + 0x5c) = (float)(-(long double)F32(obj + 0x14) - (long double)F32(car + 0xc20) * 0.5);
    }
    I32(camera + 0x48) = I32(obj) + I32(car + 0xc00);
    I32(camera + 0x4c) = I32(obj + 4) + I32(obj + 0x1c) + I32(car + 0x218) + 400;
    I32(camera + 0x50) = I32(obj + 8);
    I32(camera + 0x1c) = back ? 35 : 200;
}
#define HOOK_13bab
static void FUN_00013bab(int camera) { s2r_camera_in_car(camera, 0); }
/* the dashboard's view (0x13cbd): as the one from the car but 300 up, and its rows half the
   dashboard picture's (CRUSC's first number, 0x1242b4 + 0x58 a model: the car's, 0x124574[car];
   in time attack the first entry's) */
#define HOOK_13cbd
static void FUN_00013cbd(int camera)
{
    s2r_camera_in_car(camera, 0);
    const uint32_t car = U32(camera + 0xadc), obj = U32(car + 0x990);
    I32(camera + 0x4c) = I32(obj + 4) + I32(obj + 0x1c) + I32(car + 0x218) + 300;
    const uint32_t model = (U8(0x130d10) & 4) ? U32(0x124574) : U32(0x124574 + 4u * U32(car));
    I32(camera + 0x1c) = I32(0x1242b4 + model * 0x58u) / 2;
}
#define HOOK_13e1a
static void FUN_00013e1a(int camera) { s2r_camera_in_car(camera, 1); }

/* a camera of the third kind (0x13768): from the car's place towards its row's (+0x60..) by
   0.8 - 0.75 x the row's +0x7c of the way, each whole number cut; looking at the car (0x338ed),
   its roll the row's +0x74; 200 rows */
#define HOOK_13768
static void FUN_00013768(int camera)
{
    const uint32_t row = (uint32_t)camera + (uint32_t)I32(camera + 0x24) * 0x20u, obj = U32(U32(camera + 0xadc) + 0x990);
    for (uint32_t k = 0; k < 3; k++) {
        const long double share = (double)0.8 - (long double)F32(row + 0x7c) * 0.75;
        I32(camera + 0x48 + 4 * k) = (int32_t)((long double)(I32(row + 0x60 + 4 * k) - I32(obj + 4 * k)) * share + I32(obj + 4 * k));
    }
    FUN_000338ed((uint32_t)camera + 0x54, obj, (uint32_t)camera + 0x48);
    F32(camera + 0x5c) = F32(row + 0x74);
    I32(camera + 0x1c) = 200;
}

/* a camera of the fifth kind (0x13a8e): 1320 + 500 s along and 550 + 200 s up (s the row's
   +0x7c, cut), turned to the car's heading by the row's +0x70 + pi (0x10388), from the car's place;
   looking at the car, no roll; 200 rows */
#define HOOK_13a8e
static void FUN_00013a8e(int camera, uint32_t r1, uint32_t r2, uint32_t r3)
{
    (void)r1;
    const uint32_t row = (uint32_t)camera + (uint32_t)I32(camera + 0x24) * 0x20u, obj = U32(U32(camera + 0xadc) + 0x990);
    I32(camera + 0x48) = (int32_t)((long double)F32(row + 0x7c) * 500.0f + 1320.0);
    I32(camera + 0x4c) = (int32_t)((long double)F32(row + 0x7c) * 200.0f + 550.0);
    I32(camera + 0x50) = 0;
    const uint32_t sp = s2_sp, turned = s2_push(12);
    FUN_00010388((int)turned, camera + 0x48, r2, r3, (float)((long double)F32(row + 0x70) + (float)3.14159274f));
    for (uint32_t k = 0; k < 3; k++) I32(camera + 0x48 + 4 * k) = I32(obj + 4 * k) + I32(turned + 4 * k);
    s2_sp = sp;
    FUN_000338ed((uint32_t)camera + 0x54, obj, (uint32_t)camera + 0x48);
    I32(camera + 0x5c) = 0;
    I32(camera + 0x1c) = 200;
}

/* the angle from b to a, brought into -pi .. pi (0x14dc0; "over pi" is tested on the float's bits
   as a whole number, as the game does: a negative angle is never over) */
#define HOOK_14dc0
static double FUN_00014dc0(float a, float b)
{
    float d = (float)((long double)a - b);
    while (s2_bits(d) > 0x40490fdb) d = (float)((long double)d + -(float)6.28318548f);
    while (d < -(float)3.14159274f) d = (float)((long double)d + (float)6.28318548f);
    return (double)d;
}

/*
 * THE GEARBOX (0x1f697): one gear up (+1) or down (-1).  Only within the car's gears (0 to car
 * +0x68), and not while the last change is still settling (car +0x140: the half second set here,
 * counted down by the frame).  The revs jump to what the new gear makes of the wheels' speed,
 * with three tenths of the old kept:
 *     revs = ratio[gear] x 0.7 x the most revs x (wheel speed / the speed at the most revs)
 *            + revs x 0.3
 * multiplied in that order (the converted line had another, and the revs came out a bit off the
 * original's after a change of gear).  The ratios are the table at 0x112e24.
 */
#define HOOK_1f697
static void FUN_0001f697(int car, int by)
{
    const int gear = I32(car + 0xc7c) + by;
    if (gear > I32(car + 0x68) || gear < 0 || by == 0 || F32(car + 0x140) > 0.0f) return;
    I32(car + 0xc7c) = gear;
    long double revs = (long double)F32(0x112e24u + 4u * (uint32_t)gear) * (double)0.7;
    revs *= F32(car + 0x60);
    revs *= (long double)F32(car + 0x4fc) / F32(car + 0x5c);
    revs += (long double)F32(car + 0xc78) * (double)0.3;
    F32(car + 0xc78) = (float)revs;
    F32(car + 0x140) = 0.5f;
}

#if 1                                   /* (0x1f756: being checked against the traces) */
/*
 * THE ENGINE (0x1f756), every frame for every car: the gearbox, the revs, the wheels' speed the
 * engine drives at, the engine's sound.
 *   car +0x60 the most revs, +0x5c the speed at the most revs in top, +0x68 the top gear, +0x64 0
 *   a gearbox of the player's (manual) / 1 the car's own (automatic), +0x140 the time a change
 *   still takes, +0xc7c the gear (0 neutral), +0xc78 the revs, +0x4fc the speed the engine turns
 *   the wheels at, +0x500 the car's speed, +0x510 the throttle (1.0 full), +0x1ec the grip's
 *   share, +0x70 the engine's strength, +0xc34 the wheels' grip, +0x4d4 the slip angle, +8 the
 *   car's damage (to 2), +0x48 what the driver takes off the speed, +0xb4 the controls' record
 *   (0: the computer drives), +0x6b0 the road's kind under it, +0x4f0 / +0x74c in a tunnel now /
 *   when the sound was last chosen, +0x990 its place's record.
 */
static long double s2r_sq(long double x) { return x * x; }       /* (the game's pow(x, 2): one product, 0xb0005) */
#define HOOK_1f756
static void FUN_0001f756(int car)
{
    const float step = F32(0x133350), raw = F32(0x133354);
    /* an automatic's change settles by the raw time */
    if (I32(car + 0x64) == 1) {
        F32(car + 0x140) = (float)((long double)F32(car + 0x140) - raw);
        if (0.0f > F32(car + 0x140)) F32(car + 0x140) = 0.0f;
    }
    /* the gearbox: only while the race runs (0x11f0a8 & 0x20); else neutral */
    if (!(U8(0x11f0a8) & 0x20)) I32(car + 0xc7c) = 0;
    else if (I32(car + 0x64) == 0 && U32(car + 0xb4)) {
        /* the player's: a gear control pressed and held the same way as it was (its entry's +0x14) */
        const uint32_t keys = U32(car + 0xb4);
        const int up = I32(keys + 0xb8) < 0 ? -1 : 1, down = I32(keys + 0xbc) < 0 ? -1 : 1;
        if (I32(keys + 0x84) == up && I32(keys + 0xb8) != 0) FUN_0001f697(car, 1);
        else if (I32(keys + 0xa0) == down && I32(keys + 0xbc) != 0) FUN_0001f697(car, -1);
        else F32(car + 0x140) = 0.0f;
    } else {
        /* the car's own: first gear from neutral; up over 0.8 of the most revs (less a tenth of
           the grip) at full throttle with the wheels gripping (over 0.89); down under 0.55 (plus
           a tenth of the grip) at full throttle, never into neutral */
        if (I32(car + 0xc7c) == 0) { F32(car + 0x140) = 0.5f; I32(car + 0xc7c) = 1; }
        const long double most = F32(car + 0x60), grip = F32(car + 0x1ec), revs = F32(car + 0xc78);
        if (revs > most * (double)0.8 - grip * (double)0.1) {
            if (U32(car + 0x510) == 0x3f800000u && fabsl((long double)F32(car + 0xc34)) > (double)0.89) FUN_0001f697(car, 1);
        } else if (revs < most * (double)0.55 + grip * (double)0.1 && I32(car + 0xc7c) > 1 && U32(car + 0x510) == 0x3f800000u)
            FUN_0001f697(car, -1);
    }
    /* no more than 1.1 of the most revs */
    if ((long double)F32(car + 0xc78) > (long double)F32(car + 0x60) * (double)1.1) F32(car + 0xc78) = (float)((long double)F32(car + 0x60) * (double)1.1);
    const int gear = I32(car + 0xc7c);
    const long double ratio = gear >= 0 ? F32(0x112e24u + 4u * (uint32_t)gear) : 0.0L;
    /* in gear with the throttle open, the revs follow the wheels as far as they grip: a share w of
       the way each frame (4 x the step x the grip, at most all of it) */
    if (gear != 0 && 0.0f < F32(car + 0x510)) {
        long double w = fabsl((long double)F32(car + 0xc34)) * ((long double)step * 4.0f);
        if (1.0L <= w) w = 1.0L;
        const float wf = (float)(double)w;
        long double wheels = (long double)wf * F32(car + 0x500);
        wheels *= fabsl(cosl((long double)F32(car + 0x4d4)));
        wheels *= (long double)F32(car + 0x60) / F32(car + 0x5c);
        wheels *= ratio;
        F32(car + 0xc78) = (float)(wheels + (1.0L - wf) * F32(car + 0xc78));
    }
    /* the engine's pull at these revs: a curve of the revs' share r of the most, by the gear */
    const float r = (float)((long double)F32(car + 0xc78) / F32(car + 0x60));
    const long double c = gear != 0 ? (double)1.6 : 1.5;
    float pull = (float)((1.0L - s2r_sq(ratio * (double)0.22 + ((long double)r * c + -1.0))) * s2r_sq(ratio) * (double)0.8);
    if (0.0f > pull) pull = (float)((long double)pull * 2.0f);
    /* less with damage over 1: by 0.3 of the excess */
    float hurt = 0.0f;
    if (0.0L < (long double)F32(car + 8) + -1.0f) hurt = (float)((long double)F32(car + 8) + -1.0f);
    float force = (float)((long double)pull * F32(car + 0x60) * F32(car + 0x1ec) * F32(car + 0x70) * (1.0L - (long double)hurt * (double)0.3));
    /* what holds the revs back: the wheels' load in gear, and a fifth of the most revs always */
    float load = 0.0f;
    if (gear != 0) load = (float)(ratio * 0.5 * F32(car + 0xc34) * F32(car + 0x510) * F32(car + 0x60) * F32(car + 0x500) / F32(car + 0x5c));
    load = (float)(double)((long double)F32(car + 0x60) * (double)0.2 + (double)load);
    force = (float)((long double)force - load);
    F32(car + 0xc78) = (float)((long double)step * force + F32(car + 0xc78));
    if (0.0f > F32(car + 0xc78)) F32(car + 0xc78) = 0.0f;
    else if (0.0f < F32(car + 0x510)) {
        if ((long double)F32(car + 0xc78) > (long double)F32(car + 0x60) * (double)0.98) F32(car + 0xc78) = (float)((long double)F32(car + 0x60) * (double)0.98);
    } else if (F32(car + 0xc78) > F32(car + 0x60)) F32(car + 0xc78) = F32(car + 0x60);
    /* the speed the engine turns the wheels at */
    if (gear != 0) {
        if (0.0f < F32(car + 0x510)) {
            F32(car + 0x4fc) = (float)((long double)F32(car + 0xc78) / ratio * ((long double)F32(car + 0x5c) / F32(car + 0x60)));
            if (U8(car + 0x4c) & 0x10) F32(car + 0x4fc) = (float)((long double)F32(car + 0x4fc) + -2.5f);
            if (!U32(car + 0xb4)) F32(car + 0x4fc) = (float)((long double)F32(car + 0x4fc) - F32(car + 0x48));
        }
        if (I32(car + 0x4fc) < 0x41200000 && (long double)F32(car + 0x1ec) < (double)0.1) F32(car + 0x4fc) = 0.0f;   /* (under 10 -- the bits as a whole number -- and hardly gripping) */
    } else F32(car + 0x4fc) = F32(car + 0x500);
    if (F32(car + 0xc78) > F32(car + 0x60)) F32(car + 0xc78) = F32(car + 0x60);
    else if (0.0f > F32(car + 0xc78)) F32(car + 0xc78) = 0.0f;
    /* THE ENGINE'S SOUND: its sample by the team and model (a fifth model is its team's own, + 4),
       the tunnel's version (+0x19) on road kinds 7, 13, 34, 43; chosen again when that changes;
       then the slot's place, heard from the camera of the player (the second in a two players'
       race for its car), and its pitch: 2 x revs / the most */
    const int heard_by = ((U8(0x130d10) & 0x40) && I32(car) != 0) ? 1 : 0;
    const uint32_t kind = U32(car + 0x6b0);
    I32(car + 0x4f0) = kind == 7 || kind == 13 || kind == 0x22 || kind == 0x2b;
    if (I32(car + 0x4f0) != I32(car + 0x74c)) {
        const uint32_t entry = 0x130c58u + (uint32_t)I32(car) * 0x2cu;
        int sample = I32(entry + 4);
        if (sample == 4) sample = I32(entry) + 4;
        FUN_000aee98(U32(0x124274), I32(car));
        FUN_000aea54(U32(0x124274), I32(car + 0x4f0) ? sample + 0x19 : sample, I32(car), 0, 1, 1);
    }
    const uint32_t cam = U32(0x1192f4u + 4u * (uint32_t)heard_by);
    U32(0x124280) = U32(car);
    U32(0x124284) = U32(cam + 0x48);
    U32(0x124288) = U32(cam + 0x50);
    F32(0x12428c) = (float)((long double)F32(cam + 0x58) + -(float)3.14159274f);
    U32(0x124290) = U32(U32(car + 0x990));
    U32(0x124294) = U32(U32(car + 0x990) + 8);
    I32(0x124298) = 50000;
    F32(0x12429c) = (float)((long double)F32(car + 0xc78) / F32(car + 0x60) * 2.0f);
    FUN_000af168(U32(0x124274), 0x124280u);
}
#endif

#endif
