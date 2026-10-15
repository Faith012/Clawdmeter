#pragma once
#include <stdint.h>
#include <stdbool.h>

// A Clawd sprite drawn straight to the panel, for boards without PSRAM (the
// direct_performer capability): no screen-sized buffer, and still in front
// of the meters like the LVGL images PSRAM boards use. Only its coloured
// cells are drawn. The cells it leaves are handed back to LVGL, which
// redraws what was under them; and whatever LVGL renders under the sprite
// gets the sprite's cells written into it before it goes to the panel
// (direct_sprite_overlay, from the flush callback), so the panel never
// shows a band of meters where he stands.
//
// Each sprite owns a cell grid on the panel: column c spans
// x = org_x + c * cell, row r spans y = org_y + r * cell. A frame is the grid's
// palette indices (0 = nothing there). Rows or columns past the panel's edge
// are skipped. Only one Clawd is ever on screen, so the sprites share one
// blit buffer.

struct DirectSprite {
    int             cell, org_x, org_y, cols, rows;
    uint8_t*        drawn;    // cols * rows: what's on the panel now
    uint8_t*        next;     // cols * rows: the frame being composed
    const uint16_t* pal;      // palette of what's drawn
    bool            any;      // any coloured cell drawn
    bool            opaque;   // a whole black scene (Clawd Jump): emptied cells are
                              // painted black, not handed back
};

// Allocate the grid (internal RAM) and grow the shared blit buffer to one
// grid row (`cols` cells). False if the memory isn't there.
bool direct_sprite_init(DirectSprite* s, int cell, int org_x, int org_y, int cols, int rows);
// Compose: clear `next`, then put frames into it (top-left at col, row).
void direct_sprite_clear(DirectSprite* s);
void direct_sprite_put(DirectSprite* s, const uint8_t* src, int w, int h, bool mirror, int col, int row);
// Show `next`: hand back the cells it left, draw what's new or changed
// (everything coloured on a new palette).
void direct_sprite_commit(DirectSprite* s, const uint16_t* pal);
// Hand back everything it has on the panel.
void direct_sprite_erase(DirectSprite* s);
// Free the grid, leaving the panel as it is (whatever redraws it next wins).
void direct_sprite_free(DirectSprite* s);
// LVGL rendered [x1, x2] x [y1, y2] into px (RGB565, row-major, that area's
// width) and is about to send it: write the sprite's coloured cells into it.
void direct_sprite_overlay(DirectSprite* s, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint16_t* px);

// ─── Pure part (native-tested: test_direct_sprite) ───────────────────────────
// Row by row, left to right: the runs of cells to hand back (coloured before,
// empty now) and the runs to paint (coloured now and new or changed, or any
// coloured cell when repaint_all).
typedef void (*DirectSpriteRun)(void* ctx, int row, int c0, int c1);

static inline void direct_sprite_diff(const uint8_t* drawn, const uint8_t* next, int cols, int rows,
                                      bool repaint_all, DirectSpriteRun give_back, DirectSpriteRun paint,
                                      void* ctx) {
    for (int r = 0; r < rows; r++) {
        const uint8_t* d = drawn + r * cols;
        const uint8_t* n = next + r * cols;
        for (int c = 0; c < cols;) {
            if (!(d[c] && !n[c])) { c++; continue; }
            int e = c;
            while (e < cols && d[e] && !n[e]) e++;
            give_back(ctx, r, c, e);
            c = e;
        }
        for (int c = 0; c < cols;) {
            if (!(n[c] && (repaint_all || n[c] != d[c]))) { c++; continue; }
            int e = c;
            while (e < cols && n[e] && (repaint_all || n[e] != d[e])) e++;
            paint(ctx, r, c, e);
            c = e;
        }
    }
}
