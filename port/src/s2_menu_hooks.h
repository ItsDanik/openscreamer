/*
 * SCREAMER 2'S MENU: the engine and library calls its routines make (tools/s2c.py lists them as
 * "hooks wanted"), answered for the game's routines.  Included by s2_hooks.c before s2r_car.h.
 * The menu's routines are the game's own (from its loop 0x38540, docs/NOTES2.md S2-5zq); what
 * is answered here is the engine under them: files read out of the containers, named blocks of
 * memory, the object list's loading, the screen, the keys, the clock.  The drawing itself is the
 * port's (main.c, s2m_*).  While the race runs (s2_in_menu = 0) the calls the race shares with
 * the menu do nothing, as before.  Lines marked "not answered yet" do nothing and give 0.
 */
#include <setjmp.h>
#include <stdarg.h>

/* a whole number turned by the step (tools/s2c.py turn_by: the menu's angles += step x K, cut
   to a whole number every frame).  At the game's own frame rates (the step 1/50 s or more) as
   it is; under that -- the port draws the menu as fast as the screen shows it -- what is cut off
   is kept, by the number's address, and added the next time, so that a turn takes its time */
static int32_t s2_turn_by(uint32_t at, float k)
{
    static struct { uint32_t at; float part; } kept[64];
    const float v = (float)*(int32_t *)MP(at) + *(float *)MP(0x133350u) * k;
    if (*(float *)MP(0x133350u) >= 0.02f) return (int32_t)v;
    int i = 0;
    while (i < 64 && kept[i].at && kept[i].at != at) i++;
    if (i == 64) i = (int)(at % 64u);
    if (kept[i].at != at) { kept[i].at = at; kept[i].part = 0.0f; }
    const float w = v + kept[i].part;
    const int32_t r = (int32_t)w;
    kept[i].part = w - (float)r;
    return r;
}

static void s2m_list_clear(void);     /* (0x4b016: the draw list emptied; s2_hooks.c, after the routines) */
static jmp_buf s2_menu_out;            /* where the game's exit (0xae662) goes back to */
static int s2_menu_exit_code;

/* ---- memory: the menu's blocks come and go (the engine's named blocks, malloc / free).
   First fit over what is left of the block after the menu's start, each with 16 bytes before it:
   its size (with them) and whether it is free; free neighbours are joined as they are passed. */
static uint32_t s2m_heap_hi;
static void s2m_heap_start(void)
{
    s2m_heap_lo = (s2_heap + 15u) & ~15u; s2m_heap_hi = STACK_AT;
    I32(s2m_heap_lo) = (int32_t)(s2m_heap_hi - s2m_heap_lo); I32(s2m_heap_lo + 4) = 1;
}
static uint32_t s2m_alloc(uint32_t n)
{
    n = (n + 15u + 16u) & ~15u;
    for (uint32_t at = s2m_heap_lo; at < s2m_heap_hi; at += (uint32_t)I32(at)) {
        if (I32(at) < 16) { fprintf(stderr, "menu: the block's memory is broken at 0x%x\n", at); return 0; }
        if (!I32(at + 4)) continue;
        while (at + (uint32_t)I32(at) < s2m_heap_hi && I32(at + (uint32_t)I32(at) + 4)) I32(at) += I32(at + (uint32_t)I32(at));   /* (joined) */
        if ((uint32_t)I32(at) < n) continue;
        if ((uint32_t)I32(at) >= n + 32u) { const uint32_t rest = at + n; I32(rest) = I32(at) - (int32_t)n; I32(rest + 4) = 1; I32(at) = (int32_t)n; }
        I32(at + 4) = 0;
        memset(MP(at + 16), 0, (size_t)I32(at) - 16);
        return at + 16;
    }
    fprintf(stderr, "menu: out of the block's memory (%u wanted)\n", n);
    return 0;
}
static void s2m_free(uint32_t p) { if (p >= s2m_heap_lo + 16 && p < s2m_heap_hi && !I32(p - 12)) I32(p - 12) = 1; }

/* ---- the C library ---- */
#define HOOK_ae698                     /* malloc */
static uint32_t s2m_last_block;
static uint32_t s2m_malloc(uint32_t n) { return s2m_last_block = s2m_alloc(n ? n : 16); }
#define HOOK_ae710                     /* (C++'s array made, after the malloc: the block it was given) */
#define FUN_000ae710(...) s2m_last_block
#define HOOK_ae5f4                     /* atoi */
#define FUN_000ae5f4(s, ...) ((uint32_t)atoi((const char *)MP((uint32_t)(s))))
#define HOOK_ae762
#define FUN_000ae762(s, ...) ((uint32_t)atoi((const char *)MP((uint32_t)(s))))
#define HOOK_ae758                     /* atof (its number in ST0) */
#define FUN_000ae758(s, ...) ((long double)atof((const char *)MP((uint32_t)(s))))
#define HOOK_ae7cf                     /* strstr */
static uint32_t FUN_000ae7cf(uint32_t s, uint32_t t) { const char *a = (const char *)MP(s), *f = strstr(a, (const char *)MP(t)); return f ? s + (uint32_t)(f - a) : 0; }
#define FUN_000ae698(n, ...) s2m_malloc((uint32_t)(n))
#define HOOK_ae662                     /* exit (after 0x4b169 / 0x4b1a4 have said why): the menu ends */
static void FUN_000331dc(void);
static int s2m_window_gone;            /* the menu left by closing the window (s2_menu_run then saves) */
static void FUN_000ae662(uint32_t code) { s2_menu_exit_code = (int)code; if (s2_in_menu) longjmp(s2_menu_out, 1); }
#define HOOK_b6e7e                     /* strlen */
static uint32_t FUN_000b6e7e(uint32_t s) { return (uint32_t)strlen((const char *)MP(s)); }
#define HOOK_b3d04                     /* free */
#define S2M_FIRST(a, ...) (a)
#define FUN_000b3d04(...) (s2m_free((uint32_t)(S2M_FIRST(__VA_ARGS__ __VA_OPT__(,) 0))), 0ull)   /* (one call has lost its argument: nothing) */
#define HOOK_b3c09                     /* (already the race's: see s2_hooks.c) */

