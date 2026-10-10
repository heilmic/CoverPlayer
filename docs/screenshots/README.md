# Demo screenshots and GIFs

[English gallery](generated/en/README.md) · [German gallery](generated/README.md)

All images come from the native SDL renderer at 640 × 480. The README GIF uses
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
