// Terrarium: Nintendo DS front end (libnds).
// Top screen: the tank. Bottom screen: text UI (placeholder until its design is decided).
#include <nds.h>
#include <fat.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "game.h"
#include "art.h"

// libnds 1.x (pre-calico) has no power-management main loop; this keeps the
// game buildable with older devkitARM images, which older emulators prefer.
#if _LIBNDS_MAJOR_ < 2
#define pmMainLoop() true
#endif

// ------------------------------------------------------------------ saves --
#define SAVE_MAGIC 0x54455252u   // "TERR"
#define SAVE_VERSION 1
#define SLOTS 3

typedef struct {
    uint32_t magic;
    uint16_t version, size;
    uint8_t morph, name, happiness, humidity;
    uint16_t coins, pad;
    uint32_t play_frames;
    int32_t shed_timer;
    uint8_t item_count, pad2[3];
    Item items[MAX_ITEMS];
    Debris debris[MAX_DEBRIS];
} SaveFile;

typedef struct { bool used; uint8_t morph, name; uint32_t frames; } SlotInfo;

static bool fat_ok;
static SlotInfo slots[SLOTS];

static void slot_path(int slot, char *out, int n) { snprintf(out, n, "/data/terrarium/slot%d.sav", slot + 1); }

static bool slot_read(int slot, SaveFile *sf) {
    if (!fat_ok) return false;
    char path[64];
    slot_path(slot, path, sizeof path);
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    bool ok = fread(sf, sizeof *sf, 1, f) == 1 && sf->magic == SAVE_MAGIC &&
              sf->version == SAVE_VERSION && sf->size == sizeof *sf;
    fclose(f);
    return ok && sf->item_count <= MAX_ITEMS && sf->morph < MORPH_COUNT;
}

static void slots_scan(void) {
    static SaveFile sf;
    for (int i = 0; i < SLOTS; i++) {
        slots[i].used = slot_read(i, &sf);
        if (slots[i].used) {
            slots[i].morph = sf.morph;
            slots[i].name = sf.name;
            slots[i].frames = sf.play_frames;
        }
    }
}

static bool slot_save(int slot, const Game *g) {
    if (!fat_ok) return false;
    static SaveFile sf;
    memset(&sf, 0, sizeof sf);
    sf.magic = SAVE_MAGIC;
    sf.version = SAVE_VERSION;
    sf.size = sizeof sf;
    sf.morph = g->morph;
    sf.name = g->name;
    sf.happiness = g->happiness;
    sf.humidity = g->humidity;
    sf.coins = g->coins;
    sf.play_frames = g->play_frames;
    sf.shed_timer = g->snake.shed_timer;
    sf.item_count = g->item_count;
    memcpy(sf.items, g->items, sizeof sf.items);
    memcpy(sf.debris, g->debris, sizeof sf.debris);
    mkdir("/data", 0777);
    mkdir("/data/terrarium", 0777);
    char path[64];
    slot_path(slot, path, sizeof path);
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    bool ok = fwrite(&sf, sizeof sf, 1, f) == 1;
    fclose(f);
    slots_scan();
    return ok;
}

static bool slot_load(int slot, Game *g, uint32_t seed) {
    static SaveFile sf;
    if (!slot_read(slot, &sf)) return false;
    memset(g, 0, sizeof *g);
    g->morph = sf.morph;
    g->name = sf.name % NAME_COUNT;
    g->happiness = sf.happiness > 100 ? 100 : sf.happiness;
    g->humidity = sf.humidity > 100 ? 100 : sf.humidity;
    g->coins = sf.coins;
    g->play_frames = sf.play_frames;
    g->item_count = sf.item_count;
    memcpy(g->items, sf.items, sizeof g->items);
    memcpy(g->debris, sf.debris, sizeof g->debris);
    game_after_load(g, seed);
    g->snake.shed_timer = sf.shed_timer > 0 ? sf.shed_timer : 3600;
    return true;
}

static void slot_delete(int slot) {
    char path[64];
    slot_path(slot, path, sizeof path);
    remove(path);
    slots_scan();
}

