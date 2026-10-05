/*
 * joytest - SDL2 joystick / gamepad test program
 *
 * Graphically shows all buttons, axes (sticks/triggers) and hats (D-pad) of a
 * connected joystick or gamepad. Buttons/hats come from SDL; the axes can be
 * shown either as SDL-normalized ("calibrated") values or as the raw evdev ADC
 * values (for calibrating the sticks). Toggle with SELECT+L1.
 *
 * Controls (quitting via SELECT+START+L2 is handled by jslisten, not the tool):
 *   SELECT + R1  - next device            SELECT + Y  - toggle circle/square
 *   SELECT + X   - reset tuning+min/max   SELECT + B  - apply per-side tuning (raw mode, cal ready)
 *   SELECT + A   - set center (adc_cal)   SELECT + L1 - toggle raw / calibrated axis values
 *
 * Build:
 *   gcc joytest.c -o joytest $(pkg-config --cflags --libs sdl2) -lSDL2_gfx -lSDL2_ttf -lm
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#define WIN_W 640
#define WIN_H 480

/* Drift threshold in normalized units (-32768..32767); scaled per axis fullscale. */
#define DRIFT_THRESHOLD 600

#define MAX_AXES   32
#define MAX_BUTTONS 64
#define MAX_HATS    8

typedef struct {
    int min;      /* smallest value seen so far */
    int max;      /* largest value seen so far */
    int rest;     /* reference resting value (for drift calculation) */
} AxisStat;

/* SDL axis index -> evdev ABS event code (GO-Super Gamepad: both analog sticks). */
static const int ABS_CODE[4] = { ABS_X, ABS_Y, ABS_RX, ABS_RY };

/* Original (pre-calibration) fullscale per stick axis, captured once at startup so
 * RAW scaling stays pristine even after a calibration narrowed the kernel range. */
static int g_orig_fs[4];
static int g_have_orig = 0;

static SDL_Color COL_BG       = {  24,  26,  32, 255 };
static SDL_Color COL_PANEL    = {  36,  39,  48, 255 };
static SDL_Color COL_TEXT     = { 220, 222, 228, 255 };
static SDL_Color COL_DIM      = { 130, 134, 145, 255 };
static SDL_Color COL_ACTIVE   = {  70, 200, 120, 255 };
static SDL_Color COL_AXIS     = {  90, 160, 240, 255 };
static SDL_Color COL_DRIFT    = { 235,  80,  80, 255 };
static SDL_Color COL_BORDER   = {  70,  74,  86, 255 };

/* ------------------------------------------------------------------ */
/* text (SDL2_ttf / DejaVu, bitmap fallback, small texture cache)      */
/* ------------------------------------------------------------------ */
#define FONT_PX  15                /* DejaVuSansMono pixel size for all UI text */
#define TITLE_PX 18                /* bold title size */
static TTF_Font *g_font;
static TTF_Font *g_title_font;     /* synthetic-bold, TITLE_PX */
static int       g_have_ttf;
static int       g_text_yoff;      /* font top padding above cap height; subtracted so glyphs anchor at y like the old bitmap */
static int       g_title_yoff;

/* Top padding above cap height for a given font (so glyphs anchor at y). */
static int font_yoff(TTF_Font *f)
{
    int mnx, mxx, mny, mxy, adv;
    if (f && TTF_GlyphMetrics(f, 'A', &mnx, &mxx, &mny, &mxy, &adv) == 0)
        return TTF_FontAscent(f) - mxy;
    return 0;
}

static void font_init(void)
{
    if (TTF_Init() != 0) return;
    const char *cands[] = {
        "/usr/share/fonts/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/liberation/LiberationMono-Regular.ttf",
        NULL
    };
    for (int i = 0; cands[i]; i++) {
        g_font = TTF_OpenFont(cands[i], FONT_PX);
        if (g_font) {
            g_have_ttf = 1;
            g_title_font = TTF_OpenFont(cands[i], TITLE_PX);
            if (g_title_font) TTF_SetFontStyle(g_title_font, TTF_STYLE_BOLD);
            break;
        }
    }
    g_text_yoff  = font_yoff(g_font);
    g_title_yoff = font_yoff(g_title_font);
}

static void font_quit(void)
{
    if (g_title_font) { TTF_CloseFont(g_title_font); g_title_font = NULL; }
    if (g_font) { TTF_CloseFont(g_font); g_font = NULL; }
    if (g_have_ttf) { TTF_Quit(); g_have_ttf = 0; }
}

/* Cache rendered strings so we don't re-rasterize identical text every frame. */
typedef struct { char str[64]; int px; Uint8 r, g, b; SDL_Texture *tex; int w, h; Uint32 used; } TextCache;
#define TC_N 64
static TextCache g_tc[TC_N];
static Uint32    g_tc_clock;

