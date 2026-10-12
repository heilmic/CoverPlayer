# CoverPlayer website

Local German landing-page draft. Plain HTML, CSS and JavaScript; no build step, external fonts or runtime dependencies.

## Preview

From the repository root:

```powershell
python -m http.server 8787 --bind 127.0.0.1 --directory website/dist
```

Open `http://127.0.0.1:8787`. Stop the server with Ctrl+C.

## Files and publishing

- `dist/`: standalone site; all asset paths work under `/CoverPlayer/` on GitHub Pages.
- Download buttons link to GitHub Releases so unpublished local builds are not advertised as available assets.
- Screenshots are copies of the native German gallery in `docs/screenshots/generated/`. Refresh copies when the UI changes.
- Animation has a pause button and respects reduced-motion preferences; without JavaScript, a static image is shown.
- Artwork attribution and license links are in the footer.
- No deployment is configured. Later, a GitHub Pages workflow can upload only `website/dist` as its Pages artifact.
