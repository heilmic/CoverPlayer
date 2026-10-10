#!/bin/sh

APPDIR="$(dirname "$(readlink -f "$0")")/CoverPlayer"
LOGDIR="/userdata/system/logs"
[ -d "$LOGDIR" ] || mkdir -p "$LOGDIR" 2>/dev/null || LOGDIR="/tmp"
LOGFILE="$LOGDIR/coverplayer.log"

export LD_LIBRARY_PATH="$APPDIR/libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export XDG_DATA_HOME="/userdata/system/configs/coverplayer"
export COVERPLAYER_MEDIA_ROOT="${COVERPLAYER_MEDIA_ROOT:-/userdata/audiobooks}"
export COVERPLAYER_BROWSE_ROOT="${COVERPLAYER_BROWSE_ROOT:-/userdata}"
export COVERPLAYER_SWAP_FACE_BUTTONS=1
if [ -x /usr/bin/knulli-bluetooth ] && [ -x /usr/bin/bluetoothctl ]; then
    export COVERPLAYER_BLUETOOTH=1
fi
if command -v pactl >/dev/null 2>&1; then
    if command -v timeout >/dev/null 2>&1; then
        export COVERPLAYER_AUDIO_DUCKING="${COVERPLAYER_AUDIO_DUCKING:-1}"
        export COVERPLAYER_DUCK_PERCENT="${COVERPLAYER_DUCK_PERCENT:-50}"
    fi
    INITIAL_VOLUME="$(pactl get-sink-volume @DEFAULT_SINK@ 2>/dev/null | awk '{ for (i=1; i<=NF; i++) if ($i ~ /^[0-9]+%$/) { gsub(/%/, "", $i); print $i; exit } }')"
    case "$INITIAL_VOLUME" in
        ''|*[!0-9]*) ;;
        *) export COVERPLAYER_INITIAL_VOLUME="$INITIAL_VOLUME"; export COVERPLAYER_SYSTEM_VOLUME=1 ;;
    esac
fi

{
    echo "=== CoverPlayer start: $(date) ==="
    echo "App directory: $APPDIR"
    if [ ! -x "$APPDIR/bin/coverplayer" ]; then
        echo "ERROR: CoverPlayer binary is missing or not executable"
        exit 2
    fi
    cd "$APPDIR" || exit 3
    "$APPDIR/bin/coverplayer" "$@"
    STATUS=$?
    echo "CoverPlayer exit status: $STATUS"
    exit "$STATUS"
} >"$LOGFILE" 2>&1
