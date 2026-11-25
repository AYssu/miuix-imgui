#!/usr/bin/env python3
"""Check API and dependency declarations agree without downloading anything."""
import json
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
header = (root / "include/skui/version.h").read_text()
parts = [int(re.search(rf"#define SKUI_VERSION_{part}\s+(\d+)", header)[1])
         for part in ("MAJOR", "MINOR", "PATCH")]
version = ".".join(map(str, parts))
assert f'#define SKUI_VERSION_STRING "{version}"' in header
cmake = (root / "CMakeLists.txt").read_text()
assert f"project(miuix-imgui VERSION {version} " in cmake
lock = json.loads((root / "dependencies.lock.json").read_text())
assert lock["schema"] == 1
imgui = (root / "include/ImGui/imgui.h").read_text()
assert re.search(r'#define IMGUI_VERSION\s+"([^"]+)"', imgui)[1] == lock["imgui"]["version"]
assert f"URL {lock['freetype']['archive']}" in cmake
assert f"URL_HASH SHA256={lock['freetype']['sha256']}" in cmake
assert f"find_package(SDL2 {lock['desktop']['sdl2_minimum']} CONFIG" in cmake
workflow = (root / ".github/workflows/ci.yml").read_text()
android_build = (root / "scripts/build-android.sh").read_text()
assert f"-DANDROID_PLATFORM=android-{lock['android']['native_api']}" in android_build
assert f"-DANDROID_STL={lock['android']['stl']}" in android_build
assert lock["android"]["abi"] in android_build
assert lock["android"]["ndk"] in workflow
assert f"platforms;android-{lock['android']['sdk_platform']}" in workflow
assert f"build-tools;{lock['android']['build_tools']}" in workflow
assert f"java-version: '{lock['android']['java']}'" in workflow
for name in ("imgui", "freetype"):
    entry = lock[name]
    assert re.fullmatch(r"[a-f0-9]{40}", entry["commit"])
    assert re.fullmatch(r"[a-f0-9]{64}", entry["sha256"])
    assert (root / entry["license"]).is_file()
    if name == "imgui":
        assert entry["sha256"] in (root / "docs/dependencies.md").read_text()
print(f"PASS version/dependencies: skui {version}, ImGui {lock['imgui']['version']}, FreeType {lock['freetype']['version']}")
