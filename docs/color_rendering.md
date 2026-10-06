# Color rendering

The shared rain palette is encoded sRGB. Color normalization does not change
its RGB values or perform linear-light blending; palette tuning is a separate
step. Display calibration, brightness and desktop color-management settings can
still affect appearance.

## macOS

After SDL renderer creation, the saver explicitly assigns `kCGColorSpaceSRGB`
to its Metal presentation layer. The pixel format remains unchanged; switching
to an sRGB GPU pixel format would also change transfer/blending behavior and is
not needed to declare the meaning of these already-encoded bytes. No HDR/EDR
mode is enabled. Non-Metal fallback rendering keeps its existing path.

Real ScreenSaverEngine A/B testing on the M1 Air showed less saturated greens
with the explicit tag. Both captures carried the same display ICC profile and
both hosts reported EDR disabled. An ordinary-window comparison did not show
the difference, so validation must include actual screensaver hosting.

## Windows and Linux

Inspection of bundled SDL 2.30.9 confirms the Windows Direct3D 11 backend uses
an 8-bit `DXGI_FORMAT_B8G8R8A8_UNORM` swap chain, without opting into HDR.
Windows treats this conventional SDR output as sRGB:
https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range
No Windows renderer change is needed for the initial sRGB baseline.

SDL2's Linux paths do not attach an explicit Wayland color-management image
description. They supply conventional SDR RGB and rely on the compositor and
display configuration. X11 does not provide a uniform compositor-managed color
contract. This source inspection does not establish matching physical colors on
every Linux installation; native calibrated comparisons remain useful, especially
with wide-gamut/HDR displays. Do not add gamma conversion to the palette or GPU
textures merely to label encoded sRGB content.

## README preview

The README uses a freshly rendered 200-frame APNG with an embedded sRGB ICC
profile and the approved palette. The old GIF is retained as a historical
reference for the original palette. `tools/make_preview.py` also attaches an
sRGB profile to newly generated APNG output and uses an exact indexed palette
when possible, with RGB fallback when the palette is larger.

## Focused validation

- Release arm64 saver build and ad-hoc signature verification.
- Portable rendering and zombie timing regression tests.
- Native Metal test verifies the sRGB layer tag, unchanged pixel format/dynamic
  range, and unchanged `(150, 255, 125)` drawing/readback bytes.
- Native accelerated rendering/readback and synthetic-reset tests.
- APNG decoding/profile/frame-count and exact RGB comparison against captured frames.

The previously performed real-host A/B test exercised the same Metal layer
property assignment. Native Windows/Linux color matching and non-Metal macOS
fallback color management have not been claimed verified by this change.

## Approved palette

The six head and trail RGB pairs are explicit tables in `core/stream_field.h`,
ordered fastest to slowest. They started from equal OKLab head-lightness steps
with graded trail lightness and were refined through actual screensaver tests:
stronger separation between the faster tiers, a lift for the darkest tiers,
a slightly brighter fastest tier, and a final small lift to its trail. These
visual adjustments mean the final ramp is not a mathematically uniform OKLab
curve. Movement timing is unchanged. The original palette is commented beside
the tables for reference. The user approved the installed local rendering.
