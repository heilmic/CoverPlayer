#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace coverplayer::platform {

// Lets CoverPlayer keep playing audio after EmulationStation regains the
// screen (the user backed out, or launched a game) instead of stopping when
// the ES-launched port process exits. Linux/Knulli only - a no-op returning
// false/nullopt on other platforms.
//
// A raw fork() of the running, already-SDL-initialized process does not
// work for this: fork() only duplicates the calling thread, so SDL's own
// internal threads (the audio device's playback thread, the video
// backend's internal state) simply do not exist in the forked child,
// leading to silence or a deadlock depending on what the child then tries
// to do with that inherited-but-broken state. See docs/architecture.md.
//
// Instead, spawnBackgroundAudio() forks and immediately execs a fresh
// invocation of this same binary in a minimal "--background-audio
// <startIndex> <path0> <path1> ..." mode (see main.cpp): exec() replaces
// the entire process image before
// anything ever touches the fork-broken SDL state, so the new process
// starts genuinely fresh, exactly like a normal program launch. The
// original GUI process does not continue running at all afterward - it
// performs a completely normal, clean shutdown, and the spawned helper is
// then a fully independent process: it keeps playing whatever else is
// launched next (EmulationStation's menu, a game, an emulator), with no
// window and no dependency on CoverPlayer's own lifecycle.
//
// A small PID+path lock file tracks the one helper allowed to exist at a
// time, so relaunching CoverPlayer while one is already running can take
// control back instead of ending up with two independent helpers playing
// at once.
class LinuxBackgroundSession {
public:
    // Fire-and-forget: returns immediately without waiting for the helper.
    // Stops any already-running helper first, so at most one ever exists.
    // trackPaths is the whole album in playback order and startIndex the
    // track to begin at, so the helper can advance to the next track itself
    // once the current one finishes instead of exiting. Returns false if
    // backgrounding is unsupported on this platform, startIndex is out of
    // range, or the fork/exec itself failed to even start.
    static bool spawnBackgroundAudio(const std::vector<std::string>& trackPaths, std::size_t startIndex);

    // Call once at normal GUI startup, before anything else. If a
    // background helper is currently running, stops it and returns the
    // media path it was playing, so the GUI can resume that track itself
    // instead of leaving it running detached and unreachable. Returns
    // nullopt if no helper is running (the common case).
    static std::optional<std::string> takeOverIfRunning();

    // Called by the helper process itself (main.cpp's --background-audio
    // mode) once it has successfully opened mediaPath - including every
    // time it advances to a new track within the album - so the checks
    // above always see the track actually playing right now.
    static void recordRunning(const std::string& mediaPath);

    // A stale lock can name a PID since reused by an unrelated process.
    // Only signal it when /proc/<pid>/cmdline identifies our audio helper.
    static bool isBackgroundHelperCommandLine(const std::string& commandLine);

};

} // namespace coverplayer::platform
