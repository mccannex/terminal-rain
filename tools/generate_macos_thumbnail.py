#!/usr/bin/env python3
"""Generates platform/macos/thumbnail.png and thumbnail@2x.png -- the static
image System Settings' Screen Saver picker grid shows per module. Unlike the
larger "selected" preview canvas (which does animate the real
ScreenSaverView), the grid tile for a legacy .saver bundle does not render
the view at all -- it just displays these two loose PNGs from the bundle's
Contents/Resources if present (90x58 @1x / 180x116 @2x is the convention
other .saver bundles use), falling back to a generic icon otherwise.

Uses the same TerminalVector.ttf font as the real glyph atlas
(core/glyph_atlas_data.h, see generate_glyph_atlas.py) so the thumbnail
actually resembles the running screensaver, not a generic mockup. Re-run this
after making visible changes to the animation's look.

Easter egg: the head (brightest, lowest) glyph of each stream across a run of
adjacent columns is forced to spell WORD left to right, instead of being a
random character like every other cell -- everything else about those
columns (trailing dim glyphs above the head, stream length, and critically
the head's row) stays exactly as randomized/staggered as any other column,
matching real StreamField streams (which are never all the same length) --
forcing every letter onto one dead-straight row would look like nothing the
actual app ever renders.

Only WORD's own letters are ever drawn at full head brightness -- columns
outside the word (used purely to center/offset it within the available
width) render as fully dim streams with no bright head of their own, so
there's no stray highlighted character competing for attention next to the
word.

Each letter's row is hand-set via WORD_HEAD_ROWS below (0 = top row,
rows - 1 = bottom row) rather than randomized -- only the trailing dim tail
above each letter is still randomized, for texture.
"""
import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
FONT_PATH = ROOT / "assets" / "fonts" / "TerminalVector.ttf"
OUT_DIR = ROOT / "platform" / "macos"

GLYPHS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
WORD = "TERMINAL"

# Row each letter's head lands on: 0 = top row, rows - 1 = bottom row (rows
# is 7 at both the @1x and @2x sizes below). One entry per WORD character --
# edit freely to re-art-direct the stagger. Alternates between two rows so
# the word reads left to right at a glance while still looking like
# staggered rain. Kept off the bottom row: the picker tile's rounded corners
# and edge cropping clip anything drawn there.
WORD_HEAD_ROWS = [4, 5, 4, 5, 4, 5, 4, 5]

# Base head color, matching kHeadR/G/B in core/stream_field.h, so the tile
# uses the running saver's green rather than a near-white.
HEAD_COLOR = (150, 255, 125)

# Tail brightness, as a fraction of HEAD_COLOR: a linear gradient from
# TAIL_MAX_FADE right behind the head down to TAIL_MIN_FADE at the tail's
# far end. Ambient (letterless) columns peak lower, at a random value in
# AMBIENT_MAX_FADE, so they never outshine a word column's tail.
TAIL_MAX_FADE = 0.45
TAIL_MIN_FADE = 0.06
AMBIENT_MAX_FADE = (0.25, 0.35)
assert len(WORD_HEAD_ROWS) == len(WORD)

BASE_WIDTH = 90
BASE_HEIGHT = 58
BASE_CELL = 8


def render(scale: int) -> Image.Image:
    width, height, cell = BASE_WIDTH * scale, BASE_HEIGHT * scale, BASE_CELL * scale
    img = Image.new("RGB", (width, height), (0, 0, 0))
    draw = ImageDraw.Draw(img)
    font = ImageFont.truetype(str(FONT_PATH), int(cell * 1.15))

    random.seed(42)  # deterministic output -- re-running without a source
                      # change shouldn't churn the committed PNGs in git diffs
    rows = height // cell
    # Center the word horizontally, then fill outward with ambient columns
    # on both sides. Edge columns may be partly cut off by the image border.
    word_left = (width - len(WORD) * cell) // 2
    first_col = -(-word_left // cell)  # ceil: enough columns to reach x = 0
    last_col = len(WORD) + -(-(width - word_left - len(WORD) * cell) // cell)

    for col in range(-first_col, last_col):
        x = word_left + col * cell
        word_index = col
        is_word_col = 0 <= word_index < len(WORD)

        if is_word_col:
            head_row = WORD_HEAD_ROWS[word_index]
            # Tail reaches the top edge (or one row past it), so the full
            # brightness gradient is visible rather than cut off mid-fade.
            stream_len = head_row + 1 + random.randint(0, 1)
            start_row = head_row - stream_len + 1
        else:
            stream_len = random.randint(4, rows)
            start_row = random.randint(-4, max(0, rows - stream_len))

        peak_fade = TAIL_MAX_FADE if is_word_col else random.uniform(*AMBIENT_MAX_FADE)

        for i in range(stream_len):
            row = start_row + i
            y = row * cell
            if y < -cell or y > height:
                continue
            # Only a word column's own head glyph gets full HEAD_COLOR.
            # Everything else is a tail glyph, fading linearly from the
            # column's peak right behind the head (or the tip, for an
            # ambient column) to TAIL_MIN_FADE at the far end.
            is_word_head = is_word_col and i == stream_len - 1
            if is_word_head:
                glyph = WORD[word_index]
                fade = 1.0
            else:
                glyph = random.choice(GLYPHS)
                tail_len = stream_len - 1 if is_word_col else stream_len
                t = (i + 1) / tail_len  # 1.0 at the brightest tail cell
                fade = TAIL_MIN_FADE + (peak_fade - TAIL_MIN_FADE) * t
            color = tuple(int(c * fade) for c in HEAD_COLOR)
            draw.text((x, y), glyph, font=font, fill=color)
    return img


if __name__ == "__main__":
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    render(1).save(OUT_DIR / "thumbnail.png")
    render(2).save(OUT_DIR / "thumbnail@2x.png")
    print(f"wrote {OUT_DIR / 'thumbnail.png'} and thumbnail@2x.png")
