# Architecture

CoverPlayer uses a portable C++17 application core and optional platform adapters.
Knulli is the primary target. Missing Bluetooth, system volume or power-saving
capabilities must not prevent ordinary playback.

## Main components

| Component | Responsibility |
| --- | --- |
| `Application` | Navigation, collection state, playback actions and progress |
| `NativeFileSystem` / scanner | Folder hierarchy, metadata, cover extraction and cache validation |
| `Mpg123SdlPlayer` | Incremental MP3 decoding into a bounded SDL audio queue |
| `SdlRenderer` | Window, fonts, CoverFlow, textures and screen drawing |
| `SdlInput` | Controller and keyboard events, including wake-only input |
| `LinuxAudioSystem` | System volume and paired Bluetooth device management |
| `LinuxBackgroundSession` | Independent audio helper and safe GUI takeover |
| `AudioDucker` / `PlaybackPower` | Game volume and temporary idle protection |

`SdlPlatform` composes the adapters and owns SDL initialization order. The
application loop uses named handlers in a deliberate sequence: later handlers
must observe screen changes made earlier in the same frame.

## Library and persistence

- Media paths are opaque to the core. The filesystem adapter handles navigation,
  storage boundaries and symlink-loop prevention.
- Named collections preserve nested folders. Leaf folders expose tracks; an MP3
  beside subfolders becomes a single-track leaf. Album names come from folders.
- Explicit cover files take precedence over embedded ID3 artwork. Invalid MP3s
  are skipped; metadata reads and duration estimates avoid full-file scans.
- Versioned per-collection caches store metadata and root fingerprints. Changed
  media invalidate their collection. Config and progress use temporary-file replacement.
- CoverFlow and list views share selection and hierarchy state. Progress is
  calculated from saved track positions and durations, rather than stored as a
  separate percentage. Missing roots remain configured; media are never deleted.
- Windows paths use UTF-8 filesystem conversion. The decoder uses a short-path
  fallback for narrow-character APIs; disabled short-name generation remains a caveat.

## Playback and platform services

Background playback starts a fresh process with `fork` followed immediately by
`exec`. Continuing inside an SDL-initialized fork previously caused deadlocks
and silent audio. The helper plays the remaining album, saves progress every
five seconds, and uses a PID/path lock. GUI takeover validates process identity
before stopping a helper, preventing stale PID files from terminating unrelated
processes. A forced takeover can lose the last few seconds of saved position.

Bluetooth handover pauses playback, copies the previous sink volume and mute
state to the destination, and resumes only on success. Failure leaves playback
paused. Pairing itself belongs to the firmware.

Knulli game-volume ducking runs independently of decoding. Power protection is
temporary and exists only while playing; a watcher cleans up after a crash.
See [audio lifecycle and limitations](coverflow-audio-update.md) and
[power-saving adapters](playback-power-saving.md).

## Rendering

Nine visible cover slots retain a front-facing selection and rotated neighbors.
A bounded cache holds 24 recent covers. One background worker decodes and
area-filters images; only the main thread uploads textures. New work prioritizes
the current selection rather than queuing obsolete navigation requests. Text
textures are cached separately (64 entries). Critically damped motion preserves
position and velocity across repeated taps and direction changes. Periodic
firmware status reads are deferred while navigating.
Artwork is area-filtered to at most 384 pixels, then linearly sampled. The
focused cover is 256 pixels (264 for a single album). SDL geometry support is
resolved at runtime, so an ARM binary built with older headers can use the
newer handheld renderer. Supported backends use a subdivided triangle mesh;
older backends retain projected strips. The centered
cover uses one filtered draw. There is no oval backdrop or floor reflection.

[Original design rationale](development/design-history.md) is retained as history;
its older implementation and platform descriptions are not the current reference.