/* sprintf (0x4eaed: the buffer, the format, as many more as it wants; tools/s2_sigs.txt): the
   game's strings are addresses in its memory; %s %c %d %x and widths are what it uses */
#define HOOK_4eaed
static int FUN_0004eaed(uint32_t buf, uint32_t fmt, ...)
{
    va_list ap;
    char out[512], spec[16];
    const char *f = (const char *)MP(fmt);
    size_t o = 0;
    va_start(ap, fmt);
    while (*f && o < sizeof out - 1) {
        if (*f != '%') { out[o++] = *f++; continue; }
        size_t k = 0;
        spec[k++] = *f++;
        while (*f && strchr("0123456789.-+ #l", *f) && k < sizeof spec - 2) spec[k++] = *f++;
        const char c = *f ? *f++ : 0;
        spec[k++] = c; spec[k] = 0;
        char piece[256];
        if (c == 's') snprintf(piece, sizeof piece, spec, (const char *)MP(va_arg(ap, uint32_t)));
        else if (c == '%') snprintf(piece, sizeof piece, "%%");
        else if (c == 'f' || c == 'g') snprintf(piece, sizeof piece, spec, va_arg(ap, double));
        else snprintf(piece, sizeof piece, spec, va_arg(ap, int));
        for (const char *p = piece; *p && o < sizeof out - 1; p++) out[o++] = *p;
    }
    va_end(ap);
    out[o] = 0;
    memcpy(MP(buf), out, o + 1);
    return (int)o;
}

/* ---- the engine's named blocks of memory (0x4f0c6 make, 0x4f3c8 find, 0x4f033 / 0x4f331
   let go): blocks from the port's heap; nothing is given back while the menu runs ---- */
#define HOOK_4f0c6
static uint32_t FUN_0004f0c6(uint32_t heap, uint32_t name, uint32_t n) { (void)heap; (void)name; return s2m_alloc(n ? n : 16); }
#define HOOK_4f3c8
#define FUN_0004f3c8(...) 0
#define HOOK_4f033                     /* (said when a block could not be had: 0x4f0c6 has said it) */
#define FUN_0004f033(...) 0
#define HOOK_4f331                     /* a block let go */
#define FUN_0004f331(heap, p, ...) (s2m_free((uint32_t)(p)), 0ull)

/* ---- containers.  The game names one as "<its directory>\gfx\logo1.dat"; the port reads
   its files as build/dec/LOGO1/<NAME> (port.c packed_open) ---- */
static void s2m_cont_of(uint32_t path, char *out, size_t n)
{
    const char *p = (const char *)MP(path), *b = p;
    for (const char *q = p; *q; q++) if (*q == '\\' || *q == '/' || *q == ':') b = q + 1;
    size_t k = 0;
    for (; b[k] && b[k] != '.' && k < n - 1; k++) out[k] = (char)toupper((unsigned char)b[k]);
    out[k] = 0;
}
#define HOOK_4b49f                     /* the container things are loaded from from now on */
static int FUN_0004b49f(uint32_t path, uint32_t again)
{
    (void)again;
    if (s2_in_menu) s2m_cont_of(path, s2m_cont, sizeof s2m_cont);
    if (s2_in_menu && getenv("OS_S2MLOG")) printf("menu: container %s (%s)\n", s2m_cont, (const char *)MP(path));
    return 0;
}
#define HOOK_4feb4                     /* a material library of the container (MAP, LOGO, PLATES ...) */
static int FUN_0004feb4(uint32_t name, uint32_t slot)
{
    char n[16];
    if (!s2_in_menu) return 0;
    s2m_cont_of(name, n, sizeof n);
    s2m_materials(s2m_cont, n, (int)slot);     /* (slot: the flags -- 4 = the reflections' map) */
    snprintf(s2m_lib, sizeof s2m_lib, "%s/%s", s2m_cont, n);
    if (getenv("OS_S2MLOG")) printf("menu: materials %s of %s\n", n, s2m_cont);
    return 0;
}

/* ---- the engine's files (a record of 0xb0 bytes in the caller's frame): 0x4eba5 open an
   entry of a container, 0x4ee3a read, 0x4ef0a read a line or a word, 0x4eea5 seek, 0x4ed79
   close, 0x4eb34 say what went wrong.  The port keeps the file's bytes, by the record. ---- */
#define S2M_FILES 8
static struct { uint32_t rec; uint8_t *data; long len, at; } s2m_file[S2M_FILES];
static int s2m_file_of(uint32_t rec)
{
    for (int i = 0; i < S2M_FILES; i++) if (s2m_file[i].rec == rec && s2m_file[i].data) return i;
    return -1;
}
#define HOOK_4eba5
static int FUN_0004eba5(uint32_t rec, uint32_t cont, uint32_t entry, uint32_t mode)
{
    char c[16], path[160];
    (void)mode;
    s2m_cont_of(cont, c, sizeof c);
    snprintf(path, sizeof path, "build/dec/%s/%s", c, (const char *)MP(entry));
    for (char *q = path + 10; *q; q++) *q = (char)toupper((unsigned char)*q);
    int k = s2m_file_of(rec);
    if (k < 0) for (k = 0; k < S2M_FILES && s2m_file[k].data; k++) ;
    if (k >= S2M_FILES) return 1;
    free(s2m_file[k].data); s2m_file[k].data = NULL;
    FILE *f = os_fopen(path, "rb");
    if (getenv("OS_S2MLOG")) printf("menu: open %s (%s, %s)%s\n", path, (const char *)MP(cont), (const char *)MP(entry), f ? "" : " -- not there");
    if (!f) { fprintf(stderr, "menu: no %s\n", path); return 1; }
    fseek(f, 0, SEEK_END); const long len = ftell(f); fseek(f, 0, SEEK_SET);
    s2m_file[k].data = malloc((size_t)len + 1);
    s2m_file[k].len = s2m_file[k].data ? (long)fread(s2m_file[k].data, 1, (size_t)len, f) : 0;
    fclose(f);
    s2m_file[k].rec = rec; s2m_file[k].at = 0;
    /* (the record as the game's routines look at it: +8 its length, +0xc where it is) */
    I32(rec + 8) = (int32_t)s2m_file[k].len; I32(rec + 0xc) = 0; I32(rec + 0xac) = 0;
    return 0;
}
#define HOOK_4ee3a
static int FUN_0004ee3a(uint32_t rec, uint32_t to, int n)
{
    const int k = s2m_file_of(rec);
    if (k < 0 || n < 0) return 0;
    if (s2m_file[k].at + n > s2m_file[k].len) n = (int)(s2m_file[k].len - s2m_file[k].at);
    memcpy(MP(to), s2m_file[k].data + s2m_file[k].at, (size_t)n);
    s2m_file[k].at += n; I32(rec + 0xc) = (int32_t)s2m_file[k].at;
    return n;
}
/* a line (`whole` 1: up to its end, CR left out) or a word (0: spaces and tabs end it too,
   and empty ones are skipped); -1 at the end (0x4ef0a) */
