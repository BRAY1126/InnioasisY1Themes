// Terrarium: game rules. Platform independent (no libnds).
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "game.h"

#define BRAD(deg256) ((int32_t)(deg256) << 8)    // 256 brads = full turn
#define PI_F 3.14159265f

static const int32_t SPACING_NORMAL = 5 * ONE;
static const int32_t SPACING_COIL = 3 * ONE;

// ------------------------------------------------------------------ utils --
uint32_t game_rand(Game *g) {
    uint32_t x = g->rng ? g->rng : 0x1234567u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    g->rng = x;
    return x;
}
static int rnd(Game *g, int n) { return (int)(game_rand(g) % (uint32_t)n); }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void say(Game *g, const char *text) {
    snprintf(g->msg, sizeof g->msg, "%s", text);
    g->msg_timer = 180;
}

static float ang_rad(int32_t a) { return (float)(a & 0xFFFF) * (2 * PI_F / 65536.0f); }
static int32_t angle_to(int32_t x, int32_t y, int32_t tx, int32_t ty) {
    float a = atan2f((float)(ty - y), (float)(tx - x));
    return ((int32_t)(a * 65536.0f / (2 * PI_F))) & 0xFFFF;
}
static int32_t ang_diff(int32_t to, int32_t from) {
    return ((to - from + 32768) & 0xFFFF) - 32768;
}
static int dist_px(int32_t x, int32_t y, int32_t tx, int32_t ty) {   // fixed in, pixels out
    float dx = (float)(tx - x) / ONE, dy = (float)(ty - y) / ONE;
    return (int)sqrtf(dx * dx + dy * dy);
}

const ItemDef *def_of(const Game *g, int i) { return &item_defs[g->items[i].type]; }

static void rebuild_owner(Game *g) {
    memset(g->owner, 0, sizeof g->owner);
    for (int i = 0; i < g->item_count; i++) {
        const ItemDef *d = def_of(g, i);
        for (int y = 0; y < d->h; y++)
            for (int x = 0; x < d->w; x++)
                g->owner[g->items[i].ty + y][g->items[i].tx + x] = (uint8_t)(i + 1);
    }
}

// ------------------------------------------------------------ hides etc --
static bool is_hide(const Game *g, int i) { return i >= 0 && i < g->item_count && def_of(g, i)->kind == KIND_HIDE; }

static void hide_rect(const Game *g, int i, int *x0, int *y0, int *x1, int *y1) {
    const Item *it = &g->items[i];
    const ItemDef *d = &item_defs[it->type];
    *x0 = it->tx * TILE + 2;
    *x1 = (it->tx + d->w) * TILE - 2;
    *y0 = it->ty * TILE;
    *y1 = (it->ty + d->h) * TILE - 3;
}
static void hide_inside(const Game *g, int i, int *x, int *y) {
    const Item *it = &g->items[i];
    const ItemDef *d = &item_defs[it->type];
    *x = it->tx * TILE + d->w * TILE / 2;
    *y = it->ty * TILE + d->h * TILE * 9 / 20;
}
static void hide_entrance(const Game *g, int i, int *x, int *y) {
    const Item *it = &g->items[i];
    const ItemDef *d = &item_defs[it->type];
    *x = it->tx * TILE + d->w * TILE / 2;
    *y = clampi((it->ty + d->h) * TILE + 9, 0, SCREEN_H - 10);
}
static int hide_at(const Game *g, int x, int y) {
    for (int i = 0; i < g->item_count; i++) {
        if (!is_hide(g, i)) continue;
        int x0, y0, x1, y1;
        hide_rect(g, i, &x0, &y0, &x1, &y1);
        if (x >= x0 && x < x1 && y >= y0 && y < y1) return i;
    }
    return -1;
}
bool game_point_hidden(const Game *g, int x, int y) { return hide_at(g, x, y) >= 0; }

static bool blocked_px(const Game *g, int px, int py, int allow) {
    if (px < 6 || px > SCREEN_W - 7 || py < 8 || py > SCREEN_H - 6) return true;
    int o = g->owner[py >> 4][px >> 4];
    if (!o) return false;
    o--;
    int k = def_of(g, o)->kind;
    if (k == KIND_PLANT || k == KIND_BOWL) return false;
    if (k == KIND_HIDE && o == allow) return false;
    return true;
}

static int count_kind(const Game *g, int kind) {
    int n = 0;
    for (int i = 0; i < g->item_count; i++)
        if (def_of(g, i)->kind == kind) n++;
    return n;
}

