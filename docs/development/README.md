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

Remove `-DryRun` to build, test, verify, tag and upload the three release packages.
Existing tags are not replaced. A source push alone does not publish a release;
a successful dry run does not prove that remote publication has succeeded.

## Demo media

[Generate the galleries and music GIF](../screenshots/README.md) with the actual
SDL renderer. Original cover files are local inputs excluded from Git and packages.

The renderer cache test normally uses SDL's dummy video driver. On firmware
without that driver, `coverplayer_renderer_cache_test --device` opens a real
test window; run it from the app directory so fonts resolve. It exercises
asynchronous loading, reuse, eviction, missing artwork and shutdown. The motion
test checks rapid reversals and equivalent motion at different frame rates.
