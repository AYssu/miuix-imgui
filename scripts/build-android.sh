#!/usr/bin/env bash
set -euo pipefail

skui_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
skui_abi="${1:-arm64-v8a}"
skui_build="${2:-$skui_root/build/$skui_abi}"
skui_sdk="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-$HOME/Android/Sdk}}"
skui_ndk="${SKUI_NDK:-${ANDROID_NDK_HOME:-}}"

if [[ -z "$skui_ndk" ]]; then
    shopt -s nullglob
    skui_candidates=("$skui_sdk"/ndk/*/build/cmake/android.toolchain.cmake)
    if ((${#skui_candidates[@]})); then
        mapfile -t skui_candidates < <(printf '%s\n' "${skui_candidates[@]}" | sort -Vr)
        skui_ndk="${skui_candidates[0]%/build/cmake/android.toolchain.cmake}"
    fi
fi
if [[ ! -f "$skui_ndk/build/cmake/android.toolchain.cmake" ]]; then
    printf '找不到 Android NDK，请设置 SKUI_NDK=/path/to/ndk\n' >&2
    exit 1
fi

cmake -S "$skui_root" -B "$skui_build" -G 'Unix Makefiles' \
    -DCMAKE_TOOLCHAIN_FILE="$skui_ndk/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="$skui_abi" -DANDROID_PLATFORM=android-28 \
    -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release \
    -DSKUI_BUILD_ANDROID_DEMO=ON -DSKUI_BUILD_TESTS=OFF -DSKUI_ANDROID_SDK="$skui_sdk"
cmake --build "$skui_build" --parallel "${SKUI_JOBS:-6}"
printf '\n可执行文件：%s/sk-imgui-ui\n' "$skui_build"