static SDL_Texture *text_tex(SDL_Renderer *rnd, TTF_Font *font, int px, const char *s, SDL_Color c, int *w, int *h)
{
    for (int i = 0; i < TC_N; i++) {
        if (g_tc[i].tex && g_tc[i].px == px && g_tc[i].r == c.r && g_tc[i].g == c.g && g_tc[i].b == c.b &&
            strcmp(g_tc[i].str, s) == 0) {
            g_tc[i].used = ++g_tc_clock;
            *w = g_tc[i].w; *h = g_tc[i].h;
            return g_tc[i].tex;
        }
    }
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, s, c);
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

static int text_w(const char *s)
{
    if (g_have_ttf) { int w = 0, h = 0; TTF_SizeUTF8(g_font, s, &w, &h); return w; }
    return (int)(strlen(s) * 9);
}

/* Render one line with an explicit font/size (TTF), bitmap fallback otherwise. */
static void draw_text_f(SDL_Renderer *r, int x, int y, TTF_Font *font, int px, int yoff,
                        SDL_Color c, const char *buf)
{
    if (g_have_ttf && font) {
        int w, h;
        SDL_Texture *t = text_tex(r, font, px, buf, c, &w, &h);
        if (t) {
            SDL_Rect dst = { x, y - yoff, w, h };
            SDL_RenderCopy(r, t, NULL, &dst);
            return;
        }
    }
    /* fallback: SDL2_gfx 8px bitmap font scaled to px */
    float s = px / 8.0f, sx, sy;
    SDL_RenderGetScale(r, &sx, &sy);
    SDL_RenderSetScale(r, sx * s, sy * s);
    stringRGBA(r, (Sint16)(x / s), (Sint16)(y / s), buf, c.r, c.g, c.b, c.a);
    SDL_RenderSetScale(r, sx, sy);
}

static void draw_text(SDL_Renderer *r, int x, int y, SDL_Color c, const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    draw_text_f(r, x, y, g_font, FONT_PX, g_text_yoff, c, buf);
}

/* Bold, larger title line. */
static void draw_title(SDL_Renderer *r, int x, int y, SDL_Color c, const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    TTF_Font *f = g_title_font ? g_title_font : g_font;
    int yoff = g_title_font ? g_title_yoff : g_text_yoff;
    draw_text_f(r, x, y, f, TITLE_PX, yoff, c, buf);
}

/* Draw text horizontally centered on cx. */
static void draw_text_center(SDL_Renderer *r, int cx, int y, SDL_Color c, const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    draw_text(r, cx - text_w(buf) / 2, y, c, "%s", buf);
}

static void fill_rect(SDL_Renderer *r, int x, int y, int w, int h, SDL_Color c)
{
    SDL_Rect rc = { x, y, w, h };
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(r, &rc);
}

static void draw_rect(SDL_Renderer *r, int x, int y, int w, int h, SDL_Color c)
{
    SDL_Rect rc = { x, y, w, h };
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderDrawRect(r, &rc);
}

/* Circle->square mapping: stretches the circular stick area onto a square. */
static void circle_to_square(float *ax, float *ay)
{
    float len = sqrtf((*ax) * (*ax) + (*ay) * (*ay));
    if (len > 0.001f) {
        float maxcomp = fmaxf(fabsf(*ax), fabsf(*ay));
        float scale = len / maxcomp;
        *ax = fmaxf(-1.0f, fminf(1.0f, *ax * scale));
        *ay = fmaxf(-1.0f, fminf(1.0f, *ay * scale));
    }
}

/* Draws a stick pad: square field with a dot for (ax,ay) in -1..1. */
static void draw_stick(SDL_Renderer *r, int cx, int cy, int size,
                       float ax, float ay, const char *label, int drift,
                       int squaremap)
{
    int half = size / 2;
    int left = cx - half;
    int top  = cy - half;

    fill_rect(r, left, top, size, size, COL_PANEL);
    draw_rect(r, left, top, size, size, COL_BORDER);

    SDL_SetRenderDrawColor(r, COL_BORDER.r, COL_BORDER.g, COL_BORDER.b, 255);
    SDL_RenderDrawLine(r, left, cy, left + size, cy);
    SDL_RenderDrawLine(r, cx, top, cx, top + size);
    circleRGBA(r, (Sint16)cx, (Sint16)cy, (Sint16)(half - 2),
               COL_BORDER.r, COL_BORDER.g, COL_BORDER.b, 255);

    float disp_ax = ax, disp_ay = ay;
    if (squaremap)
        circle_to_square(&disp_ax, &disp_ay);
    if (disp_ax >  1.0f) disp_ax =  1.0f;
    if (disp_ax < -1.0f) disp_ax = -1.0f;
    if (disp_ay >  1.0f) disp_ay =  1.0f;
    if (disp_ay < -1.0f) disp_ay = -1.0f;
    int px = cx + (int)(disp_ax * (half - 6));
    int py = cy + (int)(disp_ay * (half - 6));
    SDL_Color dot = drift ? COL_DRIFT : COL_ACTIVE;
    filledCircleRGBA(r, (Sint16)px, (Sint16)py, 7, dot.r, dot.g, dot.b, 255);
    aacircleRGBA(r, (Sint16)px, (Sint16)py, 7, 0, 0, 0, 255);

    draw_text(r, left, top - 14, COL_TEXT, "%s", label);
}

