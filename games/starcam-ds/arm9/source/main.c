// StarCam for Nintendo DS: a stargazing camera game.
// Top screen: camera viewfinder on the night sky. Bottom screen: touch controls.
// SPDX-License-Identifier: MIT

#include <nds.h>
#include <fat.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "font.h"

#define W 256
#define H 192
#define D2R 0.01745329252f
#define PI_F 3.14159265f

// ---------------------------------------------------------------------------
// Pixel helpers (RGB15 with the alpha bit set for bitmap backgrounds)
// ---------------------------------------------------------------------------
typedef struct { u8 r, g, b; } RGB;
#define C15(r, g, b) ((u16)(((r) >> 3) | (((g) >> 3) << 5) | (((b) >> 3) << 10) | 0x8000))
static inline RGB hexc(u32 hex) { RGB c = { (hex >> 16) & 255, (hex >> 8) & 255, hex & 255 }; return c; }
static inline u16 c15(RGB c) { return C15(c.r, c.g, c.b); }

static u16 topbuf[W * H] __attribute__((aligned(32)));
static u16 botbuf[W * H] __attribute__((aligned(32)));

static inline void px_set(u16 *s, int x, int y, u16 c) { if ((unsigned)x < W && (unsigned)y < H) s[y * W + x] = c; }
// alpha in 0..256
static inline void px_mix(u16 *s, int x, int y, RGB c, int a) {
    if ((unsigned)x >= W || (unsigned)y >= H || a <= 0) return;
    if (a >= 256) { s[y * W + x] = c15(c); return; }
    u16 d = s[y * W + x];
    int dr = (d & 31) << 3, dg = ((d >> 5) & 31) << 3, db = ((d >> 10) & 31) << 3;
    dr += ((c.r - dr) * a) >> 8; dg += ((c.g - dg) * a) >> 8; db += ((c.b - db) * a) >> 8;
    s[y * W + x] = C15(dr, dg, db);
}
// additive, alpha 0..256
static inline void px_add(u16 *s, int x, int y, RGB c, int a) {
    if ((unsigned)x >= W || (unsigned)y >= H || a <= 0) return;
    u16 d = s[y * W + x];
    int dr = ((d & 31) << 3) + ((c.r * a) >> 8), dg = (((d >> 5) & 31) << 3) + ((c.g * a) >> 8), db = (((d >> 10) & 31) << 3) + ((c.b * a) >> 8);
    if (dr > 255) dr = 255; if (dg > 255) dg = 255; if (db > 255) db = 255;
    s[y * W + x] = C15(dr, dg, db);
}

static void fill_rect(u16 *s, int x, int y, int w, int h, RGB c, int a) {
    for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) px_mix(s, i, j, c, a);
}
static void stroke_rect(u16 *s, int x, int y, int w, int h, RGB c, int a) {
    for (int i = x; i < x + w; i++) { px_mix(s, i, y, c, a); px_mix(s, i, y + h - 1, c, a); }
    for (int j = y + 1; j < y + h - 1; j++) { px_mix(s, x, j, c, a); px_mix(s, x + w - 1, j, c, a); }
}
static void round_rect(u16 *s, int x, int y, int w, int h, RGB fill, RGB edge) {
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
        int cx = i < 3 ? 3 - i : (i >= w - 3 ? i - (w - 4) : 0);
        int cy = j < 3 ? 3 - j : (j >= h - 3 ? j - (h - 4) : 0);
        int d = cx * cx + cy * cy;
        if (d > 10) continue;
        bool border = (i == 0 || j == 0 || i == w - 1 || j == h - 1 || d > 4);
        px_mix(s, x + i, y + j, border ? edge : fill, 256);
    }
}
// The DS has no FPU, so everything that runs per pixel uses integer maths.
// isqrt4[n] = sqrt(n) * 16, for n < 65536.
static u16 isqrt4[65536];
static inline int isqrt_q4(int n) { return n < 65536 ? isqrt4[n] : (int)(sqrtf((float)n) * 16); }
// sin/cos in Q14, 1024 steps per turn
static s16 sin1k[1024];
#define SIN1K(i) sin1k[(i) & 1023]
#define COS1K(i) sin1k[((i) + 256) & 1023]

static void fill_circle(u16 *s, float cxf, float cyf, float rf, RGB c, int a) {
    int cx = (int)(cxf * 16), cy = (int)(cyf * 16), r = (int)(rf * 16), r2 = r * r;
    int x0 = (cx - r) >> 4, x1 = (cx + r) >> 4, y0 = (cy - r) >> 4, y1 = (cy + r) >> 4;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > W - 1) x1 = W - 1; if (y1 > H - 1) y1 = H - 1;
    for (int y = y0; y <= y1; y++) {
        int dy = y * 16 + 8 - cy, dy2 = dy * dy;
        if (dy2 > r2) continue;
        for (int x = x0; x <= x1; x++) { int dx = x * 16 + 8 - cx; if (dx * dx + dy2 <= r2) px_mix(s, x, y, c, a); }
    }
}
static void stroke_circle(u16 *s, float cxf, float cyf, float rf, RGB c, int a) {
    int cx = (int)cxf, cy = (int)cyf, r = (int)(rf + 0.5f), x = r, y = 0, err = 1 - r;
    while (x >= y) {
        px_mix(s, cx + x, cy + y, c, a); px_mix(s, cx - x, cy + y, c, a); px_mix(s, cx + x, cy - y, c, a); px_mix(s, cx - x, cy - y, c, a);
        if (x != y) { px_mix(s, cx + y, cy + x, c, a); px_mix(s, cx - y, cy + x, c, a); px_mix(s, cx + y, cy - x, c, a); px_mix(s, cx - y, cy - x, c, a); }
        y++;
        if (err < 0) err += 2 * y + 1; else { x--; err += 2 * (y - x) + 1; }
    }
}
static void fill_tri(u16 *s, float axf, float ayf, float bxf, float byf, float cxf, float cyf, RGB c) {
    int ax = (int)(axf * 16), ay = (int)(ayf * 16), bx = (int)(bxf * 16), by = (int)(byf * 16), cx = (int)(cxf * 16), cy = (int)(cyf * 16);
    int x0 = (ax < bx ? (ax < cx ? ax : cx) : (bx < cx ? bx : cx)) >> 4, x1 = (ax > bx ? (ax > cx ? ax : cx) : (bx > cx ? bx : cx)) >> 4;
    int y0 = (ay < by ? (ay < cy ? ay : cy) : (by < cy ? by : cy)) >> 4, y1 = (ay > by ? (ay > cy ? ay : cy) : (by > cy ? by : cy)) >> 4;
    int area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    if (!area) return;
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        int px = x * 16 + 8, py = y * 16 + 8;
        int w0 = (bx - ax) * (py - ay) - (by - ay) * (px - ax);
        int w1 = (cx - bx) * (py - by) - (cy - by) * (px - bx);
        int w2 = (ax - cx) * (py - cy) - (ay - cy) * (px - cx);
        if (area > 0 ? (w0 >= 0 && w1 >= 0 && w2 >= 0) : (w0 <= 0 && w1 <= 0 && w2 <= 0)) px_mix(s, x, y, c, 256);
    }
}

// Liang-Barsky clip (a few float ops per line), then a 16.16 fixed-point DDA.
static void draw_line(u16 *s, float x0, float y0, float x1, float y1, RGB c, int a, int dash, bool additive) {
    float dx = x1 - x0, dy = y1 - y0, t0 = 0, t1 = 1;
    float p[4] = { -dx, dx, -dy, dy }, q[4] = { x0 + 2, W + 1 - x0, y0 + 2, H + 1 - y0 };
    for (int i = 0; i < 4; i++) {
        if (p[i] == 0) { if (q[i] < 0) return; continue; }
        float r = q[i] / p[i];
        if (p[i] < 0) { if (r > t1) return; if (r > t0) t0 = r; }
        else { if (r < t0) return; if (r < t1) t1 = r; }
    }
    float ax = x0 + t0 * dx, ay = y0 + t0 * dy, bx = x0 + t1 * dx, by = y0 + t1 * dy;
    int n = (int)fmaxf(fabsf(bx - ax), fabsf(by - ay)) + 1;
    int fx = (int)(ax * 65536), fy = (int)(ay * 65536), sx = (int)((bx - ax) * 65536 / n), sy = (int)((by - ay) * 65536 / n);
    for (int i = 0; i <= n; i++, fx += sx, fy += sy) {
        if (dash && ((i / dash) & 1)) continue;
        int x = fx >> 16, y = fy >> 16;
        if (additive) px_add(s, x, y, c, a); else px_mix(s, x, y, c, a);
    }
}

