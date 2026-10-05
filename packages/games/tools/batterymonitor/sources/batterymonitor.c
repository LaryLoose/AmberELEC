/*
 * batterymonitor - SDL2 battery monitor (Android-style)
 *
 * Reads the per-day logs written by the `battery` logger in
 * /storage/.config/battery/battery-YYYY-MM-DD.log (lines of the form
 * "YYYY-MM-DD HH:MM:SS level=<N> status=<Charging|Discharging|Full>" and
 * "YYYY-MM-DD HH:MM:SS Shutdown") and draws the charge level over a
 * *time-proportional* X axis, so plateaus and fast drains are visible like in
 * the Android battery screen. Each segment is colored by its slope (level rose
 * or stayed flat -> charging/green, fell -> discharging/blue). Periods without
 * data (device in standby / off) are linearly interpolated between the two
 * surrounding samples.
 *
 * Controls (quit via SELECT+START is handled by jslisten, not the tool):
 *   LEFT / RIGHT   - pan through time (hold to scroll)
 *   L1 / DOWN      - zoom out (longer time window)
 *   R1 / UP        - zoom in  (shorter time window)
 *   A              - jump to latest / live
 *   B              - jump to oldest
 *
 * Options:
 *   --days N   - load only the last N days of logs (default 7, <=0 = all)
 *   --shot F   - render one frame to BMP file F and exit
 *
 * Build:
 *   gcc batterymonitor.c -o batterymonitor $(pkg-config --cflags --libs sdl2) -lSDL2_gfx -lm
 */

#define _GNU_SOURCE
#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <dirent.h>

#define WIN_W 640
#define WIN_H 480

#define LOG_DIR "/storage/.config/battery"

/* A single parsed data point. */
typedef struct {
    time_t t;
    short  level;     /* 0..100 */
    unsigned char charging; /* logged status: 1 = Charging or Full, 0 = Discharging */
    unsigned char full;     /* 1 = status Full */
} Sample;

/* Reload the logs from disk every so often so a running charge/discharge shows up. */
#define RELOAD_MS 30000

/* Idle loop sleep: coarse poll so the CPU can sleep instead of 1 ms busy-waiting. */
#define IDLE_POLL_MS 40

/* Pan speed as a fraction of the visible time window per second (dt-based). */
#define PAN_VEL 0.7

/* Right-stick cursor stepping: analog deflection sets the auto-repeat rate. */
#define RS_DEAD     2200
#define RS_DELAY     350   /* ms before auto-repeat starts (key-repeat style) */
#define RS_MIN_SPS   4.0f  /* steps/sec just past the deadzone */
#define RS_MAX_SPS  90.0f  /* steps/sec at full deflection */

/* ---- layout (logical 640x480) ---- */
#define PLOT_L 44
#define PLOT_R 628
#define PLOT_T 70
#define PLOT_B 406

/* ---- colors ---- */
static const SDL_Color COL_BG       = {  18,  20,  26, 255 };
static const SDL_Color COL_PANEL    = {  28,  31,  40, 255 };
static const SDL_Color COL_GRID     = {  48,  52,  64, 255 };
static const SDL_Color COL_GRID_HL  = {  74,  80,  96, 255 };
static const SDL_Color COL_TEXT     = { 222, 226, 234, 255 };
static const SDL_Color COL_DIM      = { 140, 146, 160, 255 };
static const SDL_Color COL_DISCHARGE= {  92, 180, 245, 255 };
static const SDL_Color COL_CHARGE   = {  80, 205, 130, 255 };
static const SDL_Color COL_LOW      = { 235,  86,  80, 255 };

/* ---- parsed data ---- */
static Sample *g_samples = NULL;
static int     g_count   = 0;
static int     g_cap     = 0;
static time_t  g_tmin    = 0;
static time_t  g_tmax    = 0;
static double  g_total   = 3600.0;
static char    g_last_name[64]; /* newest log file consumed (for incremental reload) */
static long    g_last_off;      /* bytes already parsed from g_last_name */
static Sint16 *g_vx = NULL, *g_vy = NULL; /* reusable fill-polygon buffers */
static int     g_vcap = 0;

/* ---- view state ---- */
static const long ZOOM_SPANS[] = { 3*3600, 6*3600, 12*3600, 24*3600, 48*3600, 7*86400 };
#define NZOOM ((int)(sizeof(ZOOM_SPANS)/sizeof(ZOOM_SPANS[0])))
static int    g_zoom      = 3;   /* index into ZOOM_SPANS, or NZOOM for "All" */
static double g_view_end  = 0;   /* right edge of the window (epoch seconds) */
static int    g_follow    = 1;   /* stick to the latest sample */
static int    g_no_present = 0;  /* screenshot mode: skip the swap so readback is valid */
static int    g_cursor    = -1;  /* interactive cursor: sample index, -1 = off */
static int    g_days      = 7;   /* days of history to load back (<=0 = all) */
static char   g_cutoff[24];      /* oldest allowed "battery-YYYY-MM-DD.log", "" = no limit */

/* ------------------------------------------------------------------ */
/* text (SDL2_ttf / DejaVu, bitmap fallback, small texture cache)      */
/* ------------------------------------------------------------------ */
#define FONT_MAX_SIZES 16
static TTF_Font *g_fonts[FONT_MAX_SIZES];
static int       g_font_px[FONT_MAX_SIZES];
static int       g_font_n;
static char      g_font_path[512];
static int       g_have_ttf;

