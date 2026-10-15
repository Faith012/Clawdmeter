#pragma once
#include <stdint.h>

// Optional accelerometer-driven orientation tracker. Returns 0..3 (quarter
// turns CW from default mounting). Boards without an IMU — or boards with
// rotation intentionally disabled, like AMOLED-1.8 fixed at 0° — return 0
// from imu_hal_rotation_quadrant() and no-op on init/tick.

void    imu_hal_init(void);
void    imu_hal_tick(void);
uint8_t imu_hal_rotation_quadrant(void);

// Clawd Jump (game.h). Optional: imu_default.cpp has weak defaults (no tilt,
// nothing to hold), and only boards that implement imu_hal_tilt get the game.
// Side tilt in g along the screen's x axis as it's shown now (+ = right edge
// down), read fresh; false without an accelerometer.
bool    imu_hal_tilt(float* x);
// While on, the screen keeps its rotation whatever the board does.
void    imu_hal_hold_rotation(bool on);