// --------------------------------------------------------------- graphics --
static int bg_floor, bg_grid, bg_frame;
static u16 *gfx_items[ITEM_COUNT], *gfx_body[12], *gfx_lump_p, *gfx_heads[32], *gfx_rats[2];
static u16 *gfx_shed_p, *gfx_drop_p, *gfx_mist_p, *gfx_cursor_p, *gfx_coin_p, *gfx_heart_p;
static uint8_t floor_pal[GRID_H][GRID_W];

static void vcopy(u16 *dst, const u8 *src, int bytes) {
    for (int i = 0; i < bytes / 2; i++) dst[i] = src[2 * i] | (src[2 * i + 1] << 8);
}

static SpriteSize size_for(int w, int h) {
    if (w == 8) return SpriteSize_8x8;
    if (w == 16) return SpriteSize_16x16;
    if (w == 32 && h == 32) return SpriteSize_32x32;
    return SpriteSize_64x64;
}

static u16 *load_sprite(const u8 *data, int w, int h) {
    u16 *p = oamAllocateGfx(&oamMain, size_for(w, h), SpriteColorFormat_16Color);
    vcopy(p, data, w * h / 2);
    return p;
}

static void apply_morph(int m) {
    for (int i = 1; i <= 10; i++) {
        const uint8_t *c = morphs[m].rgb[i];
        SPRITE_PALETTE[16 + i] = RGB15(c[0] >> 3, c[1] >> 3, c[2] >> 3);
    }
}

static int floor_variant(int x, int y) {
    uint32_t h = (uint32_t)(x * 73856093) ^ (uint32_t)(y * 19349663);
    h ^= h >> 13;
    return (int)(h % FLOOR_VARIANTS);
}

static void floor_set(int x, int y, int pal) {
    u16 *map = bgGetMapPtr(bg_floor);
    int v = floor_variant(x, y);
    for (int dy = 0; dy < 2; dy++)
        for (int dx = 0; dx < 2; dx++)
            map[(y * 2 + dy) * 32 + x * 2 + dx] = (u16)((v * 4 + dy * 2 + dx) | (pal << 12));
    floor_pal[y][x] = (uint8_t)pal;
}

static void gfx_init(void) {
    videoSetMode(MODE_0_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankB(VRAM_B_MAIN_SPRITE);

    bg_frame = bgInit(0, BgType_Text4bpp, BgSize_T_256x256, 0, 1);
    bg_grid = bgInit(1, BgType_Text4bpp, BgSize_T_256x256, 1, 2);
    bg_floor = bgInit(3, BgType_Text4bpp, BgSize_T_256x256, 2, 3);
    bgSetPriority(bg_frame, 0);
    bgSetPriority(bg_grid, 2);
    bgSetPriority(bg_floor, 3);

    for (int i = 0; i < 16; i++) {
        BG_PALETTE[i] = pal_floor_dry[i];
        BG_PALETTE[16 + i] = pal_floor_damp[i];
    }
    BG_PALETTE[0] = RGB15(4, 2, 1);

    vcopy(bgGetGfxPtr(bg_floor), gfx_floor, sizeof gfx_floor);
    for (int y = 0; y < GRID_H; y++)
        for (int x = 0; x < GRID_W; x++) floor_set(x, y, 0);

    vcopy(bgGetGfxPtr(bg_grid), gfx_grid, sizeof gfx_grid);
    u16 *gm = bgGetMapPtr(bg_grid);
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++) gm[y * 32 + x] = (u16)((y & 1) * 2 + (x & 1));
    bgHide(bg_grid);

    vcopy(bgGetGfxPtr(bg_frame), gfx_frame_tiles, sizeof gfx_frame_tiles);
    u16 *fm = bgGetMapPtr(bg_frame);
    for (int i = 0; i < 32 * 32; i++) fm[i] = 0;
    for (int i = 0; i < 32 * 24; i++) fm[i] = map_frame[i];

    oamInit(&oamMain, SpriteMapping_1D_128, false);
    for (int i = 0; i < 16; i++) {
        SPRITE_PALETTE[i] = pal_items[i];
        SPRITE_PALETTE[16 + i] = pal_snake_default[i];
        SPRITE_PALETTE[32 + i] = pal_fx[i];
        SPRITE_PALETTE[48 + i] = pal_fx_red[i];
    }
    for (int i = 0; i < ITEM_COUNT; i++)
        gfx_items[i] = load_sprite(item_defs[i].gfx, item_defs[i].spr_w, item_defs[i].spr_h);
    for (int i = 0; i < 12; i++) gfx_body[i] = load_sprite(gfx_snake_body + i * 128, 16, 16);
    gfx_lump_p = load_sprite(gfx_snake_lump, 16, 16);
    for (int i = 0; i < 32; i++) gfx_heads[i] = load_sprite(gfx_snake_head + i * 128, 16, 16);
    for (int i = 0; i < 2; i++) gfx_rats[i] = load_sprite(gfx_rat + i * 128, 16, 16);
    gfx_shed_p = load_sprite(gfx_shed, 16, 16);
    gfx_drop_p = load_sprite(gfx_dropping, 16, 16);
    gfx_mist_p = load_sprite(gfx_mist, 8, 8);
    gfx_cursor_p = load_sprite(gfx_cursor, 8, 8);
    gfx_coin_p = load_sprite(gfx_coin, 8, 8);
    gfx_heart_p = load_sprite(gfx_heart, 8, 8);

    // semi-transparent sprites (placement ghost, mist) blend over the floor
    REG_BLDCNT = BLEND_ALPHA | BLEND_DST_BG1 | BLEND_DST_BG3 | BLEND_DST_BACKDROP;
    REG_BLDALPHA = 9 | (7 << 8);
}

