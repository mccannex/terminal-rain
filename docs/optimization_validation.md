# Windows and shared-core optimization validation

Implementation/validation performed on Windows 10 (10.0.19045), 2026-10-06.
Baseline: `3b00f06def7c740c1caaeec8490c5dcd9873597e` (reviewed for issue #4).

## Changes and remaining platform work

- #7: preview's field/target now dies before its renderer. Repeated hidden
  preview-loop checks passed, with GNU linker wrappers checking ownership order.
- #6: Windows fullscreen/preview and Linux enable SDL's allow-screensaver hint
  before video initialization. Native Windows initialization leaves the test
  thread's display-required execution-state request clear. Real display timeout,
  wake behavior, and Linux power-management integration still need native checks.
- #8: target creation, initial binding/clear/restoration, drawing, and final
  copy failures stop rendering. Every field installs an SDL reset watch; it only
  sets an atomic flag, so callbacks never render or destroy resources on another
  thread. Both reset events cause clean shutdown, including when another consumer
  drains the queue. The multi-display loop also rejects partial setup instead of
  leaving a monitor uncovered. macOS's caller stops its timer/resources on failure
  rather than retrying failed setup every frame; that adaptation is unbuilt on Mac.
- #10: individual invisible glyphs and erase cells are excluded before queueing
  and sorting. Simulation, random-number consumption, visible trailing erases,
  and color/erase pass ordering are retained. No color-bucket rewrite was needed.
- #5: Linux/macOS release jobs and macOS local instructions explicitly select
  Release. All release jobs now build/run the portable regression checks; Windows
  continues to select Release with its multi-configuration build command. Linux
  and macOS CI/native builds have not been run for this working-tree change.
- #9: macOS zombie confirmation timing is unchanged; address it on a Mac.

## Executed checks

```powershell
cmake -S . -B build-mingw -DCMAKE_BUILD_TYPE=Release `
    -DTERMINAL_RAIN_BUILD_TESTS=ON -DTERMINAL_RAIN_BUILD_TOOLS=ON `
    -DFETCHCONTENT_UPDATES_DISCONNECTED=ON
cmake --build build-mingw -j 4
ctest --test-dir build-mingw --output-on-failure --timeout 30
```

Both CTest suites passed: the dummy-video/software suite and a native Windows
hidden-window suite using `direct3d11`. They cover ordinary drawing, previous-target
restoration, invalid dimensions, injected target allocation/binding failures,
runtime drawing failure, both reset types across two fields after queue draining,
restart after teardown, repeated preview shutdown, and preview input/reset behavior.
GNU-only fault injection/ownership checks run on MinGW; other toolchains run the
portable paths. Native Windows additionally checks its thread's execution-state
request and reads back nonblack pixels from an accelerated target. Reset events
were synthetic: these tests do not simulate actual GPU resource loss.

The tests require no visible fullscreen windows. On a headless machine, exclude
the optional native test with `ctest --test-dir build -C Release -LE native`.

The user visually verified the embedded Windows Screen Saver settings thumbnail
and approved the final glyph size and sustained stream density.

Still manual: physical input dismissal on multiple monitors, real idle activation
and display-off/wake transitions, and actual
device-loss recovery/shutdown. Native Linux/macOS build and runtime coverage is
pending. Existing platform context files record the handoff details.

## Windows settings thumbnail

The preview renders an aspect-matched canvas at least 320 pixels wide, scales it
with linear filtering, and primes 600 ticks before presenting. Preview-only trail
and cleanup distances scale with the canvas height; fullscreen defaults remain
unchanged. This prevents the small surface's stream cap being occupied for long
periods by heads far below the visible canvas.

A software-renderer diagnostic at 320x234, with seeds 1, 7, and 12345, measured
12,000 ticks per seed after the 600-tick warmup. With the original trail distances,
visible heads fell to zero and averaged about 9.5, with stretches below four heads
lasting 4.0-4.45 simulated seconds. Compact preview trails kept at least 32, 39,
and 35 heads visible respectively, averaging 53.7. These counts describe the
simulation, not screenshot brightness. The final preview capture took about
194 ms to initialize, prime, present, and exit on this machine.

## Optimized baseline versus complete current core

Compiler: MSYS2 UCRT64 GCC 13.2.0, `-O3 -DNDEBUG -std=c++17`. SDL: the same
statically linked Release SDL 2.30.9 library in both variants. CPU: AMD Ryzen 5
3500U. Available adapters: Radeon Vega 8 and Trigger 6 External Graphics; the
specific adapter used by SDL's Direct3D 11 renderer was not separately identified.

Each reported row uses three alternating baseline/current process runs. Values
are medians of mean elapsed time per tick within each run. The current core also
includes the failure/reset checks. Times measure `field.tick()` only, excluding
diagnostic counting/readback, final window copy/presentation, 50-ms pacing,
compositing, and macOS HID polling. Direct3D timings reflect CPU-side rendering
work/submission, not isolated GPU completion time or total application CPU usage.

| Renderer/workload | Warm-up ticks | Measured ticks/run | Baseline ms/tick | Current ms/tick | Reduction |
| --- | ---: | ---: | ---: | ---: | ---: |
| Software, 1920x1080 | 2400 | 1000 | 1.6860 | 1.5458 | 8.3% |
| Software, 3840x2160 | 1600 | 500 | 3.9711 | 3.7461 | 5.7% |
| Direct3D 11, 1920x1080 | 2400 | 1000 | 0.5029 | 0.2253 | 55.2% |

Raw per-run mean timings (baseline/current):

- Software 1080p: 1.6860/1.5662, 1.6588/1.5458, 1.7056/1.5450.
- Software 4K: 3.9618/3.7041, 4.0488/3.7461, 3.9711/3.7678.
- Direct3D 11 1080p: 0.5029/0.2308, 0.5257/0.2253, 0.4912/0.2231.

At 1080p, mean active streams remained 1014.4; glyph queue length fell from 2026.8
to 626.0 and erase rectangles from 4053.5 to 1254.7. At 4K, the corresponding glyph
counts were 2447.5/1247.4 and erase counts 4894.9/2410.8, with active streams 1224.5.

Sampled persistent-image hashes matched baseline/current at all tested dimensions:

| Workload | Combined sample signature |
| --- | --- |
| Software 256x192, scale 1, warm-up 800 / measured 400 | `c15f2f8af9b82020` |
| Software 801x603, scale 1.25, warm-up 1600 / measured 500 | `7145677fc9108700` |
| Software and Direct3D 11 1920x1080, scale 1 | `d707f66e28181ba0` |
| Software 3840x2160, scale 1 | `1b99c2f9aa554bc0` |

These are deterministic sampled comparisons with seed 12345, not an exhaustive
proof of pixel equality for every possible seed/backend. The matching signatures
and unchanged RNG consumption cover startup, below-screen heads/visible erases,
partial cells at fractional scaling, and steady animation in the tested workloads.

## Reproduction probe

This is temporary diagnostic code, not a production API. Its private-member access
allows identical deterministic seeding and queue counting for the original and
current field without adding production test hooks. Save it as
`build-mingw/perf_probe.cpp`. The benchmark deliberately does not pace frames.

```cpp
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <vector>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#define private public
#include "stream_field.h"
#undef private
#include "glyph_atlas.h"
uint64_t imageHash(SDL_Renderer* renderer, SDL_Texture* target, int w, int h) {
 std::vector<Uint32> pixels(static_cast<size_t>(w)*h);
 SDL_SetRenderTarget(renderer,target);
 if(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_RGBA8888,pixels.data(),w*4)!=0) { std::fprintf(stderr,"read error: %s\n",SDL_GetError()); std::exit(2); }
 SDL_SetRenderTarget(renderer,nullptr);
 uint64_t hash=14695981039346656037ull;
 for(auto p:pixels) { hash ^= p; hash *= 1099511628211ull; }
 return hash;
}
int main(int argc,char** argv) {
 if(argc!=6) return 2;
 int w=std::atoi(argv[1]),h=std::atoi(argv[2]),warm=std::atoi(argv[3]),frames=std::atoi(argv[4]);
 float scale=static_cast<float>(std::atof(argv[5]));
 bool native=std::getenv("TERMINAL_RAIN_PROBE_NATIVE")!=nullptr; SDL_SetMainReady(); SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER,"1"); SDL_Init(native ? SDL_INIT_VIDEO : SDL_INIT_EVENTS);
 auto* surface=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_RGBA8888);
 auto* window=native ? SDL_CreateWindow("benchmark",0,0,w,h,SDL_WINDOW_HIDDEN) : nullptr; auto* renderer=native ? SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED) : SDL_CreateSoftwareRenderer(surface); SDL_RendererInfo info{}; SDL_GetRendererInfo(renderer,&info); std::printf("backend=%s ",info.name); auto* atlas=loadGlyphAtlas(renderer);
 if(!atlas) return 1;
 uint64_t signature=0;
 {
 StreamField field(renderer,atlas,w,h,scale); field.rng_=Rng(12345);
 for(int i=0;i<warm;++i) { field.tick(); if(i==0 || i==20 || i==100 || i==400 || i==800 || i==1600 || i==warm-1) signature=signature*31+imageHash(renderer,field.targetTexture(),w,h); }
 double elapsed=0; long long glyphs=0,erases=0,active=0;
 for(int i=0;i<frames;++i) {
 auto start=std::chrono::steady_clock::now(); field.tick();
 elapsed+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 glyphs+=field.glyphDraws_.size(); erases+=field.blackCells_.size(); active+=field.activeCount_;
 if(i%100==0 || i==frames-1) signature=signature*31+imageHash(renderer,field.targetTexture(),w,h);
 }
 std::printf("%dx%d scale=%.2f tick_ms=%.4f active=%.1f glyphs=%.1f erases=%.1f signature=%016llx\n",w,h,scale,elapsed/frames,active/double(frames),glyphs/double(frames),erases/double(frames),static_cast<unsigned long long>(signature));
 }
 SDL_DestroyTexture(atlas); SDL_DestroyRenderer(renderer); SDL_FreeSurface(surface); if(window) SDL_DestroyWindow(window); SDL_Quit();
}
```

Preserve the baseline source/header together, then compile both against the same
SDL library. These commands assume the existing MinGW Release build and compiler
paths on this Windows machine; adapt paths/link dependencies for another platform.

```powershell
New-Item -ItemType Directory -Force build-mingw/baseline | Out-Null
git show 3b00f06def7c740c1caaeec8490c5dcd9873597e:core/stream_field.cpp |
    Set-Content build-mingw/baseline/stream_field.cpp
git show 3b00f06def7c740c1caaeec8490c5dcd9873597e:core/stream_field.h |
    Set-Content build-mingw/baseline/stream_field.h
foreach ($variant in @('baseline','current')) {
    $sourceDir = if ($variant -eq 'baseline') { 'build-mingw/baseline' } else { 'core' }
    & C:/msys64/ucrt64/bin/g++.exe -O3 -DNDEBUG -std=c++17 "-I$sourceDir" -Icore `
        -Ibuild-mingw/_deps/sdl2-build/include/SDL2 `
        -Ibuild-mingw/_deps/sdl2-build/include-config-release/SDL2 `
        build-mingw/perf_probe.cpp "$sourceDir/stream_field.cpp" `
        core/glyph_atlas.cpp core/glyph_atlas_data.cpp `
        build-mingw/_deps/sdl2-build/libSDL2.a `
        -lm -lkernel32 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 `
        -lversion -luuid -ladvapi32 -lsetupapi -ldinput8 -lshell32 `
        -o "build-mingw/perf_$variant.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Probe build failed' }
}
foreach ($repeat in 1..3) {
    foreach ($variant in @('baseline','current')) {
        & "./build-mingw/perf_$variant.exe" 1920 1080 2400 1000 1
    }
}
$env:TERMINAL_RAIN_PROBE_NATIVE = '1'
try {
    foreach ($repeat in 1..3) {
        foreach ($variant in @('baseline','current')) {
            & "./build-mingw/perf_$variant.exe" 1920 1080 2400 1000 1
        }
    }
} finally {
    Remove-Item Env:TERMINAL_RAIN_PROBE_NATIVE
}
```

For the other sampled workloads, pass `256 192 800 400 1`,
`801 603 1600 500 1.25`, or `3840 2160 1600 500 1` respectively.