/* Horizontal axis bar with current value plus min/max markers.
 * fullscale is the axis full deflection (32767 for SDL, ABS max for raw). */
static void draw_axis_bar(SDL_Renderer *r, int x, int y, int w,
                          int idx, int value, int fullscale, AxisStat *st)
{
    const int h = 16;
    if (fullscale <= 0) fullscale = 1;

    fill_rect(r, x, y, w, h, COL_PANEL);
    draw_rect(r, x, y, w, h, COL_BORDER);

    int mid = x + w / 2;
    SDL_SetRenderDrawColor(r, COL_DIM.r, COL_DIM.g, COL_DIM.b, 255);
    SDL_RenderDrawLine(r, mid, y, mid, y + h);

    float n = value / (float)fullscale;
    if (n > 1.0f)  n = 1.0f;
    if (n < -1.0f) n = -1.0f;
    int fillw = (int)(n * (w / 2));
    SDL_Color bc = COL_AXIS;
    if (fillw >= 0)
        fill_rect(r, mid, y + 2, fillw, h - 4, bc);
    else
        fill_rect(r, mid + fillw, y + 2, -fillw, h - 4, bc);

    int minx = mid + (int)((st->min / (float)fullscale) * (w / 2));
    int maxx = mid + (int)((st->max / (float)fullscale) * (w / 2));
    vlineRGBA(r, (Sint16)minx, (Sint16)y, (Sint16)(y + h),
              COL_TEXT.r, COL_TEXT.g, COL_TEXT.b, 255);
    vlineRGBA(r, (Sint16)maxx, (Sint16)y, (Sint16)(y + h),
              COL_TEXT.r, COL_TEXT.g, COL_TEXT.b, 255);

    int drift = value - st->rest;
    int dth = (int)((long)DRIFT_THRESHOLD * fullscale / 32767);
    if (dth < 1) dth = 1;
    SDL_Color lblc = (abs(drift) > dth) ? COL_DRIFT : COL_TEXT;
    draw_text(r, x + w + 12, y + 4, lblc,
              "%2d:%6d min%6d max%6d d%6d",
              idx, value, st->min, st->max, drift);
}

/* Hat / D-pad as a direction cross. */
static void draw_hat(SDL_Renderer *r, int cx, int cy, int size, Uint8 hat, int idx)
{
    int s = size;
    SDL_Color up    = (hat & SDL_HAT_UP)    ? COL_ACTIVE : COL_PANEL;
    SDL_Color down  = (hat & SDL_HAT_DOWN)  ? COL_ACTIVE : COL_PANEL;
    SDL_Color left  = (hat & SDL_HAT_LEFT)  ? COL_ACTIVE : COL_PANEL;
    SDL_Color right = (hat & SDL_HAT_RIGHT) ? COL_ACTIVE : COL_PANEL;

    fill_rect(r, cx,         cy - s,     s, s, up);
    fill_rect(r, cx,         cy + s,     s, s, down);
    fill_rect(r, cx - s,     cy,         s, s, left);
    fill_rect(r, cx + s,     cy,         s, s, right);
    fill_rect(r, cx,         cy,         s, s, COL_PANEL);

    draw_rect(r, cx,     cy - s, s, s, COL_BORDER);
    draw_rect(r, cx,     cy + s, s, s, COL_BORDER);
    draw_rect(r, cx - s, cy,     s, s, COL_BORDER);
    draw_rect(r, cx + s, cy,     s, s, COL_BORDER);
    draw_rect(r, cx,     cy,     s, s, COL_BORDER);

    draw_text(r, cx - s, cy - s - 16, COL_TEXT, "Hat %d", idx);
}

/* Full button labels for the RG351MP GO-Super Gamepad (SDL joystick button indices). */
static const char *btn_name(int idx)
{
    switch (idx) {
    case 0:  return "B";
    case 1:  return "A";
    case 2:  return "X";
    case 3:  return "Y";
    case 4:  return "L1";
    case 5:  return "R1";
    case 6:  return "L2";
    case 7:  return "R2";
    case 8:  return "UP";
    case 9:  return "DOWN";
    case 10: return "LEFT";
    case 11: return "RIGHT";
    case 12: return "SELECT";
    case 13: return "START";
    case 14: return "L3";
    case 15: return "R3";
    default: return "?";
    }
}

/* Axis designations for the GO-Super Gamepad (axes 0..3 = both analog sticks). */
static const char *axis_name(int idx)
{
    switch (idx) {
    case 0: return "Left Stick X";
    case 1: return "Left Stick Y";
    case 2: return "Right Stick X";
    case 3: return "Right Stick Y";
    default: return "Axis";
    }
}

/* Find and open the evdev device matching the joystick name (must expose ABS_X).
 * Fills pathout with /dev/input/eventN. Returns the fd or -1. */
