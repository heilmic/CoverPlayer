> Historical design record. Some platform and implementation details have changed.
> See the [current architecture](../architecture.md).

# Architecture decision: portable core with capability adapters

Status: accepted through the multi-collection library milestone.

Knulli and muOS are the primary platforms. Potential PortMaster and Nintendo
Switch builds must not delay or complicate their native releases.

The core treats media locations as opaque identifiers and never parses their
platform syntax. Enumeration and parent navigation are restricted to the
`FileSystem` adapter; platform defaults remain in launchers.

Optional integrations are expressed through capabilities. A platform that
does not provide Bluetooth or system-volume control reports the capability as
unavailable; normal playback continues without an error.

Knulli Bluetooth management is implemented only in the SDL platform adapter
and enabled explicitly by its launcher. It uses Knulli's `knulli-bluetooth`
service for power and connection changes, `bluetoothctl` to filter already
paired audio devices, and PulseAudio's default sink to identify the active
headphones. Device addresses are validated before an action is dispatched.
The portable application core sees only Bluetooth state and capability
interfaces; muOS and Switch therefore remain unaffected.

Knulli enables the system-volume capability in its launcher. The SDL platform
adapter reads and adjusts the active PulseAudio sink, while the portable
application core only uses the `Platform` interface. Other targets fall back
to decoder-side volume without Linux commands in the core.

Bluetooth output changes use a safety handover: the core pauses playback, the
Knulli adapter captures volume plus mute from the current sink, initially mutes
the destination, copies the level, and restores the captured mute state. The
core resumes only after that succeeds. If the sink cannot be identified or
configured, the default output is muted and playback stays paused.

MP3 audio decodes incrementally with a pinned libmpg123 build and feeds a
bounded PCM queue to an SDL2 output adapter. Opening a track never invokes
`mpg123_length()` or triggers a full-file scan.

Persistence is exposed to the core through `ProgressStore`. The desktop
adapter resolves SDL's per-user preference directory and writes a versioned
state file through a temporary file before replacement. Firmware adapters will
provide their own persistent locations without exposing those paths to the
core.

Configured media roots form the top-level collections. Scanner results preserve
the media-folder hierarchy recursively instead of flattening it. Every non-leaf
level feeds CoverFlow/list presentation; only leaf nodes expose a track list.
An MP3 beside child folders becomes a one-track leaf at the same level. This
supports artist/album, audiobook-series/episode, and mixed layouts while
preserving covers, natural order, progress, resume, and leaf-local auto-next.

Each configured root has a user-visible name, a semantic media type, an opaque
platform path, and a stable position. Configuration writes are atomic and the
manager never deletes media. Missing roots stay configured and are surfaced to
the UI for repair. Duplicate paths are not accepted.

The filesystem adapter owns directory availability checks, cache fingerprints,
and media metadata extraction. The native adapter reads ID3v2.3/v2.4 title,
artist, album, track number, and APIC artwork with ID3v1 fallbacks. The scanner
uses explicit cover files before embedded artwork, ignores invalid MP3 files,
and reports bounded progress through a callback. None of these operations add
platform paths or system calls to the portable application core.

Library caches are versioned and isolated per collection root. They contain the
root fingerprint plus the recursive node/track metadata, and are replaced through
a temporary file. A changed MP3 or cover invalidates only its collection; a
valid cache avoids metadata parsing and cover extraction on subsequent starts.

Each track's duration is estimated during the same scan (the same bounded
Xing/VBRI/bitrate read the player itself uses, `audio::estimateMp3DurationSeconds`
- library scanning is otherwise independent of the audio module, but this one
function has no SDL/mpg123 runtime dependency of its own) and cached alongside
the other track metadata. This is what lets collection/folder screens show a
real elapsed-time percentage for audiobook/radioplay collections instead of a
per-file done/not-done count, which is uninformative for a single long file.
The percentage is derived each time from `ProgressStore` position/completed
data, never stored as its own running total, so replaying an already-finished
title is reflected immediately rather than needing an explicit reset.

Media presentation is independent of library selection state. CoverFlow and
list mode share the same source, navigation path, node, and track indices, so
switching views cannot alter hierarchy position, resume, or playback behavior.

The collection picker is skipped at startup only when one available,
non-empty collection exists. It remains reachable through normal back
navigation for configuration and rescans.

Folder browsing has an adapter-defined boundary. Launchers constrain it to a
media-safe storage root, while canonical path comparison blocks traversal and
symlink escapes. Scanner traversal also remembers normalized directories to
avoid loops caused by linked folders.

An album's displayed name is always its folder name, never the ID3 album tag.
The tag is metadata about the tracks, not a naming authority, and can
disagree with how the user actually organized their files (mixed tags inside
one folder, missing tags, compilation folders). Non-leaf group names were
already folder-derived; leaf albums now follow the same rule.

## Architecture decision: SDL platform adapter split into single-purpose collaborators