// ---------------------------------------------------------------------------
// Text. Special glyphs: \x01 star, \x02 hollow star, \x03 left, \x04 right,
// \x05 up, \x06 down, \x07 check, \x08 degree, \x0e target ring
// ---------------------------------------------------------------------------
static const u8 special[15][FONT_H] = {
    [1] = { 0x00, 0x04, 0x04, 0x1f, 0x0e, 0x0e, 0x1b, 0x11, 0x00, 0x00 },
    [2] = { 0x00, 0x04, 0x0a, 0x1b, 0x11, 0x0a, 0x15, 0x1b, 0x00, 0x00 },
    [3] = { 0x00, 0x10, 0x18, 0x1c, 0x1e, 0x1c, 0x18, 0x10, 0x00, 0x00 },
    [4] = { 0x00, 0x01, 0x03, 0x07, 0x0f, 0x07, 0x03, 0x01, 0x00, 0x00 },
    [5] = { 0x00, 0x00, 0x04, 0x0e, 0x1f, 0x1f, 0x00, 0x00, 0x00, 0x00 },
    [6] = { 0x00, 0x00, 0x1f, 0x1f, 0x0e, 0x04, 0x00, 0x00, 0x00, 0x00 },
    [7] = { 0x00, 0x00, 0x10, 0x18, 0x0d, 0x07, 0x02, 0x00, 0x00, 0x00 },
    [8] = { 0x06, 0x09, 0x09, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    [14] = { 0x00, 0x0e, 0x11, 0x15, 0x11, 0x0e, 0x00, 0x00, 0x00, 0x00 },
};
// note: bit 0 = leftmost pixel for both the special glyphs and font6
static int text_width(const char *t) { return (int)strlen(t) * FONT_W; }
static void draw_text(u16 *s, int x, int y, const char *t, RGB c, int a) {
    for (; *t; t++, x += FONT_W) {
        unsigned ch = (u8)*t;
        if (ch < 15) {
            for (int j = 0; j < FONT_H; j++) for (int i = 0; i < 5; i++) if (special[ch][j] & (1 << i)) px_mix(s, x + i, y + j, c, a);
        } else if (ch >= 32 && ch < 127) {
            const u8 *g = font6[ch - 32];
            for (int j = 0; j < FONT_H; j++) for (int i = 0; i < FONT_W; i++) if (g[j] & (1 << i)) px_mix(s, x + i, y + j, c, a);
        }
    }
}
static void draw_text_c(u16 *s, int cx, int y, const char *t, RGB c, int a) { draw_text(s, cx - text_width(t) / 2, y, t, c, a); }
static void draw_text_r(u16 *s, int rx, int y, const char *t, RGB c, int a) { draw_text(s, rx - text_width(t), y, t, c, a); }
static int wrap_text(u16 *s, int x, int y, int maxw, int lh, const char *t, RGB c) {
    char line[64]; int len = 0; int maxc = maxw / FONT_W;
    const char *p = t;
    while (*p) {
        const char *wend = p; while (*wend && *wend != ' ') wend++;
        int wl = wend - p;
        if (len && len + 1 + wl > maxc) { line[len] = 0; draw_text(s, x, y, line, c, 256); y += lh; len = 0; }
        if (len) line[len++] = ' ';
        memcpy(line + len, p, wl); len += wl;
        p = *wend ? wend + 1 : wend;
    }
    if (len) { line[len] = 0; draw_text(s, x, y, line, c, 256); y += lh; }
    return y;
}

// ---------------------------------------------------------------------------
// Math helpers
// ---------------------------------------------------------------------------
typedef struct { float x, y, z; } V3;
typedef struct { s16 x, y, z; } V16;
static V3 vec(float ra, float dec) { float a = ra * D2R, d = dec * D2R; V3 v = { cosf(d) * cosf(a), cosf(d) * sinf(a), sinf(d) }; return v; }
static float dot3(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static V16 to16(V3 v) { V16 r = { (s16)(v.x * 16383), (s16)(v.y * 16383), (s16)(v.z * 16383) }; return r; }
static float ang_dist(V3 a, V3 b) { float d = dot3(a, b); if (d > 1) d = 1; if (d < -1) d = -1; return acosf(d) / D2R; }

static u32 seed = 1337;
static float rnd(void) { seed = (u32)(((u64)seed * 16807) % 2147483647); return (seed - 1) / 2147483646.0f; }
static float gauss(void) { float u = 0; while (u == 0) u = rnd(); float v = rnd(); return sqrtf(-2 * logf(u)) * cosf(2 * PI_F * v); }

// ---------------------------------------------------------------------------
// Sky data
// ---------------------------------------------------------------------------
typedef struct { float ra, dec, mag; } StarDef;
typedef struct { u8 a, b; } Link;
typedef struct { int idx; u32 color; } StarColor;
typedef struct {
    const char *id, *name, *hint;
    const StarDef *stars; int nstars;
    const Link *lines; int nlines;
    const StarColor *colors; int ncolors;
    V3 c; V3 sv[13];
} Const;

#define ARR(a) a, (int)(sizeof(a) / sizeof(a[0]))
static const StarDef s_orion[] = { {88.8f,7.4f,0.5f},{81.3f,6.3f,1.6f},{85.2f,-1.9f,1.8f},{84.1f,-1.2f,1.7f},{83.0f,-0.3f,2.2f},{86.9f,-9.7f,2.1f},{78.6f,-8.2f,0.1f},{83.8f,9.9f,3.4f} };
static const Link l_orion[] = { {7,0},{7,1},{0,2},{1,4},{4,3},{3,2},{2,5},{4,6} };
static const StarColor c_orion[] = { {0,0xffb38a},{6,0xbcd4ff} };
static const StarDef s_dipper[] = { {165.9f,61.8f,1.8f},{165.5f,56.4f,2.4f},{178.5f,53.7f,2.4f},{183.9f,57.0f,3.3f},{193.5f,56.0f,1.8f},{200.9f,54.9f,2.2f},{206.9f,49.3f,1.9f} };
static const Link l_dipper[] = { {0,1},{1,2},{2,3},{3,0},{3,4},{4,5},{5,6} };
static const StarDef s_cas[] = { {2.3f,59.1f,2.3f},{10.1f,56.5f,2.2f},{14.2f,60.7f,2.5f},{21.5f,60.2f,2.7f},{28.6f,63.7f,3.4f} };
static const Link l_cas[] = { {0,1},{1,2},{2,3},{3,4} };
static const StarDef s_cyg[] = { {310.4f,45.3f,1.3f},{305.6f,40.3f,2.2f},{311.6f,34.0f,2.5f},{296.2f,45.1f,2.9f},{292.7f,28.0f,3.1f} };
static const Link l_cyg[] = { {0,1},{1,4},{3,1},{1,2} };
static const StarColor c_cyg[] = { {4,0xffd59a} };
static const StarDef s_lyra[] = { {279.2f,38.8f,0.0f},{281.1f,39.7f,4.6f},{281.2f,37.6f,4.3f},{282.5f,33.4f,3.5f},{284.7f,32.7f,3.2f},{283.6f,36.9f,4.3f} };
static const Link l_lyra[] = { {0,1},{0,2},{2,3},{3,4},{4,5},{5,2} };
static const StarColor c_lyra[] = { {0,0xcfe0ff} };
static const StarDef s_sco[] = { {241.4f,-19.8f,2.6f},{240.1f,-22.6f,2.3f},{239.7f,-26.1f,2.9f},{247.4f,-26.4f,1.0f},{249.0f,-28.2f,2.8f},{252.5f,-34.3f,2.3f},{253.1f,-38.0f,3.0f},
    {253.6f,-42.4f,3.6f},{258.0f,-43.2f,3.3f},{264.3f,-43.0f,1.9f},{266.9f,-40.1f,3.0f},{265.6f,-39.0f,2.4f},{263.4f,-37.1f,1.6f} };
static const Link l_sco[] = { {0,1},{1,2},{1,3},{3,4},{4,5},{5,6},{6,7},{7,8},{8,9},{9,10},{10,11},{11,12} };
static const StarColor c_sco[] = { {3,0xff8a6a} };
static const StarDef s_leo[] = { {152.1f,12.0f,1.4f},{151.8f,16.8f,3.5f},{155.0f,19.8f,2.0f},{154.2f,23.4f,3.4f},{148.2f,26.0f,3.9f},{146.5f,23.8f,3.0f},{177.3f,14.6f,2.1f},{168.5f,20.5f,2.6f},{168.6f,15.4f,3.3f} };
static const Link l_leo[] = { {0,1},{1,2},{2,3},{3,4},{4,5},{2,7},{7,6},{6,8},{8,0},{7,8} };
static const StarDef s_crux[] = { {186.6f,-63.1f,0.8f},{187.8f,-57.1f,1.6f},{191.9f,-59.7f,1.3f},{183.8f,-58.7f,2.8f} };
static const Link l_crux[] = { {1,0},{2,3} };
static const StarColor c_crux[] = { {1,0xffc08a} };
static const StarDef s_ple[] = { {56.87f,24.10f,2.9f},{57.29f,24.05f,3.6f},{56.22f,24.11f,3.7f},{56.46f,24.37f,3.9f},{56.58f,23.95f,4.2f},{56.30f,24.47f,4.3f},{57.30f,24.14f,5.0f} };
static const Link l_ple[] = { {2,5},{5,3},{3,0},{0,1},{1,6},{0,4},{4,2} };
static const StarColor c_ple[] = { {0,0xd6e4ff},{1,0xd6e4ff},{2,0xd6e4ff},{3,0xd6e4ff},{4,0xd6e4ff},{5,0xd6e4ff},{6,0xd6e4ff} };

static Const CONST[] = {
    { "orion", "Orion", "The Hunter. Look for three bright stars in a row: his Belt. Red Betelgeuse marks one shoulder.", ARR(s_orion), ARR(l_orion), ARR(c_orion) },
    { "dipper", "Big Dipper", "Seven stars shaped like a ladle, high in the north. The bowl's end stars point to the North Star.", ARR(s_dipper), ARR(l_dipper), NULL, 0 },
    { "cas", "Cassiopeia", "The Queen: a bright W (or M) shape, across the North Star from the Big Dipper.", ARR(s_cas), ARR(l_cas), NULL, 0 },
    { "cygnus", "Cygnus", "The Swan, or Northern Cross. It flies along the Milky Way, bright Deneb at its tail.", ARR(s_cyg), ARR(l_cyg), ARR(c_cyg) },
    { "lyra", "Lyra", "The Lyre: a small parallelogram hanging from brilliant blue-white Vega. Zoom in close!", ARR(s_lyra), ARR(l_lyra), ARR(c_lyra) },
    { "scorpius", "Scorpius", "A long curved tail low in the south, with the red supergiant Antares as its heart.", ARR(s_sco), ARR(l_sco), ARR(c_sco) },
    { "leo", "Leo", "The Lion. A backwards question mark, the Sickle, is his mane; a triangle is his tail.", ARR(s_leo), ARR(l_leo), NULL, 0 },
    { "crux", "Southern Cross", "The smallest constellation, deep in the south. Four bright stars make a kite-shaped cross.", ARR(s_crux), ARR(l_crux), ARR(c_crux) },
    { "pleiades", "Pleiades", "The Seven Sisters: a tiny sparkling blue cluster west of Orion. Zoom in a long way.", ARR(s_ple), ARR(l_ple), ARR(c_ple) },
};
#define NCONST 9

enum { K_MOON, K_JUPITER, K_SATURN, K_COMET };
typedef struct { const char *id, *name, *hint; int kind; float ra, dec, size; V3 v; } Body;
static Body BODIES[] = {
    { "moon", "The Moon", "Our Moon. Zoom in until it fills most of the frame to catch its craters.", K_MOON, 30, 12, 3.0f },
    { "jupiter", "Jupiter", "A steady bright 'star' near Taurus that doesn't twinkle. Zoom all the way in to see its bands.", K_JUPITER, 72, 21, 0.9f },
    { "saturn", "Saturn", "A pale golden dot in a sparse patch of sky south of Pegasus. Zoom to the max for its rings.", K_SATURN, 338, -11, 0.9f },
    { "comet", "Comet", "A visiting comet between Leo and the Big Dipper. Fit its long tail in the frame.", K_COMET, 128, 42, 0.6f },
};
#define NBODIES 4

#define NTARGETS (NCONST + NBODIES + 1)
#define T_METEOR (NCONST + NBODIES)
static const char *METEOR_NAME = "Shooting Star";
static const char *METEOR_HINT = "Meteors streak across the sky at random. Keep watching, and snap one while it's in the frame!";

static const char *tgt_name(int i) { return i < NCONST ? CONST[i].name : i < NCONST + NBODIES ? BODIES[i - NCONST].name : METEOR_NAME; }
static const char *tgt_hint(int i) { return i < NCONST ? CONST[i].hint : i < NCONST + NBODIES ? BODIES[i - NCONST].hint : METEOR_HINT; }
static V3 tgt_vec(int i) { return i < NCONST ? CONST[i].c : BODIES[i - NCONST].v; }

// Background stars
typedef struct { V16 v; u8 mag10; u8 col; u8 ph; u8 sp; } Star;
#define NSTARS 2300
#define NDUST 1500
static Star stars[NSTARS];
static Star dust[NDUST];
static const RGB star_pal[6] = { {255,255,255},{223,232,255},{207,220,255},{255,244,224},{255,225,192},{255,208,176} };

typedef struct { float ra, dec, r, drift; float puff[6][3]; } Cloud;
#define NCLOUDS 9
static Cloud clouds[NCLOUDS];

static void build_sky(void) {
    for (int i = 0; i < NCONST; i++) {
        V3 s = { 0, 0, 0 };
        for (int k = 0; k < CONST[i].nstars; k++) {
            V3 v = vec(CONST[i].stars[k].ra, CONST[i].stars[k].dec);
            CONST[i].sv[k] = v; s.x += v.x; s.y += v.y; s.z += v.z;
        }
        float n = sqrtf(dot3(s, s)); s.x /= n; s.y /= n; s.z /= n;
        CONST[i].c = s;
    }
    for (int i = 0; i < NBODIES; i++) BODIES[i].v = vec(BODIES[i].ra, BODIES[i].dec);

    int n = 0;
    for (; n < 1300; n++) {
        float z = rnd() * 2 - 1, a = rnd() * 2 * PI_F, r = sqrtf(1 - z * z);
        V3 v = { r * cosf(a), r * sinf(a), z };
        stars[n].v = to16(v);
        stars[n].mag10 = (u8)((3.2f + powf(rnd(), 0.5f) * 3.3f) * 10);
        stars[n].col = (u8)(rnd() * 6); stars[n].ph = (u8)(rnd() * 255); stars[n].sp = (u8)(1 + rnd() * 3);
    }
    // Milky Way: galactic north pole at RA 192.86, Dec 27.13
    V3 gp = vec(192.86f, 27.13f);
    V3 gx = { 0 * gp.z - 1 * gp.y, 1 * gp.x - 0 * gp.z, 0 }; // (0,0,1) x gp
    float gl = sqrtf(dot3(gx, gx)); gx.x /= gl; gx.y /= gl; gx.z /= gl;
    V3 gy = { gp.y * gx.z - gp.z * gx.y, gp.z * gx.x - gp.x * gx.z, gp.x * gx.y - gp.y * gx.x };
    int d = 0;
    for (int i = 0; i < NSTARS - 1300 + NDUST; i++) {
        float l = rnd() * 2 * PI_F, b = gauss() * 6 * D2R;
        float cb = cosf(b), sb = sinf(b), cl = cosf(l), sl = sinf(l);
        V3 v = { cb * (cl * gx.x + sl * gy.x) + sb * gp.x, cb * (cl * gx.y + sl * gy.y) + sb * gp.y, cb * (cl * gx.z + sl * gy.z) + sb * gp.z };
        Star st = { to16(v), (u8)((4.8f + rnd() * 2.2f) * 10), (u8)(rnd() * 3), (u8)(rnd() * 255), (u8)(1 + rnd() * 3) };
        if (n < NSTARS) stars[n++] = st; else dust[d++] = st;
    }
    for (int i = 0; i < NCLOUDS; i++) {
        clouds[i].ra = rnd() * 360; clouds[i].dec = -40 + rnd() * 100; clouds[i].r = 4 + rnd() * 7; clouds[i].drift = 0.15f + rnd() * 0.25f;
        for (int k = 0; k < 6; k++) { clouds[i].puff[k][0] = gauss() * 0.6f; clouds[i].puff[k][1] = gauss() * 0.3f; clouds[i].puff[k][2] = 0.5f + rnd() * 0.6f; }
    }
}

// ---------------------------------------------------------------------------
// Game state
// ---------------------------------------------------------------------------
#define MAXPHOTOS 12
typedef struct { u16 *px; char label[24]; u8 stars; u16 id; } Photo;
enum { MODE_CAM, MODE_ALBUM, MODE_VIEW };
static struct {
    float ra, dec, fov, targetFov, vra, vdec;
    bool guide; int target, mode;
    u8 found[NTARGETS];
    Photo photos[MAXPHOTOS]; int nphotos, sel;
    float flash, time;
    bool sd;
} G = { 84, 2, 70, 70, 0, 0, true, 0, MODE_CAM };

typedef struct { char title[40], sub[48]; int stars; bool good; float life, max; } Toast;
static Toast toast_s; static bool toast_on;
static void toast(const char *title, const char *sub, int stars, bool good) {
    snprintf(toast_s.title, sizeof toast_s.title, "%s", title); snprintf(toast_s.sub, sizeof toast_s.sub, "%s", sub);
    toast_s.stars = stars; toast_s.good = good; toast_s.life = 0; toast_s.max = 2.4f; toast_on = true;
}

// ---------------------------------------------------------------------------
// Sound (PSG channels through the ARM7)
// ---------------------------------------------------------------------------
typedef struct { int ch; int frames; } Voice;
static Voice voices[8];
typedef struct { int delay; u16 freq; u8 vol; int dur; bool noise; } Note;
static Note queue[16];
static void sfx(u16 freq, int dur_frames, u8 vol, bool noise, int delay) {
    for (int i = 0; i < 16; i++) if (!queue[i].dur) { Note n = { delay, freq, vol, dur_frames, noise }; queue[i] = n; return; }
}
static void sound_tick(void) {
    for (int i = 0; i < 8; i++) if (voices[i].frames && --voices[i].frames == 0) soundKill(voices[i].ch);
    for (int i = 0; i < 16; i++) {
        if (!queue[i].dur) continue;
        if (queue[i].delay-- > 0) continue;
        int ch = queue[i].noise ? soundPlayNoise(queue[i].freq, queue[i].vol, 64) : soundPlayPSG(DutyCycle_50, queue[i].freq, queue[i].vol, 64);
        for (int v = 0; v < 8; v++) if (!voices[v].frames) { voices[v].ch = ch; voices[v].frames = queue[i].dur; break; }
        queue[i].dur = 0;
    }
}
static void beep(u16 f, int frames) { sfx(f, frames, 40, false, 0); }
static void chime(void) { sfx(660, 10, 50, false, 0); sfx(880, 10, 50, false, 5); sfx(1320, 14, 50, false, 10); }
static void shutter_sound(void) { sfx(12000, 4, 90, true, 0); sfx(9000, 4, 80, true, 6); }

// ---------------------------------------------------------------------------
// Projection (gnomonic)
// ---------------------------------------------------------------------------
static struct { V3 c, e, n; V16 c16, e16, n16; float scale; int scaleI; int zmin; } P;
static void setup_proj(void) {
    float a0 = G.ra * D2R, d0 = G.dec * D2R;
    P.c = vec(G.ra, G.dec);
    V3 e = { -sinf(a0), cosf(a0), 0 }; P.e = e;
    V3 n = { -sinf(d0) * cosf(a0), -sinf(d0) * sinf(a0), cosf(d0) }; P.n = n;
    P.scale = (W / 2) / tanf(G.fov * D2R / 2);
    P.scaleI = (int)P.scale;
    P.c16 = to16(P.c); P.e16 = to16(P.e); P.n16 = to16(P.n);
    float half = atanf(170.0f / P.scale);
    float zm = cosf(half); if (zm < 0.15f) zm = 0.15f;
    P.zmin = (int)(zm * 16383) - 40;
}
static bool proj(V3 v, float *ox, float *oy) {
    float z = dot3(v, P.c);
    if (z < 0.15f) return false;
    *ox = W / 2 - dot3(v, P.e) / z * P.scale;
    *oy = H / 2 - dot3(v, P.n) / z * P.scale;
    return true;
}
static inline bool proj16(V16 v, int *ox, int *oy) {
    int z = (v.x * P.c16.x + v.y * P.c16.y + v.z * P.c16.z) >> 14;
    if (z < P.zmin) return false;
    int x = (v.x * P.e16.x + v.y * P.e16.y + v.z * P.e16.z) >> 14;
    int y = (v.x * P.n16.x + v.y * P.n16.y + v.z * P.n16.z) >> 14;
    *ox = W / 2 - (x * P.scaleI) / z;
    *oy = H / 2 - (y * P.scaleI) / z;
    return true;
}

// ---------------------------------------------------------------------------
// Meteors
// ---------------------------------------------------------------------------
typedef struct { float x, y, vx, vy, life, max; bool on; } Meteor;
static Meteor meteors[3];
static float next_meteor = 3;
static void spawn_meteor(void) {
    for (int i = 0; i < 3; i++) if (!meteors[i].on) {
        float r = (float)rand() / RAND_MAX;
        float ang = (r * 0.8f + 0.3f) * (rand() & 1 ? 1 : -1) + PI_F / 2;
        Meteor m = { 30 + (float)rand() / RAND_MAX * 196, 10 + (float)rand() / RAND_MAX * 60,
                     cosf(ang) * 260 * (rand() & 1 ? 1 : -1), fabsf(sinf(ang)) * 140, 0, 0.55f + (float)rand() / RAND_MAX * 0.4f, true };
        meteors[i] = m; return;
    }
}

// ---------------------------------------------------------------------------
// Drawing the sky
// ---------------------------------------------------------------------------
static s8 sin_tab[256];
static int g_zoomboost = 256; // Q8, set once per frame

// mag10 = magnitude * 10, tw = twinkle 0..256
static void draw_star(u16 *s, int x, int y, int mag10, RGB c, int tw) {
    if (x < -10 || x > W + 10 || y < -10 || y > H + 10) return;
    int b = (68 - mag10) * 256 / 65; if (b < 0) b = 0; if (b > 256) b = 256;   // brightness Q8
    int r = ((90 + (b * 410 >> 8)) * g_zoomboost) >> 8;                       // radius Q8
    int a = ((64 + (b * 230 >> 8)) * tw) >> 8; if (a > 256) a = 256;
    px_add(s, x, y, c, a);
    if (r < 230) return;
    int ri = (r + 128) >> 8, r4 = r >> 4; // r4: radius in Q4
    for (int j = -ri; j <= ri; j++) for (int i = -ri; i <= ri; i++) {
        if (!i && !j) continue;
        int d = isqrt4[i * i + j * j];                // Q4
        if (d > r4) continue;
        int k = 282 - (d * 4096) / (r4 * 16 + 154);  // 1.1 - d/(r+0.6), Q8
        px_add(s, x + i, y + j, c, (a * k) >> 8);
    }
    if (b > 141) {
        int g4 = (r4 * 666) >> 8, gr = g4 >> 4;
        if (g4 <= 0) return;
        for (int j = -gr; j <= gr; j++) for (int i = -gr; i <= gr; i++) {
            int d = isqrt4[i * i + j * j];
            if (d > r4 && d <= g4) px_add(s, x + i, y + j, c, (((a * 56) >> 8) * (256 - d * 256 / g4)) >> 8);
        }
        if (b > 192) {
            int sl = (r * 3584 >> 8) >> 10; // r * 3.5 in pixels
            for (int k = ri + 1; k <= sl; k++) {
                int aa = ((a * 115) >> 8) * (256 - k * 256 / (sl + 1)) >> 8;
                px_add(s, x + k, y, c, aa); px_add(s, x - k, y, c, aa); px_add(s, x, y + k, c, aa); px_add(s, x, y - k, c, aa);
            }
        }
    }
}

static float body_radius(const Body *b) { float r = (b->size * D2R / 2) * P.scale; return r < 0.8f ? 0.8f : r; }

// Rotated ellipse arc for Saturn's rings, using the integer sine table.
static void ring_arc(u16 *s, int cx, int cy, int a, int b, int from, int to, u16 col) {
    const int ct = 15411, st = -5627; // cos/sin(-0.35) in Q14
    int n = (a * 6) + 16; if (n > 1024) n = 1024;
    for (int i = 0; i <= n; i++) {
        int ang = from + (to - from) * i / n;
        int ex = (COS1K(ang) * a) >> 14, ey = (SIN1K(ang) * b) >> 14;
        px_set(s, cx + ((ex * ct - ey * st) >> 14), cy + ((ex * st + ey * ct) >> 14), col);
    }
}

static void draw_body(u16 *s, const Body *b, float t) {
    (void)t;
    float xf, yf; if (!proj(b->v, &xf, &yf)) return;
    float rad = body_radius(b);
    if (xf < -rad * 25 || xf > W + rad * 25 || yf < -rad * 25 || yf > H + rad * 25) return;
    int x = (int)xf, y = (int)yf;
    // Normalised coordinates: 256 = one radius. Stepped per pixel in Q12 to avoid divisions.
    int step = (int)(256.0f / rad * 4096);
    if (b->kind == K_MOON) {
        int ir = (int)rad;
        int gr = ir < 85 ? ir * 3 : ir + 1; // skip the glow when the Moon is huge
        int x0 = x - gr, x1 = x + gr, y0 = y - gr, y1 = y + gr;
        if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 >= W) x1 = W - 1; if (y1 >= H) y1 = H - 1;
        RGB glow = hexc(0xfff8e0), shadow = hexc(0x0a0d22);
        u16 base = c15(hexc(0xece6d0)), cr1 = c15(hexc(0xc9c2aa)), cr2 = c15(hexc(0xb5ad94));
        static const s16 craters[7][3] = { {-77,-51,72},{64,-90,46},{26,64,56},{-90,90,31},{115,38,26},{-13,-128,20},{90,141,23} };
        static const s16 craters2[5][3] = { {-64,-38,20},{77,-77,13},{31,77,18},{-26,154,10},{154,-26,13} };
        int r2px = (int)(rad * rad);
        for (int py = y0; py <= y1; py++) {
            int dy = (int)((py + 0.5f - yf) * 256.0f / rad);
            int dyp = py - y, dy2 = dy * dy;
            int dxq = (int)((x0 + 0.5f - xf) * 256.0f / rad * 4096);
            for (int pxx = x0; pxx <= x1; pxx++, dxq += step) {
                int dx = dxq >> 12, d2 = dx * dx + dy2; // Q16, 65536 = edge
                if (d2 > 65536) {
                    int dxp = pxx - x, dp2 = dxp * dxp + dyp * dyp;
                    if (gr > ir + 1 && dp2 < 9 * r2px && dp2 < 65536) {
                        int d4 = isqrt4[dp2];                       // pixels, Q4
                        int k = 256 - ((d4 - ir * 16) * 128) / (ir * 16 + 1); // 1 - (d-1)/2, Q8
                        if (k > 0) px_add(s, pxx, py, glow, (85 * k >> 8) * k >> 8);
                    }
                    continue;
                }
                u16 c = base;
                for (int k = 0; k < 7; k++) { int ex = dx - craters[k][0], ey = dy - craters[k][1]; if (ex * ex + ey * ey < craters[k][2] * craters[k][2]) c = cr1; }
                for (int k = 0; k < 5; k++) { int ex = dx - craters2[k][0], ey = dy - craters2[k][1]; if (ex * ex + ey * ey < craters2[k][2] * craters2[k][2]) c = cr2; }
                s[py * W + pxx] = c;
                int sx = ((dx + 141) * 341) >> 8, sy = (dy * 244) >> 8; // waxing gibbous shadow
                if (sx * sx + sy * sy < 65536) px_mix(s, pxx, py, shadow, 222);
            }
        }
    } else if (b->kind == K_JUPITER || b->kind == K_SATURN) {
        bool J = b->kind == K_JUPITER;
        if (rad < 2) { draw_star(s, x, y, J ? -20 : 6, J ? hexc(0xfff3dc) : hexc(0xffe6a8), 256); return; }
        u16 ring = c15(hexc(0xd9c38c));
        int ir = (int)rad;
        if (!J) for (int k = 0; k < 7; k++) { int ra = ir * (185 + k * 8) / 100; ring_arc(s, x, y, ra, ra * 26 / 100, 512, 1024, ring); }
        RGB body = J ? hexc(0xe9d6b6) : hexc(0xe6cf96);
        static const s16 jb[4][2] = { {-141,72},{-51,92},{51,61},{128,51} };
        static const u32 jc[4] = { 0xb98e66, 0xa7764f, 0xc19a72, 0x9c7350 };
        static const s16 sbnd[2][2] = { {-77,77},{64,61} };
        static const u32 sc[2] = { 0xc9ae72, 0xbba066 };
        int x0 = x - ir - 1, x1 = x + ir + 1, y0 = y - ir - 1, y1 = y + ir + 1;
        if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 >= W) x1 = W - 1; if (y1 >= H) y1 = H - 1;
        for (int py = y0; py <= y1; py++) {
            int dy = (int)((py + 0.5f - yf) * 256.0f / rad);
            int dxq = (int)((x0 + 0.5f - xf) * 256.0f / rad * 4096);
            for (int pxx = x0; pxx <= x1; pxx++, dxq += step) {
                int dx = dxq >> 12, d2 = dx * dx + dy * dy;
                if (d2 > 65536) continue;
                RGB c = body;
                int nb = J ? 4 : 2;
                for (int k = 0; k < nb; k++) { int o = J ? jb[k][0] : sbnd[k][0], h = J ? jb[k][1] : sbnd[k][1]; if (dy >= o && dy < o + h) c = hexc(J ? jc[k] : sc[k]); }
                if (J) { int ex = ((dx - 77) * 256) / 46, ey = ((dy - 90) * 256) / 26; if (ex * ex + ey * ey < 65536) c = hexc(0xc0573c); }
                int l = 256 - ((90 * (d2 >> 8)) >> 8); // limb darkening
                s[py * W + pxx] = C15(c.r * l >> 8, c.g * l >> 8, c.b * l >> 8);
            }
        }
        if (!J) for (int k = 0; k < 7; k++) { int ra = ir * (185 + k * 8) / 100; ring_arc(s, x, y, ra, ra * 26 / 100, 0, 512, ring); }
        else {
            static const s16 moons[4][2] = { {-320,10},{-190,-5},{240,5},{410,-10} };
            for (int k = 0; k < 4; k++) { int mx = x + moons[k][0] * ir / 100, my = y + moons[k][1] * ir / 100;
                px_set(s, mx, my, C15(255, 255, 255)); px_add(s, mx + 1, my, hexc(0xffffff), 120); px_add(s, mx, my + 1, hexc(0xffffff), 120); }
        }
    } else { // comet: tail points up and to the right
        int len = (int)(rad * 22);
        const int ux = 211, uy = -145, nx = 145, ny = 211; // cos/sin(-0.6) in Q8 and the normal
        RGB tail = hexc(0xbfffe8), ion = hexc(0x9fc3ff);
        int ir = (int)rad; if (ir < 1) ir = 1;
        for (int sd = 0; sd < len; sd++) {
            int cxp = x + (ux * sd >> 8), cyp = y + (uy * sd >> 8);
            int wv = ir + (len * 38 >> 8) * sd / len; // rad + len*0.15*k
            if (cxp < -wv || cxp > W + wv || cyp < -wv || cyp > H + wv) continue;
            int a = 150 * (len - sd) / len;
            for (int w = -wv * 7 / 10; w <= wv; w++) {
                int edge = 256 - (w < 0 ? -w : w) * 256 / (wv + 1);
                px_add(s, cxp + (nx * w >> 8), cyp + (ny * w >> 8), tail, (a * edge >> 8) * 160 >> 8);
            }
        }
        draw_line(s, xf, yf, xf + 0.743f * len, yf - 0.669f * len, ion, 60, 0, true);
        int hr = (int)(rad * 2.5f) + 1, hr4 = hr * 16;
        RGB white = hexc(0xffffff), head = hexc(0xc8fff0);
        for (int j = -hr; j <= hr; j++) for (int i = -hr; i <= hr; i++) {
            int n = i * i + j * j; if (n >= 65536) continue;
            int d = isqrt4[n] * 256 / hr4; if (d > 256) continue;
            px_add(s, x + i, y + j, d < 77 ? white : head, 256 - d);
        }
    }
}

