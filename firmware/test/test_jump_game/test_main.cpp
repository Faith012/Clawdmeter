// Host unit test for Clawd Jump's rules (jump_game.h):
//
//   pio test -d firmware -e native -f test_jump_game

#include "jump_game.h"
#include <unity.h>
#include <math.h>
#include <string.h>

#define CHECK(cond) TEST_ASSERT_TRUE(cond)

void setUp(void) {}
void tearDown(void) {}

static const int COLS = 120, VIEW = 120;
static const float DT = 1 / 60.0f;

// Only the given platform, at (x, y); nothing else to land on.
static void only_platform(JumpGame* g, int x, int y) {
    for (int i = 0; i < JG_MAX; i++) g->plat[i].live = false;
    g->plat[0] = { x, y, true };
    g->top = 1 << 20;            // no spawning while the test runs
}

static void he_starts_bouncing_off_a_platform_under_him() {
    JumpGame g;
    jump_start(&g, COLS, VIEW, 7);
    CHECK(!g.over);
    CHECK(g.vy > 0);
    CHECK(jump_score(&g) == 0);
    const JumpPlatform& p = g.plat[0];
    CHECK(p.live && p.y == (int)g.py && fabsf(p.x + JG_PW / 2.0f - g.px) <= JG_FOOT);
    CHECK(jump_screen_row(&g, (int)g.py) >= 0 && jump_screen_row(&g, (int)g.py) < VIEW);
}

static void every_platform_is_one_jump_from_the_last_and_inside_the_world() {
    for (uint32_t seed = 1; seed < 40; seed++) {
        JumpGame g;
        jump_start(&g, COLS, VIEW, seed);
        for (int n = 0; n < 2000; n++) {
            g.best = n * 3.0f;                       // the gaps grow with the score
            for (int i = 0; i < JG_MAX; i++) g.plat[i].live = false;
            const int was = g.top;
            const JumpPlatform* p = jump_spawn(&g);
            CHECK(p != NULL);
            CHECK(p->y - was > JG_PH && p->y - was <= JG_REACH);
            CHECK(p->x >= 0 && p->x + JG_PW <= COLS);
        }
    }
}

static void a_bounce_reaches_the_jump_height_which_clears_the_reach() {
    JumpGame g;
    jump_start(&g, COLS, VIEW, 3);
    only_platform(&g, 0, 6);
    g.py = 6;
    g.vy = jump_launch_speed();
    float apex = g.py;
    for (int i = 0; i < 120 && g.vy >= 0; i++) {
        jump_step(&g, DT, 0);
        if (g.py > apex) apex = g.py;
    }
    CHECK(fabsf(apex - 6 - JG_JUMP) < 2);
    CHECK(JG_REACH < JG_JUMP - 4);
}

static void he_lands_only_while_falling() {
    JumpGame g;
    jump_start(&g, COLS, VIEW, 5);
    only_platform(&g, (int)g.px - JG_PW / 2, 40);
    g.py = 30;                                    // under it, going up: passes through
    g.vy = jump_launch_speed();
    jump_step(&g, DT, 0);
    for (int i = 0; i < 20; i++) jump_step(&g, DT, 0);
    CHECK(g.py > 40);
    CHECK(g.vy < jump_launch_speed());            // no bounce on the way up
    // Then he comes down onto it and bounces.
    bool bounced = false;
    for (int i = 0; i < 200 && !bounced; i++) {
        const float vy = g.vy;
        jump_step(&g, DT, 0);
        bounced = vy < 0 && g.vy > 0;
    }
    CHECK(bounced);
    CHECK(fabsf(g.py - 40) < 0.01f);
    CHECK(jump_frame(&g) == 1);                   // the crouch, right after it
}

static void he_misses_a_platform_off_to_the_side() {
    JumpGame g;
    jump_start(&g, COLS, VIEW, 5);
    only_platform(&g, (int)g.px + JG_FOOT + 2, 20);   // centre past the foot reach
    g.py = 30;
    g.vy = 0;
    for (int i = 0; i < 60; i++) jump_step(&g, DT, 0);
    CHECK(g.py < 20);
}

