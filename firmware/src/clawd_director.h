#pragma once
#include <stdint.h>
#include "data.h"
#include "signals.h"

// The director: one Clawd, on the meters, driven by the situation.
//
//   meters  home and default; every path ends here. The corner Clawd sits in
//           his seat and does small acts (mascot.h). He also performs here,
//           full size in front of them (performer.h): a treat after a busy
//           stretch, or a calm dip during the screensaver.
//   face    the #198 face resting on the glass (face.h): the screensaver,
//           and the peek when you tap the meters.
//
// Rules of continuity: only ever one Clawd (corner sprite, full-size sprite,
// performer or face); he enters and leaves through an edge; the screen never
// hard-cuts. A tap during a performance or on the face means "show me the
// meters"; on the meters themselves it's tapping the aquarium: he reacts.
//
// Beats (design system §09): notice, show, tap → meters, screensaver + wake,
// attention, fresh-window joy, long-turn wave, and the tap-on-the-meters
// peek. The lurk trip and idle acts are the corner's own secondary action.
// Minimal mode collapses all of it to a still Clawd. See
// docs/clawd-director.md.

enum DirectorRoom { ROOM_METERS, ROOM_FACE };

void director_init(void);          // after ui_init()
void director_tick(void);

// Per parsed payload, after signals_ingest().
void director_on_payload(const UsageData* d, const SignalsUpdate* sig);
void director_alert(const char* message);   // attention hook fired
void director_alert_clear(void);            // the session moved on
bool director_take_dismissed(void);         // an alert ended on the device since the last call

// Any touch press at screen x (wired internally to LVGL). On the meters it's
// tapping the aquarium: the face comes up, curious, for a while (a tap on it
// sends it back); anywhere else it means "show me the meters".
void director_tap(int x);
// While on, the panel is the pairing control (the touch-hold re-pair on
// boards without a PWR button): a tap on the meters still counts as you
// being here and dismisses an alert, but Clawd doesn't peek, so the hint
// stays in view for the hold.
void director_set_pairing_touch(bool on);
// A physical button press: you're here. `main_button` is BOOT, or the PWR
// key on a board whose only button it is. On a board without a touchscreen
// that press first dismisses an active alert and returns true (the caller
// then skips the button's normal action, push to talk or brightness);
// otherwise false.
bool director_button(bool main_button);
// Status mode (clawd_prefs `sm`, default on): the corner Clawd loops an act
// for what Claude is doing and the status line says it (clawd_status in
// clawd_policy.h). Off: idle acts and whimsical verbs as before.
void director_set_status_mode(bool on);
bool director_status_mode(void);
// PWR double press: a show now, with any scene (not the mood group's), that
// doesn't use up the automatic shows' cooldown. Only from the meters at
// rest (home or a corner act); false otherwise, and the caller treats it as
// a single press.
bool director_play_scene(void);
// PWR single press during a show: the next scene (any, never the same twice
// running) if one is playing. True while a show is on (the press is used
// up), false otherwise.
bool director_next_scene(void);
// Clawd Jump (game.h). A ~2 s hold on the panel: starts the game from the
// meters (not over an alert), or leaves it while it runs.
void director_touch_hold(void);
// A PWR press: leaves the game if it's on (or paused under an alert) and
// returns true (the press is used up); false otherwise.
bool director_quit_game(void);
// Apply a liveliness level (clawd_policy.h) now: the tuning changes, and
// what the new level rules out (a show, the screensaver, idle acts, a
// pending reaction) stops.
void director_set_liveliness(int level);

// How long Clawd's attention animation runs, in seconds (clawd_policy.h;
// 0 = off). The alert itself is unchanged.
void director_set_attn_anim(int seconds);

// Print every beat step with a timestamp (the sim turns it on with
// SIM_TRACE=1 for walkthrough timings). Beat changes always print.
void director_set_trace(bool on);

DirectorRoom director_room(void);
const char*  director_beat_name(void);

// QA hook for the `clawd <beat>` serial command: start a beat now so it can
// be captured with `screenshot` on hardware. beat = show | screensaver |
// attention | joy | lurk | peek [x] | game | home. Returns false for an unknown
// name, or for show / screensaver / lurk / peek while another beat is still
// running (lurk also needs the full-size sprite, i.e. PSRAM). `peek` is a tap
// on the meters at x (default: three quarters across).
bool director_debug(const char* beat);
