# CoverPlayer 0.1.0

Offline audio player for Knulli and muOS handhelds. Add folders as named
collections for audiobooks, radio plays, podcasts, or music. No audio files
or cover images are included in this package.

## Install

- Knulli: extract `CoverPlayer-Knulli.zip` into the root of the Knulli share
  or SD card so that `roms/ports/CoverPlayer.sh` and
  `roms/ports/CoverPlayer/` are both present. Refresh the ports list.
- Knulli test port: extract `CoverPlayer-Knulli-Test.zip` in the same way;
  it installs beside the production port under a separate name.
- muOS: install `CoverPlayer.muxapp` using the muOS application installer.

The application stores collections, bookmarks, progress, and library cache
outside its installation folder. Replacing the application does not require
deleting those files or any media directory.

## Controls

- A opens the selected item or pauses/resumes playback.
- B goes back one screen; it does not exit from the collection screen.
- Select opens help. B or Select closes help.
- The bottom bar always shows Select = Help. While help is open, Y switches
  between German and English; the selection is saved.
- X scans the library again on collection and CoverFlow/list screens.
  The scan refreshes every configured collection.
- Y switches between CoverFlow and list where folders are shown.
- On the player, Left/Right seek 10 seconds; L1/R1 seek 30 seconds;
  Up/Down select the previous or next track.
- On the player, a short START release cycles the sleep timer. Holding START
  for two seconds exits the interface and keeps a playing album running in
  the background on Knulli. START+SELECT exits immediately.

The background playback helper on Knulli can continue while other software
is open. A game launched afterward may need its own volume lowered in
RetroArch. Reopening CoverPlayer takes playback back into the interface.

Bluetooth management on Knulli lists already paired devices. Pair new
headphones in Knulli's system menu first. Bluetooth management is currently
unavailable on muOS.

## Troubleshooting

Knulli writes the app log to `/userdata/system/logs/coverplayer.log`.
Keep the media files and saved application data when replacing a package.

CoverPlayer is licensed under Apache-2.0; see `LICENSE`. Bundled third-party
components and notices are listed in `THIRD_PARTY_NOTICES.md`.
