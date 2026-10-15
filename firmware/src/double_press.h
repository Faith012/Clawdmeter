#pragma once
#include <stdint.h>

// PWR presses → single or double. A press opens a window; a second press
// inside it is a double, otherwise the single lands when the window closes.
// So a single costs the window in latency. Pure, native-tested
// (test_double_press). Feed it every loop, with pressed = this loop's edge.

// The AXP2101 reports a short press on release, so two presses need a bit
// more than a finger's double tap. Tune on hardware.
#define PWR_DOUBLE_MS 450

enum PressKind { PRESS_NONE, PRESS_SINGLE, PRESS_DOUBLE };

struct DoublePress {
    uint32_t first_at;
    bool     pending;
};

static inline PressKind double_press_feed(DoublePress* d, uint32_t now, bool pressed, uint32_t window_ms) {
    const bool expired = d->pending && now - d->first_at > window_ms;
    if (pressed) {
        if (d->pending && !expired) { d->pending = false; return PRESS_DOUBLE; }
        d->first_at = now;               // this press waits for a partner
        d->pending = true;
        return expired ? PRESS_SINGLE : PRESS_NONE;   // the old one never got one
    }
    if (expired) { d->pending = false; return PRESS_SINGLE; }
    return PRESS_NONE;
}