static void font_init(void)
{
    if (TTF_Init() != 0) return;
    const char *cands[] = {
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/storage/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
        NULL
    };
    for (int i = 0; cands[i]; i++) {
        FILE *f = fopen(cands[i], "rb");
        if (f) { fclose(f); strncpy(g_font_path, cands[i], sizeof(g_font_path) - 1); g_have_ttf = 1; break; }
    }
}

static int scale_px(float scale)
{
    int px = (int)lroundf(scale * 12.0f);
    return px < 6 ? 6 : px;
}

static TTF_Font *get_font(int px)
{
    if (!g_have_ttf) return NULL;
    for (int i = 0; i < g_font_n; i++) if (g_font_px[i] == px) return g_fonts[i];
    if (g_font_n >= FONT_MAX_SIZES) return g_fonts[0];
    TTF_Font *f = TTF_OpenFont(g_font_path, px);
    if (!f) return NULL;
    g_fonts[g_font_n] = f;
    g_font_px[g_font_n] = px;
    g_font_n++;
    return f;
}

static int text_h(float scale)
{
    TTF_Font *f = get_font(scale_px(scale));
    return f ? TTF_FontHeight(f) : (int)(8 * scale);
}

/* Cache rendered strings so panning doesn't re-rasterize every frame. */
typedef struct { char str[64]; int px; Uint8 r, g, b; SDL_Texture *tex; int w, h; Uint32 used; } TextCache;
#define TC_N 64
static TextCache g_tc[TC_N];
static Uint32    g_tc_clock;

static SDL_Texture *text_tex(SDL_Renderer *rnd, const char *s, int px, SDL_Color c, int *w, int *h)
{
    for (int i = 0; i < TC_N; i++) {
        if (g_tc[i].tex && g_tc[i].px == px && g_tc[i].r == c.r &&
            g_tc[i].g == c.g && g_tc[i].b == c.b && strcmp(g_tc[i].str, s) == 0) {
            g_tc[i].used = ++g_tc_clock;
            *w = g_tc[i].w; *h = g_tc[i].h;
            return g_tc[i].tex;
        }
    }
    TTF_Font *f = get_font(px);
    if (!f) return NULL;
    SDL_Surface *surf = TTF_RenderUTF8_Blended(f, s, c);
    if (!surf) return NULL;
    SDL_Texture *t = SDL_CreateTextureFromSurface(rnd, surf);
    int sw = surf->w, sh = surf->h;
    SDL_FreeSurface(surf);
    if (!t) return NULL;

    int slot = 0; Uint32 oldest = 0xffffffffu;
    for (int i = 0; i < TC_N; i++) {
        if (!g_tc[i].tex) { slot = i; break; }
        if (g_tc[i].used < oldest) { oldest = g_tc[i].used; slot = i; }
    }
    if (g_tc[slot].tex) SDL_DestroyTexture(g_tc[slot].tex);
    size_t n = strlen(s);
    if (n >= sizeof(g_tc[slot].str)) n = sizeof(g_tc[slot].str) - 1;
    memcpy(g_tc[slot].str, s, n);
    g_tc[slot].str[n] = '\0';
    g_tc[slot].px = px; g_tc[slot].r = c.r; g_tc[slot].g = c.g; g_tc[slot].b = c.b;
    g_tc[slot].tex = t; g_tc[slot].w = sw; g_tc[slot].h = sh; g_tc[slot].used = ++g_tc_clock;
    *w = sw; *h = sh;
    return t;
}

static void draw_text(SDL_Renderer *r, int x, int y, float scale, SDL_Color c,
                      const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (g_have_ttf) {
        int w, h;
        SDL_Texture *t = text_tex(r, buf, scale_px(scale), c, &w, &h);
        if (t) {
            SDL_Rect dst = { x, y, w, h };
            SDL_RenderCopy(r, t, NULL, &dst);
            return;
        }
    }
    /* fallback: SDL2_gfx 8px bitmap font */
    float sx, sy;
    SDL_RenderGetScale(r, &sx, &sy);
    SDL_RenderSetScale(r, sx * scale, sy * scale);
    stringRGBA(r, (Sint16)(x / scale), (Sint16)(y / scale), buf, c.r, c.g, c.b, c.a);
    SDL_RenderSetScale(r, sx, sy);
}

static int text_w(const char *s, float scale)
{
    TTF_Font *f = get_font(scale_px(scale));
    if (f) { int w = 0, h = 0; TTF_SizeUTF8(f, s, &w, &h); return w; }
    return (int)(strlen(s) * 8 * scale);
}

static void draw_text_right(SDL_Renderer *r, int xr, int y, float scale, SDL_Color c,
                            const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    draw_text(r, xr - text_w(buf, scale), y, scale, c, "%s", buf);
}

static void draw_text_center(SDL_Renderer *r, int cx, int y, float scale, SDL_Color c,
                             const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    draw_text(r, cx - text_w(buf, scale) / 2, y, scale, c, "%s", buf);
}