// ---------------------------------------------------------------- sprites --
typedef struct {
    int key;
    int16_t x, y;
    u16 *gfx;
    SpriteSize size;
    uint8_t pal, hflip, vflip, blend;
} Spr;

#define MAX_SPR 128
static Spr overlay[48], world[96];
static int n_overlay, n_world;

static void add(Spr *list, int *n, int cap, int key, int x, int y, u16 *gfx, SpriteSize size, int pal, int hflip, int vflip, int blend) {
    if (*n >= cap) return;
    list[(*n)++] = (Spr){key, (int16_t)x, (int16_t)y, gfx, size, (uint8_t)pal, (uint8_t)hflip, (uint8_t)vflip, (uint8_t)blend};
}
#define OVER(x, y, gfx, size, pal, hf, vf, bl) add(overlay, &n_overlay, 48, 0, x, y, gfx, size, pal, hf, vf, bl)
#define WORLD(key, x, y, gfx, size, pal, hf) add(world, &n_world, 96, key, x, y, gfx, size, pal, hf, 0, 0)

static void build_sprites(const Game *g, bool show_cursor) {
    n_overlay = n_world = 0;

    // cursor + placement ghost
    if (show_cursor) {
        int w, h;
        game_cursor_footprint(g, &w, &h);
        int x0 = g->cur_x * TILE, y0 = g->cur_y * TILE;
        int x1 = x0 + w * TILE, y1 = y0 + h * TILE;
        bool ok = !g->build || (game_can_place(g, g->selected, g->cur_x, g->cur_y) &&
                                 g->coins >= item_defs[g->selected].price);
        int pal = ok ? 2 : 3;
        int p = (g->play_frames / 12) & 1;
        OVER(x0 - 1 - p, y0 - 1 - p, gfx_cursor_p, SpriteSize_8x8, pal, 0, 0, 0);
        OVER(x1 - 7 + p, y0 - 1 - p, gfx_cursor_p, SpriteSize_8x8, pal, 1, 0, 0);
        OVER(x0 - 1 - p, y1 - 7 + p, gfx_cursor_p, SpriteSize_8x8, pal, 0, 1, 0);
        OVER(x1 - 7 + p, y1 - 7 + p, gfx_cursor_p, SpriteSize_8x8, pal, 1, 1, 0);
        if (g->build) {
            const ItemDef *d = &item_defs[g->selected];
            int flip = d->kind == KIND_CORNER && g->cur_x > 0;
            OVER(x0, y0, gfx_items[g->selected], size_for(d->spr_w, d->spr_h), 0, flip, 0, 1);
        }
    }
    for (int i = 0; i < MAX_POPUPS; i++) {
        const Popup *p = &g->popups[i];
        if (p->life && (p->life > 20 || (p->life & 2)))
            OVER(p->x - 4, p->y - 4, p->kind == POP_HEART ? gfx_heart_p : gfx_coin_p, SpriteSize_8x8, 2, 0, 0, 0);
    }
    for (int i = 0; i < MAX_MIST; i++) {
        const Mist *m = &g->mist[i];
        if (m->life) OVER((m->x >> FP) - 4, (m->y >> FP) - 4, gfx_mist_p, SpriteSize_8x8, 2, 0, 0, 1);
    }

    // world objects, depth sorted by their bottom edge
    for (int i = 0; i < g->item_count; i++) {
        const Item *it = &g->items[i];
        const ItemDef *d = &item_defs[it->type];
        int key = (it->ty + d->h) * TILE;
        if (d->kind == KIND_BOWL) key = it->ty * TILE + 4;   // the snake lies on top of the water
        WORLD(key, it->tx * TILE, it->ty * TILE, gfx_items[it->type], size_for(d->spr_w, d->spr_h), 0, it->flip);
    }
    for (int i = 0; i < MAX_DEBRIS; i++) {
        const Debris *d = &g->debris[i];
        if (d->active) WORLD(d->y - 4, d->x - 8, d->y - 8, d->kind == DEBRIS_SHED ? gfx_shed_p : gfx_drop_p, SpriteSize_16x16, 2, 0);
    }
    if (g->rat.active) {
        int rx = g->rat.x >> FP, ry = g->rat.y >> FP;
        WORLD(ry + 6, rx - 8, ry - 10, gfx_rats[(g->rat.frame >> 3) & 1], SpriteSize_16x16, 2, g->rat.vx < 0);
    }
    const Snake *s = &g->snake;
    int lump = snake_lump_segment(s);
    for (int i = SEGMENTS; i >= 1; i--) {
        int x, y;
        snake_segment_pos(s, i, &x, &y);
        if (game_point_hidden(g, x, y)) continue;
        int size = i <= 5 ? 0 : i <= 9 ? 1 : i == 10 ? 2 : i == 11 ? 3 : i == 12 ? 4 : 5;
        u16 *gfx = (i == lump) ? gfx_lump_p : gfx_body[((i / 2) & 1) * 6 + size];
        WORLD(y + 6, x - 8, y - 8, gfx, SpriteSize_16x16, 1, 0);
    }
    {
        int hx = s->x >> FP, hy = s->y >> FP, nx, ny;
        snake_segment_pos(s, 1, &nx, &ny);
        if (!game_point_hidden(g, hx, hy)) {
            int dir = (((s->ang >> 8) + 8) >> 4) & 15;
            int key = (hy > ny ? hy : ny) + 7;
            WORLD(key, hx - 8, hy - 8, gfx_heads[dir + (s->tongue > 0 ? 16 : 0)], SpriteSize_16x16, 1, 0);
        }
    }

    // insertion sort: nearest (largest key) first, so it gets the lowest OAM index and draws on top
    for (int i = 1; i < n_world; i++) {
        Spr t = world[i];
        int j = i - 1;
        while (j >= 0 && world[j].key < t.key) { world[j + 1] = world[j]; j--; }
        world[j + 1] = t;
    }
}