#define HOOK_4ef0a
static int FUN_0004ef0a(uint32_t rec, uint32_t to, int whole)
{
    const int k = s2m_file_of(rec);
    if (k < 0) return -1;
    int n = 0;
    for (;;) {
        if (s2m_file[k].at >= s2m_file[k].len) { I32(rec + 0xac) = 4; if (n) break; return -1; }
        const char c = (char)s2m_file[k].data[s2m_file[k].at++];
        if (c == '\n' || (!whole && (c == ' ' || c == '\t'))) { if (whole || n) break; continue; }
        if (c != '\r') U8(to + (uint32_t)n++) = (uint8_t)c;
    }
    U8(to + (uint32_t)n) = 0;
    I32(rec + 0xc) = (int32_t)s2m_file[k].at;
    return 0;
}
#define HOOK_4eea5
#define FUN_0004eea5(rec, off, whence, ...) s2m_seek((uint32_t)(rec), (int)(off), (int)(whence))
static int s2m_seek(uint32_t rec, int off, int whence)
{
    const int k = s2m_file_of(rec);
    if (k < 0) return 0;
    s2m_file[k].at = whence == 0 ? off : whence == 1 ? s2m_file[k].at + off : s2m_file[k].len + off;
    if (s2m_file[k].at < 0) s2m_file[k].at = 0;
    if (s2m_file[k].at > s2m_file[k].len) s2m_file[k].at = s2m_file[k].len;
    I32(rec + 0xc) = (int32_t)s2m_file[k].at;
    return 0;
}
#define HOOK_4ed79
static int FUN_0004ed79(uint32_t rec, uint32_t r1) { (void)r1; const int k = s2m_file_of(rec); if (k >= 0) { free(s2m_file[k].data); s2m_file[k].data = NULL; s2m_file[k].rec = 0; } return 0; }
#define HOOK_4eb34
static int FUN_0004eb34(uint32_t rec) { fprintf(stderr, "menu: a file could not be read (record 0x%x)\n", rec); return 0; }

/* a palette of the container, NAME.COL (0x4fe21): 768 of 0..63 to `to` */
#define HOOK_4fe21
static int FUN_0004fe21(uint32_t name, uint32_t to, uint32_t r2, uint32_t r3)
{
    char n[16], path[160];
    (void)r2; (void)r3;
    s2m_cont_of(name, n, sizeof n);
    snprintf(path, sizeof path, "build/dec/%s/%s.COL", s2m_cont, n);
    FILE *f = os_fopen(path, "rb");
    if (!f) { fprintf(stderr, "menu: no %s\n", path); return 1; }
    const size_t got = fread(MP(to), 1, 768, f);
    fclose(f);
    return got == 768 ? 0 : 1;
}

/* ---- the screen ---- */
#define HOOK_51b02                     /* a picture onto the screen: at (x, y), its header (+8 width, +0xc height), its pixels */
static uint32_t FUN_00051b02(int x, int y, uint32_t head, uint32_t px)
{
    s2m_base(MP(0x1338cc));
    const int rows = s2_screen_hi() ? 400 : 200;    /* (STARTH: 640 x 400) */
    s2m_picture(x, y, I32(head + 8), I32(head + 0xc) > rows ? rows : I32(head + 0xc), MP(px));
    return 0;
}
#define HOOK_50f45                     /* text: the record, x, y, the text, font, colour, the space between letters */
static int s2m_fonts_ok;
static int FUN_00050f45(uint32_t rec, int x, int y, uint32_t text, int font, int colour, int space)
{
    (void)rec;
    if (!s2m_fonts_ok) { s2m_fonts(); s2m_fonts_ok = 1; }     /* (the game's loader 0x50dd3 is taken: the port's own copy of the font) */
    s2m_base(MP(0x1338cc));
    return s2m_text(x, y, (const char *)MP(text), font, colour, space);
}
#define HOOK_51020                     /* the width of a text (into *w; the height into *h) */
static int FUN_00051020(uint32_t rec, uint32_t w, uint32_t h, uint32_t text, int font, int space)
{
    (void)rec;
    if (!s2m_fonts_ok) { s2m_fonts(); s2m_fonts_ok = 1; }
    const int wide = s2m_text_wide((const char *)MP(text), font, space);
    if (w) I32(w) = wide;
    if (h) I32(h) = 8;
    return wide;
}
#define HOOK_50c0a                     /* a sprite: the record, x, y, which, (0) */
static int FUN_00050c0a(uint32_t rec, int x, int y, int n, int flag) { (void)rec; (void)flag; if (!s2m_fonts_ok) { s2m_fonts(); s2m_fonts_ok = 1; } s2m_base(MP(0x1338cc)); s2m_sprite(x, y, n); return 0; }
#define HOOK_51698                     /* a darkened box */
static uint32_t FUN_00051698(int x, int y, int w, int h) { s2m_base(MP(0x1338cc)); s2m_dark(x, y, w, h); return 0; }
#define HOOK_50d6c                     /* the sprites' clip box (record, x0, y0, x1, y1: x0 <= x < x1, y0 <= y < y1 --
                                          0xc6435; the HI SCORES TABLE's list scrolls in one, 0x420c0) */