/* ------------------------------------------------------------------ */
/* log parsing                                                         */
/* ------------------------------------------------------------------ */
static void push_sample(time_t t, int level, int charging, int full)
{
    if (g_count >= g_cap) {
        int ncap = g_cap ? g_cap * 2 : 1024;
        Sample *n = realloc(g_samples, (size_t)ncap * sizeof(Sample));
        if (!n) return;
        g_samples = n;
        g_cap = ncap;
    }
    Sample *s = &g_samples[g_count++];
    s->t = t;
    s->level = (short)level;
    s->charging = (unsigned char)charging;
    s->full = (unsigned char)full;
}

/* Build the oldest allowed log filename for the current --days window. */
static void compute_cutoff(void)
{
    if (g_days <= 0) { g_cutoff[0] = '\0'; return; }
    time_t ct = time(NULL) - (time_t)(g_days - 1) * 86400;
    struct tm tm;
    localtime_r(&ct, &tm);
    strftime(g_cutoff, sizeof(g_cutoff), "battery-%Y-%m-%d.log", &tm);
}

static int name_filter(const struct dirent *e)
{
    if (strncmp(e->d_name, "battery-", 8) != 0 || strstr(e->d_name, ".log") == NULL)
        return 0;
    if (g_cutoff[0] && strcmp(e->d_name, g_cutoff) < 0) return 0;  /* older than the window */
    return 1;
}

/* Parse a log file starting at byte offset; appends samples, returns new EOF offset. */
static long parse_file_from(const char *path, long offset)
{
    FILE *fp = fopen(path, "r");
    if (!fp) return offset;
    if (offset > 0) fseek(fp, offset, SEEK_SET);
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        struct tm tm;
        memset(&tm, 0, sizeof(tm));
        if (strptime(line, "%Y-%m-%d %H:%M:%S", &tm) == NULL) continue;
        tm.tm_isdst = -1;
        time_t t = mktime(&tm);
        if (t == (time_t)-1) continue;

        const char *rest = line + 19;
        while (*rest == ' ') rest++;
        if (strncmp(rest, "level=", 6) != 0) continue;  /* skip Shutdown and others */

        int lvl = 0;
        char st[32] = {0};
        if (sscanf(rest, "level=%d status=%31s", &lvl, st) < 1) continue;
        if (lvl < 0) lvl = 0;
        if (lvl > 100) lvl = 100;
        int charging = (strcmp(st, "Charging") == 0 || strcmp(st, "Full") == 0);
        int full = (strcmp(st, "Full") == 0);
        push_sample(t, lvl, charging, full);
    }
    long end = ftell(fp);
    fclose(fp);
    return end;
}

/* Recompute time bounds + default view after the sample array changed. */
static void compute_bounds(void)
{
    if (g_count > 0) {
        g_tmin = g_samples[0].t;
        g_tmax = g_samples[g_count - 1].t;
        if (g_tmax <= g_tmin) g_tmax = g_tmin + 3600;
        g_total = (double)(g_tmax - g_tmin);
        if (g_total < 1.0) g_total = 3600.0;
    } else {
        g_tmin = g_tmax = time(NULL);
        g_total = 3600.0;
    }

    /* pick a sensible default zoom the first time */
    if (g_view_end == 0) {
        g_zoom = (g_total > 24 * 3600) ? 3 : NZOOM;
        g_view_end = (double)g_tmax;
        g_follow = 1;
    }
    if (g_follow) g_view_end = (double)g_tmax;
    if (g_cursor >= g_count) g_cursor = g_count - 1;
}

static void load_logs(void)
{
    free(g_samples);
    g_samples = NULL;
    g_count = 0;
    g_cap = 0;
    g_last_name[0] = '\0';
    g_last_off = 0;

    compute_cutoff();
    struct dirent **list = NULL;
    int n = scandir(LOG_DIR, &list, name_filter, alphasort);
    if (n > 0) {
        for (int i = 0; i < n; i++) {
            char path[512];
            snprintf(path, sizeof(path), "%s/%s", LOG_DIR, list[i]->d_name);
            g_last_off = parse_file_from(path, 0);
            strncpy(g_last_name, list[i]->d_name, sizeof(g_last_name) - 1);
            g_last_name[sizeof(g_last_name) - 1] = '\0';
            free(list[i]);
        }
        free(list);
    }
    compute_bounds();
}

/* Incremental reload: parse only appended/new file content. Returns 1 if data grew. */
static int reload_logs(void)
{
    compute_cutoff();
    struct dirent **list = NULL;
    int n = scandir(LOG_DIR, &list, name_filter, alphasort);
    if (n <= 0) { if (list) free(list); return 0; }
    int before = g_count;
    for (int i = 0; i < n; i++) {
        const char *nm = list[i]->d_name;
        int cmp = g_last_name[0] ? strcmp(nm, g_last_name) : 1;
        if (cmp >= 0) {   /* current file (append) or a newer file (full) */
            char path[512];
            snprintf(path, sizeof(path), "%s/%s", LOG_DIR, nm);
            g_last_off = parse_file_from(path, (cmp == 0) ? g_last_off : 0);
            strncpy(g_last_name, nm, sizeof(g_last_name) - 1);
            g_last_name[sizeof(g_last_name) - 1] = '\0';
        }
        free(list[i]);
    }
    free(list);
    if (g_count != before) { compute_bounds(); return 1; }
    return 0;
}