static int open_evdev_for(const char *jsname, char *pathout, int pathlen)
{
    if (pathout && pathlen) pathout[0] = 0;
    if (!jsname) return -1;
    DIR *d = opendir("/dev/input");
    if (!d) return -1;
    struct dirent *de;
    int fd = -1;
    char path[300], nm[256];
    while ((de = readdir(d)) != NULL) {
        if (strncmp(de->d_name, "event", 5) != 0) continue;
        snprintf(path, sizeof(path), "/dev/input/%s", de->d_name);
        int f = open(path, O_RDONLY | O_NONBLOCK);
        if (f < 0) continue;
        nm[0] = 0;
        unsigned long absbit = 0;
        if (ioctl(f, EVIOCGNAME(sizeof(nm)), nm) >= 0 &&
            strcmp(nm, jsname) == 0 &&
            ioctl(f, EVIOCGBIT(EV_ABS, sizeof(absbit)), &absbit) >= 0 &&
            (absbit & (1UL << ABS_X))) {
            if (pathout && pathlen) snprintf(pathout, pathlen, "%s", path);
            /* Reopen writable so we can push calibration into the kernel (EVIOCSABS). */
            int wf = open(path, O_RDWR | O_NONBLOCK);
            if (wf >= 0) { close(f); fd = wf; }
            else fd = f;
            break;
        }
        close(f);
    }
    closedir(d);
    return fd;
}

/* Current raw value of an ABS code; also returns the declared fullscale (max(|min|,max)). */
static int read_raw_axis(int fd, int code, int *fullscale)
{
    struct input_absinfo ai;
    if (fd < 0 || ioctl(fd, EVIOCGABS(code), &ai) < 0) {
        if (fullscale) *fullscale = 1;
        return 0;
    }
    if (fullscale) {
        int fs = ai.maximum;
        if (-ai.minimum > fs) fs = -ai.minimum;
        *fullscale = (fs > 0) ? fs : 1;
    }
    return ai.value;
}

/* Current value + fullscale of axis i, honoring raw/calibrated mode. */
static int axis_val(SDL_Joystick *joy, int ev_fd, int raw_mode, int i, int *fullscale)
{
    if (raw_mode && ev_fd >= 0 && i >= 0 && i < 4) {
        int v = read_raw_axis(ev_fd, ABS_CODE[i], fullscale);
        /* Scale RAW against the original range, not a range a calibration shrank. */
        if (g_have_orig && fullscale) *fullscale = g_orig_fs[i];
        return v;
    }
    if (fullscale) *fullscale = 32767;
    return joy ? SDL_JoystickGetAxis(joy, i) : 0;
}

/* --- odroidgo3-joypad driver calibration (runtime sysfs) --------------------
 * The driver centers each axis on adc->cal and scales each side independently:
 *   report = (raw - cal) * tuning_[p|n] / 100, clamped to +-1800.
 * adc_cal (echo 1) re-zeroes at the current rest position; adc_tuning sets the
 * per-side gain. The driver axis index is reversed vs. SDL order (SDL i -> 3-i). */
#define JOYPAD_SYS   "/sys/devices/platform/odroidgo3-joypad"
#define TUNE_TARGET  1800   /* driver report full-scale (ADC_MAX_VOLTAGE/2 * scale) */
#define TUNE_MIN     100
#define TUNE_MAX     300

static int write_sys(const char *path, const char *val)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    ssize_t n = write(fd, val, strlen(val));
    close(fd);
    return (n < 0) ? -1 : 0;
}

/* Re-zero all axes at the current (resting) position. */
static void trigger_center(void)
{
    write_sys(JOYPAD_SYS "/adc_cal", "1\n");
}

/* Reset per-side gain to neutral (1:1) so a fresh measurement is unamplified. */
static void reset_tuning(void)
{
    for (int i = 0; i < 4; ++i) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d 100 100\n", i);
        write_sys(JOYPAD_SYS "/adc_tuning", buf);
    }
}

/* Read the driver's current per-side gain (driver-indexed), defaulting to 100. */
static void read_current_tuning(int p[4], int n[4])
{
    for (int i = 0; i < 4; ++i) { p[i] = 100; n[i] = 100; }
    int fd = open(JOYPAD_SYS "/adc_tuning", O_RDONLY);
    if (fd < 0) return;
    char buf[512];
    ssize_t r = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (r <= 0) return;
    buf[r] = 0;
    for (char *line = buf; line && *line; ) {
        int idx, pp, nn;
        if (sscanf(line, "adc[%d]->tuning_p = %d, tuning_n = %d", &idx, &pp, &nn) == 3
            && idx >= 0 && idx < 4) { p[idx] = pp; n[idx] = nn; }
        line = strchr(line, '\n');
        if (line) line++;
    }
}

/* New per-side gain so the measured half-travel reaches full scale, cumulative on
 * the current gain (so re-calibrating refines instead of resetting). */