static void emit_one(int id, const Spr *sp) {
    oamSet(&oamMain, id, sp->x & 0x1FF, sp->y & 0xFF, 1, sp->pal, sp->size, SpriteColorFormat_16Color,
           sp->gfx, -1, false, false, sp->hflip, sp->vflip, false);
    oamMain.oamMemory[id].blendMode = sp->blend ? OBJMODE_BLENDED : OBJMODE_NORMAL;
}

static void flush_sprites(void) {
    int id = 0;
    for (int i = 0; i < n_overlay && id < MAX_SPR; i++) emit_one(id++, &overlay[i]);
    for (int i = 0; i < n_world && id < MAX_SPR; i++) emit_one(id++, &world[i]);
    if (id < MAX_SPR) oamClear(&oamMain, id, MAX_SPR - id);
}

static void sync_floor(const Game *g, bool build) {
    for (int y = 0; y < GRID_H; y++)
        for (int x = 0; x < GRID_W; x++) {
            int pal = g->damp[y][x] ? 1 : 0;
            if (floor_pal[y][x] != pal) floor_set(x, y, pal);
        }
    if (build) bgShow(bg_grid);
    else bgHide(bg_grid);
}

// --------------------------------------------------------------------- ui --
static void line(int row, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void line(int row, const char *fmt, ...) {
    char buf[64];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    iprintf("\x1b[%d;0H%-32.32s", row, buf);
}

static void bar(char *out, int value, int width) {
    int filled = value * width / 100;
    out[0] = '[';
    for (int i = 0; i < width; i++) out[1 + i] = i < filled ? '#' : '-';
    out[width + 1] = ']';
    out[width + 2] = 0;
}

static void ui_clear(void) {
    for (int r = 0; r < 24; r++) line(r, " ");
}

static void ui_play(const Game *g, int slot) {
    char b[32];
    line(0, "      ~ T E R R A R I U M ~");
    line(1, " ");
    line(2, " %s the %s", snake_names[g->name % NAME_COUNT], morphs[g->morph].name);
    line(3, " Day %-4d          Coins %4d", game_age_days(g) + 1, g->coins);
    line(4, " ");
    bar(b, g->happiness, 18);
    line(5, " Happy %s %3d", b, g->happiness);
    bar(b, g->humidity, 18);
    line(6, " Humid %s %3d", b, g->humidity);
    line(7, " ");
    line(8, " %s", snake_state_text(g));
    if (g->snake.digest > 0) line(9, "   (digesting a meal)");
    else if (g->rat.active) line(9, "   A rat is loose!");
    else line(9, " ");
    line(10, " ");
    if (g->build) {
        const ItemDef *d = &item_defs[g->selected];
        line(11, " Placing: %s %dx%d", d->name, d->w, d->h);
        if (d->price) line(12, "   Cost: %d coins", d->price);
        else line(12, "   Cost: free");
    } else {
        line(11, " ");
        line(12, " ");
    }
    line(13, " ");
    line(14, " %s", g->msg_timer ? g->msg : "");
    line(15, " ");
    line(16, "--------------------------------");
    line(17, " A Mist     B Gather   R Rat");
    line(18, " Y Item     X Place    L Pick up");
    line(19, g->build ? " SELECT Done building" : " ");
    line(20, " START Pause");
    line(21, " ");
    line(22, fat_ok ? " Slot %d" : " Slot %d (no SD: saving off)", slot + 1);
    line(23, " ");
}

// ------------------------------------------------------------------- main --
enum Mode { MODE_TITLE, MODE_REVEAL, MODE_PLAY, MODE_PAUSE, MODE_DELETE };

static Game game;

static Input read_input(void) {
    Input in;
    u32 down = keysDown(), rep = keysDownRepeat();
    in.up = rep & KEY_UP;
    in.down = rep & KEY_DOWN;
    in.left = rep & KEY_LEFT;
    in.right = rep & KEY_RIGHT;
    in.a = down & KEY_A;
    in.b = down & KEY_B;
    in.x = down & KEY_X;
    in.y = down & KEY_Y;
    in.l = down & KEY_L;
    in.r = down & KEY_R;
    in.select = down & KEY_SELECT;
    return in;
}

static int new_morph(uint32_t seed) {
    // every new save gets a colouring no other slot is using (while any remain)
    int pool[MORPH_COUNT], n = 0;
    for (int m = 0; m < MORPH_COUNT; m++) {
        bool used = false;
        for (int s = 0; s < SLOTS; s++) used |= slots[s].used && slots[s].morph == m;
        if (!used) pool[n++] = m;
    }
    if (n == 0) return (int)(seed % MORPH_COUNT);
    return pool[seed % (uint32_t)n];
}

static void start_demo(void) {
    game_new(&game, 7, 0, 0);
    apply_morph(0);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    gfx_init();
    consoleDemoInit();
    keysSetRepeat(15, 5);
    fat_ok = fatInitDefault();
    slots_scan();

    int mode = MODE_TITLE, sel = 0, slot = 0, demo_morph = 0;
    uint32_t frame = 0;
    char status[33] = "";
    start_demo();
    ui_clear();

    while (pmMainLoop()) {
        swiWaitForVBlank();
        scanKeys();
        frame++;
        u32 down = keysDown();
        Input none;
        memset(&none, 0, sizeof none);

        switch (mode) {
        case MODE_TITLE: {
            // the tank runs on its own behind the title, cycling colourings
            if (frame % 240 == 0) { demo_morph = (demo_morph + 1) % MORPH_COUNT; apply_morph(demo_morph); }
            Input demo = none;
            if (frame % 1200 < 60 && frame % 8 == 0) { demo.a = 1; game.cur_x = 7; game.cur_y = 6; }
            game_update(&game, &demo);
            if (down & KEY_UP) sel = (sel + SLOTS - 1) % SLOTS;
            if (down & KEY_DOWN) sel = (sel + 1) % SLOTS;
            if ((down & KEY_X) && slots[sel].used) mode = MODE_DELETE;
            if (down & (KEY_A | KEY_START)) {
                slot = sel;
                uint32_t seed = (uint32_t)time(NULL) ^ (frame * 2654435761u);
                if (slots[sel].used && slot_load(sel, &game, seed)) {
                    apply_morph(game.morph);
                    mode = MODE_PLAY;
                } else {
                    int m = new_morph(seed >> 3);
                    game_new(&game, seed, m, (int)((seed >> 11) % NAME_COUNT));
                    apply_morph(m);
                    slot_save(slot, &game);
                    mode = MODE_REVEAL;
                }
                ui_clear();
                break;
            }
            line(0, "      ~ T E R R A R I U M ~");
            line(1, "     a snake-keeping sim");
            line(2, " ");
            line(3, " Choose a save:");
            for (int i = 0; i < SLOTS; i++) {
                if (slots[i].used)
                    line(5 + i * 2, " %c Slot %d  %-8s %s", sel == i ? '>' : ' ', i + 1,
                         snake_names[slots[i].name % NAME_COUNT], morphs[slots[i].morph % MORPH_COUNT].name);
                else
                    line(5 + i * 2, " %c Slot %d  - new snake -", sel == i ? '>' : ' ', i + 1);
                line(6 + i * 2, slots[i].used ? "            Day %lu" : " ", (unsigned long)(slots[i].frames / FRAMES_PER_DAY + 1));
            }
            line(12, " ");
            line(13, " A Play     X Delete save");
            line(15, " Every new save hatches a");
            line(16, " snake with its own colours.");
            line(18, fat_ok ? " " : " No SD card found:");
            line(19, fat_ok ? " " : " progress won't be saved.");
            break;
        }
        case MODE_DELETE:
            game_update(&game, &none);
            line(13, " Delete slot %d? A Yes  B No", sel + 1);
            if (down & KEY_A) { slot_delete(sel); mode = MODE_TITLE; ui_clear(); }
            else if (down & KEY_B) { mode = MODE_TITLE; ui_clear(); }
            break;

        case MODE_REVEAL:
            game_update(&game, &none);
            line(0, "      ~ T E R R A R I U M ~");
            line(4, "     A new hatchling!");
            line(6, "     Meet %s,", snake_names[game.name % NAME_COUNT]);
            line(7, "     a %s snake.", morphs[game.morph].name);
            line(10, " Mist (A) coaxes it out of its");
            line(11, " hide. Release a rat with R.");
            line(12, " It can never be hurt: every");
            line(13, " meal just makes it happier.");
            line(17, "         Press A");
            if (down & (KEY_A | KEY_START)) { mode = MODE_PLAY; ui_clear(); }
            break;

        case MODE_PLAY: {
            Input in = read_input();
            game_update(&game, &in);
            if (down & KEY_START) { mode = MODE_PAUSE; status[0] = 0; ui_clear(); break; }
            if (frame % 4 == 0) ui_play(&game, slot);
            break;
        }
        case MODE_PAUSE:
            line(0, "      ~ T E R R A R I U M ~");
            line(5, "           PAUSED");
            line(8, " A       Save");
            line(9, " B/START Resume");
            line(10, " SELECT  Save and quit");
            line(13, " %s", status);
            if (down & KEY_A)
                snprintf(status, sizeof status, slot_save(slot, &game) ? "Saved to slot %d." : "Couldn't save (no SD card).", slot + 1);
            if (down & (KEY_B | KEY_START)) { mode = MODE_PLAY; ui_clear(); }
            if (down & KEY_SELECT) {
                slot_save(slot, &game);
                start_demo();
                mode = MODE_TITLE;
                ui_clear();
            }
            break;
        }

        build_sprites(&game, mode == MODE_PLAY || mode == MODE_PAUSE);
        sync_floor(&game, game.build && mode == MODE_PLAY);
        flush_sprites();
        oamUpdate(&oamMain);
    }
    return 0;
}
