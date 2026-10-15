# Clawd Jump Implementation Plan

**Goal:** the tilt-steered jumping game from the spec, on the C6 2.16.

**Spec:** `docs/superpowers/specs/2026-10-15-clawd-jump-design.md`

**Architecture:** pure logic (`jump_game`) → glue that draws it with an
opaque `DirectSprite` and reads the tilt (`game`) → a director beat that owns
entry, exit and alerts.

## Global constraints

- No `#ifdef BOARD_*` in shared code: availability is `has_imu &&
  direct_performer && imu_hal_tilt()`.
- One Clawd on screen: entering the game hides the face, the performer and
  the corner Clawd; leaving it walks the corner Clawd back in.
- Clawd art only from `research/clawd-official/` frames (the "jumping"
  animation); platforms, digits and the play sign are not Clawd art.
- PWR hold + release (pairing) is never consumed by the game.

## Review focus

1. Tilt sign per rotation (only verifiable on hardware): flip `TILT_SIGN`.
2. An alert while on the game-over screen: returns to game over, not play.
3. A hold that starts during the peek: the peek's face must be gone in the game.
4. LVGL redraws under the cover (a payload) must not punch black holes in
   the world: the overlay repaints the cells.
5. Leaving the game while the panel sleeps: the corner Clawd is home on wake.

## Tasks

### Task 1: `jump_game` logic + native tests
- Create `firmware/src/jump_game.h/.cpp`, `firmware/test/test_jump_game/test_main.cpp`;
  add `+<jump_game.cpp>` to the native env.
- API: `jump_start(JumpGame*, int cols, int view, uint32_t seed)`,
  `jump_step(JumpGame*, float dt, float tilt)`, `jump_spawn(JumpGame*)`,
  `jump_score()`, `jump_frame()`, `jump_screen_row()`.
- Tests: start on a platform; spawned gaps in `[JG_PH+1, JG_REACH]` and x in
  the world; apex ≈ `JG_JUMP`; landing only while falling; wrap; camera
  monotonic; falling off → over; dead zone.

### Task 2: HAL
- `hal/imu_hal.h`: `bool imu_hal_tilt(float* x)`, `void imu_hal_hold_rotation(bool)`.
- `imu_default.cpp`: weak defaults (false / no-op).
- C6 2.16 `imu.cpp`: fresh accelerometer read projected on the screen's x
  axis for `current_rotation`; hold skips rotation updates.

### Task 3: `DirectSprite` opaque mode + free
- `opaque` field: commit paints vacated cells black instead of handing them
  back. `direct_sprite_free()`.

### Task 4: `game` glue
- `game.h/.cpp`: `game_available/start/stop/pause/resume/tap/tick/overlay`.
  Cover object, sprite, palette, frames' feet rows, pixel digits, play sign,
  game-over labels, best in NVS (`clawd_prefs`: `jump_best_get/set`).
- `main.cpp`: `game_overlay()` in the flush callback.

### Task 5: director + input
- Beat `B_GAME`; `director_touch_hold()`, `director_quit_game()`; alert pause
  and return; `clawd game` serial QA.
- `main.cpp`: 2 s touch hold, PWR press → quit before the double-press feed.
- Fakes for `test_director`: game stubs; the fuzz holds and quits.

### Task 6: docs, build, hardware test, changelog
- `docs/clawd-director.md`, `docs/user-guide.md`, `CHANGELOG.md`.
- Build the C6 env and sim; flash; test on the board with the owner.
