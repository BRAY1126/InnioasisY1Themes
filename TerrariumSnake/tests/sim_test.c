// Host-side soak test for the game rules: gcc -Iinclude tests/sim_test.c source/game.c source/morphs.c source/art.c -lm
// Runs hours of simulated play with random input and checks invariants.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL frame %u: ", g.play_frames); printf(__VA_ARGS__); printf("\n"); if (++fails > 10) exit(1); } } while (0)

int main(void) {
    static Game g;
    int eaten = 0, rats = 0, sheds = 0, hid = 0, roamed = 0, soaked = 0;
    for (int run = 0; run < 6; run++) {
        game_new(&g, 1000 + run * 77, run % MORPH_COUNT, run);
        srand(run);
        int last_state = -1;
        for (int f = 0; f < 60 * 60 * 30; f++) {           // 30 minutes per run
            Input in;
            memset(&in, 0, sizeof in);
            int r = rand() % 1000;
            if (r < 30) in.left = 1; else if (r < 60) in.right = 1; else if (r < 90) in.up = 1; else if (r < 120) in.down = 1;
            if (rand() % 400 == 0) in.a = 1;
            if (rand() % 300 == 0) in.b = 1;
            if (rand() % 2000 == 0) { in.r = 1; rats++; }
            if (rand() % 600 == 0) in.y = 1;
            if (rand() % 900 == 0) in.x = 1;
            if (rand() % 1500 == 0) in.select = 1;
            if (rand() % 4000 == 0) in.l = 1;
            if (f % 1800 < 120 && f % 6 == 0) in.a = 1;  // mist bursts
            bool had_rat = g.rat.active;
            game_update(&g, &in);
            if (had_rat && !g.rat.active) eaten++;
            if (g.snake.state != last_state) {
                if (g.snake.state == ST_HIDING) hid++;
                if (g.snake.state == ST_ROAM) roamed++;
                if (g.snake.state == ST_SOAK) soaked++;
                last_state = g.snake.state;
            }
            int hx = g.snake.x >> FP, hy = g.snake.y >> FP;
            CHECK(hx >= 0 && hx < SCREEN_W && hy >= 0 && hy < SCREEN_H, "head off screen %d,%d", hx, hy);
            CHECK(g.happiness <= 100, "happiness %d", g.happiness);
            CHECK(g.humidity <= 100, "humidity %d", g.humidity);
            CHECK(g.cur_x >= 0 && g.cur_y >= 0 && g.cur_x < GRID_W && g.cur_y < GRID_H, "cursor");
            if (g.rat.active) {
                int rx = g.rat.x >> FP, ry = g.rat.y >> FP;
                CHECK(rx >= 0 && rx < SCREEN_W && ry >= 0 && ry < SCREEN_H, "rat off screen %d,%d", rx, ry);
            }
            for (int i = 0; i <= SEGMENTS; i++) {
                int x, y;
                snake_segment_pos(&g.snake, i, &x, &y);
                CHECK(x > -20 && x < SCREEN_W + 20 && y > -20 && y < SCREEN_H + 20, "segment %d at %d,%d", i, x, y);
            }
        }
        for (int i = 0; i < MAX_DEBRIS; i++) sheds += g.debris[i].active && g.debris[i].kind == DEBRIS_SHED;
        printf("run %d: items=%d coins=%d happy=%d humid=%d state=%s\n", run, g.item_count, g.coins, g.happiness, g.humidity, snake_state_text(&g));
    }
    // Overfeeding: release 200 rats back to back. The snake must eat them all and stay happy.
    game_new(&g, 42, 0, 0);
    int fed = 0;
    for (int k = 0; k < 200; k++) {
        Input in; memset(&in, 0, sizeof in);
        g.cur_x = 8; g.cur_y = 6;
        in.r = 1;
        game_update(&g, &in);
        memset(&in, 0, sizeof in);
        for (int f = 0; f < 60 * 120 && g.rat.active; f++) game_update(&g, &in);
        if (!g.rat.active) fed++;
    }
    printf("overfeed: %d/200 rats eaten, happiness %d, digest %d\n", fed, g.happiness, g.snake.digest);
    CHECK(fed >= 190, "snake missed too many rats");
    CHECK(g.happiness >= 90, "feeding should keep it happy");
    printf("totals: rats released %d eaten %d, hid %d, roamed %d, soaked %d, sheds on floor %d\n", rats, eaten, hid, roamed, soaked, sheds);
    printf(fails ? "FAILED\n" : "OK\n");
    return fails != 0;
}
