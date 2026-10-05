# Releases

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
