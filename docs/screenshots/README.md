# Demo screenshot review

Reviewed on 2026-10-06. RC4 device approval is based on the user's Knulli
and muOS tests; this review did not deploy to either device.

## Cover quality

The local source covers are 1000 x 1000 pixels (JPEG) or 2048 x 2048 pixels
(PNG). The renderer loads them into textures with a maximum side of 384
pixels. The central CoverFlow cover occupies approximately 218 x 218 pixels
in the 640 x 480 output.

Visual comparison of `238.png`, independent bicubic reductions to 384 and
218 pixels, and `generated/01-coverflow.png` shows that small illustration
details are lost at the final display size even with a high-quality reduction.
The rendered cover lettering also has more visible pixel steps than the
bicubic comparison. These reference reductions are diagnostic images, not
an exact reproduction of SDL's scaling pipeline.

The existing screenshots remain suitable for showing the handheld UI at its
native size. Keep the tested renderer and the 640 x 480 series unchanged.
Simply enlarging the PNG files would not recover detail. If larger marketing
images are needed later, use a separate screenshot-only rendering path with
larger cover textures and output dimensions, and retain the native series.

## Verification

- Desktop build and all seven CTest tests passed.
- Package verifier tests completed successfully.
- Regeneration reproduced all 14 screenshots and the README alias byte for byte.
- Both deployment preflights and the RC4 release dry run passed package checks.
- Cover sources and `scripts/deploy.local.json` remain excluded from Git.

Preflights verify local prerequisites and packages; they do not establish that
installation or publication works on the live systems. The RC4 dry run checks
existing release notes and assets, not readiness to publish a new release.

## Open workflow findings

Before the first real deployment with the scripts, address these points:

- `Deploy-Knulli.ps1` installs the regular CoverPlayer package by default;
  `-IncludeTest` adds the test package rather than selecting only the test app.
- Neither deployment script backs up the installed app before overwriting it.
- Neither deployment script pauses or excludes Syncthing during installation.

Archive hashes and installed manifests are checked by the scripts, but those
checks do not replace backup and synchronization handling. No real deployment
or release was performed during this review. Add new release notes only when
preparing the next release.