static void tuning_for(const AxisStat *s, int cur_p, int cur_n, int *tp, int *tn)
{
    int pos = s->max;  if (pos < 1) pos = 1;
    int neg = -s->min; if (neg < 1) neg = 1;
    int p = (int)((long)cur_p * TUNE_TARGET / pos);
    int n = (int)((long)cur_n * TUNE_TARGET / neg);
    if (p < TUNE_MIN) p = TUNE_MIN;
    if (p > TUNE_MAX) p = TUNE_MAX;
    if (n < TUNE_MIN) n = TUNE_MIN;
    if (n > TUNE_MAX) n = TUNE_MAX;
    *tp = p; *tn = n;
}

/* Compute final per-side gain for all axes (driver-indexed) from the measurement. */
static void compute_tuning(const AxisStat *st, int naxes, int out_p[4], int out_n[4])
{
    int curp[4], curn[4];
    read_current_tuning(curp, curn);
    for (int d = 0; d < 4; ++d) { out_p[d] = curp[d]; out_n[d] = curn[d]; }
    int n = naxes; if (n > 4) n = 4;
    for (int i = 0; i < n; ++i) {
        int d = 3 - i;                 /* SDL axis i -> driver axis */
        tuning_for(&st[i], curp[d], curn[d], &out_p[d], &out_n[d]);
    }
}

static int apply_tuning_vals(const int p[4], const int n[4])
{
    int ok = 1;
    for (int d = 0; d < 4; ++d) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%d %d %d\n", d, p[d], n[d]);
        if (write_sys(JOYPAD_SYS "/adc_tuning", buf) < 0) ok = 0;
    }
    return ok ? 0 : -1;
}

