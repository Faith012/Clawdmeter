#include "hal/imu_hal.h"

// Weak defaults: a board without a tilt reading has no Clawd Jump.
__attribute__((weak)) bool imu_hal_tilt(float*) { return false; }
__attribute__((weak)) void imu_hal_hold_rotation(bool) {}
