#!/usr/bin/env python3
"""Generates src/macos_saver/thumbnail.png and thumbnail@2x.png -- the static
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
OUT_DIR = ROOT / "src" / "macos_saver"

GLYPHS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
WORD = "GLYPHRAIN"

# Row each letter's head lands on: 0 = top row, rows - 1 = bottom row (rows
# is 7 at both the @1x and @2x sizes below). One entry per WORD character --
# edit freely to re-art-direct the stagger. Clustered near the bottom
# (rows 4-6) rather than spanning the full height, so the word stays close
# enough to a straight line to actually read left to right at a glance,
# while still looking like staggered rain rather than a ruler-straight banner.
WORD_HEAD_ROWS = [5, 6, 4, 6, 5, 6, 4, 5, 6]
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
    cols = width // cell
    rows = height // cell
    word_start_col = max(0, (cols - len(WORD)) // 2)

    for col in range(cols):
        x = col * cell
        word_index = col - word_start_col
        is_word_col = 0 <= word_index < len(WORD)

        if is_word_col:
            head_row = WORD_HEAD_ROWS[word_index]
            # Tail length above the fixed head is still randomized, for
            # texture -- some tails are allowed to run off the top edge.
            stream_len = random.randint(4, head_row + 5)
            start_row = head_row - stream_len + 1
        else:
            stream_len = random.randint(4, rows)
            start_row = random.randint(-4, max(0, rows - stream_len))

        for i in range(stream_len):
            row = start_row + i
            y = row * cell
            if y < -cell or y > height:
                continue
            # Only a word column's own head glyph ever gets full brightness.
            # A word column's *other* (trailing) glyphs still fade by
            # distance from its real head, same as the running app. An
            # ambient column has no head at all -- every glyph in it gets a
            # flat, low, jittered dimness so nothing in it is ever the
            # locally-brightest cell competing for attention next to a
            # letter (the distance-from-head formula would otherwise put its
            # brightest value exactly at the tip, i.e. right next to a
            # letter).
            is_word_head = is_word_col and i == stream_len - 1
            if is_word_head:
                glyph = WORD[word_index]
                color = (200, 255, 200)  # bright head, matches StreamField's head color
            elif is_word_col:
                glyph = random.choice(GLYPHS)
                fade = max(0.15, 1.0 - (stream_len - 1 - i) / stream_len)
                color = (int(60 * fade), int(220 * fade), int(110 * fade))
            else:
                glyph = random.choice(GLYPHS)
                fade = random.uniform(0.15, 0.4)
                color = (int(60 * fade), int(220 * fade), int(110 * fade))
            draw.text((x, y), glyph, font=font, fill=color)
    return img


if __name__ == "__main__":
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    render(1).save(OUT_DIR / "thumbnail.png")
    render(2).save(OUT_DIR / "thumbnail@2x.png")
    print(f"wrote {OUT_DIR / 'thumbnail.png'} and thumbnail@2x.png")
