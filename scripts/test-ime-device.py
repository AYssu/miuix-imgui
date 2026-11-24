#!/usr/bin/env python3
"""Run the IME regression target on one explicitly selected, rooted Android device."""
import argparse
from pathlib import Path
import queue
import re
import shlex
import subprocess
import threading
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--serial", required=True)
parser.add_argument("--build", type=Path, default=Path(__file__).resolve().parents[1] / "build/arm64-v8a")
parser.add_argument("--mode", choices=("all", "input", "cancel", "disconnect", "direct", "install-disconnect", "monitor-stop", "collision"), default="all")
args = parser.parse_args()
adb = ["adb", "-s", args.serial]
package = "skui.android.imehelper"
remote = "/data/local/tmp/skui-ime-tests"


def shell(command, root=False):
    if root:
        command = "su -c " + shlex.quote(command)
    return subprocess.run(adb + ["shell", command], capture_output=True, text=True, timeout=45)


def installed():
    result = shell("pm path " + package, True)
    return result.stdout.strip()


def processes():
    return shell("pidof skui-ime-tests; pidof skui-ime; pidof sk-imgui-ui; pidof " + package, True).stdout.strip()


def clean():
    deadline = time.monotonic() + 40
    while time.monotonic() < deadline:
        if not processes() and not installed():
            return
        time.sleep(0.2)
    raise AssertionError("Input processes/package still present; inspect the device before continuing")


def run(mode, collision=False):
    print("RUN", mode, flush=True)
    actual_mode = "input" if mode in ("install-disconnect", "monitor-stop") else mode
    command = ("SKUI_IME_DIAGNOSTICS=1 " if mode == "install-disconnect" else "") + remote + " " + actual_mode
    child = subprocess.Popen(adb + ["shell", "su -c " + shlex.quote(command)],
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
    messages = queue.Queue()

    def read():
        for line in child.stdout:
            messages.put(line)
        messages.put(None)

    threading.Thread(target=read, daemon=True).start()
    lines, stage, renderer_pid = [], 0, None
    deadline = time.monotonic() + 80
    try:
        while time.monotonic() < deadline:
            try:
                line = messages.get(timeout=1)
            except queue.Empty:
                continue
            if line is None:
                break
            lines.append(line)
            if line.startswith(("[TEST]", "[IME APK]")):
                print(line, end="", flush=True)
            match = re.search(r"\[TEST\] pid=(\d+)", line)
            if match:
                renderer_pid = match.group(1)
            if mode == "install-disconnect" and "[IME APK] installing" in line:
                assert renderer_pid
                assert shell("kill -KILL " + renderer_pid, True).returncode == 0
            if mode == "monitor-stop" and "[TEST] keyboard=" in line:
                rows = shell("ps -A -o PID,PPID,ARGS", True).stdout.splitlines()
                monitors = []
                for row in rows:
                    fields = row.split(None, 2)
                    if len(fields) == 3 and fields[1] == renderer_pid and "--ime-apk-host" in fields[2]:
                        monitors.append(fields[0])
                assert len(monitors) == 1, monitors
                assert shell("kill -TERM " + monitors[0], True).returncode == 0
            if mode in ("input", "direct") and not collision:
                if stage == 0 and "[TEST] keyboard=" in line:
                    if mode == "input":
                        assert installed(), "Fallback did not install the APK"
                        launcher = shell("cmd package query-activities --brief -a android.intent.action.MAIN "
                                         "-c android.intent.category.LAUNCHER -p " + package, True)
                        assert launcher.returncode == 0, launcher.stderr
                        assert "skui.android.ImeActivity" not in launcher.stdout, launcher.stdout
                    else:
                        assert not installed(), "Direct input unexpectedly installed an APK"
                    assert shell("input text abc; input keyevent KEYCODE_ENTER").returncode == 0
                    stage = 1
                elif stage == 1 and "[TEST] text=你好😀abc " in line:
                    assert shell("input keyevent KEYCODE_DEL; input keyevent KEYCODE_ENTER").returncode == 0
                    stage = 2
                elif stage == 2 and "[TEST] text=你好😀ab " in line:
                    assert shell("input keyevent KEYCODE_BACK").returncode == 0
                    stage = 3
        result = child.wait(timeout=10)
        transcript = "".join(lines)
        if mode != "install-disconnect" and not collision:
            assert "[IME APK]" not in transcript and "java.lang." not in transcript, transcript
        if collision:
            assert result != 0 and "已有同名输入辅助包" in transcript, transcript
        elif mode in ("install-disconnect", "monitor-stop"):
            assert result != 0 and renderer_pid, transcript
        else:
            assert result == 0, transcript
            if mode in ("input", "direct"):
                assert "done=1 final=你好😀ab\n" in transcript, transcript
        if not collision:
            clean()
        print("PASS", mode, flush=True)
    finally:
        # Only the PID printed by this test is eligible for emergency cleanup.
        if child.poll() is None:
            if renderer_pid:
                shell("kill -TERM " + renderer_pid, True)
            child.terminate()


assert shell("id -u", True).stdout.strip() == "0", "Root required"
assert not processes(), "Existing input/demo instance: stop it before device tests"
assert not installed(), "An existing helper package must be preserved; refusing to test"
subprocess.run(adb + ["push", str(args.build / "skui-ime-tests"), remote], check=True)
assert shell("chmod 755 " + remote, True).returncode == 0
modes = ("input", "cancel", "disconnect", "install-disconnect", "monitor-stop", "direct", "collision") if args.mode == "all" else (args.mode,)
for mode in modes:
    if mode != "collision":
        run(mode)
        continue
    # Install our own fixture, then prove the runtime does not replace/remove it.
    fixture = "/data/local/tmp/skui-ime-existing-test.apk"
    subprocess.run(adb + ["push", str(args.build / "generated/ime/skui-ime-helper.apk"), fixture], check=True)
    user = shell("am get-current-user", True).stdout.strip()
    assert user.isdigit()
    assert shell("pm install --user " + user + " " + fixture, True).returncode == 0
    before = installed()
    assert before
    try:
        run("input", collision=True)
        assert installed() == before, "Pre-existing package was changed"
    finally:
        shell("pm uninstall --user " + user + " " + package, True)
        shell("rm -f " + fixture, True)
    clean()
    print("PASS existing-package preservation", flush=True)
assert shell("rm -f " + remote, True).returncode == 0
print("PASS all requested cases; no helper package or test processes remain", flush=True)
