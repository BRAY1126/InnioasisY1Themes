// Groovy Grow - a psychedelic farm clicker for Nintendo DS / DSi / 3DS (TWiLight Menu++).
// Built with BlocksDS. Runs from an R4 (or any DLDI flashcart) and saves to the SD card.
//
// Controls:
//   Tap the big button or press A  -> pick a bud
//   Tap a shop row                 -> select + buy it
//   Up / Down + X (or R)           -> select + buy with buttons
//   SELECT                         -> save now
//   START                          -> boss screen (any key returns)

#include <nds.h>
#include <fat.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SAVE_FILE   "/groovygrow.sav"
#define MAX_OFFLINE (8 * 3600) // max seconds of offline earnings

typedef struct {
    const char *name;
    const char *desc;
    double cost;
    double rate;
} Item;

#define NITEMS 7
static const Item ITEMS[NITEMS] = {
    { "Seedling Pot",    "A tiny sprout with big dreams.",  15,       0.1  },
    { "Hippie Helper",   "Plays bongos while trimming.",    100,      1    },
    { "Grow Lamp",       "Purple light, good vibes.",       1100,     8    },
    { "Greenhouse",      "Foggy windows, happy plants.",    12000,    47   },
    { "Peace Van",       "Delivers buds and good vibes.",   130000,   260  },
    { "Hilltop Commune", "Free love, freer harvests.",      1400000,  1400 },
    { "Cosmic Field",    "The universe farms itself, man.", 20000000, 7800 },
};

#define NCLICK 6
static const char *CLICK_NAMES[NCLICK] = {
    "Rusty Shears", "Tie-Dye Gloves", "Crystal Trimmers",
    "Lava Lamp Focus", "Third Eye", "Golden Sickle",
};

// Game state
static double buds, total;
static int clickLvl;
static int owned[NITEMS];

// UI state
static PrintConsole topCon, botCon;
static u16 *fb;            // bottom screen 16-bit bitmap
static int sel;            // 0 = pick upgrade, 1..NITEMS = items
static int boss;
static int btnFlash;
static char msg[33];
static int msgTimer;
static int fatOK;
static int hue;

// ---------------------------------------------------------------------------
// Rules

static double itemCost(int i) { return ceil(ITEMS[i].cost * pow(1.15, owned[i])); }
static double clickCost(void) { return ceil(50 * pow(6, clickLvl)); }
static double clickPower(void) { return (double)(1 << clickLvl); }

static double perSec(void)
{
    double r = 0;
    for (int i = 0; i < NITEMS; i++)
        r += ITEMS[i].rate * owned[i];
    return r;
}

static int itemVisible(int i)
{
    return i == 0 || owned[i] > 0 || owned[i - 1] > 0 || total >= ITEMS[i].cost * 0.5;
}

static void add(double n) { buds += n; total += n; }

// ---------------------------------------------------------------------------
// Formatting (no float printf needed)

static void fmt(double n, char *out)
{
    static const char *u[] = { "", "K", "M", "B", "T", "Qa", "Qi" };
    int i = 0;
    n = floor(n);
    if (n < 10000) {
        sprintf(out, "%ld", (long)n);
        return;
    }
    while (n >= 1000 && i < 6) {
        n /= 1000;
        i++;
    }
    if (n < 100) {
        int t = (int)(n * 10);
        sprintf(out, "%d.%d%s", t / 10, t % 10, u[i]);
    } else {
        sprintf(out, "%d%s", (int)n, u[i]);
    }
}

static void fmtRate(double r, char *out)
{
    if (r < 10) {
        int t = (int)(r * 10 + 0.5);
        sprintf(out, "%d.%d", t / 10, t % 10);
    } else {
        fmt(r, out);
    }
}

static void say(const char *t)
{
    snprintf(msg, sizeof(msg), "%s", t);
    msgTimer = 150;
}