static int nearest_hide(const Game *g, int32_t x, int32_t y) {
    int best = -1, bd = 1 << 30;
    for (int i = 0; i < g->item_count; i++) {
        if (!is_hide(g, i)) continue;
        int ex, ey;
        hide_entrance(g, i, &ex, &ey);
        int d = dist_px(x, y, ex * ONE, ey * ONE);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

static bool random_free_point(Game *g, int *ox, int *oy) {
    for (int tries = 0; tries < 40; tries++) {
        int x = 14 + rnd(g, SCREEN_W - 28), y = 16 + rnd(g, SCREEN_H - 30);
        if (!blocked_px(g, x, y, -1) && !game_point_hidden(g, x, y)) { *ox = x; *oy = y; return true; }
    }
    return false;
}

static bool free_point_near(Game *g, int x, int y, int *ox, int *oy) {
    for (int r = 0; r <= 48; r += 4)
        for (int k = 0; k < 8; k++) {
            int px = x + (int)(cosf(k * PI_F / 4) * r), py = y + (int)(sinf(k * PI_F / 4) * r);
            if (!blocked_px(g, px, py, -1) && !game_point_hidden(g, px, py)) { *ox = px; *oy = py; return true; }
            if (r == 0) break;
        }
    return false;
}

// --------------------------------------------------------------- trail --
static void trail_reset(Snake *s) {
    // lay the body out straight behind the head
    float a = ang_rad(s->ang);
    int32_t dx = (int32_t)(cosf(a) * ONE), dy = (int32_t)(sinf(a) * ONE);
    for (int j = 0; j < TRAIL_LEN; j++) {
        int idx = (TRAIL_LEN - 1 - j);
        s->trail_x[idx] = s->x - dx * j;
        s->trail_y[idx] = s->y - dy * j;
    }
    s->trail_head = TRAIL_LEN - 1;
    s->last_x = s->x;
    s->last_y = s->y;
}

static void trail_update(Snake *s) {
    for (int guard = 0; guard < 16; guard++) {
        float dx = (float)(s->x - s->last_x), dy = (float)(s->y - s->last_y);
        float d = sqrtf(dx * dx + dy * dy);
        if (d < ONE) break;
        s->last_x += (int32_t)(dx * ONE / d);
        s->last_y += (int32_t)(dy * ONE / d);
        s->trail_head = (s->trail_head + 1) & (TRAIL_LEN - 1);
        s->trail_x[s->trail_head] = s->last_x;
        s->trail_y[s->trail_head] = s->last_y;
    }
}

void snake_segment_pos(const Snake *s, int i, int *x, int *y) {
    if (i <= 0) { *x = s->x >> FP; *y = s->y >> FP; return; }
    int32_t d = i * s->spacing;
    int k = d >> FP, f = d & (ONE - 1);
    int a = (s->trail_head - k) & (TRAIL_LEN - 1), b = (a - 1) & (TRAIL_LEN - 1);
    *x = (s->trail_x[a] + (int32_t)(((int64_t)(s->trail_x[b] - s->trail_x[a]) * f) >> FP)) >> FP;
    *y = (s->trail_y[a] + (int32_t)(((int64_t)(s->trail_y[b] - s->trail_y[a]) * f) >> FP)) >> FP;
}

int snake_lump_segment(const Snake *s) {
    if (s->digest <= 0 || s->digest_total <= 0) return -1;
    int done = s->digest_total - s->digest;
    return 2 + done * (SEGMENTS - 4) / s->digest_total;
}

// --------------------------------------------------------------- motion --
static bool probe_free(const Game *g, const Snake *s, int32_t h) {
    float a = ang_rad(h), c = cosf(a), sn = sinf(a);
    int hx = s->x >> FP, hy = s->y >> FP;
    for (int d = 6; d <= 18; d += 6) {
        int px = hx + (int)(c * d), py = hy + (int)(sn * d);
        if (blocked_px(g, px, py, s->allow_hide)) return false;
    }
    return true;
}

static void steer(Game *g, int rate) {
    Snake *s = &g->snake;
    static const int offs[] = {0, 12, -12, 24, -24, 40, -40, 64, -64, 96, -96, 128};
    int32_t desired = angle_to(s->x, s->y, s->tx * ONE, s->ty * ONE);
    int32_t chosen = -1;
    for (unsigned i = 0; i < sizeof offs / sizeof offs[0]; i++) {
        int32_t h = (desired + BRAD(offs[i])) & 0xFFFF;
        if (probe_free(g, s, h)) { chosen = h; break; }
    }
    if (chosen < 0) chosen = (s->ang + BRAD(40)) & 0xFFFF;
    int32_t diff = ang_diff(chosen, s->ang);
    int32_t r = BRAD(rate);
    if (diff > r) diff = r;
    if (diff < -r) diff = -r;
    s->ang = (s->ang + diff) & 0xFFFF;
}

static bool move_head(Game *g) {
    Snake *s = &g->snake;
    if (s->speed <= 0) return true;
    float a = ang_rad(s->ang);
    int32_t nx = s->x + (int32_t)(cosf(a) * s->speed);
    int32_t ny = s->y + (int32_t)(sinf(a) * s->speed);
    bool was = blocked_px(g, s->x >> FP, s->y >> FP, s->allow_hide);
    bool now = blocked_px(g, nx >> FP, ny >> FP, s->allow_hide);
    if (now && !was) {
        s->ang = (s->ang + BRAD(20)) & 0xFFFF;
        return false;
    }
    s->x = nx;
    s->y = ny;
    return true;
}

static int head_dist_to_target(const Snake *s) { return dist_px(s->x, s->y, s->tx * ONE, s->ty * ONE); }

// ---------------------------------------------------------- snake states --
static void enter_rest_or_hide(Game *g);

static void set_target(Snake *s, int x, int y, int frames) { s->tx = x; s->ty = y; s->timer = frames; }

static void start_roam(Game *g) {
    Snake *s = &g->snake;
    s->state = ST_ROAM;
    s->phase = 0;
    s->bowl = -1;
    s->speed = ONE * 9 / 10;
    if (s->hide >= 0 && is_hide(g, s->hide)) {
        int ex, ey;
        hide_entrance(g, s->hide, &ex, &ey);
        set_target(s, ex, ey + 6, 300);
    } else {
        s->timer = 0;
    }
    s->hide = -1;
}

static void pick_roam_target(Game *g, bool allow_bowl) {
    Snake *s = &g->snake;
    int r = rnd(g, 100), x, y;
    s->bowl = -1;
    if (r < 35 && g->damp_age < 600 && free_point_near(g, g->damp_x + rnd(g, 17) - 8, g->damp_y + rnd(g, 17) - 8, &x, &y)) {
        set_target(s, x, y, 360);
        return;
    }
    if (r < 70 && allow_bowl) {
        int n = count_kind(g, KIND_BOWL);
        if (n > 0) {
            int pick = rnd(g, n);
            for (int i = 0; i < g->item_count; i++) {
                if (def_of(g, i)->kind != KIND_BOWL) continue;
                if (pick-- == 0) {
                    const Item *it = &g->items[i];
                    const ItemDef *d = def_of(g, i);
                    s->bowl = i;
                    set_target(s, it->tx * TILE + d->w * TILE / 2, it->ty * TILE + d->h * TILE / 2, 480);
                    return;
                }
            }
        }
    }
    if (random_free_point(g, &x, &y)) set_target(s, x, y, 360);
    else set_target(s, s->x >> FP, s->y >> FP, 60);
}

static void start_goto_hide(Game *g) {
    Snake *s = &g->snake;
    int h = nearest_hide(g, s->x, s->y);
    if (h < 0) { enter_rest_or_hide(g); return; }
    s->state = ST_GOTO_HIDE;
    s->hide = h;
    s->phase = 0;
    s->speed = ONE * 8 / 10;
    int ex, ey;
    hide_entrance(g, h, &ex, &ey);
    set_target(s, ex, ey, 900);
}

static void enter_rest_or_hide(Game *g) {
    Snake *s = &g->snake;
    if (nearest_hide(g, s->x, s->y) >= 0) { start_goto_hide(g); return; }
    s->state = ST_RESTING;
    s->speed = 0;
    s->hide = -1;
}

static void start_hunt(Game *g) {
    Snake *s = &g->snake;
    s->state = ST_HUNT;
    s->speed = ONE * 3 / 4;
    s->hide = -1;
    s->bowl = -1;
}

static void spawn_debris(Game *g, int kind, int x, int y) {
    int fx, fy;
    if (!free_point_near(g, x, y, &fx, &fy)) return;
    for (int i = 0; i < MAX_DEBRIS; i++) {
        if (!g->debris[i].active) {
            g->debris[i] = (Debris){(uint8_t)kind, 1, (int16_t)fx, (int16_t)fy};
            return;
        }
    }
}

static void popup(Game *g, int kind, int x, int y) {
    for (int i = 0; i < MAX_POPUPS; i++)
        if (!g->popups[i].life) { g->popups[i] = (Popup){(uint8_t)kind, 60, (int16_t)x, (int16_t)y}; return; }
}

static void add_happy(Game *g, int n) {
    int h = g->happiness + n;
    g->happiness = (uint8_t)clampi(h, 0, 100);
}

static void eat_rat(Game *g) {
    Snake *s = &g->snake;
    char buf[40];
    g->rat.active = false;
    // Feeding can never hurt the snake: every rat just makes it happier.
    add_happy(g, 20);
    popup(g, POP_HEART, s->x >> FP, (s->y >> FP) - 10);
    s->digest = s->digest > 0 ? clampi(s->digest + 900, 0, 3600) : 1800;
    s->digest_total = s->digest;
    s->spacing = SPACING_NORMAL;
    snprintf(buf, sizeof buf, "%s gulped down the rat!", snake_names[g->name % NAME_COUNT]);
    say(g, buf);
    enter_rest_or_hide(g);
}

static void snake_update(Game *g) {
    Snake *s = &g->snake;
    int32_t want_spacing = SPACING_NORMAL;

    if (s->tongue > 0) s->tongue--;
    else if (rnd(g, 150) == 0) s->tongue = 18;

    if (s->digest > 0 && --s->digest == 0) {
        int tx, ty;
        snake_segment_pos(s, SEGMENTS, &tx, &ty);
        spawn_debris(g, DEBRIS_DROPPING, tx, ty);
    }
    if (s->shed_timer > 0) s->shed_timer--;

    // Leaving whatever hide it is in is always allowed; entering only when heading home.
    if (!(s->state == ST_GOTO_HIDE && s->phase == 1) && s->state != ST_HIDING)
        s->allow_hide = hide_at(g, s->x >> FP, s->y >> FP);

    if (g->rat.active && s->state != ST_HUNT && s->state != ST_COIL && s->state != ST_STRIKE)
        start_hunt(g);

    int rat_d = g->rat.active ? dist_px(s->x, s->y, g->rat.x, g->rat.y) : 9999;

    switch (s->state) {
    case ST_HIDING:
        s->allow_hide = s->hide;
        if (s->phase >= 0) {
            // curl up inside until only the tail tip shows
            int tx, ty;
            snake_segment_pos(s, SEGMENTS - 3, &tx, &ty);
            if (game_point_hidden(g, tx, ty) || s->phase > 900) {
                s->phase = -1;
                s->speed = 0;
            } else {
                s->phase++;
                s->speed = ONE * 45 / 100;
                s->ang = (s->ang + BRAD(6)) & 0xFFFF;
            }
        }
        if (!is_hide(g, s->hide)) { s->hide = -1; start_roam(g); break; }
        if (s->digest == 0 && g->humidity >= 60) start_roam(g);
        else if (s->digest == 0 && g->happiness > 40 && rnd(g, 5400) == 0) start_roam(g);
        break;

    case ST_RESTING:
        s->speed = 0;
        if (nearest_hide(g, s->x, s->y) >= 0) start_goto_hide(g);
        else if (s->digest == 0 && g->humidity >= 60) start_roam(g);
        break;

    case ST_ROAM:
        s->speed = ONE * 9 / 10;
        s->phase++;
        if (s->timer-- <= 0 || head_dist_to_target(s) < 6) {
            if (s->bowl >= 0 && s->timer > 0) {
                s->state = ST_SOAK;
                s->timer = 240 + rnd(g, 180);
                s->speed = 0;
                add_happy(g, 3);
                g->humidity = (uint8_t)clampi(g->humidity + 4, 0, 100);
                break;
            }
            pick_roam_target(g, true);
        }
        if (s->shed_timer == 0 && g->humidity >= 40) {
            int x, y;
            char buf[40];
            snake_segment_pos(s, SEGMENTS - 4, &x, &y);
            spawn_debris(g, DEBRIS_SHED, x, y);
            s->shed_timer = 9000 + rnd(g, 3600);
            snprintf(buf, sizeof buf, "%s shed its skin!", snake_names[g->name % NAME_COUNT]);
            say(g, buf);
        }
        if (g->humidity < 35 && s->phase > 600) { enter_rest_or_hide(g); break; }
        steer(g, 5);
        // a gentle side-to-side wander
        s->ang = (s->ang + (int32_t)(sinf(g->play_frames * 0.07f) * 90.0f)) & 0xFFFF;
        break;

    case ST_SOAK:
        s->speed = 0;
        if (--s->timer <= 0) {
            s->state = ST_ROAM;
            s->phase = 601;
            pick_roam_target(g, false);
        }
        break;

    case ST_GOTO_HIDE:
        if (!is_hide(g, s->hide)) { enter_rest_or_hide(g); break; }
        if (s->phase == 0) {
            s->speed = ONE * 8 / 10;
            int x0, y0, x1, y1;
            hide_rect(g, s->hide, &x0, &y0, &x1, &y1);
            if (head_dist_to_target(s) < 8) {
                int ix, iy;
                hide_inside(g, s->hide, &ix, &iy);
                s->phase = 1;
                s->allow_hide = s->hide;
                set_target(s, ix, iy, 300);
            } else if (s->timer-- <= 0) {
                // stuck behind something: wander a moment, then try again
                int x, y;
                s->phase = 2;
                if (random_free_point(g, &x, &y)) set_target(s, x, y, 120);
            }
        } else if (s->phase == 1) {
            s->speed = ONE * 6 / 10;
            s->allow_hide = s->hide;
            if (head_dist_to_target(s) < 5 || s->timer-- <= 0) {
                s->state = ST_HIDING;
                s->phase = 0;
                s->speed = 0;
            }
        } else {
            if (s->timer-- <= 0 || head_dist_to_target(s) < 6) {
                int ex, ey;
                hide_entrance(g, s->hide, &ex, &ey);
                s->phase = 0;
                set_target(s, ex, ey, 900);
            }
        }
        steer(g, s->phase == 1 ? 10 : 6);
        break;

    case ST_HUNT:
        if (!g->rat.active) { start_roam(g); s->phase = 601; break; }
        s->speed = ONE * 3 / 4;
        set_target(s, g->rat.x >> FP, g->rat.y >> FP, 60);
        if (rat_d < 38) {
            s->state = ST_COIL;
            s->timer = 45;
            s->speed = 0;
            break;
        }
        steer(g, 6);
        break;

    case ST_COIL:
        if (!g->rat.active) { start_roam(g); s->phase = 601; break; }
        want_spacing = SPACING_COIL;
        s->speed = 0;
        set_target(s, g->rat.x >> FP, g->rat.y >> FP, 0);
        {
            int32_t diff = ang_diff(angle_to(s->x, s->y, g->rat.x, g->rat.y), s->ang);
            diff = clampi(diff, -BRAD(10), BRAD(10));
            s->ang = (s->ang + diff) & 0xFFFF;
        }
        if (--s->timer <= 0) {
            s->state = ST_STRIKE;
            s->timer = 12;
            s->speed = ONE * 42 / 10;
            s->tongue = 0;
        }
        break;

    case ST_STRIKE:
        if (!g->rat.active) { start_roam(g); s->phase = 601; break; }
        set_target(s, g->rat.x >> FP, g->rat.y >> FP, s->timer);
        if (rat_d < 10) { eat_rat(g); break; }
        {
            int32_t diff = ang_diff(angle_to(s->x, s->y, g->rat.x, g->rat.y), s->ang);
            diff = clampi(diff, -BRAD(4), BRAD(4));
            s->ang = (s->ang + diff) & 0xFFFF;
        }
        if (--s->timer <= 0) { start_hunt(g); }
        break;
    }

    if (!move_head(g) && s->state == ST_STRIKE) start_hunt(g);
    if (g->rat.active && (s->state == ST_STRIKE || s->state == ST_HUNT) &&
        dist_px(s->x, s->y, g->rat.x, g->rat.y) < 8)
        eat_rat(g);
    s->spacing += (want_spacing - s->spacing) / 6;
    trail_update(s);
}

// ------------------------------------------------------------------ rat --
static void rat_update(Game *g) {
    Rat *r = &g->rat;
    Snake *s = &g->snake;
    if (!r->active) return;
    int d = dist_px(s->x, s->y, r->x, r->y);
    bool scared = (d < 24 && s->state != ST_COIL) || (s->state == ST_STRIKE && d < 60);
    if (scared && r->flee <= 0) {
        float a = atan2f((float)(r->y - s->y), (float)(r->x - s->x)) + (rnd(g, 100) - 50) * 0.01f;
        r->vx = (int32_t)(cosf(a) * ONE * 1.5f);
        r->vy = (int32_t)(sinf(a) * ONE * 1.5f);
        r->flee = 40;
    } else if (r->flee > 0) {
        r->flee--;
    } else if (--r->timer <= 0) {
        r->timer = 30 + rnd(g, 70);
        if (rnd(g, 3) == 0) { r->vx = r->vy = 0; }
        else {
            float a = rnd(g, 628) * 0.01f;
            r->vx = (int32_t)(cosf(a) * ONE * 0.55f);
            r->vy = (int32_t)(sinf(a) * ONE * 0.55f);
        }
    }
    int32_t nx = r->x + r->vx, ny = r->y + r->vy;
    bool was = blocked_px(g, r->x >> FP, r->y >> FP, -1);
    if (blocked_px(g, nx >> FP, ny >> FP, -1) && !was) {
        r->vx = -r->vx;
        r->vy = -r->vy;
        r->timer = 20;
    } else {
        r->x = nx;
        r->y = ny;
    }
    if (r->vx || r->vy) r->frame++;
}

// --------------------------------------------------------------- items --
void game_cursor_footprint(const Game *g, int *w, int *h) {
    if (g->build) { *w = item_defs[g->selected].w; *h = item_defs[g->selected].h; }
    else { *w = 1; *h = 1; }
}

bool game_can_place(const Game *g, int type, int tx, int ty) {
    const ItemDef *d = &item_defs[type];
    if (g->item_count >= MAX_ITEMS) return false;
    if (tx < 0 || ty < 0 || tx + d->w > GRID_W || ty + d->h > GRID_H) return false;
    if (d->kind == KIND_CORNER &&
        !((tx == 0 || tx == GRID_W - d->w) && (ty == 0 || ty == GRID_H - d->h)))
        return false;
    for (int y = 0; y < d->h; y++)
        for (int x = 0; x < d->w; x++)
            if (g->owner[ty + y][tx + x]) return false;
    return true;
}

bool game_place(Game *g, int type, int tx, int ty, int flip) {
    if (!game_can_place(g, type, tx, ty)) return false;
    g->items[g->item_count++] = (Item){(uint8_t)type, (uint8_t)tx, (uint8_t)ty, (uint8_t)flip};
    rebuild_owner(g);
    return true;
}

int game_item_at(const Game *g, int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= GRID_W || ty >= GRID_H) return -1;
    return g->owner[ty][tx] - 1;
}

static void fix_index(int *v, int removed) {
    if (*v == removed) *v = -1;
    else if (*v > removed) (*v)--;
}

bool game_remove(Game *g, int index) {
    if (index < 0 || index >= g->item_count) return false;
    Snake *s = &g->snake;
    bool lost_home = (s->hide == index);
    for (int i = index; i < g->item_count - 1; i++) g->items[i] = g->items[i + 1];
    g->item_count--;
    rebuild_owner(g);
    fix_index(&s->hide, index);
    fix_index(&s->allow_hide, index);
    fix_index(&s->bowl, index);
    if (lost_home && (s->state == ST_HIDING || s->state == ST_GOTO_HIDE)) {
        s->state = ST_ROAM;
        s->phase = 0;
        s->timer = 0;
    }
    if (s->state == ST_SOAK && s->bowl < 0) { s->state = ST_ROAM; s->timer = 0; }
    return true;
}

// ---------------------------------------------------------------- input --
static void clamp_cursor(Game *g) {
    int w, h;
    game_cursor_footprint(g, &w, &h);
    g->cur_x = clampi(g->cur_x, 0, GRID_W - w);
    g->cur_y = clampi(g->cur_y, 0, GRID_H - h);
    if (g->build && item_defs[g->selected].kind == KIND_CORNER) {
        g->cur_x = g->cur_x * 2 + w < GRID_W ? 0 : GRID_W - w;
        g->cur_y = g->cur_y * 2 + h < GRID_H ? 0 : GRID_H - h;
    }
}

static void announce_selected(Game *g) {
    char buf[40];
    const ItemDef *d = &item_defs[g->selected];
    if (d->price) snprintf(buf, sizeof buf, "%s %dx%d - %d coins", d->name, d->w, d->h, d->price);
    else snprintf(buf, sizeof buf, "%s %dx%d - free", d->name, d->w, d->h);
    say(g, buf);
}

static void do_mist(Game *g) {
    if (g->mist_cool > 0) return;
    g->mist_cool = 8;
    int cx = g->cur_x * TILE + TILE / 2, cy = g->cur_y * TILE + TILE / 2;
    if (g->humidity < 50 && ++g->mist_count % 3 == 0) add_happy(g, 1);
    g->humidity = (uint8_t)clampi(g->humidity + 6, 0, 100);
    for (int n = 0, i = 0; i < MAX_MIST && n < 6; i++) {
        Mist *m = &g->mist[i];
        if (m->life) continue;
        m->x = (cx + rnd(g, 25) - 12) * ONE;
        m->y = (cy + rnd(g, 17) - 14) * ONE;
        m->vx = (rnd(g, 41) - 20) * 4;
        m->vy = -(ONE / 2) - rnd(g, 64);
        m->life = (uint8_t)(18 + rnd(g, 12));
        n++;
    }
    for (int y = g->cur_y - 1; y <= g->cur_y + 1; y++)
        for (int x = g->cur_x - 1; x <= g->cur_x + 1; x++)
            if (x >= 0 && y >= 0 && x < GRID_W && y < GRID_H) g->damp[y][x] = 600;
    g->damp_x = cx;
    g->damp_y = cy;
    g->damp_age = 0;
}

static void do_gather(Game *g) {
    int w, h;
    game_cursor_footprint(g, &w, &h);
    int x0 = g->cur_x * TILE - 5, y0 = g->cur_y * TILE - 5;
    int x1 = (g->cur_x + w) * TILE + 5, y1 = (g->cur_y + h) * TILE + 5;
    for (int i = 0; i < MAX_DEBRIS; i++) {
        Debris *d = &g->debris[i];
        if (!d->active || d->x < x0 || d->x >= x1 || d->y < y0 || d->y >= y1) continue;
        int coins = d->kind == DEBRIS_SHED ? 12 : 6;
        d->active = 0;
        g->coins = (uint16_t)clampi(g->coins + coins, 0, 9999);
        add_happy(g, 3);
        popup(g, POP_COIN, d->x, d->y - 8);
        say(g, d->kind == DEBRIS_SHED ? "Gathered the shed skin! +12" : "Cleaned up a dropping. +6");
        return;
    }
    say(g, "Nothing to gather here");
}

static void do_rat(Game *g) {
    if (g->rat.active) { say(g, "One rat at a time!"); return; }
    int x = g->cur_x * TILE + TILE / 2, y = g->cur_y * TILE + TILE / 2;
    if (blocked_px(g, x, y, -1)) { say(g, "Can't let the rat go there"); return; }
    g->rat = (Rat){true, x * ONE, y * ONE, 0, 0, 0, 0, 0};
    say(g, "A rat scurries into the tank...");
}

static void handle_input(Game *g, const Input *in) {
    int w, h;
    game_cursor_footprint(g, &w, &h);
    bool corner = g->build && item_defs[g->selected].kind == KIND_CORNER;
    if (corner) {
        if (in->left) g->cur_x = 0;
        if (in->right) g->cur_x = GRID_W - w;
        if (in->up) g->cur_y = 0;
        if (in->down) g->cur_y = GRID_H - h;
    } else {
        if (in->left) g->cur_x--;
        if (in->right) g->cur_x++;
        if (in->up) g->cur_y--;
        if (in->down) g->cur_y++;
    }
    clamp_cursor(g);

    if (in->y) {
        if (g->build) g->selected = (g->selected + 1) % ITEM_COUNT;
        g->build = true;
        clamp_cursor(g);
        announce_selected(g);
    }
    if (in->select && g->build) {
        g->build = false;
        clamp_cursor(g);
        say(g, "Done building");
    }
    if (in->x) {
        if (!g->build) say(g, "Press Y to choose an item");
        else {
            const ItemDef *d = &item_defs[g->selected];
            char buf[40];
            if (!game_can_place(g, g->selected, g->cur_x, g->cur_y))
                say(g, d->kind == KIND_CORNER ? "Needs an empty corner" : "That doesn't fit there");
            else if (g->coins < d->price) {
                snprintf(buf, sizeof buf, "Need %d coins (have %d)", d->price, g->coins);
                say(g, buf);
            } else {
                int flip = d->kind == KIND_CORNER && g->cur_x > 0;
                game_place(g, g->selected, g->cur_x, g->cur_y, flip);
                g->coins -= d->price;
                snprintf(buf, sizeof buf, "Placed the %s", d->name);
                say(g, buf);
            }
        }
    }
    if (in->l) {
        int idx = -1;
        for (int y = 0; y < h && idx < 0; y++)
            for (int x = 0; x < w && idx < 0; x++)
                idx = game_item_at(g, g->cur_x + x, g->cur_y + y);
        if (idx < 0) say(g, "Nothing to pick up here");
        else {
            char buf[40];
            const ItemDef *d = def_of(g, idx);
            g->coins = (uint16_t)clampi(g->coins + d->price, 0, 9999);
            snprintf(buf, sizeof buf, "Picked up the %s", d->name);
            game_remove(g, idx);
            say(g, buf);
        }
    }
    if (in->a) do_mist(g);
    if (in->b) do_gather(g);
    if (in->r) do_rat(g);
}

// --------------------------------------------------------------- update --
void game_update(Game *g, const Input *in) {
    g->play_frames++;
    handle_input(g, in);

    if (g->mist_cool > 0) g->mist_cool--;
    if (g->damp_age < 10000) g->damp_age++;
    if (g->msg_timer > 0) g->msg_timer--;
    for (int y = 0; y < GRID_H; y++)
        for (int x = 0; x < GRID_W; x++)
            if (g->damp[y][x]) g->damp[y][x]--;
    for (int i = 0; i < MAX_MIST; i++) {
        Mist *m = &g->mist[i];
        if (!m->life) continue;
        m->life--;
        m->x += m->vx;
        m->y += m->vy;
        m->vy += 6;
    }
    for (int i = 0; i < MAX_POPUPS; i++)
        if (g->popups[i].life) {
            g->popups[i].life--;
            if (g->popups[i].life % 3 == 0) g->popups[i].y--;
        }

    // humidity slowly dries out; bowls slow it down
    int bowls = 0;
    for (int i = 0; i < g->item_count; i++)
        if (def_of(g, i)->kind == KIND_BOWL) bowls += def_of(g, i)->w > 2 ? 2 : 1;
    if (++g->hum_tick >= 90 + 30 * bowls) {
        g->hum_tick = 0;
        if (g->humidity > 0) g->humidity--;
    }

    // happiness drifts down slowly; it never reaches anything worse than a sulk
    int debris = 0;
    for (int i = 0; i < MAX_DEBRIS; i++) debris += g->debris[i].active;
    if (++g->happy_tick % 1200 == 0) {
        int dec = 0;
        if (g->humidity < 40) dec++;
        if (g->humidity > 90) dec++;
        if (debris >= 3) dec++;
        if (g->happy_tick % 2400 == 0) dec++;
        add_happy(g, -dec);
    }
    // a happy snake earns you a coin every 10 seconds
    if (g->happiness >= 50 && g->play_frames % 600 == 0)
        g->coins = (uint16_t)clampi(g->coins + 1, 0, 9999);

    rat_update(g);
    snake_update(g);
}

// ------------------------------------------------------------- setup --
static void snake_place(Game *g) {
    Snake *s = &g->snake;
    memset(s, 0, sizeof *s);
    s->spacing = SPACING_NORMAL;
    s->hide = s->allow_hide = s->bowl = -1;
    s->shed_timer = 3600;
    int h = -1;
    for (int i = 0; i < g->item_count && h < 0; i++)
        if (is_hide(g, i)) h = i;
    if (h >= 0) {
        int ix, iy;
        hide_inside(g, h, &ix, &iy);
        s->x = ix * ONE;
        s->y = iy * ONE;
        s->ang = BRAD(192);              // facing up, body trailing out the entrance
        s->hide = s->allow_hide = h;
        s->state = ST_HIDING;
        s->phase = 0;
    } else {
        s->x = (SCREEN_W / 2) * ONE;
        s->y = (SCREEN_H / 2) * ONE;
        s->ang = 0;
        s->state = ST_RESTING;
    }
    trail_reset(s);
}

void game_new(Game *g, uint32_t seed, int morph, int name) {
    memset(g, 0, sizeof *g);
    g->rng = seed | 1;
    g->morph = (uint8_t)morph;
    g->name = (uint8_t)name;
    g->coins = 30;
    g->happiness = 60;
    g->humidity = 45;
    game_place(g, ITEM_LOG_HIDE, 2, 1, 0);
    game_place(g, ITEM_BOWL, 11, 7, 0);
    game_place(g, ITEM_FERN, 6, 2, 0);
    game_place(g, ITEM_GRASS, 13, 2, 0);
    game_place(g, ITEM_FERN, 4, 9, 0);
    game_place(g, ITEM_MOSS, 8, 8, 0);
    game_after_load(g, seed);
}

void game_after_load(Game *g, uint32_t seed) {
    g->rng = seed | 1;
    rebuild_owner(g);
    memset(g->damp, 0, sizeof g->damp);
    memset(g->mist, 0, sizeof g->mist);
    memset(g->popups, 0, sizeof g->popups);
    memset(&g->rat, 0, sizeof g->rat);
    g->cur_x = 7;
    g->cur_y = 5;
    g->build = false;
    g->selected = 0;
    g->damp_age = 10000;
    g->msg[0] = 0;
    snake_place(g);
}

int game_age_days(const Game *g) { return (int)(g->play_frames / FRAMES_PER_DAY); }

const char *snake_state_text(const Game *g) {
    static char buf[40];
    const Snake *s = &g->snake;
    switch (s->state) {
    case ST_HIDING:
        snprintf(buf, sizeof buf, "Hiding in the %s", is_hide(g, s->hide) ? def_of(g, s->hide)->name : "hide");
        return buf;
    case ST_RESTING: return "Resting";
    case ST_ROAM: return "Exploring";
    case ST_GOTO_HIDE: return "Heading home";
    case ST_SOAK: return "Soaking";
    case ST_HUNT: return "Stalking the rat";
    case ST_COIL: return "Coiled to strike!";
    case ST_STRIKE: return "Strike!";
    }
    return "";
}
