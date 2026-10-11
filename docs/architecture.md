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
| `SdlRenderer` | Window, fonts, shared CoverFlow, texture cache and drawing primitives |
| Handheld / Switch renderers | Separate screen layouts, using shared UI content and a compile-time profile |
| `SdlInput` | Controller and keyboard events, including wake-only input |
| `LinuxAudioSystem` | System volume and paired Bluetooth device management |
| `LinuxBackgroundSession` | Independent audio helper and safe GUI takeover |
| `AudioDucker` / `PlaybackPower` | Game volume and temporary idle protection |

`SdlPlatform` and `SwitchPlatform` compose their respective adapters and own
SDL initialization order. Linux does not compile the Switch screen layouts or
link Switch libraries. The optional desktop Switch preview selects only the
Switch layout, without the device adapter. The
application loop uses named handlers in a deliberate sequence: later handlers
must observe screen changes made earlier in the same frame.

## Library and persistence

- Media paths are opaque to the core. The filesystem adapter handles navigation,
  storage boundaries and symlink-loop prevention.
- Named collections preserve nested folders. Leaf folders expose tracks; an MP3
  beside subfolders becomes a single-track leaf. Album names come from folders.
- Album browsing and CoverFlow prefer explicit folder cover files over embedded
  ID3 artwork. The player prefers embedded artwork from the playing track,
  falling back to the album cover. Invalid MP3s are skipped; metadata reads and
  duration estimates avoid full-file scans.
- Versioned per-collection caches store metadata and root fingerprints. Changed
  media invalidate their collection. Config and progress use temporary-file replacement.
- CoverFlow and list views share selection and hierarchy state. Progress is
  calculated from saved track positions and durations, rather than stored as a
  separate percentage. Missing roots remain configured; media are never deleted.
- Windows paths use UTF-8 filesystem conversion. The decoder uses a short-path
  fallback for narrow-character APIs; disabled short-name generation remains a caveat.

## Playback and platform services

The browsing row (`trackIndex_`) and playback identity (`currentMediaId_`, the
media path) have separate responsibilities. `openTrack(album, index)` saves the
outgoing media's progress and opens the requested track without changing the
row. Callers explicitly update selection when appropriate. Playback, player
metadata and background handoff resolve the playing index from the media ID;
no pointer into rescan-replaced library storage is retained. Auto-next follows
playback while preserving a deliberately different row. Returning from the
player to a multi-track list keeps playing; leaving that list pauses playback.
A one-track album returns directly to the album browser and pauses.

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

Concrete screens live in `handheld_renderer.cpp` and `switch_renderer.cpp`.
`ui_content.hpp` shares section labels, hints, help rows and text helpers;
`ui_profile.hpp` selects size, font, cover-cache and perspective values at
compile time. Platform-specific button labels and services remain distinct.

Linux uses a 640 or 720 × 480 logical canvas, 18/26 px fonts and a VOL indicator.
Switch uses native 1280 × 720, 28/40 px fonts and no VOL indicator, with Minus/Plus
help and sleep/quit controls instead of Select/Start. Nine visible cover slots
retain a front-facing selection and rotated neighbors.
A bounded cache holds 24 recent covers on Linux and 16 on Switch. One background worker decodes and
area-filters images; only the main thread uploads textures. New work prioritizes
the current selection rather than queuing obsolete navigation requests. Text
textures are cached separately (64 entries). Critically damped motion preserves
position and velocity across repeated taps and direction changes. Periodic
firmware status reads are deferred while navigating.
Artwork is area-filtered to at most 384 pixels on Linux or 512 on Switch, then
linearly sampled. The focused cover is 256 pixels (264 for a single album) on
Linux and 400 pixels (416 for a single album) on Switch. SDL geometry support is
resolved at runtime on desktop/Linux, so an ARM binary built with older headers can use the
newer handheld renderer. Supported backends use a subdivided triangle mesh;
older backends retain projected strips. The centered
cover uses one filtered draw. Switch links the geometry API from its static SDL.
There is no oval backdrop or floor reflection.

## Build metadata

The root `VERSION` file contains the base version on its first line and the
Switch alpha number on its second line. CMake derives the project/NACP versions
and a generated start-log header; packaging derives filenames and `BUILD.txt`.
The packager independently reads the version embedded in the built NRO's NACP
and rejects a mismatch. Its source/relink archive includes `VERSION` and `cmake/`
and builds independently with tests disabled. Build and staging outputs remain
separate for desktop, ARM64 and Switch; each packager preserves the other's
release artifacts.

[Original design rationale](development/design-history.md) is retained as history;
its older implementation and platform descriptions are not the current reference.
