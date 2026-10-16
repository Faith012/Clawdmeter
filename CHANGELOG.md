# Changelog

This is [Faith012/Clawdmeter](https://github.com/Faith012/Clawdmeter), a fork
of [HermannBjorgvin/Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter).
It follows upstream and adds its own releases on top. Upstream's history, up
to the v2.1.0 this fork is based on, is in
[its releases](https://github.com/HermannBjorgvin/Clawdmeter/releases).

Each release's section below is also its GitHub release text.

## v2.5.0

Firmware only: the desktop app is the same as v2.3.1. Update the board from
the app's **Flash** page.

### What's Changed

* **Clawd Jump, a game on the ESP32-C6-Touch-AMOLED-2.16.** Hold a finger
  on the screen for about 2 seconds. Clawd bounces up an endless stack of
  platforms on his own, and you steer him by tilting the board left or
  right. Leaving one side of the screen brings him in from the other, and
  the view scrolls up as he climbs. The gaps grow with the score. Fall off
  the bottom and the screen shows your score and your best, which the board
  keeps across restarts; a tap plays again.
  ([dc1373d](https://github.com/Faith012/Clawdmeter/commit/dc1373d),
  [design](https://github.com/Faith012/Clawdmeter/blob/main/docs/superpowers/specs/2026-10-15-clawd-jump-design.md))
  * **Leave** with a single PWR press or another 2-second hold. The game also
    ends on its own 30 seconds after game over, or when the screen sleeps.
    Holding PWR for 3 seconds still clears pairing.
  * **Alerts** pause it: the meters and the alert come back, and afterwards
    the game returns frozen with a play sign; a tap carries on.
  * **Extras**, now and then: a **spring** on a platform sends him three
    times as high; a small **cloud** (from score 50) gives him a ride
    straight up on his cloud for a few seconds, through everything; and
    from score 150 **bugs** crawl across the gaps. Land on a bug to squash
    it; touch one any other way and he's knocked off.
    ([b92756c](https://github.com/Faith012/Clawdmeter/commit/b92756c))
  * The screen doesn't rotate while you play, and no shows or screensaver
    interrupt it. The meters keep updating underneath.

  Clawd is drawn from the official "jumping" and "cloud" animations, on a grid of 4 px
  cells written straight to the panel; only the cells that change each
  frame are sent, at about 30 frames a second. The grid's 29 KB are taken
  only while you play.

Tested on an ESP32-C6-Touch-AMOLED-2.16. Other boards are unchanged: the
game needs a tilt reading (`imu_hal_tilt`) and `direct_performer`.

**Full Changelog**: https://github.com/Faith012/Clawdmeter/compare/v2.4.0...v2.5.0

## v2.4.0

Firmware only: the desktop app is the same as v2.3.1. Update the board from
the app's **Flash** page.

### What's Changed

* **The full-size Clawd, everywhere, on the ESP32-C6-Touch-AMOLED-2.16.**
  The C6 has no PSRAM for the full-size Clawd's buffers, so v2.2.0 drew his
  scenes straight to the panel on a black screen, and the rest still fell
  back to the corner. A new `direct_sprite` module draws any full-size Clawd
  straight to the panel and keeps him in front of the meters, as on S3
  boards: only his coloured cells are drawn, the cells he leaves go back to
  the display library to redraw the meters, and after every redraw he is
  painted over it again. All three full-size Clawds use it now:
  * **Alerts:** when Claude needs you, the corner Clawd walks off and the
    big one walks on, waving and pointing at you over the meters, with the
    alert's line still readable below him. A tap sends him off. Before, he
    only waved from his corner seat.
  * **Shows and calm dips:** the scenes play in front of the meters, which
    stay visible behind him, instead of on a black screen.
  * **Lurk trips:** now and then he walks off and peeks back in from the
    right edge, full size. The C6 skipped these before.

  Smooth, with no flicker or leftovers, including rotating the board in the
  middle of any of them: he is written into each band of the screen before
  it is sent, so the meters never flash through him. One 8 KB buffer serves
  all three, since only one Clawd is ever on screen.
  ([383a66d](https://github.com/Faith012/Clawdmeter/commit/383a66d),
  [2bfd881](https://github.com/Faith012/Clawdmeter/commit/2bfd881))

Tested on an ESP32-C6-Touch-AMOLED-2.16. Other boards are unchanged: the
mode only turns on for boards checked on hardware (`direct_performer`).

**Full Changelog**: https://github.com/Faith012/Clawdmeter/compare/v2.3.1...v2.4.0

## v2.3.1

Firmware only: the desktop app is the same as v2.3.0. Update the board from
the app's **Flash** page.

### What's Changed

* **Clawd reacts to Claude Code the moment it happens.** The app sends every
  live hook event (a prompt, a Stop, the turn's first tool call) in a payload
  marked `rp:1`, because its usage numbers are cached. The firmware dropped
  events from such payloads, so the board never reacted to a prompt or a
  Stop. This bug dates back to upstream v2.0: it is why the noticing point
  and the long-turn wave never showed, and in status mode why "Claude is
  working" lingered for about a minute after Claude finished, with no
  thinking and no Done. Events now fire on a new sequence number whatever
  the flag; heartbeats repeat the number the board already has, so nothing
  fires twice. ([48ce8f7](https://github.com/Faith012/Clawdmeter/commit/48ce8f7))
* **The first event after a reboot or reconnect is no longer lost.** The
  board took the first sequence number it saw as a baseline, so a Stop
  right after it came back was swallowed. A payload with no event now tells
  it nothing stale is pending. ([48ce8f7](https://github.com/Faith012/Clawdmeter/commit/48ce8f7))
* **Thinking shows right after a reboot.** The board now knows from the
  app's first payload (it carries `sm`) that the app marks each turn's
  first tool call, instead of waiting for one; until then a prompt read as
  working. ([ec6941e](https://github.com/Faith012/Clawdmeter/commit/ec6941e))
* **Status mode starts thinking at once.** On a prompt Clawd goes straight
  to the magnifier instead of first pointing at the title for about three
  seconds. With status mode off he still points. ([ae98271](https://github.com/Faith012/Clawdmeter/commit/ae98271))
* Release notes now come from this file. ([CHANGELOG.md](https://github.com/Faith012/Clawdmeter/blob/main/CHANGELOG.md))

Tested on an ESP32-C6-Touch-AMOLED-2.16 with Claude Code in VS Code.

**Full Changelog**: https://github.com/Faith012/Clawdmeter/compare/v2.3.0...v2.3.1

## v2.3.0

Desktop app and firmware: install the new `.msi` (or your platform's
package), then update the board from the **Flash** page.

### What's Changed

* **Status mode: Clawd shows what Claude Code is doing.** The corner Clawd
  and the status line become a live readout
  ([design](https://github.com/Faith012/Clawdmeter/blob/main/docs/superpowers/specs/2026-10-15-status-mode-design.md)):

  | Claude Code | Clawd in the corner | Status line |
  | --- | --- | --- |
  | Thinking (a prompt, no tool calls yet) | searches with his magnifier | `Claude is thinking…` |
  | Working (running tools) | takes his laptop out once and types until Claude is done | `Claude is working…` |
  | Done (Stop) | puts the laptop away and jumps for joy | `Done` (five minutes) |
  | Needs you (an alert) | the usual alert | the alert's message |
  | Out of quota | sits still | `Limit reached` |

  Shows, the PWR scenes, the screensaver and alerts work as before; they
  take over from the readout and it resumes after them. It works at every
  Silliness level. ([1598d0b](https://github.com/Faith012/Clawdmeter/commit/1598d0b),
  [628f321](https://github.com/Faith012/Clawdmeter/commit/628f321),
  [faedf4e](https://github.com/Faith012/Clawdmeter/commit/faedf4e),
  [553079f](https://github.com/Faith012/Clawdmeter/commit/553079f))
* **A setting to turn it off.** **Show what Claude is doing** on the app's
  Device page (on by default). Off, Clawd behaves as in v2.2.0. The board
  remembers it across restarts; the `status_mode [0|1]` serial command sets
  it for testing. ([3b2def0](https://github.com/Faith012/Clawdmeter/commit/3b2def0),
  [0d4d0f6](https://github.com/Faith012/Clawdmeter/commit/0d4d0f6),
  [d8d8a3c](https://github.com/Faith012/Clawdmeter/commit/d8d8a3c))
* **Protocol additions** (older firmware ignores both): `sm` carries the
  setting; `e:"w"` marks the first tool call of each turn, so a new prompt
  reads as thinking even while `a` still counts the last turn's tool calls.
  See [docs/protocol.md](https://github.com/Faith012/Clawdmeter/blob/main/docs/protocol.md).
* The corner Clawd's buffer grows by about 6 KB to fit the laptop and the
  magnifier.

**Known issue, fixed in v2.3.1:** the board ignored prompt and Stop events,
so thinking and Done rarely showed and working lingered about a minute.

**Full Changelog**: https://github.com/Faith012/Clawdmeter/compare/v2.2.0...v2.3.0

## v2.2.0

Firmware: update the board from the **Flash** page.

### What's Changed

* **Full-size Clawd on the ESP32-C6-Touch-AMOLED-2.16.** The C6 has no PSRAM
  for the 690 KB buffer the full-size scenes use, so it only ever played
  them as small corner acts. A new `direct_performer` board capability draws
  Clawd straight to the panel instead, one row of the 60×60 grid at a time
  from an 8 KB strip, repainting only what changed, with the meters hidden
  while he's on. Earned shows and the screensaver's calm dips now look as
  they do on S3 boards, smooth, and survive rotating the board mid-scene.
  Only boards checked on hardware turn it on; every other board is
  unchanged. ([95d821c](https://github.com/Faith012/Clawdmeter/commit/95d821c))
* **Scenes on demand with PWR.** A double press on the PWR (middle) button
  plays a random scene now, never the same one twice running, without using
  up the earned shows' cooldown. During any scene a single press moves on to
  the next. A single press elsewhere still cycles brightness, about half a
  second later (the double-press window). ([446d45b](https://github.com/Faith012/Clawdmeter/commit/446d45b))

**Full Changelog**: https://github.com/Faith012/Clawdmeter/compare/v2.1.1...v2.2.0

## v2.1.1

The first release of this fork, on top of upstream v2.1.0.

### What's Changed

* **No more console windows flashing on Windows.** While the app's window
  was open it ran `reg.exe` every three seconds to check Start at login, and
  each run flashed a console window that stole focus, so clicks in the app
  were lost. Every console program the app starts now runs without a
  window. ([a1c0d87](https://github.com/Faith012/Clawdmeter/commit/a1c0d87))
* **Updates come from this fork.** The app's update check and its Flash page
  read releases from Faith012/Clawdmeter, so they offer this fork's firmware
  rather than upstream's. `release_repo` in the config still overrides it.
  ([7e7ee12](https://github.com/Faith012/Clawdmeter/commit/7e7ee12))

**Full Changelog**: https://github.com/Faith012/Clawdmeter/compare/887a5e9...v2.1.1 (upstream v2.1.0 is 887a5e9)