Status: accepted after the SdlPlatform God-object review.

`SdlPlatform` used to own SDL window/renderer/font lifecycle, controller and
keyboard input mapping, and every Linux Bluetooth/PulseAudio shell
integration in one class. It now composes three collaborators and forwards
each `Platform` call to whichever one owns that concern:

- `LinuxAudioSystem` owns every `pactl`/`bluetoothctl`/`knulli-bluetooth`
  shell invocation. No other class runs a shell command.
- `SdlRenderer` owns the window, renderer, both fonts, and every draw call.
  It knows nothing about controllers or shell integrations; it turns a
  `ViewModel` plus a handful of status setters into pixels.
- `SdlInput` owns the game controller lifecycle and the keyboard/controller
  event-to-`InputActions` mapping, parameterized by the current `Screen` as a
  plain argument rather than shared mutable state.

`SdlPlatform` itself is reduced to construction order (a small
`SdlSubsystemGuard` brackets `SDL_Init`/`SDL_Quit` around the three
collaborators, so a throwing collaborator constructor still leaves SDL
correctly torn down) plus one-line forwarding methods. This is a mechanical
split along already-clean boundaries, not a redesign: behavior is unchanged
and verified by the existing desktop build and test suite.

## Architecture decision: flattened per-screen dispatch in Application::run()

Status: accepted alongside the SdlPlatform split.

`Application::run()` used to be a single ~170-line loop body with one long
`if`/`else if` chain keyed on `screen_`, backed by ~25 loosely related member
variables. It is now a short, fixed sequence of named handler calls
(`handleCollectionManagementInput`, `handleRescanRequest`,
`handleAcceptAndBack`, `updatePlayback`, `publishCurrentView`, ...), each a
direct extraction of what used to be one part of that chain. The call order
in `run()` is deliberately unchanged from the original statement order,
because several handlers mutate `screen_` mid-frame and later handlers must
observe that updated value - the split is a naming and organization change,
not a logic change.

Only `handleAcceptAndBack` and `publishCurrentView` dispatch through a
`switch (screen_)`, since at most one `screen_` value is active per frame and
splitting them per screen cannot reorder anything relative to the original
chain. Transient state used by exactly one flow (`CollectionEditorState`,
`FolderBrowserState`, `BluetoothUiState`) is grouped into small structs
instead of loose members with independent lifetimes.

## Architecture decision: background playback across EmulationStation launches

Status: accepted after two rejected designs; Linux/Knulli-only.

EmulationStation launches CoverPlayer, like any port or emulator, as a
blocking foreground child process: its launcher script runs the binary and
waits for it to exit before redrawing its own UI. Without intervention,
leaving CoverPlayer to browse EmulationStation or start a game means the
process - and playback - stops.

The first two designs tried here both forked the already-running,
already-SDL-initialized GUI process and kept executing in the child
afterward - first tearing the window down post-fork (deadlocked: SDL's own
internal threads, e.g. the video backend's event-pump thread, do not exist
in a forked child, since `fork()` only duplicates the calling thread, so
destroying SDL objects there could block forever on a lock a vanished
thread still held), then reopening just the audio device post-fork
(silent: the audio subsystem's own internal playback thread, created once
back when `SDL_Init(AUDIO)` ran before the fork, is equally gone, and even
resetting the whole SDL audio subsystem in the child did not reliably
avoid a hang). Both failures were reproduced on real hardware, not just
suspected; an SDL upstream issue confirms fork() after `SDL_Init()` is not
supported for exactly this reason. A raw `fork()`-and-continue of a
live SDL process is not viable here.

The accepted design never continues the GUI process after forking at all.
`Platform::enterBackgroundPlayback(mediaPath)` spawns a **fresh process
image** for the background player: `LinuxBackgroundSession::
spawnBackgroundAudio()` (`src/platform/sdl/linux_background_session.cpp`)
forks and immediately `execl()`s `/proc/self/exe` again as
`coverplayer --background-audio <path>`. `exec()` replaces the entire
process image before anything ever touches the fork-duplicated (and
potentially broken) SDL state, so the new process starts genuinely fresh -
indistinguishable from a normal launch. `main.cpp` recognizes that mode
and runs a small, dedicated loop with no window, no controller, and no
library scanning: it opens `mediaPath`, resumes from the position
`Application::handleBackgroundRequest()` just saved via the normal
`ProgressStore`, and calls `AudioPlayer::update()` on a timer until the
track finishes, saving progress every five seconds.

Holding Start alone for two seconds while a track is open triggers this
(`Application::handleBackgroundRequest`); the GUI process then performs a
completely ordinary shutdown regardless of whether spawning succeeded,
exactly like the plain quit this gesture always performed. Start+Select
stays an unconditional immediate quit regardless of playback state. With
nothing open, the two-second hold is still just a plain quit.