static u8 cloudmap[48][64];
static void draw_sky(u16 *s, float t) {
    setup_proj();
    float zb = 1 + (40 - G.fov) / 60; if (zb > 1.6f) zb = 1.6f; if (zb < 1) zb = 1;
    g_zoomboost = (int)(zb * 256);
    // background gradient
    float low = (-G.dec + 10) / 80; if (low < 0) low = 0; if (low > 1) low = 1;
    int rb = (int)(low > 0 ? 8 + low * 14 : 7), gb = (int)(low > 0 ? 10 + low * 12 : 10), bb = (int)(low > 0 ? 28 + low * 14 : 28);
    for (int y = 0; y < H; y++) {
        // 2x2 ordered dither hides the 5-bit colour banding of the dark gradient
        int r = 3 + (rb - 3) * y / H, g = 4 + (gb - 4) * y / H, b = 12 + (bb - 12) * y / H, o = (y & 1) ? 6 : 2;
        u16 ca = C15(r + o, g + o, b + o), cb = C15(r + 4 - o + 4, g + 4 - o + 4, b + 4 - o + 4);
        u32 c2 = ca | ((u32)cb << 16);
        u32 *row = (u32 *)&s[y * W];
        for (int x = 0; x < W / 2; x++) row[x] = c2;
    }
    // milky way dust
    RGB mw = hexc(0x9fb0ff);
    for (int i = 0; i < NDUST; i++) {
        int x, y; if (!proj16(dust[i].v, &x, &y)) continue;
        if (x < 0 || x >= W || y < 0 || y >= H) continue;
        int a = 18 + (69 - dust[i].mag10) * 128 / 100; // (0.07 + (6.9 - mag) * 0.05) * 256
        px_add(s, x, y, mw, a); px_add(s, x + 1, y, mw, a); px_add(s, x, y + 1, mw, a); px_add(s, x + 1, y + 1, mw, a);
    }
    // stars
    float limit = 5.2f + fminf(1.6f, (70 - G.fov) / 30); int lim10 = (int)(limit * 10);
    int ti = (int)(t * 40);
    for (int i = 0; i < NSTARS; i++) {
        if (stars[i].mag10 > lim10) continue;
        int x, y; if (!proj16(stars[i].v, &x, &y)) continue;
        if (x < -2 || x > W + 2 || y < -2 || y > H + 2) continue;
        int tw = 205 + (51 * sin_tab[(stars[i].ph + ti * stars[i].sp) & 255]) / 127;
        draw_star(s, x, y, stars[i].mag10, star_pal[stars[i].col], tw);
    }
    // constellations
    RGB gold = hexc(0xffd36b), guide = hexc(0x7e95ff), white = hexc(0xf4f7ff);
    int t60 = (int)(t * 60);
    for (int i = 0; i < NCONST; i++) {
        Const *c = &CONST[i];
        float px[13], py[13]; bool ok[13];
        for (int k = 0; k < c->nstars; k++) ok[k] = proj(c->sv[k], &px[k], &py[k]);
        bool done = G.found[i] != 0;
        if (G.guide || done) {
            for (int k = 0; k < c->nlines; k++) {
                int a = c->lines[k].a, b = c->lines[k].b;
                if (!ok[a] || !ok[b]) continue;
                draw_line(s, px[a], py[a], px[b], py[b], done ? gold : guide, done ? 140 : 80, done ? 0 : 2, false);
            }
        }
        for (int k = 0; k < c->nstars; k++) {
            if (!ok[k] || px[k] < -4 || px[k] > W + 4 || py[k] < -4 || py[k] > H + 4) continue;
            RGB col = white;
            for (int q = 0; q < c->ncolors; q++) if (c->colors[q].idx == k) col = hexc(c->colors[q].color);
            int tw = 218 + (38 * sin_tab[(t60 + k * 70) & 255]) / 127;
            draw_star(s, (int)px[k], (int)py[k], (int)(c->stars[k].mag * 10), col, tw);
        }
        if (done) {
            float cx, cy;
            if (proj(c->c, &cx, &cy) && cx > -60 && cx < W + 60 && cy > 0 && cy < H - 12) {
                char up[24]; int n = 0; for (const char *p = c->name; *p && n < 23; p++) up[n++] = (*p >= 'a' && *p <= 'z') ? *p - 32 : *p; up[n] = 0;
                draw_text_c(s, (int)cx, (int)cy + 10, up, gold, 170);
            }
        }
    }
    for (int i = 0; i < NBODIES; i++) draw_body(s, &BODIES[i], t);
    // meteors
    for (int i = 0; i < 3; i++) if (meteors[i].on) {
        Meteor *m = &meteors[i];
        float k = m->life / m->max, hx = m->x + m->vx * m->life, hy = m->y + m->vy * m->life;
        float tx = hx - m->vx * 0.12f, ty = hy - m->vy * 0.12f;
        int n = (int)fmaxf(fabsf(tx - hx), fabsf(ty - hy)) + 1;
        int fx = (int)(hx * 256), fy = (int)(hy * 256), sx = (int)((tx - hx) * 256) / n, sy = (int)((ty - hy) * 256) / n;
        int a0 = (int)(256 * (1 - k));
        RGB white = hexc(0xffffff), blue = hexc(0xa0c8ff);
        for (int j = 0; j <= n; j++, fx += sx, fy += sy)
            px_add(s, fx >> 8, fy >> 8, j * 10 < n * 3 ? white : blue, a0 * (n - j) / n);
    }
    // clouds (via a 4x4-pixel cell coverage map)
    memset(cloudmap, 0, sizeof cloudmap);
    bool any = false;
    for (int i = 0; i < NCLOUDS; i++) {
        float cx, cy; if (!proj(vec(clouds[i].ra, clouds[i].dec), &cx, &cy)) continue;
        float r = clouds[i].r * D2R * P.scale;
        if (cx < -r * 3 || cx > W + r * 3 || cy < -r * 3 || cy > H + r * 3) continue;
        for (int k = 0; k < 6; k++) {
            int gx = (int)(cx + clouds[i].puff[k][0] * r), gy = (int)(cy + clouds[i].puff[k][1] * r), rr = (int)(r * clouds[i].puff[k][2]);
            if (rr < 1) continue;
            int c0 = (gx - rr) >> 2, c1 = ((gx + rr) >> 2) + 1, r0 = (gy - rr) >> 2, r1 = ((gy + rr) >> 2) + 1;
            if (c0 < 0) c0 = 0; if (r0 < 0) r0 = 0; if (c1 > 63) c1 = 63; if (r1 > 47) r1 = 47;
            int rr4 = rr * 16;
            for (int cyc = r0; cyc <= r1; cyc++) for (int cxc = c0; cxc <= c1; cxc++) {
                int dx = cxc * 4 + 2 - gx, dy = cyc * 4 + 2 - gy, d4 = isqrt_q4(dx * dx + dy * dy);
                if (d4 >= rr4) continue;
                int a = 190 - 190 * d4 / rr4;
                int cur = cloudmap[cyc][cxc]; cur = cur + a - ((cur * a) >> 8); if (cur > 235) cur = 235;
                cloudmap[cyc][cxc] = (u8)cur; any = true;
            }
        }
    }
    if (any) {
        RGB cc = hexc(0x20243e);
        for (int cy = 0; cy < 48; cy++) for (int cx = 0; cx < 64; cx++) {
            int a = cloudmap[cy][cx]; if (!a) continue;
            for (int y = cy * 4; y < cy * 4 + 4; y++) for (int x = cx * 4; x < cx * 4 + 4; x++) px_mix(s, x, y, cc, a);
        }
    }
}

