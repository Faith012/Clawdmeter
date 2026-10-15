# Status mode: Clawd shows what Claude is doing

Date: 2026-10-15. Fork: Faith012/Clawdmeter. Target release: v2.3.0.

## Goal

Today Clawd's corner acts are mostly autonomous: a prompt earns a point, a
long turn a wave, and the rest is idle play. Status mode makes the corner
Clawd and the status line a continuous readout of Claude Code's state, so a
glance at the board tells you whether Claude is thinking, working, done,
waiting for you or out of quota. A setting in the desktop app turns it on
or off. Default: on.

## States

The device derives one state from what it already receives (hooks through
the app: `e`, `a`, alerts; usage: `st`). Highest priority first:

| State | Device sees | Corner Clawd | Status line |
|---|---|---|---|
| Limit | `st` is `limited` | sits still, no acts | `Limit reached` |
| Waiting | an alert is up | the existing attention beat | the alert's line (unchanged) |
| Working | a PostToolUse in the last minute (`a` >= 1) and no Stop since | `laptop`, looped | `Claude is working...` |
| Thinking | a prompt (`e:"p"`) with no tool use and no Stop since | `magnifier`, looped | `Claude is thinking...` |
| Done | a Stop (`e:"s"`) in the last 5 min | `jumping happy` once, then sits still | `Done` |
| Idle | none of the above | today's behaviour (idle acts) | today's whimsical verbs |

- The rules live in one pure function in `clawd_policy.{h,cpp}`
  (`clawd_status(...)`), native-tested. The director feeds it timestamps
  it already tracks from `signals` and the usage payload.
- "No Stop since" compares timestamps: a Stop after the last prompt / tool
  activity ends Thinking / Working.
- Working and Thinking turn corner autonomy off (no idle acts or lurk trips)
  so the readout isn't diluted. Idle restores it.
- Status mode is informational, so it applies at every liveliness level,
  No-nonsense included. Liveliness still governs shows, the screensaver
  and idle acts.

## What stays the same

- Shows (earned or asked for with a PWR double press), calm dips, the
  screensaver, alerts and taps work as in v2.2.0. A show takes Clawd out of
  the corner; when it ends he comes back and resumes the current state.
- The screensaver needs 10 quiet minutes, so it never starts while Claude
  is thinking or working.
- With status mode off, everything behaves exactly as in v2.2.0.

## Corner rendering

- `laptop` (34x23 cells) and `magnifier` (36x27) join `CORNER_ANIMS` in
  `mascot.cpp`, so the corner buffer is sized for them: about 29 KB on a
  480 panel instead of about 23 KB (internal RAM on the C6). How they sit
  next to the meters (laptop reaches right, magnifier slightly left) is
  checked on hardware.
- Looping: when the state's act ends and the state holds, the director
  starts it again with `snappy` (no idle lead-in frame), so it reads as one
  continuous loop.

## The setting

- Desktop app: a checkbox "Show what Claude is doing" in Settings, next to
  Silliness. Config key `status_mode = on|off`; the default (on) stays out
  of the file, like `liveliness`.
- Protocol: new payload key `sm` (`1` on, `0` off). Sent with every data
  payload and right away when it changes, like `lv` / `aa`
  (`engine.rs`: `sent_sm`). Old firmware ignores it. Documented in
  `docs/protocol.md`; the app's payload test already cross-checks keys
  against `doc["..."]` in `firmware/src`.
- Firmware: `payload.cpp` parses `sm` (-1 when absent = keep the saved
  value), `clawd_prefs` keeps it in NVS (default on), and the serial command
  `status_mode [0|1]` sets it for QA until the app sends its own.

## Testing

- Native (firmware): `clawd_status` for every state and the priorities;
  `test_payload` for `sm`; `test_director` with status mode on: the state
  acts play and the one-Clawd fuzz still holds.
- Rust (CI): config parse/save of `status_mode`; the engine sends `sm` with
  data and on change; the Settings checkbox writes the config.
- Hardware (ESP32-C6-Touch-AMOLED-2.16, the owner's board): use Claude Code
  normally and confirm each state on the corner and the status line; turn
  the setting off in the app and confirm v2.2.0 behaviour.
- The app can't be built on the owner's Windows PC (no Rust), so its part is
  verified by CI and then from the v2.3.0 release installer.

## Out of scope

- The full-size attention walk-on on C6 (separate project, next).
- Thinking vs writing text: the hooks don't tell them apart; "working"
  covers tool use.
