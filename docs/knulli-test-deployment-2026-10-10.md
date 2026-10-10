# Knulli test deployment — 2026-10-10

Target: 192.168.10.145 (aarch64). Installed CoverPlayer-Test only.

- Docker Desktop startup recovered by backing up stale runtime directories;
  no factory reset was performed by the deployment workflow.
- Desktop build, eight CTest tests and package-verifier regression tests passed.
- ARM64 release build and verification of all three packages passed.
- Uploaded archive hash and both installed manifests verified.
- Previous test app, launcher and configuration backed up on the device at
  `/userdata/system/coverplayer-backups/CoverPlayer-Test-20261010-061213-4171.tar.gz`.
- Running Syncthing processes paused for installation and confirmed running afterward.
- Production CoverPlayer binary and launcher hashes unchanged.
- ARM64 audio-ducker unit test, background-helper identity test and fake-pactl
  lifecycle test passed directly on the target. The lifecycle test covers late
  streams, stereo balance, own-stream exclusion, manual changes, recycled IDs,
  SIGKILL cleanup, guardian crash recovery and opt-out.

Installed binary SHA256:
`99993b243f785b0720bf46ac1c9033eb4b700c452d2d8a9cd3b686e719e79255`

Test archive SHA256:
`a50424c554d93f9483e92bb88c81f2865d053177767dd7c70f3b8a81f03ef7f4`

This is a test deployment, not a published GitHub release. Real playback,
Bluetooth, controller navigation, cover animation and frame rate remain manual
device checks. The automated lifecycle test uses a fake audio server and does
not validate interaction with the real Knulli audio server.

## Follow-up: half-amplitude ducking and game-switch recovery

Updated CoverPlayer-Test on 192.168.10.145 later on 2026-10-10.

- Default attenuation now halves linear audio amplitude (about -6.02 dB).
- New streams at a remembered ducked level inherit their restore group's original
  channel volumes instead of compounding attenuation. Exact level matching is
  required; manual changes and unrelated restore groups remain independent.
- After the audiobook stops, a recovery watcher can restore a game returning at
  its remembered level. It yields to a new playback guardian.
- Eight desktop tests passed. ARM64 fake-pactl regression tests passed directly
  on the device, including five consecutive game switches, delayed restoration,
  recovery-watcher handoff, stereo balance, manual changes and recycled IDs.
- Deployment backed up the test installation and configuration to
  `/userdata/system/coverplayer-backups/CoverPlayer-Test-20261010-063930-14971.tar.gz`.
- Background audiobook playback was briefly stopped for installation and resumed
  from saved progress; the running Tetris process was not stopped.
- Real RetroArch stream measured at -6.02 dB after installation, with the
  CoverPlayer stream unchanged at 100%. User confirmation of subsequent real
  game switches remains the next device check.
- Production CoverPlayer binary and launcher hashes remain unchanged.

Updated test binary SHA256:
`6125c54157ef93737fe9d6f3430d33b48d51dd4f1c95f9562c56dd0862b2ba62`

## Follow-up: larger, cleaner CoverFlow

- Installed Test only on 192.168.10.145; production binary and launcher hashes
  were rechecked and remain unchanged.
- Removed oval lighting, reflections and selection underline; selected cover
  now measures 280 pixels (288 for a single album), with less rotated and more
  widely spaced neighboring covers. Area-filtered thumbnails and linear texture
  sampling improve reduction quality, including the centered older-SDL path.
- Eight desktop tests and all package verification checks passed. Native-size
  previews inspected for the middle, collection edge and single-album cases.
  German/English demo screenshots and README animations regenerated.
- Closed the running Test GUI and inactive old audio recovery watcher before
  installation. The updated app is ready to open from Ports.
- Backup: `/userdata/system/coverplayer-backups/CoverPlayer-Test-20261010-065122-19087.tar.gz`.
- Installed Test binary SHA256:
  `d3c37318acdc832209b78f29fff9eb49a46dcd50b1fd85ed744b788709305562`.
- Deployment and installed payload verified over SSH. Actual rendering and
  controller navigation of this updated build remain a user device check.

## Follow-up: restore original CoverFlow motion

At the user's request, restored the original neighboring-cover rotation (0.98
radians), spacing (158 pixels to the first neighbor, then 48 per slot), and
perspective depth (0.13 per slot). Retained the larger selected cover, improved
filtering and removal of oval/reflections. Clip artwork to the area between the
header and caption so the larger rotating covers cannot overpaint either.

Eight desktop tests and package verification passed. Updated native previews
and both README animations were visually reviewed. Final Test binary SHA256:
`5e3faae104ae6bc33c4a0568a4f28005cd702ac1337489a9373fd68e44edba66`.

Final installation verified on .145 against root and app manifests and the
expected binary SHA256. App backup:
`/userdata/system/coverplayer-backups/CoverPlayer-Test-app-20261010-070101.tar.gz`.
The existing full backup from 06:56 remains available. A subsequent full backup
attempt stopped because the user reopened the GUI during configuration archiving;
installation was retried after closing it, with an app-only backup. Configuration
was not overwritten. Test app is ready to reopen.

## Follow-up: playback idle protection and full backlight dimming

Installed and verified Test only on .145 after the playback-power fixture and
real-backlight checks described in [playback-power-saving.md](playback-power-saving.md).
Production binary/launcher and both Knulli configuration hashes were rechecked
and remain unchanged. Backup:
`/userdata/system/coverplayer-backups/CoverPlayer-Test-20261010-083833-6395.tar.gz`.
Installed Test binary SHA256:
`cd6b568da1db4342d902dc11ec5b0b1c0ee962d398058152da96e1439fbf4c80`.
The app is ready to reopen; natural 60-second dimming and continued foreground
playback past Knulli's five-minute timeout remain user acceptance checks.

## Follow-up: rapid navigation and documentation refresh

Test build `a50bcabbc459220abc4b0c2d66a255b386437e0d5ef38dde72bf003af865d95d`
was installed and manifest-verified on .145. Backup:
`/userdata/system/coverplayer-backups/CoverPlayer-Test-20261010-130454-6901.tar.gz`.

Changes: continuous position/velocity on retarget, off-thread cover decoding,
24-cover and 64-text caches, navigation-aware status polling, and runtime SDL
geometry detection. The H700's newer SDL now exposes batched cover drawing to
the binary built with older headers. Cover size remains 256/264 pixels.

Ten desktop tests passed. Motion, cover-cache and power lifecycle checks passed
on .145. Its SDL build lacks the dummy video driver, so the renderer test ran
with `--device` using a real window and logged `batched geometry`. This verifies
loading, eviction, missing artwork and shutdown; it is not an FPS benchmark.
Rapid handheld navigation still needs user assessment. Both native galleries
and music GIFs were regenerated. Documentation links and package checks passed.

## Follow-up: held directions and left analog stick

Installed and verified Test build
`4f36c7af7af5528e3a5050ed0913b45ff5aee493f3f6c96acc3e5e643e6e3ffe`.
Backup: `/userdata/system/coverplayer-backups/CoverPlayer-Test-20261010-131306-8319.tar.gz`.
D-pad, keyboard arrows and left stick share direction mapping. Browsing repeats
after 330 ms (110 ms steps, then 65 ms after 1.2 seconds); player actions stay
single-step. Ten desktop tests and the ARM input regression test on .145 passed.
Help screenshots and installation controls were updated. Physical stick feel
and hold speed remain user acceptance checks.