static uint32_t FUN_00050d6c(uint32_t rec, int x0, int y0, int x1, int y1) { (void)rec; s2m_clip(0, x0, y0, x1, y1); return 0; }
#define HOOK_50fb9                     /* the text's (0xc6646: the same rule) */
static uint32_t FUN_00050fb9(uint32_t rec, int x0, int y0, int x1, int y1) { (void)rec; s2m_clip(1, x0, y0, x1, y1); return 0; }
#define HOOK_5105e                     /* the darkening tables from the palette: the port makes them as it draws (s2_dark_make) */
#define FUN_0005105e(...) 0
#define HOOK_51aa4                     /* a fill of bytes; of the game's screen (0x134794): the port's picture cleared */
static uint64_t FUN_00051aa4(uint32_t to, uint32_t n, uint32_t v)
{
    if (s2_in_menu && to == U32(0x134794) && to) s2m_clear((uint8_t)v);
    if (to) memset(MP(to), (int)(v & 255), n);
    return (uint64_t)n << 32 | to;
}
#define HOOK_51664                     /* the palette shown */
static uint32_t FUN_00051664(uint32_t pal, uint32_t r1) { (void)r1; s2m_palette(MP(pal)); return 0; }
/* the palette faded: amount (1 the game's own, 0 the colour), towards (r, g, b) of 0..255 */
#define HOOK_512f1
static void FUN_000512f1(float f, int r, int g, int b)
{
    uint8_t out[768];
    const int to[3] = { r >> 2, g >> 2, b >> 2 };
    float k = 1.0f - f;
    if (k < 0.0f) k = 0.0f;
    if (k > 1.0f) k = 1.0f;
    for (int i = 0; i < 768; i++)
        out[i] = (uint8_t)(int)((float)to[i % 3] + (float)((int)U8(0x1338cc + (uint32_t)i) - to[i % 3]) * k);
    s2m_palette(out);
}
#define HOOK_7f504                     /* the far plane: the port's own */
#define FUN_0007f504(...) 0
#define HOOK_7e8b4                     /* the camera from now on: its record */
static uint32_t s2m_camera;
static uint32_t FUN_0007e8b4(uint32_t rec, uint32_t r1) { (void)r1; s2m_camera = rec; return rec; }
/* the engine's list drawn (0x753a0, 0x753e8): every object in it that is shown, with its
   name and container, its place and turn (its record: +8 x, +0xc y, +0x10 z, +0x14.. three
   angles, +0x20 how far through its frames) */
static void s2m_draw_list(uint32_t list, int room)
{
    static s2m_item it[200];
    int n = 0;
    if (getenv("OS_S2MAPYAW"))                  /* (a test: the track maps' turn held, to match a picture of the original's) */
        for (uint32_t k = 0; k < 7; k++) {
            const uint32_t id = U32(0x131058 + k * 8), rec = id < 200 ? U32(0x11da20 + id * 0x1c) : 0;
            if (rec) *(int32_t *)MP(rec + 0x18) = atoi(getenv("OS_S2MAPYAW"));
        }
    for (int i = 0; i < 400 && n < 200; i++) {
        const uint32_t e = U32(list + 4u * (uint32_t)i);
        if (e == 0) break;
        if (e == 0xffffffffu || (e & 0xc0000000u) == 0xc0000000u) continue;     /* a free place, a hidden one */
        int k = -1;
        for (int j = 0; j < s2_nobject; j++) if (U32(0x11da20 + (uint32_t)j * 0x1c) == e) { k = j; break; }
        if (k < 0) continue;
        s2m_item *o = &it[n++];
        snprintf(o->cont, sizeof o->cont, "%s", s2m_obj_cont[k]); snprintf(o->name, sizeof o->name, "%s", s2m_obj_name[k]);
        snprintf(o->lib, sizeof o->lib, "%s", s2m_obj_lib[k]);
        o->x = I32(e + 8); o->y = I32(e + 0xc); o->z = I32(e + 0x10);
        o->a[0] = I32(e + 0x14); o->a[1] = I32(e + 0x18); o->a[2] = I32(e + 0x1c); o->frame = I32(e + 0x20);
    }
    s2m_cam c = { 0, 0, 0, { 0, 0, 0 } };
    if (s2m_camera) { c.a[0] = I32(s2m_camera); c.a[1] = I32(s2m_camera + 4); c.a[2] = I32(s2m_camera + 8); c.x = I32(s2m_camera + 0xc); c.y = I32(s2m_camera + 0x10); c.z = I32(s2m_camera + 0x14); }
    s2m_base(MP(0x1338cc));
    if (getenv("OS_S2MLOG")) {
        static int f;
        if (f++ % 30 == 0 || atoi(getenv("OS_S2MLOG")) == 2) {
            printf("menu: step %.5f row 0x%x ", (double)F32(0x133350), U32(0x1240c0));
            printf("menu: draw %d object(s), camera %d %d %d / %d %d %d:", n, c.x, c.y, c.z, c.a[0], c.a[1], c.a[2]);
            for (int i = 0; i < n && i < 6; i++) printf(" %s/%s at %d %d %d turned %d %d %d frame %d", it[i].cont, it[i].name, it[i].x, it[i].y, it[i].z, it[i].a[0], it[i].a[1], it[i].a[2], it[i].frame);
            printf("\n");
        }
    }
    s2m_draw(it, n, &c, room);
}
#define HOOK_753a0
static uint64_t FUN_000753a0(uint32_t list, uint32_t r1) { (void)r1; s2m_draw_list(list, 0); return 0; }
#define HOOK_753e8
static uint64_t FUN_000753e8(uint32_t list, uint32_t r1) { (void)r1; s2m_draw_list(list, 1); return 0; }   /* (with the room) */
#define HOOK_7f5a2                     /* the frame shown */
static uint64_t FUN_0007f5a2(uint32_t r0, uint32_t r1) { if (!s2m_show() && s2_in_menu) { s2_menu_exit_code = 0; s2m_window_gone = 1; longjmp(s2_menu_out, 1); } return (uint64_t)r1 << 32 | r0; }

