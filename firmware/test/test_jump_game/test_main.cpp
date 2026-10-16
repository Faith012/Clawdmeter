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
    for (int i = 0; i < JG_BUGS; i++) g->bug[i].live = false;
    g->plat[0] = { x, y, true, JG_ITEM_NONE };
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

// Highest point of the next bounce off a platform at y he's falling onto.
static float apex_after_bounce(JumpGame* g) {
    bool up = false;
    float apex = g->py;
    for (int i = 0; i < 600; i++) {
        jump_step(g, DT, 0);
        if (g->vy > 0) up = true;
        if (up && g->py > apex) apex = g->py;
        if (up && g->vy < 0) break;
    }
    return apex;
}

static void a_spring_sends_him_three_times_as_high() {
    JumpGame g;
    jump_start(&g, COLS, VIEW, 21);
    only_platform(&g, (int)g.px - JG_PW / 2, 20);
    g.plat[0].item = JG_ITEM_SPRING;
    g.py = 22;
    g.vy = 0;
    const float apex = apex_after_bounce(&g);
    CHECK(fabsf(apex - 20 - 3 * JG_JUMP) < 4);
}

static void the_cloud_carries_him_up_through_everything_then_lets_go() {
    JumpGame g;
    jump_start(&g, COLS, VIEW, 22);
    only_platform(&g, (int)g.px - JG_PW / 2, 20);
    g.plat[0].item = JG_ITEM_CLOUD;
    g.plat[1] = { (int)g.px - JG_PW / 2, 60, true, JG_ITEM_NONE };   // in his way
    g.bug[0] = { g.px - JG_BUG_W / 2.0f, 90, 0, true };               // so is this
    g.py = 21;
    g.vy = 0;
    jump_step(&g, DT, 0);
    CHECK(g.fly > 0);
    CHECK(g.plat[0].item == JG_ITEM_NONE);        // taken
    float y = g.py;
    for (int i = 0; i < (int)(JG_FLY_S / DT) - 2; i++) {
        jump_step(&g, DT, 0);
        CHECK(g.py > y);                          // straight up, nothing stops him
        y = g.py;
        if (g.py > 100 && g.py < 140) CHECK(g.bug[0].live);   // passed through, not squashed
    }
    CHECK(!g.knocked);
    for (int i = 0; i < 10; i++) jump_step(&g, DT, 0);
    CHECK(g.fly <= 0);
    for (int i = 0; i < 120; i++) jump_step(&g, DT, 0);
    CHECK(g.vy < 0);                              // falling again, as usual
}

static void landing_on_a_bug_squashes_it() {
    JumpGame g;
    jump_start(&g, COLS, VIEW, 23);
    only_platform(&g, 0, -500);
    g.bug[0] = { g.px - JG_BUG_W / 2.0f, 30, 0, true };
    g.py = 40;
    g.vy = 0;
    bool bounced = false;
    for (int i = 0; i < 60 && !bounced; i++) { jump_step(&g, DT, 0); bounced = g.vy > 0; }
    CHECK(bounced);
    CHECK(!g.bug[0].live);
    CHECK(!g.knocked);
}

static void a_bug_from_below_knocks_him_off_for_good() {
    JumpGame g;
    jump_start(&g, COLS, VIEW, 24);
    only_platform(&g, (int)g.px - JG_PW / 2, 10);
    g.bug[0] = { g.px - JG_BUG_W / 2.0f, 30, 0, true };
    g.py = 10;
    g.vy = jump_launch_speed();                   // bouncing up into it
    for (int i = 0; i < 30 && !g.knocked; i++) jump_step(&g, DT, 0);
    CHECK(g.knocked);
    CHECK(g.bug[0].live);
    for (int i = 0; i < 600 && !g.over; i++) {
        jump_step(&g, DT, 1);                     // no steering, no bounce off the platform
        CHECK(g.vy <= 0);
    }
    CHECK(g.over);
}

static void extras_are_rare_and_bugs_wait_for_a_score() {
    JumpGame g;
    jump_start(&g, COLS, VIEW, 25);
    CHECK(g.plat[0].item == JG_ITEM_NONE);        // the first platform is plain
    int springs = 0, clouds = 0, bugs = 0;
    for (int n = 0; n < 6000; n++) {
        for (int i = 0; i < JG_MAX; i++) g.plat[i].live = false;
        for (int i = 0; i < JG_BUGS; i++) g.bug[i].live = false;
        const JumpPlatform* p = jump_spawn(&g);
        springs += p->item == JG_ITEM_SPRING;
        clouds += p->item == JG_ITEM_CLOUD;
        for (int i = 0; i < JG_BUGS; i++) bugs += g.bug[i].live;
    }
    CHECK(clouds == 0 && bugs == 0);              // score 0: springs only
    CHECK(springs > 6000 / 20 && springs < 6000 / 8);
    g.best = JG_START_Y + 2 * 200;                // score 200
    springs = clouds = bugs = 0;
    for (int n = 0; n < 6000; n++) {
        for (int i = 0; i < JG_MAX; i++) g.plat[i].live = false;
        for (int i = 0; i < JG_BUGS; i++) g.bug[i].live = false;
        const int below = g.top;
        const JumpPlatform* p = jump_spawn(&g);
        clouds += p->item == JG_ITEM_CLOUD;
        for (int i = 0; i < JG_BUGS; i++) {
            if (!g.bug[i].live) continue;
            bugs++;
            CHECK(g.bug[i].y > below - JG_BUG_H && g.bug[i].y < p->y);   // in the gap
        }
    }
    CHECK(clouds > 6000 / 60 && clouds < 6000 / 25);
    CHECK(bugs > 6000 / 40 && bugs < 6000 / 15);
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
    RUN_TEST(a_spring_sends_him_three_times_as_high);
    RUN_TEST(the_cloud_carries_him_up_through_everything_then_lets_go);
    RUN_TEST(landing_on_a_bug_squashes_it);
    RUN_TEST(a_bug_from_below_knocks_him_off_for_good);
    RUN_TEST(extras_are_rare_and_bugs_wait_for_a_score);
    return UNITY_END();
}
