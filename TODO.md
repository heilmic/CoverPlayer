# Future work

## Safe background-audio ducking on Knulli

The current background helper lowers only audio streams that already exist
when background playback starts. A game launched afterward can remain at full
volume. It also restores other streams to a fixed 100% instead of their
original levels. Do not describe this as reliable automatic volume balancing.

For a later version, implement Knulli-specific, per-stream ducking with these
invariants:

- Duck only while CoverPlayer's background playback helper is actually active.
  A normal CoverPlayer exit without background playback must not duck audio.
- Observe newly created game/EmulationStation streams as well as streams that
  were already playing when background playback began.
- Save each affected stream's original volume and mute state and restore those
  exact values when background playback stops or CoverPlayer takes over again.
  Do not change the master device/sink volume or set other streams to a fixed
  100% during cleanup.
- Handle normal exit, helper failure, interrupted handover, and device restart
  safely. Do not assume a reboot clears ducked levels: the audio session
  manager may persist per-application stream volumes.
- Verify behavior on the actual Knulli audio stack, including a game launched
  after backgrounding, reopening CoverPlayer, exiting without backgrounding,
  and a killed helper. Keep this optional platform capability disabled where
  the required audio controls are unavailable.

Until this is implemented and device-tested, lower RetroArch's game audio
manually when needed.
