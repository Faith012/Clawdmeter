# Clawd Jump: design

A Doodle Jump style game on the board: Clawd bounces up an endless stack of
platforms and you steer him by tilting the board. Approved by the owner on
2026-10-15.

## Where it runs

Boards with an accelerometer and `direct_performer` (drawn straight to the
panel): today only the Waveshare ESP32-C6-Touch-AMOLED-2.16. Everywhere else
nothing changes. A board opts in by implementing `imu_hal_tilt()`; the
weak default says "no tilt".

## Playing

- **Enter:** hold a finger on the screen for about 2 s on the meters (any
  beat except an alert). The tap that starts the hold still gets its normal
  reaction until the game takes over.
- **Screen:** black. Clawd (the official "jumping" frames, 4 px cells, so
  slightly bigger than the corner Clawd) bounces on pixel platforms: Clawd's
  dark terra-cotta with an ivory top edge. The score (height climbed) sits at
  the top in pixel digits.
- **Control:** he bounces on his own; tilting left or right moves him.
  Leaving one side of the screen brings him in from the other. The view
  scrolls up when he climbs past the upper third; it never scrolls down.
- **Platforms:** random, always within one jump of the last; the gaps grow
  with the score.
- **Game over:** he falls off the bottom. A screen with the score and the
  best (kept in NVS) follows. A tap plays again.
- **Exit:** a single PWR press, or another ~2 s hold on the screen, at any
  time. Game over with nobody playing exits on its own after 30 s; so does
  the panel going to sleep.
- **PWR hold ~3 s + release still clears the pairing**: the game never
  touches it.

## Bonuses and bugs

Added on 2026-10-15, before the first release. All rare, so the game stays
the same with surprises:

- **Spring** (~1 platform in 12): landing on that platform sends him about
  three times as high.
- **Cloud** (~1 in 40, from score 50): a small cloud on a platform. Touch it
  and he rides the official "cloud" animation straight up for 2.5 s,
  through everything, then carries on falling and bouncing as usual.
- **Bugs** (~1 in 25, from score 150): original pixel beetles, a joke on
  code bugs, crawling sideways (wrapping) halfway up a gap. Land on one and
  he squashes it and bounces; touch it any other way and he's knocked off:
  he drops through everything and the game ends. The cloud ride passes
  through them.

The spring, the cloud pickup and the bugs are original pixel art, not
Clawd; Clawd himself is only ever the official "jumping" and "cloud" frames.

## While playing

- An alert pauses the game: the meters and the normal alert come back.
  When the alert ends the game returns frozen, with a play sign; a tap
  carries on.
- No shows, screensaver, corner acts or reactions: the game owns the
  screen. The meters keep updating underneath.
- Tilting counts as activity for the idle timer (a tilt change of about
  0.08 g), so the panel stays on while you play and sleeps if you walk away.
- Auto-rotation is held for the whole game, so tilting doesn't turn the
  screen.

## Structure

| Piece | File | Role |
| --- | --- | --- |
| Logic | `firmware/src/jump_game.h/.cpp` | Pure: physics, platforms, wrap, camera, score, game over, which frame. Native-tested (`test/test_jump_game`). |
| Glue | `firmware/src/game.h/.cpp` | Black cover object, the world as a 120×120 `DirectSprite` of 4 px cells (opaque: emptied cells are painted black, not handed back to LVGL), pixel digits, game-over labels, tilt input, best score, 30 fps render. |
| Director | `clawd_director.cpp` | New beat `game`: entry and exit, pause for an alert and return, one Clawd (the others are hidden on entry; the corner Clawd walks back in on exit). |
| HAL | `hal/imu_hal.h` | `imu_hal_tilt(float*)`: side tilt in g along the screen's x axis, for the current rotation. `imu_hal_hold_rotation(bool)`. Weak defaults in `imu_default.cpp`. |
| Input | `main.cpp` | The 2 s touch hold → `director_touch_hold()`; a PWR press in the game → `director_quit_game()` (before the double-press detector). |

Memory: the world grid is 2 × 14.4 KB, allocated when the game starts and
freed when it ends.

## Testing

- Native: platforms always reachable and inside the world; jump height
  matches the reach; landing only while falling; passing up through
  platforms; wrap; camera never goes down; falling off ends the game; tilt
  dead zone.
- Director (native, `test_director`): the one-Clawd rule holds with the game
  in the fuzz; an alert pauses and returns to the game.
- Hardware (C6 2.16): tilt direction in all four rotations, bounce and
  landing, scroll, game over and best, alert during play, PWR and hold exits,
  idle sleep.