/* ---- the keys and the clock ---- */
/* A GAME CONTROLLER is the game's joystick: with one (0x11f298 set; the game asks its setup) the
   menu's frame (0x38d48) has 0x31888 read the first player's six controls (0x127708: entries of
   0x1c bytes -- in use, 0 a button / 1 an axis, the device 0 a joystick / 1 a key, ..., +0x18 the
   button's bit; the values from +0xa8, 0x3ff held) and 0x38b2c takes them as the menu's keys:
   accelerate = ENTER (where the page allows it: "ACCELERATE TO CONTINUE"), brake = ESC, left,
   right, gear up = down, gear down = up.  The port sets the record so (CONTROLS.CFG's joystick
   lines are not read: a controller is the port's own, as in the race) and answers the button
   read 0x4cd69 with the controller's (controls_pad) */
static uint32_t s2m_pad_bits;
static void s2m_pad(void)
{
    int sw[6];
    if (!controls_pad(sw)) { I32(0x11f298) = 0; s2m_pad_bits = 0; return; }
    if (!I32(0x11f298))
        for (uint32_t k = 0; k < 6; k++) {
            const int entry[7] = { 1, 0, 0, 0, 0, 0, 1 << k };
            for (uint32_t j = 0; j < 7; j++) I32(0x127708 + k * 0x1c + j * 4) = entry[j];
        }
    I32(0x11f298) = 1;
    s2m_pad_bits = 0;
    for (int k = 0; k < 6; k++) if (sw[k]) s2m_pad_bits |= 1u << k;
}
#define HOOK_4cca5                     /* the keys down, copied for the frame to 0x100002 + scancode (and the controller) */
static uint64_t FUN_0004cca5(uint32_t r0, uint32_t r1) { const uint8_t *k = s2m_keys(); for (int i = 0; i < 128; i++) U8(0x100002 + (uint32_t)i) = k[i]; s2m_pad(); (void)r1; return r0; }
/* the frame's step (0x4adf8): the time since the last, eased as the race's (S2-5l).  The game
   takes no step under 1/50 s -- no PC of its day drew faster -- and its easing tables have rows
   for 1 to 50 frames a second (0x315a4: row r = (0.5 + i * 0.005) ^ (50 / r)).  The port draws
   the menu as fast as the screen shows it: a shorter step is kept, and its row is made for it
   (the same sum with 50 x the step), so that every movement takes the time it took. */
#define HOOK_4adf8
static uint32_t s2m_ease_row;
static void FUN_0004adf8(void)
{
    const float d = s2m_elapsed();
    F32(0x133354) = d;
    F32(0x133350) = (F32(0x133350) * 9.0f + d) / 10.0f;
    if (F32(0x133350) >= 0.2f) F32(0x133350) = 0.2f;
    if (F32(0x133350) <= 0.001f) F32(0x133350) = 0.001f;
    I32(0x4ce0a) = s2m_hundredths();
    s2m_list_clear();
    if (F32(0x133350) < 0.02f) {
        if (!s2m_ease_row) s2m_ease_row = s2m_alloc(400);
        for (int i = 0; i < 100 && s2m_ease_row; i++)
            F32(s2m_ease_row + 4u * (uint32_t)i) = (float)pow(0.5 + (double)((float)i * 0.005f), 50.0 * (double)F32(0x133350));
        if (s2m_ease_row) U32(0x1240c0) = s2m_ease_row;
    }
}

/* ---- the engine's object list, the menu's share (0x4b843 / 0x4bc94 are s2_hooks.c's, which
   note the name and the container here) ---- */
#define HOOK_4b4d4                     /* (the object's library: the port's meshes have theirs) */
#define FUN_0004b4d4(...) 0
/* an object let go (0x4ba39 -> 0x4b931 / 0x4b9ad): only when it is not in the draw list
   (+0x18 -1, else 0xb); its record given back and its entry cleared, so that its place in the
   list of 200 is free again (s2_hooks.c's 0x4b843 takes the first free one) */
#define HOOK_4ba39
static int FUN_0004ba39(uint32_t list, uint32_t idx)
{
    const uint32_t e = list + (uint32_t)I32(idx) * 0x1c;
    if (getenv("OS_S2MLOG")) printf("menu: let go of object %d (record 0x%x, in the list at %d)\n", I32(idx), U32(e + 0xc), I32(e + 0x18));
    if (!U32(e + 0xc)) return 9;
    if (I32(e + 0x18) != -1) return 0xb;
    s2m_free(U32(e + 0xc));
    U8(e) = 0; U32(e + 0xc) = 0; U32(e + 0x10) = 0; U32(e + 0x14) = 0;
    return 0;
}
/* every object let go (0x4baf6: a race's set-up does it): records given back, the 200 places
   free, none in the draw list */
#define HOOK_4baf6
static void s2m_objects_clear(void)
{
    for (int k = 0; k < 200; k++) {
        const uint32_t e = 0x11da14 + (uint32_t)k * 0x1c;
        if (U32(e + 0xc)) s2m_free(U32(e + 0xc));
        U8(e) = 0; U32(e + 0xc) = 0; U32(e + 0x10) = 0; U32(e + 0x14) = 0; I32(e + 0x18) = -1;
    }
}
#define FUN_0004baf6(...) (s2m_objects_clear(), 0)
#define HOOK_4c1c3
#define FUN_0004c1c3(...) 0
#define HOOK_4c21a
#define FUN_0004c21a(...) 0

/* ---- what is not answered yet ---- */
/* the race (0x2b2a8): not yet from this menu.  What its set-up does to the engine's objects is
   done (0x4baf6), so that the menu finds the list as after a race -- the attract race (0x3c9ac,
   the game's own: after 30 s on the team's page) brings it back to the team's page */
#define HOOK_2b2a8
static int FUN_0003248f(uint32_t name, int to);
static int s2m_resume;                 /* the menu's loop is to go on after its race (tools/s2c_fix.txt @38540) */
static void s2m_keep(void);
/*
 * THE MENU'S RACE (0x2b2a8 after 0x339e7 has made the race's record).  The race is the port's
 * other part, with a fresh block of its own: what the record says goes to it (the settings it
 * starts from, as the plain menu gives them), the menu's block and tables are kept, and the
 * part is left; the menu comes back to its loop just after this call (s2_menu_run).
 */
