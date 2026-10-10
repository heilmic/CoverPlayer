#!/usr/bin/env bash
# Run in the pinned devkitPro container; see Build-Switch.ps1.
set -euo pipefail
cd /work
build=build/switch-alpha
cmake -S . -B "$build" -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake \
    -DCMAKE_BUILD_TYPE=Release -DCOVERPLAYER_BUILD_TESTS=OFF
cmake --build "$build" -j4

# Keep exact decoder sources and the port patch so the statically linked LGPL
# library can be modified and relinked using the accompanying application source.
thirdparty="$build/third-party"
mkdir -p "$thirdparty/licenses" "$thirdparty/relink"
fetch() {
    local url="$1" destination="$2"
    if [ ! -s "$destination" ]; then
        curl --fail --location --retry 3 "$url" -o "$destination.tmp"
        mv "$destination.tmp" "$destination"
    fi
}
fetch https://www.mpg123.de/download/mpg123-1.31.3.tar.bz2 "$thirdparty/relink/mpg123-1.31.3.tar.bz2"
echo "1ca77d3a69a5ff845b7a0536f783fee554e1041139a6b978f6afe14f5814ad1a  $thirdparty/relink/mpg123-1.31.3.tar.bz2" | sha256sum -c -
fetch https://raw.githubusercontent.com/devkitPro/pacman-packages/master/switch/mpg123/mpg123-1.31.3.patch "$thirdparty/relink/mpg123-1.31.3.patch"
echo "99e86f82aa5c53769747c7e104f6d2f3a679b29481df38386321beb6f7ef0b70  $thirdparty/relink/mpg123-1.31.3.patch" | sha256sum -c -
fetch https://raw.githubusercontent.com/devkitPro/pacman-packages/master/switch/mpg123/PKGBUILD "$thirdparty/relink/PKGBUILD"
tar -xOf "$thirdparty/relink/mpg123-1.31.3.tar.bz2" mpg123-1.31.3/COPYING > "$thirdparty/licenses/mpg123-LGPL-2.1.txt"
for package in switch-sdl2_image switch-sdl2_ttf switch-freetype switch-libpng switch-libjpeg-turbo switch-bzip2 switch-zlib; do
    cp -r "/opt/devkitpro/portlibs/switch/licenses/$package" "$thirdparty/licenses/"
done
fetch https://raw.githubusercontent.com/devkitPro/SDL/switch-sdl-2.28/LICENSE.txt "$thirdparty/licenses/SDL2.txt"
fetch https://raw.githubusercontent.com/switchbrew/libnx/v4.12.0/LICENSE.md "$thirdparty/licenses/libnx.txt"
fetch https://raw.githubusercontent.com/harfbuzz/harfbuzz/main/COPYING "$thirdparty/licenses/harfbuzz.txt"
fetch https://raw.githubusercontent.com/webmproject/libwebp/main/COPYING "$thirdparty/licenses/libwebp.txt"
fetch https://archive.mesa3d.org/older-versions/20.x/mesa-20.1.0-rc3.tar.xz "$thirdparty/mesa-20.1.0-rc3.tar.xz"
echo "c90b75ea34302ebde9b81b87c5642fa864c40fe9c4ad34ce0793170c1413168d  $thirdparty/mesa-20.1.0-rc3.tar.xz" | sha256sum -c -
tar -xOf "$thirdparty/mesa-20.1.0-rc3.tar.xz" mesa-20.1.0-rc3/docs/license.html > "$thirdparty/licenses/mesa.html"
# This port keeps its MIT notices in individual source/header files.
fetch https://codeload.github.com/devkitPro/libdrm_nouveau/tar.gz/refs/tags/v1.0.1 "$thirdparty/libdrm_nouveau-1.0.1.tar.gz"
tar -xf "$thirdparty/libdrm_nouveau-1.0.1.tar.gz" -C "$thirdparty/licenses"
fetch https://raw.githubusercontent.com/gcc-mirror/gcc/releases/gcc-15.2.0/COPYING.RUNTIME "$thirdparty/licenses/GCC-Runtime-Exception.txt"
fetch https://raw.githubusercontent.com/gcc-mirror/gcc/releases/gcc-15.2.0/COPYING3 "$thirdparty/licenses/GPL-3.0.txt"
fetch https://raw.githubusercontent.com/devkitPro/newlib/master/COPYING.NEWLIB "$thirdparty/licenses/newlib.txt"
dkp-pacman -Q > "$thirdparty/package-versions.txt"
/opt/devkitpro/devkitA64/bin/aarch64-none-elf-readelf -h "$build/coverplayer.elf" > "$thirdparty/elf-header.txt"