// ---------------------------------------------------------------------------
// Saving

static void save(void)
{
    if (!fatOK)
        return;
    FILE *f = fopen(SAVE_FILE, "w");
    if (!f)
        return;
    fprintf(f, "GG1 %llu %llu %d", (unsigned long long)buds, (unsigned long long)total, clickLvl);
    for (int i = 0; i < NITEMS; i++)
        fprintf(f, " %d", owned[i]);
    fprintf(f, " %lld\n", (long long)time(NULL));
    fclose(f);
}

static int load(long long *last)
{
    char buf[256];
    if (!fatOK)
        return 0;
    FILE *f = fopen(SAVE_FILE, "r");
    if (!f)
        return 0;
    size_t len = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[len] = 0;
    if (strncmp(buf, "GG1 ", 4) != 0)
        return 0;

    char *p = buf + 4;
    buds = (double)strtoull(p, &p, 10);
    total = (double)strtoull(p, &p, 10);
    clickLvl = (int)strtol(p, &p, 10);
    if (clickLvl < 0 || clickLvl > NCLICK)
        clickLvl = 0;
    for (int i = 0; i < NITEMS; i++)
        owned[i] = (int)strtol(p, &p, 10);
    *last = strtoll(p, &p, 10);
    return 1;
}

// ---------------------------------------------------------------------------
// Bottom screen drawing

#define C(r, g, b) (RGB15(r, g, b) | BIT(15))

static void rect(int x, int y, int w, int h, u16 c)
{
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            fb[j * 256 + i] = c;
}

static const u16 PASTEL[5] = {
    C(31, 22, 28), C(31, 28, 17), C(22, 31, 19), C(19, 28, 31), C(26, 22, 31),
};

static int rowY(int k) { return 56 + 16 * k; }

static int buyable(int k)
{
    if (k == 0)
        return clickLvl < NCLICK && buds >= clickCost();
    return buds >= itemCost(k - 1);
}

static int visible(int k)
{
    if (k == 0)
        return 1;
    return itemVisible(k - 1);
}

static u32 lastSig = 0xFFFFFFFF;

static void drawBottomGfx(int force)
{
    // Only repaint the bitmap when something that affects it changes.
    u32 sig = (u32)sel | (btnFlash ? 0x10 : 0);
    for (int k = 0; k <= NITEMS; k++) {
        sig |= (u32)buyable(k) << (5 + k);
        sig |= (u32)visible(k) << (14 + k);
    }
    if (!force && sig == lastSig)
        return;
    lastSig = sig;

    // Psychedelic background: horizontal colour bands
    for (int y = 0; y < 192; y++) {
        int t = (y / 6) % 6;
        static const u16 bands[6] = {
            C(10, 2, 15), C(12, 2, 14), C(14, 3, 13), C(12, 2, 14), C(10, 2, 15), C(8, 2, 14),
        };
        u16 c = bands[t];
        for (int x = 0; x < 256; x++)
            fb[y * 256 + x] = c;
    }

    // Big pick button
    rect(4, 2, 248, 44, C(31, 17, 0));
    rect(7, 5, 242, 38, btnFlash ? C(31, 31, 10) : C(15, 31, 10));

    // Shop rows
    for (int k = 0; k <= NITEMS; k++) {
        int y = rowY(k);
        if (!visible(k)) {
            rect(2, y, 252, 15, C(6, 2, 9));
            continue;
        }
        if (k == sel)
            rect(1, y - 1, 254, 17, C(31, 31, 31));
        u16 c = buyable(k) ? PASTEL[k % 5] : C(13, 11, 15);
        rect(2, y, 252, 15, c);
    }
}

static void pad(char *line, int width)
{
    int n = strlen(line);
    while (n < width)
        line[n++] = ' ';
    line[width] = 0;
}

