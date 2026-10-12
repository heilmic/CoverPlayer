# CoverPlayer for Nintendo Switch — 1.0.1 alpha 6

**Experimental MP3 player for Switch / Switch Lite.**

- CoverFlow, collections, resume, bookmarks, auto-next and sleep timer.
- 1280 × 720 interface, larger text and per-track artwork.
- Switch collection creation/playback confirmed in alpha 2; alpha 5, Lite and docking checks pending.

## Install

1. Extract the ZIP to the root of your homebrew-enabled Switch's SD card.
2. Put MP3 folders in `/media/`, or choose another SD folder in the app.
3. Launch **CoverPlayer** in hbmenu. Prefer full application mode (hold R while launching an installed title); applet memory is limited.

- App: `/switch/CoverPlayer/CoverPlayer.nro` (font embedded).
- Settings, progress, bookmarks, cache and log: `/config/coverplayer/`.
- Update: replace the NRO. Older caches rebuild; collections and progress remain.

## Controls

| Button | Action |
| --- | --- |
| D-pad / left stick | Navigate; hold for fast scrolling |
| A / B | Open or play / back |
| Y / X | Context action; see on-screen help |
| Minus | Help; Y changes German / English |
| Plus (tap, player) | Sleep timer |
| Plus (hold 2 seconds, player) | Save and quit |
| Plus + Minus (help closed) | Save and quit |
| Left / right (player) | Seek 10 seconds |
| L / R (player) | Seek 30 seconds |
| Up / down (player) | Previous / next track |

- **B** returns to the track list without pausing; leaving the album pauses and saves. One-track albums pause immediately on return.
- Selecting the playing track does not restart it; browsing leaves playback unchanged.
- Player artwork: embedded track cover → album cover → placeholder.
- Volume, headphones and Bluetooth: use the Switch system controls.

## Scope and first device test

- Auto-sleep inhibited during active playback; HOME/focus loss pauses and saves, return resumes. Hardware verification pending.
- No custom dimming, Linux background helpers or audio sysmodule.
- Applet-mode background playback reported; compatibility across modes/games remains unverified.
- Test: SD browsing/non-ASCII names, playback, covers, held controls, save/restart, HOME, sleep/wake and headphones.
- Original Switch: Joy-Con, Pro Controller, docking. Lite: built-in controls and readability.
- Startup problems: `/config/coverplayer/coverplayer.log`.

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
