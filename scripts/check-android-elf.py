#!/usr/bin/env python3
"""Verify the shipped arm64 ELF's load segments without a device/toolchain."""
from pathlib import Path
import struct
import sys


def check(filename):
    data = Path(filename).read_bytes()
    if data[:6] != b"\x7fELF\x02\x01":
        raise ValueError(f"{filename}: expected little-endian ELF64")
    if struct.unpack_from("<H", data, 18)[0] != 183:
        raise ValueError(f"{filename}: expected AArch64")
    offset = struct.unpack_from("<Q", data, 32)[0]
    entry_size, count = struct.unpack_from("<HH", data, 54)
    loads = 0
    if entry_size < 56 or offset + entry_size * count > len(data):
        raise ValueError(f"{filename}: invalid program header table")
    for index in range(count):
        entry = offset + index * entry_size
        if struct.unpack_from("<I", data, entry)[0] != 1:  # PT_LOAD
            continue
        loads += 1
        file_offset, address = struct.unpack_from("<QQ", data, entry + 8)
        alignment = struct.unpack_from("<Q", data, entry + 48)[0]
        if alignment < 16384 or (address - file_offset) % 16384:
            raise ValueError(f"{filename}: load segment is not 16 KiB aligned")
    if not loads:
        raise ValueError(f"{filename}: no load segments")
    print(f"PASS ELF: {filename}, AArch64, {loads} load segments aligned to 16 KiB")


if len(sys.argv) < 2:
    sys.exit("Usage: check-android-elf.py <arm64-elf> [<arm64-elf> ...]")
for filename in sys.argv[1:]:
    check(filename)
