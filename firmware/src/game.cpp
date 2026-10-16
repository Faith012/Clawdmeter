#include "game.h"
#include "jump_game.h"
#include "direct_sprite.h"
#include "clawd_anim.h"
#include "idle.h"
#include "theme.h"
#include "hal/board_caps.h"
#include "hal/imu_hal.h"
#include <Arduino.h>
#include <Preferences.h>
#include <lvgl.h>
#include <math.h>
#include <string.h>

LV_FONT_DECLARE(font_tiempos_56);
LV_FONT_DECLARE(font_styrene_28);
LV_FONT_DECLARE(font_styrene_20);

#define CELL           4
#define FRAME_MS       33        // ~30 fps
#define OVER_EXIT_MS   30000     // game over, untouched: back to the meters
#define TILT_ACTIVITY  0.08f     // a tilt change (g) that counts as you being here

// Palette: the official "jumping" Clawd's colours, the "cloud" ride's mapped
// in from slot 10 (cloud_map), plus the scene's own.
#define C_BUG   13               // a bug's shell
#define C_BODY  14               // platform body: Clawd's shade
#define C_EDGE  15               // platform edge, score, play sign, spring, legs: ivory
static uint16_t pal[CLAWD_PALETTE_SIZE];
static uint8_t  jump_map[CLAWD_PALETTE_SIZE], cloud_map[CLAWD_PALETTE_SIZE];

enum State { G_OFF, G_PLAY, G_PAUSED, G_OVER };
static State        st = G_OFF;
static bool         shown = false;     // the cover is up (not hidden for an alert)
static JumpGame     g;
static DirectSprite ds;
static const ClawdAnim* anim = NULL;     // "jumping"
static const ClawdAnim* cloud = NULL;    // "cloud": the ride
static uint16_t     cloud_f = 0;         // the ride's frame, looping
static uint32_t     cloud_at = 0;
static uint32_t     last_ms = 0, drawn_ms = 0, over_at = 0;
static float        noted_tilt = 0;
static uint32_t     best = 0;
static lv_obj_t*    cover = NULL;
static lv_obj_t*    over_box = NULL;
static lv_obj_t*    lbl_score = NULL;
static lv_obj_t*    lbl_best = NULL;

static lv_obj_t* label(lv_obj_t* parent, const lv_font_t* font, lv_color_t color, int y) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_obj_align(l, LV_ALIGN_CENTER, 0, y);
    return l;
}

static void build(void) {
    if (cover) return;
    cover = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(cover);
    lv_obj_set_size(cover, board_caps().width, board_caps().height);
    lv_obj_set_style_bg_color(cover, THEME_BG, 0);
    lv_obj_set_style_bg_opa(cover, LV_OPA_COVER, 0);
    lv_obj_add_flag(cover, LV_OBJ_FLAG_HIDDEN);

    over_box = lv_obj_create(cover);
    lv_obj_remove_style_all(over_box);
    lv_obj_set_size(over_box, LV_PCT(100), LV_PCT(100));
    lv_label_set_text(label(over_box, &font_styrene_28, THEME_DIM, -90), "Game over");
    lbl_score = label(over_box, &font_tiempos_56, THEME_TEXT, -20);
    lbl_best = label(over_box, &font_styrene_20, THEME_ACCENT, 40);
    lv_label_set_text(label(over_box, &font_styrene_20, THEME_DIM, 110), "Tap to play again");

    anim = clawd_anim("jumping");
    cloud = clawd_anim("cloud");
    if (anim) memcpy(pal, anim->palette, sizeof(pal));
    for (int i = 0; i < CLAWD_PALETTE_SIZE; i++) jump_map[i] = i;
    // The cloud's colours: the same slot where "jumping" has the colour,
    // else a free one from 10 up.
    uint8_t next_free = 10;
    for (int i = 1; cloud && i < CLAWD_PALETTE_SIZE; i++) {
        int k = 1;
        while (k < 10 && pal[k] != cloud->palette[i]) k++;
        if (k == 10 && next_free < C_BUG) { k = next_free++; pal[k] = cloud->palette[i]; }
        cloud_map[i] = k < C_BUG ? k : 1;
    }
    pal[C_BUG] = 0x7C6B;
    pal[C_BODY] = 0xBB49;
    pal[C_EDGE] = 0xFFDE;

    Preferences prefs;
    prefs.begin("clawdmeter", true);
    best = prefs.getUInt("jump_best", 0);
    prefs.end();
}

// ─── Drawing ─────────────────────────────────────────────────────────────────
// 3x5 digits, drawn at 2x.
static const uint16_t DIGITS[10] = {
    0x7B6F, 0x2C97, 0x73E7, 0x73CF, 0x5BC9, 0x79CF, 0x79EF, 0x7249, 0x7BEF, 0x7BCF,
};

