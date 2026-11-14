#include "gfx.h"
#include "font8.h"
#include <string.h>

#define PANO ((u16 *)0x06000000)
#define PANO_W 512

static u16 P(int r, int g, int b) { return RGB15(r >> 3, g >> 3, b >> 3); }

void gfxInitPalette(void) {
	u16 pal[C_COUNT];
	pal[C_CLEAR]    = P(28, 31, 38);
	pal[C_BLACK]    = P(0, 0, 0);
	pal[C_WHITE]    = P(255, 255, 255);
	pal[C_BG]       = P(28, 31, 38);
	pal[C_PANEL]    = P(32, 35, 42);
	pal[C_PANEL2]   = P(42, 46, 56);
	pal[C_BTN]      = P(48, 52, 60);
	pal[C_GREY]     = P(136, 136, 136);
	pal[C_LIGHT]    = P(221, 221, 221);
	pal[C_DIM]      = P(100, 100, 108);
	pal[C_RED]      = P(230, 50, 50);
	pal[C_PINK]     = P(255, 140, 140);
	pal[C_YELLOW]   = P(255, 255, 70);
	pal[C_GOLD]     = P(255, 213, 74);
	pal[C_GREEN]    = P(70, 200, 70);
	pal[C_LGREEN]   = P(150, 255, 150);
	pal[C_BLUE]     = P(70, 170, 255);
	pal[C_TAB]      = P(74, 111, 165);
	pal[C_TILE]     = P(51, 64, 79);
	pal[C_TILEEQ]   = P(47, 93, 58);
	pal[C_TILELOCK] = P(38, 40, 45);
	pal[C_ORANGE]   = P(255, 180, 50);
	pal[C_ROD]      = P(59, 36, 18);
	pal[C_RODTIP]   = P(34, 34, 34);
	pal[C_REEL]     = P(160, 160, 160);
	pal[C_LINE]     = P(230, 230, 230);
	pal[C_RAIN]     = P(170, 200, 232);
	pal[C_RIPPLE]   = P(150, 200, 225);
	pal[C_SHADE]    = P(16, 16, 20);
	pal[C_FOAM]     = P(220, 240, 250);
	pal[C_CYAN]     = P(120, 230, 230);
	pal[C_TIER0]    = P(158, 158, 158);
	pal[C_TIER1]    = P(76, 175, 80);
	pal[C_TIER2]    = P(33, 150, 243);
	pal[C_TIER3]    = P(156, 39, 176);
	pal[C_TIER4]    = P(255, 152, 0);
	pal[C_TIER5]    = P(255, 215, 0);
	for (int i = 0; i < C_COUNT; i++) { BG_PALETTE[i] = pal[i]; BG_PALETTE_SUB[i] = pal[i]; }
}

// ---------------- 8bpp ----------------
void c8Clear(u8 *b, u8 col) { memset(b, col, SW * SH); }

void c8Pixel(u8 *b, int x, int y, u8 col) {
	if ((unsigned)x < SW && (unsigned)y < SH) b[y * SW + x] = col;
}

void c8Rect(u8 *b, int x, int y, int w, int h, u8 col) {
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > SW) w = SW - x;
	if (y + h > SH) h = SH - y;
	if (w <= 0 || h <= 0) return;
	for (int j = 0; j < h; j++) memset(b + (y + j) * SW + x, col, w);
}

void c8Frame(u8 *b, int x, int y, int w, int h, u8 col) {
	c8Rect(b, x, y, w, 1, col); c8Rect(b, x, y + h - 1, w, 1, col);
	c8Rect(b, x, y, 1, h, col); c8Rect(b, x + w - 1, y, 1, h, col);
}

void c8Dither(u8 *b, int x, int y, int w, int h, u8 col, int level) {
	if (level >= 4) { c8Rect(b, x, y, w, h, col); return; }
	if (level <= 0) return;
	for (int j = y; j < y + h; j++) {
		if ((unsigned)j >= SH) continue;
		for (int i = x; i < x + w; i++) {
			if ((unsigned)i >= SW) continue;
			int on;
			if (level == 3) on = ((i ^ j) & 1) == 0 || (j & 1) == 0;
			else if (level == 2) on = ((i ^ j) & 1) == 0;
			else on = (i & 1) == 0 && (j & 1) == 0;
			if (on) b[j * SW + i] = col;
		}
	}
}

