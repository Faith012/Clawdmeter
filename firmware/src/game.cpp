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

// Palette: the official "jumping" Clawd's colours, plus the scene's own.
#define C_BODY  14               // platform body: Clawd's shade
#define C_EDGE  15               // platform edge, score, play sign: ivory
static uint16_t pal[CLAWD_PALETTE_SIZE];

enum State { G_OFF, G_PLAY, G_PAUSED, G_OVER };
static State        st = G_OFF;
static bool         shown = false;     // the cover is up (not hidden for an alert)
static JumpGame     g;
static DirectSprite ds;
static const ClawdAnim* anim = NULL;
static uint8_t      feet[16];          // per frame: its lowest drawn row
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
    if (anim) {
        memcpy(pal, anim->palette, sizeof(pal));
        for (int f = 0; f < anim->frame_count && f < 16; f++) {
            const uint8_t* src = clawd_frame(anim, f);
            for (int r = 0; r < anim->h; r++)
                for (int c = 0; c < anim->w; c++)
                    if (src[r * anim->w + c]) feet[f] = r;
        }
    }
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
        }
        if (anim) {
            const int f = jump_frame(&g);
            const int row = jump_screen_row(&g, (int)floorf(g.py)) - feet[f];
            const int col = (int)g.px - anim->w / 2;
            const uint8_t* src = clawd_frame(anim, f);
            direct_sprite_put(&ds, src, anim->w, anim->h, false, col, row);
            // Halfway through a side, he shows on both.
            if (col < 0) direct_sprite_put(&ds, src, anim->w, anim->h, false, col + ds.cols, row);
            if (col + anim->w > ds.cols) direct_sprite_put(&ds, src, anim->w, anim->h, false, col - ds.cols, row);
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
