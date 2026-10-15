#pragma once
#include <stdint.h>
#include <stdbool.h>

// Clawd Jump's rules, without drawing or input (game.cpp does both), so they
// run in the native tests (test_jump_game).
//
// The world is a grid of cells, `cols` wide and endless upward. y grows
// upward from the start; cell row j spans [j, j+1). The view shows `view`
// rows from `cam` up. Clawd bounces on his own: landing on a platform while
// falling sends him back up at the same speed. Tilt moves him sideways, and
// leaving one side brings him in from the other. The view follows him up
// past its upper third and never comes down. He falls out of the bottom:
// game over.

#define JG_JUMP     36      // apex height of a bounce, cells
#define JG_APEX_S   0.5f    // seconds from a bounce to the apex
#define JG_REACH    30      // highest gap between platform tops (< JG_JUMP: he clears it)
#define JG_PW       18      // platform width, cells
#define JG_PH       3       // platform height: an edge row and the body
#define JG_FOOT     14      // |feet - platform centre| that still lands (he's 16 wide)
#define JG_SPEED    125.0f  // sideways speed at full tilt, cells/s
#define JG_DEAD     0.06f   // tilt (g) that doesn't move him
#define JG_FULL     0.45f   // tilt (g) for full speed
#define JG_MAX      24      // platforms alive at once
#define JG_START_Y  6       // the first platform, under him at the start

struct JumpPlatform {
    int  x, y;              // left column; top surface (he stands at y)
    bool live;
};

struct JumpGame {
    int          cols, view;
    float        px, py;      // feet: centre column, height
    float        vx, vy;      // cells/s
    float        cam;         // world y at the bottom of the view
    float        since_bounce;
    float        best;        // highest py so far
    int          top;         // highest platform's y
    bool         over;
    uint32_t     rng;
    JumpPlatform plat[JG_MAX];
};

void jump_start(JumpGame* g, int cols, int view, uint32_t seed);
// Advance dt seconds (clamped to 50 ms). tilt: g along the screen's x axis,
// + = right.
void jump_step(JumpGame* g, float dt, float tilt);
// Add the next platform above the highest one (one jump away at most); NULL
// when every slot is in use.
const JumpPlatform* jump_spawn(JumpGame* g);
int jump_score(const JumpGame* g);
// Which frame of the official "jumping" animation fits the moment:
// 1 crouch, 2 launch, 4 apex, 8 coming down.
int jump_frame(const JumpGame* g);
// The view's row (0 = top) of world cell row j.
int jump_screen_row(const JumpGame* g, int j);
float jump_launch_speed(void);
