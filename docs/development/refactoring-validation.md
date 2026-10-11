# CoverPlayer refactoring validation — 2026-10-11

Baseline: `2b955fa`, in `D:\Codex\CoverPlayer-github-20261005`.
The pre-existing untracked `docs/development/refactoring-prompt.md` was preserved.
All changes are local commits; no deployment, push, release or tag change occurred.

## Changes and commits

| Commit | Change |
| --- | --- |
| `2028b3c` | Shared base/Switch versions, generated startup header, complete source-archive build inputs |
| `6f2dfe4` | Named navigation scenarios and three-track auto-next regression before the core refactor |
| `4260a86` | Explicit track opening, playback identity separate from browsing selection |
| `7ea5c0b` | Separate handheld screen translation unit, shared UI content and compile-time profile |
| `02cd55c` | Readable navigation fixtures and named scenario runner |
| `53b0cab` | Architecture/preview documentation, package metadata tests, incremental version regeneration |
| `b53ecdf` | Explicit D-pad and shoulder-button transport mapping tests |
| `0a25f83` | Tie each navigation view assertion to the input step that produced it |

The background handoff now also resolves the playing track by media ID when
another row is selected, so it retains the album sequence. Handler dispatch
order, decoder, persistence formats, audio ducking, power hooks and dimming code
were preserved. Optional CMake test-definition and ARM64 incremental-build
cleanup was omitted to keep the scope focused.

## Executed checks

- Before editing: desktop build with all 11 CTest tests and Python/doc checks;
  Switch cross-build/package verification; ARM64 build and all four Linux packages.
- After the relevant stages and finally: `./scripts/Build-And-Test.ps1 -SkipPackages`;
  all 11 CTest tests passed, package-verifier checks passed, four documentation
  checker tests and two new Switch-package tests passed; tracked documentation
  has no link/encoding errors.
- `python tests/switch_deploy_test.py`: all four fake-FTP tests passed.
  No connection to a device was made.
- `python scripts/check-doc-links.py`: passed.
- `./scripts/Build-Switch.ps1`: passed; built NACP is `1.0.0-sw-a5`, startup
  version is `1.0.0 Switch alpha 5`, output filenames remain alpha 5.
- `./scripts/Build-Packages.ps1`: ARM64 build, runtime collection, manifests and
  Knulli, Knulli Test, Knulli Category and muOS package verifiers passed.
- Native Linux x86-64 build in a Debian 12 Docker container: all 13 CTest tests
  passed, including fake-PulseAudio `audio_ducker_lifecycle` and isolated-filesystem
  `playback_power`. The later D-pad/shoulder test addition was rebuilt and passed
  separately on Linux as well as in the final desktop suite.
- Follow-up review: a pre-input library-scan view made the three-track test's
  numeric view offset observe the frame before automatic advance. The fixture
  now records the final view for each polled input step. The complete desktop
  suite and the targeted native Linux navigation test passed. In an isolated
  mutation check, removing selection preservation from the application makes
  `automaticAdvancePreservesDifferentSelection` fail; the unchanged application
  passes. No production source was changed for this follow-up.
- Switch source archive extracted twice (version stage and final stage), then
  configured and cross-built with tests OFF in the pinned devkitPro container.
  Only the extracted archive was mounted; no original checkout or test sources
  were available. Both builds passed. In the final extracted copy, changing
  only `VERSION` from alpha 5 to alpha 6 and running `cmake --build` regenerated
  CMake, the startup header and the NACP correctly. The actual checkout stayed
  at alpha 5. Synthetic packager tests independently reject a stale built NACP.

The initial Linux test-image attempt using Debian 11 failed because security
package downloads returned HTTP 404. The replacement Debian 12 environment
built and passed; the project's existing ARM64 Debian 11 build image also built
successfully from its available local layers. Temporary test tooling and logs
remain under the ignored `build/refactoring/` directory.

## UI and platform isolation

The existing native screenshot tool rendered both languages before and after
the renderer extraction. All 66 images were pixel-identical: 15 handheld and
18 Switch images per language. CoverFlow, tracks, player, collection manager
and help were also visually inspected. Additional 720 × 480 handheld previews
were rendered and the player inspected. The separate Switch preview used
`COVERPLAYER_BUILD_SWITCH_PREVIEW=ON` and `COVERPLAYER_BUILD_TESTS=OFF`, and rendered
at 1280 × 720. Local demo covers were available. The documented gallery script
passed with 14 published screenshots after filtering out preview-only images.

The Linux compile commands contain no Switch translation units, and the Switch
link inputs contain no handheld screen translation unit. ARM64 symbols contain
`renderHandheldUi`, not `renderSwitchUi`; its dynamic dependencies remain SDL2,
SDL2_image, SDL2_ttf, mpg123 and the standard C/C++ runtime libraries. Knulli/muOS
package listings contain no Switch libraries or assets. No extra runtime
dependency was introduced. Cross-building against the existing older ARM64
SDL headers still succeeds with the geometry compatibility implementation.

SHA-256 comparisons around the final builds confirmed that Switch packaging
preserved all four Linux packages byte-for-byte, and Linux packaging preserved
all 15 existing Switch artifacts (alphas 1–5, NRO/ZIP/checksum) byte-for-byte.

## Size comparison

Sizes are bytes, comparing freshly built baseline outputs with final outputs.

| Output | Baseline | Final | Difference |
| --- | ---: | ---: | ---: |
| ARM64 executable | 733160 | 733672 | +512 |
| Switch ELF | 29444048 | 29259168 | -184880 |
| Switch NRO | 10207307 | 10203211 | -4096 |
| Switch alpha 5 ZIP | 5786069 | 5788098 | +2029 |
| Knulli ZIP | 3092495 | 3092615 | +120 |
| Knulli Test ZIP | 3093111 | 3093230 | +119 |
| Knulli Category ZIP | 3253399 | 3253517 | +118 |
| muOS MUXAPP | 1292237 | 1292352 | +115 |
| Windows debug executable | 10235771 | 10538657 | +302886 |

The Switch binary shrank after removing unused handheld screen code. Its ZIP
grew slightly because the source/relink archive now includes the new metadata,
CMake inputs and extracted UI sources. Linux release-size changes are below
0.1%. The Windows debug executable grew about 3%; a separate rebuild of
`2b955fa` and PE-section comparison attributes 276957 bytes of growth to debug
sections, while `.text` grew 864 bytes. Additional translation-unit/header
debug information accounts for most of the difference; this is not a similar
increase in the device packages. Archive compression/tar timestamps can also
affect ZIP byte counts.

## Limits

No hardware check mode or real device test was run. Screenshots do not establish
Switch/handheld performance, docking, audio routing or firmware behavior.
Sleep-timer cycling and suspend/resume behavior were tested; a real 15-minute
timer expiration was not awaited. The optional real-MP3 seek executable was
built but not run with `-Mp3Path`. Real-device validation remains a separate
follow-up; no additional product changes were added to this refactor.
