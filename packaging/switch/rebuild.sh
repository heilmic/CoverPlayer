#!/usr/bin/env bash
# From the extracted ZIP, run in the documented devkitPro container.
set -euo pipefail
cd /package/relink
if ! command -v autoreconf >/dev/null || [ ! -f /usr/share/aclocal/ltdl.m4 ]; then
    apt-get update
    apt-get install -y --no-install-recommends autoconf automake libtool libltdl-dev
fi
mkdir -p work
if [ ! -d work/mpg123-1.31.3 ]; then
    tar -xf mpg123-1.31.3.tar.bz2 -C work
    (cd work/mpg123-1.31.3 && patch -Np1 -i ../../mpg123-1.31.3.patch)
fi
# Edit work/mpg123-1.31.3 before rerunning to relink a modified decoder.
(
    cd work/mpg123-1.31.3
    source /opt/devkitpro/switchvars.sh
    # mpg123 1.31.3 predates GCC 15's default C23 function declarations.
    export CFLAGS="$CFLAGS -std=gnu11"
    autoreconf -fi
    LIBS='-lnx -lm' ./configure --prefix="$PORTLIBS_PREFIX" --host=aarch64-none-elf \
        --disable-shared --enable-static --enable-fifo=no --enable-ipv6=no \
        --enable-network=no --enable-int-quality=no --with-cpu=generic --with-default-audio=dummy
    make -j4
    make install
)
mkdir -p work/coverplayer
tar -xf coverplayer-source.tar.gz -C work/coverplayer
cmake -S work/coverplayer -B work/build -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake \
    -DCMAKE_BUILD_TYPE=Release -DCOVERPLAYER_BUILD_TESTS=OFF
cmake --build work/build -j4
cp work/build/CoverPlayer.nro /package/switch/CoverPlayer/CoverPlayer.nro
