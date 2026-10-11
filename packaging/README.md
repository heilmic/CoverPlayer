# CoverPlayer

Offline MP3 playback for Knulli and muOS. Media files and original artwork are
not included. [Project and downloads](https://github.com/heilmic/CoverPlayer).

## Install

| Firmware | Package | Installation |
| --- | --- | --- |
| Knulli | `CoverPlayer-Knulli.zip` | Extract into the share or SD root; refresh the ports list. |
| Knulli category | `CoverPlayer-Knulli-Category.zip` | Complete regular app with its own main-menu category; [guide and screenshots](https://github.com/heilmic/CoverPlayer/blob/main/docs/knulli-category.md). |
| Knulli Test | `CoverPlayer-Knulli-Test.zip` | Extract the same way; installs beside the regular app. |
| muOS | `CoverPlayer.muxapp` | Use the muOS application installer. |

- Knulli needs both `roms/ports/CoverPlayer.sh` and `roms/ports/CoverPlayer/` (or Test equivalents).
- Updates: retain media and saved data; collections, bookmarks, progress and cache live outside the app folder.

## Controls

D-pad / left stick: navigate; hold to scroll faster. Seeking and track changes stay single-step.

| Button | Action |
| --- | --- |
| A | Open selection; pause/resume on the player |
| B | Back; does not exit from the collection screen |
| Select | Context help; B or Select closes it |
| Y | Add a collection; switch CoverFlow/list in the library; switch language in help |
| X | Rescan all configured collections from collection/library screens |
| Left / Right | Seek 10 seconds on the player |
| L1 / R1 | Seek 30 seconds on the player |
| Up / Down | Previous/next track on the player |
| Start, short press | Cycle sleep timer on the player |
| Start, hold 2 seconds | Leave with background playback on Knulli/muOS |
| Start + Select | Exit without background playback |

- Reopen CoverPlayer to return from background playback.
- Knulli lowers other server-visible audio by about 6 dB; muOS may need manual game-volume adjustment.

## Screen and Bluetooth

- Foreground dims after 60 seconds; first press wakes it. Background playback never dims other apps.
- Supported firmware: playback prevents idle mute/suspend; pause, playback end or sleep-timer expiry restores normal behavior. Firmware settings remain unchanged.
- Newer muOS power adapter: hardware validation pending.
- Bluetooth: pair in Knulli's system menu first; manage paired devices in CoverPlayer. No muOS Bluetooth management.

## Troubleshooting

- Knulli logs: `/userdata/system/logs/coverplayer.log` or `coverplayer-test.log`.
- muOS log: `MUOS/log/coverplayer.log` on application storage.
- Knulli ducking: `COVERPLAYER_DUCK_PERCENT=50` by default (0–100);
  `COVERPLAYER_AUDIO_DUCKING=0` disables it. Direct ALSA streams are outside it.
- `COVERPLAYER_DIM_SECONDS` sets the foreground dim timeout (1–3600 seconds).
- Intermittent Bluetooth audio stutter is under investigation; a clean audio
  log does not rule it out.

## Album playback and artwork

- **B** from player: return to track list, keep listening. **B** again: leave album, pause and save.
- One-track album: return directly to browser and pause.
- **A** in list: play selected track; selecting the current track returns to the player without restarting it.
- **Up/Down** in player: previous/next track, without wrapping.
- Player cover: embedded artwork → folder cover → placeholder. Album browser prefers folder artwork.
- Collection manager: **X** removes, **A** confirms, **B** cancels. Media stays; last collection cannot be removed.

[More documentation](https://github.com/heilmic/CoverPlayer/blob/main/docs/README.md) ·
[Switch guide](https://github.com/heilmic/CoverPlayer/blob/main/packaging/switch/README.md)

License: `LICENSE` · Dependencies: `THIRD_PARTY_NOTICES.md`
