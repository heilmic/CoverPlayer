# CoverPlayer

![CoverPlayer CoverFlow](docs/screenshots/generated/coverplayer-coverflow.png)

CoverPlayer is a controller-first offline audio player for ARM64 retro
handhelds. Its primary targets are Knulli and muOS.

## Screenshots

| Now playing | Album list |
| --- | --- |
| ![Now playing](docs/screenshots/generated/05-jetzt-laeuft.png) | ![Album list](docs/screenshots/generated/03-albumliste.png) |

| Collections | Bluetooth |
| --- | --- |
| ![Collections](docs/screenshots/generated/02-sammlungen.png) | ![Bluetooth](docs/screenshots/generated/06-bluetooth.png) |

| Kids audio plays | International audiobooks |
| --- | --- |
| ![Die drei Fragezeichen Kids](docs/screenshots/generated/07-drei-fragezeichen-kids.png) | ![International audiobooks](docs/screenshots/generated/09-international-audiobooks.png) |

| Podcasts | Music |
| --- | --- |
| ![CheckPod](docs/screenshots/generated/08-checkpod.png) | ![Music CoverFlow](docs/screenshots/generated/10-music-coverflow.png) |

Additional screenshots show the album list and track list under
`docs/screenshots/generated`. These are illustrative views rendered by the
actual SDL interface with example library entries, not screenshots of a
shipped media library. The example screenshots are not part of the application
package. Downloaded full-resolution source artwork is not included in this
repository. Cover artwork visible in example screenshots belongs to its
respective rights holders; it is shown only to illustrate the interface.

The project is intentionally separate from AlbumPlayer. AlbumPlayer remains a
functional reference and must not be modified or replaced by CoverPlayer
development.

## Platform priority

1. Knulli package (`CoverPlayer-Knulli.zip`)
2. Native muOS application (`CoverPlayer.muxapp`)
3. PortMaster package after both native releases are stable
4. Nintendo Switch technical prototype at a later stage

ROCKNIX is currently outside the project scope.

## Architecture

The application core is C++17 and must not invoke Linux services, shell
commands, POSIX APIs, or firmware-specific paths. Filesystem access, audio
output, input, lifecycle events, and system integrations are provided through
interfaces under `include/coverplayer/platform`.

## Desktop smoke build

Install CMake, Ninja, a C++17 compiler, and an SDL2 CMake package, then run:

```sh
cmake --preset desktop-debug
cmake --build --preset desktop-debug
```

The executable is written to `build/desktop-debug`. Required Windows runtime
DLLs and the bundled font are copied next to it automatically.

## Library and playback

Pass an MP3 path as the first command-line argument to start the incremental
libmpg123/SDL2 playback prototype:

```powershell
& '.\build\desktop-debug\coverplayer.exe' 'D:\path\to\audiobook.mp3'
```

Alternatively, start CoverPlayer normally and drag an MP3 file onto its
window. A controller is optional for desktop testing.

CoverPlayer scans MP3 files recursively. Multiple folders can be registered as
independently named collections, for example Music, Podcasts, radio plays, or
a particular artist. Press Y on the collection screen, choose `Neue Sammlung`,
select a folder, assign its type, and confirm or edit its prefilled name using
the on-screen keyboard. In the manager, A edits type and name, Y changes the
folder, X removes the collection after confirmation, and L1/R1 changes its
order. Removing a collection never removes media. Duplicate folders are
rejected, and disconnected or moved folders remain visible with `[FEHLT]` so
their configuration can be repaired.

The folder browser is restricted by the platform launcher (`/userdata` on
Knulli and the selected storage root on muOS). Paths are normalized before
duplicate checks, symlink directory loops are ignored, and an unreachable
folder cannot be selected accidentally.

The library index is cached separately for every collection. A deterministic
fingerprint of relevant files automatically invalidates stale cache entries;
an explicit refresh remains available. A progress screen is shown during real
rescans, and unreadable or invalid MP3 files are skipped and written to the
application log instead of aborting the complete library scan.

