# Playback and screen power saving

CoverPlayer keeps active audio playback independent of the firmware's idle mute
and automatic suspend. No firmware configuration or system scripts are edited.

## Behavior

- After 60 seconds without foreground input during playback, lower the backlight
  to zero. Skip drawing while dark and reduce UI polling frequency.
- The first keyboard/controller input only wakes the screen; a later press acts
  normally. Restore the preceding brightness on wake, pause, stop and normal exit.
- Release the temporary idle inhibitor on pause, finish, Sleep Timer expiry or exit.
- A separate watcher restores brightness and removes owned inhibitors after the
  player exits unexpectedly, including SIGKILL. It does not unmute user audio.
- Background playback inhibits automatic idle mute/suspend but never changes
  another application's brightness. Manual power-button actions are not blocked.
- `COVERPLAYER_DIM_SECONDS` overrides the foreground inactivity timeout (1..3600).

## Firmware adapters

Knulli: create `/var/run/battery-saver/coverplayer-PID.pause` only while playing.
The installed idle and extended hooks honor this runtime mechanism. Use the
backlight node if available, otherwise `knulli-brightness` for hardware such as
H700. Only the display level is changed; Knulli settings remain untouched.

muOS: enable only when the installed `muhotkey` exposes both PID-based
`/run/muos/idle_game_inhibit` and `/run/muos/idle_sleep_inhibit` slots and
`bright.sh` supports the `transient` argument. Respect other live owners of the
shared slots. Use `bright.sh VALUE transient` to avoid saving brightness settings.
Older versions without these capabilities are left untouched; they require a
separate compatibility adapter. muOS hardware validation is still pending.

Primary sources consulted on 2026-10-10:

- Device .145: `/etc/idlewatcher/{idle,extended,active}.d`,
  `/etc/idlewatcher/idlewatcher.conf`, `/usr/bin/knulli-brightness`.
- https://github.com/MustardOS/frontend/blob/main/module/muhotkey.c
- https://github.com/MustardOS/frontend/blob/main/common/base/options.h
- https://github.com/MustardOS/internal/blob/main/script/device/bright.sh
- https://github.com/atalaygrgn/XMPlayer/blob/main/.xmplayer/systems/system.lua

## Regression checks

`coverplayer_playback_power_test` runs on Linux using an isolated temporary
filesystem (`COVERPLAYER_POWER_TEST_ROOT`), with no changes to real device
settings. Covers full dimming, wake, pause, background isolation, external
brightness changes, SIGKILL cleanup, muOS transient brightness and slot ownership.
SDL input tests cover wake-only presses and START release behavior.

## Knulli .145 validation (2026-10-10)

Eight desktop tests passed, including wake-only controller input. The ARM64
lifecycle test passed directly on .145 using isolated Knulli and muOS fixtures.
A real-hardware check reduced `knulli-brightness` from 50 to 0, invoked both idle
and extended hooks with the CoverPlayer inhibitor held, confirmed `Mute: no`,
and restored brightness to 50. All owned inhibitors were removed afterward.
The Knulli configuration hashes remained:

- `/userdata/system/knulli.conf`:
  `e4d68c68fbeadbb37a9acba6581dd6209eefc25ab8df3892aa95a61c27664ad0`
- `/etc/idlewatcher/idlewatcher.conf`:
  `9d6bf6f60b1b29d9e2ff32f4651d832c5efa13f2b2560a54afb1ad10561d9fa9`

Natural foreground dimming was subsequently observed on the device. The user
accepted the current behavior. Inactive startup and repeated inactive ticks
are covered by regression tests; the app was also observed open without an
inhibitor when not playing. Intermittent Bluetooth stutter remains a separate
open issue. muOS was tested with fixtures, not real muOS hardware.