static void s2m_race(void)
{
    char v[32];
    const uint32_t me = 0x130c58;                        /* (entry 0: the player's -- 0x339e7) */
    snprintf(v, sizeof v, "%d", I32(0x130c4c) + 1); setenv("OS_TRACK", v, 1);
    snprintf(v, sizeof v, "%d", I32(0x130c50)); setenv("OS_COND", v, 1);
    snprintf(v, sizeof v, "%d", I32(0x130d4c)); setenv("OS_DIFF", v, 1);
    snprintf(v, sizeof v, "%d", getenv("OS_S2MTESTLAPS") ? atoi(getenv("OS_S2MTESTLAPS")) : I32(0x130d48)); setenv("OS_LAPS", v, 1);   /* (OS_S2MTESTLAPS: a test) */
    snprintf(v, sizeof v, "%u", s2_seed); setenv("OS_S2SEED", v, 1);   /* (the random numbers as 0x339e7 left them: a league's race) */
    snprintf(v, sizeof v, "%d", U32(0x130d10) & 0xff); setenv("OS_S2GAME", v, 1);
    snprintf(v, sizeof v, "%d", I32(me)); setenv("OS_TEAM", v, 1);
    snprintf(v, sizeof v, "%d", I32(me + 4)); setenv("OS_CAR", v, 1);
    snprintf(v, sizeof v, "%d", I32(me + 0xc)); setenv("OS_S2AUTO", v, 1);
    snprintf(v, sizeof v, "%.3s", (const char *)MP(me + 8)); setenv("OS_S2NAME", v, 1);
    if (getenv("OS_S2MLOG")) printf("menu: the race: track %s, condition %d, level %d, laps %d, mode 0x%x, team %d car %d, automatic %d, %s\n",
                                    getenv("OS_TRACK"), I32(0x130c50), I32(0x130d4c), I32(0x130d48), U32(0x130d10), I32(me), I32(me + 4), I32(me + 0xc), v);
    {   /* the cars as the record has them: count, then team and model each (OS_S2ENTRIES) */
        char e[96]; int at = snprintf(e, sizeof e, "%d", I32(0x130c48));
        for (int i = 0; i < I32(0x130c48) && i < 4; i++)
            at += snprintf(e + at, sizeof e - (size_t)at, " %d %d", I32(0x130c58 + 0x2c * (uint32_t)i), I32(0x130c58 + 0x2c * (uint32_t)i + 4));
        setenv("OS_S2ENTRIES", e, 1);
        if (getenv("OS_S2MLOG")) printf("menu: the race's cars: %s; damage %d, 0x130d64 %d, detail %d, 0x130c64 %d\n", e, I32(0x130d54), I32(0x130d64), I32(0x130c54), I32(0x130c64));
    }
    FUN_000331dc();                                       /* (the port's: the settings file written before every race,
                                                              not only as the menu ends -- a window closed in a race
                                                              loses nothing) */
    s2m_keep();
    snd_close();                                          /* (the race opens the sound again) */
    s2m_race_go(I32(0x130c4c) + 1);                       /* (does not come back) */
}
int s2_rpl_load(const char *name);
static uint64_t s2m_race_standin(void)
{
    /* VIEW REPLAY (0x1324dc -1, the name in 0x130d34): the race's load reads the file (0x2e1f3 ->
       0x327ed) -- here, before the race part is given the record; no file: no race */
    if (s2_in_menu && I32(0x1324dc) < 0 && !(U32(0x130d10) & 0x80) && U8(0x130d34)) {
        char name[16];
        snprintf(name, sizeof name, "%.12s", (const char *)MP(0x130d34));
        if (s2_rpl_load(name)) s2m_race();
    }
    if (s2_in_menu && ((U32(0x130d10) & 0x80) || I32(0x1324dc) >= 0)) s2m_race();   /* (the attract race too, 0x3c9ac) */
    s2m_objects_clear();
    /* (the race's set-up reads its texts, GAME.TXT, and from the unit's word whether speeds
       are in miles, 0x124654: the car's page shows them so -- 0x2ef4e) */
    s2_str(S2_TMP + 0x40, "game");
    U32(0x124650) = (uint32_t)FUN_0003248f(S2_TMP + 0x40, 0x124588);
    const uint32_t unit = U32(0x1245f8);
    I32(0x124654) = unit && (U8(unit) == 'm' || U8(unit) == 'M');
    return 0;
}
#define FUN_0002b2a8(...) s2m_race_standin()
#define HOOK_3162c                     /* is the CD there (0 yes): the port has the game's files */
#define FUN_0003162c(...) 0
#define HOOK_30e75                     /* the game's "cannot go on" */
static void FUN_00030e75(void) { fprintf(stderr, "menu: the game stopped (0x30e75)\n"); FUN_000ae662(1); }
#define HOOK_4b169                     /* back to the text screen */
#define FUN_0004b169(...) 0
#define HOOK_4b1a4                     /* the error's message */
static int FUN_0004b1a4(int code) { fprintf(stderr, "menu: the game's error %d\n", code); return 0; }
#define HOOK_4cd69                     /* the joystick's buttons: the controller's six controls (s2m_pad) */
#define FUN_0004cd69(...) ((uint64_t)s2m_pad_bits)
#define HOOK_4cd77
#define FUN_0004cd77(...) 0
#define HOOK_4ccfd
#define FUN_0004ccfd(...) 0
#define HOOK_4cbf0
#define FUN_0004cbf0(...) 0
#define HOOK_4cfe0
#define FUN_0004cfe0(...) 0
#define HOOK_4d064                     /* the CD's music: not answered yet */
#define FUN_0004d064(...) 0
#define HOOK_4d0f4
#define FUN_0004d0f4(...) 0
/* the CD's music (the DOS CD driver's requests), answered by files: track_NN.ogg / .mp3 / .wav
   (cdaudio.c).  0x4d5b4(track, again) plays the track -- `again`: 0x4d630, asked every 0.4 s,
   starts it once more as it ends; the port's player loops it itself; 0x4d738 stops it;
   0x4d7c0(left, right) the CD's volume, 0..255 a side (the settings' 0x128a5c >> 7) */