Covers named `cover.jpg`, `cover.jpeg`, `cover.png`, `folder.jpg`, or
`folder.png` are detected automatically. A collection first uses artwork in
its own root folder, then the first available album cover, and finally the
built-in placeholder. Album folders prefer those external cover files and
otherwise extract embedded ID3 APIC artwork into the application cache.
ID3v2 title, artist, album, and track number fields are used when available;
ID3v1 text fields remain a fallback. Filename and folder-name fallbacks keep
untagged libraries usable. An album's displayed name always comes from its
folder name, never from the ID3 album tag, so it matches how the user
organized their files even when tags disagree or vary between tracks.

Opening a collection enters the handheld CoverFlow. Media folders remain a
recursive hierarchy at any depth: CoverFlow or list view is used while a folder
contains further media folders, and the track list appears only at a leaf.
Loose MP3 files beside subfolders become playable one-track entries at that
same level. A opens the selected entry, B moves up one level, and Y switches
between CoverFlow and a compact list without changing selection or resume.
Cover images are downscaled while loading so very large artwork cannot consume
unbounded graphics memory on a handheld. The centered cover in CoverFlow gets
a soft accent-colored glow and sits in front of a subtle ambient spotlight, so
the current selection reads clearly even before the title below it is read.

When exactly one available collection contains media, startup goes directly
to its CoverFlow/list browser. B still reaches the collection screen so the
collection manager remains accessible.

Screen titles use a larger title font, distinct from list rows and footer
hints, so screens have a clear visual hierarchy instead of one uniform text
size everywhere. Bluetooth activity, playback state, and the progress bar use
a separate accent color from the list-selection cursor, so "this is on" is
never visually confused with "this is selected." Every scrollable list
(collections, collection manager, Bluetooth devices, tracks, folders) shares
one rendering path with a consistent row style. Truncated titles cut on
UTF-8 character boundaries, so German umlauts and other multi-byte
characters at the end of a shortened name render correctly instead of
corrupting into replacement glyphs.

Controller controls:

- A: select or pause/resume
- B: back; on the collection list it deliberately does not exit
- D-pad Left/Right: seek 10 seconds in the player
- D-pad Up/Down: previous/next track in the player
- L2/R2: seek 30 seconds
- L1/R1: seek 30 seconds in the player or previous/next item in menus
- X: jump to the next bookmark in the player; refresh collection screens
- Y: add a bookmark in the player; open/select collection folders
- Y in a media browser: switch between CoverFlow and list
- Collection manager: A edits, Y changes path, X deletes, L1/R1 reorders
- Start briefly: cycle the sleep timer in the player; hold for two seconds to
  background playback (Knulli) or exit if nothing is playing
- Select: open or close the modal help on every screen; B also closes it
- X while help is open: open Bluetooth management when supported
- Start+Select: exit immediately, regardless of playback state

On Knulli, holding Start for two seconds while a track is actually playing
closes CoverPlayer as usual but hands the whole album's remaining tracks to
an independent background process first, so EmulationStation regains the
screen immediately and playback keeps going - through its own menu, a
game, an emulator, whatever is opened next, and on into the next chapter
once the current one ends, exactly like the foreground player would - with
no window and no tie to CoverPlayer's own session. If the track is only
open but paused, there is
nothing to keep playing in the background, so the same hold just performs
a plain exit instead - the background helper always starts playing
immediately and has no way to stay paused, so backgrounding a paused
track would otherwise resume it as an unintended side effect of a
gesture that never pressed play. Reopening the port takes control back
automatically: the background process is stopped and the same track
resumes inside CoverPlayer at its current position, so at most one
playback path is ever active - opening the port never leaves a second,
unreachable copy playing behind it. Start+Select always performs a full,
immediate exit regardless of what is playing. This is a Knulli/Linux-only
capability; the desktop build's two-second hold
always exits, exactly as before.