static void he_lands_across_the_wrap() {
    JumpGame g;
    jump_start(&g, COLS, VIEW, 5);
    only_platform(&g, COLS - JG_PW / 2, 20);      // half of it hangs past the right edge...
    g.px = 2;                                     // ...over him on the left
    g.py = 30;
    g.vy = 0;
    bool bounced = false;
    for (int i = 0; i < 60 && !bounced; i++) { jump_step(&g, DT, 0); bounced = g.vy > 0; }
    CHECK(bounced);
}

static void leaving_one_side_brings_him_in_from_the_other() {
    JumpGame g;
    jump_start(&g, COLS, VIEW, 9);
    for (int i = 0; i < 120; i++) {
        jump_step(&g, DT, 1);                     // full tilt right for 2 s
        CHECK(g.px >= 0 && g.px < COLS);
    }
    for (int i = 0; i < 120; i++) {
        jump_step(&g, DT, -1);
        CHECK(g.px >= 0 && g.px < COLS);
    }
}

static void a_small_tilt_does_not_move_him() {
    JumpGame g;
    jump_start(&g, COLS, VIEW, 9);
    const float x = g.px;
    for (int i = 0; i < 60; i++) jump_step(&g, DT, JG_DEAD * 0.9f);
    CHECK(g.px == x);
    for (int i = 0; i < 60; i++) jump_step(&g, DT, 0.3f);
    CHECK(g.px != x);
}

static void the_view_follows_him_up_and_never_down() {
    JumpGame g;
    jump_start(&g, COLS, VIEW, 11);
    float cam = g.cam;
    float tilt = 0, base = g.py;
    for (int i = 0; i < 60 * 60 && !g.over; i++) {
        // Steer for the next platform up from the one he last bounced off,
        // the short way round, like a player would.
        const JumpPlatform* next = NULL;
        for (int k = 0; k < JG_MAX; k++) {
            const JumpPlatform& p = g.plat[k];
            if (p.live && p.y > base + 0.5f && (!next || p.y < next->y)) next = &p;
        }
        if (next) {
            float d = fmodf(next->x + JG_PW / 2.0f - g.px, (float)COLS);
            if (d < -COLS / 2) d += COLS;
            if (d >= COLS / 2) d -= COLS;
            tilt = d > 2 ? 0.5f : d < -2 ? -0.5f : 0;
        }
        jump_step(&g, DT, tilt);
        if (g.since_bounce == 0) base = g.py;
        CHECK(g.cam >= cam);
        cam = g.cam;
        CHECK(jump_screen_row(&g, (int)g.py) >= VIEW / 3 - 2);   // never past the upper third
    }
    CHECK(g.cam > 200);                           // he climbed
    CHECK(jump_score(&g) > 100);
}

static void falling_out_of_the_bottom_ends_the_game() {
    JumpGame g;
    jump_start(&g, COLS, VIEW, 13);
    g.cam = 100;                                  // as if he'd climbed
    only_platform(&g, 0, 500);                    // nothing under him
    g.py = g.best = 120;
    g.vy = 0;
    const int score = jump_score(&g);
    for (int i = 0; i < 600 && !g.over; i++) jump_step(&g, DT, 0);
    CHECK(g.over);
    CHECK(g.py + 24 < g.cam);
    CHECK(jump_score(&g) == score);
    const float py = g.py;
    jump_step(&g, DT, 0);                         // over: nothing moves
    CHECK(g.py == py);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(he_starts_bouncing_off_a_platform_under_him);
    RUN_TEST(every_platform_is_one_jump_from_the_last_and_inside_the_world);
    RUN_TEST(a_bounce_reaches_the_jump_height_which_clears_the_reach);
    RUN_TEST(he_lands_only_while_falling);
    RUN_TEST(he_misses_a_platform_off_to_the_side);
    RUN_TEST(he_lands_across_the_wrap);
    RUN_TEST(leaving_one_side_brings_him_in_from_the_other);
    RUN_TEST(a_small_tilt_does_not_move_him);
    RUN_TEST(the_view_follows_him_up_and_never_down);
    RUN_TEST(falling_out_of_the_bottom_ends_the_game);
    return UNITY_END();
}