/* Record the tuning as a shell script for optional boot persistence. */
static void save_tuning_file(const char *name, const int p[4], const int n[4])
{
    char path[256];
    snprintf(path, sizeof(path), "/storage/.config/%s.tuning", name ? name : "joystick");
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "#!/bin/sh\n# joytest per-side tuning for %s\n", name ? name : "joystick");
    for (int d = 0; d < 4; ++d)
        fprintf(f, "echo \"%d %d %d\" > %s/adc_tuning\n", d, p[d], n[d], JOYPAD_SYS);
    fclose(f);
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    /* SDL otherwise turns SIGINT/SIGTERM into SDL_QUIT; since jslisten kills us with
     * SIGTERM and we do not handle SDL_QUIT, disable SDL's signal handlers. */
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow("Joystick / Gamepad Test",
                                       SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED,
                                       WIN_W, WIN_H, SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!win) {
        fprintf(stderr, "CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1,
                            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren)
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) {
        fprintf(stderr, "CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    SDL_RenderSetLogicalSize(ren, WIN_W, WIN_H);
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    font_init();

    int current = 0;
    SDL_Joystick *joy = NULL;
    int ev_fd = -1;                  /* evdev fd for raw axis reads */
    char ev_path[300] = "";          /* holds /dev/input/eventN; sized to open_evdev_for's path buffer */
    int raw_mode = 0;                /* 0 = SDL calibrated, 1 = raw evdev values */
    AxisStat axstat[MAX_AXES];
    int squaremap = 0;

    #define BTN_MODIFIER  12  /* SELECT */
    #define BTN_NEXT_DEV   5  /* R1  (+ mod -> next device) */
    #define BTN_RESET      2  /* X   (+ mod -> reset min/max) */
    #define BTN_DRIFTCAL   1  /* A   (+ mod -> set center/zero) */
    #define BTN_SQUARETGL  3  /* Y   (+ mod -> toggle mapping) */
    #define BTN_CALIB      0  /* B   (+ mod -> save calibration) */
    #define BTN_RAWTGL     4  /* L1  (+ mod -> toggle raw/calibrated) */

    Uint8 btn_state[MAX_BUTTONS];
    memset(btn_state, 0, sizeof(btn_state));

    #define RESET_STATS() do {                         \
        for (int i = 0; i < MAX_AXES; ++i) {           \
            axstat[i].min  = 0;                        \
            axstat[i].max  = 0;                        \
            axstat[i].rest = 0;                        \
        }                                              \
    } while (0)

    RESET_STATS();

    #define OPEN_JOY(idx) do {                              \
        if (joy) { SDL_JoystickClose(joy); joy = NULL; }    \
        if (ev_fd >= 0) { close(ev_fd); ev_fd = -1; }       \
        ev_path[0] = 0;                                     \
        memset(btn_state, 0, sizeof(btn_state));            \
        int n = SDL_NumJoysticks();                         \
        if (n > 0) {                                        \
            current = ((idx) % n + n) % n;                  \
            joy = SDL_JoystickOpen(current);                \
            ev_fd = open_evdev_for(SDL_JoystickName(joy), ev_path, sizeof(ev_path)); \
            RESET_STATS();                                  \
        }                                                   \
    } while (0)

    OPEN_JOY(0);

    /* Capture the pristine fullscale once, before any calibration can narrow it. */
    if (!g_have_orig && ev_fd >= 0) {
        for (int i = 0; i < 4; ++i) { int fs; read_raw_axis(ev_fd, ABS_CODE[i], &fs); g_orig_fs[i] = fs; }
        g_have_orig = 1;
    }

    #define AXIS_EPS 256
    int  need_draw = 2;
    int  prev_axis[MAX_AXES];
    for (int i = 0; i < MAX_AXES; ++i) prev_axis[i] = 0;
    char last_active[32] = "-";
    char saved_msg[96] = "";
    int  stable_cnt = 0;
    int  cal_ready = 0;
    #define CAL_STABLE_FRAMES 60   /* frames of rotation w/o new extremes -> ready */

    int running = 1;
    SDL_Event e;

    while (running) {
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_KEYDOWN:
                need_draw = 2;
                if (e.key.keysym.sym == SDLK_TAB) {
                    OPEN_JOY(current + 1);
                } else if (e.key.keysym.sym == SDLK_r) {
                    RESET_STATS();
                } else if (e.key.keysym.sym == SDLK_m) {
                    squaremap = !squaremap;
                }
                break;

            case SDL_JOYBUTTONDOWN:
                need_draw = 2;
                if (joy && e.jbutton.which == SDL_JoystickInstanceID(joy)) {
                    int btn = e.jbutton.button;
                    if (btn < MAX_BUTTONS) btn_state[btn] = 1;
                    snprintf(last_active, sizeof(last_active), "%s", btn_name(btn));
                    int mod = btn_state[BTN_MODIFIER];
                    if (mod && btn == BTN_NEXT_DEV) {
                        OPEN_JOY(current + 1);
                    } else if (mod && btn == BTN_RESET) {
                        reset_tuning();   /* clear driver gain back to neutral (1:1) */
                        RESET_STATS();
                        stable_cnt = 0; cal_ready = 0;
                    } else if (mod && btn == BTN_DRIFTCAL) {
                        trigger_center();   /* driver re-zeroes at the current rest position */
                        int na = SDL_JoystickNumAxes(joy);
                        if (na > MAX_AXES) na = MAX_AXES;
                        for (int i = 0; i < na; ++i) { int fs; axstat[i].rest = axis_val(joy, ev_fd, raw_mode, i, &fs); }
                    } else if (mod && btn == BTN_SQUARETGL) {
                        squaremap = !squaremap;
                    } else if (mod && btn == BTN_RAWTGL) {
                        raw_mode = !raw_mode;
                        RESET_STATS();
                        stable_cnt = 0; cal_ready = 0;
                        for (int i = 0; i < MAX_AXES; ++i) prev_axis[i] = 0;
                        saved_msg[0] = 0;
                    } else if (mod && btn == BTN_CALIB) {
                        if (raw_mode && cal_ready) {
                            const char *dn = SDL_JoystickName(joy);
                            int na2 = SDL_JoystickNumAxes(joy);
                            int fp[4], fn[4];
                            compute_tuning(axstat, na2, fp, fn);
                            int ap = apply_tuning_vals(fp, fn);
                            save_tuning_file(dn, fp, fn);
                            snprintf(saved_msg, sizeof(saved_msg), "%s: %s.tuning",
                                     ap == 0 ? "Tuning applied" : "Tuning FAILED",
                                     dn ? dn : "joystick");
                            /* Show the normalized result; re-entering RAW keeps the
                             * calibration (SELECT+X clears gain to neutral). */
                            raw_mode = 0;
                            RESET_STATS();
                            stable_cnt = 0; cal_ready = 0;
                            for (int i = 0; i < MAX_AXES; ++i) prev_axis[i] = 0;
                        } else if (!raw_mode) {
                            snprintf(saved_msg, sizeof(saved_msg), "Switch to RAW (SELECT+L1) first!");
                        } else {
                            snprintf(saved_msg, sizeof(saved_msg), "Rotate all sticks fully first!");
                        }
                    }
                }
                break;

            case SDL_JOYBUTTONUP:
                need_draw = 2;
                if (joy && e.jbutton.which == SDL_JoystickInstanceID(joy)) {
                    int btn = e.jbutton.button;
                    if (btn < MAX_BUTTONS) btn_state[btn] = 0;
                }
                break;

            case SDL_JOYHATMOTION:
                need_draw = 2;
                break;

            case SDL_JOYDEVICEADDED:
                need_draw = 2;
                if (!joy) OPEN_JOY(0);
                break;

            case SDL_JOYDEVICEREMOVED:
                need_draw = 2;
                if (joy && e.jdevice.which == SDL_JoystickInstanceID(joy)) {
                    OPEN_JOY(0);
                }
                break;
            }
        }

        /* Track min/max, activity and calibration-ready (raw mode only). */
        if (joy) {
            int na = SDL_JoystickNumAxes(joy);
            if (na > MAX_AXES) na = MAX_AXES;
            int any_activity = 0, any_minmax = 0;
            for (int i = 0; i < na; ++i) {
                int fs;
                int v = axis_val(joy, ev_fd, raw_mode, i, &fs);
                int eps = (int)((long)AXIS_EPS * fs / 32767); if (eps < 4) eps = 4;
                if (v < axstat[i].min) { axstat[i].min = v; any_minmax = 1; need_draw = 2; }
                if (v > axstat[i].max) { axstat[i].max = v; any_minmax = 1; need_draw = 2; }
                if (abs(v - prev_axis[i]) > eps) {
                    need_draw = 2;
                    prev_axis[i] = v;
                    any_activity = 1;
                    snprintf(last_active, sizeof(last_active), "%s", axis_name(i));
                }
            }
            if (raw_mode) {
                int naxc = (na < 4) ? na : 4;   /* consider the stick axes */
                int all_ranged = (naxc > 0);
                for (int i = 0; i < naxc; ++i) {
                    int fs; axis_val(joy, ev_fd, raw_mode, i, &fs);
                    int side = fs * 40 / 100;   /* must reach beyond 40% both sides */
                    if (axstat[i].min > -side || axstat[i].max < side) all_ranged = 0;
                }
                if (any_minmax)
                    stable_cnt = 0;
                else if (any_activity && all_ranged && stable_cnt < 1000000)
                    stable_cnt++;
                int was = cal_ready;
                cal_ready = (all_ranged && stable_cnt >= CAL_STABLE_FRAMES);
                if (cal_ready != was) need_draw = 2;
            } else {
                cal_ready = 0;
            }
        }

        if (need_draw <= 0) {
            SDL_Delay(33);
            continue;
        }
        need_draw--;

        /* --- render --- */
        SDL_SetRenderDrawColor(ren, COL_BG.r, COL_BG.g, COL_BG.b, 255);
        SDL_RenderClear(ren);

        draw_title(ren, 10, 4, COL_TEXT, "JOYSTICK / GAMEPAD TEST");

        /* cal-ready LED (raw mode only): red until calibration values are stable, then green */
        if (raw_mode) {
            SDL_Color led = cal_ready ? COL_ACTIVE : COL_DRIFT;
            filledCircleRGBA(ren, 300, 10, 7, led.r, led.g, led.b, 255);
            aacircleRGBA(ren, 300, 10, 7, 0, 0, 0, 255);
            draw_text(ren, 314, 5, led, "cal ready");
        }

        /* extra field (bottom-right, 3 buttons wide): last active input */
        {
            int fw = 3 * 38 + 2 * 6, fh = 38;
            int fx = WIN_W - 20 - fw;
            int fy = WIN_H - 20 - fh;
            fill_rect(ren, fx, fy, fw, fh, COL_PANEL);
            draw_rect(ren, fx, fy, fw, fh, COL_BORDER);
            draw_text(ren, fx + 8, fy + 6,  COL_DIM,    "Last active:");
            draw_text(ren, fx + 8, fy + 20, COL_ACTIVE, "%s", last_active);
        }

        int njoys = SDL_NumJoysticks();
        if (njoys <= 0 || !joy) {
            draw_text(ren, 10, 40, COL_DRIFT, "No joystick / gamepad found.");
            draw_text(ren, 10, 54, COL_DRIFT, "Please connect a device...");
            SDL_RenderPresent(ren);
            SDL_Delay(100);
            continue;
        }

        const char *name = SDL_JoystickName(joy);
        int num_axes    = SDL_JoystickNumAxes(joy);
        int num_buttons = SDL_JoystickNumButtons(joy);
        int num_hats    = SDL_JoystickNumHats(joy);
        int num_balls   = SDL_JoystickNumBalls(joy);

        int stick_size  = 150;
        int stick_cy    = 140;
        int stick_lx    = 95;
        int stick_rx    = 545;
        int stick_left  = stick_lx - stick_size / 2;   /* 20 */
        int stick_right = stick_rx + stick_size / 2;   /* 620 */
        int center_x    = (stick_left + stick_right) / 2;

        int cols = 14;
        int cell = 38;
        int gap  = 6;
        int shown_btn = num_buttons;
        if (shown_btn > MAX_BUTTONS) shown_btn = MAX_BUTTONS;
        int rows = (shown_btn + cols - 1) / cols;
        int btn_y = WIN_H - 20 - (rows * cell + (rows - 1) * gap);
        int btn_cap_y = btn_y - 16;

        /* ---- sticks (axis pairs 0/1 and 2/3), mode-aware ---- */
        if (num_axes >= 2) {
            int fs0, fs1;
            int v0 = axis_val(joy, ev_fd, raw_mode, 0, &fs0);
            int v1 = axis_val(joy, ev_fd, raw_mode, 1, &fs1);
            int d0 = (int)((long)DRIFT_THRESHOLD * fs0 / 32767); if (d0 < 1) d0 = 1;
            int d1 = (int)((long)DRIFT_THRESHOLD * fs1 / 32767); if (d1 < 1) d1 = 1;
            int d = (abs(v0 - axstat[0].rest) > d0) || (abs(v1 - axstat[1].rest) > d1);
            draw_stick(ren, stick_lx, stick_cy, stick_size,
                       v0 / (float)fs0, v1 / (float)fs1, "Stick L (0/1)", d, squaremap);
        }
        if (num_axes >= 4) {
            int fs2, fs3;
            int v2 = axis_val(joy, ev_fd, raw_mode, 2, &fs2);
            int v3 = axis_val(joy, ev_fd, raw_mode, 3, &fs3);
            int d2 = (int)((long)DRIFT_THRESHOLD * fs2 / 32767); if (d2 < 1) d2 = 1;
            int d3 = (int)((long)DRIFT_THRESHOLD * fs3 / 32767); if (d3 < 1) d3 = 1;
            int d = (abs(v2 - axstat[2].rest) > d2) || (abs(v3 - axstat[3].rest) > d3);
            draw_stick(ren, stick_rx, stick_cy, stick_size,
                       v2 / (float)fs2, v3 / (float)fs3, "Stick R (2/3)", d, squaremap);
        }

        /* ---- hats (right of the sticks) ---- */
        int hatx = 410;
        for (int i = 0; i < num_hats && i < MAX_HATS; ++i) {
            Uint8 hv = SDL_JoystickGetHat(joy, i);
            draw_hat(ren, hatx, stick_cy - 20, 28, hv, i);
            hatx += 150;
        }

        /* ---- device name + properties: centered in the gap between the sticks ---- */
        draw_text_center(ren, center_x, 74, COL_TEXT, "Device %d/%d: %s", current + 1, njoys, name ? name : "(unknown)");
        draw_text_center(ren, center_x, 90, COL_DIM, "Axes:%d Btns:%d Hats:%d Balls:%d", num_axes, num_buttons, num_hats, num_balls);

        /* ---- control help: left-aligned block, centered between the sticks ---- */
        int lx = 184;
        int ly = 120;
        draw_text(ren, lx, ly,       COL_DIM,  "SELECT + R1: next device");
        draw_text(ren, lx, ly + 13,  COL_DIM,  "SELECT + X:  reset cal/min/max");
        draw_text(ren, lx, ly + 26,  COL_DIM,  "SELECT + A:  set center/zero");
        draw_text(ren, lx, ly + 39,  COL_DIM,  "SELECT + Y:  circle/square: %s", squaremap ? "ON" : "OFF");
        draw_text(ren, lx, ly + 52,  COL_DIM,  "SELECT + L1: values: %s", raw_mode ? "RAW" : "CAL");
        draw_text(ren, lx, ly + 65,  COL_DIM,  "SELECT + B:  apply tuning");
        draw_text(ren, lx + 1, ly + 81,  COL_TEXT, "Quit:  SELECT + START + L2");
        if (saved_msg[0])
            draw_text(ren, lx, ly + 91, COL_ACTIVE, "%s", saved_msg);

        /* ---- axes as bars, mode-aware; vertically centered in the middle gap ---- */
        int shown_axes = num_axes;
        if (shown_axes > MAX_AXES) shown_axes = MAX_AXES;
        int axis_step = 22;
        int above_bottom  = stick_cy + stick_size / 2;
        int axes_block_h  = 16 + shown_axes * axis_step;
        int agap = (btn_cap_y - above_bottom - axes_block_h) / 2;
        if (agap < 2) agap = 2;
        int axes_cap_y = above_bottom + agap;
        int bar_x = 20;
        int bar_y = axes_cap_y + 16;
        draw_text(ren, bar_x, axes_cap_y, COL_TEXT,
                  "AXES [%s] (value / min / max / drift):", raw_mode ? "RAW" : "CAL");
        for (int i = 0; i < shown_axes; ++i) {
            int fs;
            int v = axis_val(joy, ev_fd, raw_mode, i, &fs);
            draw_axis_bar(ren, bar_x, bar_y + i * axis_step, 250, i, v, fs, &axstat[i]);
        }

        /* ---- buttons: bottom, aligned to the sticks' horizontal extent ---- */
        draw_text(ren, stick_left, btn_cap_y, COL_TEXT, "BUTTONS (pressed = green):");
        for (int i = 0; i < shown_btn; ++i) {
            int row = i / cols;
            int col = i % cols;
            int x = stick_left + col * ((stick_right - stick_left) - cell) / (cols - 1);
            int y = btn_y + row * (cell + gap);
            int pressed = SDL_JoystickGetButton(joy, i);
            SDL_Color c = pressed ? COL_ACTIVE : COL_PANEL;
            fill_rect(ren, x, y, cell, cell, c);
            draw_rect(ren, x, y, cell, cell, COL_BORDER);
            SDL_Color tc = pressed ? COL_BG : COL_DIM;
            draw_text(ren, x + (i < 10 ? 15 : 11), y + 15, tc, "%d", i);
        }

        SDL_RenderPresent(ren);
        SDL_Delay(33);
    }

    if (ev_fd >= 0) close(ev_fd);
    if (joy) SDL_JoystickClose(joy);
    font_quit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
