#!/bin/sh
set -eu

BINARY="$1"
DESTINATION="$2"
LICENSE_DESTINATION="$3"
# Space-separated exact .so names to exclude from the walk entirely (not
# just from the copied output): passing a name here means "assume this is
# provided by the target system", so nothing reachable only through that
# name's own dependencies gets pulled in either. Used to keep Knulli's
# package free of the desktop Debian SDL2/ALSA build and everything that
# exists solely to support it (PulseAudio, X11, Wayland, D-Bus, Kerberos,
# NFS/RPC, tcp-wrappers, ...) - Knulli ships its own hardware-matched SDL2
# and expects coverplayer to resolve it from the system library path.
EXTRA_EXCLUDED="${4:-}"
SEARCH_PATHS="/usr/lib/aarch64-linux-gnu /lib/aarch64-linux-gnu"
EXCLUDED='^(libc\.so|libm\.so|libpthread\.so|libdl\.so|librt\.so|ld-linux-aarch64\.so'
for NAME in $EXTRA_EXCLUDED; do
    EXCLUDED="$EXCLUDED|$(printf '%s' "$NAME" | sed 's/\./\\./g')"
done
EXCLUDED="$EXCLUDED)"

mkdir -p "$DESTINATION"
mkdir -p "$LICENSE_DESTINATION/common-licenses"
cp /usr/share/common-licenses/* "$LICENSE_DESTINATION/common-licenses/"
QUEUE_FILE="$(mktemp)"
SEEN_FILE="$(mktemp)"
trap 'rm -f "$QUEUE_FILE" "$SEEN_FILE"' EXIT

printf '%s\n' "$BINARY" >"$QUEUE_FILE"
while [ -s "$QUEUE_FILE" ]; do
    CURRENT="$(sed -n '1p' "$QUEUE_FILE")"
    sed '1d' "$QUEUE_FILE" >"$QUEUE_FILE.next"
    mv "$QUEUE_FILE.next" "$QUEUE_FILE"
    aarch64-linux-gnu-readelf -d "$CURRENT" 2>/dev/null | sed -n 's/.*Shared library: \[\(.*\)\]/\1/p' | while read -r NAME; do
        echo "$NAME" | grep -Eq "$EXCLUDED" && continue
        grep -Fxq "$NAME" "$SEEN_FILE" && continue
        printf '%s\n' "$NAME" >>"$SEEN_FILE"
        FOUND="$(find $SEARCH_PATHS -name "$NAME" -print -quit 2>/dev/null)"
        if [ -z "$FOUND" ]; then
            echo "ERROR: unresolved ARM64 runtime library: $NAME" >&2
            exit 1
        fi
        cp -L "$FOUND" "$DESTINATION/$NAME"
        OWNER="$(dpkg-query -S "$FOUND" 2>/dev/null | sed -n '1s/: .*//p' || true)"
        if [ -z "$OWNER" ]; then
            RESOLVED="$(readlink -f "$FOUND")"
            OWNER="$(dpkg-query -S "$RESOLVED" 2>/dev/null | sed -n '1s/: .*//p' || true)"
        fi
        if [ -z "$OWNER" ]; then
            echo "ERROR: no Debian package owner for $FOUND" >&2
            exit 1
        fi
        PACKAGE="${OWNER%%:*}"
        COPYRIGHT="/usr/share/doc/$PACKAGE/copyright"
        if [ ! -f "$COPYRIGHT" ]; then
            echo "ERROR: missing Debian copyright file for $OWNER" >&2
            exit 1
        fi
        cp -L "$COPYRIGHT" "$LICENSE_DESTINATION/$PACKAGE.copyright"
        dpkg-query -W -f='${Package} ${Version}\n' "$OWNER" >>"$LICENSE_DESTINATION/package-versions.txt"
        printf '%s\n' "$FOUND" >>"$QUEUE_FILE"
    done
done
sort -u "$LICENSE_DESTINATION/package-versions.txt" -o "$LICENSE_DESTINATION/package-versions.txt"
