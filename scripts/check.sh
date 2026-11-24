#!/usr/bin/env bash
# Same entry points locally and in CI. Never starts a device process.
set -euo pipefail
skui_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
skui_mode="${1:-host}"
skui_build="${2:-$skui_root/build/check-$skui_mode}"
skui_jobs="${SKUI_JOBS:-4}"
case "$skui_mode" in
    host|sanitizers)
        skui_flags=()
        if [[ "$skui_mode" == sanitizers ]]; then
            skui_flags+=("-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer"
                         "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined")
        fi
        cmake -S "$skui_root" -B "$skui_build" \
            -DCMAKE_BUILD_TYPE=Debug -DSKUI_BUILD_ANDROID_DEMO=OFF \
            -DSKUI_BUILD_TESTS=ON "${skui_flags[@]}"
        cmake --build "$skui_build" --parallel "$skui_jobs"
        ctest --test-dir "$skui_build" --output-on-failure
        ;;
    consumer)
        cmake -S "$skui_root/examples/minimal" -B "$skui_build" -DCMAKE_BUILD_TYPE=Debug
        cmake --build "$skui_build" --parallel "$skui_jobs"
        "$skui_build/skui-consumer"
        ;;
    *) printf 'Usage: bash scripts/check.sh {host|sanitizers|consumer} [build-directory]\n' >&2; exit 2 ;;
esac