/* effective span (seconds) for the current zoom level */
static double current_span(void)
{
    double span = (g_zoom >= NZOOM) ? g_total : (double)ZOOM_SPANS[g_zoom];
    if (span > g_total) span = g_total;
    if (span < 600.0) span = 600.0;
    return span;
}

static void clamp_view(void)
{
    double span = current_span();
    if (g_follow) {
        g_view_end = (double)g_tmax;
        return;
    }
    double lo = (double)g_tmin + span;   /* window must contain data */
    double hi = (double)g_tmax;
    if (lo > hi) lo = hi;
    if (g_view_end > hi) { g_view_end = hi; g_follow = 1; }
    if (g_view_end < lo) g_view_end = lo;
}

/* Scroll the window so the cursor sample stays visible. */
static void ensure_cursor_visible(void)
{
    if (g_cursor < 0 || g_count == 0) return;
    double span = current_span();
    double ct = (double)g_samples[g_cursor].t;
    double margin = span * 0.08;
    if (ct > g_view_end - margin)                 g_view_end = ct + margin;
    else if (ct < g_view_end - span + margin)     g_view_end = ct - margin + span;
    g_follow = 0;
    clamp_view();
}

/* Move the cursor to the prev/next actual sample (activating it on first use). */
static void cursor_step(int dir)
{
    if (g_count == 0) return;
    if (g_cursor < 0) {
        int idx = g_count - 1;
        while (idx > 0 && (double)g_samples[idx].t > g_view_end) idx--;
        g_cursor = idx;
        g_follow = 0;
    }
    g_cursor += dir;
    if (g_cursor < 0) g_cursor = 0;
    if (g_cursor > g_count - 1) g_cursor = g_count - 1;
    ensure_cursor_visible();
}

/* ------------------------------------------------------------------ */
/* coordinate mapping                                                  */
/* ------------------------------------------------------------------ */
static double time_to_x(double t, double vs, double span)
{
    return PLOT_L + (t - vs) / span * (double)(PLOT_R - PLOT_L);
}

static double level_to_y(double lvl)
{
    return PLOT_B - (lvl / 100.0) * (double)(PLOT_B - PLOT_T);
}

/* Charging is inferred from the slope: the later sample is at or above the
 * earlier one -> charging, otherwise discharging. */
static int seg_charging(const Sample *a, const Sample *b)
{
    return b->level >= a->level;
}

/* ------------------------------------------------------------------ */
/* drawing                                                             */
/* ------------------------------------------------------------------ */
static void draw_battery_icon(SDL_Renderer *r, int x, int y, int w, int h,
                              int level, SDL_Color fill, int charging)
{
    int nub = 3;
    rectangleRGBA(r, x, y, x + w, y + h, COL_TEXT.r, COL_TEXT.g, COL_TEXT.b, 255);
    boxRGBA(r, x + w + 1, y + h / 2 - 4, x + w + nub, y + h / 2 + 4,
            COL_TEXT.r, COL_TEXT.g, COL_TEXT.b, 255);
    int inner_w = w - 4;
    int fillw = (int)(inner_w * (level / 100.0) + 0.5);
    if (fillw < 1 && level > 0) fillw = 1;
    if (fillw > 0)
        boxRGBA(r, x + 2, y + 2, x + 2 + fillw, y + h - 2, fill.r, fill.g, fill.b, 255);

    if (charging) {   /* lightning bolt overlay = charging */
        int cx = x + w / 2;
        int cy = y + h / 2;
        int bh = (h - 4) / 2;          /* bolt half-height, fits inside the body */
        int a = bh / 3, b = bh / 2;
        Sint16 bx[6] = { (Sint16)(cx + a), (Sint16)(cx - b), (Sint16)(cx - 1),
                         (Sint16)(cx - a), (Sint16)(cx + b), (Sint16)(cx + 1) };
        Sint16 by[6] = { (Sint16)(cy - bh), (Sint16)(cy + 1), (Sint16)(cy + 1),
                         (Sint16)(cy + bh), (Sint16)(cy - 1), (Sint16)(cy - 1) };
        filledPolygonRGBA(r, bx, by, 6, 255, 255, 255, 255);
        polygonRGBA(r, bx, by, 6, COL_BG.r, COL_BG.g, COL_BG.b, 255);
    }
}