static float cloud_cover(V3 v) {
    float cov = 0;
    for (int i = 0; i < NCLOUDS; i++) {
        float d = ang_dist(v, vec(clouds[i].ra, clouds[i].dec));
        if (d < clouds[i].r * 0.9f) { float c = 1 - d / (clouds[i].r * 0.9f); if (c > cov) cov = c; }
    }
    return cov;
}

static void draw_hud(u16 *s, float t) {
    RGB w = hexc(0xffffff);
    const int m = 8, L = 12;
    int corners[4][4] = { {m, m, 1, 1}, {W - m - 1, m, -1, 1}, {m, H - m - 1, 1, -1}, {W - m - 1, H - m - 1, -1, -1} };
    for (int k = 0; k < 4; k++) {
        for (int i = 0; i < L; i++) { px_mix(s, corners[k][0] + i * corners[k][2], corners[k][1], w, 210); px_mix(s, corners[k][0], corners[k][1] + i * corners[k][3], w, 210); }
    }
    for (int i = 2; i <= 6; i++) { px_mix(s, W / 2 - i, H / 2, w, 140); px_mix(s, W / 2 + i, H / 2, w, 140); px_mix(s, W / 2, H / 2 - i, w, 140); px_mix(s, W / 2, H / 2 + i, w, 140); }
    char buf[48];
    float raH = G.ra / 15; int h = (int)raH, mn = (int)((raH - h) * 60);
    snprintf(buf, sizeof buf, "RA %02dh%02dm DEC %c%.1f\x08", h, mn, G.dec >= 0 ? '+' : '-', fabsf(G.dec));
    draw_text(s, 13, 13, buf, w, 210);
    snprintf(buf, sizeof buf, "x%.1f FOV %d\x08", 70 / G.fov, (int)(G.fov + 0.5f));
    draw_text_r(s, W - 13, 13, buf, w, 210);
    if (((int)(t * 2)) & 1) fill_circle(s, W - 17, H - 17, 2.2f, hexc(0xff5a6a), 256);
    snprintf(buf, sizeof buf, "%d/%d", G.nphotos, MAXPHOTOS);
    draw_text_r(s, W - 22, H - 22, buf, w, 180);
    snprintf(buf, sizeof buf, "\x0e %s%s", tgt_name(G.target), G.found[G.target] ? " \x07" : "");
    draw_text(s, 13, H - 22, buf, w, 200);
}

