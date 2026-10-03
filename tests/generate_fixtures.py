#!/usr/bin/env python3
"""Generate deterministic PNG test fixtures for the internal PNG decoder.

Uses only the Python standard library (zlib, struct).
"""
import os
import struct
import sys
import zlib


def chunk(tag: bytes, data: bytes) -> bytes:
    c = struct.pack(">I", len(data)) + tag + data
    c += struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    return c


def make_png(path, width, height, color_type, bit_depth, rows, palette=None,
             interlace=False):
    """rows: list of row-bytes (already packed per pixel, big-endian samples)."""
    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", width, height, bit_depth, color_type, 0, 0,
                       1 if interlace else 0)
    out = sig + chunk(b"IHDR", ihdr)
    if palette:
        out += chunk(b"PLTE", bytes(palette))
    raw = bytearray()
    for r in rows:
        raw.append(0)  # filter: None
        raw.extend(r)
    out += chunk(b"IDAT", zlib.compress(bytes(raw)))
    out += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(out)


def rgba_rows(width, height, pixels):
    """pixels: list of (r,g,b,a) per pixel, row-major. -> RGBA8 rows."""
    rows = []
    for y in range(height):
        row = bytearray()
        for x in range(width):
            r, g, b, a = pixels[y * width + x]
            row += bytes((r, g, b, a))
        rows.append(bytes(row))
    return rows


def main():
    d = os.path.join(os.path.dirname(__file__), "data")
    os.makedirs(d, exist_ok=True)
    ok = True

    def check(name):
        nonlocal ok
        p = os.path.join(d, name)
        if not os.path.exists(p):
            print(f"FAIL: {name} not created")
            ok = False
        else:
            print(f"OK: {name} ({os.path.getsize(p)} bytes)")

    # 2x2 RGBA8
    make_png(os.path.join(d, "rgba8_2x2.png"), 2, 2, 6, 8,
             rgba_rows(2, 2, [(255, 0, 0, 255), (0, 255, 0, 128),
                              (0, 0, 255, 64), (255, 255, 0, 32)]))
    check("rgba8_2x2.png")

    # 2x2 RGB8
    rows = []
    for y in range(2):
        row = bytearray()
        for x in range(2):
            row += bytes((10 + 30 * (y * 2 + x), 20 + 30 * (y * 2 + x),
                          30 + 30 * (y * 2 + x)))
        rows.append(bytes(row))
    make_png(os.path.join(d, "rgb8_2x2.png"), 2, 2, 2, 8, rows)
    check("rgb8_2x2.png")

    # 2x2 palette
    make_png(os.path.join(d, "palette8_2x2.png"), 2, 2, 3, 8,
             [b"\x00\x01", b"\x01\x00"],
             palette=(1, 2, 3, 4, 5, 6))
    check("palette8_2x2.png")

    # 2x2 gray8
    make_png(os.path.join(d, "gray8_2x2.png"), 2, 2, 0, 8, [b"\x0a\x14", b"\x1e\xc8"])
    check("gray8_2x2.png")

    # 2x2 gray+alpha8
    make_png(os.path.join(d, "grayalpha8_2x2.png"), 2, 2, 4, 8,
             [b"\x0a\xff\x14\x80", b"\x1e\x40\xc8\x20"])
    check("grayalpha8_2x2.png")

    # 2x2 RGBA16 (high byte kept)
    def px16(r, g, b, a):
        return struct.pack(">HHHH", r, g, b, a)
    make_png(os.path.join(d, "rgba16_2x2.png"), 2, 2, 6, 16,
             [px16(0x1234, 0x5678, 0x9ABC, 0xFF00) + px16(0x0001, 0x0102, 0x0203, 0x0304),
              px16(0x1111, 0x2222, 0x3333, 0x4444) + px16(0x5555, 0x6666, 0x7777, 0x8888)])
    check("rgba16_2x2.png")

    # 4x4 RGBA8 exercising all 5 row filters.
    # Build the "true" image, then encode rows with filters 0..3 and check
    # that the decoder reverses them.
    w = h = 4
    true_rows = []
    for y in range(h):
        row = bytearray()
        for x in range(w):
            row += bytes(((x * 60 + y * 30) % 256, (x * 40 + y * 50) % 256,
                          (x * 20 + y * 70) % 256, (x + y * 64) % 256))
        true_rows.append(bytes(row))

    bpp = 4

    def paeth(a, b, c):
        p = a + b - c
        pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
        if pa <= pb and pa <= pc:
            return a
        return b if pb <= pc else c

    # PNG stores stored[x] = original[x] - prediction, where the prediction
    # is computed from already-reconstructed (i.e. original) samples.
    filtered = bytearray()
    prev = bytes(w * bpp)
    for y in range(h):
        ftype = y % 5  # row y uses filter y%5 (0..3 for 4 rows)
        orig = true_rows[y]
        row = bytearray(orig)
        if ftype == 1:  # Sub
            for i in range(bpp, len(row)):
                row[i] = (row[i] - orig[i - bpp]) & 0xFF
        elif ftype == 2:  # Up
            for i in range(len(row)):
                row[i] = (row[i] - prev[i]) & 0xFF
        elif ftype == 3:  # Average
            for i in range(len(row)):
                a = orig[i - bpp] if i >= bpp else 0
                row[i] = (row[i] - (a + prev[i]) // 2) & 0xFF
        elif ftype == 4:  # Paeth
            for i in range(len(row)):
                a = orig[i - bpp] if i >= bpp else 0
                c = prev[i - bpp] if i >= bpp else 0
                row[i] = (row[i] - paeth(a, prev[i], c)) & 0xFF
        filtered.append(ftype)
        filtered.extend(row)
        prev = orig

    # Store expected "true" pixels for the C++ test.
    with open(os.path.join(d, "filters4x4_expected.txt"), "w") as f:
        for y in range(h):
            f.write(" ".join(str(b) for b in true_rows[y]) + "\n")

    raw = bytes(filtered)
    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    out = sig + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")
    with open(os.path.join(d, "filters4x4.png"), "wb") as f:
        f.write(out)
    check("filters4x4.png")

    # Corrupt: valid header but truncated mid-IDAT.
    with open(os.path.join(d, "rgba8_2x2.png"), "rb") as f:
        data = f.read()
    with open(os.path.join(d, "truncated.png"), "wb") as f:
        f.write(data[: len(data) // 2])
    check("truncated.png")

    # Not a PNG at all.
    with open(os.path.join(d, "notapng.png"), "wb") as f:
        f.write(b"this is definitely not a png file")
    check("notapng.png")

    # CRC-corrupted: flip one byte inside IDAT (after the length field).
    with open(os.path.join(d, "rgba8_2x2.png"), "rb") as f:
        data = bytearray(f.read())
    # Find IDAT chunk start.
    i = data.find(b"IDAT")
    data[i + 8] ^= 0xFF  # corrupt first data byte
    with open(os.path.join(d, "crc_corrupt.png"), "wb") as f:
        f.write(bytes(data))
    check("crc_corrupt.png")

    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
