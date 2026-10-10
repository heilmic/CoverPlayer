# CoverPlayer for Nintendo Switch — 1.0.0 alpha 5

Experimental player for Switch and Switch Lite. The user confirmed collection
creation and MP3 playback on a physical Switch in alpha 2. Switch Lite testing
is pending. Alpha 4 adds a full 16:9 interface with larger, directly rendered
text, 400-pixel covers and a wider player layout. The in-app VOL indicator
is hidden; use the system volume overlay.

Alpha 2 fixes collection saving, library caching and embedded-cover extraction:
the bundled SDL preference-path API is a stub, so the Switch port now uses an
explicit writable SD directory. Replace the existing NRO; saved data is retained.

Alpha 5 keeps playback running when returning to the album track list; leaving
the album pauses and saves. In the player, embedded track artwork takes priority
over the album cover. Older library caches rebuild once after this update;
collections and progress are retained. Up/Down changes tracks; the collection
manager shows X/Delete directly in its footer.

## Install

1. Extract the ZIP to the root of your homebrew-enabled Switch's SD card.
2. Put your MP3 folders in `/media/`, or select another SD folder inside the app.
3. Open **CoverPlayer** in hbmenu. Prefer full application mode (commonly holding
   R while starting an installed title); applet-mode memory is limited.

The executable is `/switch/CoverPlayer/CoverPlayer.nro`. The font is embedded.
Settings, playback positions, bookmarks, library cache and startup log are in
`/config/coverplayer/`. No firmware settings or system modules are installed.

## Controls

| Button | Action |
| --- | --- |
| D-pad / left stick | Navigate; hold for fast scrolling |
| A / B | Open or play / back |
| Y / X | Context action, explained by on-screen help |
| Minus | Help; Y changes German / English |
| Plus (tap, player screen) | Sleep timer |
| Plus (hold 2 seconds, player screen) | Save progress and quit |
| Plus + Minus (help closed) | Save progress and quit |
| Left / right (player screen) | Seek 10 seconds |
| L / R (player screen) | Seek 30 seconds |
| Up / down (player screen) | Previous / next track |

Nintendo A/B/X/Y labels are used. Volume buttons, headphones and Bluetooth
audio are handled by the Switch system. The UI fills the 16:9 display using
a 1280 × 720 canvas, with 28-point body text and 40-point headings. Docked
output scales uniformly; cover artwork stays square.

## Scope and first device test

CoverFlow, named collections, folder/tag covers, MP3 playback, resume,
bookmarks, automatic next track and sleep timer use the shared player code.
Knulli audio ducking, background processes, Bluetooth pairing and display
dimming helpers are omitted. Automatic sleep is inhibited only during active,
unpaused playback. HOME/focus loss pauses and saves; returning resumes.
Sleep/focus behavior still needs verification on hardware.

Please test launch, SD browsing (including non-ASCII filenames), playback,
cover animation, held controls, save/restart, HOME, sleep/wake and headphones.
On an original Switch, also test Joy-Con/Pro Controller and docking. On Lite,
test the built-in controls. If startup fails, check `/config/coverplayer/coverplayer.log`.

The user reports background playback in applet mode. The tested circumstances
are not yet fully characterized, so this is not a guarantee for all modes or
games. No background audio sysmodule is bundled. A future Ultrahand/Tesla
integration can use a separate audio sysmodule and a small IPC control overlay.
The sys-tune project demonstrates that architecture:
https://github.com/ppkantorski/sys-tune

## Licenses and rebuilding

`LICENSE`, `THIRD_PARTY_NOTICES.md` and `licenses/` contain applicable notices.
libmpg123 is statically linked under LGPL 2.1. `relink/` includes its original
source, Switch patch, recipe and all CoverPlayer source needed to rebuild and
relink it, together with the bundled font and icon. Modification and reverse
engineering for debugging modifications to the LGPL library are permitted.

Build with Docker from the extracted package directory:

```sh
docker run --rm -v "${PWD}:/package" -w /package devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282 bash /package/relink/rebuild.sh
```

The script extracts and patches mpg123 only if `relink/work/mpg123-1.31.3`
does not exist. You can edit that tree and rerun the same command; the NRO is
replaced with the result. It needs no firmware keys or Nintendo SDK.
The container installs Autotools from Debian when needed, requiring internet
access for that step.