static void drawBottomText(void)
{
    char line[64], a[16], b[16];
    consoleSelect(&botCon);

    printf("\x1b[2;0H\x1b[35m        ~ PICK A BUD ~         ");
    fmt(clickPower(), a);
    snprintf(line, sizeof(line), "+%s per pick", a);
    int pad1 = (32 - (int)strlen(line)) / 2;
    printf("\x1b[3;0H%*s%s%*s", pad1, "", line, 31 - pad1 - (int)strlen(line), "");

    snprintf(line, sizeof(line), "%s", msgTimer > 0 ? msg : "");
    pad(line, 31);
    printf("\x1b[6;0H\x1b[33m%s", line);

    for (int k = 0; k <= NITEMS; k++) {
        int row = 7 + 2 * k;
        if (!visible(k)) {
            printf("\x1b[%d;0H\x1b[35m  ???                           ", row);
            printf("\x1b[%d;0H                                ", row + 1);
            continue;
        }
        const char *name;
        double cost;
        if (k == 0) {
            if (clickLvl >= NCLICK) {
                printf("\x1b[%d;0H\x1b[30m Picks maxed out. Far out!      ", row);
                printf("\x1b[%d;0H                                ", row + 1);
                continue;
            }
            name = CLICK_NAMES[clickLvl];
            cost = clickCost();
        } else {
            name = ITEMS[k - 1].name;
            cost = itemCost(k - 1);
        }
        fmt(cost, a);
        snprintf(line, sizeof(line), " %s", name);
        pad(line, 31 - (int)strlen(a));
        strcat(line, a);
        printf("\x1b[%d;0H\x1b[30m%s", row, line);

        if (k == 0) {
            snprintf(line, sizeof(line), " Picks x2");
        } else {
            fmtRate(ITEMS[k - 1].rate, b);
            snprintf(line, sizeof(line), " Own %d   +%s/s", owned[k - 1], b);
        }
        pad(line, 31);
        printf("\x1b[%d;0H%s", row + 1, line);
    }

    printf("\x1b[23;0H\x1b[37mA:pick X:buy SEL:save START:?");
}

// ---------------------------------------------------------------------------
// Top screen