static void s2m_sound_device(void) { if (!snd_open()) snd_init("build/dec/SOUND", "build/dec/SOUND/none"); }
#define HOOK_4d5b4
static uint64_t FUN_0004d5b4(int track, int again)
{
    if (getenv("OS_NOMUSIC") || getenv("OS_NOSOUND")) return 1;
    s2m_sound_device();
    if (snd_cd_track() == track) return 0;     /* (the drive told to play what it is playing goes on) */
    return snd_cd(track, again != 0) ? 0 : 1;
}
#define HOOK_4d630
#define FUN_0004d630(...) ((uint64_t)snd_cd_track())
#define HOOK_4d738
#define FUN_0004d738(...) (snd_cd(0, 0), 0)
#define HOOK_4d7c0
static uint64_t FUN_0004d7c0(int left, int right)
{
    if (getenv("OS_SNDLOG")) printf("menu: the CD's volume %d %d\n", left, right);
    snd_cd_volume((float)(left > right ? left : right) / 255.0f);
    return 0;
}

/* the attract race (0x3c9ac) over, the rest of its page: the next of its six tracks (0x128a58,
   kept in CHOICE.DAT), TEAM SELECTION next, ATTRACT.TXT let go, and the menu's song (track 2, if
   MUSIC is on) at the music volume */
static void s2m_attract_done(void)
{
    if (++I32(0x128a58) > 5) I32(0x128a58) = 0;
    I32(0x132494) = 0x3ff37;
    FUN_0004f331(U32(0x11d6ec), 0x1286a8);
    if (I32(0x4d900) == 0) {
        FUN_0004d7c0(I32(0x130d5c) >> 7, I32(0x130d5c) >> 7);
        if (I32(0x128a44) == 0) { FUN_0004d738(); FUN_0004d5b4(2, 1); }
    }
    if (getenv("OS_S2MLOG")) printf("menu: the attract race over, the next on track %d\n", I32(0x113224 + 4 * (uint32_t)I32(0x128a58)) + 1);
}
/* the menu's sounds, by the same manager as the race's (s2_hooks.c): 0xafc70(m, container,
   name) reads "<name>.dtr" of "<container>.dat" (the page loader's item 12: MENU.DTR of SOUND,
   15 samples) and gives how many; 0xaeef8(m) stops the first 32 slots; 0xaf120(m) stops them and
   lets the samples go (the page loader as the item is no longer wanted, 0x31495 after a race);
   0xaec90(m, to, step) eases the manager's volume towards `to` by `step` (the fade as a page is
   left for a race) */
#define HOOK_afc70
static uint32_t FUN_000afc70(uint32_t m, uint32_t cont, uint32_t name)
{
    (void)m;
    char c[16], n[16], file[64];
    s2m_cont_of(cont, c, sizeof c);
    snprintf(n, sizeof n, "%s", (const char *)MP(name));
    for (char *q = n; *q; q++) *q = (char)toupper((unsigned char)*q);
    snprintf(file, sizeof file, "build/dec/%s/%s.DTR", c, n);
    for (int i = 0; i < S2_SAMPLES; i++) s2_sample[i].id = -1;
    for (int i = 0; i < S2_SLOTS; i++) { s2_slot[i].handle = -1; s2_slot[i].sample = -1; s2_slot[i].last = -1.0f; }
    if (snd_open()) snd_forget(); else snd_init("build/dec/SOUND", "build/dec/SOUND/none");
    s2_sound_table(file);
    s2_sound_on = !getenv("OS_NOSOUND");
    if (getenv("OS_S2SNDGAIN")) s2_sound_gain = (float)atof(getenv("OS_S2SNDGAIN"));
    int count = 0;
    for (int i = 0; i < S2_SAMPLES; i++) count += s2_sample[i].id >= 0;
    if (getenv("OS_S2MLOG")) printf("menu: sounds %s: %d samples\n", file, count);
    return (uint32_t)count;
}
#define HOOK_aeef8
static uint32_t FUN_000aeef8(uint32_t m, ...) { for (int i = 0; i < 0x20; i++) FUN_000aee98(m, i); return 0; }
#define HOOK_af120
static uint32_t FUN_000af120(uint32_t m, ...)
{
    if (!s2_sound_on) return 0;
    FUN_000aeef8(m);
    s2_sound_on = 0;
    for (int i = 0; i < S2_SAMPLES; i++) s2_sample[i].id = -1;
    snd_forget();                                         /* (the device stays: the CD's song is another thing) */
    if (getenv("OS_S2MLOG")) printf("menu: sounds let go\n");
    return 0;
}
#define HOOK_aec90
static uint32_t FUN_000aec90(uint32_t m, uint32_t to, int step)
{
    (void)m;
    int v = s2_master;
    if ((uint32_t)v < to) { v += step; if ((uint32_t)v > to) v = (int)to; }
    else if ((uint32_t)v > to) { v -= step; if ((uint32_t)v < to || v > 0x7fff) v = (int)to; }
    else return 0;
    s2_master_set(v);
    return 0;
}
#define HOOK_50165                     /* a material library of the container let go (by name): the port keeps
                                          its libraries; only the reflections' map stops being one */
static uint64_t FUN_00050165(uint32_t name, ...) { char n[16]; if (s2_in_menu) { s2m_cont_of(name, n, sizeof n); s2m_materials_go(s2m_cont, n); } return 0; }
#define HOOK_50247                     /* every library let go: the same */
#define FUN_00050247(...) (s2_in_menu ? s2m_materials_all_go() : (void)0, 0)
#define HOOK_502d4                     /* the room let go */
#define FUN_000502d4(...) (s2m_scene_free(), 0)
#define HOOK_504f2                     /* the see-through table from the palette (the menu's rooms: no smoke colours, so
                                          only the shadow's 16 rows to black) */
