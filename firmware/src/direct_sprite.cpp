#include "direct_sprite.h"
#include "clawd_anim.h"
#include "splash_geometry.h"
#include "hal/board_caps.h"
#include "hal/display_hal.h"
#include <lvgl.h>
#include <string.h>
#include <esp_heap_caps.h>

// One blit buffer for every sprite: one grid row (cols cells x one cell row).
// Only one Clawd is ever on screen, and blits run one at a time on the loop.
static uint16_t* strip = NULL;
static size_t    strip_px = 0;

bool direct_sprite_init(DirectSprite* s, int cell, int org_x, int org_y, int cols, int rows) {
    memset(s, 0, sizeof(*s));
    s->cell = cell;
    s->org_x = org_x;
    s->org_y = org_y;
    s->cols = cols;
    s->rows = rows;
    const size_t n = (size_t)cols * rows;
    s->drawn = (uint8_t*)heap_caps_calloc(n, 1, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    s->next = (uint8_t*)heap_caps_calloc(n, 1, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t px = (size_t)cols * cell * cell;
    if (px > strip_px) {
        heap_caps_free(strip);
        strip = (uint16_t*)heap_caps_malloc(px * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        strip_px = strip ? px : 0;
    }
    return s->drawn && s->next && strip;
}

void direct_sprite_clear(DirectSprite* s) {
    memset(s->next, 0, (size_t)s->cols * s->rows);
}

void direct_sprite_put(DirectSprite* s, const uint8_t* src, int w, int h, bool mirror, int col, int row) {
    for (int y = 0; y < h; y++) {
        const int r = row + y;
        if (r < 0 || r >= s->rows) continue;
        for (int x = 0; x < w; x++) {
            const int c = col + x;
            if (c < 0 || c >= s->cols) continue;
            const uint8_t code = src[y * w + (mirror ? w - 1 - x : x)];
            if (code) s->next[r * s->cols + c] = code;
        }
    }
}

// Row r's band on the panel, or false if it hangs past an edge.
static bool row_y(const DirectSprite* s, int r, int* y) {
    *y = s->org_y + r * s->cell;
    return *y >= 0 && *y + s->cell <= board_caps().height;
}

// Draw cells [c0, c1) of row r from `drawn`, clipped to the panel's width.
static void blit(void* ctx, int r, int c0, int c1) {
    DirectSprite* s = (DirectSprite*)ctx;
    int y;
    if (!row_y(s, r, &y)) return;
    while (c0 < c1 && s->org_x + c0 * s->cell < 0) c0++;
    while (c1 > c0 && s->org_x + c1 * s->cell > board_caps().width) c1--;
    if (c0 >= c1) return;
    const int w = (c1 - c0) * s->cell;
    for (int c = c0; c < c1; c++) {
        const uint8_t code = s->drawn[r * s->cols + c];
        const uint16_t col = code && s->pal && code < CLAWD_PALETTE_SIZE ? s->pal[code] : 0;
        uint16_t* p = strip + (c - c0) * s->cell;
        for (int i = 0; i < s->cell; i++) p[i] = col;
    }
    for (int d = 1; d < s->cell; d++) memcpy(strip + d * w, strip, w * 2);
    display_hal_draw_bitmap(s->org_x + c0 * s->cell, y, w, s->cell, strip);
}

// LVGL redraws what's under cells [c0, c1) of row r on its next refresh.
static void give_back(void* ctx, int r, int c0, int c1) {
    DirectSprite* s = (DirectSprite*)ctx;
    lv_area_t a;
    a.x1 = s->org_x + c0 * s->cell;
    a.x2 = s->org_x + c1 * s->cell - 1;
    a.y1 = s->org_y + r * s->cell;
    a.y2 = a.y1 + s->cell - 1;
    lv_obj_invalidate_area(lv_screen_active(), &a);
}

void direct_sprite_commit(DirectSprite* s, const uint16_t* pal) {
    const bool repaint_all = pal != s->pal;
    // Hand back first, against the old frame; then the new frame is the one
    // on the panel, so blit (and any flush that follows) paints it.
    static const DirectSpriteRun none = [](void*, int, int, int) {};
    direct_sprite_diff(s->drawn, s->next, s->cols, s->rows, false, give_back, none, s);
    // `drawn` must hold the new frame before blitting it; keep the old one in
    // `next` to diff against.
    const size_t n = (size_t)s->cols * s->rows;
    for (size_t i = 0; i < n; i++) {
        const uint8_t t = s->drawn[i];
        s->drawn[i] = s->next[i];
        s->next[i] = t;
    }
    s->pal = pal;
    direct_sprite_diff(s->next, s->drawn, s->cols, s->rows, repaint_all,
                       [](void*, int, int, int) {}, blit, s);
    s->any = false;
    for (size_t i = 0; i < n && !s->any; i++) s->any = s->drawn[i] != 0;
}

void direct_sprite_erase(DirectSprite* s) {
    if (!s->drawn || !s->any) return;
    direct_sprite_clear(s);
    direct_sprite_commit(s, s->pal);
}

void direct_sprite_overlay(DirectSprite* s, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint16_t* px) {
    if (!s->drawn || !s->any) return;
    int c0, c1, r0, r1;
    span_touch_cells(s->org_x, s->cell, s->cols, x1, x2 + 1, &c0, &c1);
    span_touch_cells(s->org_y, s->cell, s->rows, y1, y2 + 1, &r0, &r1);
    const int w = x2 - x1 + 1;
    for (int r = r0; r < r1; r++) {
        int ya = s->org_y + r * s->cell, yb = ya + s->cell;      // the row's band, clipped to the area
        if (ya < y1) ya = y1;
        if (yb > y2 + 1) yb = y2 + 1;
        for (int c = c0; c < c1; c++) {
            const uint8_t code = s->drawn[r * s->cols + c];
            if (!code || !s->pal || code >= CLAWD_PALETTE_SIZE) continue;
            const uint16_t col = s->pal[code];
            int xa = s->org_x + c * s->cell, xb = xa + s->cell;
            if (xa < x1) xa = x1;
            if (xb > x2 + 1) xb = x2 + 1;
            for (int y = ya; y < yb; y++) {
                uint16_t* p = px + (size_t)(y - y1) * w + (xa - x1);
                for (int x = xa; x < xb; x++) *p++ = col;
            }
        }
    }
}