static void draw_header(SDL_Renderer *r)
{
    const Sample *last = (g_count > 0) ? &g_samples[g_count - 1] : NULL;
    int level = last ? last->level : 0;
    int charging = last ? last->charging : 0;   /* parse folds Full into charging */
    SDL_Color icon = COL_TEXT;
    if (charging)                 icon = COL_CHARGE;
    else if (last && level <= 15) icon = COL_LOW;

    float hdr = 1.6f;                 /* % and title share this size */
    int ty = 8;
    /* Size the battery body to the visible digit height, not the full line box. */
    TTF_Font *hf = get_font(scale_px(hdr));
    int ih = text_h(hdr), gtop = 0;
    if (hf) {
        int mnx, mxx, mny, mxy, adv;
        if (TTF_GlyphMetrics(hf, '0', &mnx, &mxx, &mny, &mxy, &adv) == 0) {
            ih = mxy - mny;
            gtop = TTF_FontAscent(hf) - mxy;   /* digit top offset below draw-y */
        }
    }
    int iw = (ih * 19) / 10;          /* ~1.9:1 battery body */
    draw_battery_icon(r, 12, ty + gtop, iw, ih, level, icon, charging);
    draw_text(r, 12 + iw + 16, ty, hdr, COL_TEXT, "%d%%", level);

    draw_text_right(r, PLOT_R, ty, hdr, COL_TEXT, "BATTERY MONITOR");

    /* window range + zoom label */
    char span_lbl[32];
    if (g_zoom >= NZOOM) snprintf(span_lbl, sizeof(span_lbl), "All");
    else if (ZOOM_SPANS[g_zoom] < 86400) snprintf(span_lbl, sizeof(span_lbl), "%ldh", ZOOM_SPANS[g_zoom] / 3600);
    else snprintf(span_lbl, sizeof(span_lbl), "%ldd", ZOOM_SPANS[g_zoom] / 86400);

    double span = current_span();
    time_t vs = (time_t)(g_view_end - span);
    time_t ve = (time_t)g_view_end;
    struct tm a, b;
    localtime_r(&vs, &a);
    localtime_r(&ve, &b);
    char sa[24], sb[24];
    strftime(sa, sizeof(sa), "%b %d %H:%M", &a);
    strftime(sb, sizeof(sb), "%b %d %H:%M", &b);
    draw_text_right(r, PLOT_R, 42, 1.3f, COL_DIM, "Zoom %s   %s - %s%s",
                    span_lbl, sa, sb, (g_follow && g_cursor < 0) ? "  (live)" : "");
}

static void draw_grid(SDL_Renderer *r, double vs, double span)
{
    /* panel */
    boxRGBA(r, PLOT_L, PLOT_T, PLOT_R, PLOT_B, COL_PANEL.r, COL_PANEL.g, COL_PANEL.b, 255);

    /* horizontal gridlines 0/25/50/75/100 */
    for (int p = 0; p <= 100; p += 25) {
        int y = (int)(level_to_y(p) + 0.5);
        hlineRGBA(r, PLOT_L, PLOT_R, y, COL_GRID.r, COL_GRID.g, COL_GRID.b, 255);
        draw_text_right(r, PLOT_L - 6, y - text_h(1.3f) / 2, 1.3f, COL_DIM, "%d", p);
    }

    /* vertical time gridlines + labels */
    double ve = vs + span;
    long step;
    int day_mode;
    if (span <= 3.0 * 86400) {
        day_mode = 0;
        long cand[] = { 3600, 2*3600, 3*3600, 6*3600, 12*3600, 86400 };
        step = 86400;
        for (unsigned i = 0; i < sizeof(cand)/sizeof(cand[0]); i++)
            if (span / cand[i] <= 7) { step = cand[i]; break; }
    } else {
        day_mode = 1;
        long cand[] = { 86400, 2*86400, 7*86400 };
        step = 7 * 86400;
        for (unsigned i = 0; i < sizeof(cand)/sizeof(cand[0]); i++)
            if (span / cand[i] <= 8) { step = cand[i]; break; }
    }

    time_t base_t = (time_t)vs;
    struct tm bt;
    localtime_r(&base_t, &bt);
    bt.tm_hour = 0; bt.tm_min = 0; bt.tm_sec = 0; bt.tm_isdst = -1;
    time_t t = mktime(&bt);
    while ((double)t < vs) t += step;

    for (; (double)t <= ve + 1; t += step) {
        int x = (int)(time_to_x((double)t, vs, span) + 0.5);
        if (x < PLOT_L || x > PLOT_R) continue;
        struct tm lt;
        localtime_r(&t, &lt);
        int midnight = (lt.tm_hour == 0 && lt.tm_min == 0);
        SDL_Color gc = midnight ? COL_GRID_HL : COL_GRID;
        vlineRGBA(r, x, PLOT_T, PLOT_B, gc.r, gc.g, gc.b, 255);
        char lbl[24];
        if (day_mode || midnight) strftime(lbl, sizeof(lbl), "%b %d", &lt);
        else                      strftime(lbl, sizeof(lbl), "%H:%M", &lt);
        /* clamp label to the plot so the first/last butt against the axis edges */
        int lw = text_w(lbl, 1.3f);
        int lx = x - lw / 2;
        if (lx < PLOT_L) lx = PLOT_L;
        if (lx + lw > PLOT_R) lx = PLOT_R - lw;
        draw_text(r, lx, PLOT_B + 4, 1.3f, COL_DIM, "%s", lbl);
    }

    /* plot border */
    rectangleRGBA(r, PLOT_L, PLOT_T, PLOT_R, PLOT_B, COL_GRID_HL.r, COL_GRID_HL.g, COL_GRID_HL.b, 255);
}

