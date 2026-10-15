# Status mode Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The corner Clawd and the status line show what Claude Code is doing (thinking, working, done, waiting, limit), behind a desktop-app setting sent to the device as `sm`.

**Architecture:** A pure `clawd_status()` rule in `clawd_policy` turns the timestamps the director already sees (prompt / stop events, tool activity, alert, `st`) into one state. The director loops the state's corner act and tells the status line; the setting rides the existing `lv`/`aa` path (app config → engine → payload → `payload.cpp` → NVS prefs).

**Tech Stack:** C++ (Arduino, LVGL 9, Unity native tests via PlatformIO), Rust (desktop app, GPUI tests).

**Spec:** `docs/superpowers/specs/2026-10-15-status-mode-design.md`

## Global Constraints

- Animations: Working = `laptop`, Thinking = `magnifier`, Done = `jumping happy` once. Official animations only.
- Status line text (English, ASCII): `Claude is working` / `Working`, `Claude is thinking` / `Thinking` (short form when the long one doesn't fit), `Done`, `Limit reached`.
- Done lasts 5 min after a Stop (`STATUS_DONE_MS`); an open turn with nothing for 10 min is over (`STATUS_TURN_STALE_MS`).
- Status mode works at every liveliness level; with it off, behaviour is exactly v2.2.0.
- Payload key `sm`: `1` on, `0` off, sent with every data payload and right away on change; absent = keep the saved setting. Default on.
- App config key `status_mode = on|off`, default on (stays out of the file).
- No `#ifdef BOARD_*` in shared code.

## Review Focus

- A turn ended with Esc (no Stop hook fires): must not show "thinking" forever → Task 1 test `aborted_turn_goes_idle`.
- `a` stays above 0 for up to a minute after a Stop: Done must win over Working → Task 1 test `stop_outranks_lingering_activity`.
- `millis()` wrapping between the prompt and now → Task 1 test `wraps_around`.
- New firmware with the old app (no `sm`): keeps the saved setting, default on → Task 3 test `test_status_mode` (absent → -1).
- Status mode turned off mid-loop: idle acts come back and the state reads idle → Task 2 director test.

---

### Task 1: `clawd_status()` rule

**Files:**
- Modify: `firmware/src/clawd_policy.h` (append a "Status mode" section)
- Modify: `firmware/src/clawd_policy.cpp` (append the implementation)
- Test: `firmware/test/test_clawd_policy/test_main.cpp`

**Interfaces:**
- Produces: `enum ClawdStatus { STATUS_IDLE, STATUS_DONE, STATUS_THINKING, STATUS_WORKING, STATUS_WAITING, STATUS_LIMIT }`, `struct StatusInput { uint32_t now; bool limited; bool alert; int activity; bool have_prompt; uint32_t prompt_at; bool have_stop; uint32_t stop_at; bool have_active; uint32_t active_at; }`, `ClawdStatus clawd_status(const StatusInput*)`, `const char* clawd_status_name(ClawdStatus)`, `STATUS_DONE_MS`, `STATUS_TURN_STALE_MS`.

- [ ] **Step 1: Write the failing tests** (append before `main`, register with `RUN_TEST`)

```cpp
static StatusInput status_at(uint32_t now) {
    StatusInput in;
    memset(&in, 0, sizeof(in));
    in.now = now;
    return in;
}

static void test_status_idle_by_default(void) {
    StatusInput in = status_at(10 * MIN);
    TEST_ASSERT_EQUAL(STATUS_IDLE, clawd_status(&in));
}

static void test_status_limit_and_alert_outrank_work(void) {
    StatusInput in = status_at(10 * MIN);
    in.activity = 3;
    in.alert = true;
    TEST_ASSERT_EQUAL(STATUS_WAITING, clawd_status(&in));
    in.limited = true;
    TEST_ASSERT_EQUAL(STATUS_LIMIT, clawd_status(&in));
}

static void test_status_prompt_thinks_tools_work(void) {
    StatusInput in = status_at(10 * MIN);
    in.have_prompt = true; in.prompt_at = 9 * MIN;
    TEST_ASSERT_EQUAL(STATUS_THINKING, clawd_status(&in));
    in.activity = 1;
    TEST_ASSERT_EQUAL(STATUS_WORKING, clawd_status(&in));
    in.have_prompt = false;                       // rebooted mid-turn: tools still mean working
    TEST_ASSERT_EQUAL(STATUS_WORKING, clawd_status(&in));
}

static void test_status_stop_is_done_then_idle(void) {
    StatusInput in = status_at(10 * MIN);
    in.have_prompt = true; in.prompt_at = 5 * MIN;
    in.have_stop = true;   in.stop_at = 9 * MIN;
    TEST_ASSERT_EQUAL(STATUS_DONE, clawd_status(&in));
    in.now = 9 * MIN + STATUS_DONE_MS;
    TEST_ASSERT_EQUAL(STATUS_IDLE, clawd_status(&in));
    in.prompt_at = in.now;                        // a new prompt opens a new turn
    TEST_ASSERT_EQUAL(STATUS_THINKING, clawd_status(&in));
}

static void stop_outranks_lingering_activity(void) {
    StatusInput in = status_at(10 * MIN);
    in.have_prompt = true; in.prompt_at = 5 * MIN;
    in.have_stop = true;   in.stop_at = 10 * MIN - 5 * SEC;
    in.activity = 2;                              // `a` counts the last minute's tool calls
    TEST_ASSERT_EQUAL(STATUS_DONE, clawd_status(&in));
}

static void aborted_turn_goes_idle(void) {
    StatusInput in = status_at(0);
    in.have_prompt = true; in.prompt_at = 0;      // Esc: no Stop ever comes
    in.now = STATUS_TURN_STALE_MS - 1;
    TEST_ASSERT_EQUAL(STATUS_THINKING, clawd_status(&in));
    in.now = STATUS_TURN_STALE_MS;
    TEST_ASSERT_EQUAL(STATUS_IDLE, clawd_status(&in));
    in.have_active = true; in.active_at = 8 * MIN;   // tools ran later in the turn
    TEST_ASSERT_EQUAL(STATUS_THINKING, clawd_status(&in));
}

static void wraps_around(void) {
    StatusInput in = status_at(0x00001000u);
    in.have_prompt = true; in.prompt_at = 0xFFFFF000u;
    TEST_ASSERT_EQUAL(STATUS_THINKING, clawd_status(&in));
    in.have_stop = true; in.stop_at = 0x00000800u;   // after the prompt, across the wrap
    TEST_ASSERT_EQUAL(STATUS_DONE, clawd_status(&in));
}
```

```cpp
    RUN_TEST(test_status_idle_by_default);
    RUN_TEST(test_status_limit_and_alert_outrank_work);
    RUN_TEST(test_status_prompt_thinks_tools_work);
    RUN_TEST(test_status_stop_is_done_then_idle);
    RUN_TEST(stop_outranks_lingering_activity);
    RUN_TEST(aborted_turn_goes_idle);
    RUN_TEST(wraps_around);
```

- [ ] **Step 2: Run to verify it fails**

Run: `pio test -d firmware -e native -f test_clawd_policy` (CI only on Windows: no host g++)
Expected: compile error, `clawd_status` not declared.

- [ ] **Step 3: Implement** — append to `clawd_policy.h`:

```cpp
// ─── Status mode ─────────────────────────────────────────────────────────────
// What Claude Code is doing, from what the device already gets: hook events
// (`e` prompt / stop, `a` tool activity), the alert, and `st`. Highest
// priority first. The director loops the state's corner act and puts it on
// the status line (clawd_director.h, director_set_status_mode).
enum ClawdStatus {
    STATUS_IDLE,       // nothing going on: Clawd as usual
    STATUS_DONE,       // a Stop in the last STATUS_DONE_MS
    STATUS_THINKING,   // a turn is open, no tool calls right now
    STATUS_WORKING,    // tool calls in the last minute (`a` > 0), turn not stopped
    STATUS_WAITING,    // an alert is up
    STATUS_LIMIT,      // `st` is "limited"
};
#define STATUS_DONE_MS       (5UL * 60UL * 1000UL)
// An open turn with no prompt or tool activity for this long is over: a
// turn ended with Esc never sends a Stop.
#define STATUS_TURN_STALE_MS (10UL * 60UL * 1000UL)

struct StatusInput {
    uint32_t now;
    bool     limited;
    bool     alert;
    int      activity;                        // `a`; <= 0 = no tool calls in the last minute
    bool     have_prompt; uint32_t prompt_at; // the last 'p' / 'b'
    bool     have_stop;   uint32_t stop_at;   // the last 's'
    bool     have_active; uint32_t active_at; // the last moment `a` was > 0
};
ClawdStatus clawd_status(const StatusInput* in);
const char* clawd_status_name(ClawdStatus s);   // for the serial log
```

Append to `clawd_policy.cpp`:

```cpp
// ─── Status mode ─────────────────────────────────────────────────────────────
static bool before(uint32_t a, uint32_t b) { return (int32_t)(a - b) < 0; }

ClawdStatus clawd_status(const StatusInput* in) {
    if (in->limited) return STATUS_LIMIT;
    if (in->alert)   return STATUS_WAITING;
    // The latest turn event decides: a Stop at or after the last prompt ends
    // the turn, even while `a` still counts that minute's tool calls.
    const bool stopped = in->have_stop && (!in->have_prompt || !before(in->stop_at, in->prompt_at));
    if (stopped) return in->now - in->stop_at < STATUS_DONE_MS ? STATUS_DONE : STATUS_IDLE;
    if (in->activity > 0) return STATUS_WORKING;
    if (!in->have_prompt) return STATUS_IDLE;
    uint32_t last = in->prompt_at;
    if (in->have_active && before(last, in->active_at)) last = in->active_at;
    return in->now - last < STATUS_TURN_STALE_MS ? STATUS_THINKING : STATUS_IDLE;
}

const char* clawd_status_name(ClawdStatus s) {
    switch (s) {
    case STATUS_DONE:     return "done";
    case STATUS_THINKING: return "thinking";
    case STATUS_WORKING:  return "working";
    case STATUS_WAITING:  return "waiting";
    case STATUS_LIMIT:    return "limit";
    default:              return "idle";
    }
}
```

- [ ] **Step 4: Run the tests** — same command (CI). Expected: PASS.
- [ ] **Step 5: Commit** — `git commit -m "Work out what Claude is doing from the device's signals"`

---

### Task 2: Director, corner and status line

**Files:**
- Modify: `firmware/src/clawd_director.h`, `firmware/src/clawd_director.cpp`
- Modify: `firmware/src/mascot.cpp:80-82` (`CORNER_ANIMS`)
- Modify: `firmware/src/ui.h`, `firmware/src/ui.cpp` (`ui_tick_anim` text selection)
- Test: `firmware/test/test_director/fake_world.h`, `fake_actors.cpp`, `test_main.cpp`

**Interfaces:**
- Consumes: Task 1's `clawd_status`, `StatusInput`, `ClawdStatus`, `clawd_status_name`.
- Produces: `void director_set_status_mode(bool on)`, `bool director_status_mode(void)`, `void ui_set_claude_status(ClawdStatus s)`.

- [ ] **Step 1: Test fakes and the failing director test**

`fake_world.h` (append):

```cpp
extern const char* fake_last_act;   // the last act the corner was asked to play
bool fake_mascot_autonomy(void);
```

`fake_actors.cpp`: name the `mascot_act` parameter and record it, add the getter and the ui stand-in:

```cpp
const char* fake_last_act = NULL;
bool fake_mascot_autonomy(void) { return m_autonomy; }
void ui_set_claude_status(ClawdStatus) {}
```

```cpp
bool mascot_act(const char* name, uint8_t times, bool, uint16_t speed_pct) {
    if (m_minimal) return false;
    if (!(m_mode == M_SEATED || (m_mode == M_ACT && m_act_auto))) return false;
    fake_last_act = name;
    m_mode = M_ACT; m_act_auto = false;
    m_until = now() + scaled(1200 * (times ? times : 1), speed_pct);
    return true;
}
```

`test_main.cpp` (and `RUN_TEST(test_status_mode_shows_what_claude_is_doing);`; `fresh_director()` also gets `director_set_status_mode(false);` so the other tests keep v2.2.0 behaviour; the fuzz's `case 14` becomes `director_set_liveliness(...)` or, on odd draws, `director_set_status_mode(fz_next() & 2)`):

```cpp
static void test_status_mode_shows_what_claude_is_doing(void) {
    fresh_director();
    director_set_status_mode(true);
    run_for(1000);
    UsageData d = live_payload();
    SignalsUpdate sig = no_signal();
    sig.event_fired = true; sig.event = 'p'; sig.event_seq = 1;
    send(d, sig);
    run_for(4000);                                // he points, then thinks
    TEST_ASSERT_EQUAL(STATUS_THINKING, status);
    TEST_ASSERT_EQUAL_STRING("magnifier", fake_last_act);
    world.activity = 2;
    run_for(4000);
    TEST_ASSERT_EQUAL(STATUS_WORKING, status);
    TEST_ASSERT_EQUAL_STRING("laptop", fake_last_act);
    TEST_ASSERT_FALSE(fake_mascot_autonomy());
    sig.event = 's'; sig.event_seq = 2;
    send(d, sig);
    run_for(4000);
    TEST_ASSERT_EQUAL(STATUS_DONE, status);       // `a` is still up: the Stop wins
    TEST_ASSERT_EQUAL_STRING("jumping happy", fake_last_act);
    world.activity = 0;
    director_set_status_mode(false);
    run_for(4000);
    TEST_ASSERT_EQUAL(STATUS_IDLE, status);
    TEST_ASSERT_TRUE(fake_mascot_autonomy());     // home, idle acts back
    assert_one_clawd();
}
```

- [ ] **Step 2: Run to verify it fails** — `pio test -d firmware -e native -f test_director` (CI). Expected: compile error, `director_set_status_mode` not declared.

- [ ] **Step 3: Implement**

`clawd_director.h` (after `director_set_liveliness`):

```cpp
// Status mode (clawd_prefs `sm`, default on): the corner Clawd loops an act
// for what Claude is doing and the status line says it (clawd_status in
// clawd_policy.h). Off: idle acts and whimsical verbs as before.
void director_set_status_mode(bool on);
bool director_status_mode(void);
```

`clawd_director.cpp` state (next to `on_demand`):

```cpp
static bool        status_on = true;          // status mode (clawd_prefs)
static ClawdStatus status = STATUS_IDLE;
static StatusInput status_in = {};            // what clawd_status reads
```

Autonomy follows the status (`go()` and `director_set_liveliness`):

```cpp
// Idle acts and lurk trips, unless status mode is showing something.
static bool autonomy_wanted(void) {
    return !minimal && clawd_tuning().idle_acts && (status == STATUS_IDLE || status == STATUS_WAITING);
}
```

`go()`: `mascot_autonomy(b == B_HOME && autonomy_wanted());`
`director_set_liveliness()`: `mascot_autonomy(beat == B_HOME && autonomy_wanted());`

`director_on_payload()`: after the `ok` check, `status_in.limited = strcmp(d->status, "limited") == 0;`. In the `event_fired` block, before `if (minimal) return;`:

```cpp
        if (sig->event == 'p' || sig->event == 'b') { status_in.have_prompt = true; status_in.prompt_at = now_ms(); }
        if (sig->event == 's')                      { status_in.have_stop = true;   status_in.stop_at = now_ms(); }
```

and the long-turn wave yields to the Done cheer: `} else if (sig->event == 's' && wave && !status_on) {`.

New helpers (before `run_home`):

```cpp
// Status mode, once a turn is done: jumping happy, after whatever act is
// playing (run_corner plays a pending reaction once he's free).
static void cheer_done(void) {
    if (beat != B_HOME && beat != B_CORNER) return;
    react_anim = "jumping happy";
    react_times = 1;
    react_at = now_ms();
    if (beat != B_CORNER) go(B_CORNER);
}

// Work out what Claude is doing; on a change, tell the status line, set the
// corner's autonomy and cheer when a turn is done.
static void update_status(uint32_t now, int activity) {
    ClawdStatus s = STATUS_IDLE;
    if (status_on && !minimal) {
        status_in.now = now;
        status_in.alert = attention_on;
        status_in.activity = activity;
        s = clawd_status(&status_in);
    }
    if (s == status) return;
    status = s;
    Serial.printf("[%lu] status: %s\n", (unsigned long)now, clawd_status_name(s));
    ui_set_claude_status(s);
    mascot_autonomy(beat == B_HOME && autonomy_wanted());
    if (s == STATUS_DONE) cheer_done();
}
```

`run_home()` — after the show check:

```cpp
    // Status mode: the state's act, looped. snappy skips the idle lead-in so
    // it reads as one loop; shows and the screensaver above come first.
    const char* loop = status == STATUS_WORKING ? "laptop" : status == STATUS_THINKING ? "magnifier" : NULL;
    if (loop && mascot_seated() && mascot_act(loop, 1, true)) go(B_CORNER);
```

`director_tick()` — after `if (activity > 0) last_engagement = now;`:

```cpp
    if (activity > 0) { status_in.have_active = true; status_in.active_at = now; }
    update_status(now, activity);
```

API (next to `director_set_liveliness`):

```cpp
void director_set_status_mode(bool on) { status_on = on; }
bool director_status_mode(void) { return status_on; }
```

`mascot.cpp`: `"walking", "waving", "pointing", "dancing", "jumping happy", "laptop", "magnifier",`.

`ui.h`: `#include "clawd_policy.h"` and

```cpp
// What Claude is doing (status mode): the status line says it instead of the
// whimsical verbs. STATUS_IDLE / STATUS_WAITING keep the verbs.
void ui_set_claude_status(ClawdStatus s);
```

`ui.cpp`:

```cpp
static ClawdStatus claude_status = STATUS_IDLE;

void ui_set_claude_status(ClawdStatus s) {
    claude_status = s;
    frozen_shown = nullptr;
}

// The long wording when it fits the status line as "<glyph> <text>…", else the short.
static const char* fitting_status(const char* long_text, const char* short_text) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s %s\xE2\x80\xA6", spinner_frames[1], long_text);
    return layout_text_width(L.status_font, buf) <= L.status_w ? long_text : short_text;
}
```

In `ui_tick_anim()`, the text block becomes:

```cpp
    const char* text;
    bool dots = true;                  // "…": still going
    if (!s_ble_connected) {
        text = "Waiting";
    } else if (view_state == 1) {
        text = (anim_msg_idx & 1) ? (data_received ? "Stale" : "No data") : "Listening";
    } else if (now - connected_at_ms < 5000) {
        text = "Connected";
    } else if (claude_status == STATUS_WORKING) {
        text = fitting_status("Claude is working", "Working");
    } else if (claude_status == STATUS_THINKING) {
        text = fitting_status("Claude is thinking", "Thinking");
    } else if (claude_status == STATUS_DONE) {
        text = "Done";
        dots = false;
    } else if (claude_status == STATUS_LIMIT) {
        text = "Limit reached";
        dots = false;
    } else {
        text = anim_messages[anim_verb_idx];
    }
```

and the format: `snprintf(buf, sizeof(buf), "%s %s%s", spinner_frames[anim_spinner_idx], text, dots ? "\xE2\x80\xA6" : "");`

- [ ] **Step 4: Build the C6 env and run the tests** — `python -m platformio run -d firmware -e waveshare_amoled_216_c6` (expect SUCCESS); native tests in CI.
- [ ] **Step 5: Commit** — `git commit -m "Status mode: the corner Clawd and status line follow Claude"`

---

### Task 3: `sm` on the device: payload, NVS, serial

**Files:**
- Modify: `firmware/src/data.h`, `firmware/src/payload.cpp`, `firmware/src/clawd_prefs.{h,cpp}`, `firmware/src/main.cpp`
- Test: `firmware/test/test_payload/test_main.cpp`

**Interfaces:**
- Consumes: Task 2's `director_set_status_mode`, `director_status_mode`.
- Produces: `UsageData::status_mode` (`int8_t`, -1/0/1), `void status_mode_set(bool on)`, `bool status_mode_get(void)`.

- [ ] **Step 1: Failing test** (append, register `RUN_TEST(test_status_mode);`)

```cpp
static void test_status_mode(void) {
    UsageData d;
    TEST_ASSERT_TRUE(payload_parse("{\"s\":1,\"ok\":true}", &d, nullptr));
    TEST_ASSERT_EQUAL_INT(-1, d.status_mode);      // absent: keep the saved setting
    TEST_ASSERT_TRUE(payload_parse("{\"sm\":0}", &d, nullptr));
    TEST_ASSERT_EQUAL_INT(0, d.status_mode);
    TEST_ASSERT_TRUE(payload_parse("{\"sm\":1}", &d, nullptr));
    TEST_ASSERT_EQUAL_INT(1, d.status_mode);
    TEST_ASSERT_TRUE(payload_parse("{\"sm\":7}", &d, nullptr));
    TEST_ASSERT_EQUAL_INT(1, d.status_mode);
}
```

- [ ] **Step 2: Run to verify it fails** — `pio test -d firmware -e native -f test_payload` (CI). Expected: `status_mode` is not a member.

- [ ] **Step 3: Implement**

`data.h` after `attn_anim_s`: `int8_t   status_mode;    // \`sm\` 1 status mode on, 0 off; -1 = absent (keep the saved setting)`

`payload.cpp` after the `aa` block:

```cpp
    JsonVariant sm = doc["sm"];
    d.status_mode = sm.isNull() ? -1 : ((sm | 1) ? 1 : 0);
```

`clawd_prefs.h`: add to the comment `//   status mode      \`sm\`  1 = Clawd shows what Claude is doing (default), 0 = off` and

```cpp
void status_mode_set(bool on);        // apply; saved only when it changes
bool status_mode_get(void);
```

`clawd_prefs.cpp`: in `clawd_prefs_init`, read `const bool status = prefs.getUChar("status_mode", 1) != 0;` before `prefs.end()`, call `director_set_status_mode(status);` and extend the log line with `, status mode %s` / `status ? "on" : "off"`. Then:

```cpp
void status_mode_set(bool on) {
    if (on == director_status_mode()) return;
    save("status_mode", on ? 1 : 0);
    director_set_status_mode(on);
    Serial.printf("Status mode: %s\n", on ? "on" : "off");
}

bool status_mode_get(void) { return director_status_mode(); }
```

`main.cpp`: next to `if (usage.liveliness > 0) liveliness_set(usage.liveliness);` add `if (usage.status_mode >= 0) status_mode_set(usage.status_mode == 1);`. Next to the `liveliness` serial command:

```cpp
            else if (strcmp(cmd_buf, "status_mode") == 0 || strncmp(cmd_buf, "status_mode ", 12) == 0) {
                // QA: `status_mode [0|1]` (until the app sends its own).
                if (cmd_buf[11] == ' ') status_mode_set(atoi(cmd_buf + 12) != 0);
                Serial.printf("{\"status_mode\":%d}\n", status_mode_get() ? 1 : 0);
            }
```

- [ ] **Step 4: Build C6, flash, check** — `status_mode` prints `{"status_mode":1}`; `status_mode 0` → `{"status_mode":0}`; survives a reset.
- [ ] **Step 5: Commit** — `git commit -m "Carry the status mode setting to the device as sm"`

---

### Task 4: App core: config, payload, engine

**Files:**
- Modify: `desktop/crates/clawdmeter-core/src/config.rs`, `payload.rs`, `engine.rs`

**Interfaces:**
- Produces: `Settings::status_mode: bool` (default `true`), `Payload::sm: Option<u8>`.

- [ ] **Step 1: Failing tests**

`config.rs` tests:

```rust
    #[test]
    fn status_mode() {
        assert!(settings("").status_mode, "on by default");
        assert!(!settings("status_mode = off\n").status_mode);
        let mut cfg = ConfigFile::parse("");
        let mut s = Settings::from_config(&cfg, &paths(), None);
        s.apply_to(&mut cfg, &paths());
        assert!(cfg.get("status_mode").is_none(), "the default stays out of the file");
        s.status_mode = false;
        s.apply_to(&mut cfg, &paths());
        assert_eq!(cfg.get("status_mode"), Some("off"));
        assert!(!Settings::from_config(&cfg, &paths(), None).status_mode);
    }
```

`payload.rs` full-payload test: add `sm: Some(1),` after `aa: Some(120),`.

`engine.rs`:

```rust
    #[test]
    fn status_mode_goes_out_with_the_data_and_when_it_changes() {
        let mut e = Engine::new(settings(""));
        e.on_poll_results(T0, vec![(PathBuf::from("/h/.claude"), Ok(reading(15)))]);
        let w = write(&mut e, T0).unwrap();
        assert_eq!(w.payload.sm, Some(1), "every data payload carries it");
        assert!(write(&mut e, T0 + 1).is_none(), "nothing new");
        e.update_settings(settings("status_mode = off\n"));
        let w = write(&mut e, T0 + 2).expect("a new status mode is sent right away");
        assert_eq!((w.kind, w.payload.sm), (WriteKind::Event, Some(0)));
        assert!(write(&mut e, T0 + 3).is_none());
        e.on_connected();
        assert_eq!(write(&mut e, T0 + 4).unwrap().payload.sm, Some(0), "resent on a reconnect");
    }
```

- [ ] **Step 2: Run to verify they fail** — `cargo test -p clawdmeter-core` (CI). Expected: no field `status_mode` / `sm`.

- [ ] **Step 3: Implement**

`config.rs` `Settings`: `/// Clawd shows what Claude is doing (\`sm\`, default on).` `pub status_mode: bool,`; `from_config`: `status_mode: on_off(cfg.get("status_mode"), true),`; write-back (next to `attention_anim`): `if current.status_mode != self.status_mode { cfg.set("status_mode", oo(self.status_mode)); }`.

`payload.rs`: after `aa`:

```rust
    /// Status mode: 1 = Clawd shows what Claude is doing, 0 = off. The device
    /// keeps the last value it got.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub sm: Option<u8>,
```

`PAYLOAD_KEYS`: `"rp", "lv", "aa", "sm",`.

`engine.rs`: field `/// The status mode the device last got (\`sm\`).` `sent_sm: Option<u8>,`; reset with the others (`self.sent_sm = None;`); in `next_write`: `let sm = u8::from(self.settings.status_mode);` and `prefs_changed` gains `|| self.sent_sm != Some(sm)`; `p.sm = Some(sm);` after `p.aa`; in `mark_written`: `if let Some(sm) = w.payload.sm { self.sent_sm = Some(sm); }`.

- [ ] **Step 4: Run** — CI `desktop.yml` (fmt, clippy, tests). The existing `keys_match_firmware_parser` test now sees `sm` on both sides.
- [ ] **Step 5: Commit** — `git commit -m "Send the status mode setting as sm"`

---

### Task 5: App Settings toggle

**Files:**
- Modify: `desktop/crates/clawdmeter-app/src/ui/settings.rs` (after the Attention animation field; test `toggles_and_choices_write_the_config`)

- [ ] **Step 1: Failing test** — in `toggles_and_choices_write_the_config`, after the attention-anim clicks:

```rust
        click(cx, "status-mode");
        assert!(std::fs::read_to_string(&paths.config_file).unwrap().contains("status_mode = off"));
```

- [ ] **Step 2: Implement** (after the Attention animation `pane.child(...)`):

```rust
        let me = cx.entity().downgrade();
        let status = self.settings.status_mode;
        pane = pane.child(field(
            t,
            "Show what Claude is doing",
            "Clawd and the status line follow Claude Code: thinking, working, done.",
            toggle(t, "status-mode", status, true, move |_, _, cx| {
                let _ = me.update(cx, |v, cx| {
                    v.save(|s, _| s.status_mode = !status);
                    cx.notify();
                });
            }),
        ));
```

- [ ] **Step 3: Run** — CI. Expected: PASS.
- [ ] **Step 4: Commit** — `git commit -m "Settings: turn status mode on or off"`

---

### Task 6: Docs, hardware check, release

**Files:**
- Modify: `docs/protocol.md` (row after `aa`), `docs/clawd-director.md` (new "Status mode" section), `docs/user-guide.md` (settings), `CLAUDE.md:88` (payload keys line)

- [ ] **Step 1: Docs** — `protocol.md`: `| \`sm\` | Status mode: \`1\` (the default) Clawd and the status line follow Claude Code, \`0\` off. Sent with every data payload and right away when it changes; the device keeps the last value it got. |`. `clawd-director.md`: a "Status mode" section with the state table from the spec, the loop, the Done cheer and autonomy. `user-guide.md`: one paragraph on the setting. Root `CLAUDE.md`: `lv`, `aa` and `sm`.
- [ ] **Step 2: Hardware** — flash the C6, use Claude Code: thinking → magnifier loop + "Claude is thinking…", tools → laptop + "Claude is working…", Stop → jumping happy + "Done", `status_mode 0` over serial → v2.2.0 behaviour. Check the laptop and magnifier sit well next to the meters.
- [ ] **Step 3: Push** `main`, wait for Firmware + Desktop CI (check once, no polling loop: the GitHub API limit is 60 an hour per IP).
- [ ] **Step 4: Release** — with the owner's go-ahead: tag `v2.3.0`, install the `.msi`, flash v2.3.0 from the Flash page, check the Settings toggle turns status mode off on the device.
