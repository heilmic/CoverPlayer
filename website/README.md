# CoverPlayer website

[Live website](https://heilmic.github.io/CoverPlayer/). German project landing page. Plain HTML, CSS and JavaScript; no build step, external fonts or runtime dependencies.

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
- GitHub Pages publishes the root of `gh-pages`; `.nojekyll` serves these files without Jekyll processing.
- Source is maintained on `main`. Only `website/dist` is exported to the publishing branch.

## Publish an update

Commit and push the reviewed website changes on `main`, then run from the repository root:

```powershell
$websiteCommit = git subtree split --prefix=website/dist
if ($LASTEXITCODE -ne 0) { throw "Website export failed" }
git push origin "${websiteCommit}:refs/heads/gh-pages"
if ($LASTEXITCODE -ne 0) { throw "Website push failed" }
```

GitHub deploys the branch automatically. Check the Pages build in GitHub Actions and open the live URL to verify the result. Do not force-push; fetch and resolve a divergent branch before publishing.

Repository settings: **Settings → Pages → Deploy from a branch → gh-pages / (root)**.

## Visual identity and content

- Navy surfaces and restrained turquoise accents follow the app UI; amber is reserved for alpha status.
- Header/footer reuse the existing Knulli category SVG wordmark. The favicon uses the same player symbol.
- Family use: parents prepare MP3 collections; children choose familiar cover artwork with physical controls.
- Knulli category package is featured alongside its actual Art Book Next screenshot and setup guide.
- Keep copy factual and short; avoid lifestyle slogans.