/* First sample index with t >= tt (binary search; samples are time-sorted). */
static int lower_bound_t(double tt)
{
    int lo = 0, hi = g_count;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if ((double)g_samples[mid].t < tt) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

static void draw_chart(SDL_Renderer *r, double vs, double span)
{
    if (g_count < 1) return;
    double ve = vs + span;

    SDL_Rect clip = { PLOT_L + 1, PLOT_T + 1, PLOT_R - PLOT_L - 1, PLOT_B - PLOT_T - 1 };
    SDL_RenderSetClipRect(r, &clip);

    /* Restrict work to the visible window (+1 on each side for edge segments). */
    int lo = lower_bound_t(vs);
    int start = lo > 0 ? lo - 1 : 0;

    /* Area fill: one polygon per contiguous run of equal slope-state (even fill,
     * no seams). Reusable buffers avoid a per-frame malloc. */
    if (g_vcap < g_count + 2) {
        int nc = g_count + 2;
        Sint16 *nx = realloc(g_vx, (size_t)nc * sizeof(Sint16));
        Sint16 *ny = realloc(g_vy, (size_t)nc * sizeof(Sint16));
        if (nx) g_vx = nx;
        if (ny) g_vy = ny;
        if (nx && ny) g_vcap = nc;
    }
    if (g_vx && g_vy && g_vcap >= g_count + 2) {
        int i = start;
        while (i < g_count - 1) {
            if ((double)g_samples[i].t > ve) break;   /* rest is off the right edge */
            int state = seg_charging(&g_samples[i], &g_samples[i + 1]);
            int j = i + 1;
            while (j < g_count - 1 &&
                   seg_charging(&g_samples[j], &g_samples[j + 1]) == state)
                j++;
            int k = 0;
            for (int s = i; s <= j; s++) {
                g_vx[k] = (Sint16)time_to_x((double)g_samples[s].t, vs, span);
                g_vy[k] = (Sint16)level_to_y(g_samples[s].level);
                k++;
            }
            g_vx[k] = (Sint16)time_to_x((double)g_samples[j].t, vs, span); g_vy[k] = PLOT_B; k++;
            g_vx[k] = (Sint16)time_to_x((double)g_samples[i].t, vs, span); g_vy[k] = PLOT_B; k++;
            SDL_Color c = state ? COL_CHARGE : COL_DISCHARGE;
            filledPolygonRGBA(r, g_vx, g_vy, k, c.r, c.g, c.b, 70);
            i = j;
        }
    }

    /* Line on top, each segment colored by its slope. */
    for (int s = start + 1; s < g_count; s++) {
        Sample *a = &g_samples[s - 1];
        if ((double)a->t > ve) break;             /* past the right edge */
        Sample *b = &g_samples[s];
        double x0 = time_to_x((double)a->t, vs, span);
        double x1 = time_to_x((double)b->t, vs, span);
        double y0 = level_to_y(a->level);
        double y1 = level_to_y(b->level);
        SDL_Color c = seg_charging(a, b) ? COL_CHARGE : COL_DISCHARGE;
        thickLineRGBA(r, (Sint16)x0, (Sint16)y0, (Sint16)x1, (Sint16)y1, 2,
                      c.r, c.g, c.b, 255);
    }

    if (g_cursor >= 0 && g_cursor < g_count) {
        /* interactive cursor: vertical guide + dot at the selected sample */
        Sample *cs = &g_samples[g_cursor];
        if ((double)cs->t >= vs && (double)cs->t <= ve) {
            double x = time_to_x((double)cs->t, vs, span);
            double y = level_to_y(cs->level);
            vlineRGBA(r, (Sint16)x, PLOT_T, PLOT_B, COL_TEXT.r, COL_TEXT.g, COL_TEXT.b, 110);
            int up = (g_cursor > 0) ? (cs->level >= g_samples[g_cursor - 1].level)
                   : (g_cursor < g_count - 1 ? g_samples[g_cursor + 1].level >= cs->level : cs->charging);
            SDL_Color c = up ? COL_CHARGE : COL_DISCHARGE;
            filledCircleRGBA(r, (Sint16)x, (Sint16)y, 5, c.r, c.g, c.b, 255);
            aacircleRGBA(r, (Sint16)x, (Sint16)y, 5, 255, 255, 255, 230);
        }
    } else {
        /* newest-sample marker (live / free pan) */
        Sample *last = &g_samples[g_count - 1];
        if ((double)last->t >= vs && (double)last->t <= ve) {
            double x = time_to_x((double)last->t, vs, span);
            double y = level_to_y(last->level);
            SDL_Color c = last->charging ? COL_CHARGE : COL_DISCHARGE;
            filledCircleRGBA(r, (Sint16)x, (Sint16)y, 4, c.r, c.g, c.b, 255);
            aacircleRGBA(r, (Sint16)x, (Sint16)y, 4, 255, 255, 255, 200);
        }
    }

    SDL_RenderSetClipRect(r, NULL);
}

static void draw_legend(SDL_Renderer *r)
{
    int y = 434;
    int x = PLOT_L;
    int th = text_h(1.3f);
    struct { SDL_Color c; const char *t; } items[] = {
        { COL_CHARGE,    "Charging"    },
        { COL_DISCHARGE, "Discharging" },
    };
    for (int i = 0; i < 2; i++) {
        boxRGBA(r, x, y + 2, x + 14, y + 2 + th - 6, items[i].c.r, items[i].c.g, items[i].c.b, 255);
        draw_text(r, x + 20, y, 1.3f, COL_DIM, "%s", items[i].t);
        x += 20 + text_w(items[i].t, 1.3f) + 22;
    }

    /* cursor readout on the right of the legend row (hidden while live) */
    if (g_cursor >= 0 && g_cursor < g_count) {
        Sample *cs = &g_samples[g_cursor];
        int up = (g_cursor > 0) ? (cs->level >= g_samples[g_cursor - 1].level)
               : (g_cursor < g_count - 1 ? g_samples[g_cursor + 1].level >= cs->level : cs->charging);
        SDL_Color vc = up ? COL_CHARGE : COL_DISCHARGE;
        struct tm lt;
        localtime_r(&cs->t, &lt);
        char ts[32];
        strftime(ts, sizeof(ts), "%b %d  %H:%M", &lt);
        draw_text_right(r, PLOT_R, y, 1.3f, vc, "%s   %d%%", ts, cs->level);
    }

    draw_text_center(r, WIN_W / 2, 456, 1.3f, COL_DIM,
                     "R-Stick Cursor   D-Pad Pan   L1/R1 Zoom   A Live   SEL+START Quit");
}

static void render(SDL_Renderer *r)
{
    SDL_SetRenderDrawColor(r, COL_BG.r, COL_BG.g, COL_BG.b, 255);
    SDL_RenderClear(r);

    clamp_view();
    double span = current_span();
    double vs = g_view_end - span;

    draw_header(r);

    if (g_count == 0) {
        draw_text_center(r, WIN_W / 2, WIN_H / 2 - 20, 1.95f, COL_DIM,
                         "No battery logs found");
        draw_text_center(r, WIN_W / 2, WIN_H / 2 + 6, 1.3f, COL_DIM, "%s", LOG_DIR);
        if (!g_no_present) SDL_RenderPresent(r);
        return;
    }

    draw_grid(r, vs, span);
    draw_chart(r, vs, span);
    draw_legend(r);

    if (!g_no_present) SDL_RenderPresent(r);
}

/* ------------------------------------------------------------------ */
/* input                                                               */
/* ------------------------------------------------------------------ */
#define BTN_B      0
#define BTN_A      1
#define BTN_L1     4
#define BTN_R1     5
#define BTN_UP     8
#define BTN_DOWN   9
#define BTN_LEFT   10
#define BTN_RIGHT  11
#define BTN_SELECT 12
#define BTN_START  13

static void zoom_in(void)  { if (g_zoom > 0) g_zoom--; else if (g_zoom == NZOOM) g_zoom = NZOOM - 1; clamp_view(); }
static void zoom_out(void) { if (g_zoom < NZOOM) g_zoom++; clamp_view(); }

int main(int argc, char **argv)
{
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1"); /* let jslisten killall work */

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow("batterymonitor", 0, 0, WIN_W, WIN_H,
                                       SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!win) { fprintf(stderr, "CreateWindow: %s\n", SDL_GetError()); SDL_Quit(); return 1; }

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1,
                                           SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) { fprintf(stderr, "CreateRenderer: %s\n", SDL_GetError()); SDL_Quit(); return 1; }

    SDL_RenderSetLogicalSize(ren, WIN_W, WIN_H);
    SDL_ShowCursor(SDL_DISABLE);

    font_init();

    for (int i = 0; i < SDL_NumJoysticks(); i++) SDL_JoystickOpen(i);

    const char *shot_path = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--days") && i + 1 < argc)      g_days = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) shot_path = argv[++i];
    }

    load_logs();

    /* Screenshot mode: render one frame, save it as a BMP, and exit. */
    if (shot_path) {
        g_no_present = 1;
        render(ren);
        int ow = WIN_W, oh = WIN_H;
        SDL_GetRendererOutputSize(ren, &ow, &oh);
        SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, ow, oh, 32, SDL_PIXELFORMAT_ARGB8888);
        if (s && SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch) == 0)
            SDL_SaveBMP(s, shot_path);
        if (s) SDL_FreeSurface(s);
        for (int i = 0; i < TC_N; i++) if (g_tc[i].tex) SDL_DestroyTexture(g_tc[i].tex);
        for (int i = 0; i < g_font_n; i++) if (g_fonts[i]) TTF_CloseFont(g_fonts[i]);
        if (g_have_ttf) TTF_Quit();
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 0;
    }

    int held_left = 0, held_right = 0;
    int stick_left = 0, stick_right = 0;
    int held_select = 0, held_start = 0;
    int rs_val = 0, rs_prev = 0;
    Uint32 rs_hold = 0;
    float rs_accum = 0.0f;
    Uint32 last_reload = SDL_GetTicks();
    Uint32 prev = SDL_GetTicks();
    int running = 1;

    render(ren);   /* initial frame, before we ever block on events */

    while (running) {
        int dirty = 0;

        /* Drain input non-blocking; pacing / idle sleep happen after rendering. */
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
                running = 0;
                break;
            case SDL_WINDOWEVENT:
                dirty = 1;
                break;
            case SDL_JOYDEVICEADDED:
                SDL_JoystickOpen(e.jdevice.which);
                break;
            case SDL_KEYDOWN:
                switch (e.key.keysym.sym) {
                case SDLK_ESCAPE:
                case SDLK_q:      running = 0; break;
                case SDLK_LEFT:   held_left = 1; break;
                case SDLK_RIGHT:  held_right = 1; break;
                case SDLK_UP:
                case SDLK_PLUS:
                case SDLK_EQUALS: zoom_in(); dirty = 1; break;
                case SDLK_DOWN:
                case SDLK_MINUS:  zoom_out(); dirty = 1; break;
                case SDLK_a:      g_follow = 1; g_cursor = -1; clamp_view(); dirty = 1; break;
                case SDLK_b:      g_follow = 0; g_cursor = -1; g_view_end = (double)g_tmin + current_span(); clamp_view(); dirty = 1; break;
                case SDLK_COMMA:  cursor_step(-1); dirty = 1; break;
                case SDLK_PERIOD: cursor_step(1);  dirty = 1; break;
                default: break;
                }
                break;
            case SDL_KEYUP:
                if (e.key.keysym.sym == SDLK_LEFT)  held_left = 0;
                if (e.key.keysym.sym == SDLK_RIGHT) held_right = 0;
                break;
            case SDL_JOYBUTTONDOWN:
                switch (e.jbutton.button) {
                case BTN_LEFT:  held_left = 1; break;
                case BTN_RIGHT: held_right = 1; break;
                case BTN_R1:
                case BTN_UP:    zoom_in(); dirty = 1; break;
                case BTN_L1:
                case BTN_DOWN:  zoom_out(); dirty = 1; break;
                case BTN_A:     g_follow = 1; g_cursor = -1; clamp_view(); dirty = 1; break;
                case BTN_B:     g_follow = 0; g_cursor = -1; g_view_end = (double)g_tmin + current_span(); clamp_view(); dirty = 1; break;
                case BTN_SELECT: held_select = 1; break;
                case BTN_START:  held_start = 1; break;
                default: break;
                }
                break;
            case SDL_JOYBUTTONUP:
                if (e.jbutton.button == BTN_LEFT)  held_left = 0;
                if (e.jbutton.button == BTN_RIGHT) held_right = 0;
                if (e.jbutton.button == BTN_SELECT) held_select = 0;
                if (e.jbutton.button == BTN_START)  held_start = 0;
                break;
            case SDL_JOYAXISMOTION:
                if (e.jaxis.axis == 0) {
                    stick_left  = (e.jaxis.value < -12000);
                    stick_right = (e.jaxis.value >  12000);
                } else if (e.jaxis.axis == 2) {   /* right stick X = cursor step */
                    rs_val = e.jaxis.value;
                }
                break;
            default: break;
            }
        }

        if (held_select && held_start) running = 0;   /* Select+Start quits (jslisten combo needs L2) */

        Uint32 now = SDL_GetTicks();
        float dt = (float)(now - prev) / 1000.0f;
        prev = now;
        if (dt > 0.033f) dt = 0.033f;   /* clamp so a held pan can't jump after idle/stall */

        int dir = (held_right || stick_right) - (held_left || stick_left);
        if (dir) {
            double before = g_view_end;
            g_view_end += (double)dir * PAN_VEL * current_span() * (double)dt;
            g_follow = 0;
            clamp_view();
            if (g_view_end != before) dirty = 1;   /* stop redrawing once at the edge */
        }

        /* Right stick steps the cursor; full deflection = fast, with a key-repeat delay. */
        int rs_dir = (rs_val < -RS_DEAD) ? -1 : (rs_val > RS_DEAD) ? 1 : 0;
        if (rs_dir != 0) {
            if (rs_prev == 0) {
                cursor_step(rs_dir);              /* one step on press */
                rs_hold = now + RS_DELAY;         /* then wait before auto-repeat */
                rs_accum = 0.0f;
                dirty = 1;
            } else if ((Sint32)(now - rs_hold) >= 0) {
                int span = 32767 - RS_DEAD;
                int mag = abs(rs_val) - RS_DEAD;
                if (mag > span) mag = span;
                float sps = RS_MIN_SPS + (RS_MAX_SPS - RS_MIN_SPS) * (float)mag / (float)span;
                rs_accum += sps * dt;
                int n = (int)rs_accum;
                if (n > 0) { rs_accum -= (float)n; cursor_step(n * rs_dir); dirty = 1; }
            }
        }
        rs_prev = rs_dir;

        if (now - last_reload >= RELOAD_MS) {
            if (reload_logs()) dirty = 1;
            last_reload = now;
        }

        int active = (held_left || held_right || stick_left || stick_right ||
                      rs_val < -RS_DEAD || rs_val > RS_DEAD);

        if (dirty) render(ren);

        if (active) {
            /* ~60 fps cadence while interacting (vsync already paces dirty frames) */
            Uint32 ft = SDL_GetTicks() - now;
            if (ft < 15) SDL_Delay(15 - ft);
        } else {
            SDL_Delay(IDLE_POLL_MS);   /* idle: coarse poll so the CPU can sleep */
        }
    }

    free(g_samples);
    free(g_vx);
    free(g_vy);
    for (int i = 0; i < TC_N; i++) if (g_tc[i].tex) SDL_DestroyTexture(g_tc[i].tex);
    for (int i = 0; i < g_font_n; i++) if (g_fonts[i]) TTF_CloseFont(g_fonts[i]);
    if (g_have_ttf) TTF_Quit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
