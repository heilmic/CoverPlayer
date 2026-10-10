# Build, test and deploy

[Documentation](../README.md) · [Architecture](../architecture.md)

## Desktop build

Requires CMake, Ninja, a C++17 compiler, SDL2, SDL2_image, SDL2_ttf and libmpg123.

```sh
cmake --preset desktop-debug
cmake --build --preset desktop-debug
ctest --test-dir build/desktop-debug --output-on-failure
```

On Windows, use MSYS2 UCRT64 under `C:\msys64\ucrt64` and Python on PATH:

```powershell
./scripts/Build-And-Test.ps1 -SkipPackages
```

Omit `-SkipPackages` to also build ARM64 packages using Docker Desktop.
Add `-Mp3Path <file>` for the optional real-file seek test.
Build outputs stay under `build/`; building does not install or publish anything.

The build-and-test script also checks tracked Markdown for valid UTF-8, exact
repository paths and heading anchors. To include external URLs and published
release notes, run `python scripts/check-doc-links.py --online --release v1.0.0`.

Linux also runs the playback-power and fake-PulseAudio lifecycle tests. The
power test uses an isolated filesystem; its explicit `--knulli-hardware-check`
mode changes real brightness and invokes firmware idle hooks for a device test.

## Device deployment

Copy `scripts/deploy.example.json` to the ignored `scripts/deploy.local.json`.
Set device addresses and, optionally, SSH host-key fingerprints there. Passwords
are prompted for; do not commit credentials or the local configuration.

```powershell
./scripts/Deploy-Knulli.ps1 -TestOnly -Targets <device-ip>
./scripts/Deploy-muOS.ps1
```

- `-TestOnly` installs only CoverPlayer-Test beside the regular Knulli app.
- Without it, Knulli deployment installs the regular app; `-IncludeTest` adds Test.
- `-Build` rebuilds and tests first. `-PreflightOnly` checks packages locally.
- Close CoverPlayer before installing. Upload hashes and installed manifests
  are verified; a running player causes deployment to stop.
- Knulli Test deployment backs up the app, launcher and saved configuration in
  `/userdata/system/coverplayer-backups`, pauses Syncthing for installation,
  and resumes it on success or failure. Do not assume the muOS script has these
  same safeguards.

Keep experimental device changes in the Test app. After installation, check
launch, navigation, playback, pause, seeking, resume, auto-next and help.
The [device record](../knulli-test-deployment-2026-10-10.md) tracks tested builds.

## Publish a release

Choose a new version, add its section to `CHANGELOG.md`, and commit the matching
source before running:

```powershell
./scripts/Publish-Release.ps1 -Tag <new-version-tag> -DryRun
```

Remove `-DryRun` to build, test, verify, tag and upload the four release packages.
Existing tags are not replaced. A source push alone does not publish a release;
a successful dry run does not prove that remote publication has succeeded.

The category ZIP reuses every file from the regular Knulli ZIP and adds a user
system overlay, wrapper, optional SVG and illustrated guide. To package it
without rebuilding the app, run `python scripts/build-knulli-category.py` with
the existing `build/release/CoverPlayer-Knulli.zip`. Its root manifest is rebuilt
and both archives are verified. Use `--base` and `--output` to select other paths.
`scripts/Generate-CategoryLogo.ps1` regenerates the SVG player symbol and outlined
wordmark from the bundled Roboto Mono Bold font using Windows System.Drawing.

## Demo media

[Generate the galleries and music GIF](../screenshots/README.md) with the actual
SDL renderer. Original cover files are local inputs excluded from Git and packages.

The renderer cache test normally uses SDL's dummy video driver. On firmware
without that driver, `coverplayer_renderer_cache_test --device` opens a real
test window; run it from the app directory so fonts resolve. It exercises
asynchronous loading, reuse, eviction, missing artwork and shutdown. The motion
test checks rapid reversals and equivalent motion at different frame rates.

## Platform build isolation

Linux ARM64 (Knulli/muOS), Switch, desktop tests and Switch UI previews use
`build/arm64-release`, `build/switch-alpha`, `build/desktop-debug` and
`build/switch-preview` respectively. CMake includes only the selected platform
adapter. The Switch UI is enabled only for the NRO and optional preview target;
Linux retains its existing layout and dependencies.

ARM64 packaging uses `build/staging-arm64` and cleans only its own unpacked
outputs in `build/release`. It preserves Switch archives and unrelated files.
Switch packaging also writes only its named outputs. Build each target once
at a time; simultaneous builds of the same target share its cache/output files.

See the [Switch build and FTP guide](../switch.md). FTP credentials are stored
only in the ignored `scripts/deploy-switch.local.json` file.

For an explicitly requested update to an existing release, keep its historical
Git tag unchanged. Publish the refreshed packages with release notes linking
the exact update commit and a matching source archive; the automatically
provided GitHub tag archives still describe the original tag. Include hashes
for the refreshed downloads. Switch alpha assets remain labeled experimental
inside a stable Knulli/muOS release.