static void draw_stars_row(u16 *s, int cx, int y, int n, RGB c) {
    char buf[4] = { 0 };
    for (int i = 0; i < 3; i++) buf[i] = i < n ? '\x01' : '\x02';
    draw_text_c(s, cx, y, buf, c, 256);
}

static void draw_toast(u16 *s) {
    if (!toast_on) return;
    Toast *ts = &toast_s;
    float a = fminf(1, fminf(ts->life * 3, (ts->max - ts->life) * 3)); if (a < 0) a = 0;
    int A = (int)(a * 256);
    RGB edge = ts->good ? hexc(0xffd36b) : hexc(0x7e95ff);
    fill_rect(s, 24, H / 2 - 26, W - 48, 52, hexc(0x0b0e22), (int)(A * 0.9f));
    stroke_rect(s, 24, H / 2 - 26, W - 48, 52, edge, A);
    RGB tc = ts->good ? hexc(0xffd36b) : hexc(0xcfd6ff);
    draw_text_c(s, W / 2, H / 2 - 22, ts->title, tc, A);
    draw_text_c(s, W / 2 + 1, H / 2 - 22, ts->title, tc, A); // faux bold
    draw_text_c(s, W / 2, H / 2 - 9, ts->sub, hexc(0xe8ecff), A);
    if (ts->stars >= 0) {
        char buf[4] = { 0 };
        for (int i = 0; i < 3; i++) buf[i] = i < ts->stars ? '\x01' : '\x02';
        draw_text_c(s, W / 2, H / 2 + 5, buf, hexc(0xffd36b), A);
    }
}

// ---------------------------------------------------------------------------
// Photo evaluation
// ---------------------------------------------------------------------------
typedef struct { int t; int stars; const char *tip; } Result;
static bool in_frame(float x, float y, float m) { return x > m && x < W - m && y > m && y < H - m; }