static void fill(int c0, int r0, int w, int h, uint8_t code) {
    for (int r = r0; r < r0 + h; r++) {
        if (r < 0 || r >= ds.rows) continue;
        for (int c = c0; c < c0 + w; c++)
            if (c >= 0 && c < ds.cols) ds.next[r * ds.cols + c] = code;
    }
}

static void draw_number(int n, int row) {
    char s[12];
    const int len = snprintf(s, sizeof(s), "%d", n);
    int col = ds.cols / 2 - (len * 8 - 2) / 2;
    for (int i = 0; i < len; i++, col += 8) {
        const uint16_t bits = DIGITS[s[i] - '0'];
        for (int y = 0; y < 5; y++)
            for (int x = 0; x < 3; x++)
                if (bits & (1 << (14 - y * 3 - x))) fill(col + x * 2, row + y * 2, 2, 2, C_EDGE);
    }
}

// Original pixel art for what isn't Clawd (rows top down; '.' empty).
// B shell, E eye, L ivory, A cloud, S its shade.
static const char* const SPRING[JG_ITEM_H] = {
    "..LLLLLL..", "....LL....", "...L..L...", "....LL....", "...L..L...",
};
static const char* const CLOUDLET[JG_ITEM_H] = {
    "...AA.....", "..AAAA.AA.", ".AAAAAAAAA", "AAAAAAAAAA", ".SSSSSSSS.",
};
static const char* const BUG[JG_BUG_H] = {
    "...L.....L...", "....L...L....", "...BBBBBBB...", "..BBEBBBEBB..",
    "LBBBBBBBBBBBL", ".BBBBBBBBBBB.", "LBBBBBBBBBBBL", "..L.L...L.L..",
};

static void art(const char* const* rows, int h, int col, int row) {
    for (int y = 0; y < h; y++)
        for (int x = 0; rows[y][x]; x++) {
            const char ch = rows[y][x];
            const uint8_t code = ch == 'B' ? C_BUG : ch == 'E' ? 2 : ch == 'L' ? C_EDGE
                               : ch == 'A' ? cloud_map[2] : ch == 'S' ? cloud_map[3] : 0;
            if (code) fill(col + x, row + y, 1, 1, code);
        }
}

// A Clawd frame through a palette map, its lowest drawn row on screen row
// `feet_row`, centred on column `centre`, on both sides when it wraps.
static void put_frame(const ClawdAnim* a, uint16_t f, const uint8_t* map, int centre, int feet_row) {
    const uint8_t* src = clawd_frame(a, f);
    int feet = 0;
    for (int i = 0; i < a->w * a->h; i++) if (src[i]) feet = i / a->w;
    const int row = feet_row - feet, col = centre - a->w / 2;
    for (int shift = -ds.cols; shift <= ds.cols; shift += ds.cols) {
        if (col + shift >= ds.cols || col + shift + a->w <= 0) continue;
        for (int y = 0; y < a->h; y++)
            for (int x = 0; x < a->w; x++)
                if (src[y * a->w + x]) fill(col + shift + x, row + y, 1, 1, map[src[y * a->w + x]]);
    }
}

static void draw_play_sign(void) {
    const int h = 12, c0 = ds.cols / 2 - 8, r0 = ds.rows / 2;
    for (int k = -h; k <= h; k++) fill(c0, r0 + k, (h - abs(k)) * 7 / 4 + 1, 1, C_EDGE);
}

static void render(void) {
    direct_sprite_clear(&ds);
    if (st != G_OVER) {
        for (int i = 0; i < JG_MAX; i++) {
            const JumpPlatform& p = g.plat[i];
            if (!p.live) continue;
            const int top = jump_screen_row(&g, p.y - 1);
            fill(p.x, top, JG_PW, 1, C_EDGE);
            fill(p.x, top + 1, JG_PW, JG_PH - 1, C_BODY);
            if (p.item == JG_ITEM_NONE) continue;
            art(p.item == JG_ITEM_SPRING ? SPRING : CLOUDLET, JG_ITEM_H, p.x + (JG_PW - JG_ITEM_W) / 2,
                jump_screen_row(&g, p.y + JG_ITEM_H - 1));
        }
        for (int i = 0; i < JG_BUGS; i++) {
            const JumpBug& b = g.bug[i];
            if (!b.live) continue;
            const int row = jump_screen_row(&g, b.y + JG_BUG_H - 1);
            art(BUG, JG_BUG_H, (int)b.x, row);
            if (b.x + JG_BUG_W > ds.cols) art(BUG, JG_BUG_H, (int)b.x - ds.cols, row);
        }
        const int feet_row = jump_screen_row(&g, (int)floorf(g.py));
        if (g.fly > 0 && cloud) {
            // The ride: the official cloud's flying loop, at its own pace.
            if (cloud_f < cloud->loop_start || cloud_f > cloud->loop_end) {
                cloud_f = cloud->loop_start;
                cloud_at = millis();
            } else if (millis() - cloud_at >= cloud->holds[cloud_f]) {
                cloud_f = cloud_f == cloud->loop_end ? cloud->loop_start : cloud_f + 1;
                cloud_at = millis();
            }
            put_frame(cloud, cloud_f, cloud_map, (int)g.px, feet_row);
        } else if (anim) {
            cloud_f = 0;
            put_frame(anim, jump_frame(&g), jump_map, (int)g.px, feet_row);
        }
        draw_number(jump_score(&g), 3);
        if (st == G_PAUSED) draw_play_sign();
    }
    direct_sprite_commit(&ds, pal);
}

