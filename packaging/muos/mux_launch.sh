#!/bin/sh
# HELP: CoverPlayer
# GRID: CoverPlayer
# ICON: CoverPlayer

. /opt/muos/script/var/func.sh

if pgrep -f "playbgm.sh" >/dev/null; then
    killall -q "playbgm.sh" "mpg123"
fi

echo app >/tmp/act_go
STORAGE="$(GET_VAR "device" "storage/rom/mount")"
APPDIR="/run/muos/storage/application/CoverPlayer"
export LD_LIBRARY_PATH="$APPDIR/libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
if [ -x /usr/bin/wpctl ] && /usr/bin/wpctl get-volume @DEFAULT_AUDIO_SINK@ >/dev/null 2>&1; then
    export COVERPLAYER_VOLUME_BACKEND=wpctl
elif command -v pactl >/dev/null 2>&1; then
    INITIAL_VOLUME="$(pactl get-sink-volume @DEFAULT_SINK@ 2>/dev/null | awk '{ for (i=1; i<=NF; i++) if ($i ~ /^[0-9]+%$/) { gsub(/%/, "", $i); print $i; exit } }')"
    case "$INITIAL_VOLUME" in
        ''|*[!0-9]*) ;;
        *) export COVERPLAYER_INITIAL_VOLUME="$INITIAL_VOLUME"; export COVERPLAYER_SYSTEM_VOLUME=1 ;;
    esac
fi
export XDG_DATA_HOME="/run/muos/storage/save/CoverPlayer"
export COVERPLAYER_MEDIA_ROOT="${COVERPLAYER_MEDIA_ROOT:-$STORAGE/Music}"
export COVERPLAYER_BROWSE_ROOT="${COVERPLAYER_BROWSE_ROOT:-$STORAGE}"

SETUP_SDL_ENVIRONMENT
SET_VAR "system" "foreground_process" "coverplayer"

cd "$APPDIR" || exit 3
# muOS passes the application directory as $1; CoverPlayer interprets
# positional arguments as media files, so do not forward the launcher args.
"$APPDIR/bin/coverplayer" >>"$STORAGE/MUOS/log/coverplayer.log" 2>&1
