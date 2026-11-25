#!/usr/bin/env python3
"""Exercise the real desktop gallery renderer, persistence and clean exit."""
import argparse
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("executable", type=Path)
parser.add_argument("--build-root", type=Path, default=root / "build/check-desktop")
args = parser.parse_args()
executable = args.executable.resolve()
active = subprocess.run(["pgrep", "-a", "-x", executable.name], capture_output=True, text=True)
if active.returncode not in (0, 1):
    raise RuntimeError(f"Cannot check existing gallery processes: {active.stderr.strip()}")
if active.stdout.strip():
    raise RuntimeError(f"A gallery is already running; won't start a duplicate: {active.stdout.strip()}")
args.build_root.mkdir(parents=True, exist_ok=True)


def run(arguments, expected=0):
    process = subprocess.Popen([str(executable), *map(str, arguments)],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        stdout, stderr = process.communicate(timeout=30)
    except subprocess.TimeoutExpired:
        process.terminate()
        try:
            process.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.communicate()
        raise RuntimeError("Owned desktop test exceeded its deadline and was stopped")
    if process.returncode != expected:
        raise RuntimeError(f"Gallery exited {process.returncode}, expected {expected}\n{stdout}\n{stderr}")
    assert process.poll() is not None
    return stdout


with tempfile.TemporaryDirectory(prefix="desktop-", dir=args.build_root.resolve()) as directory:
    directory = Path(directory)
    settings = directory / "settings.cfg"
    run(["--continuous", "--frames", "12", "--settings", settings])
    saved = settings.read_bytes()
    assert b"version=1\n" in saved
    run(["--frames", "8", "--settings", settings])
    assert settings.read_bytes() == saved
    run(["--frames", "4", "--settings", settings, "--font", directory / "missing.ttf"], expected=4)
    assert settings.read_bytes() == saved
    corrupt = b"version=future\n"
    settings.write_bytes(corrupt)
    run(["--continuous", "--frames", "8", "--settings", settings])
    assert settings.read_bytes() == corrupt
    run(["--frames", "0"], expected=64)
    assert "--font" in run(["--help"])
print("PASS desktop: real GL frames, settings restart, corrupt preservation, font failure, CLI and owned process exit")
