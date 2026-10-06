#!/usr/bin/env python3
"""Encodes frames from the capture tool into a looping GIF and/or APNG for
the README's animated preview.

Build and run the capture tool first:
    cmake -S . -B build -DTERMINAL_RAIN_BUILD_TOOLS=ON
    cmake --build build --target terminal_rain_capture
    ./build/terminal_rain_capture 1280 720 180 10 1 frames.rgb

Then encode:
    python3 tools/make_preview.py frames.rgb 1280 720 preview.gif preview.png

Output format is picked by extension (.gif or .png for APNG). Both loop
forever at 20 fps (50 ms per frame, the screensaver's real rate).

Colors: the screensaver draws only a dozen or so greens plus black, so
non-crossfade frames fit a GIF's 256-color palette exactly. If the
crossfade frames push the total past 255 (one index is reserved for
transparency), the palette is built from the most common colors and the
rare blended ones map to their nearest entry.

GIF size: after the first frame, pixels that didn't change since the
previous frame are written as transparent, so each frame mostly encodes
as long runs of one index. That's what keeps the file small.

Every encode is verified by decoding the file and comparing each frame to
the source. Exact for GIF unless the palette had to be reduced; APNG is
always exact.

Requires: Pillow and numpy (`uv run --with pillow --with numpy ...`).
"""

import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageCms

FRAME_MS = 50
TRANSPARENT = 255


def load_frames(path, width, height):
    raw = np.fromfile(path, dtype=np.uint8)
    frame_bytes = width * height * 3
    if raw.size % frame_bytes:
        sys.exit(f"{path}: size isn't a whole number of {width}x{height} RGB frames")
    return raw.reshape(-1, height, width, 3)


def build_palette(frames):
    """Returns (palette RGB array, packed-color -> index lookup function)."""
    packed = (frames[..., 0].astype(np.uint32) << 16) | (frames[..., 1].astype(np.uint32) << 8) | frames[..., 2]
    colors, counts = np.unique(packed, return_counts=True)
    exact = len(colors) <= TRANSPARENT
    if not exact:
        colors = colors[np.argsort(counts)[::-1][:TRANSPARENT]]
    palette = np.stack([(colors >> 16) & 255, (colors >> 8) & 255, colors & 255], axis=1).astype(np.int32)
    print(f"  {len(np.unique(packed))} distinct colors, palette {'exact' if exact else 'reduced to 255'}")

    sorted_idx = np.argsort(colors)
    sorted_colors = colors[sorted_idx]

    def to_indices(frame):
        p = (frame[..., 0].astype(np.uint32) << 16) | (frame[..., 1].astype(np.uint32) << 8) | frame[..., 2]
        pos = np.clip(np.searchsorted(sorted_colors, p), 0, len(sorted_colors) - 1)
        idx = sorted_idx[pos]
        miss = colors[idx] != p
        if miss.any():
            # Nearest palette entry for the few blended colors not in it.
            rgb = frame[miss].astype(np.int32)
            d = ((rgb[:, None, :] - palette[None, :, :]) ** 2).sum(axis=2)
            idx[miss] = d.argmin(axis=1)
        return idx.astype(np.uint8)

    return palette, to_indices, exact


def write_gif(frames, out):
    palette, to_indices, exact = build_palette(frames)
    flat_palette = palette.astype(np.uint8).flatten().tolist()
    flat_palette += [0, 0, 0] * (256 - len(palette))

    images = []
    prev = None
    for frame in frames:
        idx = to_indices(frame)
        shown = idx.copy()
        if prev is not None:
            shown[idx == prev] = TRANSPARENT
        prev = idx
        img = Image.fromarray(shown, mode="P")
        img.putpalette(flat_palette)
        images.append(img)

    images[0].save(out, save_all=True, append_images=images[1:], duration=FRAME_MS,
                   loop=0, disposal=1, transparency=TRANSPARENT, optimize=False)
    return exact


def write_apng(frames, out):
    # Indexed APNG keeps sparse rain previews compact without changing colors.
    palette, to_indices, exact = build_palette(frames)
    if exact:
        flat_palette = palette.astype(np.uint8).flatten().tolist()
        flat_palette += [0] * (768 - len(flat_palette))
        images = []
        for frame in frames:
            img = Image.fromarray(to_indices(frame), mode="P")
            img.putpalette(flat_palette)
            images.append(img)
    else:
        images = [Image.fromarray(f, mode="RGB") for f in frames]
    profile = ImageCms.ImageCmsProfile(ImageCms.createProfile("sRGB")).tobytes()
    images[0].save(out, save_all=True, append_images=images[1:], duration=FRAME_MS,
                   loop=0, optimize=True, icc_profile=profile)


def verify(frames, out):
    """Decodes the written file and counts frames that don't match the source."""
    img = Image.open(out)
    bad = 0
    n = 0
    for i in range(len(frames)):
        img.seek(i)
        decoded = np.asarray(img.convert("RGB"))
        if not np.array_equal(decoded, frames[i]):
            bad += 1
        n += 1
    return n, bad


def main():
    if len(sys.argv) < 5:
        sys.exit(__doc__)
    src, width, height = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    frames = load_frames(src, width, height)
    print(f"{len(frames)} frames, {width}x{height}, {len(frames) * FRAME_MS / 1000:.1f}s loop")

    for out in sys.argv[4:]:
        ext = Path(out).suffix.lower()
        print(out)
        if ext == ".gif":
            write_gif(frames, out)
        elif ext == ".png":
            write_apng(frames, out)
        else:
            sys.exit(f"{out}: use .gif or .png")
        n, bad = verify(frames, out)
        size = Path(out).stat().st_size / 1e6
        print(f"  {size:.1f} MB, {n} frames decoded, {bad} differ from source")


if __name__ == "__main__":
    main()
