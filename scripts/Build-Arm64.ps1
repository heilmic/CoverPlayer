[CmdletBinding()]
param()

# 'Continue' (not 'Stop'): this script only runs native docker commands, and
# Windows PowerShell 5.1 wraps a native command's stderr output as a
# terminating NativeCommandError under 'Stop' even when it exits 0 (docker's
# own build/pull progress goes to stderr). Real failures are still caught
# below via the explicit $LASTEXITCODE checks, which do not depend on this
# preference.
$ErrorActionPreference = 'Continue'
$repositoryRoot = Split-Path -Parent $PSScriptRoot

docker build --file "$repositoryRoot/docker/Dockerfile.arm64" --tag coverplayer-arm64-build "$repositoryRoot"
if ($LASTEXITCODE -ne 0) {
    throw 'CoverPlayer ARM64 build image failed.'
}

docker run --rm `
    --volume "${repositoryRoot}:/work" `
    coverplayer-arm64-build `
    sh -lc 'cmake -E remove_directory build/arm64-release && cmake -S . -B build/arm64-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-aarch64.cmake -DCOVERPLAYER_BUNDLE_WINDOWS_RUNTIME=OFF && cmake --build build/arm64-release'
if ($LASTEXITCODE -ne 0) {
    throw 'CoverPlayer ARM64 compilation failed.'
}
