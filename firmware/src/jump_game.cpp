#include "jump_game.h"
#include <math.h>
#include <string.h>

static const float GRAVITY = 2.0f * JG_JUMP / (JG_APEX_S * JG_APEX_S);
static const float LAUNCH = GRAVITY * JG_APEX_S;

float jump_launch_speed(void) { return LAUNCH; }

static uint32_t next_rand(JumpGame* g) {
    uint32_t x = g->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return g->rng = x;
}

static int rand_range(JumpGame* g, int lo, int hi) {   // [lo, hi]
    return lo + (int)(next_rand(g) % (uint32_t)(hi - lo + 1));
}

const JumpPlatform* jump_spawn(JumpGame* g) {
    for (int i = 0; i < JG_MAX; i++) {
        JumpPlatform* p = &g->plat[i];
        if (p->live) continue;
        // Gaps grow with the score, up to the reach.
        float t = jump_score(g) / 600.0f;
        if (t > 1) t = 1;
        const int lo = 8 + (int)(10 * t);
        const int hi = 16 + (int)((JG_REACH - 16) * t);
        p->live = true;
        p->y = g->top + rand_range(g, lo, hi);
        p->x = rand_range(g, 0, g->cols - JG_PW);
        g->top = p->y;
        return p;
    }
    return 0;
}

void jump_start(JumpGame* g, int cols, int view, uint32_t seed) {
    memset(g, 0, sizeof(*g));
    g->cols = cols;
    g->view = view;
    g->rng = seed ? seed : 1;
    JumpPlatform* p = &g->plat[0];
    p->live = true;
    p->x = cols / 2 - JG_PW / 2;
    p->y = JG_START_Y;
    g->top = p->y;
    g->px = cols / 2.0f;
    g->py = g->best = p->y;
    g->vy = LAUNCH;
    while (g->top < view + JG_REACH && jump_spawn(g)) {}
}

int jump_score(const JumpGame* g) { return (int)((g->best - JG_START_Y) / 2); }

int jump_screen_row(const JumpGame* g, int j) { return g->view - 1 - (j - (int)g->cam); }

int jump_frame(const JumpGame* g) {
    if (g->since_bounce < 0.1f) return 1;
    if (g->vy > 0.45f * LAUNCH) return 2;
    if (g->vy > -0.35f * LAUNCH) return 4;
    return 8;
}

// Signed distance from a to b around the world's width, in [-cols/2, cols/2).
static float wrap_delta(float a, float b, int cols) {
    float d = fmodf(b - a, (float)cols);
    if (d < -cols / 2.0f) d += cols;
    if (d >= cols / 2.0f) d -= cols;
    return d;
}

void jump_step(JumpGame* g, float dt, float tilt) {
    if (g->over) return;
    if (dt > 0.05f) dt = 0.05f;

    // Sideways: dead zone, then proportional up to full tilt; eased so a
    // shaky hand doesn't jitter him.
    float a = fabsf(tilt) - JG_DEAD;
    if (a < 0) a = 0;
    a /= JG_FULL - JG_DEAD;
    if (a > 1) a = 1;
    const float target = (tilt < 0 ? -a : a) * JG_SPEED;
    float k = dt * 12;
    if (k > 1) k = 1;
    g->vx += (target - g->vx) * k;
    g->px = fmodf(g->px + g->vx * dt, (float)g->cols);
    if (g->px < 0) g->px += g->cols;

    const float was = g->py;
    g->vy -= GRAVITY * dt;
    g->py += g->vy * dt;
    g->since_bounce += dt;

    if (g->vy < 0) {
        for (int i = 0; i < JG_MAX; i++) {
            const JumpPlatform* p = &g->plat[i];
            if (!p->live || was < p->y || g->py >= p->y) continue;
            if (fabsf(wrap_delta(p->x + JG_PW / 2.0f, g->px, g->cols)) > JG_FOOT) continue;
            g->py = p->y;
            g->vy = LAUNCH;
            g->since_bounce = 0;
            break;
        }
    }

    if (g->py > g->best) g->best = g->py;
    const float follow = g->py - g->view * 2 / 3.0f;
    if (follow > g->cam) g->cam = follow;

    for (int i = 0; i < JG_MAX; i++)
        if (g->plat[i].live && g->plat[i].y < g->cam - JG_PH) g->plat[i].live = false;
    while (g->top < g->cam + g->view + JG_REACH && jump_spawn(g)) {}

    if (g->py + 24 < g->cam) g->over = true;   // his head is past the bottom too
}
