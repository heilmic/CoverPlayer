# Releases

## Unreleased

- Add accelerated hold-to-scroll for D-pad, keyboard arrows and the left analog
  stick. Share direction mapping, suppress stick drift/diagonal jitter and stop
  repeats on release, wake-only input, help, focus loss and screen changes.

- Smooth rapid CoverFlow navigation with continuous position and velocity.
  Decode artwork off the render thread, retain 24 recent covers and cache text;
  defer periodic system-status reads while navigating. Resolve SDL geometry
  support at runtime so newer H700 firmware can batch cover drawing even when
  the ARM binary was built with older SDL headers.
- Refresh the README with a native music-cover GIF and shorter documentation.

- Keep playback audible through firmware idle timeouts using temporary Knulli
  inhibitors; dim the foreground backlight fully after 60 seconds without input.
  Wake-only first input, paused/finished playback cleanup and crash restoration.
- Add capability-gated muOS transient brightness and PID-inhibitor support.
  Existing firmware settings are preserved; older muOS versions need validation.

### CoverFlow readability on handhelds

- Remove the oval backdrop, floor reflection and selection underline.
- Set focused artwork to 256 pixels (264 for a single album), keeping it readable
  with more room around the covers. Ease navigation more gently and match the
  movement speed at the transition between the first and outer side slots.
- Preserve the original side-cover rotation, spacing and depth for the familiar
  page-turning feel; retain the album caption.
- Area-filter decoded cover thumbnails and use linear texture filtering, with
  a single filtered draw for the centered artwork on older SDL versions too.
- Reset navigation animation when switching to a collection of a different size.


- Default Knulli game-audio ducking to half the linear amplitude (about -6 dB).
  Prevent compounded attenuation when games restart at a remembered volume,
  and recover returning streams after background playback ends.

- Refine CoverFlow with projected perspective and missing-cover placeholders.
- Add an opt-out Knulli audio guardian for existing and newly created streams,
  relative per-channel attenuation, exact unchanged-volume restoration, and
  cleanup after helper termination. Preserve manual volume changes and mute.
- Always take over an existing background session when opening a media file.
- Add parser/volume tests and a fake-audio-server lifecycle regression test.
- Hardware validation and persistent application volume recovery after a hard
  restart remain pending (see TODO.md).

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
