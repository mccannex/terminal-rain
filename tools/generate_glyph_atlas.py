#!/usr/bin/env python3
"""
One-time asset-generation tool: bakes the CP437 oldschool-vga-8x16 bitmap
font data (from assets/fonts/oldschool-vga-8x16-fontlist.js) into a single
24-bit uncompressed BMP glyph atlas, laid out as a 16x16 grid of 8x16 cells
(128x256 px total). Glyph N lives at cell (N % 16, N // 16).

BMP is used (rather than PNG) so the runtime can load it with SDL2's
built-in SDL_LoadBMP and needs no additional image-decoding library.
White pixels (255,255,255) are glyph foreground; black (0,0,0) is
background, meant to be color-keyed transparent at load time.

Run manually whenever the source font data changes:
    python3 tools/generate_glyph_atlas.py
"""

import re
import struct
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
FONTLIST_JS = REPO_ROOT / "assets" / "fonts" / "oldschool-vga-8x16-fontlist.js"
OUTPUT_BMP = REPO_ROOT / "assets" / "fonts" / "cp437_vga_8x16.bmp"

GLYPH_W = 8
GLYPH_H = 16
GRID_COLS = 16
GRID_ROWS = 16
NUM_GLYPHS = GRID_COLS * GRID_ROWS  # 256
ATLAS_W = GLYPH_W * GRID_COLS       # 128
ATLAS_H = GLYPH_H * GRID_ROWS       # 256


def parse_fontlist(text: str):
    """Extract the 256 sixteen-byte glyph rows, in CP437 index order."""
    glyphs = []
    for match in re.finditer(r"\[((?:0x[0-9a-fA-F]{2},?\s*){16})\]", text):
        bytes_str = match.group(1)
        rows = [int(b, 16) for b in re.findall(r"0x[0-9a-fA-F]{2}", bytes_str)]
        glyphs.append(rows)
    if len(glyphs) != NUM_GLYPHS:
        raise ValueError(f"expected {NUM_GLYPHS} glyphs, parsed {len(glyphs)}")
    return glyphs


def write_bmp(path: Path, width: int, height: int, pixels):
    """pixels: list of rows (top-to-bottom), each a list of (r,g,b) tuples."""
    row_size = (width * 3 + 3) & ~3  # rows padded to 4-byte boundary
    pixel_data_size = row_size * height
    file_size = 54 + pixel_data_size

    header = struct.pack(
        "<2sIHHI", b"BM", file_size, 0, 0, 54
    )
    dib_header = struct.pack(
        "<IiiHHIIiiII",
        40,          # DIB header size
        width,
        height,      # positive = bottom-up rows
        1,           # planes
        24,          # bpp
        0,           # no compression
        pixel_data_size,
        2835, 2835,  # ~72 DPI
        0, 0,        # palette
    )

    with open(path, "wb") as f:
        f.write(header)
        f.write(dib_header)
        # BMP rows are stored bottom-to-top
        for row in reversed(pixels):
            row_bytes = bytearray()
            for (r, g, b) in row:
                row_bytes += bytes((b, g, r))  # BMP is BGR
            row_bytes += bytes(row_size - len(row_bytes))
            f.write(row_bytes)


def main():
    text = FONTLIST_JS.read_text(encoding="utf-8")
    glyphs = parse_fontlist(text)

    white = (255, 255, 255)
    black = (0, 0, 0)

    pixels = [[black] * ATLAS_W for _ in range(ATLAS_H)]

    for glyph_index, rows in enumerate(glyphs):
        cell_col = glyph_index % GRID_COLS
        cell_row = glyph_index // GRID_COLS
        origin_x = cell_col * GLYPH_W
        origin_y = cell_row * GLYPH_H

        for y, row_byte in enumerate(rows):
            for x in range(GLYPH_W):
                bit = (row_byte >> (7 - x)) & 1
                pixels[origin_y + y][origin_x + x] = white if bit else black

    write_bmp(OUTPUT_BMP, ATLAS_W, ATLAS_H, pixels)
    print(f"Wrote {OUTPUT_BMP} ({ATLAS_W}x{ATLAS_H}, {len(glyphs)} glyphs)")


if __name__ == "__main__":
    main()
