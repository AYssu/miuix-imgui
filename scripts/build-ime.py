#!/usr/bin/env python3
"""Embed the direct DEX and a signed, launcher-less fallback APK in the ELF."""
import argparse
from pathlib import Path
import shutil
import subprocess
import zipfile

parser = argparse.ArgumentParser()
for option in ("javac", "d8", "android-jar", "source", "output", "activity", "manifest"):
    parser.add_argument("--" + option, required=True)
args = parser.parse_args()
out = Path(args.output)
classes = out / "classes"
shutil.rmtree(classes, ignore_errors=True)
classes.mkdir(parents=True)
subprocess.run([args.javac, "--release", "8", "-cp", args.android_jar,
                "-d", str(classes), args.source, args.activity], check=True)
subprocess.run([args.d8, "--min-api", "28", "--lib", args.android_jar,
                "--output", str(out), *map(str, sorted(classes.rglob("*.class")))], check=True)
build_tools = Path(args.d8).parent
unsigned = out / "ime-unsigned.apk"
aligned = out / "ime-aligned.apk"
apk = out / "skui-ime-helper.apk"
subprocess.run([str(build_tools / "aapt"), "package", "-f", "-M", args.manifest,
                "-I", args.android_jar, "-F", str(unsigned)], check=True)
with zipfile.ZipFile(unsigned, "a") as archive:
    archive.write(out / "classes.dex", "classes.dex")
subprocess.run([str(build_tools / "zipalign"), "-f", "4", str(unsigned), str(aligned)], check=True)
key = out / "ime-signing.p12"
if not key.exists():
    subprocess.run(["keytool", "-genkeypair", "-keystore", str(key), "-storetype", "PKCS12",
                    "-storepass", "skui-build-key", "-keypass", "skui-build-key", "-alias", "skui",
                    "-keyalg", "RSA", "-keysize", "2048", "-validity", "3650",
                    "-dname", "CN=SKUI Temporary Input Bridge"], check=True)
    key.chmod(0o600)
subprocess.run([str(build_tools / "apksigner"), "sign", "--ks", str(key), "--ks-key-alias", "skui",
                "--ks-pass", "pass:skui-build-key", "--key-pass", "pass:skui-build-key",
                "--out", str(apk), str(aligned)], check=True)
subprocess.run([str(build_tools / "apksigner"), "verify", str(apk)], check=True)
lines = ["#pragma once", "namespace skui_ime_data {"]
for name, artifact in (("dex", out / "classes.dex"), ("apk", apk)):
    data = artifact.read_bytes()
    lines.append(f"inline constexpr unsigned char {name}[] = {{")
    for i in range(0, len(data), 20):
        lines.append(",".join(f"0x{x:02x}" for x in data[i:i + 20]) + ",")
    lines.append("};")
lines.extend(["}", ""])
(out / "skui_ime_dex.h").write_text("\n".join(lines))