The Now Playing screen shows remaining battery charge next to the volume
readout, read from the kernel's power-supply status. It stays hidden
whenever no battery is reachable there, which includes every desktop
build.

On Knulli, the background helper ramps other audio streams that are already
present when it starts down to 50% by default. A game launched afterward may
remain at normal volume. The current cleanup sets affected streams to a fixed
100%, not their previous level. This behavior is experimental; use RetroArch's
own game-audio gain if you need predictable balancing. The target percentage
can be overridden with `COVERPLAYER_DUCK_PERCENT`. A safer per-stream design is
tracked in [TODO.md](TODO.md).

The decoder only keeps a bounded half-second PCM queue. It does not load or
scan the complete compressed MP3 before playback.

Track duration comes from Xing/VBRI frame metadata or a first-frame bitrate
estimate. At most 256 KiB are inspected, and the application never calls
`mpg123_length()` or scans an entire audiobook before playback.

Opening a file searches up to 8 MiB for the first valid MPEG frame before
giving up, well past mpg123's much smaller 64 KiB default - some real
files (e.g. ones saved by browser video-downloader extensions) have
non-standard leading data that pushes the actual audio past that default
and would otherwise fail to open at all, even though the audio itself
plays fine once found.

Playback position is saved every five seconds and on lifecycle transitions.
Resume, automatic next-track playback, completion state, bookmarks, configured
collections, and the library cache live in SDL's per-user application-data
directory, outside executable and media directories.

Audiobook and radio play collections show real listening progress on every
collection and folder screen - a percentage of total elapsed time across all
their tracks, so a single 70-minute file is just as informative as twenty
5-minute episodes, unlike a per-file done/not-done count. Progress for a
collection or folder always reflects its current listen: replaying an
already-finished title starts the percentage climbing from its actual
position again rather than staying stuck at 100%. Music, podcast, and other
collection types are browsed non-linearly, so no completion indicator is
shown for them at all.

On Knulli, the Bluetooth page lists already paired audio devices and can
connect or disconnect them with A. X turns Bluetooth on or off, Y refreshes
the state, and B returns to the previous screen. New devices are still paired
through Knulli's system menu. A green Bluetooth symbol appears in the header
only while a Bluetooth sink is the active audio output. Bluetooth management
is an optional platform capability and remains disabled on muOS and future
Switch builds until a native adapter is provided.

Bluetooth output changes use a safety handover. Playback pauses while the
current output's volume and mute state are copied to the destination sink. It
resumes only after the handover succeeds; otherwise the output is fail-safe
muted and playback remains paused.

## ARM64 cross-build

With Docker Desktop running, build the conservative Debian 11 ARM64 binary:

```powershell
.\scripts\Build-Arm64.ps1
```

The output is written to `build/arm64-release/coverplayer`. This binary is the
common input for the later Knulli and muOS staging packages; firmware-specific
paths and launch behavior remain outside the executable core.

## Release packages

Build the Knulli production and test packages plus the muOS candidate with:

```powershell
.\scripts\Build-Packages.ps1
```

This produces the distributable `build/release/CoverPlayer-Knulli.zip`, the
parallel `build/release/CoverPlayer-Knulli-Test.zip`, and
`build/release/CoverPlayer.muxapp`. Packaging recursively collects the ARM64
runtime libraries, sets Unix executable mode bits, creates SHA-256 manifests,
and verifies every archived payload before reporting success. This command
does not connect to or modify a handheld.

See the [package installation notes](packaging/README.md). Source publication
does not mean the current package candidates have passed device-release checks.

Each archive's unzipped content is also left alongside it -
`build/release/CoverPlayer-Knulli/`, `build/release/CoverPlayer-Knulli-Test/`,
and `build/release/CoverPlayer-muOS/` - for deploying with a plain file
transfer (WinSCP, `scp -r`, a mounted SD card) without needing to unzip on
the device first.
