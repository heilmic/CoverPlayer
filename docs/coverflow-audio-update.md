# Background audio and game volume

[Documentation](README.md) · [Controls](../packaging/README.md)

Hold Start for two seconds during playback to continue the album in the
background on Knulli or muOS. Reopening CoverPlayer returns playback to the GUI.

## Knulli ducking

Other PulseAudio-compatible streams play at half their original audio amplitude
(about -6 dB), including games launched later. The guardian runs independently
of decoding. Master volume, mute switches and stereo balance are preserved.

| Setting | Default | Meaning |
| --- | --- | --- |
| `COVERPLAYER_DUCK_PERCENT` | `50` | Percentage of original amplitude, 0–100 |
| `COVERPLAYER_AUDIO_DUCKING` | `1` | Set to 0 to disable |

Both Knulli launchers enable this only when `pactl` and `timeout` exist. The
guardian polls every 350 ms with a two-second server-call timeout. muOS does
not enable automatic game-volume ducking.

## Restoration and limitations

Saved per-channel levels are restored when the background helper exits,
including after SIGKILL, unless the user changed those levels. Stream identity
checks protect against reused IDs. A journal supports next-launch recovery
when the guardian failed.

Matching application restore groups and exact remembered levels prevent
repeated attenuation when games restart. A recovery watcher restores returning
streams after playback ends and yields to a new guardian. Full recovery after
power loss or audio-server restart still needs validation/startup integration.
Direct ALSA streams bypassing the server cannot be controlled.

## Validation

The user confirmed audible, reduced game music and normal audiobook volume on
Knulli, including switching games. Automated lifecycle tests cover late streams,
manual adjustments, stereo balance, recycled IDs, helper/guardian failure and
opt-out. Broader device, Bluetooth reconnection and restart checks remain in the
[roadmap](../TODO.md).

```sh
python3 tests/audio_ducker_integration_test.py build/desktop-debug/coverplayer_audio_ducker_test
```

For the current rendering design, see [architecture](architecture.md#rendering).