// ─── States ──────────────────────────────────────────────────────────────────
static bool open_grid(void) {
    if (ds.drawn) return true;
    const int cols = board_caps().width / CELL, rows = board_caps().height / CELL;
    if (!direct_sprite_init(&ds, CELL, 0, 0, cols, rows)) {
        direct_sprite_free(&ds);
        Serial.println("game: no memory for the grid");
        return false;
    }
    ds.opaque = true;
    return true;
}

static void new_game(void) {
    jump_start(&g, ds.cols, ds.rows, millis() ^ 0x5EED);
    st = G_PLAY;
    last_ms = millis();
    lv_obj_add_flag(over_box, LV_OBJ_FLAG_HIDDEN);
}

static void cover_up(void) {
    lv_obj_move_foreground(cover);
    lv_obj_clear_flag(cover, LV_OBJ_FLAG_HIDDEN);
    shown = true;
}

bool game_available(void) {
    float t;
    return board_caps().direct_performer && board_caps().has_imu && imu_hal_tilt(&t);
}

bool game_start(void) {
    build();
    if (!anim || !open_grid()) return false;
    imu_hal_hold_rotation(true);
    cover_up();
    new_game();
    render();
    Serial.printf("game: start (best %lu)\n", (unsigned long)best);
    return true;
}

static void close_grid(void) {
    direct_sprite_free(&ds);
    lv_obj_add_flag(cover, LV_OBJ_FLAG_HIDDEN);    // LVGL redraws the meters over it
    shown = false;
}

void game_stop(void) {
    if (st == G_OFF) return;
    close_grid();
    imu_hal_hold_rotation(false);
    st = G_OFF;
    Serial.println("game: stop");
}

void game_hide(void) {
    if (st == G_OFF || !shown) return;
    if (st == G_PLAY) st = G_PAUSED;
    close_grid();
}

bool game_show(void) {
    if (st == G_OFF) return false;
    if (!open_grid()) return false;
    cover_up();
    render();
    return true;
}

void game_tap(void) {
    if (st == G_PAUSED) { st = G_PLAY; last_ms = millis(); }
    else if (st == G_OVER) { new_game(); render(); }
}

static void game_over(void) {
    st = G_OVER;
    over_at = millis();
    const uint32_t score = jump_score(&g);
    const bool record = score > best;
    if (record) {
        best = score;
        Preferences prefs;
        prefs.begin("clawdmeter", false);
        prefs.putUInt("jump_best", best);
        prefs.end();
    }
    lv_label_set_text_fmt(lbl_score, "%lu", (unsigned long)score);
    if (record) lv_label_set_text(lbl_best, "New best!");
    else        lv_label_set_text_fmt(lbl_best, "Best %lu", (unsigned long)best);
    lv_obj_clear_flag(over_box, LV_OBJ_FLAG_HIDDEN);
    render();                                      // the world goes; the labels show
    Serial.printf("game: over, score %lu%s\n", (unsigned long)score, record ? " (best)" : "");
}

bool game_tick(void) {
    if (st == G_OFF || !shown) return true;
    const uint32_t now = millis();
    if (st == G_OVER) return now - over_at < OVER_EXIT_MS;
    if (st != G_PLAY) return true;

    float tilt = 0;
    imu_hal_tilt(&tilt);
    if (fabsf(tilt - noted_tilt) > TILT_ACTIVITY) {
        noted_tilt = tilt;
        idle_note_activity();
    }
    jump_step(&g, (now - last_ms) / 1000.0f, tilt);
    last_ms = now;
    if (g.over) { game_over(); return true; }
    if (now - drawn_ms >= FRAME_MS) {
        drawn_ms = now;
        render();
    }
    return true;
}

void game_overlay(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint16_t* px) {
    if (st != G_OFF) direct_sprite_overlay(&ds, x1, y1, x2, y2, px);
}