void c8Line(u8 *b, int x0, int y0, int x1, int y1, u8 col) {
	int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
	int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
	int err = dx + dy;
	for (;;) {
		c8Pixel(b, x0, y0, col);
		if (x0 == x1 && y0 == y1) break;
		int e2 = 2 * err;
		if (e2 >= dy) { err += dy; x0 += sx; }
		if (e2 <= dx) { err += dx; y0 += sy; }
	}
}

void c8ThickLine(u8 *b, int x0, int y0, int x1, int y1, int t, u8 col) {
	for (int o = -(t / 2); o < t - t / 2; o++) {
		c8Line(b, x0 + o, y0, x1 + o, y1, col);
		c8Line(b, x0, y0 + o, x1, y1 + o, col);
	}
}

void c8Quad(u8 *b, int x0, int y0, int cx, int cy, int x1, int y1, u8 col) {
	int px = x0, py = y0;
	for (int i = 1; i <= 16; i++) {
		int t = i * 16, it = 256 - t;               // /256
		int x = (it * it * x0 + 2 * it * t * cx + t * t * x1) >> 16;
		int y = (it * it * y0 + 2 * it * t * cy + t * t * y1) >> 16;
		c8Line(b, px, py, x, y, col);
		px = x; py = y;
	}
}

void c8Disc(u8 *b, int cx, int cy, int r, u8 col) {
	for (int y = -r; y <= r; y++)
		for (int x = -r; x <= r; x++)
			if (x * x + y * y <= r * r + r) c8Pixel(b, cx + x, cy + y, col);
}

void c8HalfDiscTop(u8 *b, int cx, int cy, int r, u8 col) {
	for (int y = -r; y <= 0; y++)
		for (int x = -r; x <= r; x++)
			if (x * x + y * y <= r * r + r) c8Pixel(b, cx + x, cy + y, col);
}

void c8EllipseFill(u8 *b, int cx, int cy, int rx, int ry, u8 col) {
	if (rx <= 0 || ry <= 0) return;
	for (int y = -ry; y <= ry; y++)
		for (int x = -rx; x <= rx; x++)
			if (x * x * ry * ry + y * y * rx * rx <= rx * rx * ry * ry) c8Pixel(b, cx + x, cy + y, col);
}

static int inEll(int x, int y, int rx, int ry) { return x * x * ry * ry + y * y * rx * rx <= rx * rx * ry * ry; }

void c8Ellipse(u8 *b, int cx, int cy, int rx, int ry, u8 col) {
	if (rx <= 0 || ry <= 0) return;
	for (int y = -ry; y <= ry; y++)
		for (int x = -rx; x <= rx; x++)
			if (inEll(x, y, rx, ry) && (!inEll(x + 1, y, rx, ry) || !inEll(x - 1, y, rx, ry) || !inEll(x, y + 1, rx, ry) || !inEll(x, y - 1, rx, ry)))
				c8Pixel(b, cx + x, cy + y, col);
}

void c8Fish(u8 *b, int x, int y, int s, u8 col) {
	int rx = 14 * s / 4, ry = 7 * s / 4, tl = 8 * s / 4;
	if (ry < 1) ry = 1;
	c8EllipseFill(b, x, y, rx, ry, col);
	for (int i = 0; i <= tl; i++) {
		int h = i * ry / (tl ? tl : 1);
		c8Rect(b, x + rx - 2 + i, y - h, 1, 2 * h + 1, col);
	}
	int e = s / 4 + 1;
	c8Rect(b, x - rx * 2 / 3, y - e / 2 - 1, e, e, C_BLACK);
}

// ---------------- text ----------------
int textW(const char *s, int scale) { return (int)strlen(s) * 8 * scale; }

void c8Text(u8 *b, int x, int y, const char *s, u8 col, int scale) {
	for (; *s; s++, x += 8 * scale) {
		int c = (unsigned char)*s;
		if (c < 32 || c > 126) c = '?';
		const u8 *g = FONT8[c - 32];
		for (int r = 0; r < 8; r++) {
			u8 bits = g[r];
			if (!bits) continue;
			for (int k = 0; k < 8; k++)
				if (bits & (0x80 >> k)) {
					if (scale == 1) c8Pixel(b, x + k, y + r, col);
					else c8Rect(b, x + k * scale, y + r * scale, scale, scale, col);
				}
		}
	}
}

