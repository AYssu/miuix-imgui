#!/usr/bin/env python3
"""Build/install, relocate, and run independent bundled/host ImGui consumers."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--build-root", type=Path, default=root / "build/check-package")
parser.add_argument("--jobs", type=int, default=4)
parser.add_argument("--android-build", type=Path, help="Test an existing Android library build (compile consumers only)")
args = parser.parse_args()
args.build_root.mkdir(parents=True, exist_ok=True)


def run(command, fail=False):
    result = subprocess.run([str(part) for part in command], cwd=root, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if (result.returncode == 0) == fail:
        raise RuntimeError(f"Unexpected exit {result.returncode}: {command}\n{result.stdout}")
    return result.stdout


with tempfile.TemporaryDirectory(prefix="package-", dir=args.build_root.resolve()) as temporary:
    work = Path(temporary)
    # Copy consumer sources so compilation doesn't borrow repository headers.
    sources = work / "independent-source"
    shutil.copytree(root / "examples/minimal", sources / "minimal")
    shutil.copytree(root / "examples/package-consumer", sources / "package-consumer")
    for mode in (("android",) if args.android_build else ("bundled", "host")):
        print(f"Checking {mode} ImGui package installation and relocation", flush=True)
        build, stage = work / f"{mode}-build", work / f"{mode}-original"
        cross = []
        if mode == "android":
            build = args.android_build.resolve()
            cache = {}
            for line in (build / "CMakeCache.txt").read_text().splitlines():
                if ":" in line and "=" in line and not line.startswith(("//", "#")):
                    name, value = line.split("=", 1)
                    cache[name.split(":", 1)[0]] = value
            for name in ("CMAKE_TOOLCHAIN_FILE", "ANDROID_ABI", "ANDROID_PLATFORM", "ANDROID_STL"):
                if not cache.get(name):
                    raise RuntimeError(f"Android build cache is missing {name}")
                cross.append(f"-D{name}={cache[name]}")
            run(["cmake", "--build", build, "--target", "skui-ui", "--parallel", args.jobs])
        else:
            source = root if mode == "bundled" else root / "tests/fixtures/host-imgui"
            run(["cmake", "-S", source, "-B", build, "-DCMAKE_BUILD_TYPE=Release",
                 "-DCMAKE_INSTALL_LIBDIR=lib", "-DSKUI_BUILD_ANDROID_DEMO=OFF",
                 f"-DSKUI_SOURCE_DIR={root}"])
            run(["cmake", "--build", build, "--parallel", args.jobs])
        run(["cmake", "--install", build, "--prefix", stage])
        relocated = work / f"{mode} relocated package"
        stage.rename(relocated)
        assert not stage.exists()
        for metadata in relocated.rglob("*.cmake"):
            text = metadata.read_text()
            for forbidden in (str(root), str(build), str(stage)):
                assert forbidden not in text, (metadata, forbidden)
        assert (relocated / "share/miuix-imgui/LICENSE").exists()
        assert (relocated / "share/miuix-imgui/LICENSES/Dear-ImGui-MIT.txt").exists()
        if mode == "host":
            assert not (relocated / "include/skui/imgui/imgui.h").exists()
            assert not (relocated / "lib/libskui-imgui.a").exists()
        for navigation_only in (False, True):
            consumer = work / f"{mode}-consumer-{navigation_only}"
            flags = [f"-DCMAKE_PREFIX_PATH={relocated}", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"]
            if mode == "android":
                flags.extend([*cross, f"-DCMAKE_FIND_ROOT_PATH={relocated}"])
            if navigation_only:
                flags.append("-DSKUI_NAVIGATION_ONLY=ON")
            elif mode == "host":
                flags.append(f"-DSKUI_HOST_IMGUI_PACKAGE={relocated}/lib/cmake/host-imgui")
            run(["cmake", "-S", sources / "package-consumer", "-B", consumer, *flags])
            run(["cmake", "--build", consumer, "--parallel", args.jobs])
            if mode != "android":
                output = run([consumer / "skui-package-consumer"])
                assert "skui" in output
            compilation = (consumer / "compile_commands.json").read_text()
            assert str(root / "include") not in compilation
            if navigation_only:
                link = (consumer / "CMakeFiles/skui-package-consumer.dir/link.txt").read_text()
                assert "imgui.a" not in link and "skui-ui.a" not in link
        if mode == "host":
            output = run(["cmake", "-S", sources / "package-consumer", "-B", work / "missing-host",
                          f"-DCMAKE_PREFIX_PATH={relocated}"], fail=True)
            assert "host ImGui" in output
        negative = work / f"{mode}-negative-source"
        negative.mkdir()
        for name, request in (("version", "find_package(skui 99.0 CONFIG REQUIRED)"),
                              ("component", "find_package(skui CONFIG REQUIRED COMPONENTS unknown)")):
            (negative / "CMakeLists.txt").write_text(
                f"cmake_minimum_required(VERSION 3.21)\nproject(negative LANGUAGES CXX)\n{request}\n")
            run(["cmake", "-S", negative, "-B", work / f"{mode}-{name}",
                 f"-DCMAKE_PREFIX_PATH={relocated}"], fail=True)
        evidence = "compiled (not device-run)" if mode == "android" else "compiled and executed"
        print(f"PASS {mode}: moved prefix, UI/navigation consumers {evidence}, licenses, paths and negative contracts", flush=True)
print("PASS package checks; owned temporary builds and installations removed")
