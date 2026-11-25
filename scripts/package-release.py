#!/usr/bin/env python3
"""Create a reproducible source snapshot, manifest and SHA-256 checksums."""
import argparse
import gzip
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import tarfile
from publication import excluded, has_private_key

root = Path(__file__).resolve().parents[1]


def git(*arguments):
    return subprocess.run(["git", *arguments], cwd=root, check=True, stdout=subprocess.PIPE).stdout


def package(ref, output):
    revision = git("rev-parse", "--verify", "--end-of-options", ref + "^{commit}").decode().strip()
    header = git("show", f"{revision}:include/skui/version.h").decode()
    version = re.search(r'#define SKUI_VERSION_STRING "(\d+\.\d+\.\d+)"', header)[1]
    if (ref.startswith("refs/tags/") or re.fullmatch(r"v\d+\.\d+\.\d+", ref)) and ref.rsplit("/", 1)[-1] != "v" + version:
        raise ValueError("Release tag does not match the API version")
    prefix = f"miuix-imgui-{version}-g{revision[:12]}"
    source = git("archive", "--format=tar", "--prefix=" + prefix + "/", revision)
    names = set()
    with tarfile.open(fileobj=io.BytesIO(source), mode="r:") as archive:
        for entry in archive:
            relative = "" if entry.name.rstrip("/") == prefix else entry.name.removeprefix(prefix + "/").rstrip("/")
            path = PurePosixPath(relative)
            if (entry.name.rstrip("/") != prefix and not entry.name.startswith(prefix + "/")) or path.is_absolute() or ".." in path.parts:
                raise ValueError(f"Unsafe archive member: {entry.name}")
            if relative and (excluded(relative) or not (entry.isfile() or entry.isdir())):
                raise ValueError(f"Excluded archive member: {relative}")
            if entry.isfile():
                content = archive.extractfile(entry).read().decode("utf-8")
                if has_private_key(content):
                    raise ValueError(f"Private key in archive member: {relative}")
            names.add(relative)
    required = {"README.md", "LICENSE", "THIRD_PARTY_NOTICES.md", "CMakeLists.txt", "include/skui/version.h", "dependencies.lock.json"}
    if not required.issubset(names):
        raise ValueError(f"Incomplete source archive: {required - names}")
    dependencies = json.loads(git("show", f"{revision}:dependencies.lock.json"))
    if dependencies.get("schema") != 1:
        raise ValueError("Unknown dependency lock schema")
    for dependency in ("imgui", "freetype"):
        if dependencies[dependency]["license"] not in names:
            raise ValueError(f"Missing dependency license: {dependency}")
    cmake = git("show", f"{revision}:CMakeLists.txt").decode()
    if f"project(miuix-imgui VERSION {version} " not in cmake:
        raise ValueError("CMake and API versions differ")
    filename = prefix + ".tar.gz"
    manifest = {"schema": 1, "version": version, "revision": revision, "archive": filename,
                "source_epoch": int(git("show", "-s", "--format=%ct", revision)), "dependencies": dependencies}
    output = Path(output).resolve()
    # Reserve a dedicated empty output directory; never overwrite a release.
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise ValueError("Release output directory must be empty")
    compressed = io.BytesIO()
    with gzip.GzipFile(filename="", mode="wb", fileobj=compressed, compresslevel=9, mtime=0) as stream:
        stream.write(source)
    files = {filename: compressed.getvalue(),
             "manifest.json": (json.dumps(manifest, ensure_ascii=False, sort_keys=True, indent=2) + "\n").encode()}
    for name, contents in files.items():
        (output / name).write_bytes(contents)
    checksums = "".join(f"{hashlib.sha256(data).hexdigest()}  {name}\n" for name, data in sorted(files.items()))
    (output / "SHA256SUMS").write_text(checksums)
    print(f"PASS source snapshot: {revision}, {len(names)} archive entries -> {output}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ref", default="HEAD")
    parser.add_argument("--output", type=Path, default=root / "build/release")
    arguments = parser.parse_args()
    package(arguments.ref, arguments.output)
