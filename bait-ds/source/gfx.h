#pragma once
#include <nds.h>

#define SW 256
#define SH 192

// Shared 8bpp palette for the top overlay and the bottom screen.
// Index 0 is transparent on the top overlay and the backdrop on the bottom.
enum {
	C_CLEAR = 0,
	C_BLACK, C_WHITE, C_BG, C_PANEL, C_PANEL2, C_BTN, C_GREY, C_LIGHT, C_DIM,
	C_RED, C_PINK, C_YELLOW, C_GOLD, C_GREEN, C_LGREEN, C_BLUE, C_TAB,
	C_TILE, C_TILEEQ, C_TILELOCK, C_ORANGE, C_ROD, C_RODTIP, C_REEL, C_LINE,
	C_RAIN, C_RIPPLE, C_SHADE, C_FOAM, C_CYAN,
	C_TIER0, C_TIER1, C_TIER2, C_TIER3, C_TIER4, C_TIER5,
	C_COUNT
};

void gfxInitPalette(void);

// 8bpp drawing into a 256x192 RAM buffer
void c8Clear(u8 *b, u8 col);
void c8Rect(u8 *b, int x, int y, int w, int h, u8 col);
void c8Frame(u8 *b, int x, int y, int w, int h, u8 col);
void c8Dither(u8 *b, int x, int y, int w, int h, u8 col, int level); // level 1..4 (4 = solid)
void c8Pixel(u8 *b, int x, int y, u8 col);
void c8Line(u8 *b, int x0, int y0, int x1, int y1, u8 col);
void c8ThickLine(u8 *b, int x0, int y0, int x1, int y1, int t, u8 col);
void c8Quad(u8 *b, int x0, int y0, int cx, int cy, int x1, int y1, u8 col); // quadratic bezier
void c8Disc(u8 *b, int cx, int cy, int r, u8 col);
void c8HalfDiscTop(u8 *b, int cx, int cy, int r, u8 col);
void c8Ellipse(u8 *b, int cx, int cy, int rx, int ry, u8 col);
void c8EllipseFill(u8 *b, int cx, int cy, int rx, int ry, u8 col);
void c8Fish(u8 *b, int x, int y, int s, u8 col); // little fish icon, s = scale in quarter units

int  textW(const char *s, int scale);
void c8Text(u8 *b, int x, int y, const char *s, u8 col, int scale);
void c8TextS(u8 *b, int x, int y, const char *s, u8 col, int scale);        // with shadow
void c8TextC(u8 *b, int cx, int y, const char *s, u8 col, int scale);       // centred, shadow
void c8TextR(u8 *b, int rx, int y, const char *s, u8 col, int scale);       // right aligned, shadow

// 16bpp panorama (512x256 bitmap in VRAM A+B)
typedef struct { int r, g, b; } RGB;
u16  rgb16(RGB c);
RGB  rgbMix(RGB a, RGB b, int t256);
RGB  rgbScale(RGB a, int t256);
void p16Grad(int y0, int y1, RGB top, RGB bot);
void p16Rect(int x, int y, int w, int h, RGB c);
void p16Disc(int cx, int cy, int r, RGB c);
void p16Hill(int cx, int base, int rx, int ry, RGB c);
void p16Tri(int xTop, int yTop, int halfW, int yBase, RGB c);
void p16Pixel(int x, int y, RGB c);
void p16DitherRect(int x, int y, int w, int h, RGB c, int level);
