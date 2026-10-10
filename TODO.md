# Future work

Knulli reliability and cover-first offline playback take priority. This is a
backlog, not a commitment to implement every feature.

## Reliability follow-ups

- Invalidate the in-memory artwork cache on rescan when a cover is replaced
  under the same filename; restarting currently reloads it.
- Investigate intermittent Bluetooth audio stutter reported during dimmed playback.
  The inspected session was not muted and showed no audio-server underruns;
  the cause remains unconfirmed.
- Measure the updated CoverFlow on H700 hardware during rapid navigation and
  direction changes. Test large artwork and long collections.
- Expand audio lifecycle checks: Bluetooth reconnect, manual stream volume,
  mono/stereo, muted streams, helper/guardian failure and audio-server restart.
  [Recovery limits](docs/coverflow-audio-update.md#restoration-and-limitations).
- Validate the capability-gated muOS power adapter on real muOS hardware;
  its isolated fixture tests do not establish firmware compatibility.

## Optional improvements

- Extend [Switch alpha](docs/switch.md) hardware validation to Lite, docking,
  HOME/sleep transitions and long sessions; evaluate an audio sysmodule with
  Ultrahand/Tesla controls later. Basic playback and collections work on Switch.

- Continue-listening row using existing progress, with last-played ordering and
  clear handling of completed or unavailable media.
- Extend browsing during playback beyond the current album. Within-album track
  browsing and automatic next-track playback are implemented.
- Page jumps and accelerated seeking if device testing demonstrates a need.
- Offline podcast downloads: RSS browsing and manual episode downloads first.
  Define MP3 format compatibility, use temporary downloads, expose only complete
  files and preserve progress. Scheduling and automatic deletion are later work.

Web radio and Jellyfin are out of scope. Switch/PortMaster ports and further
muOS-specific features must not delay Knulli reliability. Native demo galleries
are available; larger marketing images are not a current priority.
