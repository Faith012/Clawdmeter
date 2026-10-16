#include "jump_game.h"
#include <math.h>
#include <string.h>

static const float GRAVITY = 2.0f * JG_JUMP / (JG_APEX_S * JG_APEX_S);
static const float LAUNCH = GRAVITY * JG_APEX_S;

float jump_launch_speed(void) { return LAUNCH; }

// Signed distance from a to b around the world's width, in [-cols/2, cols/2).
static float wrap_delta(float a, float b, int cols) {
    float d = fmodf(b - a, (float)cols);
    if (d < -cols / 2.0f) d += cols;
    if (d >= cols / 2.0f) d -= cols;
    return d;
}

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
        const int below = g->top;
        p->live = true;
        p->y = g->top + rand_range(g, lo, hi);
        p->x = rand_range(g, 0, g->cols - JG_PW);
        g->top = p->y;
        // Extras, rare: a cloud 1 in 40 (from 50), a spring 1 in 12, a bug
        // halfway up the gap below 1 in 25 (from 150).
        p->item = JG_ITEM_NONE;
        const int score = jump_score(g);
        const uint32_t r = next_rand(g) % 1000;
        if (score >= 50 && r < 25) p->item = JG_ITEM_CLOUD;
        else if (r >= 25 && r < 108) p->item = JG_ITEM_SPRING;
        else if (score >= 150 && r >= 108 && r < 148) {
            for (int k = 0; k < JG_BUGS; k++) {
                JumpBug* b = &g->bug[k];
                if (b->live) continue;
                b->live = true;
                b->x = rand_range(g, 0, g->cols - 1);
                b->y = below + (p->y - below) / 2 - JG_BUG_H / 2;
                b->vx = (next_rand(g) & 1) ? JG_BUG_V : -JG_BUG_V;
                break;
            }
        }
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

// He touches the box [x, x+w) x [y, y+h) (x around the wrap).
static bool touches(const JumpGame* g, float x, int y, int w, int h) {
    return fabsf(wrap_delta(x + w / 2.0f, g->px, g->cols)) < (JG_BODY_W + w) / 2.0f &&
           g->py < y + h && g->py + JG_BODY_H > y;
}

int jump_frame(const JumpGame* g) {
    if (g->since_bounce < 0.1f) return 1;
    if (g->vy > 0.45f * LAUNCH) return 2;
    if (g->vy > -0.35f * LAUNCH) return 4;
    return 8;
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
    if (g->knocked) a = 0;
    const float target = (tilt < 0 ? -a : a) * JG_SPEED;
    float k = dt * 12;
    if (k > 1) k = 1;
    g->vx += (target - g->vx) * k;
    g->px = fmodf(g->px + g->vx * dt, (float)g->cols);
    if (g->px < 0) g->px += g->cols;

    const float was = g->py;
    if (g->fly > 0) {                            // on the cloud: straight up
        g->fly -= dt;
        g->vy = g->fly > 0 ? JG_FLY_V : LAUNCH * 0.6f;
    } else {
        g->vy -= GRAVITY * dt;
    }
    g->py += g->vy * dt;
    g->since_bounce += dt;

    for (int i = 0; i < JG_BUGS; i++) {
        JumpBug* b = &g->bug[i];
        if (!b->live) continue;
        b->x = fmodf(b->x + b->vx * dt, (float)g->cols);
        if (b->x < 0) b->x += g->cols;
        if (g->fly > 0 || g->knocked || !touches(g, b->x, b->y, JG_BUG_W, JG_BUG_H)) continue;
        if (g->vy < 0 && was >= b->y + JG_BUG_H - 2) {   // from above: squashed
            b->live = false;
            g->vy = LAUNCH;
            g->since_bounce = 0;
        } else {
            g->knocked = true;
            if (g->vy > 0) g->vy = 0;
        }
    }

    for (int i = 0; i < JG_MAX && g->fly <= 0 && !g->knocked; i++) {
        JumpPlatform* p = &g->plat[i];
        if (p->live && p->item == JG_ITEM_CLOUD &&
            touches(g, p->x + (JG_PW - JG_ITEM_W) / 2.0f, p->y, JG_ITEM_W, JG_ITEM_H)) {
            p->item = JG_ITEM_NONE;
            g->fly = JG_FLY_S;
        }
    }

    if (g->vy < 0 && g->fly <= 0 && !g->knocked) {
        for (int i = 0; i < JG_MAX; i++) {
            const JumpPlatform* p = &g->plat[i];
            if (!p->live || was < p->y || g->py >= p->y) continue;
            if (fabsf(wrap_delta(p->x + JG_PW / 2.0f, g->px, g->cols)) > JG_FOOT) continue;
            g->py = p->y;
            g->vy = p->item == JG_ITEM_SPRING ? LAUNCH * JG_SPRING : LAUNCH;
            g->since_bounce = 0;
            break;
        }
    }

    if (g->py > g->best) g->best = g->py;
    const float follow = g->py - g->view * 2 / 3.0f;
    if (follow > g->cam) g->cam = follow;

    for (int i = 0; i < JG_MAX; i++)
        if (g->plat[i].live && g->plat[i].y < g->cam - JG_PH) g->plat[i].live = false;
    for (int i = 0; i < JG_BUGS; i++)
        if (g->bug[i].live && g->bug[i].y + JG_BUG_H < g->cam) g->bug[i].live = false;
    while (g->top < g->cam + g->view + JG_REACH && jump_spawn(g)) {}

    if (g->py + 24 < g->cam) g->over = true;   // his head is past the bottom too
}
