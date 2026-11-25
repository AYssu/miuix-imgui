#!/usr/bin/env python3
"""Verify source packaging determinism, exclusions, tags and overwrite safety."""
import hashlib
import json
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile

root = Path(__file__).resolve().parents[1]
build = root / "build/check-release"
build.mkdir(parents=True, exist_ok=True)


def run(command, cwd, expected=0):
    result = subprocess.run(command, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if result.returncode != expected:
        raise RuntimeError(f"{command}: exit {result.returncode}, expected {expected}\n{result.stdout}\n{result.stderr}")
    return result


with tempfile.TemporaryDirectory(prefix="source-", dir=build) as temporary:
    fixture = Path(temporary) / "fixture"
    fixture.mkdir()
    names = ["scripts/package-release.py", "scripts/publication.py", "include/skui/version.h", "dependencies.lock.json",
             "CMakeLists.txt", "README.md", "LICENSE", "THIRD_PARTY_NOTICES.md",
             "LICENSES/Dear-ImGui-MIT.txt", "LICENSES/FreeType-FTL.txt"]
    for name in names:
        target = fixture / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(root / name, target)
    (fixture / ".gitignore").write_text("/build/\n*.png\n")
    run(["git", "init", "--quiet"], fixture)
    run(["git", "add", "."], fixture)
    run(["git", "-c", "user.name=Source package test", "-c", "user.email=package-test@example.invalid",
         "commit", "--quiet", "-m", "fixture"], fixture)
    revision = run(["git", "rev-parse", "HEAD"], fixture).stdout.strip()
    # Neither ignored files nor uncommitted modifications belong to the commit snapshot.
    (fixture / "capture.png").write_text("ignored capture")
    (fixture / "untracked.txt").write_text("not committed")
    (fixture / "README.md").write_text("uncommitted changes")
    script = fixture / "scripts/package-release.py"
    outputs = [Path(temporary) / name for name in ("first", "second")]
    for output in outputs:
        run([sys.executable, str(script), "--output", str(output)], fixture)
        checksums = (output / "SHA256SUMS").read_text().splitlines()
        for line in checksums:
            digest, name = line.split("  ")
            assert hashlib.sha256((output / name).read_bytes()).hexdigest() == digest
        manifest = json.loads((output / "manifest.json").read_text())
        assert manifest["revision"] == revision and manifest["dependencies"]["schema"] == 1
        with tarfile.open(output / manifest["archive"]) as archive:
            archived = {member.name.split("/", 1)[1]: member for member in archive if "/" in member.name}
            assert "capture.png" not in archived and "untracked.txt" not in archived
            assert archive.extractfile(archived["README.md"]).read() == (root / "README.md").read_bytes()
    assert {path.name: path.read_bytes() for path in outputs[0].iterdir()} == {path.name: path.read_bytes() for path in outputs[1].iterdir()}
    # Refuse an occupied destination without replacing valid artifacts.
    before = (outputs[0] / "SHA256SUMS").read_bytes()
    run([sys.executable, str(script), "--output", str(outputs[0])], fixture, expected=1)
    assert (outputs[0] / "SHA256SUMS").read_bytes() == before
    run(["git", "tag", "v99.0.0"], fixture)
    run([sys.executable, str(script), "--ref", "v99.0.0", "--output", str(Path(temporary) / "wrong")], fixture, expected=1)
    # License closure and version mismatches also fail before writing artifacts.
    license_path = fixture / "LICENSES/FreeType-FTL.txt"
    license_content = license_path.read_bytes()
    license_path.unlink()
    run(["git", "add", "-u"], fixture)
    run(["git", "-c", "user.name=Source package test", "-c", "user.email=package-test@example.invalid",
         "commit", "--quiet", "-m", "missing license"], fixture)
    run([sys.executable, str(script), "--output", str(Path(temporary) / "missing-license")], fixture, expected=1)
    license_path.write_bytes(license_content)
    cmake = fixture / "CMakeLists.txt"
    cmake.write_text(re.sub(r"(project\(miuix-imgui VERSION )\d+\.\d+\.\d+", r"\g<1>99.0.0", cmake.read_text()))
    run(["git", "add", "."], fixture)
    run(["git", "-c", "user.name=Source package test", "-c", "user.email=package-test@example.invalid",
         "commit", "--quiet", "-m", "version mismatch"], fixture)
    run([sys.executable, str(script), "--output", str(Path(temporary) / "version-mismatch")], fixture, expected=1)
    shutil.copy2(root / "CMakeLists.txt", cmake)
    (fixture / "secret.txt").write_text("-----BEGIN " + "PRIVATE KEY-----")
    run(["git", "add", "."], fixture)
    run(["git", "-c", "user.name=Source package test", "-c", "user.email=package-test@example.invalid",
         "commit", "--quiet", "-m", "secret marker"], fixture)
    run([sys.executable, str(script), "--output", str(Path(temporary) / "secret")], fixture, expected=1)
    (fixture / "secret.txt").unlink()
    run(["git", "add", "-u"], fixture)
    # A forbidden file committed accidentally must fail rather than be silently published.
    (fixture / "capture.png").write_text("tracked capture")
    run(["git", "add", "-f", "capture.png"], fixture)
    run(["git", "-c", "user.name=Source package test", "-c", "user.email=package-test@example.invalid",
         "commit", "--quiet", "-m", "forbidden capture"], fixture)
    run([sys.executable, str(script), "--output", str(Path(temporary) / "forbidden")], fixture, expected=1)
print("PASS release: reproducible committed source, SHA256/manifest, untracked exclusion, tag mismatch and overwrite/forbidden protection")
