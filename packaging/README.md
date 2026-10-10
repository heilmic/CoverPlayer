# CoverPlayer

Offline MP3 playback for Knulli and muOS. Media files and original artwork are
not included. [Project and downloads](https://github.com/heilmic/CoverPlayer).

## Install

| Firmware | Package | Installation |
| --- | --- | --- |
| Knulli | `CoverPlayer-Knulli.zip` | Extract into the share or SD root; refresh the ports list. |
| Knulli Test | `CoverPlayer-Knulli-Test.zip` | Extract the same way; installs beside the regular app. |
| muOS | `CoverPlayer.muxapp` | Use the muOS application installer. |

On Knulli, both `roms/ports/CoverPlayer.sh` and `roms/ports/CoverPlayer/` must
exist (or their Test equivalents). Saved collections, bookmarks, progress and
cache live outside the installation folder. Keep them and your media when updating.

## Controls

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

Reopen CoverPlayer to take background playback back into the interface. Knulli
lowers other server-visible audio streams to half their original amplitude
(about -6 dB). muOS game volume may need manual adjustment.

## Screen and Bluetooth

During playback, supported firmware adapters protect audio from automatic
idle mute/suspend. The foreground display dims fully after 60 seconds; the
first press wakes it. Pause, playback end and Sleep Timer expiry release the
protection. Background playback never dims another app. Firmware settings stay
unchanged. The newer muOS power adapter still needs hardware validation.

Pair headphones in Knulli's system menu first; CoverPlayer manages already
paired devices. Bluetooth management is unavailable on muOS.

## Troubleshooting

- Knulli logs: `/userdata/system/logs/coverplayer.log` or `coverplayer-test.log`.
- muOS log: `MUOS/log/coverplayer.log` on application storage.
- Knulli ducking: `COVERPLAYER_DUCK_PERCENT=50` by default (0–100);
  `COVERPLAYER_AUDIO_DUCKING=0` disables it. Direct ALSA streams are outside it.
- `COVERPLAYER_DIM_SECONDS` sets the foreground dim timeout (1–3600 seconds).
- Intermittent Bluetooth audio stutter is under investigation; a clean audio
  log does not rule it out.

See the repository's `docs/README.md` for architecture, compatibility and
recovery limitations. License: `LICENSE`; dependencies: `THIRD_PARTY_NOTICES.md`.
