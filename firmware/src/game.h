#pragma once
#include <stdint.h>
#include <stdbool.h>

// Clawd Jump on the panel: a black screen over the meters, the world from
// jump_game.h drawn straight to the panel as a grid of 4 px cells (an opaque
// DirectSprite), steered by tilting the board (imu_hal_tilt). The director
// (clawd_director.cpp) decides when it runs; nothing here starts on its own.
// See docs/superpowers/specs/2026-10-15-clawd-jump-design.md.

// The board can play: an accelerometer reading and direct drawing.
bool game_available(void);
// A new game over the meters; false if the grid's memory isn't there.
bool game_start(void);
void game_stop(void);           // the meters come back
// An alert takes the screen: the meters come back and play pauses.
void game_hide(void);
// Back after it, frozen with a play sign (a tap carries on); false if the
// grid's memory isn't there any more.
bool game_show(void);
// A tap: carry on after a pause, play again after game over.
void game_tap(void);
// Every loop while it runs. False once game over has sat untouched long
// enough that nobody is playing.
bool game_tick(void);
// The flush callback calls this with each area LVGL rendered, before it goes
// to the panel, so the world is written into it.
void game_overlay(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint16_t* px);
