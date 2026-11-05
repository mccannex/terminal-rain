# Stream density

Fullscreen stream arrivals use one baseline: 20 streams per second across
160 glyph columns (1280 pixels with the native eight-pixel glyph width).
Each field derives its rate from its actual number of glyph columns:

```
streams per second = baseline streams per second * columns / baseline columns
```

Column count includes content scaling, so enlarged glyphs do not inadvertently
increase density per glyph cell. Height does not multiply the arrival rate:
streams on taller surfaces already remain visible longer. Scaling arrivals by
area would count that increase twice. The area-scaled active-stream capacity
remains a safety limit, including invisible heads waiting for trail cleanup.

`core/stream_spawn.h` holds the baseline and shared 50-ms simulation cadence.
Integer credit carries fractional arrivals between ticks; for example, 1080p
alternates one and two arrivals per tick instead of rounding 1.5 down to one.
Arrivals blocked by the stream cap are discarded instead of accumulating a
burst. The compact-trail Windows thumbnail retains its existing one-per-tick
rate, 600-tick warmup, and preview trail tuning.

Previously only capacity scaled with area; every surface spawned at most one
stream per tick. Ordinary fullscreen fields did not reach the capacity, so
larger surfaces looked sparser despite their larger caps.

## Windows validation, 2026-11-04

The MinGW Release build passed all four CTest suites, including native
Direct3D 11 rendering and fractional-rate regression checks. The genuine SDL
draw-failure check now waits for the first glyph arrival on its small surface
rather than assuming a stream spawns on the first tick.

A temporary lifecycle-only probe ran three seeds (1, 7, 12345), with 4,000
warmup ticks and 8,000 sampled ticks per seed. It used the actual spawn/movement
methods without rendering; rendering RNG consumption and perceived brightness
are not covered by this probe. Counts are averages of visible heads, excluding
off-screen cleanup heads, per million surface pixels at native glyph size.

| Surface | Previous heads/MP | New heads/MP |
| --- | ---: | ---: |
| 1280x720 | 226.2 | 226.2 |
| 1920x1080 | 151.1 | 226.6 |
| 3840x2160 | 75.7 | 228.1 |
| 1080x1920 | 269.0 | 227.3 |

The probe did not reach the capacity on any tested surface. These results
validate arrival/lifecycle scaling, not exact pixel coverage.

The rebuilt Windows MinGW `.scr` then ran on the real desktop for 71.3 seconds
and exited normally with code zero. After being asked to compare density across
the monitors, the user approved its appearance ("yep, that looks pretty good").
All four Windows CTest suites passed immediately before that launch. The tested
binary's SHA-256 was
`f16faad71684605bf5052dc99956a97820000aa4a12063c9a5e24157f5a88ee3`.

During handoff, the same approved binary and its three MinGW runtime DLLs were
copied to `build-installed-windows` and selected as the installed saver. Its
installed hash matched the tested binary, it loaded with only the Windows
system PATH, and the saver timeout remained 300 seconds. A direct `/s` launch
was then confirmed on all four screens and dismissed normally with code zero.

A diagnostic launched through the `.scr` shell association had unintentionally
forced `/S` and hidden the first window, showing only three displays. Direct
execution with normal visibility resolved that launcher artifact; see
`docs/release_handoff.md` for the required diagnostic launch method.

On 2026-11-05, cross-platform CI run `42083438520` passed for density source
`d5ad85d`. Mac arm64/universal automated checks and user-confirmed real fullscreen
density, dismissal/cleanup and Settings coexistence also passed. Linux native
density/integration validation remains; use `docs/release_handoff.md`.