static int evaluate(Result *out) {
    setup_proj();
    int n = 0;
    for (int i = 0; i < NCONST; i++) {
        Const *c = &CONST[i];
        float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f; bool all = true;
        for (int k = 0; k < c->nstars; k++) {
            float x, y;
            if (!proj(c->sv[k], &x, &y) || !in_frame(x, y, 6)) { all = false; break; }
            minx = fminf(minx, x); maxx = fmaxf(maxx, x); miny = fminf(miny, y); maxy = fmaxf(maxy, y);
        }
        if (!all) continue;
        float fill = fmaxf((maxx - minx) / W, (maxy - miny) / H);
        int st = fill >= 0.72f ? 3 : fill >= 0.52f ? 2 : fill >= 0.32f ? 1 : 0;
        if (st && cloud_cover(c->c) > 0.35f) st--;
        out[n].t = i; out[n].stars = st; out[n].tip = st ? "" : "Too small: zoom in!"; n++;
    }
    for (int i = 0; i < NBODIES; i++) {
        Body *b = &BODIES[i];
        float x, y; if (!proj(b->v, &x, &y) || !in_frame(x, y, 2)) continue;
        float rad = (b->size * D2R / 2) * P.scale, off = sqrtf((x - W / 2) * (x - W / 2) + (y - H / 2) * (y - H / 2));
        int st = 0; const char *tip = "";
        if (b->kind == K_COMET) {
            float len = rad * 22, tx = x + cosf(-0.6f) * len, ty = y + sinf(-0.6f) * len;
            bool tailIn = tx > 0 && tx < W && ty > 0 && ty < H;
            float fill = len / W;
            st = !tailIn ? 0 : fill > 0.55f ? 3 : fill > 0.35f ? 2 : fill > 0.18f ? 1 : 0;
            tip = !tailIn ? "Fit the whole tail in!" : "Zoom in on the comet!";
        } else {
            float frac = rad * 2 / H;
            if (off > H * 0.3f) tip = "Center it in the frame!";
            else if (b->kind == K_MOON) st = frac > 0.7f ? 3 : frac > 0.45f ? 2 : frac > 0.22f ? 1 : 0;
            else st = frac > 0.16f ? 3 : frac > 0.10f ? 2 : frac > 0.05f ? 1 : 0;
            if (!*tip) tip = "Zoom in closer!";
        }
        if (st && cloud_cover(b->v) > 0.35f) st--;
        out[n].t = NCONST + i; out[n].stars = st; out[n].tip = tip; n++;
    }
    for (int i = 0; i < 3; i++) if (meteors[i].on) {
        float hx = meteors[i].x + meteors[i].vx * meteors[i].life, hy = meteors[i].y + meteors[i].vy * meteors[i].life;
        if (hx > 0 && hx < W && hy > 0 && hy < H) { out[n].t = T_METEOR; out[n].stars = meteors[i].life / meteors[i].max < 0.5f ? 3 : 2; out[n].tip = ""; n++; break; }
    }
    // sort: current target first, then by stars
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) {
        int si = (out[i].t == G.target) * 10 + out[i].stars, sj = (out[j].t == G.target) * 10 + out[j].stars;
        if (sj > si) { Result tmp = out[i]; out[i] = out[j]; out[j] = tmp; }
    }
    return n;
}

// ---------------------------------------------------------------------------
// Saving (SD card / flashcart via FAT). Files live next to the ROM.
// ---------------------------------------------------------------------------
#define SAVE_FILE "starcam.sav"
#define PHOTO_DIR "starcam_photos"
typedef struct { char magic[4]; u8 found[NTARGETS]; u8 nphotos; char labels[MAXPHOTOS][24]; u8 pstars[MAXPHOTOS]; u16 ids[MAXPHOTOS]; u16 next_id; } SaveData;
static u16 next_photo_id = 1;
static void photo_path(char *buf, int n, u16 id) { snprintf(buf, n, PHOTO_DIR "/starcam_%04u.bmp", id); }

static void write_bmp(const char *path, const u16 *px) {
    FILE *f = fopen(path, "wb"); if (!f) return;
    static u8 row[W * 3];
    u32 size = 54 + W * H * 3;
    u8 hdr[54] = { 'B', 'M' };
    hdr[2] = size; hdr[3] = size >> 8; hdr[4] = size >> 16; hdr[10] = 54; hdr[14] = 40;
    hdr[18] = W & 255; hdr[19] = W >> 8; hdr[22] = H; hdr[26] = 1; hdr[28] = 24;
    fwrite(hdr, 1, 54, f);
    for (int y = H - 1; y >= 0; y--) {
        for (int x = 0; x < W; x++) { u16 c = px[y * W + x]; row[x * 3] = ((c >> 10) & 31) << 3; row[x * 3 + 1] = ((c >> 5) & 31) << 3; row[x * 3 + 2] = (c & 31) << 3; }
        fwrite(row, 1, sizeof row, f);
    }
    fclose(f);
}
static bool read_bmp(const char *path, u16 *px) {
    FILE *f = fopen(path, "rb"); if (!f) return false;
    static u8 row[W * 3];
    fseek(f, 54, SEEK_SET);
    for (int y = H - 1; y >= 0; y--) {
        if (fread(row, 1, sizeof row, f) != sizeof row) { fclose(f); return false; }
        for (int x = 0; x < W; x++) px[y * W + x] = C15(row[x * 3 + 2], row[x * 3 + 1], row[x * 3]);
    }
    fclose(f); return true;
}
// Saves progress and the album index. Each photo is its own BMP file, written once
// when it is taken (save_photo) and removed when it is deleted (remove_photo).
static void save_all(void) {
    if (!G.sd) return;
    SaveData sd; memset(&sd, 0, sizeof sd);
    memcpy(sd.magic, "SCM2", 4); memcpy(sd.found, G.found, sizeof G.found); sd.nphotos = G.nphotos; sd.next_id = next_photo_id;
    for (int i = 0; i < G.nphotos; i++) { memcpy(sd.labels[i], G.photos[i].label, 24); sd.pstars[i] = G.photos[i].stars; sd.ids[i] = G.photos[i].id; }
    FILE *f = fopen(SAVE_FILE, "wb"); if (f) { fwrite(&sd, 1, sizeof sd, f); fclose(f); }
}
static void save_photo(const Photo *p) {
    if (!G.sd) return;
    mkdir(PHOTO_DIR, 0777);
    char path[64]; photo_path(path, sizeof path, p->id); write_bmp(path, p->px);
}
static void remove_photo(const Photo *p) {
    if (!G.sd) return;
    char path[64]; photo_path(path, sizeof path, p->id); remove(path);
}
static void load_all(void) {
    if (!G.sd) return;
    SaveData sd; FILE *f = fopen(SAVE_FILE, "rb"); if (!f) return;
    bool ok = fread(&sd, 1, sizeof sd, f) == sizeof sd; fclose(f);
    if (!ok || memcmp(sd.magic, "SCM2", 4)) return;
    memcpy(G.found, sd.found, sizeof G.found);
    next_photo_id = sd.next_id ? sd.next_id : 1;
    char path[64];
    for (int i = 0; i < sd.nphotos && i < MAXPHOTOS; i++) {
        u16 *px = malloc(W * H * 2); if (!px) break;
        photo_path(path, sizeof path, sd.ids[i]);
        if (!read_bmp(path, px)) { free(px); continue; }
        Photo *p = &G.photos[G.nphotos++]; p->px = px; memcpy(p->label, sd.labels[i], 24); p->label[23] = 0; p->stars = sd.pstars[i]; p->id = sd.ids[i];
    }
}

// ---------------------------------------------------------------------------
// Taking a photo
// ---------------------------------------------------------------------------
static int count_found(void) { int n = 0; for (int i = 0; i < NTARGETS; i++) n += G.found[i] != 0; return n; }
static int count_stars(void) { int n = 0; for (int i = 0; i < NTARGETS; i++) n += G.found[i]; return n; }
static bool pending_shot;
static float complete_timer = -1;

