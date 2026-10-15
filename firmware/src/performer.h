#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <lvgl.h>

// Clawd performing on the meters: the official scenes at full size (the
// 60x60-cell grid, 8 px cells on a 480 panel), walking on and off through
// the screen edges. The director (clawd_director.cpp) decides when he
// performs and what he does; nothing here starts on its own.
//
// He's drawn in front of the meters, fully opaque, like the attention
// walk-on: while he performs he covers part of the panels, and a tap sends
// him off.
//
// Performer model: one Clawd with an x offset (cells) from his authored home
// spot. He is offstage (left or right), standing (the official idle pose),
// walking, or performing a scene. Scenes always end on the idle pose, so the
// next thing starts from the same spot.
//
// With PSRAM he's an LVGL image over the meters (a full-screen RGB565A8
// buffer, about 690 KB on a 480 panel). Without it he's drawn straight to
// the panel, one grid row at a time, and the meters are hidden while he's
// on. Only if even that row can't be allocated is performer_available()
// false, and the director plays performances as corner acts.

// feet_y: the screen row his feet stand on at home (the floor he performs on).
void performer_init(lv_obj_t *parent, int feet_y);
void performer_tick(void);
bool performer_available(void);
// The flush callback calls this with each area LVGL rendered, before it goes
// to the panel, so a directly drawn performer is written into it. A no-op
// otherwise.
void performer_overlay(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint16_t* px);

enum PerformerSide { PERFORMER_LEFT = -1, PERFORMER_RIGHT = +1 };

void performer_place_offstage(int side);           // gone, just past that edge
void performer_walk_home(uint16_t speed_pct);      // walking gait to home, then stand
void performer_walk_off(int side, uint16_t speed_pct);
// A run-off: the walk starts at from_pct and speeds up to to_pct over
// ramp_ms (ease-in), so he gets going instead of starting at full tilt.
void performer_run_off(int side, uint16_t from_pct, uint16_t to_pct, uint32_t ramp_ms);
// Perform one scene at home, holding its loop for loop_ms, then its outro;
// ends standing. Walker scenes (walking, crab walking) do one round trip.
void performer_perform(int anim_index, uint32_t loop_ms);
// Pick a scene from mood group 0..3 and perform it. calm = only the gentle
// Idle-group scenes (magnifier, walking). Never the same scene twice running
// (tracked separately for shows and calm dips).
void performer_perform_group(int group, uint32_t loop_ms, bool calm);
// Any show scene from every mood group (a scene asked for with PWR), never
// the same one twice running.
void performer_perform_any(uint32_t loop_ms);
void performer_release(void);   // stop now, snap to the idle pose where he is
bool performer_busy(void);      // walking or performing
bool performer_visible(void);   // any of him is on screen right now