The spawned helper is a fully independent OS process once running: it has
no relationship to CoverPlayer's own window/session lifecycle, so it keeps
playing regardless of what launches next - EmulationStation's own menu, a
game, an emulator - which is the actual goal (not just "survives returning
to the menu").

At most one helper is allowed to exist at a time, tracked through a small
PID+path lock file (`background-audio-v1.txt`, next to the other per-user
state files; see `krupkat/podcaster`'s separate-daemon-plus-thin-client
design, which independently arrives at the same "never let the GUI's own
process lifecycle own playback" principle via a persistent daemon and a
gRPC control channel - deliberately not adopted here, since a one-shot
handoff with no live control protocol is enough for this feature's scope).
The helper writes this lock file itself once it has successfully opened
its file (`LinuxBackgroundSession::recordRunning`, called from
`runBackgroundAudio()` in `main.cpp`). `spawnBackgroundAudio()` stops any
existing helper before starting a new one, and normal GUI startup
(`main()`, before constructing `Application`) calls
`LinuxBackgroundSession::takeOverIfRunning()`: if a helper is running, it
is stopped and its path becomes the session's `initialMediaPath`, so
`Application::openMedia()` resumes it at the position the helper was
periodically saving - normal library-browsing startup is otherwise
unaffected. This is what actually prevents two helpers (or a helper and
the GUI) from ever playing the same or different tracks at once, which a
pure fire-and-forget handoff does not.

Stopping an existing helper (either case above) sends `SIGKILL`, not
`SIGTERM`: on-device testing found the helper did not reliably die from
`SIGTERM` (cause unconfirmed - something in its environment not honoring
it as a plain terminate), which manifested as two copies of the same
track playing, offset, with the GUI's own pause/play only reaching one of
them. `SIGKILL` cannot be caught, blocked, or ignored by the target for
any reason, removing that uncertainty; the helper has nothing worth
flushing that its own five-second `saveProgress()` has not already
persisted. The caller then polls the PID for up to a second to confirm it
is actually gone before proceeding, so a fast reopen can never end up
racing the old helper for the same file.

Whether a game's own audio gets mixed with the still-playing helper or
takes over the output entirely still depends on the audio backend in use
at that moment (PulseAudio mixes independent client streams by default;
an emulator that grabs an ALSA device exclusively would not) - the
application does not control that. What it does control, once mixed, is
the relative volume: `duckOtherAudio()`/`restoreOtherAudio()` ramp every
*other* PulseAudio sink input (matched by `application.process.binary` in
`pactl list sink-inputs`, which identifies "not this executable" without
this app having to tag its own stream) down to `COVERPLAYER_DUCK_PERCENT`
(default 50) while the helper plays, and back to 100% when it stops -
found necessary in practice because a game's own sound otherwise drowned
out a backgrounded audiobook. Both directions ramp over ~700ms in six
steps rather than jumping, and restoring always targets a fixed 100%
rather than a remembered prior value: simpler, and correct even if the
helper restoring it was itself just `SIGKILL`ed (`stopIfRunning` ramps
back up itself in that case, so a killed helper can never leave other
audio stuck quiet).

This entire mechanism is compiled out to unconditional no-ops on non-Linux
platforms (`#ifndef _WIN32` in `linux_background_session.cpp`); the desktop
Windows build's two-second-hold-to-quit behavior is unchanged.

## Architecture decision: UTF-8 paths through narrow-char file APIs on Windows

Status: accepted; Windows-desktop-only, no effect on the Linux/Knulli target.

Every path in this codebase is UTF-8, matching `NativeFileSystem`'s own use
of `std::filesystem::u8path`. A plain `std::ifstream(const std::string&)`
or a C API taking `const char*` (like `mpg123_open`) does not know that,
though: on Windows both eventually reach a narrow-char file API that
decodes the bytes using the current ANSI code page, not UTF-8. A filename
with any non-ASCII character (a German umlaut, for instance) then resolves
to the wrong bytes and the file is reported as not found, even though it
exists - reproduced with a real file (`... würde.mp3`) that failed to
play until this fix. Linux has no such distinction (`open()` treats a path
as opaque bytes, and UTF-8 is the practical convention there already), so
this is invisible on the actual target platform.

`estimateMp3DurationSeconds` (`src/audio/mp3_duration.cpp`) now opens
through `std::filesystem::u8path`, whose stream constructors correctly use
the wide-char file API on Windows. `Mpg123SdlPlayer::open` (`src/audio/
mpg123_sdl_player.cpp`) has no such overload to fall back on - it must
hand libmpg123 a `const char*` - so on Windows it first converts to the
file's 8.3 short path (`GetShortPathNameW`), which is ASCII by definition
for any file that exists, sidestepping the encoding question entirely
without needing mpg123's fd-based open (and the file-ownership questions
that would come with it). Falls back to the original path unchanged if
short-name generation is unavailable, which only matters on the (rare)
systems where it has been disabled at the volume level.