static void take_photo(void) {
    if (G.mode != MODE_CAM || G.flash > 0) return;
    pending_shot = true; shutter_sound(); G.flash = 1;
}
static void finish_photo(const u16 *frame) {
    Result res[20]; int n = evaluate(res);
    int best = -1; for (int i = 0; i < n; i++) if (res[i].stars > 0) { best = i; break; }
    char label[24] = "Night sky"; int stars = 0; char title[40], sub[48];
    if (best >= 0) {
        int t = res[best].t; stars = res[best].stars;
        snprintf(label, sizeof label, "%s", tgt_name(t));
        int prev = G.found[t];
        if (!prev) {
            chime(); char up[32]; int k = 0; for (const char *p = tgt_name(t); *p && k < 31; p++) up[k++] = (*p >= 'a' && *p <= 'z') ? *p - 32 : *p; up[k] = 0;
            snprintf(title, sizeof title, "NEW: %s!", up); toast(title, "Added to your star log", stars, true);
        } else if (stars > prev) { chime(); snprintf(title, sizeof title, "%s: better shot!", tgt_name(t)); toast(title, "Rating improved", stars, true); }
        else toast(tgt_name(t), prev >= 3 ? "Already a perfect shot" : "Logged. Try for a better rating!", stars, false);
        if (stars > prev) G.found[t] = stars;
        if (!prev && G.target == t) {
            int nxt = -1;
            for (int i = 1; i <= NTARGETS; i++) { int j = (G.target + i) % NTARGETS; if (!G.found[j]) { nxt = j; break; } }
            if (nxt >= 0) G.target = nxt;
        }
        if (count_found() == NTARGETS && !prev) complete_timer = 2.6f;
    } else {
        if (n) { snprintf(sub, sizeof sub, "%s", res[0].tip); toast(tgt_name(res[0].t), sub, -1, false); }
        else toast("Nothing logged", "Find a target in the frame", -1, false);
        sfx(220, 9, 40, false, 0);
    }
    // keep the photo
    if (G.nphotos == MAXPHOTOS) { remove_photo(&G.photos[0]); free(G.photos[0].px); memmove(&G.photos[0], &G.photos[1], sizeof(Photo) * (MAXPHOTOS - 1)); G.nphotos--; }
    u16 *px = malloc(W * H * 2);
    if (px) {
        memcpy(px, frame, W * H * 2);
        char stamp[40]; snprintf(stamp, sizeof stamp, "%s %s", label, stars == 3 ? "\x01\x01\x01" : stars == 2 ? "\x01\x01" : stars == 1 ? "\x01" : "");
        draw_text(px, 9, H - 15, stamp, hexc(0x000000), 160);
        draw_text(px, 8, H - 16, stamp, hexc(0xffd36b), 240);
        time_t now = time(NULL); struct tm *tm = localtime(&now);
        if (tm) { char d[16]; snprintf(d, sizeof d, "%04d-%02d-%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday); draw_text_r(px, W - 8, H - 16, d, hexc(0xffffff), 150); }
        Photo *p = &G.photos[G.nphotos++]; p->px = px; snprintf(p->label, sizeof p->label, "%s", label); p->stars = stars;
        p->id = next_photo_id++; if (!next_photo_id) next_photo_id = 1;
        save_photo(p);
    }
    save_all();
}

// ---------------------------------------------------------------------------
// Bottom screen UI
// ---------------------------------------------------------------------------
enum { B_NONE, B_PREV, B_NEXT, B_UP, B_DOWN, B_LEFT, B_RIGHT, B_GUIDE, B_ALBUM, B_ZOUT, B_ZIN, B_SHOOT, B_SLIDER, B_BACK, B_DEL, B_PHOTO0 };
typedef struct { int id, x, y, w, h; } Btn;
static Btn btns[40]; static int nbtns;
static int held_btn = B_NONE;
static void add_btn(int id, int x, int y, int w, int h) { if (nbtns < 40) { Btn b = { id, x, y, w, h }; btns[nbtns++] = b; } }

static void pill(u16 *s, int id, int x, int y, int w, int h, const char *label, bool on) {
    bool pr = held_btn == id;
    round_rect(s, x, y + pr, w, h, on ? hexc(0x3b4aa0) : hexc(0x232a55), on ? hexc(0x9fb0ff) : hexc(0x3c4787));
    draw_text_c(s, x + w / 2, y + (h - FONT_H) / 2 + pr, label, on ? hexc(0xffffff) : hexc(0xb9c2ff), 256);
    add_btn(id, x, y, w, h);
}

static float zoom_k(void) { return (logf(90) - logf(G.targetFov)) / (logf(90) - logf(3)); }

static void draw_bottom_cam(u16 *s, float t) {
    int tg = G.target; char buf[64];
    fill_rect(s, 0, 0, W, 22, hexc(0x151a38), 256);
    char up[32]; int k = 0; for (const char *p = tgt_name(tg); *p && k < 31; p++) up[k++] = (*p >= 'a' && *p <= 'z') ? *p - 32 : *p; up[k] = 0;
    if (G.found[tg]) { int n = strlen(up); up[n++] = ' '; for (int i = 0; i < G.found[tg]; i++) up[n++] = '\x01'; up[n] = 0; }
    draw_text_c(s, W / 2, 6, up, hexc(0xffd36b), 256); draw_text_c(s, W / 2 + 1, 6, up, hexc(0xffd36b), 256);
    draw_text(s, 8, 6, "\x03", hexc(0x9fb0ff), 256); draw_text(s, W - 14, 6, "\x04", hexc(0x9fb0ff), 256);
    add_btn(B_PREV, 0, 0, 40, 22); add_btn(B_NEXT, W - 40, 0, 40, 22);

    // compass
    int cx = 34, cy = 54, R = 24;
    fill_circle(s, cx, cy, R - 1, hexc(0x0d1130), 256);
    stroke_circle(s, cx, cy, R, hexc(0x3c4787), 256);
    if (tg != T_METEOR) {
        setup_proj();
        V3 tv = tgt_vec(tg);
        float dist = ang_dist(P.c, tv);
        float x = dot3(tv, P.e), y = dot3(tv, P.n), ang = atan2f(-y, -x);
        if (dist < G.fov * 0.25f) stroke_circle(s, cx, cy, 6 + sinf(t * 6) * 1.5f, hexc(0x52ff9a), 256);
        else {
            float c = cosf(ang), sn = sinf(ang);
            #define RX(px_, py_) (cx + (px_) * c - (py_) * sn)
            #define RY(px_, py_) (cy + (px_) * sn + (py_) * c)
            fill_tri(s, RX(R - 5, 0), RY(R - 5, 0), RX(-6, -6), RY(-6, -6), RX(-2, 0), RY(-2, 0), hexc(0xffd36b));
            fill_tri(s, RX(R - 5, 0), RY(R - 5, 0), RX(-2, 0), RY(-2, 0), RX(-6, 6), RY(-6, 6), hexc(0xffd36b));
        }
        snprintf(buf, sizeof buf, "%d\x08 away", (int)dist);
        draw_text_c(s, cx, cy + R + 4, buf, hexc(0xcfd6ff), 256);
    } else {
        draw_text_c(s, cx, cy - 5, "\x01", hexc(0xffd36b), 256);
        draw_line(s, cx - 10, cy + 6, cx - 2, cy - 1, hexc(0xffd36b), 200, 0, false);
    }
    wrap_text(s, 66, 27, 184, 11, tgt_hint(tg), hexc(0xcfd6ff));
    snprintf(buf, sizeof buf, "LOG %d/%d  \x01 %d/%d", count_found(), NTARGETS, count_stars(), NTARGETS * 3);
    draw_text(s, 66, 81, buf, hexc(0x7d87c9), 256);

    // d-pad
    int dx = 40, dy = 132, sz = 15;
    round_rect(s, dx - sz / 2, dy - sz * 3 / 2 - 2, sz, sz * 3 + 4, hexc(0x232a55), hexc(0x232a55));
    round_rect(s, dx - sz * 3 / 2 - 2, dy - sz / 2, sz * 3 + 4, sz, hexc(0x232a55), hexc(0x232a55));
    struct { int id, ox, oy; const char *g; } pad[4] = { {B_UP,0,-1,"\x05"}, {B_DOWN,0,1,"\x06"}, {B_LEFT,-1,0,"\x03"}, {B_RIGHT,1,0,"\x04"} };
    for (int i = 0; i < 4; i++) {
        int bx = dx + pad[i].ox * (sz + 1) - sz / 2, by = dy + pad[i].oy * (sz + 1) - sz / 2;
        if (held_btn == pad[i].id || (keysHeld() & (pad[i].id == B_UP ? KEY_UP : pad[i].id == B_DOWN ? KEY_DOWN : pad[i].id == B_LEFT ? KEY_LEFT : KEY_RIGHT)))
            fill_rect(s, bx, by, sz, sz, hexc(0x3b4aa0), 256);
        draw_text(s, bx + 5, by + 3, pad[i].g, hexc(0x9fb0ff), 256);
        add_btn(pad[i].id, bx - 3, by - 3, sz + 6, sz + 6);
    }
    pill(s, B_GUIDE, 88, 100, 72, 18, G.guide ? "GUIDE: ON" : "GUIDE: OFF", G.guide);
    snprintf(buf, sizeof buf, "ALBUM (%d)", G.nphotos);
    pill(s, B_ALBUM, 88, 122, 72, 18, buf, false);
    pill(s, B_ZOUT, 88, 144, 34, 18, "-", held_btn == B_ZOUT || (keysHeld() & KEY_L));
    pill(s, B_ZIN, 126, 144, 34, 18, "+", held_btn == B_ZIN || (keysHeld() & KEY_R));

    // shutter
    int sx = 210, sy = 131, sr = 26; bool pr = held_btn == B_SHOOT || (keysHeld() & KEY_A);
    fill_circle(s, sx, sy, sr + 3, hexc(0x0d1130), 256);
    { // radial gradient lit from the upper left
        RGB a = pr ? hexc(0xff9a7a) : hexc(0xffb59e), b = pr ? hexc(0xb8343f) : hexc(0xe2475a);
        int cy = sy + pr, hx = sx - 6, hy = cy - 6, span = sr + 8;
        for (int y = cy - sr; y <= cy + sr; y++) for (int x = sx - sr; x <= sx + sr; x++) {
            int dx = x - sx, dy = y - cy; if (dx * dx + dy * dy > sr * sr) continue;
            int ex = x - hx, ey = y - hy, k = isqrt4[ex * ex + ey * ey] / span; if (k > 16) k = 16; // 0..16
            px_set(s, x, y, C15(a.r + (b.r - a.r) * k / 16, a.g + (b.g - a.g) * k / 16, a.b + (b.b - a.b) * k / 16));
        }
    }
    stroke_circle(s, sx, sy + pr, sr - 7, hexc(0xffffff), 110);
    draw_text_c(s, sx, sy - 4 + pr, "SNAP", hexc(0xffffff), 256);
    draw_text_c(s, sx + 1, sy - 4 + pr, "SNAP", hexc(0xffffff), 256);
    add_btn(B_SHOOT, sx - sr, sy - sr, sr * 2, sr * 2);

    // zoom slider
    int zx = 16, zw = W - 32, zy = 178;
    round_rect(s, zx, zy - 2, zw, 5, hexc(0x232a55), hexc(0x232a55));
    float kz = zoom_k();
    if (zw * kz > 6) round_rect(s, zx, zy - 2, (int)(zw * kz), 5, hexc(0x3b4aa0), hexc(0x3b4aa0));
    fill_circle(s, zx + zw * kz, zy + 0.5f, 5, hexc(0xffd36b), 256);
    draw_text(s, zx, zy - 15, "WIDE", hexc(0x7d87c9), 256); draw_text_r(s, zx + zw, zy - 15, "TELE", hexc(0x7d87c9), 256);
    draw_text_c(s, W / 2, zy - 15, "L/R", hexc(0x4a5490), 256);
    add_btn(B_SLIDER, zx - 6, zy - 10, zw + 12, 20);
}

static void thumb(u16 *s, const u16 *src, int x, int y, int w, int h) {
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) px_set(s, x + i, y + j, src[(j * H / h) * W + (i * W / w)]);
}

static void draw_bottom_album(u16 *s) {
    fill_rect(s, 0, 0, W, 22, hexc(0x151a38), 256);
    draw_text_c(s, W / 2, 6, "PHOTO ALBUM", hexc(0xffd36b), 256); draw_text_c(s, W / 2 + 1, 6, "PHOTO ALBUM", hexc(0xffd36b), 256);
    int cols = 4, tw = 56, th = 42, gx = 8, gy = 6, ox = (W - (cols * tw + (cols - 1) * gx)) / 2, oy = 28;
    for (int i = 0; i < MAXPHOTOS; i++) {
        int x = ox + (i % cols) * (tw + gx), y = oy + (i / cols) * (th + gy);
        fill_rect(s, x, y, tw, th, hexc(0x0d1130), 256);
        if (i < G.nphotos) { thumb(s, G.photos[i].px, x + 1, y + 1, tw - 2, th - 2); add_btn(B_PHOTO0 + i, x, y, tw, th); }
        stroke_rect(s, x, y, tw, th, i == G.sel ? hexc(0xffd36b) : hexc(0x3c4787), 256);
        if (i == G.sel) stroke_rect(s, x - 1, y - 1, tw + 2, th + 2, hexc(0xffd36b), 256);
    }
    pill(s, B_BACK, 8, 170, 64, 16, "\x03 CAMERA", false);
    if (G.mode == MODE_VIEW && G.sel < G.nphotos) pill(s, B_DEL, W - 68, 170, 60, 16, "DELETE", false);
    else draw_text(s, 80, 173, G.nphotos ? "A: view  X: delete" : "No photos yet!", hexc(0x7d87c9), 256);
}

static void draw_top_album(u16 *s) {
    if (G.mode == MODE_VIEW && G.sel < G.nphotos) { memcpy(s, G.photos[G.sel].px, W * H * 2); return; }
    for (int i = 0; i < W * H; i++) s[i] = C15(5, 6, 15);
    draw_text_c(s, W / 2, 10, "STAR LOG", hexc(0xcfd6ff), 256); draw_text_c(s, W / 2 + 1, 10, "STAR LOG", hexc(0xcfd6ff), 256);
    for (int i = 0; i < NTARGETS; i++) {
        int col = i < 7 ? 0 : 1, row = i % 7, x = col ? W * 3 / 4 : W / 4, y = 30 + row * 22;
        int st = G.found[i];
        RGB c = st ? hexc(0xffd36b) : hexc(0x4a5490);
        draw_text_c(s, x, y, st ? tgt_name(i) : "? ? ?", c, 256);
        draw_stars_row(s, x, y + 10, st, c);
    }
    if (!G.sd) draw_text_c(s, W / 2, H - 12, "No SD access: progress won't be saved", hexc(0x7d87c9), 256);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
static volatile int vbl_count;
static void vblank_irq(void) { vbl_count++; }

static void cycle_target(int d) { G.target = (G.target + d + NTARGETS) % NTARGETS; beep(d > 0 ? 480 : 400, 3); }

static void press(int id, int tx) {
    switch (id) {
        case B_PREV: cycle_target(-1); break;
        case B_NEXT: cycle_target(1); break;
        case B_GUIDE: G.guide = !G.guide; beep(520, 3); break;
        case B_ALBUM: G.mode = MODE_ALBUM; G.sel = G.nphotos ? G.nphotos - 1 : 0; beep(600, 3); break;
        case B_SHOOT: take_photo(); break;
        case B_SLIDER: { float k = (tx - 16) / (float)(W - 32); if (k < 0) k = 0; if (k > 1) k = 1; G.targetFov = expf(logf(90) - k * (logf(90) - logf(3))); } break;
        case B_BACK: G.mode = MODE_CAM; beep(500, 3); break;
        case B_DEL:
            if (G.sel < G.nphotos) {
                remove_photo(&G.photos[G.sel]); free(G.photos[G.sel].px);
                memmove(&G.photos[G.sel], &G.photos[G.sel + 1], sizeof(Photo) * (G.nphotos - G.sel - 1));
                G.nphotos--; if (G.sel >= G.nphotos && G.sel) G.sel--; G.mode = MODE_ALBUM; save_all(); sfx(180, 6, 40, false, 0);
            }
            break;
        default:
            if (id >= B_PHOTO0 && id < B_PHOTO0 + MAXPHOTOS) { G.sel = id - B_PHOTO0; G.mode = MODE_VIEW; beep(700, 2); }
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    for (int i = 0; i < 256; i++) sin_tab[i] = (s8)(sinf(i * 2 * PI_F / 256) * 127);
    for (int i = 0; i < 1024; i++) sin1k[i] = (s16)(sinf(i * 2 * PI_F / 1024) * 16384);
    { int r = 0; for (int n = 0; n < 65536; n++) { while ((r + 1) * (r + 1) <= n * 256) r++; isqrt4[n] = (u16)r; } }

    videoSetMode(MODE_5_2D);
    videoSetModeSub(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
    vramSetBankB(VRAM_B_MAIN_BG_0x06020000);
    vramSetBankC(VRAM_C_SUB_BG_0x06200000);
    int bgTop = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    int bgBot = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    u16 *topVram[2] = { (u16 *)BG_BMP_RAM(0), (u16 *)BG_BMP_RAM(8) };
    u16 *botVram = bgGetGfxPtr(bgBot);
    lcdMainOnTop();

    // loading screen
    memset(topbuf, 0, sizeof topbuf);
    draw_text_c(topbuf, W / 2, 90, "STARCAM", hexc(0xffd36b), 256);
    draw_text_c(topbuf, W / 2, 104, "Charting the sky...", hexc(0x7d87c9), 256);
    dmaCopy(topbuf, topVram[0], sizeof topbuf);

    irqSet(IRQ_VBLANK, vblank_irq);
    irqEnable(IRQ_VBLANK);
    soundEnable();

    G.sd = fatInitDefault();
    build_sky();
    load_all();
#ifdef TEST_RA // test builds only: start pointed at a given spot
    G.ra = TEST_RA; G.dec = TEST_DEC; G.fov = G.targetFov = TEST_FOV;
#endif
    toast("STARCAM", "Aim at Orion and press A!", -1, true);
    if (!G.sd) toast("STARCAM", "No SD card found: no saving", -1, true);

    int front = 0, last_vbl = vbl_count, frame_no = 0, last_mode = -1, fps_vbl = 0, fps_frames = 0, fps_show = 0;
    bool touching = false;
    u32 last_held = 0;

    while (1) {
        int now = vbl_count, frames = now - last_vbl; last_vbl = now;
        if (frames < 1) frames = 1; if (frames > 4) frames = 4;
        float dt = frames / 60.0f; G.time += dt; float t = G.time;
        for (int f = 0; f < frames; f++) sound_tick();

        scanKeys();
        u32 down = keysDown(), heldk = keysHeld();

        // touch
        if (heldk & KEY_TOUCH) {
            touchPosition tp; touchRead(&tp);
            if (!touching) {
                touching = true; held_btn = B_NONE;
                for (int i = nbtns - 1; i >= 0; i--) {
                    Btn *b = &btns[i];
                    if (tp.px >= b->x && tp.px < b->x + b->w && tp.py >= b->y && tp.py < b->y + b->h) { held_btn = b->id; press(b->id, tp.px); break; }
                }
            } else if (held_btn == B_SLIDER) press(B_SLIDER, tp.px);
        } else { touching = false; held_btn = B_NONE; }

        if (G.mode == MODE_CAM) {
            if (down & KEY_A) take_photo();
            if (down & (KEY_B | KEY_SELECT)) { G.guide = !G.guide; beep(520, 3); }
            if (down & KEY_X) cycle_target(1);
            if (down & KEY_Y) cycle_target(-1);
            if (down & KEY_START) press(B_ALBUM, 0);
            float speed = G.fov * 0.9f, cd = cosf(G.dec * D2R); if (cd < 0.2f) cd = 0.2f;
            int ix = ((heldk & KEY_RIGHT) || held_btn == B_RIGHT) - ((heldk & KEY_LEFT) || held_btn == B_LEFT);
            int iy = ((heldk & KEY_UP) || held_btn == B_UP) - ((heldk & KEY_DOWN) || held_btn == B_DOWN);
            if (ix || iy) { G.vra = -ix * speed / cd; G.vdec = iy * speed; }
            else { float damp = powf(0.02f, dt); G.vra *= damp; G.vdec *= damp; }
            G.ra = fmodf(G.ra + G.vra * dt + 360, 360); G.dec += G.vdec * dt;
            if (G.dec > 85) G.dec = 85; if (G.dec < -75) G.dec = -75;
            if ((heldk & KEY_R) || held_btn == B_ZIN) { G.targetFov *= expf(-1.4f * dt); if (G.targetFov < 3) G.targetFov = 3; }
            if ((heldk & KEY_L) || held_btn == B_ZOUT) { G.targetFov *= expf(1.4f * dt); if (G.targetFov > 90) G.targetFov = 90; }
            float zk = dt * 10; if (zk > 1) zk = 1;
            G.fov += (G.targetFov - G.fov) * zk;
        } else {
            if (down & (KEY_B | KEY_START)) { if (G.mode == MODE_VIEW) G.mode = MODE_ALBUM; else G.mode = MODE_CAM; beep(500, 3); }
            if (G.nphotos) {
                if (down & KEY_RIGHT) G.sel = (G.sel + 1) % G.nphotos;
                if (down & KEY_LEFT) G.sel = (G.sel + G.nphotos - 1) % G.nphotos;
                if (down & KEY_DOWN) { G.sel += 4; if (G.sel >= G.nphotos) G.sel = G.nphotos - 1; }
                if (down & KEY_UP) { G.sel -= 4; if (G.sel < 0) G.sel = 0; }
                if (down & KEY_A) { G.mode = MODE_VIEW; beep(700, 2); }
                if ((down & KEY_X) && G.mode == MODE_VIEW) press(B_DEL, 0);
            }
        }

        for (int i = 0; i < NCLOUDS; i++) clouds[i].ra = fmodf(clouds[i].ra + clouds[i].drift * dt + 360, 360);
        next_meteor -= dt; if (next_meteor <= 0) { spawn_meteor(); next_meteor = 4 + (float)rand() / RAND_MAX * 8; }
        for (int i = 0; i < 3; i++) if (meteors[i].on && (meteors[i].life += dt) >= meteors[i].max) meteors[i].on = false;
        if (toast_on && (toast_s.life += dt) > toast_s.max) toast_on = false;
        if (complete_timer > 0 && (complete_timer -= dt) <= 0) toast("SKY LOG COMPLETE!", "You photographed every target!", -1, true);

        // top screen
        if (G.mode == MODE_CAM) {
            draw_sky(topbuf, t);
            if (pending_shot) { pending_shot = false; finish_photo(topbuf); }
            draw_hud(topbuf, t);
        } else draw_top_album(topbuf);
        if (G.flash > 0) {
            fill_rect(topbuf, 0, 0, W, H, hexc(0xffffff), (int)(G.flash * 256));
            G.flash -= dt * 4; if (G.flash < 0) G.flash = 0;
        }
        draw_toast(topbuf);

        // bottom screen: redraw on input or mode change, otherwise ~10 times a second
        bool bot_dirty = (frame_no % 4 == 0) || down || heldk != last_held || touching || G.mode != last_mode;
        last_held = heldk; last_mode = G.mode; frame_no++;
        if (bot_dirty) {
            nbtns = 0;
            for (int y = 0; y < H; y++) { u16 c = (y & 1) ? C15(11, 14, 34) : C15(13, 16, 37); for (int x = 0; x < W; x++) botbuf[y * W + x] = c; }
            if (G.mode == MODE_CAM) draw_bottom_cam(botbuf, t); else draw_bottom_album(botbuf);
        }
#ifdef SHOW_FPS
        fps_vbl += frames; fps_frames++;
        if (fps_vbl >= 60) { fps_show = fps_frames * 600 / fps_vbl; fps_vbl = fps_frames = 0; }
        { char fb[16]; snprintf(fb, sizeof fb, "%d.%d fps", fps_show / 10, fps_show % 10); fill_rect(topbuf, 100, 26, 56, 11, hexc(0), 256); draw_text(topbuf, 101, 27, fb, hexc(0x52ff9a), 256); }
#else
        (void)fps_vbl; (void)fps_frames; (void)fps_show;
#endif

        // present: draw into the hidden top buffer, then flip on vblank
        int back = front ^ 1;
        DC_FlushRange(topbuf, sizeof topbuf);
        if (bot_dirty) DC_FlushRange(botbuf, sizeof botbuf);
        dmaCopy(topbuf, topVram[back], sizeof topbuf);
        swiWaitForVBlank();
        bgSetMapBase(bgTop, back ? 8 : 0);
        bgUpdate();
        if (bot_dirty) dmaCopy(botbuf, botVram, sizeof botbuf);
        front = back;
    }
    return 0;
}