static void drawTop(void)
{
    static const char *colors[6] = { "\x1b[31;1m", "\x1b[33;1m", "\x1b[32;1m", "\x1b[36;1m", "\x1b[35;1m", "\x1b[37;1m" };
    static const char *title = "GROOVY GROW";
    char a[16], b[16], line[64];

    consoleSelect(&topCon);

    printf("\x1b[1;10H");
    for (int i = 0; title[i]; i++)
        printf("%s%c", colors[(i + hue) % 6], title[i]);

    printf("\x1b[2;4H\x1b[36m~ peace, love & plants ~");

    fmt(buds, a);
    snprintf(line, sizeof(line), "%s buds", a);
    int p = (32 - (int)strlen(line)) / 2;
    printf("\x1b[5;0H\x1b[32m%*s%s%*s", p, "", line, 31 - p - (int)strlen(line), "");

    fmtRate(perSec(), a);
    fmt(clickPower(), b);
    snprintf(line, sizeof(line), "%s/sec  -  %s/pick", a, b);
    p = (32 - (int)strlen(line)) / 2;
    printf("\x1b[7;0H\x1b[37m%*s%s%*s", p, "", line, 31 - p - (int)strlen(line), "");

    fmt(total, a);
    snprintf(line, sizeof(line), "all-time: %s", a);
    p = (32 - (int)strlen(line)) / 2;
    printf("\x1b[8;0H\x1b[36m%*s%s%*s", p, "", line, 31 - p - (int)strlen(line), "");

    // Selected item info
    printf("\x1b[11;0H\x1b[33m -------------------------------");
    if (sel == 0) {
        if (clickLvl >= NCLICK) {
            snprintf(line, sizeof(line), " Golden Sickle mastered");
            pad(line, 31);
            printf("\x1b[12;0H\x1b[37m%s", line);
            snprintf(line, sizeof(line), " Your hands are one with");
            pad(line, 31);
            printf("\x1b[13;0H%s", line);
            snprintf(line, sizeof(line), " the garden.");
        } else {
            snprintf(line, sizeof(line), " %s", CLICK_NAMES[clickLvl]);
            pad(line, 31);
            printf("\x1b[12;0H\x1b[37m%s", line);
            snprintf(line, sizeof(line), " Doubles every pick.");
            pad(line, 31);
            printf("\x1b[13;0H%s", line);
            fmt(clickCost(), a);
            snprintf(line, sizeof(line), " Cost: %s buds", a);
        }
    } else {
        const Item *it = &ITEMS[sel - 1];
        if (!itemVisible(sel - 1)) {
            snprintf(line, sizeof(line), " ??? (keep growing, man)");
            pad(line, 31);
            printf("\x1b[12;0H\x1b[37m%s", line);
            printf("\x1b[13;0H%31s", "");
            line[0] = 0;
        } else {
            snprintf(line, sizeof(line), " %s  (own %d)", it->name, owned[sel - 1]);
            pad(line, 31);
            printf("\x1b[12;0H\x1b[37m%s", line);
            snprintf(line, sizeof(line), " %s", it->desc);
            pad(line, 31);
            printf("\x1b[13;0H%s", line);
            fmt(itemCost(sel - 1), a);
            fmtRate(it->rate, b);
            snprintf(line, sizeof(line), " Cost: %s   +%s/sec", a, b);
        }
    }
    pad(line, 31);
    printf("\x1b[14;0H%s", line);
    printf("\x1b[15;0H\x1b[33m -------------------------------");

    printf("\x1b[19;0H\x1b[36m %s", fatOK ? "Autosaves to your SD card     " : "No SD card found - no saving  ");
    printf("\x1b[21;0H\x1b[37m Tap/A: pick    Up/Down: choose");
    printf("\x1b[22;0H X/R/tap row: buy  SELECT: save");
    printf("\x1b[23;0H START: boss screen");
}

// ---------------------------------------------------------------------------
// Boss screen: looks like a boring spreadsheet

static void showBoss(void)
{
    BG_PALETTE[0] = RGB15(31, 31, 31);
    consoleSelect(&topCon);
    consoleClear();
    printf("\x1b[30m");
    printf(" Q3_Budget.xls\n\n");
    printf("          Jul     Aug     Sep\n");
    printf(" ------------------------------\n");
    printf(" Revenue  41,200  43,915  44,080\n");
    printf(" Payroll  18,450  18,450  19,020\n");
    printf(" Supplies  1,212     988   1,340\n");
    printf(" Travel    2,780   3,115   2,240\n");
    printf(" Software  4,300   4,300   4,450\n");
    printf(" Misc        615     702     590\n");
    printf(" ------------------------------\n");
    printf(" Net      13,843  16,360  16,440\n\n");
    printf(" Variance  -2.1%%   +3.4%%   +0.5%%\n");
    printf(" Forecast  on track      review\n");

    for (int i = 0; i < 256 * 192; i++)
        fb[i] = C(31, 31, 31);
    consoleSelect(&botCon);
    consoleClear();
    printf("\x1b[30m");
    printf("\n Notes:\n\n");
    printf(" - Follow up w/ vendor re: Q4\n");
    printf(" - Reconcile travel receipts\n");
    printf(" - Update headcount forecast\n");
    printf(" - Send deck before Friday\n");
}

static void hideBoss(void)
{
    BG_PALETTE[0] = RGB15(10, 2, 15);
    consoleSelect(&topCon);
    consoleClear();
    consoleSelect(&botCon);
    consoleClear();
    drawBottomGfx(1);
}

// ---------------------------------------------------------------------------
// Actions

static void pick(void)
{
    add(clickPower());
    btnFlash = 6;
}

