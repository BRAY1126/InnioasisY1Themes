// Terrarium: platform-independent game state and rules.
// Nothing in here touches DS hardware, so it also builds on a PC for tests.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "art.h"

#define SCREEN_W 256
#define SCREEN_H 192
#define TILE 16
#define GRID_W 16
#define GRID_H 12

#define MAX_ITEMS 40
#define MAX_DEBRIS 12
#define MAX_MIST 24
#define MAX_POPUPS 4
#define SEGMENTS 14
#define TRAIL_LEN 256
#define FP 8                    // fixed point: 24.8
#define ONE (1 << FP)

#define FRAMES_PER_DAY (60 * 60 * 5)   // one in-game day = 5 minutes of play

typedef struct { uint8_t type, tx, ty, flip; } Item;

enum DebrisKind { DEBRIS_SHED = 0, DEBRIS_DROPPING = 1 };
typedef struct { uint8_t kind, active; int16_t x, y; } Debris;   // x,y = sprite centre

typedef struct { int32_t x, y, vx, vy; uint8_t life; } Mist;

enum PopupKind { POP_HEART = 0, POP_COIN = 1 };
typedef struct { uint8_t kind, life; int16_t x, y; } Popup;

enum SnakeState {
    ST_HIDING,      // inside a hide, tail tip showing
    ST_RESTING,     // no hide available: lies still where it is
    ST_ROAM,        // exploring after misting
    ST_GOTO_HIDE,   // heading back to a hide
    ST_SOAK,        // lying in a bowl
    ST_HUNT,        // creeping toward a rat
    ST_COIL,        // bunched up, about to strike
    ST_STRIKE,      // lunging
};

typedef struct {
    int32_t x, y;            // head position, fixed point
    int32_t ang;             // heading in brads << 8 (65536 = full turn, 0 = right, +y = down)
    int32_t speed;           // pixels/frame, fixed point
    int32_t spacing;         // trail distance between segments, fixed point
    int32_t trail_x[TRAIL_LEN], trail_y[TRAIL_LEN];   // 1px-spaced head history
    int trail_head;          // index of newest point
    int32_t last_x, last_y;  // last trail point

    uint8_t state;
    int timer;               // frames left in the current state / target
    int32_t tx, ty;          // current target (pixels)
    int hide;                // index of the hide it is going to / inside, or -1
    int allow_hide;          // hide whose tiles it may enter, or -1
    int phase;               // sub-step within a state
    int settle;              // pixels left to curl up inside a hide
    int digest;              // frames left digesting (0 = not digesting)
    int digest_total;
    int tongue;              // frames the tongue is out
    int bowl;                // bowl item index while soaking
    int shed_timer;
} Snake;

typedef struct {
    bool active;
    int32_t x, y;            // fixed point
    int32_t vx, vy;
    int timer, flee, frame;
} Rat;

typedef struct {
    // --- saved ---
    uint8_t morph, name;
    uint16_t coins;
    uint8_t happiness;       // 0..100. The snake can never die; low happiness only sulks.
    uint8_t humidity;        // 0..100
    uint32_t play_frames;
    uint8_t item_count;
    Item items[MAX_ITEMS];
    Debris debris[MAX_DEBRIS];

    // --- runtime ---
    uint8_t owner[GRID_H][GRID_W];      // item index + 1 occupying each tile, 0 = free
    uint16_t damp[GRID_H][GRID_W];      // frames a floor tile stays damp
    Snake snake;
    Rat rat;
    Mist mist[MAX_MIST];
    Popup popups[MAX_POPUPS];
    int cur_x, cur_y;                   // cursor tile
    bool build;                         // item selected, grid showing
    int selected;                       // ItemType being placed
    int mist_cool;
    int damp_x, damp_y, damp_age;       // most recent misted spot (pixels)
    int hum_tick, happy_tick, mist_count;
    uint32_t rng;
    char msg[48];
    int msg_timer;
} Game;

typedef struct {
    bool up, down, left, right;   // pressed this frame (with key repeat)
    bool a, b, x, y, l, r, select;
} Input;

// Morphs: every new save rolls one of these colourings.
#define MORPH_COUNT 12
typedef struct { const char *name; uint8_t rgb[11][3]; } Morph;
extern const Morph morphs[MORPH_COUNT];
#define NAME_COUNT 16
extern const char *const snake_names[NAME_COUNT];

void game_new(Game *g, uint32_t seed, int morph, int name);
void game_after_load(Game *g, uint32_t seed);   // rebuild runtime state after a load
void game_update(Game *g, const Input *in);

bool game_can_place(const Game *g, int type, int tx, int ty);
bool game_place(Game *g, int type, int tx, int ty, int flip);
bool game_remove(Game *g, int index);
int  game_item_at(const Game *g, int tx, int ty);   // -1 if none
void game_cursor_footprint(const Game *g, int *w, int *h);

// Snake body helpers for rendering
void snake_segment_pos(const Snake *s, int i, int *x, int *y);   // i = 0 head .. SEGMENTS
bool game_point_hidden(const Game *g, int x, int y);            // inside a hide
int  snake_lump_segment(const Snake *s);                        // -1 if none
const char *snake_state_text(const Game *g);
int  game_age_days(const Game *g);
uint32_t game_rand(Game *g);