void c8TextS(u8 *b, int x, int y, const char *s, u8 col, int scale) {
	c8Text(b, x + scale, y + scale, s, C_BLACK, scale);
	c8Text(b, x, y, s, col, scale);
}

void c8TextC(u8 *b, int cx, int y, const char *s, u8 col, int scale) {
	c8TextS(b, cx - textW(s, scale) / 2, y, s, col, scale);
}

void c8TextR(u8 *b, int rx, int y, const char *s, u8 col, int scale) {
	c8TextS(b, rx - textW(s, scale), y, s, col, scale);
}

// ---------------- 16bpp panorama ----------------
static int clamp255(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }
u16 rgb16(RGB c) { return RGB15(clamp255(c.r) >> 3, clamp255(c.g) >> 3, clamp255(c.b) >> 3) | BIT(15); }
RGB rgbMix(RGB a, RGB b, int t) { RGB r = { a.r + (b.r - a.r) * t / 256, a.g + (b.g - a.g) * t / 256, a.b + (b.b - a.b) * t / 256 }; return r; }
RGB rgbScale(RGB a, int t) { RGB r = { a.r * t / 256, a.g * t / 256, a.b * t / 256 }; return r; }

void p16Pixel(int x, int y, RGB c) {
	if ((unsigned)x < PANO_W && (unsigned)y < SH) PANO[y * PANO_W + x] = rgb16(c);
}

void p16Grad(int y0, int y1, RGB top, RGB bot) {
	int h = y1 - y0; if (h <= 0) return;
	for (int y = y0; y < y1; y++) {
		RGB c = rgbMix(top, bot, (y - y0) * 256 / h);
		// two-tone ordered dither between this row's colour and the next step for smoother bands
		u16 v = rgb16(c);
		u16 *row = PANO + y * PANO_W;
		for (int x = 0; x < PANO_W; x++) row[x] = v;
	}
}

void p16Rect(int x, int y, int w, int h, RGB c) {
	u16 v = rgb16(c);
	for (int j = y; j < y + h; j++) {
		if ((unsigned)j >= SH) continue;
		for (int i = x; i < x + w; i++)
			if ((unsigned)i < PANO_W) PANO[j * PANO_W + i] = v;
	}
}

void p16DitherRect(int x, int y, int w, int h, RGB c, int level) {
	u16 v = rgb16(c);
	for (int j = y; j < y + h; j++) {
		if ((unsigned)j >= SH) continue;
		for (int i = x; i < x + w; i++) {
			if ((unsigned)i >= PANO_W) continue;
			int on = level >= 2 ? ((i ^ j) & 1) == 0 : ((i & 1) == 0 && (j & 1) == 0);
			if (on) PANO[j * PANO_W + i] = v;
		}
	}
}

void p16Disc(int cx, int cy, int r, RGB c) {
	u16 v = rgb16(c);
	for (int y = -r; y <= r; y++)
		for (int x = -r; x <= r; x++)
			if (x * x + y * y <= r * r + r) {
				int px = cx + x, py = cy + y;
				if ((unsigned)px < PANO_W && (unsigned)py < SH) PANO[py * PANO_W + px] = v;
			}
}

// upper half of an ellipse sitting on y = base
void p16Hill(int cx, int base, int rx, int ry, RGB c) {
	u16 v = rgb16(c);
	for (int y = -ry; y <= 0; y++)
		for (int x = -rx; x <= rx; x++)
			if (x * x * ry * ry + y * y * rx * rx <= rx * rx * ry * ry) {
				int px = ((cx + x) % PANO_W + PANO_W) % PANO_W, py = base + y;
				if ((unsigned)py < SH) PANO[py * PANO_W + px] = v;
			}
}

void p16Tri(int xTop, int yTop, int halfW, int yBase, RGB c) {
	u16 v = rgb16(c);
	int h = yBase - yTop; if (h <= 0) return;
	for (int y = yTop; y <= yBase; y++) {
		int w = halfW * (y - yTop) / h;
		for (int x = xTop - w; x <= xTop + w; x++) {
			int px = (x % PANO_W + PANO_W) % PANO_W;
			if ((unsigned)y < SH) PANO[y * PANO_W + px] = v;
		}
	}
}