static void buy(int k)
{
    char a[16], line[40];
    if (!visible(k))
        return;
    if (k == 0) {
        if (clickLvl >= NCLICK)
            return;
        double c = clickCost();
        if (buds < c) {
            fmt(c - buds, a);
            snprintf(line, sizeof(line), "Need %s more buds, man.", a);
            say(line);
            return;
        }
        buds -= c;
        clickLvl++;
        fmt(clickPower(), a);
        snprintf(line, sizeof(line), "Groovy! Picks now worth %s.", a);
        say(line);
        return;
    }
    int i = k - 1;
    double c = itemCost(i);
    if (buds < c) {
        fmt(c - buds, a);
        snprintf(line, sizeof(line), "Need %s more buds, man.", a);
        say(line);
        return;
    }
    buds -= c;
    owned[i]++;
    snprintf(line, sizeof(line), "Far out! Got a %s.", ITEMS[i].name);
    say(line);
}

// ---------------------------------------------------------------------------

int main(int argc, char **argv)
{
    // Top: text console on a purple backdrop
    videoSetMode(MODE_0_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    consoleInit(&topCon, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
    BG_PALETTE[0] = RGB15(10, 2, 15);

    // Bottom: 16-bit bitmap (BG3) with a text console layered on top (BG0)
    videoSetModeSub(MODE_5_2D);
    vramSetBankC(VRAM_C_SUB_BG);
    int bg = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 2, 0);
    fb = bgGetGfxPtr(bg);
    consoleInit(&botCon, 0, BgType_Text4bpp, BgSize_T_256x256, 0, 1, false, true);
    bgSetPriority(botCon.bgId, 0);
    bgSetPriority(bg, 1);

    consoleSelect(&topCon);
    printf("\x1b[10;6H\x1b[33mTuning in the vibes...");

    fatOK = fatInitDefault();

    long long last = 0;
    if (load(&last) && last > 0) {
        long long away = (long long)time(NULL) - last;
        if (away > MAX_OFFLINE)
            away = MAX_OFFLINE;
        if (away > 60) {
            double earned = perSec() * (double)away;
            if (earned >= 1) {
                char a[16], line[40];
                add(earned);
                fmt(earned, a);
                snprintf(line, sizeof(line), "Farm grew +%s while away", a);
                say(line);
            }
        }
    }

    consoleClear();
    drawBottomGfx(1);

    int frame = 0;
    while (1) {
        swiWaitForVBlank();
        scanKeys();
        int down = keysDown();
        frame++;

        if (boss) {
            if (down) {
                boss = 0;
                hideBoss();
            }
            add(perSec() / 60.0);
            continue;
        }

        if (down & KEY_START) {
            boss = 1;
            showBoss();
            continue;
        }

        if (down & KEY_A)
            pick();
        if (down & KEY_UP)
            sel = (sel + NITEMS) % (NITEMS + 1);
        if (down & KEY_DOWN)
            sel = (sel + 1) % (NITEMS + 1);
        if (down & (KEY_X | KEY_R))
            buy(sel);
        if (down & KEY_SELECT) {
            save();
            say(fatOK ? "Saved. Peace!" : "Can't reach SD card.");
        }

        if (down & KEY_TOUCH) {
            touchPosition t;
            touchRead(&t);
            if (t.py < 48) {
                pick();
            } else if (t.py >= 56 && t.py < 56 + 16 * (NITEMS + 1)) {
                int k = (t.py - 56) / 16;
                sel = k;
                buy(k);
            }
        }

        add(perSec() / 60.0);

        if (btnFlash > 0)
            btnFlash--;
        if (msgTimer > 0)
            msgTimer--;
        if (frame % 8 == 0)
            hue++;

        drawBottomGfx(0);
        drawBottomText();
        drawTop();

        if (frame % (60 * 30) == 0)
            save(); // autosave every 30 seconds
    }

    return 0;
}
