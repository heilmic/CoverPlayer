# Releases

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
