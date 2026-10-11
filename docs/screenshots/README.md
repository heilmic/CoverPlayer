# Demo screenshots and GIFs

[English gallery](generated/en/README.md) · [German gallery](generated/README.md)

The published galleries come from the native SDL renderer at 640 × 480. The README GIF uses
music covers. Audiobook examples use Sherlock Holmes BBC productions; progress
and chapter data are fictional. No media library is bundled.

## Regenerate

Supply the files listed in [cover sources](demo-cover-sources.md) under the
Git-ignored `docs/screenshots/source-covers` directory, then run from the repo:

```powershell
./scripts/Generate-DemoScreenshots.ps1 -Language en -IncludeGif
./scripts/Generate-DemoScreenshots.ps1 -Language de -IncludeGif
```

Requires the Windows desktop build tools and Python with Pillow. Each command
renders 14 screenshots and an optional 7.6-second looping GIF. Use
`-OutputDirectory <path>` for a preview outside the published gallery.
Single-album and edge previews are under `build/screenshots/demo-raw-*/layout-review`.

Review the result after renderer changes. The GIF encoder checks size, duration
and loop settings. Artwork is area-filtered and linearly sampled, but small
lettering remains limited by the handheld's native resolution. Original artwork
is excluded from Git and packages; displayed cover art belongs to its rights holders.

## Layout previews

For direct previews, run `coverplayer_screenshot_tool <output-dir> <source-cover-dir> en`
from the repository root after the desktop build. This includes the collection
manager; the gallery script selects only the 14 published screenshots.
Set `COVERPLAYER_UI_WIDTH=720` to review the wider handheld layout; the tool
defaults to 640 when the variable is unset. Output paths should stay under `build/`.

Build the separate Switch preview with tests disabled:

```sh
cmake -S . -B build/switch-preview -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCOVERPLAYER_BUILD_SWITCH_PREVIEW=ON -DCOVERPLAYER_BUILD_TESTS=OFF
cmake --build build/switch-preview --target coverplayer_switch_preview
build/switch-preview/coverplayer_switch_preview build/screenshots/switch-preview docs/screenshots/source-covers en
```

On Windows the executables have an `.exe` suffix and require the MSYS2 UCRT64
runtime on PATH. The Switch preview always renders 1280 × 720 and also includes
name/folder editors and a long collection list. Check CoverFlow, tracks, player,
collection management and help in both languages. These software-rendered
previews verify layout, not device performance or hardware lifecycle behavior.
