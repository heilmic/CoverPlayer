# Releases

## v1.0.0 — First Stable Release

CoverPlayer reaches its first stable milestone: an offline MP3 player for
music, audiobooks, radio plays and podcasts on handheld gaming systems.
Knulli is the primary platform, with muOS packages also available.

Packaging addendum: `CoverPlayer-Knulli-Category.zip` is a complete alternative
Knulli download with a dedicated main-menu category, illustrated setup guide
and optional Art Book Next SVG player icon. It reuses the unchanged 1.0.0
regular app and saved data. [Setup and screenshots](docs/knulli-category.md).

### Highlights

- **Fluid CoverFlow on H700:** continuous motion through rapid taps and direction
  changes, background image decoding, recent-cover/text caches and runtime SDL
  geometry batching. Readable 256-pixel artwork, rotated neighbors and a clean
  layout without the oval backdrop or reflection.
- **Faster controller navigation:** hold the D-pad, keyboard arrows or left
  analog stick to scroll, with acceleration and stick dead zones. Release to
  stop; player seeking and track selection remain single-step.
- **Listen while gaming:** independent background playback across apps and games,
  with reliable GUI takeover and Knulli game-audio ducking to half amplitude
  (about -6 dB). Game restarts do not compound attenuation; manual adjustments
  are respected and unchanged levels are restored when listening ends.
- **Screen power saving:** fully dim the foreground backlight after 60 seconds
  without input while keeping audio audible. The first press wakes the screen.
  Pause, playback end and Sleep Timer expiry release automatic-standby protection.
  Firmware settings are preserved; a watcher handles unexpected player exits.
- **A complete offline library:** named collections, nested folders, CoverFlow
  and list views, folder/embedded artwork, saved progress, bookmarks, automatic
  next track and a sleep timer. German and English help, plus Knulli management
  of already paired Bluetooth headphones.
- **Updated presentation and deployment:** native music-cover GIF, refreshed
  galleries, shorter documentation and verified Test-app installation with
  backups and temporary Syncthing suspension.

### Downloads and installation

- **CoverPlayer-Knulli.zip:** regular Knulli port. Extract into the share or SD
  root, then refresh the ports list.
- **CoverPlayer-Knulli-Test.zip:** separate Test port, installed the same way.
- **CoverPlayer.muxapp:** install through the muOS application installer.

Keep existing media and saved application data when updating. Audio files and
original cover images are not bundled. See the
[installation guide](https://github.com/heilmic/CoverPlayer/blob/v1.0.0/packaging/README.md).

![Music CoverFlow](https://raw.githubusercontent.com/heilmic/CoverPlayer/v1.0.0/docs/screenshots/generated/en/coverplayer-coverflow.gif)

### Validation and compatibility

Ten desktop regression tests passed, including frame-rate-independent motion,
async artwork caching and controller repeat/stick handling. ARM input, renderer
cache and power-lifecycle checks passed on Knulli device .145. The user confirmed
smooth H700 navigation, held scrolling, left-stick control and the current
playback behavior. All three release packages pass manifest verification.

The newer capability-gated muOS power-saving adapter is fixture-tested and still
needs hardware validation; older muOS versions without the required capabilities
leave it disabled. Automatic game-audio ducking is Knulli-only and cannot affect
direct ALSA streams. Full recovery after power loss/audio-server restart and
intermittent Bluetooth audio stutter remain follow-ups. Replaced artwork under
the same filename can require an app restart. See the
[known issues](https://github.com/heilmic/CoverPlayer/blob/v1.0.0/TODO.md).

## v0.1.0-rc.4 — simpler single-track playback



- Opening a folder with one MP3 now starts or resumes it immediately; folders

  with multiple tracks still show the track list.

- Added a muOS-compatible CoverPlayer app icon.

- Rebuilt the Knulli and muOS packages from the same application source.



## v0.1.0-rc.3 — muOS hardware support



- CoverPlayer now starts and plays audio on muOS Jacaranda using the device's

  SDL2 and ALSA/PipeWire libraries. Volume display and controls follow the

  muOS system volume.

- The current handheld layout, covers, help, normal playback, and background

  playback have been tested on a muOS handheld.

- The sleep timer has also been confirmed to end playback cleanly on muOS.

- Track lists now give filenames the full screen width; Now Playing keeps its

  large cover while wrapping long track names on both 640px and 720px screens.

- Knulli and muOS packages are built from the same application source.

  Bluetooth management remains Knulli-only. The Knulli packages were rebuilt

  and passed automated tests, but were not retested on hardware for this RC.

- Package manifests now use Unix line endings so they can be checked directly

  on handhelds with `sha256sum -c`.

- The muOS launcher uses the system's SD1/SD2-aware application and save paths.



## v0.1.0-rc.2 — English interface



- Added an English interface. Open Help with Select and press Y to switch

  languages; the choice is saved for future launches.

- small minor fixes.



The Knulli packages have been exercised on an RG34XXSP. The muOS package

remains a hardware-untested candidate. Background game-audio balancing remains

experimental; see [TODO.md](TODO.md).



## v0.1.0-rc.1 — first public release candidate



- Knulli package for controller-driven offline MP3 playback; an isolated

  Knulli test package is also available.

- Named collections, cover browsing or list view, recursive folders, library

  cache, resume, bookmarks, sleep timer, and Bluetooth controls for already

  paired Knulli devices.

- Background playback on Knulli, with automatic return to the player when the

  app is reopened.

- muOS package included as an **untested hardware candidate**, not a stable

  muOS release.



Known limitations: automatic game-audio balancing in the background is

experimental. Streams started after background playback may stay at normal

volume, and existing stream volumes are not restored exactly. See

[TODO.md](TODO.md). Commercial artwork visible in repository screenshots is

illustrative and is not bundled in the installable archives.



All three archives passed the package manifest/hash verifier. This release

candidate does not claim a complete device acceptance test of these exact

archives.
