#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 3 || $# -gt 4 ]]; then
    echo "usage: $0 PAPERBOAT_BINARY SHIP_HOME CAPTURE_DIR [new|existing]" >&2
    exit 2
fi

paperboat_binary=$1
ship_home=$2
capture_dir=$3
save_mode=${4:-existing}
tool_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
preload_library=${TMPDIR:-/tmp}/paperboat-sdl-input-capture-${UID}.so

cc -shared -fPIC -Wall -Wextra -Werror -O2 \
    -o "$preload_library" "$tool_dir/sdl_input_capture.c" \
    $(pkg-config --cflags sdl2) -ldl -lGL
mkdir -p -- "$capture_dir"

case "$save_mode" in
    new)
        # Skip logos, enter file select, create an eight-character file, confirm it,
        # select it, and then advance dialogue through the Mario's-house sequence.
        input_script='2:Space,5:Space,6:X,7:X,8:X,9:X,10:X,11:X,12:X,13:X,14:X,15:X,17:X,19:X,22:X'
        input_repeat='24:0.65:100:X'
        capture_start=32
        capture_end=55
        timeout_seconds=58
        ;;
    existing)
        # Skip logos, enter file select, load slot one, and advance dialogue.
        input_script='2:Space,5:Space,7:X'
        input_repeat='9:0.65:100:X'
        capture_start=18
        capture_end=42
        timeout_seconds=45
        ;;
    *)
        echo "save mode must be 'new' or 'existing'" >&2
        exit 2
        ;;
esac

timeout "${timeout_seconds}s" env \
    SDL_VIDEODRIVER=offscreen \
    SDL_AUDIODRIVER=dummy \
    SHIP_HOME="$ship_home" \
    LD_PRELOAD="$preload_library" \
    PAPERBOAT_CAPTURE_DIR="$capture_dir" \
    PAPERBOAT_CAPTURE_INTERVAL=0.1 \
    PAPERBOAT_CAPTURE_START="$capture_start" \
    PAPERBOAT_CAPTURE_END="$capture_end" \
    PAPERBOAT_INPUT_SCRIPT="$input_script" \
    PAPERBOAT_INPUT_REPEAT="$input_repeat" \
    "$paperboat_binary" || [[ $? -eq 124 ]]