static uint64_t FUN_000504f2(uint32_t pal, int r, int g, int b, int r2, int g2, int b2)
{
    if (s2_in_menu) { const int smoke[3] = { r, g, b }, dust[3] = { r2, g2, b2 }; s2m_base(MP(pal)); s2m_blend(smoke, dust); }
    return 0;
}
#define HOOK_7e428
#define FUN_0007e428(...) 0
#define HOOK_4abf5
#define FUN_0004abf5(...) 0
#define HOOK_4ac32
#define FUN_0004ac32(...) 0
#define HOOK_4ee3a_done
#define HOOK_ae88d
#define FUN_000ae88d(...) 0
#define HOOK_af1b0
#define HOOK_4fca9                     /* a room of the container ("map1": MAP1.MMP / .UNR) */
static uint32_t FUN_0004fca9(uint32_t name, ...)
{
    char n[16];
    s2m_cont_of(name, n, sizeof n);
    s2m_scene_load(s2m_cont, n);
    return 0;
}
/* the library's files: fopen, fread, fwrite, fclose.  The game opens one file through them,
   "\config\choice.dat" -- the settings, the championship, the best times (0x32d53 reads it as
   the menu starts, 0x331dc writes it as the menu ends; each word i stored as (v + i*0x1050c) ^ i).
   The port keeps it in its home as openscreamer.s2choice, the game's bytes as they are; any other
   name is refused (said once), which the game takes as "no file". */
#define S2M_CFILES 4
static FILE *s2m_cfile[S2M_CFILES];
#define HOOK_b6aac
static uint32_t FUN_000b6aac(uint32_t name, uint32_t mode)
{
    const char *n = (const char *)MP(name), *b = n, *m = (const char *)MP(mode);
    for (const char *c = n; *c; c++) if (*c == '\\' || *c == '/' || *c == ':') b = c + 1;
    if (strcasecmp(b, "choice.dat") != 0) {
        fprintf(stderr, "screamer 2: the menu opened \"%s\", which the port does not keep\n", n);
        return 0;
    }
    for (int k = 0; k < S2M_CFILES; k++)
        if (!s2m_cfile[k]) {
            s2m_cfile[k] = os_fopen("openscreamer.s2choice", m);
            return s2m_cfile[k] ? (uint32_t)k + 1 : 0;
        }
    return 0;
}
static FILE *s2m_cfileof(uint32_t f) { return f >= 1 && f <= S2M_CFILES ? s2m_cfile[f - 1] : NULL; }
#define HOOK_b6b60
static uint32_t FUN_000b6b60(uint32_t buf, uint32_t size, uint32_t n, uint32_t f)
{
    FILE *h = s2m_cfileof(f);
    return h ? (uint32_t)fread(MP(buf), size, n, h) : 0;
}
#define HOOK_b6e97
static uint32_t FUN_000b6e97(uint32_t buf, uint32_t size, uint32_t n, uint32_t f)
{
    FILE *h = s2m_cfileof(f);
    return h ? (uint32_t)fwrite(MP(buf), size, n, h) : 0;
}
#define HOOK_b6d3b
static uint32_t FUN_000b6d3b(uint32_t f)
{
    FILE *h = s2m_cfileof(f);
    if (!h) return (uint32_t)-1;
    s2m_cfile[f - 1] = NULL;
    return fclose(h) == 0 ? 0 : (uint32_t)-1;
}
#define HOOK_b73c7                     /* abs */
static uint32_t FUN_000b73c7(uint32_t v) { return (int32_t)v < 0 ? -v : v; }
#define HOOK_b743d                     /* itoa (the value, the buffer, the radix; Watcom's small letters): the buffer */
static uint32_t FUN_000b743d(uint32_t v, uint32_t buf, uint32_t radix)
{
    char t[40], *o = (char *)MP(buf);
    uint32_t u = v;
    int k = 0;
    if (radix == 10 && (int32_t)v < 0) { *o++ = '-'; u = -v; }
    if (radix < 2 || radix > 36) radix = 10;
    do { t[k++] = "0123456789abcdefghijklmnopqrstuvwxyz"[u % radix]; u /= radix; } while (u);
    while (k) *o++ = t[--k];
    *o = 0;
    return buf;
}
#define HOOK_b48eb
#define FUN_000b48eb(...) 0
#define HOOK_b4980
#define FUN_000b4980(...) 0
#define HOOK_b4c46
#define FUN_000b4c46(...) 0
#define HOOK_b4d73
#define FUN_000b4d73(...) 0
#define HOOK_b52f1
#define FUN_000b52f1(...) 0
#define HOOK_b5383
#define FUN_000b5383(...) 0
#define HOOK_b5727
#define FUN_000b5727(...) 0
#define HOOK_b57e9
#define FUN_000b57e9(...) 0
#define HOOK_b5827
#define FUN_000b5827(...) 0
#define HOOK_b58b6
#define FUN_000b58b6(...) 0
#define HOOK_b5ae0
#define FUN_000b5ae0(...) 0
#define HOOK_b5b0b
#define FUN_000b5b0b(...) 0
#define HOOK_b5b11
#define FUN_000b5b11(...) 0
#define HOOK_b5b17
#define FUN_000b5b17(...) 0
#define HOOK_b5bc4
#define FUN_000b5bc4(...) 0
#define HOOK_b5bdf
#define FUN_000b5bdf(...) 0
#define HOOK_b5c63
#define FUN_000b5c63(...) 0
#define HOOK_b5ca0
#define FUN_000b5ca0(...) 0
#define HOOK_b5d84
#define FUN_000b5d84(...) 0
#define HOOK_b5e08
#define FUN_000b5e08(...) 0
#define HOOK_b5fa4
#define FUN_000b5fa4(...) 0
#define HOOK_b66e0
#define FUN_000b66e0(...) 0
#define HOOK_b68c5
#define FUN_000b68c5(...) 0
#define HOOK_b7356
#define FUN_000b7356(...) 0
#define HOOK_b7374
#define FUN_000b7374(...) 0

/* a call through an address the converted routines do not have (s2_dispatch's default): said,
   and the menu's page set to none, which ends its loop */
static void s2_dispatch_other(uint32_t at)
{
    fprintf(stderr, "screamer 2: the menu called 0x%x, which is not converted\n", at);
    *(uint32_t *)MP(0x132494u) = 0;
}
