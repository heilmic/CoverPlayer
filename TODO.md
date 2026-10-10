# Future work

Knulli reliability and cover-first offline playback take priority. This is a
backlog, not a commitment to implement every feature.

## Before the next release

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
- Choose a version and exercise the real release publication workflow.
  Test deployment, backup and verification already work; a push is not a release.

## Optional improvements

- Continue-listening row using existing progress, with last-played ordering and
  clear handling of completed or unavailable media.
- Browse while listening, keeping the playing album independent of selection
  and preserving auto-next. Review button conflicts and return-to-player behavior.
- Page jumps and accelerated seeking if device testing demonstrates a need.
- Offline podcast downloads: RSS browsing and manual episode downloads first.
  Define MP3 format compatibility, use temporary downloads, expose only complete
  files and preserve progress. Scheduling and automatic deletion are later work.

Web radio and Jellyfin are out of scope. Switch/PortMaster ports and further
muOS-specific features must not delay Knulli reliability. Native demo galleries
are available; larger marketing images are not a current priority.
